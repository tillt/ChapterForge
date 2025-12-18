cmake_minimum_required(VERSION 3.16)

# Base version is "dev"; Git tags override when present.
project(ChapterForge VERSION 0.0.0 LANGUAGES CXX)

set(CHAPTERFORGE_VERSION_STRING "dev")
set(CHAPTERFORGE_VERSION_DEV TRUE)
set(CHAPTERFORGE_GIT_HASH "unknown")
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found; fetching header-only dependency")
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()
find_program(GIT_EXE git)
if(GIT_EXE)
    execute_process(
        COMMAND ${GIT_EXE} describe --tags --dirty --abbrev=7
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE GIT_DESC_RES
        OUTPUT_VARIABLE GIT_DESC_OUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(GIT_DESC_RES EQUAL 0 AND NOT GIT_DESC_OUT STREQUAL "")
        set(CHAPTERFORGE_VERSION_STRING "${GIT_DESC_OUT}")
        if(GIT_DESC_OUT MATCHES "-")
            set(CHAPTERFORGE_VERSION_DEV TRUE)
        else()
            set(CHAPTERFORGE_VERSION_DEV FALSE)
        endif()
    endif()
    execute_process(
        COMMAND ${GIT_EXE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE GIT_HASH_RES
        OUTPUT_VARIABLE GIT_HASH_OUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(GIT_HASH_RES EQUAL 0 AND NOT GIT_HASH_OUT STREQUAL "")
        set(CHAPTERFORGE_GIT_HASH "${GIT_HASH_OUT}")
    endif()
endif()
if(CHAPTERFORGE_VERSION_DEV)
    set(CHAPTERFORGE_VERSION_DISPLAY "${CHAPTERFORGE_VERSION_STRING}+${CHAPTERFORGE_GIT_HASH}")
else()
    set(CHAPTERFORGE_VERSION_DISPLAY "${CHAPTERFORGE_VERSION_STRING}")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# macOS universal helper (opt-in).
if(APPLE)
    option(CHAPTERFORGE_UNIVERSAL "Build arm64 + x86_64 universal binaries on macOS" OFF)
    if(CHAPTERFORGE_UNIVERSAL)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures" FORCE)
        message(STATUS "CHAPTERFORGE_UNIVERSAL=ON -> CMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}")
    endif()
endif()

option(ENABLE_ASAN "Enable AddressSanitizer for ChapterForge" OFF)
if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -fno-common)
    add_link_options(-fsanitize=address -fno-omit-frame-pointer -fno-common)
    message(STATUS "AddressSanitizer enabled")
endif()

#Core sources(no CLI) so we can build a reusable library
set(CHAPTERFORGE_CORE_SOURCES
    src/aac_extractor.cpp
    src/dinf_builder.cpp
    src/hdlr_builder.cpp
    src/jpeg_entry_builder.cpp
    src/jpeg_info.cpp
    src/logging.cpp
    src/mdat_writer.cpp
    src/mdhd_builder.cpp
    src/mdia_builder.cpp
    src/meta_builder.cpp
    src/minf_builder.cpp
    src/moov_builder.cpp
    src/mp4_atoms.cpp
    src/mp4_muxer.cpp
    src/mp4a_builder.cpp
    src/mvhd_builder.cpp
    src/nmhd_builder.cpp
    src/parser.cpp
    src/smhd_builder.cpp
    src/stbl_audio_builder.cpp
    src/stbl_image_builder.cpp
    src/stbl_metadata_builder.cpp
    src/stbl_text_builder.cpp
    src/stco_builder.cpp
    src/stsc_builder.cpp
    src/stsd_builder.cpp
    src/stsz_builder.cpp
    src/stts_builder.cpp
    src/tkhd_builder.cpp
    src/trak_builder.cpp
    src/tx3g_stsd_builder.cpp
    src/udta_builder.cpp
    src/vmhd_builder.cpp
    src/chapterforge.cpp
)

add_library(chapterforge STATIC ${CHAPTERFORGE_CORE_SOURCES})
target_include_directories(chapterforge PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_link_libraries(chapterforge PUBLIC nlohmann_json::nlohmann_json)
file(GENERATE
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated/chapterforge_version.hpp
    CONTENT "//
//  chapterforge_version.hpp
//

#pragma once

#define CHAPTERFORGE_VERSION \"${CHAPTERFORGE_VERSION_STRING}\"
#define CHAPTERFORGE_VERSION_DISPLAY \"${CHAPTERFORGE_VERSION_DISPLAY}\"
#define CHAPTERFORGE_VERSION_IS_DEV ${CHAPTERFORGE_VERSION_DEV}
#define CHAPTERFORGE_GIT_HASH \"${CHAPTERFORGE_GIT_HASH}\"
"
)

#CLI target links against the library
add_executable(chapterforge_cli src/main.cpp)
target_link_libraries(chapterforge_cli PRIVATE chapterforge)

#clang - format helper
find_program(CLANG_FORMAT_EXE clang-format)
if(CLANG_FORMAT_EXE)
    file(GLOB_RECURSE FORMAT_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    )
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i -style=file ${FORMAT_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-format on sources"
    )
endif()

# clang-tidy helper (honors compile_commands.json in build dir).
find_program(CLANG_TIDY_EXE clang-tidy)
if(CLANG_TIDY_EXE)
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY_EXE}
            -p ${CMAKE_BINARY_DIR}
            -checks=modernize-*,bugprone-*,readability-braces-around-statements
            ${CHAPTERFORGE_CORE_SOURCES} src/main.cpp tests/*.cpp
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-tidy (modernize/bugprone/readability checks)"
    )
endif()

# README TOC helper
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_custom_target(toc
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/update_readme_toc.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Regenerate README.md Table of Contents"
    )

    # Fails if README.md would change (useful in CI/lint).
    add_custom_target(toc_check
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md
                ${CMAKE_CURRENT_BINARY_DIR}/README.md.toc_check
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/update_readme_toc.py
        COMMAND ${CMAKE_COMMAND} -E compare_files
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md
                ${CMAKE_CURRENT_BINARY_DIR}/README.md.toc_check
        COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_CURRENT_BINARY_DIR}/README.md.toc_check
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Regenerate README.md TOC (fails if outdated)"
    )
endif()

set(TEST_INPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/testdata" CACHE PATH "Directory containing sample input/output media")
# Backward-compatible alias; prefer TEST_INPUT_DIR going forward.
set(TESTDATA_DIR "${TEST_INPUT_DIR}" CACHE PATH "Alias for TEST_INPUT_DIR")
set(TEST_OUTPUT_DIR "${CMAKE_BINARY_DIR}/test_outputs" CACHE PATH "Directory for generated test outputs")
file(MAKE_DIRECTORY "${TEST_OUTPUT_DIR}")
set(OUTPUT_M4A_PATH "${TEST_OUTPUT_DIR}/output.m4a" CACHE STRING "Path to the output M4A to inspect with tools" FORCE)
set(INPUT_M4A_PATH "${TEST_INPUT_DIR}/input.m4a" CACHE STRING "Path to the source input M4A to inspect with tools")
set(INPUT_AAC_PATH "${TEST_INPUT_DIR}/input.aac" CACHE STRING "Path to the source AAC (ADTS) file for AAC ingest test")
set(GOLDEN_M4A_PATH "${TEST_INPUT_DIR}/golden_chapters.m4a" CACHE STRING "Path to the golden M4A reference for chapter images")
set(CHAPTER_JSON_PATH "${TEST_INPUT_DIR}/chapters.json" CACHE STRING "Path to the primary chapters JSON")
set(INPUT_LARGE_M4A_PATH "${TEST_INPUT_DIR}/input_large.m4a" CACHE STRING "Path to the large input M4A for large image tests")
set(INPUT_LARGE_AAC_PATH "${TEST_INPUT_DIR}/input_large.aac" CACHE STRING "Path to the large input AAC (ADTS) for large image AAC ingest tests")
# Common tooling defaults for tests; can be overridden by the user.
if(NOT DEFINED XXD_PATH OR NOT XXD_PATH)
    find_program(XXD_PATH xxd)
endif()

enable_testing()

# Always provide a fixture that regenerates synthetic assets before tests need them.
    add_test(
        NAME generate_assets
        COMMAND ${CMAKE_COMMAND}
            -DPROJECT_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_assets.cmake
    )
    set_tests_properties(generate_assets PROPERTIES FIXTURES_SETUP assets)
    set_tests_properties(generate_assets PROPERTIES LABELS "core")

add_custom_target(generate_test_assets
    COMMAND ${CMAKE_COMMAND} -E env bash ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_test_audio.sh
    COMMAND ${CMAKE_COMMAND} -E env bash ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_test_images.sh
    COMMAND ${CMAKE_COMMAND} -E env bash ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_test_json.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating synthetic test audio/images into ${TESTDATA_DIR}"
)

find_package(nlohmann_json QUIET)
add_executable(parser_fallback_check tests/parser_fallback.cpp)
target_link_libraries(parser_fallback_check PRIVATE chapterforge)
target_include_directories(parser_fallback_check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME parser_fallback_check COMMAND parser_fallback_check)
set_tests_properties(parser_fallback_check PROPERTIES FIXTURES_REQUIRED assets)
set_tests_properties(parser_fallback_check PROPERTIES LABELS "core")

# Safety regression: refuse absurd atom sizes / overflows without crashing.
add_executable(parser_safety_check tests/parser_safety.cpp)
target_link_libraries(parser_safety_check PRIVATE chapterforge)
target_include_directories(parser_safety_check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME parser_safety_check COMMAND parser_safety_check)
set_tests_properties(parser_safety_check PROPERTIES LABELS "core")

if(nlohmann_json_FOUND)
    add_executable(image_fixtures tests/image_fixtures.cpp)
    target_link_libraries(image_fixtures PRIVATE chapterforge nlohmann_json::nlohmann_json)
    target_include_directories(image_fixtures PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    add_test(NAME image_fixtures COMMAND image_fixtures "${TESTDATA_DIR}")
    set_tests_properties(image_fixtures PROPERTIES FIXTURES_REQUIRED assets)
    set_tests_properties(image_fixtures PROPERTIES LABELS "core")
else()
    message(WARNING "nlohmann_json not found; image_fixtures test will be skipped")
endif()

add_executable(overload_variants tests/overload_variants.cpp)
target_link_libraries(overload_variants PRIVATE chapterforge)
target_include_directories(overload_variants PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME overload_variants
    COMMAND overload_variants "${INPUT_M4A_PATH}" "${TESTDATA_DIR}/images/cover.jpg" "${TEST_OUTPUT_DIR}")
set_tests_properties(overload_variants PROPERTIES FIXTURES_REQUIRED assets)
set_tests_properties(overload_variants PROPERTIES LABELS "core")

# Documentation (HTML via Doxygen)
if(ENABLE_DOCS)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        set(DOXYFILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
        set(DOXYFILE_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
        configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)
        add_custom_target(docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${CMAKE_CURRENT_BINARY_DIR}/docs/html
                    ${CMAKE_CURRENT_SOURCE_DIR}/docs/html
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation in ${CMAKE_CURRENT_BINARY_DIR}/docs/html"
            VERBATIM
        )

    else()
        message(WARNING "ENABLE_DOCS=ON but Doxygen not found; docs target will not be generated.")
    endif()
endif()

option(ENABLE_OUTPUT_TOOL_TESTS "Run tooling checks on an existing output.m4a" OFF)
option(ENABLE_STRICT_VALIDATION "Run stricter validation with multiple parsers (mp4info/mp4dump/ffprobe/MP4Box)" OFF)
option(ENABLE_LARGE_IMAGE_TESTS "Run heavy chapter-image tests (requires large input_large.m4a and large JPEGs)" OFF)
option(ENABLE_VIDEO_DEBUG_COMPARE "Run optional video debug against a golden file (requires GOLDEN_M4A_PATH, mp4dump, ffprobe)" OFF)
option(ENABLE_AVFOUNDATION_SMOKE "Run AVFoundation smoke test (macOS only, requires swift)" OFF)
option(ENABLE_DOCS "Generate Doxygen HTML documentation" ON)

# Optional: produce a macOS Framework wrapping the static library.
option(ENABLE_MACOS_FRAMEWORK "Build ChapterForge.framework on macOS" OFF)

add_executable(chapter_start_rules
    tests/chapter_start_rules.cpp
)
target_link_libraries(chapter_start_rules PRIVATE chapterforge)
target_include_directories(chapter_start_rules PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(
    NAME chapter_start_rules
    COMMAND chapter_start_rules
)

# macOS Framework packaging (uses the existing static lib).
if(APPLE AND ENABLE_MACOS_FRAMEWORK)
    # Stage a static framework bundle that wraps the built static library.
    set(FRAMEWORK_NAME ChapterForge)
    set(FRAMEWORK_DIR ${CMAKE_CURRENT_BINARY_DIR}/${FRAMEWORK_NAME}.framework)
    set(FRAMEWORK_VER_DIR ${FRAMEWORK_DIR}/Versions/A)
    # Ship all public headers (developers can opt into lower-level APIs).
    file(GLOB FRAMEWORK_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp)

    add_custom_target(ChapterForgeFramework ALL
        DEPENDS chapterforge ${FRAMEWORK_HEADERS}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FRAMEWORK_VER_DIR}/Headers
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FRAMEWORK_VER_DIR}/Resources
        # Write a minimal Info.plist carrying our version strings.
        COMMAND ${CMAKE_COMMAND} -E echo "<?xml version=\\\"1.0\\\" encoding=\\\"UTF-8\\\"?>\n<!DOCTYPE plist PUBLIC \\\"-//Apple//DTD PLIST 1.0//EN\\\" \\\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\\\"> \n<plist version=\\\"1.0\\\"> \n<dict> \n    <key>CFBundleIdentifier</key><string>com.chapterforge</string> \n    <key>CFBundleName</key><string>ChapterForge</string> \n    <key>CFBundleVersion</key><string>${CHAPTERFORGE_VERSION_DISPLAY}</string> \n    <key>CFBundleShortVersionString</key><string>${CHAPTERFORGE_VERSION_DISPLAY}</string> \n</dict> \n</plist>" > ${FRAMEWORK_VER_DIR}/Resources/Info.plist
        # Copy the static lib into the bundle as the framework binary.
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:chapterforge> ${FRAMEWORK_VER_DIR}/${FRAMEWORK_NAME}
        # Copy public headers.
        COMMAND ${CMAKE_COMMAND} -E copy ${FRAMEWORK_HEADERS} ${FRAMEWORK_VER_DIR}/Headers
        # Symlinks to match standard framework layout.
        COMMAND ${CMAKE_COMMAND} -E create_symlink A ${FRAMEWORK_DIR}/Versions/Current
        COMMAND ${CMAKE_COMMAND} -E create_symlink Versions/Current/Headers ${FRAMEWORK_DIR}/Headers
        COMMAND ${CMAKE_COMMAND} -E create_symlink Versions/Current/Resources ${FRAMEWORK_DIR}/Resources
        COMMAND ${CMAKE_COMMAND} -E create_symlink Versions/Current/${FRAMEWORK_NAME} ${FRAMEWORK_DIR}/${FRAMEWORK_NAME}
        COMMENT "Staging static ${FRAMEWORK_NAME}.framework (universal if CHAPTERFORGE_UNIVERSAL=ON)"
        VERBATIM
    )
endif()
set_tests_properties(chapter_start_rules PROPERTIES LABELS "unit")

add_test(
    NAME image_missing_first
    COMMAND $<TARGET_FILE:chapterforge_cli>
        ${INPUT_M4A_PATH}
        ${TESTDATA_DIR}/chapters_10s_mixedimg.json
        ${TEST_OUTPUT_DIR}/output_mixedimg.m4a
)
set_tests_properties(image_missing_first PROPERTIES FIXTURES_REQUIRED assets LABELS "core")

add_test(
    NAME image_mixed_sizes
    COMMAND $<TARGET_FILE:chapterforge_cli>
        ${INPUT_M4A_PATH}
        ${TESTDATA_DIR}/chapters_10s_mixed_sizes.json
        ${TEST_OUTPUT_DIR}/output_mixed_sizes.m4a
)
set_tests_properties(image_mixed_sizes PROPERTIES FIXTURES_REQUIRED assets LABELS "core")

add_test(
    NAME image_mixed_sizes_warn
    COMMAND ${CMAKE_COMMAND}
        -DCHAPTERFORGE_BIN=$<TARGET_FILE:chapterforge_cli>
        -DINPUT_M4A=${INPUT_M4A_PATH}
        -DCHAPTER_JSON=${TESTDATA_DIR}/chapters_10s_mixed_sizes.json
        -DOUTPUT_M4A=${TEST_OUTPUT_DIR}/output_mixed_sizes_warn.m4a
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_image_mismatch_warn.cmake
)
set_tests_properties(image_mixed_sizes_warn PROPERTIES FIXTURES_REQUIRED assets LABELS "core")

if(ENABLE_OUTPUT_TOOL_TESTS)
    find_program(XXD_PATH xxd)
    find_program(MP4INFO_PATH mp4info)
    find_program(MP4DUMP_PATH mp4dump)
    find_program(ATOMIC_PARSLEY_PATH AtomicParsley)
    find_program(FFPROBE_PATH ffprobe)

    add_executable(audio_reuse_check
        tests/audio_reuse_check.cpp
    )
    target_link_libraries(audio_reuse_check PRIVATE chapterforge)
    target_include_directories(audio_reuse_check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_include_directories(audio_reuse_check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_test(
        NAME audio_reuse_check
        COMMAND audio_reuse_check "${INPUT_M4A_PATH}" "${OUTPUT_M4A_PATH}"
    )
    set_tests_properties(audio_reuse_check PROPERTIES FIXTURES_REQUIRED assets DEPENDS output_tool_checks)
    set_tests_properties(audio_reuse_check PROPERTIES LABELS "core")

    if(MP4DUMP_PATH)
        add_test(
            NAME offset_first_chapter_bento
            COMMAND ${CMAKE_COMMAND}
                -DINPUT_M4A=${INPUT_M4A_PATH}
                -DCHAPTER_JSON=${TESTDATA_DIR}/chapters_10s_offset_first.json
                -DOUTPUT_M4A=${TEST_OUTPUT_DIR}/output_offset_bento.m4a
                -DCHAPTERFORGE_BIN=$<TARGET_FILE:chapterforge_cli>
                -DMP4DUMP_PATH=${MP4DUMP_PATH}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_offset_with_bento.cmake
        )
        set_tests_properties(offset_first_chapter_bento PROPERTIES FIXTURES_REQUIRED assets LABELS "tooling")
        add_test(
            NAME url_text_payload
            COMMAND ${CMAKE_COMMAND}
                -DINPUT_M4A=${INPUT_M4A_PATH}
                -DCHAPTER_JSON=${TESTDATA_DIR}/chapters_10s_urltext.json
                -DOUTPUT_M4A=${TEST_OUTPUT_DIR}/output_urltext_payload.m4a
                -DCHAPTERFORGE_BIN=$<TARGET_FILE:chapterforge_cli>
                -DMP4DUMP_PATH=${MP4DUMP_PATH}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_url_text.cmake
        )
        set_tests_properties(url_text_payload PROPERTIES FIXTURES_REQUIRED assets LABELS "tooling")
    else()
        message(WARNING "mp4dump not found; skipping mp4dump-dependent tests (offset_first_chapter_bento, url_text_payload)")
    endif()

    if(APPLE)
        add_test(
            NAME url_text_avfoundation
            COMMAND ${CMAKE_COMMAND}
                -DCHAPTERFORGE_BIN=$<TARGET_FILE:chapterforge_cli>
                -DINPUT_M4A=${INPUT_M4A_PATH}
                -DCHAPTER_JSON=${TESTDATA_DIR}/chapters_10s_urltext.json
                -DOUTPUT_M4A=${TEST_OUTPUT_DIR}/output_urltext_avf.m4a
                -DPROJECT_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_avfoundation_urltext.cmake
        )
        set_tests_properties(url_text_avfoundation PROPERTIES FIXTURES_REQUIRED assets LABELS "tooling")
        add_test(
            NAME url_text_nohref_avfoundation
            COMMAND ${CMAKE_COMMAND}
                -DCHAPTERFORGE_BIN=$<TARGET_FILE:chapterforge_cli>
                -DINPUT_M4A=${INPUT_M4A_PATH}
                -DCHAPTER_JSON=${TESTDATA_DIR}/chapters_10s_urltext_nohref.json
                -DOUTPUT_M4A=${TEST_OUTPUT_DIR}/output_urltext_nohref_avf.m4a
                -DPROJECT_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_avfoundation_urltext.cmake
        )
        set_tests_properties(url_text_nohref_avfoundation PROPERTIES FIXTURES_REQUIRED assets LABELS "tooling")
    else()
        message(STATUS "Skipping AVFoundation url_text test on non-Apple platform")
    endif()

    add_test(
        NAME output_tool_checks
        COMMAND ${CMAKE_COMMAND}
            -DINPUT_M4A=${INPUT_M4A_PATH}
            -DOUTPUT_M4A=${OUTPUT_M4A_PATH}
            -DCHAPTER_JSON=${CHAPTER_JSON_PATH}
            -DCHAPTERFORGE_BIN=$<TARGET_FILE:chapterforge_cli>
            -DFASTSTART_FLAG=ON
            -DXXD_PATH=${XXD_PATH}
            -DMP4INFO_PATH=${MP4INFO_PATH}
            -DMP4DUMP_PATH=${MP4DUMP_PATH}
            -DATOMIC_PARSLEY_PATH=${ATOMIC_PARSLEY_PATH}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tools_with_mux.cmake
    )
    set_tests_properties(output_tool_checks PROPERTIES FIXTURES_REQUIRED assets LABELS "tooling")
#Metadata preservation on small input(JSON has no metadata)
    add_test(
        NAME metadata_preserve_small
        COMMAND ${CMAKE_COMMAND}
            -DINPUT_M4A=${INPUT_M4A_PATH}
            -DOUTPUT_M4A=${TEST_OUTPUT_DIR}/output_meta_preserve.m4a
            -DCHAPTER_JSON=${TESTDATA_DIR}/chapters_nometa.json
            -DCHAPTERFORGE_BIN=$<TARGET_FILE:chapterforge_cli>
            -DFASTSTART_FLAG=ON
            -DXXD_PATH=${XXD_PATH}
            -DMP4INFO_PATH=${MP4INFO_PATH}
            -DMP4DUMP_PATH=${MP4DUMP_PATH}
            -DATOMIC_PARSLEY_PATH=${ATOMIC_PARSLEY_PATH}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tools_with_mux.cmake
    )
    set_tests_properties(metadata_preserve_small PROPERTIES FIXTURES_REQUIRED assets LABELS "core")
#AAC input ingest->MP4, validated against the original input M4A for metadata / audio consistency.
    set(OUTPUT_AAC_M4A_PATH "${TEST_OUTPUT_DIR}/output_from_aac.m4a")
    if(EXISTS "${INPUT_AAC_PATH}")
        add_test(
            NAME output_tool_checks_aac
        COMMAND ${CMAKE_COMMAND}
            -DINPUT_AAC=${INPUT_AAC_PATH}
            -DINPUT_M4A=${INPUT_M4A_PATH}
            -DOUTPUT_M4A=${OUTPUT_AAC_M4A_PATH}
            -DCHAPTER_JSON=${CHAPTER_JSON_PATH}
            -DCHAPTERFORGE_BIN=$<TARGET_FILE:chapterforge_cli>
            -DFASTSTART_FLAG=ON
            -DXXD_PATH=${XXD_PATH}
            -DMP4INFO_PATH=${MP4INFO_PATH}
            -DMP4DUMP_PATH=${MP4DUMP_PATH}
            -DATOMIC_PARSLEY_PATH=${ATOMIC_PARSLEY_PATH}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_aac_with_mux.cmake
        )
        set_tests_properties(output_tool_checks_aac PROPERTIES FIXTURES_REQUIRED assets LABELS "tooling")
    else()
        message(STATUS "Skipping AAC ingest tests (INPUT_AAC_PATH missing: ${INPUT_AAC_PATH})")
    endif()

    # Convenience target: generate assets, then run all tests.
    add_custom_target(check_with_assets
        DEPENDS generate_test_assets chapterforge chapterforge_cli
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generate test assets then run ctest"
    )

    # Convenience: build everything then run the test suite.
    add_custom_target(check
        DEPENDS chapterforge chapterforge_cli
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Build targets then run ctest"
    )
endif()

if(ENABLE_STRICT_VALIDATION)
    find_program(XXD_PATH xxd)
    find_program(MP4INFO_PATH mp4info)
    find_program(MP4DUMP_PATH mp4dump)
    find_program(ATOMIC_PARSLEY_PATH AtomicParsley)
    find_program(FFPROBE_PATH ffprobe)
    find_program(MP4BOX_PATH MP4Box)

    add_test(
        NAME strict_validation
        COMMAND ${CMAKE_COMMAND}
            -DOUTPUT_M4A=${OUTPUT_M4A_PATH}
            -DXXD_PATH=${XXD_PATH}
            -DMP4INFO_PATH=${MP4INFO_PATH}
            -DMP4DUMP_PATH=${MP4DUMP_PATH}
            -DATOMIC_PARSLEY_PATH=${ATOMIC_PARSLEY_PATH}
            -DFFPROBE_PATH=${FFPROBE_PATH}
            -DMP4BOX_PATH=${MP4BOX_PATH}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/strict_validate.cmake
    )
    set_tests_properties(strict_validation PROPERTIES LABELS "tooling")
endif()

if(ENABLE_LARGE_IMAGE_TESTS)
#Paths to chapter JSONs with normal/large images; guarded by file existence
    set(NORMAL5_JSON "${TESTDATA_DIR}/chapters_10s_2ch_normalimg_meta.json")
    set(NORMAL_NOMETA_JSON "${TESTDATA_DIR}/chapters_10s_2ch_normalimg_nometa.json")
    set(NORMAL50_JSON "${TESTDATA_DIR}/chapters_250s_50ch_normalimg_meta.json")
    set(LARGE50_JSON "${TESTDATA_DIR}/chapters_250s_50ch_largeimg_meta.json")
    set(OUTPUT_NORMAL5 "${TEST_OUTPUT_DIR}/output_normal5.m4a" CACHE STRING "Normal 5-chapter output" FORCE)
    set(OUTPUT_NORMAL50 "${TEST_OUTPUT_DIR}/output_normal50.m4a" CACHE STRING "Normal 50-chapter output (800px art)" FORCE)
    set(OUTPUT_LARGE50 "${TEST_OUTPUT_DIR}/output_large50.m4a" CACHE STRING "Large 50-chapter output" FORCE)
    set(OUTPUT_LARGE_AAC "${TEST_OUTPUT_DIR}/output_large_from_aac.m4a" CACHE STRING "Large AAC ingest output" FORCE)

    if(EXISTS "${INPUT_M4A_PATH}" AND EXISTS "${NORMAL5_JSON}")
        add_test(
            NAME long_audio_largeimg_5
            COMMAND ${CMAKE_COMMAND}
            -DINPUT_M4A=${INPUT_M4A_PATH}
            -DCHAPTER_JSON=${NORMAL5_JSON}
            -DOUTPUT_M4A=${OUTPUT_NORMAL5}
            -DCHAPTERFORGE_BIN=$<TARGET_FILE:chapterforge_cli>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_big_image_test.cmake
        )
        set_tests_properties(long_audio_largeimg_5 PROPERTIES LABELS "core")
    else()
        message(STATUS "Skipping long_audio_largeimg_5 (missing INPUT_M4A_PATH or chapters_10s_2ch_normalimg_meta.json)")
    endif()

    if(EXISTS "${INPUT_LARGE_M4A_PATH}" AND EXISTS "${LARGE50_JSON}")
        add_test(
            NAME long_audio_largeimg_50
            COMMAND ${CMAKE_COMMAND}
            -DINPUT_M4A=${INPUT_LARGE_M4A_PATH}
            -DCHAPTER_JSON=${LARGE50_JSON}
            -DOUTPUT_M4A=${OUTPUT_LARGE50}
            -DCHAPTERFORGE_BIN=$<TARGET_FILE:chapterforge_cli>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_big_image_test.cmake
        )
        set_tests_properties(long_audio_largeimg_50 PROPERTIES LABELS "core")
    else()
        message(STATUS "Skipping long_audio_largeimg_50 (missing INPUT_LARGE_M4A_PATH or chapters_250s_50ch_largeimg_meta.json)")
    endif()

    # 50 chapters with small (400x400) art over the long input.
    set(SMALL50_JSON "${TESTDATA_DIR}/chapters_small_50.json")
    set(OUTPUT_SMALL50 "${TEST_OUTPUT_DIR}/output_small50.m4a" CACHE STRING "Small art 50-chapter output" FORCE)
    if(EXISTS "${INPUT_LARGE_M4A_PATH}" AND EXISTS "${SMALL50_JSON}")
        add_test(
            NAME long_audio_smallimg_50
            COMMAND ${CMAKE_COMMAND}
            -DINPUT_M4A=${INPUT_LARGE_M4A_PATH}
            -DCHAPTER_JSON=${SMALL50_JSON}
            -DOUTPUT_M4A=${OUTPUT_SMALL50}
            -DCHAPTERFORGE_BIN=$<TARGET_FILE:chapterforge_cli>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_big_image_test.cmake
        )
        set_tests_properties(long_audio_smallimg_50 PROPERTIES LABELS "core")
    else()
        message(STATUS "Skipping long_audio_smallimg_50 (missing INPUT_LARGE_M4A_PATH or chapters_small_50.json)")
    endif()

    # 50 chapters with normal (800x800) art over the long input.
    if(EXISTS "${INPUT_LARGE_M4A_PATH}" AND EXISTS "${NORMAL50_JSON}")
        add_test(
            NAME long_audio_normalimg_50
            COMMAND ${CMAKE_COMMAND}
            -DINPUT_M4A=${INPUT_LARGE_M4A_PATH}
            -DCHAPTER_JSON=${NORMAL50_JSON}
            -DOUTPUT_M4A=${OUTPUT_NORMAL50}
            -DCHAPTERFORGE_BIN=$<TARGET_FILE:chapterforge_cli>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_big_image_test.cmake
        )
        set_tests_properties(long_audio_normalimg_50 PROPERTIES LABELS "core")
    else()
        message(STATUS "Skipping long_audio_normalimg_50 (missing INPUT_LARGE_M4A_PATH or chapters_250s_50ch_normalimg_meta.json)")
    endif()

    # Golden2-inspired structure: 8 chapters, URL+image tracks to mirror the layout.
    set(GOLDEN2_LIKE_JSON "${TESTDATA_DIR}/chapters_golden2_like.json")
    set(OUTPUT_GOLDEN2_LIKE "${TEST_OUTPUT_DIR}/output_golden2_like.m4a" CACHE STRING "Golden2-like output" FORCE)
    if(EXISTS "${INPUT_LARGE_M4A_PATH}" AND EXISTS "${GOLDEN2_LIKE_JSON}")
        add_test(
            NAME long_audio_golden2_like
            COMMAND ${CMAKE_COMMAND}
            -DINPUT_M4A=${INPUT_LARGE_M4A_PATH}
            -DCHAPTER_JSON=${GOLDEN2_LIKE_JSON}
            -DOUTPUT_M4A=${OUTPUT_GOLDEN2_LIKE}
            -DCHAPTERFORGE_BIN=$<TARGET_FILE:chapterforge_cli>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_big_image_test.cmake
        )
        set_tests_properties(long_audio_golden2_like PROPERTIES LABELS "core")
    else()
        message(STATUS "Skipping long_audio_golden2_like (missing INPUT_LARGE_M4A_PATH or chapters_golden2_like.json)")
    endif()
endif()

if(ENABLE_VIDEO_DEBUG_COMPARE AND EXISTS "${GOLDEN_M4A_PATH}")
    find_program(FFPROBE_PATH ffprobe)
    find_program(MP4DUMP_PATH mp4dump)
    if(FFPROBE_PATH AND MP4DUMP_PATH)
        add_test(
            NAME video_debug_compare
            COMMAND ${CMAKE_COMMAND}
                -DGOLDEN_M4A=${GOLDEN_M4A_PATH}
                -DOUTPUT_M4A=${OUTPUT_M4A_PATH}
                -DFFPROBE_PATH=${FFPROBE_PATH}
                -DMP4DUMP_PATH=${MP4DUMP_PATH}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/video_debug_compare.cmake
        )
        set_tests_properties(video_debug_compare PROPERTIES FIXTURES_REQUIRED assets LABELS "tooling")
    else()
        message(STATUS "Skipping video_debug_compare (ffprobe or mp4dump not found)")
    endif()
endif()

if(ENABLE_AVFOUNDATION_SMOKE)
    find_program(SWIFT_PATH swift)
    if(SWIFT_PATH)
        add_test(
            NAME avfoundation_smoke
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/avfoundation_smoke.sh
        )
        set_tests_properties(avfoundation_smoke PROPERTIES
            ENVIRONMENT "AVF_OUTPUT=${OUTPUT_M4A_PATH};SWIFT_MODULE_CACHE_PATH=${CMAKE_BINARY_DIR}/swift_module_cache"
        )
    else()
        message(STATUS "Skipping avfoundation_smoke (swift not found)")
    endif()
endif()
