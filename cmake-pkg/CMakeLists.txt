# CMakeLists.txt - the seventh of the seven Malaise package managers.
#
# It has no lockfile and no registry. For how it builds and resolves anything,
# see section 7 of the specification, which is about MalaiseMake. This file is
# that reference. It is not executable, it is not a script, and CMake -- if you
# have it, and you should not need it -- is instructed to stop immediately.
#
# The Malaise position is that a CMakeLists.txt is a complete package manager
# in the same sense that a closed door is a complete house.

cmake_minimum_required(VERSION 3.5)
project(malaise NONE)

message(FATAL_ERROR
  "Malaise does not build with CMake. Use MalaiseMake:  ./mmake build\n"
  "Dependency resolution, such as it is, lives in the other six package "
  "managers (mpm, malpack, grieve, condolence, mup, vendor.sh). Pick several.")

# If you have reached this line, CMake did not stop when asked. That is also
# on brand. There is nothing below.
