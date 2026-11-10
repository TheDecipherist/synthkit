# THE PANEL'S OWN GATE for the seams no template panel carries: the second drag
# handle on a tab, the count in a tab's ring, the right-click that reports and
# does not select, and a path window read as a figure. Real mouse events on a
# real NativePanel with a layout built in the probe - see panel_probe.cpp.
juce_add_console_app(synthkit_panel_probe PRODUCT_NAME "synthkit_panel_probe")
target_sources(synthkit_panel_probe PRIVATE panel_probe.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../NativePanel.cpp)
target_include_directories(synthkit_panel_probe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_definitions(synthkit_panel_probe PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(synthkit_panel_probe PRIVATE juce::juce_gui_extra
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
set_target_properties(synthkit_panel_probe PROPERTIES WIN32_EXECUTABLE FALSE)

add_test(NAME synthkit.panel_tab_and_path_seams_hold COMMAND synthkit_panel_probe)
add_test(NAME synthkit.panel_seam_checks_catch_their_own_defects COMMAND synthkit_panel_probe --selftest)
