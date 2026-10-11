find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Network)

add_test_target(core_test core/core_test.cpp)
target_link_libraries(core_test PRIVATE LibCore)

add_test_target(agent_adapter_test agent/agent_adapter_test.cpp)
target_link_libraries(agent_adapter_test PRIVATE LibAgent)

add_test_target(imagery_tile_source_test imagery/tile_source_test.cpp)
target_link_libraries(imagery_tile_source_test PRIVATE LibImagery)

add_test_target(orchestration_conversation_service_test orchestration/conversation_service_test.cpp)
target_link_libraries(orchestration_conversation_service_test PRIVATE LibOrchestration)

add_test_target(orchestration_harness_config_test orchestration/harness_config_test.cpp)
target_link_libraries(orchestration_harness_config_test PRIVATE LibOrchestration)

add_test_target(events_test events/events_test.cpp)
target_link_libraries(events_test PRIVATE LibEvents)

add_test_target(config_test config/config_test.cpp)
target_link_libraries(config_test PRIVATE LibProvider)

add_test_target(persistence_test persistence/persistence_test.cpp)
target_link_libraries(persistence_test PRIVATE LibPersistence)

add_test_target(provider_test provider/provider_test.cpp)
target_link_libraries(provider_test PRIVATE LibProvider)

add_test_target(agent_facade_test agent/agent_facade_test.cpp)
target_link_libraries(agent_facade_test PRIVATE LibAgent)

add_test_target(interaction_test interaction/interaction_test.cpp)
target_link_libraries(interaction_test PRIVATE LibInteraction)

add_test_target(interaction_context_test interaction/interaction_context_test.cpp)
target_link_libraries(interaction_context_test PRIVATE LibInteraction)

add_test_target(interaction_selection_test interaction/interaction_selection_test.cpp)
target_link_libraries(interaction_selection_test PRIVATE LibInteraction)

add_test_target(interaction_trace_test interaction/interaction_trace_test.cpp)
target_link_libraries(interaction_trace_test PRIVATE LibInteraction)

add_test_target(interaction_layers_test interaction/interaction_layers_test.cpp)
target_link_libraries(interaction_layers_test PRIVATE LibInteraction)

add_test_target(interaction_actions_test interaction/interaction_actions_test.cpp)
target_link_libraries(interaction_actions_test PRIVATE LibInteraction)

add_test_target(interaction_navigation_test interaction/interaction_navigation_test.cpp)
target_link_libraries(interaction_navigation_test PRIVATE LibInteraction)

add_test_target(interaction_commands_test interaction/interaction_commands_test.cpp)
target_link_libraries(interaction_commands_test PRIVATE LibInteraction)

add_test_target(markdown_parser_test markdown/markdown_parser_test.cpp)
target_link_libraries(markdown_parser_test PRIVATE LibMarkdown)

add_test_target(regex_facade_test regex/regex_facade_test.cpp)
target_link_libraries(regex_facade_test PRIVATE LibRegex)

add_test_target(markdown_image_service_test markdown/markdown_image_service_test.cpp)
target_sources(markdown_image_service_test PRIVATE ../src/qobjects/markdown_image_service.cpp)
target_include_directories(markdown_image_service_test PRIVATE ../src/qobjects ../src/include)
set_target_properties(markdown_image_service_test PROPERTIES AUTOMOC ON)
target_link_libraries(markdown_image_service_test PRIVATE LibConfig Qt6::Core Qt6::Gui Qt6::Network)

set(MARKDOWN_QML_IMPORT_DIR "${CMAKE_BINARY_DIR}/markdown-qml-imports")
file(MAKE_DIRECTORY "${MARKDOWN_QML_IMPORT_DIR}/Lib/Interaction")
foreach(QML_IMPORT_LINK
    "${MARKDOWN_QML_IMPORT_DIR}/Lib/Controls|${CMAKE_BINARY_DIR}/src/qml/controls"
    "${MARKDOWN_QML_IMPORT_DIR}/Lib/Theme|${CMAKE_BINARY_DIR}/src/qml/theme"
    "${MARKDOWN_QML_IMPORT_DIR}/Lucide|${CMAKE_BINARY_DIR}/src/qml/lucide/Lucide"
    "${MARKDOWN_QML_IMPORT_DIR}/Lib/Interaction/qmldir|${CMAKE_BINARY_DIR}/src/qmldir"
    "${MARKDOWN_QML_IMPORT_DIR}/Lib/Interaction/libinteractionuiplugin.so|${CMAKE_BINARY_DIR}/src/libinteractionuiplugin.so"
)
    string(REPLACE "|" ";" QML_IMPORT_LINK_PARTS "${QML_IMPORT_LINK}")
    list(GET QML_IMPORT_LINK_PARTS 0 QML_IMPORT_LINK_PATH)
    list(GET QML_IMPORT_LINK_PARTS 1 QML_IMPORT_TARGET_PATH)
    if(NOT EXISTS "${QML_IMPORT_LINK_PATH}")
        file(CREATE_LINK "${QML_IMPORT_TARGET_PATH}" "${QML_IMPORT_LINK_PATH}" SYMBOLIC)
    endif()
endforeach()

find_program(
    QMLTESTRUNNER_EXECUTABLE
    NAMES qmltestrunner
    HINTS "${Qt6Core_DIR}/../../qt6/bin"
    REQUIRED
)
add_test(
    NAME markdown_qml_renderer_test
    COMMAND
        "${QMLTESTRUNNER_EXECUTABLE}"
        -import "${MARKDOWN_QML_IMPORT_DIR}"
        -input "${CMAKE_CURRENT_SOURCE_DIR}/markdown/tst_markdownrenderer.qml"
)
set_tests_properties(markdown_qml_renderer_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "markdown;qml"
)
add_test(
    NAME interaction_qml_action_controls_test
    COMMAND
        "${QMLTESTRUNNER_EXECUTABLE}"
        -import "${MARKDOWN_QML_IMPORT_DIR}"
        -input "${CMAKE_CURRENT_SOURCE_DIR}/interaction/tst_action_controls.qml"
)
set_tests_properties(interaction_qml_action_controls_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "interaction;qml"
)

add_test_target(map_view_test map/map_view_test.cpp)
target_sources(map_view_test PRIVATE ../src/qobjects/map_view.cpp)
target_include_directories(map_view_test PRIVATE ../src/qobjects)
target_link_libraries(map_view_test PRIVATE Qt6::Core)
