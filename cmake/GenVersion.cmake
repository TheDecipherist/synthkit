# The build stamp the panel prints top-right, so a screenshot always says which
# build it is. git count + short hash, "+" when the tree is dirty.
if(NOT DEFINED SRC OR NOT DEFINED OUT)
    message(FATAL_ERROR "GenVersion.cmake needs -DSRC=<repo> -DOUT=<header>")
endif()
set(stamp "dev")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
                    WORKING_DIRECTORY ${SRC} OUTPUT_VARIABLE n
                    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
                    WORKING_DIRECTORY ${SRC} OUTPUT_VARIABLE h
                    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND ${GIT_EXECUTABLE} status --porcelain
                    WORKING_DIRECTORY ${SRC} OUTPUT_VARIABLE dirty
                    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(n AND h)
        set(stamp "${n}.${h}")
        if(NOT dirty STREQUAL "")
            set(stamp "${stamp}+")
        endif()
    endif()
endif()
get_filename_component(outdir "${OUT}" DIRECTORY)
file(MAKE_DIRECTORY "${outdir}")
set(text "#pragma once\n#define SYNTHKIT_BUILD_VERSION \"${stamp}\"\n")
if(EXISTS "${OUT}")
    file(READ "${OUT}" old)
    if(old STREQUAL text)
        return()
    endif()
endif()
file(WRITE "${OUT}" "${text}")
