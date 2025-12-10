cmake_minimum_required(VERSION 3.16)

project(ChapterForge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Core sources (no CLI) so we can build a reusable library
set(MP4CHAPTERS_CORE_SOURCES
    aac_extractor.cpp
    dinf_builder.cpp
    hdlr_builder.cpp
    jpeg_entry_builder.cpp
    mdat_writer.cpp
    mdhd_builder.cpp
    mdia_builder.cpp
    meta_builder.cpp
    minf_builder.cpp
    moov_builder.cpp
    mp4_atoms.cpp
    mp4_muxer.cpp
    mp4a_builder.cpp
    mvhd_builder.cpp
    nmhd_builder.cpp
    parser.cpp
    smhd_builder.cpp
    stbl_audio_builder.cpp
    stbl_builder.cpp
    stbl_image_builder.cpp
    stbl_text_builder.cpp
    stco_builder.cpp
    stsc_builder.cpp
    stsd_builder.cpp
    stsz_builder.cpp
    stts_builder.cpp
    tkhd_builder.cpp
    trak_builder.cpp
    tx3g_stsd_builder.cpp
    udta_builder.cpp
    vmhd_builder.cpp
)

add_library(chapterforge STATIC ${MP4CHAPTERS_CORE_SOURCES} chapterforge.cpp)
target_include_directories(chapterforge PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/nlohmann
)

# CLI target links against the library
add_executable(chapterforge_cli main.cpp)
target_link_libraries(chapterforge_cli PRIVATE chapterforge)

enable_testing()

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
        parser.cpp
        mp4_atoms.cpp
    )
    target_include_directories(audio_reuse_check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(
        NAME audio_reuse_check
        COMMAND audio_reuse_check "${INPUT_M4A_PATH}" "${OUTPUT_M4A_PATH}"
    )
    set_tests_properties(audio_reuse_check PROPERTIES DEPENDS output_tool_checks)

    add_test(
        NAME output_tool_checks
        COMMAND ${CMAKE_COMMAND}
            -DINPUT_M4A=${INPUT_M4A_PATH}
            -DOUTPUT_M4A=${OUTPUT_M4A_PATH}
            -DCHAPTER_JSON=${CHAPTER_JSON_PATH}
            -DMP4CHAPTERS_BIN=$<TARGET_FILE:chapterforge_cli>
            -DFASTSTART_FLAG=ON
            -DXXD_PATH=${XXD_PATH}
            -DMP4INFO_PATH=${MP4INFO_PATH}
            -DMP4DUMP_PATH=${MP4DUMP_PATH}
            -DATOMIC_PARSLEY_PATH=${ATOMIC_PARSLEY_PATH}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tools_with_mux.cmake
    )
    # Metadata preservation on small input (JSON has no metadata)
    add_test(
        NAME metadata_preserve_small
        COMMAND ${CMAKE_COMMAND}
            -DINPUT_M4A=${INPUT_M4A_PATH}
            -DOUTPUT_M4A=${CMAKE_BINARY_DIR}/output_meta_preserve.m4a
            -DCHAPTER_JSON=${TESTDATA_DIR}/chapters_nometa.json
            -DMP4CHAPTERS_BIN=$<TARGET_FILE:chapterforge_cli>
            -DFASTSTART_FLAG=ON
            -DXXD_PATH=${XXD_PATH}
            -DMP4INFO_PATH=${MP4INFO_PATH}
            -DMP4DUMP_PATH=${MP4DUMP_PATH}
            -DATOMIC_PARSLEY_PATH=${ATOMIC_PARSLEY_PATH}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tools_with_mux.cmake
    )
    # AAC input ingest -> MP4, validated against the original input M4A for metadata/audio consistency.
    set(OUTPUT_AAC_M4A_PATH "${CMAKE_BINARY_DIR}/output_from_aac.m4a")
    if(EXISTS "${INPUT_AAC_PATH}")
        add_test(
            NAME output_tool_checks_aac
        COMMAND ${CMAKE_COMMAND}
            -DINPUT_AAC=${INPUT_AAC_PATH}
            -DINPUT_M4A=${INPUT_M4A_PATH}
            -DOUTPUT_M4A=${OUTPUT_AAC_M4A_PATH}
            -DCHAPTER_JSON=${CHAPTER_JSON_PATH}
            -DMP4CHAPTERS_BIN=$<TARGET_FILE:chapterforge_cli>
            -DFASTSTART_FLAG=ON
            -DXXD_PATH=${XXD_PATH}
            -DMP4INFO_PATH=${MP4INFO_PATH}
            -DMP4DUMP_PATH=${MP4DUMP_PATH}
            -DATOMIC_PARSLEY_PATH=${ATOMIC_PARSLEY_PATH}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_aac_with_mux.cmake
        )
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
    # Paths to chapter JSONs with large images; guarded by file existence
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
                -DMP4CHAPTERS_BIN=$<TARGET_FILE:chapterforge_cli>
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_big_image_test.cmake
        )
    else()
        message(STATUS "Skipping big_image_5 (missing INPUT_M4A_PATH or chapters_big5.json)")
    endif()

    # Metadata preservation on large input: leave JSON metadata empty so ilst is
    # copied through verbatim from the source M4A.
    if(EXISTS "${INPUT_BIG_M4A_PATH}" AND EXISTS "${BIG_NOMETA_JSON}")
        set(OUTPUT_BIG_NOMETA "${CMAKE_BINARY_DIR}/output_big_nometa.m4a")
        add_test(
            NAME big_image_metadata_preserve
            COMMAND ${CMAKE_COMMAND}
                -DINPUT_M4A=${INPUT_BIG_M4A_PATH}
                -DCHAPTER_JSON=${BIG_NOMETA_JSON}
                -DOUTPUT_M4A=${OUTPUT_BIG_NOMETA}
                -DMP4CHAPTERS_BIN=$<TARGET_FILE:chapterforge_cli>
                -DXXD_PATH=${XXD_PATH}
                -DMP4INFO_PATH=${MP4INFO_PATH}
                -DMP4DUMP_PATH=${MP4DUMP_PATH}
                -DATOMIC_PARSLEY_PATH=${ATOMIC_PARSLEY_PATH}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tools_with_mux.cmake
        )
    else()
        message(STATUS "Skipping big_image_metadata_preserve (missing INPUT_BIG_M4A_PATH or chapters_big_nometa.json)")
    endif()

    # AAC ingest variant using the large AAC source and the smaller big5 chapter
    # set to keep run time reasonable.
    if(EXISTS "${INPUT_BIG_AAC_PATH}" AND EXISTS "${INPUT_BIG_M4A_PATH}" AND EXISTS "${BIG5_JSON}")
        add_test(
            NAME big_image_aac
            COMMAND ${CMAKE_COMMAND}
                -DINPUT_AAC=${INPUT_BIG_AAC_PATH}
                -DINPUT_M4A=${INPUT_BIG_M4A_PATH}
                -DOUTPUT_M4A=${OUTPUT_BIG_AAC}
                -DCHAPTER_JSON=${BIG5_JSON}
                -DMP4CHAPTERS_BIN=$<TARGET_FILE:chapterforge_cli>
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
                -DMP4CHAPTERS_BIN=$<TARGET_FILE:chapterforge_cli>
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
