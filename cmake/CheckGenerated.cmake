# Re-running the generator must produce no diff.
#
# Playbook section 15: when a research pass widens a range in a CSV, the CSV and
# the engine silently disagree until somebody re-runs the generator, and nothing
# complains. The Access Virus shipped for weeks with Delay/Reverb Mode clamped to
# 0..1 on a machine with 26 modes, Filter1 Mode to 0..3 of 8, and Saturation
# Curve to 0..6 of 12 - every one reachable on the hardware and unreachable in
# the plugin, because the generated header had not been regenerated. The build
# was green the whole time and the only symptom was that most of the machine did
# not exist.
#
# It writes into a scratch directory and compares, rather than regenerating in
# place and diffing with git, so a failing check never leaves the tree modified
# and the test works in a tree that is not a git checkout.
if(NOT DEFINED SRC)
    message(FATAL_ERROR "CheckGenerated.cmake needs -DSRC=<repo>")
endif()
if(NOT DEFINED GEN)
    set(GEN "${SRC}/tools/gen_headers.py")
endif()
if(NOT DEFINED OUTDIR)
    set(OUTDIR "${SRC}/src/generated")
endif()
if(NOT EXISTS "${GEN}")
    message(FATAL_ERROR
        "no generator at ${GEN}. Every unit has generated headers - the parameter\n"
        "table and the factory bank come from CSVs with a provenance column - and\n"
        "this check is what stops the CSV and the header drifting apart. If this\n"
        "machine genuinely has neither, delete the test deliberately and say why\n"
        "in todo.md; do not leave a check that cannot fail.")
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(scratch "${CMAKE_CURRENT_BINARY_DIR}/generated-check")
file(REMOVE_RECURSE "${scratch}")
file(MAKE_DIRECTORY "${scratch}")

execute_process(COMMAND ${Python3_EXECUTABLE} "${GEN}" --out "${scratch}"
                WORKING_DIRECTORY "${SRC}"
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "the generator failed:\n${out}\n${err}")
endif()

file(GLOB fresh "${scratch}/*")
file(GLOB committed "${OUTDIR}/*")
list(LENGTH fresh nf)
list(LENGTH committed nc)
if(NOT nf EQUAL nc)
    message(FATAL_ERROR
        "the generator writes ${nf} files and ${OUTDIR} holds ${nc}. "
        "Run: python3 tools/gen_headers.py")
endif()
set(bad "")
foreach(f ${fresh})
    get_filename_component(n "${f}" NAME)
    if(NOT EXISTS "${OUTDIR}/${n}")
        list(APPEND bad "${n} (missing)")
        continue()
    endif()
    file(READ "${f}" a)
    file(READ "${OUTDIR}/${n}" b)
    if(NOT a STREQUAL b)
        list(APPEND bad "${n} (differs)")
    endif()
endforeach()
if(bad)
    string(REPLACE ";" "\n  " pretty "${bad}")
    message(FATAL_ERROR
        "the generated headers are out of date - the CSV and the code disagree "
        "and nothing else in this build would have said so:\n  ${pretty}\n"
        "Run: python3 tools/gen_headers.py")
endif()
message(STATUS "generated headers are current (${nf} files)")
