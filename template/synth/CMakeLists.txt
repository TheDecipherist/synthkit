cmake_minimum_required(VERSION 3.22)
project(ExampleSynth VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    add_compile_definitions(_USE_MATH_DEFINES)
endif()

option(EXAMPLESYNTH_INSTALL_PLUGIN "Copy the VST3 to the system folder after building" OFF)

# ---------------------------------------------------------------------------
# synthkit's cmake helpers. Found either beside this unit (a generated repo) or
# two levels up (this unit sitting inside synthkit itself), so the template
# builds where it lives and keeps building where it lands.
foreach(_p "${CMAKE_CURRENT_SOURCE_DIR}/cmake" "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")
    if(EXISTS "${_p}/Siblings.cmake")
        set(SYNTHKIT_CMAKE_DIR "${_p}")
        break()
    endif()
endforeach()
if(NOT SYNTHKIT_CMAKE_DIR)
    message(FATAL_ERROR "synthkit's cmake/ helpers not found")
endif()

# JUCE, once. A unit built inside synthkit shares the parent's copy.
if(NOT TARGET juce::juce_audio_utils)
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# synthshell (the bottom module) and dspblocks (the effect blocks). DEPENDENCIES.
include("${SYNTHKIT_CMAKE_DIR}/Siblings.cmake")

# ---------------------------------------------------------------------------
set(EXAMPLESYNTH_ENGINE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/Engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/Coeffs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/patch/Program.cpp)
set(EXAMPLESYNTH_PLUGIN_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PluginEditor.cpp)

juce_add_plugin(ExampleSynth
    COMPANY_NAME              "The Decipherist"
    BUNDLE_ID                 com.thedecipherist.examplesynth
    PLUGIN_MANUFACTURER_CODE  Dcph
    PLUGIN_CODE               Exs1
    FORMATS                   VST3 Standalone
    PRODUCT_NAME              "ExampleSynth"
    IS_SYNTH                  TRUE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    COPY_PLUGIN_AFTER_BUILD   ${EXAMPLESYNTH_INSTALL_PLUGIN}
    VST3_CATEGORIES           "Instrument" "Synth")

# The panel SPEC always travels with the plugin: it is what synthkit's panel
# falls back to, drawing plain shapes at the spec's own coordinates, while the
# artwork is still being authored. The native EXPORT joins it the moment
# `npm run panel:native` has written resources/native/, and the panel paints
# that instead with no code change. CONFIGURE_DEPENDS so the first export does
# not need a manual re-run of cmake to be noticed.
file(GLOB EXAMPLESYNTH_PANEL_NATIVE CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/resources/native/*.json
     ${CMAKE_CURRENT_SOURCE_DIR}/resources/native/*.png
     ${CMAKE_CURRENT_SOURCE_DIR}/resources/native/sprites/*.png)
juce_add_binary_data(ExampleSynthAssets SOURCES
    panel/panel.json
    panel/bindings.json
    ${EXAMPLESYNTH_PANEL_NATIVE})

set(EXAMPLESYNTH_VERSION_HEADER ${CMAKE_CURRENT_BINARY_DIR}/generated/BuildVersion.h)
add_custom_target(ExampleSynthVersion ALL
    COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR} -DOUT=${EXAMPLESYNTH_VERSION_HEADER}
            -P ${SYNTHKIT_CMAKE_DIR}/GenVersion.cmake
    BYPRODUCTS ${EXAMPLESYNTH_VERSION_HEADER})

target_sources(ExampleSynth PRIVATE ${EXAMPLESYNTH_PLUGIN_SOURCES} ${EXAMPLESYNTH_ENGINE_SOURCES})
target_compile_definitions(ExampleSynth PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_include_directories(ExampleSynth PRIVATE src ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(ExampleSynth PRIVATE
    ExampleSynthAssets synthshell dspblocks synthkitpanel
    juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
add_dependencies(ExampleSynth ExampleSynthVersion)

# --- probes ----------------------------------------------------------------
# A helper, because four probes each need the whole plugin in the path and
# repeating the recipe four times is how they drift apart.
function(examplesynth_probe target main)
    juce_add_console_app(${target} PRODUCT_NAME "${target}")
    target_sources(${target} PRIVATE ${main} ${EXAMPLESYNTH_PLUGIN_SOURCES} ${EXAMPLESYNTH_ENGINE_SOURCES})
    target_include_directories(${target} PRIVATE src ${CMAKE_CURRENT_BINARY_DIR}/generated)
    target_compile_definitions(${target} PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JucePlugin_Name="ExampleSynth" JucePlugin_IsSynth=1
        JucePlugin_WantsMidiInput=1 JucePlugin_ProducesMidiOutput=0)
    target_link_libraries(${target} PRIVATE ExampleSynthAssets synthshell dspblocks synthkitpanel
        juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_dependencies(${target} ExampleSynthVersion)
    set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE FALSE)
endfunction()

# The tripwires: byte-exact record round-trip, every stored value reaching the
# engine, frozen parameter ids, neutral defaults. No JUCE, no plugin: the engine
# and the bank alone, so it runs in a second and bisects cleanly.
add_executable(examplesynth_tests tools/tests_main.cpp ${EXAMPLESYNTH_ENGINE_SOURCES})
target_include_directories(examplesynth_tests PRIVATE src ${DSPBLOCKS_ROOT}/include)

# Does the box follow the clock, measured in the AUDIO out of the real processor?
examplesynth_probe(ExampleSynthSyncProbe tools/sync_probe.cpp)
# Do the front panel's controls reach anything, and does every parameter have a
# control? Driven through the real editor.
examplesynth_probe(ExampleSynthControlsProbe tools/controls_probe.cpp)

# Does the panel still get clicked where it is drawn once it no longer fits its
# window and starts scrolling? The offset has to be put back before any hit test,
# in every handler, and a panel at offset zero looks perfect either way - which is
# where every other gate and every render happens to sit.
examplesynth_probe(ExampleSynthViewportProbe tools/viewport_probe.cpp)

# Does a parameter written the way the EDITOR and the HOST write it reach the
# audio? The controls probe drives devPanel(), which builds the event itself, so
# it proves the editor HANDLES an event and never that a real write is heard.
examplesynth_probe(ExampleSynthParamAudio tools/param_reaches_audio.cpp)
# Do the factory programs actually sound different from each other?
examplesynth_probe(ExampleSynthSpreadProbe tools/spread_probe.cpp)
# The panel, rendered to a PNG at the width the window opens to.
juce_add_gui_app(ExampleSynthSnapshot PRODUCT_NAME "ExampleSynthSnapshot")
target_sources(ExampleSynthSnapshot PRIVATE tools/snapshot_main.cpp
    ${EXAMPLESYNTH_PLUGIN_SOURCES} ${EXAMPLESYNTH_ENGINE_SOURCES})
target_include_directories(ExampleSynthSnapshot PRIVATE src ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_compile_definitions(ExampleSynthSnapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JucePlugin_Name="ExampleSynth" JucePlugin_IsSynth=1
    JucePlugin_WantsMidiInput=1 JucePlugin_ProducesMidiOutput=0)
target_link_libraries(ExampleSynthSnapshot PRIVATE ExampleSynthAssets synthshell dspblocks synthkitpanel
    juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
add_dependencies(ExampleSynthSnapshot ExampleSynthVersion)

# The shared clock gate, run against a KNOWN-GOOD host and four KNOWN-BAD ones.
# A shared check that is not itself checked is a shared bug: ours silently
# inverted into passing nothing and failing eleven healthy units.
# It links the whole shell, not synthshell_headers: shell::ShellHost's vtable
# pulls in shell::defaultCategories(), which lives in the shell's own sources. A
# header-only link looks tidier and does not build.
juce_add_console_app(examplesynth_clock_gate PRODUCT_NAME "examplesynth_clock_gate")
target_sources(examplesynth_clock_gate PRIVATE tools/clock_gate_test.cpp)
target_link_libraries(examplesynth_clock_gate PRIVATE synthshell
    juce::juce_audio_utils juce::juce_recommended_config_flags)
target_compile_definitions(examplesynth_clock_gate PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1)
set_target_properties(examplesynth_clock_gate PROPERTIES WIN32_EXECUTABLE FALSE)

# --- the gates -------------------------------------------------------------
enable_testing()

# EVERY TEST NAME CARRIES THE UNIT'S NAME, and that is not decoration. CTest
# looks test PROPERTIES up by NAME, project-wide: build two units in one tree
# with a test called `panel_renders` in each and the second one's
# WORKING_DIRECTORY silently applies to both, so one unit's snapshot lands in the
# other's build folder and both tests pass. That happened here, between these two
# templates, and it is the same class of bug as a colliding binary-data filename.
add_test(NAME ${PROJECT_NAME}.tripwires COMMAND examplesynth_tests)
add_test(NAME ${PROJECT_NAME}.follows_the_clock COMMAND ExampleSynthSyncProbe)
add_test(NAME ${PROJECT_NAME}.controls_are_alive COMMAND ExampleSynthControlsProbe)
add_test(NAME ${PROJECT_NAME}.panel_scrolls_and_still_hits COMMAND ExampleSynthViewportProbe)
add_test(NAME ${PROJECT_NAME}.a_direct_write_reaches_the_audio COMMAND ExampleSynthParamAudio)
add_test(NAME ${PROJECT_NAME}.bank_still_spreads COMMAND ExampleSynthSpreadProbe)
add_test(NAME ${PROJECT_NAME}.clock_gate_is_itself_checked COMMAND examplesynth_clock_gate)

# EVERY PROBE MUST FAIL WHEN IT SHOULD. Each --selftest run injects, in process,
# the exact defect the probe exists to catch and requires the probe to report it;
# the injection never touches the tree, so a session that dies mid-run cannot
# leave a bug behind that looks identical to a real failure.
add_test(NAME ${PROJECT_NAME}.probes_fail_when_they_should_sync     COMMAND ExampleSynthSyncProbe --selftest)
add_test(NAME ${PROJECT_NAME}.probes_fail_when_they_should_controls COMMAND ExampleSynthControlsProbe --selftest)
add_test(NAME ${PROJECT_NAME}.probes_fail_when_they_should_viewport COMMAND ExampleSynthViewportProbe --selftest)
add_test(NAME ${PROJECT_NAME}.probes_fail_when_they_should_spread   COMMAND ExampleSynthSpreadProbe --selftest)
add_test(NAME ${PROJECT_NAME}.probes_fail_when_they_should_tests    COMMAND examplesynth_tests --selftest)

# The panel renders, at the width the window opens to. A snapshot that never
# renders is how a panel regression ships: the picture is the only check that
# sees a control drawn on top of another one, or a legend off the edge.
add_test(NAME ${PROJECT_NAME}.panel_renders
         COMMAND $<TARGET_FILE:ExampleSynthSnapshot>
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# The CSV and the generated header disagree in silence until somebody re-runs
# the generator, and nothing else in this build would say so.
add_test(NAME ${PROJECT_NAME}.generated_headers_are_current
         COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
                 -DCMAKE_CURRENT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
                 -P ${SYNTHKIT_CMAKE_DIR}/CheckGenerated.cmake)
