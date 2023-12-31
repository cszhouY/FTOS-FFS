#include <elf.h>

#include <aarch64/mmu.h>
#include <common/string.h>
#include <core/console.h>
#include <core/proc.h>
#include <core/sched.h>
#include <core/syscall.h>
#include <core/trap.h>
#include <core/virtual_memory.h>
#include <fs/file.h>
#include <fs/inode.h>

static uint64_t auxv[][2] = {{AT_PAGESZ, PAGE_SIZE}};

int execve(const char *path, char *const argv[], char *const envp[]) {
    // printf("current %s\n", path);
    char *s;
    if (fetchstr((uint64_t)path, &s) < 0)
        return -1;

    // Save previous page table.
    struct proc *curproc = thiscpu()->proc;
    void *oldpgdir = curproc->pgdir, *pgdir = pgdir_init();
    Inode *ip = 0;

    if (pgdir == 0) {
        // debug("vm init failed");
        goto bad;
    }

    // trace("path='%s', argv=0x%p, envp=0x%p", s, argv, envp);

    OpContext ctx;
    bcache.begin_op(&ctx);
    ip = namei(path, &ctx);
    if (ip == 0) {
        bcache.end_op(&ctx);
        // debug("namei bad");
        goto bad;
    }
    inodes.lock(ip);

    Elf64_Ehdr elf;

    // printf("exec start read elf inode %u\n", ip->inode_no);    
    inodes.read(ip, (u8 *)&elf, 0, sizeof(elf));
    // printf("exec finish read elf inode %u \n", ip->inode_no);
    if (!(elf.e_ident[EI_MAG0] == ELFMAG0 && elf.e_ident[EI_MAG1] == ELFMAG1 &&
          elf.e_ident[EI_MAG2] == ELFMAG2 && elf.e_ident[EI_MAG3] == ELFMAG3)) {
        // debug("elf header magic invalid");
        goto bad;
    }
    if (elf.e_ident[EI_CLASS] != ELFCLASS64) {
        // debug("64 bit program not supported");
        goto bad;
    }
    // trace("check elf header finish");

    int i;
    uint64_t off;
    Elf64_Phdr ph;

    curproc->pgdir =
        pgdir;  // Required since inodes.read(sdrw) involves context switch(switch page table).

    // Load program into memory.
    usize sz = 0, base = 0, stksz = 0;
    int first = 1;
    for (i = 0, off = elf.e_phoff; i < elf.e_phnum; i++, off += sizeof(ph)) {
        inodes.read(ip, (u8 *)&ph, off, sizeof(ph));
        // printf("exec read elf_ph inode %u \n", ip->inode_no);

        if (ph.p_type != PT_LOAD) {
            // debug("unsupported type 0x%x, skipped\n", ph.p_type);
            continue;
        }

        if (ph.p_memsz < ph.p_filesz) {
            // debug("memsz smaller than filesz");
            goto bad;
        }

        if (ph.p_vaddr + ph.p_memsz < ph.p_vaddr) {
            // debug("vaddr + memsz overflow");
            goto bad;
        }

        if (first) {
            first = 0;
            sz = base = ph.p_vaddr;
            if (base % PAGE_SIZE != 0) {
                // debug("first section should be page aligned!");
                goto bad;
            }
        }

        if ((sz = (usize)uvm_alloc(pgdir, base, stksz, sz, ph.p_vaddr + ph.p_memsz)) == 0) {
            // debug("uvm_alloc bad");
            goto bad;
        }

        uvm_switch(pgdir);

        inodes.read(ip, (u8 *)ph.p_vaddr, ph.p_offset, ph.p_filesz);
        // printf("exec read elf_ph_vaddr inode %u \n", ip->inode_no);
        // Initialize BSS.
        memset((void *)ph.p_vaddr + ph.p_filesz, 0, ph.p_memsz - ph.p_filesz);

        // Flush dcache to memory so that icache can retrieve the correct one.
        // dccivac(ph.p_vaddr, ph.p_memsz);

        // trace("init bss [0x%p, 0x%p)", ph.p_vaddr + ph.p_filesz,
        //       ph.p_vaddr + ph.p_memsz);
    }
    inodes.unlock(ip);
    inodes.put(&ctx, ip);
    bcache.end_op(&ctx);
    ip = 0;

    // Push argument strings, prepare rest of stack in ustack.
    uvm_switch(oldpgdir);
    char *sp = (char *)USPACE_TOP;
    int argc = 0, envc = 0;
    isize len;
    if (argv) {
        for (; in_user((void *)(argv + argc), sizeof(*argv)) && argv[argc]; argc++) {
            if ((len = (isize)fetchstr((uint64_t)argv[argc], &s)) < 0) {
                // debug("argv fetchstr bad");
                goto bad;
            }
            // trace("argv[%d] = '%s', len: %d", argc, argv[argc], len);
            sp -= len + 1;
            if (copyout(pgdir, sp, argv[argc], (usize)(len + 1)) < 0)  // include '\0';
                goto bad;
        }
    }
    if (envp) {
        for (; in_user((void *)(envp + envc), sizeof(*envp)) && envp[envc]; envc++) {
            if ((len = (isize)fetchstr((uint64_t)envp[envc], &s)) < 0) {
                // debug("envp fetchstr bad");
                goto bad;
            }
            // trace("envp[%d] = '%s', len: %d", envc, envp[envc], len);
            sp -= len + 1;
            if (copyout(pgdir, sp, envp[envc], (usize)(len + 1)) < 0)  // include '\0';
                goto bad;
        }
    }
    // Align to 16B. 3 zero terminator of auxv/envp/argv and 1 argc.
    void *newsp = (void *)round_down((usize)sp - sizeof(auxv) - (usize)(envc + argc + 4) * 8, 16);
    if (copyout(pgdir, newsp, 0, (usize)sp - (usize)newsp) < 0)
        goto bad;

    uvm_switch(pgdir);
    uint64_t *newargv = newsp + 8;
    uint64_t *newenvp = (void *)newargv + 8 * (argc + 1);
    uint64_t *newauxv = (void *)newenvp + 8 * (envc + 1);
    // trace("argv: 0x%p, envp: 0x%p, auxv: 0x%p", newargv, newenvp, newauxv);
    memmove(newauxv, auxv, sizeof(auxv));

    for (int i = envc - 1; i >= 0; i--) {
        newenvp[i] = (uint64_t)sp;
        for (; *sp; sp++)
            ;
        sp++;
    }
    for (int i = argc - 1; i >= 0; i--) {
        newargv[i] = (uint64_t)sp;
        for (; *sp; sp++)
            ;
        sp++;
    }
    *(usize *)(newsp) = (usize)argc;

    sp = newsp;
    // trace("newsp: 0x%p", sp);

    // Allocate user stack.
    stksz = round_up(USPACE_TOP - (usize)sp, 10 * PAGE_SIZE);
    if (copyout(pgdir, (void *)(USPACE_TOP - stksz), 0, stksz - (USPACE_TOP - (usize)sp)) < 0)
        goto bad;
    assert((uint64_t)sp > USPACE_TOP - stksz);

    // Commit to the user image.
    curproc->pgdir = pgdir;

    curproc->base = base;
    curproc->sz = sz;
    curproc->stksz = stksz;

    // memset(curproc->tf, 0, sizeof(*curproc->tf));

    curproc->tf->elr = elf.e_entry;
    curproc->tf->sp = (uint64_t)sp;

    // trace("entry 0x%p", elf.e_entry);

    uvm_switch(oldpgdir);

    // Save program name for debugging.
    const char *last, *cur;
    for (last = cur = path; *cur; cur++)
        if (*cur == '/')
            last = cur + 1;
    memmove(curproc->name, last, sizeof(curproc->name));
    uvm_switch(curproc->pgdir);
    // vm_free(oldpgdir);
    // trace("finish %s", curproc->name);
    return 0;

bad:
    if (pgdir)
        vm_free(pgdir);
    if (ip)
        inodes.unlock(ip), inodes.put(&ctx, ip), bcache.end_op(&ctx);
    thiscpu()->proc->pgdir = oldpgdir;
    // debug("bad");
    return -1;
}
