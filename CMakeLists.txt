cmake_minimum_required(VERSION 3.22)
project(synthkit VERSION 0.1.0 LANGUAGES C CXX)

# synthkit builds its own template units, both of them, and runs their gates.
#
# A skeleton nobody can run rots. These two are complete plugins - they build,
# they make a sound, and every gate a real machine ships with is already wired -
# so a change to the skeleton is caught here rather than on the next machine.
#
#   cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
#   cmake --build build
#   ctest --test-dir build --output-on-failure
#
# A NEW MACHINE IS NOT BUILT FROM THIS DIRECTORY. Run tools/new_synth.py, which
# instantiates one of these into its own repo. See docs/new-machine.md.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE once, shared by both templates.
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

enable_testing()

# The package's own panel gate, before the templates: it needs nothing but JUCE
# and the panel source, and a broken seam should fail here first.
add_subdirectory(panel/tests)

add_subdirectory(template/synth)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/template/effect/CMakeLists.txt)
    add_subdirectory(template/effect)
endif()
