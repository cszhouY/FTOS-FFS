# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.26

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /snap/cmake/1299/bin/cmake

# The command to remove a file.
RM = /snap/cmake/1299/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/user/Desktop/lib14/src/fs/test

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/user/Desktop/lib14/src/fs/test/build

# Include any dependencies generated for this target.
include CMakeFiles/mock.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/mock.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/mock.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/mock.dir/flags.make

CMakeFiles/mock.dir/mock/arena.cpp.o: CMakeFiles/mock.dir/flags.make
CMakeFiles/mock.dir/mock/arena.cpp.o: /home/user/Desktop/lib14/src/fs/test/mock/arena.cpp
CMakeFiles/mock.dir/mock/arena.cpp.o: CMakeFiles/mock.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/user/Desktop/lib14/src/fs/test/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/mock.dir/mock/arena.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/mock.dir/mock/arena.cpp.o -MF CMakeFiles/mock.dir/mock/arena.cpp.o.d -o CMakeFiles/mock.dir/mock/arena.cpp.o -c /home/user/Desktop/lib14/src/fs/test/mock/arena.cpp

CMakeFiles/mock.dir/mock/arena.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/mock.dir/mock/arena.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/user/Desktop/lib14/src/fs/test/mock/arena.cpp > CMakeFiles/mock.dir/mock/arena.cpp.i

CMakeFiles/mock.dir/mock/arena.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/mock.dir/mock/arena.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/user/Desktop/lib14/src/fs/test/mock/arena.cpp -o CMakeFiles/mock.dir/mock/arena.cpp.s

CMakeFiles/mock.dir/mock/core.c.o: CMakeFiles/mock.dir/flags.make
CMakeFiles/mock.dir/mock/core.c.o: /home/user/Desktop/lib14/src/fs/test/mock/core.c
CMakeFiles/mock.dir/mock/core.c.o: CMakeFiles/mock.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/user/Desktop/lib14/src/fs/test/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building C object CMakeFiles/mock.dir/mock/core.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT CMakeFiles/mock.dir/mock/core.c.o -MF CMakeFiles/mock.dir/mock/core.c.o.d -o CMakeFiles/mock.dir/mock/core.c.o -c /home/user/Desktop/lib14/src/fs/test/mock/core.c

CMakeFiles/mock.dir/mock/core.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/mock.dir/mock/core.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/user/Desktop/lib14/src/fs/test/mock/core.c > CMakeFiles/mock.dir/mock/core.c.i

CMakeFiles/mock.dir/mock/core.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/mock.dir/mock/core.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/user/Desktop/lib14/src/fs/test/mock/core.c -o CMakeFiles/mock.dir/mock/core.c.s

CMakeFiles/mock.dir/mock/list.cpp.o: CMakeFiles/mock.dir/flags.make
CMakeFiles/mock.dir/mock/list.cpp.o: /home/user/Desktop/lib14/src/fs/test/mock/list.cpp
CMakeFiles/mock.dir/mock/list.cpp.o: CMakeFiles/mock.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/user/Desktop/lib14/src/fs/test/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building CXX object CMakeFiles/mock.dir/mock/list.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/mock.dir/mock/list.cpp.o -MF CMakeFiles/mock.dir/mock/list.cpp.o.d -o CMakeFiles/mock.dir/mock/list.cpp.o -c /home/user/Desktop/lib14/src/fs/test/mock/list.cpp

CMakeFiles/mock.dir/mock/list.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/mock.dir/mock/list.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/user/Desktop/lib14/src/fs/test/mock/list.cpp > CMakeFiles/mock.dir/mock/list.cpp.i

CMakeFiles/mock.dir/mock/list.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/mock.dir/mock/list.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/user/Desktop/lib14/src/fs/test/mock/list.cpp -o CMakeFiles/mock.dir/mock/list.cpp.s

CMakeFiles/mock.dir/mock/lock.cpp.o: CMakeFiles/mock.dir/flags.make
CMakeFiles/mock.dir/mock/lock.cpp.o: /home/user/Desktop/lib14/src/fs/test/mock/lock.cpp
CMakeFiles/mock.dir/mock/lock.cpp.o: CMakeFiles/mock.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/user/Desktop/lib14/src/fs/test/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Building CXX object CMakeFiles/mock.dir/mock/lock.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/mock.dir/mock/lock.cpp.o -MF CMakeFiles/mock.dir/mock/lock.cpp.o.d -o CMakeFiles/mock.dir/mock/lock.cpp.o -c /home/user/Desktop/lib14/src/fs/test/mock/lock.cpp

CMakeFiles/mock.dir/mock/lock.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/mock.dir/mock/lock.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/user/Desktop/lib14/src/fs/test/mock/lock.cpp > CMakeFiles/mock.dir/mock/lock.cpp.i

CMakeFiles/mock.dir/mock/lock.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/mock.dir/mock/lock.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/user/Desktop/lib14/src/fs/test/mock/lock.cpp -o CMakeFiles/mock.dir/mock/lock.cpp.s

CMakeFiles/mock.dir/mock/lock_config.cpp.o: CMakeFiles/mock.dir/flags.make
CMakeFiles/mock.dir/mock/lock_config.cpp.o: /home/user/Desktop/lib14/src/fs/test/mock/lock_config.cpp
CMakeFiles/mock.dir/mock/lock_config.cpp.o: CMakeFiles/mock.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/user/Desktop/lib14/src/fs/test/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Building CXX object CMakeFiles/mock.dir/mock/lock_config.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/mock.dir/mock/lock_config.cpp.o -MF CMakeFiles/mock.dir/mock/lock_config.cpp.o.d -o CMakeFiles/mock.dir/mock/lock_config.cpp.o -c /home/user/Desktop/lib14/src/fs/test/mock/lock_config.cpp

CMakeFiles/mock.dir/mock/lock_config.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/mock.dir/mock/lock_config.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/user/Desktop/lib14/src/fs/test/mock/lock_config.cpp > CMakeFiles/mock.dir/mock/lock_config.cpp.i

CMakeFiles/mock.dir/mock/lock_config.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/mock.dir/mock/lock_config.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/user/Desktop/lib14/src/fs/test/mock/lock_config.cpp -o CMakeFiles/mock.dir/mock/lock_config.cpp.s

CMakeFiles/mock.dir/mock/panic.cpp.o: CMakeFiles/mock.dir/flags.make
CMakeFiles/mock.dir/mock/panic.cpp.o: /home/user/Desktop/lib14/src/fs/test/mock/panic.cpp
CMakeFiles/mock.dir/mock/panic.cpp.o: CMakeFiles/mock.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/user/Desktop/lib14/src/fs/test/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_6) "Building CXX object CMakeFiles/mock.dir/mock/panic.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/mock.dir/mock/panic.cpp.o -MF CMakeFiles/mock.dir/mock/panic.cpp.o.d -o CMakeFiles/mock.dir/mock/panic.cpp.o -c /home/user/Desktop/lib14/src/fs/test/mock/panic.cpp

CMakeFiles/mock.dir/mock/panic.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/mock.dir/mock/panic.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/user/Desktop/lib14/src/fs/test/mock/panic.cpp > CMakeFiles/mock.dir/mock/panic.cpp.i

CMakeFiles/mock.dir/mock/panic.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/mock.dir/mock/panic.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/user/Desktop/lib14/src/fs/test/mock/panic.cpp -o CMakeFiles/mock.dir/mock/panic.cpp.s

CMakeFiles/mock.dir/mock/rc.cpp.o: CMakeFiles/mock.dir/flags.make
CMakeFiles/mock.dir/mock/rc.cpp.o: /home/user/Desktop/lib14/src/fs/test/mock/rc.cpp
CMakeFiles/mock.dir/mock/rc.cpp.o: CMakeFiles/mock.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/user/Desktop/lib14/src/fs/test/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_7) "Building CXX object CMakeFiles/mock.dir/mock/rc.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/mock.dir/mock/rc.cpp.o -MF CMakeFiles/mock.dir/mock/rc.cpp.o.d -o CMakeFiles/mock.dir/mock/rc.cpp.o -c /home/user/Desktop/lib14/src/fs/test/mock/rc.cpp

CMakeFiles/mock.dir/mock/rc.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/mock.dir/mock/rc.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/user/Desktop/lib14/src/fs/test/mock/rc.cpp > CMakeFiles/mock.dir/mock/rc.cpp.i

CMakeFiles/mock.dir/mock/rc.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/mock.dir/mock/rc.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/user/Desktop/lib14/src/fs/test/mock/rc.cpp -o CMakeFiles/mock.dir/mock/rc.cpp.s

CMakeFiles/mock.dir/mock/yield.cpp.o: CMakeFiles/mock.dir/flags.make
CMakeFiles/mock.dir/mock/yield.cpp.o: /home/user/Desktop/lib14/src/fs/test/mock/yield.cpp
CMakeFiles/mock.dir/mock/yield.cpp.o: CMakeFiles/mock.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/user/Desktop/lib14/src/fs/test/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_8) "Building CXX object CMakeFiles/mock.dir/mock/yield.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/mock.dir/mock/yield.cpp.o -MF CMakeFiles/mock.dir/mock/yield.cpp.o.d -o CMakeFiles/mock.dir/mock/yield.cpp.o -c /home/user/Desktop/lib14/src/fs/test/mock/yield.cpp

CMakeFiles/mock.dir/mock/yield.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/mock.dir/mock/yield.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/user/Desktop/lib14/src/fs/test/mock/yield.cpp > CMakeFiles/mock.dir/mock/yield.cpp.i

CMakeFiles/mock.dir/mock/yield.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/mock.dir/mock/yield.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/user/Desktop/lib14/src/fs/test/mock/yield.cpp -o CMakeFiles/mock.dir/mock/yield.cpp.s

# Object files for target mock
mock_OBJECTS = \
"CMakeFiles/mock.dir/mock/arena.cpp.o" \
"CMakeFiles/mock.dir/mock/core.c.o" \
"CMakeFiles/mock.dir/mock/list.cpp.o" \
"CMakeFiles/mock.dir/mock/lock.cpp.o" \
"CMakeFiles/mock.dir/mock/lock_config.cpp.o" \
"CMakeFiles/mock.dir/mock/panic.cpp.o" \
"CMakeFiles/mock.dir/mock/rc.cpp.o" \
"CMakeFiles/mock.dir/mock/yield.cpp.o"

# External object files for target mock
mock_EXTERNAL_OBJECTS =

libmock.a: CMakeFiles/mock.dir/mock/arena.cpp.o
libmock.a: CMakeFiles/mock.dir/mock/core.c.o
libmock.a: CMakeFiles/mock.dir/mock/list.cpp.o
libmock.a: CMakeFiles/mock.dir/mock/lock.cpp.o
libmock.a: CMakeFiles/mock.dir/mock/lock_config.cpp.o
libmock.a: CMakeFiles/mock.dir/mock/panic.cpp.o
libmock.a: CMakeFiles/mock.dir/mock/rc.cpp.o
libmock.a: CMakeFiles/mock.dir/mock/yield.cpp.o
libmock.a: CMakeFiles/mock.dir/build.make
libmock.a: CMakeFiles/mock.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/user/Desktop/lib14/src/fs/test/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_9) "Linking CXX static library libmock.a"
	$(CMAKE_COMMAND) -P CMakeFiles/mock.dir/cmake_clean_target.cmake
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/mock.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/mock.dir/build: libmock.a
.PHONY : CMakeFiles/mock.dir/build

CMakeFiles/mock.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/mock.dir/cmake_clean.cmake
.PHONY : CMakeFiles/mock.dir/clean

CMakeFiles/mock.dir/depend:
	cd /home/user/Desktop/lib14/src/fs/test/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/user/Desktop/lib14/src/fs/test /home/user/Desktop/lib14/src/fs/test /home/user/Desktop/lib14/src/fs/test/build /home/user/Desktop/lib14/src/fs/test/build /home/user/Desktop/lib14/src/fs/test/build/CMakeFiles/mock.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/mock.dir/depend

