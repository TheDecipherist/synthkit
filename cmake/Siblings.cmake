# Find the shared packages a unit DEPENDS on, from wherever this unit happens to
# sit on disk.
#
# Why a search rather than `../synthshell`: a generated unit lives at
# ~/projects/<name> and the templates it came from live at
# ~/projects/synthkit/template/<kind>, so a fixed relative path is wrong in one
# of the two places and the skeleton stops building the day it is instantiated.
# Walking up until the package is found is right in both, and stays right if the
# whole workspace moves.
#
# DEPEND, NEVER COPY. If you ever find yourself copying a file out of synthshell
# or dspblocks into a unit, stop: that is the mistake FIX ONCE exists to prevent
# and it is why thirty per-unit SessionStrip.cpp copies had to be deleted.

function(_synthkit_find_sibling out_var name probe)
    if(DEFINED ${out_var} AND EXISTS "${${out_var}}/${probe}")
        return()
    endif()
    set(dir "${CMAKE_CURRENT_SOURCE_DIR}")
    foreach(_i RANGE 8)
        set(cand "${dir}/../${name}")
        if(EXISTS "${cand}/${probe}")
            get_filename_component(cand "${cand}" ABSOLUTE)
            set(${out_var} "${cand}" PARENT_SCOPE)
            return()
        endif()
        get_filename_component(dir "${dir}" DIRECTORY)
    endforeach()
    message(FATAL_ERROR
        "synthkit: could not find ${name}. It is a DEPENDENCY, not something to "
        "copy in. Clone it beside this unit (git clone "
        "https://github.com/TheDecipherist/${name}.git) or pass "
        "-D${out_var}=/path/to/${name}.")
endfunction()

# The bottom shell: session strip, transport header, Patch Explorer, keyboard
# row, audition source, master clock, and the clock conformance gate.
_synthkit_find_sibling(SYNTHSHELL_ROOT synthshell include/shell/ShellHost.h)
# The effect blocks: delay, filters, modulation, drive, reverb, dynamics, and
# dsp/Measure.h, the shared differentiation metric.
_synthkit_find_sibling(DSPBLOCKS_ROOT dspblocks include/dsp/Blocks.h)
# The panel consumer: the ONE component that paints synthpanel-creator's native
# export, and the spec placeholder that stands in until the export exists. It
# used to be forty per-unit copies (MIGRATION.md).
_synthkit_find_sibling(SYNTHKIT_ROOT synthkit panel/NativePanel.h)

message(STATUS "synthkit: synthshell at ${SYNTHSHELL_ROOT}")
message(STATUS "synthkit: dspblocks at ${DSPBLOCKS_ROOT}")
message(STATUS "synthkit: panel consumer at ${SYNTHKIT_ROOT}/panel")

if(NOT TARGET synthshell)
    add_subdirectory("${SYNTHSHELL_ROOT}" synthshell EXCLUDE_FROM_ALL)
endif()
if(NOT TARGET dspblocks)
    add_library(dspblocks INTERFACE)
    target_include_directories(dspblocks INTERFACE "${DSPBLOCKS_ROOT}/include")
endif()
# An INTERFACE library carrying its own source, exactly as synthshell does and
# for the same reason: a STATIC library would compile the JUCE modules a second
# time and collide with the plugin that links it.
if(NOT TARGET synthkitpanel)
    add_library(synthkitpanel INTERFACE)
    target_sources(synthkitpanel INTERFACE "${SYNTHKIT_ROOT}/panel/NativePanel.cpp")
    target_include_directories(synthkitpanel INTERFACE "${SYNTHKIT_ROOT}/panel")
endif()

# THE DRAWING PACKAGE: a hardware panel drawn with juce::Graphics and no images
# (synthkit/draw/README.md). INTERFACE with its sources, like synthkitpanel.
if(NOT TARGET synthkitdraw AND EXISTS "${SYNTHKIT_ROOT}/draw/include/draw/Draw.h")
    add_library(synthkitdraw INTERFACE)
    file(GLOB _synthkit_draw_sources CONFIGURE_DEPENDS "${SYNTHKIT_ROOT}/draw/src/*.cpp")
    target_sources(synthkitdraw INTERFACE ${_synthkit_draw_sources})
    target_include_directories(synthkitdraw INTERFACE "${SYNTHKIT_ROOT}/draw/include")
endif()

# ---------------------------------------------------------------- COMPONENTS
# A COMPONENT IS A WHOLE MODULE OF A SYNTH, not a DSP block and not a widget: the
# FILTER with its topologies, its knobs, its response screen and its drag points.
# `synthkit/synth-component-library.md` is the registry and the id law.
#
# ADDING A COMPONENT IS A ROW in the list below. Everything under the loop is
# generic, which is the rule SynthShellFxUnits.cmake proved first: if adding one
# needs code past the table, the table is wrong.
#
# The include path is `${SYNTHKIT_ROOT}/components`, so a component's headers are
# spelled `<filter/dsp/FilterModule.h>` - the component's own name is the first
# path segment, which is what keeps two components' filenames from colliding the
# way two flat `layout.json` files do inside JUCE's BinaryData.
#
# INTERFACE carrying its own sources, never STATIC, for the same reason as
# synthkitpanel above: a STATIC library would compile the JUCE modules a second
# time and collide with the plugin that links it.
set(_synthkit_components
    filter                      # dspblocks: dsp/SuperFilter.h, dsp/TwinFilter.h
    piano                       # the multisampled piano: dsp/PianoSamples.{h,cpp}
    oscillator                  # dspblocks: dsp/JpOsc.h, plus this section's word laws
)
foreach(_c IN LISTS _synthkit_components)
    if(NOT TARGET synthkit_${_c})
        add_library(synthkit_${_c} INTERFACE)
        target_include_directories(synthkit_${_c} INTERFACE "${SYNTHKIT_ROOT}/components")
        # BOTH HALVES. This globbed `ui/` alone, which is right for a component whose only
        # source is a screen and silently wrong for one whose DSP has an implementation
        # file - the PIANO's loader is 364 lines and would have linked as nothing.
        file(GLOB _csrc "${SYNTHKIT_ROOT}/components/${_c}/ui/*.cpp"
                        "${SYNTHKIT_ROOT}/components/${_c}/dsp/*.cpp")
        if(_csrc)
            target_sources(synthkit_${_c} INTERFACE ${_csrc})
        endif()
        # Every component's DSP half is a dspblocks dependency, never a copy.
        target_link_libraries(synthkit_${_c} INTERFACE dspblocks)
    endif()
endforeach()
