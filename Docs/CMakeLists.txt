cmake_minimum_required(VERSION 3.28)

project(QuietDocumentation LANGUAGES NONE)
find_package(Doxygen 1.9.6 REQUIRED COMPONENTS doxygen)

get_filename_component(
        QUIET_DOCS_SOURCE_ROOT
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
        ABSOLUTE
)

set(QUIET_DOCS_OUTPUT_DIR "${QUIET_DOCS_SOURCE_ROOT}/Build/Docs")
set(QUIET_DOCS_THEME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/DoxygenAwesome")
set(QUIET_DOCS_CONFIG "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile")

if(NOT EXISTS "${QUIET_DOCS_THEME_DIR}/doxygen-awesome.css")
    message(FATAL_ERROR
            "Missing Doxygen Awesome submodule. "
            "Run 'git submodule update --init --recursive' "
            "from the Quiet repository root."
    )
endif()

configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in"
        "${QUIET_DOCS_CONFIG}"
        @ONLY
)

add_custom_target(Quiet_Docs
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${QUIET_DOCS_OUTPUT_DIR}"
        COMMAND Doxygen::doxygen "${QUIET_DOCS_CONFIG}"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${QUIET_DOCS_SOURCE_ROOT}/Docs/Brand"
        "${QUIET_DOCS_OUTPUT_DIR}/html/Docs/Brand"
        WORKING_DIRECTORY "${QUIET_DOCS_SOURCE_ROOT}"
        COMMENT "Generating Quiet documentation"
        VERBATIM
)