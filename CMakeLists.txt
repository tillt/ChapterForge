cmake_minimum_required(VERSION 3.16)

# Base version is "dev"; Git tags override when present.
project(ChapterForge VERSION 0.0.0 LANGUAGES CXX)

set(CHAPTERFORGE_VERSION_STRING "dev")
set(CHAPTERFORGE_VERSION_DEV TRUE)
set(CHAPTERFORGE_GIT_HASH "unknown")
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

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#Core sources(no CLI) so we can build a reusable library
set(CHAPTERFORGE_CORE_SOURCES
    src/aac_extractor.cpp
    src/dinf_builder.cpp
    src/hdlr_builder.cpp
    src/jpeg_entry_builder.cpp
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
    src/stbl_builder.cpp
    src/stbl_image_builder.cpp
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
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nlohmann
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)
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
    list(FILTER FORMAT_FILES EXCLUDE REGEX ".*/include/nlohmann/.*")
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i -style=file ${FORMAT_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-format on sources"
    )
endif()

#clang - tidy helper(braces around statements, etc.)
find_program(CLANG_TIDY_EXE clang-tidy)
if(CLANG_TIDY_EXE)
    file(GLOB_RECURSE TIDY_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
    )
    list(FILTER TIDY_FILES EXCLUDE REGEX ".*/include/nlohmann/.*")
    add_custom_target(lint
        COMMAND ${CLANG_TIDY_EXE}
            -checks=-*,readability-braces-around-statements
            -p ${CMAKE_BINARY_DIR}
            ${TIDY_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-tidy (braces-around-statements check)"
    )
endif()

enable_testing()

add_custom_target(generate_test_assets
    COMMAND ${CMAKE_COMMAND} -E env bash ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_test_audio.sh
    COMMAND ${CMAKE_COMMAND} -E env bash ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_test_images.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating synthetic test audio/images into ${TESTDATA_DIR}"
)

# Generate-once variant for tests (skip regeneration if stamp exists).
set(TEST_ASSET_STAMP ${CMAKE_BINARY_DIR}/generated_test_assets.stamp)
add_custom_command(
    OUTPUT ${TEST_ASSET_STAMP}
    COMMAND ${CMAKE_COMMAND} -E env bash ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_test_audio.sh
    COMMAND ${CMAKE_COMMAND} -E env bash ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_test_images.sh
    COMMAND ${CMAKE_COMMAND} -E touch ${TEST_ASSET_STAMP}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating synthetic test assets (stamp guarded)"
)
add_custom_target(generate_test_assets_once DEPENDS ${TEST_ASSET_STAMP})

option(ENABLE_OUTPUT_TOOL_TESTS "Run tooling checks on an existing output.m4a" OFF)
set(TESTDATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/testdata" CACHE PATH "Directory containing sample input/output media")
set(OUTPUT_M4A_PATH "${TESTDATA_DIR}/output.m4a" CACHE STRING "Path to the output M4A to inspect with tools")
set(INPUT_M4A_PATH "${TESTDATA_DIR}/input.m4a" CACHE STRING "Path to the source input M4A to inspect with tools")
set(INPUT_AAC_PATH "${TESTDATA_DIR}/input.aac" CACHE STRING "Path to the source AAC (ADTS) file for AAC ingest test")
set(GOLDEN_M4A_PATH "${TESTDATA_DIR}/golden_chapters.m4a" CACHE STRING "Path to the golden M4A reference for chapter images")
set(CHAPTER_JSON_PATH "${TESTDATA_DIR}/chapters.json" CACHE STRING "Path to the primary chapters JSON")
option(ENABLE_STRICT_VALIDATION "Run stricter validation with multiple parsers (mp4info/mp4dump/ffprobe/MP4Box)" OFF)
option(ENABLE_BIG_IMAGE_TESTS "Run heavy chapter-image tests (requires large input_big.m4a and large JPEGs)" OFF)
set(INPUT_BIG_M4A_PATH "${TESTDATA_DIR}/input_big.m4a" CACHE STRING "Path to the large input M4A for big image tests")
set(INPUT_BIG_AAC_PATH "${TESTDATA_DIR}/input_big.aac" CACHE STRING "Path to the large input AAC (ADTS) for big image AAC ingest tests")
option(ENABLE_AVFOUNDATION_SMOKE "Run AVFoundation smoke test (macOS only, requires swift)" OFF)

if(ENABLE_OUTPUT_TOOL_TESTS)
    find_program(XXD_PATH xxd)
    find_program(MP4INFO_PATH mp4info)
    find_program(MP4DUMP_PATH mp4dump)
    find_program(ATOMIC_PARSLEY_PATH AtomicParsley)

    add_executable(audio_reuse_check
        tests/audio_reuse_check.cpp
    )
    target_link_libraries(audio_reuse_check PRIVATE chapterforge)
    target_include_directories(audio_reuse_check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    add_test(
        NAME audio_reuse_check
        COMMAND audio_reuse_check "${INPUT_M4A_PATH}" "${OUTPUT_M4A_PATH}"
    )
    set_property(TEST audio_reuse_check PROPERTY DEPENDS generate_test_assets_once)

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
    set_property(TEST output_tool_checks PROPERTY DEPENDS generate_test_assets_once)
#Metadata preservation on small input(JSON has no metadata)
    add_test(
        NAME metadata_preserve_small
        COMMAND ${CMAKE_COMMAND}
            -DINPUT_M4A=${INPUT_M4A_PATH}
            -DOUTPUT_M4A=${CMAKE_BINARY_DIR}/output_meta_preserve.m4a
            -DCHAPTER_JSON=${TESTDATA_DIR}/chapters_nometa.json
            -DCHAPTERFORGE_BIN=$<TARGET_FILE:chapterforge_cli>
            -DFASTSTART_FLAG=ON
            -DXXD_PATH=${XXD_PATH}
            -DMP4INFO_PATH=${MP4INFO_PATH}
            -DMP4DUMP_PATH=${MP4DUMP_PATH}
            -DATOMIC_PARSLEY_PATH=${ATOMIC_PARSLEY_PATH}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tools_with_mux.cmake
    )
    set_property(TEST metadata_preserve_small PROPERTY DEPENDS generate_test_assets_once)
#AAC input ingest->MP4, validated against the original input M4A for metadata / audio consistency.
    set(OUTPUT_AAC_M4A_PATH "${CMAKE_BINARY_DIR}/output_from_aac.m4a")
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
        set_property(TEST output_tool_checks_aac PROPERTY DEPENDS generate_test_assets_once)
    else()
        message(STATUS "Skipping AAC ingest tests (INPUT_AAC_PATH missing: ${INPUT_AAC_PATH})")
    endif()

    add_test(
        NAME jpeg_track_compare
        COMMAND ${CMAKE_COMMAND}
            -DOUTPUT_M4A=${OUTPUT_M4A_PATH}
            -DGOLDEN_M4A=${GOLDEN_M4A_PATH}
            -DMP4INFO_PATH=${MP4INFO_PATH}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_jpeg_track.cmake
    )
    set_property(TEST jpeg_track_compare PROPERTY DEPENDS generate_test_assets_once)

    # Convenience target: generate assets, then run all tests.
    add_custom_target(check_with_assets
        DEPENDS generate_test_assets chapterforge chapterforge_cli
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generate test assets then run ctest"
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
endif()

if(ENABLE_BIG_IMAGE_TESTS)
#Paths to chapter JSONs with large images; guarded by file existence
    set(BIG5_JSON "${TESTDATA_DIR}/chapters_big5.json")
    set(BIG35_JSON "${TESTDATA_DIR}/chapters_big35.json")
    set(BIG_NOMETA_JSON "${TESTDATA_DIR}/chapters_big_nometa.json")
    set(OUTPUT_BIG5 "${TESTDATA_DIR}/output_big5.m4a")
    set(OUTPUT_BIG35 "${TESTDATA_DIR}/output_big35.m4a")
    set(OUTPUT_BIG_AAC "${CMAKE_BINARY_DIR}/output_big_from_aac.m4a")
    set(OUTPUT_BIG_AAC "${CMAKE_BINARY_DIR}/output_big_from_aac.m4a")

    if(EXISTS "${INPUT_M4A_PATH}" AND EXISTS "${BIG5_JSON}")
        add_test(
            NAME big_image_5
            COMMAND ${CMAKE_COMMAND}
            -DINPUT_M4A=${INPUT_M4A_PATH}
            -DCHAPTER_JSON=${BIG5_JSON}
            -DOUTPUT_M4A=${OUTPUT_BIG5}
            -DCHAPTERFORGE_BIN=$<TARGET_FILE:chapterforge_cli>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_big_image_test.cmake
        )
    else()
        message(STATUS "Skipping big_image_5 (missing INPUT_M4A_PATH or chapters_big5.json)")
    endif()

#Metadata preservation on large input : leave JSON metadata empty so ilst is
#copied through verbatim from the source M4A.
    if(EXISTS "${INPUT_BIG_M4A_PATH}" AND EXISTS "${BIG_NOMETA_JSON}")
        set(OUTPUT_BIG_NOMETA "${CMAKE_BINARY_DIR}/output_big_nometa.m4a")
        add_test(
            NAME big_image_metadata_preserve
            COMMAND ${CMAKE_COMMAND}
            -DINPUT_M4A=${INPUT_BIG_M4A_PATH}
            -DCHAPTER_JSON=${BIG_NOMETA_JSON}
            -DOUTPUT_M4A=${OUTPUT_BIG_NOMETA}
            -DCHAPTERFORGE_BIN=$<TARGET_FILE:chapterforge_cli>
            -DXXD_PATH=${XXD_PATH}
            -DMP4INFO_PATH=${MP4INFO_PATH}
            -DMP4DUMP_PATH=${MP4DUMP_PATH}
            -DATOMIC_PARSLEY_PATH=${ATOMIC_PARSLEY_PATH}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tools_with_mux.cmake
        )
    else()
        message(STATUS "Skipping big_image_metadata_preserve (missing INPUT_BIG_M4A_PATH or chapters_big_nometa.json)")
    endif()

#AAC ingest variant using the large AAC source and the smaller big5 chapter
#set to keep run time reasonable.
    if(EXISTS "${INPUT_BIG_AAC_PATH}" AND EXISTS "${INPUT_BIG_M4A_PATH}" AND EXISTS "${BIG5_JSON}")
        add_test(
            NAME big_image_aac
            COMMAND ${CMAKE_COMMAND}
            -DINPUT_AAC=${INPUT_BIG_AAC_PATH}
            -DINPUT_M4A=${INPUT_BIG_M4A_PATH}
            -DOUTPUT_M4A=${OUTPUT_BIG_AAC}
            -DCHAPTER_JSON=${BIG5_JSON}
            -DCHAPTERFORGE_BIN=$<TARGET_FILE:chapterforge_cli>
            -DFASTSTART_FLAG=ON
            -DXXD_PATH=${XXD_PATH}
            -DMP4INFO_PATH=${MP4INFO_PATH}
            -DMP4DUMP_PATH=${MP4DUMP_PATH}
            -DATOMIC_PARSLEY_PATH=${ATOMIC_PARSLEY_PATH}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_aac_with_mux.cmake
        )
    else()
        message(STATUS "Skipping big_image_aac (missing INPUT_BIG_AAC_PATH, INPUT_BIG_M4A_PATH, or chapters_big5.json)")
    endif()

    if(EXISTS "${INPUT_BIG_M4A_PATH}" AND EXISTS "${BIG35_JSON}")
        add_test(
            NAME big_image_35
            COMMAND ${CMAKE_COMMAND}
            -DINPUT_M4A=${INPUT_BIG_M4A_PATH}
            -DCHAPTER_JSON=${BIG35_JSON}
            -DOUTPUT_M4A=${OUTPUT_BIG35}
            -DCHAPTERFORGE_BIN=$<TARGET_FILE:chapterforge_cli>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_big_image_test.cmake
        )
    else()
        message(STATUS "Skipping big_image_35 (missing INPUT_BIG_M4A_PATH or chapters_big35.json)")
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
