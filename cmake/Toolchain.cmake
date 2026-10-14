# -- Initialization Macros -----------------------------------------------------

macro(initialize_toolchain)
    set_global_options()
    prepare_cpm()
    prepare_build_paths()
    prepare_toolchain()
    include(cmake/Vendor.cmake)
endmacro()

macro(prepare_testing)
    find_package(Catch2 3 REQUIRED)
    enable_testing()
    include(CTest)
    include(Catch)
endmacro()

macro(prepare_cpm)
    set(CPM_SOURCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")
    set(CPM_DOWNLOAD_URL "https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.40.8/CPM.cmake")
    set(CPM_EXPECTED_HASH "78ba32abdf798bc616bab7c73aac32a17bbd7b06ad9e26a6add69de8f3ae4791")

    if(NOT EXISTS ${CPM_SOURCE_PATH})
        message(STATUS "CPM source file does not exist, downloading...")
        set(NEED_DOWNLOAD TRUE)
    else()
        file(SHA256 ${CPM_SOURCE_PATH} FILE_HASH)
        string(REPLACE "SHA256=" "" EXPECTED_HASH_VALUE ${CPM_EXPECTED_HASH})
        
        if(NOT FILE_HASH STREQUAL EXPECTED_HASH_VALUE)
            message(STATUS "CPM file hash mismatch")
            message(STATUS "  Expected: ${EXPECTED_HASH_VALUE}")
            message(STATUS "  Got:      ${FILE_HASH}")
            message(STATUS "Re-downloading")
            set(NEED_DOWNLOAD TRUE)
        endif()
    endif()

    if(NEED_DOWNLOAD)
        file(DOWNLOAD
            ${CPM_DOWNLOAD_URL}
            "${CPM_SOURCE_PATH}"
            EXPECTED_HASH SHA256=${CPM_EXPECTED_HASH}
            SHOW_PROGRESS
            STATUS download_status
            TIMEOUT 60
        )

        list(GET download_status 0 status_code)
        if(NOT status_code EQUAL 0)
            list(GET download_status 1 error_message)
            message(FATAL_ERROR "CPM download failed: ${error_message}")
        endif()
    endif()

    if(EXISTS ${CPM_SOURCE_PATH})
        if(NOT DEFINED ENV{CPM_SOURCE_CACHE})
            set(ENV{CPM_SOURCE_CACHE} ${CMAKE_CURRENT_SOURCE_DIR}/.cpm-cache)
        endif()
        include(${CPM_SOURCE_PATH})
    endif()
endmacro()

macro(set_global_options)
    set(CMAKE_CXX_STANDARD 26)
    set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

    # Support c++ modules
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
    set(CMAKE_CXX_EXTENSIONS OFF)

    set(PROJECT_HEADERS_IN_DIR "${CMAKE_BINARY_DIR}/headers/in")
    set(PROJECT_TEMPLATE_HEADERS_IN_DIR "${CMAKE_SOURCE_DIR}/templates")
endmacro()

macro(prepare_build_paths)
    file(MAKE_DIRECTORY ${PROJECT_HEADERS_IN_DIR})
    include_directories(${PROJECT_HEADERS_IN_DIR})
endmacro()

macro(prepare_toolchain)
    if(UNIX AND NOT APPLE)
        message(STATUS "Preparing toolchain for Linux")

        find_toolchain_config_linux(PKG_MANAGER_COMMAND)

        if(DEFINED PKG_MANAGER_COMMAND)
            message(STATUS "Found toolchain config: ${PKG_MANAGER_COMMAND}")
        else()
            message(FATAL_ERROR "System package manager not found")
        endif()

        message(STATUS "Searching for Clang LLVM toolchain")

        prepare_linux_llvm_toolchain(TOOLCHAIN_NAME)

        if(DEFINED TOOLCHAIN_NAME)
            execute_process(
                 COMMAND ${CMAKE_CXX_COMPILER} --version
                 OUTPUT_VARIABLE llvm_version
            )
            message(STATUS "Toolchain: ${TOOLCHAIN_NAME}")
            message(STATUS "${llvm_version}")
        else()
            message(FATAL_ERROR "Toolchain not found")
        endif()


        prepare_linux_llvm_modules_support(MODULES_SUPPORT_SUCCESS)

        if(MODULES_SUPPORT_SUCCESS)
            message(STATUS "C++ modules configured for ${TOOLCHAIN_NAME} toolchain")
        else()
            message(FATAL_ERROR "Could not prepare toolchain")
        endif()
    elseif(APPLE)
        message(STATUS "Preparing toolchain for Apple/Darwin")

        find_package_manager_brew(BREW_COMMAND)

        if(DEFINED BREW_COMMAND)
            message(STATUS "Found Homebrew package manager: ${BREW_COMMAND}")
        else()
            message(FATAL_ERROR "System package manager not found")
        endif()

        message(STATUS "Searching for Clang LLVM toolchain")

        prepare_darwin_llvm_toolchain(TOOLCHAIN_NAME)

        if(DEFINED TOOLCHAIN_NAME)
            execute_process(
                 COMMAND ${CMAKE_CXX_COMPILER} --version
                 OUTPUT_VARIABLE llvm_version
            )
            message(STATUS "Toolchain: ${TOOLCHAIN_NAME}")
            message(STATUS "${llvm_version}")
        else()
            message(FATAL_ERROR "Toolchain not found")
        endif()

        prepare_darwin_llvm_modules_support(MODULES_SUPPORT_SUCCESS)

        if(MODULES_SUPPORT_SUCCESS)
            message(STATUS "C++ modules configured for ${TOOLCHAIN_NAME} toolchain")
        else()
            message(FATAL_ERROR "Could not prepare toolchain")
        endif()
    elseif(WIN32)
        message(FATAL_ERROR "todo: not implemented")
    else()
        message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
    endif()
endmacro()

# ------------------------------------------------------------------------------


# -- Prepare Platform Toolchains -----------------------------------------------

function(prepare_linux_llvm_toolchain NAME)
    execute_process(
        COMMAND ${LLVM_CONFIG_COMMAND} --prefix
        OUTPUT_VARIABLE LLVM_ROOT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    if(NOT LLVM_ROOT)
        message(FATAL_ERROR "Could not find root directory for LLVM toolchain")
    endif()

    execute_process(
        COMMAND ${LLVM_CONFIG_COMMAND} --bindir
        OUTPUT_VARIABLE LLVM_BIN_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    if(NOT LLVM_BIN_DIR)
        message(FATAL_ERROR "Could not find bin directory for LLVM toolchain")
    endif()

    execute_process(
        COMMAND ${LLVM_CONFIG_COMMAND} --includedir
        OUTPUT_VARIABLE LLVM_INCLUDE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    if(NOT LLVM_INCLUDE_DIR)
        message(FATAL_ERROR "Could not find include directory for LLVM toolchain")
    endif()

    execute_process(
        COMMAND ${LLVM_CONFIG_COMMAND} --libdir
        OUTPUT_VARIABLE LLVM_LIB_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    if(NOT LLVM_LIB_DIR)
        message(FATAL_ERROR "Could not find lib directory for LLVM toolchain")
    endif()

    set(CMAKE_C_COMPILER "${LLVM_BIN_DIR}/clang" PARENT_SCOPE)
    set(CMAKE_CXX_COMPILER "${LLVM_BIN_DIR}/clang++" PARENT_SCOPE)
    set(LLVM_LIBRARY_DIR "${LLVM_LIB_DIR}" PARENT_SCOPE)

    set(${NAME} "LLVM Clang" PARENT_SCOPE)
    set(LLVM_ROOT ${LLVM_ROOT} PARENT_SCOPE)
endfunction()

function(prepare_linux_llvm_modules_support SUCCESS)
    set(${SUCCESS} FALSE PARENT_SCOPE)

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++ -Wno-reserved-identifier -Wno-reserved-module-identifier" PARENT_SCOPE)

    file(GLOB LIBSTDCXX_STD_MODULE_SOURCES "/usr/include/c++/*/bits/std.cc")
    list(SORT LIBSTDCXX_STD_MODULE_SOURCES ORDER DESCENDING)
    list(LENGTH LIBSTDCXX_STD_MODULE_SOURCES LIBSTDCXX_STD_MODULE_SOURCE_COUNT)
    if(LIBSTDCXX_STD_MODULE_SOURCE_COUNT EQUAL 0)
        message(FATAL_ERROR "Could not find the libstdc++ standard module source")
    endif()
    list(GET LIBSTDCXX_STD_MODULE_SOURCES 0 LIBSTDCXX_STD_MODULE_SOURCE)

    add_custom_command(
        OUTPUT std.pcm
        COMMAND ${CMAKE_CXX_COMPILER}
                -std=c++${CMAKE_CXX_STANDARD}
                -stdlib=libstdc++
                -Wno-reserved-identifier
                -Wno-reserved-module-identifier
                --precompile
                -x c++-module
                -o std.pcm
                ${LIBSTDCXX_STD_MODULE_SOURCE}
        DEPENDS ${LIBSTDCXX_STD_MODULE_SOURCE} ${CMAKE_CXX_COMPILER}
        COMMENT "Precompiling libstdc++ standard module (std.pcm)"
    )

    add_custom_target(cxx_precompiled_std ALL DEPENDS std.pcm)

    set(CXX_TARGET_PRECOMPILED_STD cxx_precompiled_std PARENT_SCOPE)
    set(CXX_STD_PCM ${CMAKE_BINARY_DIR}/std.pcm PARENT_SCOPE)
    set(${SUCCESS} TRUE PARENT_SCOPE)
endfunction()

function(prepare_darwin_llvm_toolchain NAME)
    execute_process(
        COMMAND ${BREW_COMMAND} --prefix llvm
        OUTPUT_VARIABLE LLVM_ROOT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    if(LLVM_ROOT)
        set(CMAKE_C_COMPILER "${LLVM_ROOT}/bin/clang" PARENT_SCOPE)
        set(CMAKE_CXX_COMPILER "${LLVM_ROOT}/bin/clang++" PARENT_SCOPE)
        set(LLVM_LIBRARY_DIR "${LLVM_ROOT}/lib" PARENT_SCOPE)

        execute_process(
            COMMAND xcrun --show-sdk-path
            OUTPUT_VARIABLE APPLE_SDK_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        add_compile_options(-isysroot "${APPLE_SDK_PATH}")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${LLVM_ROOT}/lib/c++ -Wl,-rpath,${LLVM_ROOT}/lib/c++" PARENT_SCOPE)
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -L${LLVM_ROOT}/lib/c++ -Wl,-rpath,${LLVM_ROOT}/lib/c++" PARENT_SCOPE)
        set(LLVM_LIBRARY_DIR "${LLVM_ROOT}/lib" PARENT_SCOPE)

        set(${NAME} "LLVM Clang" PARENT_SCOPE)
        set(LLVM_ROOT ${LLVM_ROOT} PARENT_SCOPE)
    endif()
endfunction()

function(prepare_darwin_llvm_modules_support SUCCESS)
    set(${SUCCESS} FALSE PARENT_SCOPE)

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -Wno-reserved-identifier -Wno-reserved-module-identifier" PARENT_SCOPE)

    add_custom_command(
        OUTPUT std.pcm
        COMMAND ${CMAKE_CXX_COMPILER}
                -std=c++${CMAKE_CXX_STANDARD}
                -stdlib=libc++
                -Wno-reserved-identifier
                -Wno-reserved-module-identifier
                --precompile
                -o std.pcm
                ${LLVM_ROOT}/share/libc++/v1/std.cppm
        DEPENDS ${LLVM_ROOT}/share/libc++/v1/std.cppm ${CMAKE_CXX_COMPILER}
        COMMENT "Precompiling libc++ standard module (std.pcm)"
    )

    add_custom_target(cxx_precompiled_std ALL DEPENDS std.pcm)

    set(CXX_TARGET_PRECOMPILED_STD cxx_precompiled_std PARENT_SCOPE)
    set(CXX_STD_PCM ${CMAKE_BINARY_DIR}/std.pcm PARENT_SCOPE)
    set(${SUCCESS} TRUE PARENT_SCOPE)
endfunction()

# ------------------------------------------------------------------------------
