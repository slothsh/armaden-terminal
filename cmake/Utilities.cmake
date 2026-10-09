function(find_package_manager_brew COMMAND)
    find_program(BREW_COMMAND brew)
    if(BREW_COMMAND)
        set(${COMMAND} ${BREW_COMMAND} PARENT_SCOPE)
    endif()
endfunction()

function(find_toolchain_config_linux COMMAND)
    # TODO: get current distro then set package manager based on that
    find_program(LLVM_CONFIG_COMMAND llvm-config)
    if(LLVM_CONFIG_COMMAND)
        set(${COMMAND} ${LLVM_CONFIG_COMMAND} PARENT_SCOPE)
    endif()
endfunction()

function(add_test_target TARGET_NAME SOURCE_FILES)
    add_executable(${TARGET_NAME} ${SOURCE_FILES})
    add_dependencies(${TARGET_NAME} ${CXX_TARGET_PRECOMPILED_STD})
    target_compile_options(${TARGET_NAME} PRIVATE -fmodule-file=std=${CXX_STD_PCM})
    target_link_libraries(${TARGET_NAME} PRIVATE Catch2::Catch2WithMain)
    catch_discover_tests(${TARGET_NAME} ADD_TAGS_AS_LABELS)
endfunction()

function(add_executable_target TARGET_NAME)
    cmake_parse_arguments(
        PARSE_ARGV 1
        ARG
        ""
        ""
        "SOURCE_FILES;MODULE_FILES;THIRD_PARTY_MODULE_FILES;BASE_DIRS"
    )

    add_executable(${TARGET_NAME} ${ARG_SOURCE_FILES})
    target_sources(
        ${TARGET_NAME}
        PUBLIC
        FILE_SET cxx_modules
        TYPE CXX_MODULES
        BASE_DIRS
            ${ARG_BASE_DIRS}
        FILES
            ${ARG_MODULE_FILES}
            ${ARG_THIRD_PARTY_MODULE_FILES}
    )

    foreach(SOURCE_FILE ${ARG_SOURCE_FILES})
        configure_module_file(${SOURCE_FILE} ${TARGET_NAME} IS_SOURCE_FILE)
    endforeach()

    foreach(MODULE_FILE ${ARG_MODULE_FILES})
        configure_module_file(${MODULE_FILE} ${TARGET_NAME})
    endforeach()

    add_dependencies(${TARGET_NAME} ${CXX_TARGET_PRECOMPILED_STD})
    target_compile_options(${TARGET_NAME} PRIVATE -fmodule-file=std=${CXX_STD_PCM})
endfunction()

function(add_qml_module_target TARGET_NAME)
    cmake_parse_arguments(
        PARSE_ARGV 1
        ARG
        ""
        ""
        "URI;VERSION;BASE_DIRS;QML_SOURCE_FILES;QML_MODULE_FILES"
    )

    set(QML_SOURCE_FILES "${ARG_QML_SOURCE_FILES}")
    set(QML_MODULE_FILES "${ARG_QML_MODULE_FILES}")

    foreach(QML_FILE_LIST QML_SOURCE_FILES QML_MODULE_FILES)
        set(RELATIVE_QML_FILES)

        foreach(QML_FILE IN LISTS ${QML_FILE_LIST})
            get_filename_component(QML_FILE_ABSOLUTE "${QML_FILE}" ABSOLUTE)

            foreach(BASE_DIR IN LISTS ARG_BASE_DIRS)
                get_filename_component(BASE_DIR_ABSOLUTE "${BASE_DIR}" ABSOLUTE)
                file(RELATIVE_PATH RELATIVE_QML_FILE
                    "${BASE_DIR_ABSOLUTE}"
                    "${QML_FILE_ABSOLUTE}"
                )

                if(
                    NOT RELATIVE_QML_FILE MATCHES "^\\.\\.(/|$)"
                    AND NOT RELATIVE_QML_FILE STREQUAL "."
                )
                    set(QML_FILE "${RELATIVE_QML_FILE}")
                    break()
                endif()
            endforeach()

            list(APPEND RELATIVE_QML_FILES "${QML_FILE}")
        endforeach()

        set(${QML_FILE_LIST} "${RELATIVE_QML_FILES}")
    endforeach()

    qt_add_qml_module(
        ${TARGET_NAME}
        URI ${ARG_URI}
        VERSION ${ARG_VERSION}
        SOURCES
            ${QML_SOURCE_FILES}
        QML_FILES
            ${QML_MODULE_FILES}
    )

    set(QML_CPP_SOURCE_FILES ${ARG_QML_SOURCE_FILES})
    list(FILTER QML_CPP_SOURCE_FILES EXCLUDE REGEX "(\\.hpp|\\.h)$")

    foreach(QML_SOURCE_FILE IN LISTS QML_CPP_SOURCE_FILES)
        configure_module_file("${QML_SOURCE_FILE}" ${TARGET_NAME} IS_SOURCE_FILE)
    endforeach()
endfunction()

function(add_library_target TARGET_NAME)
    cmake_parse_arguments(
        PARSE_ARGV 1
        ARG
        "SHARED;STATIC"
        ""
        "SOURCE_FILES;MODULE_FILES;THIRD_PARTY_MODULE_FILES;BASE_DIRS"
    )

    set(LIB_TYPE "")
    if(ARG_SHARED)
        set(LIB_TYPE SHARED)
    elseif(ARG_STATIC)
        set(LIB_TYPE STATIC)
    endif()

    add_library(${TARGET_NAME} ${LIB_TYPE} ${ARG_SOURCE_FILES})
    target_sources(
        ${TARGET_NAME}
        PUBLIC
        FILE_SET cxx_modules
        TYPE CXX_MODULES
        BASE_DIRS
            ${ARG_BASE_DIRS}
        FILES
            ${ARG_MODULE_FILES}
            ${ARG_THIRD_PARTY_MODULE_FILES}
    )

    foreach(MODULE_FILE ${ARG_MODULE_FILES})
        configure_module_file(${MODULE_FILE} ${TARGET_NAME})
    endforeach()

    add_dependencies(${TARGET_NAME} ${CXX_TARGET_PRECOMPILED_STD})
    target_compile_options(${TARGET_NAME} PRIVATE -fmodule-file=std=${CXX_STD_PCM})
endfunction()

function(configure_module_file MODULE_FILE TARGET_NAME)
    cmake_parse_arguments(
        PARSE_ARGV 2
        ARG
        "IS_SOURCE_FILE"
        ""
        ""
    )

    get_filename_component(MODULE_NAME ${MODULE_FILE} NAME_WE)
    string(SHA256 TAG_HASH "${TARGET_NAME}_${MODULE_NAME}")
    string(SUBSTRING "${TAG_HASH}" 0 7 HEX_VAL)
    math(EXPR MODULE_ID "0x${HEX_VAL}" OUTPUT_FORMAT DECIMAL)

    set(MODULE_INFO_FILE "${CMAKE_BINARY_DIR}/headers/in/${TARGET_NAME}_${MODULE_NAME}_${MODULE_ID}.hpp")
    set(LIBRARY_NAME_DEFINITION "${TARGET_NAME}")
    set(MODULE_NAME_DEFINITION "${MODULE_NAME}")

    configure_file(
        "${PROJECT_TEMPLATE_HEADERS_IN_DIR}/module_global_header.hpp.in"
        ${MODULE_INFO_FILE}
        @ONLY
    )

    set(COMPILE_DEFINITIONS
        "LIBRARY_NAME_DEFINITION=\"${TARGET_NAME}\""
        "MODULE_NAME_DEFINITION=\"${MODULE_NAME}\""
        "MODULE_ID=${MODULE_ID}"
    )

    if(NOT ARG_IS_SOURCE_FILE)
        list(APPEND COMPILE_DEFINITIONS MODULE_FILE_TRANSLATION_UNIT)
    endif()

    set_source_files_properties(
        ${MODULE_FILE}
        PROPERTIES
        COMPILE_OPTIONS
        "-include${MODULE_INFO_FILE}"
        COMPILE_DEFINITIONS
        "${COMPILE_DEFINITIONS}"
    )
endfunction()
