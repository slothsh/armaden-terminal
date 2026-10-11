add_library(md4c STATIC "${md4c_SOURCE_DIR}/src/md4c.c")
target_include_directories(md4c PUBLIC "${md4c_SOURCE_DIR}/src")

# Third-party module source files
file(GLOB_RECURSE MAGIC_ENUM_MODULE_SRC_FILES CONFIGURE_DEPENDS "${magic_enum_SOURCE_DIR}/module/*.cppm")

add_library_target(
    LibMagicEnum
    STATIC
    BASE_DIRS
        "${magic_enum_SOURCE_DIR}/module"
    THIRD_PARTY_MODULE_FILES
        ${MAGIC_ENUM_MODULE_SRC_FILES}
)

set_target_properties(
    LibMagicEnum
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)

target_include_directories(
    LibMagicEnum
    PUBLIC
        "${magic_enum_SOURCE_DIR}/include/"
)
