cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED INPUT_M4A OR NOT DEFINED CHAPTER_JSON OR NOT DEFINED OUTPUT_M4A)
    message(FATAL_ERROR "INPUT_M4A, CHAPTER_JSON, OUTPUT_M4A must be set")
endif()
if(NOT EXISTS "${INPUT_M4A}")
    message(FATAL_ERROR "INPUT_M4A not found: ${INPUT_M4A}")
endif()
if(NOT EXISTS "${CHAPTER_JSON}")
    message(FATAL_ERROR "CHAPTER_JSON not found: ${CHAPTER_JSON}")
endif()
get_filename_component(CHAPTER_DIR "${CHAPTER_JSON}" DIRECTORY)

if(NOT DEFINED CHAPTERFORGE_BIN)
    set(CHAPTERFORGE_BIN "${CMAKE_BINARY_DIR}/chapterforge_cli")
endif()
if(NOT EXISTS "${CHAPTERFORGE_BIN}")
    message(FATAL_ERROR "ChapterForge binary not found at: ${CHAPTERFORGE_BIN}")
endif()

find_program(MP4INFO_PATH mp4info)
if(NOT MP4INFO_PATH)
    message(FATAL_ERROR "mp4info not found; install it or set MP4INFO_PATH")
endif()

# Run the muxer
execute_process(
    COMMAND "${CHAPTERFORGE_BIN}" "${INPUT_M4A}" "${CHAPTER_JSON}" "${OUTPUT_M4A}"
    RESULT_VARIABLE rv
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
)
if(NOT rv EQUAL 0)
    message(FATAL_ERROR "ChapterForge failed (${rv}):\n${err}\n${out}")
endif()
message(STATUS "ChapterForge OK for ${CHAPTER_JSON}")

# Inspect resulting file with mp4info to ensure it is well-formed
execute_process(
    COMMAND "${MP4INFO_PATH}" "${OUTPUT_M4A}"
    RESULT_VARIABLE rv2
    OUTPUT_VARIABLE out2
    ERROR_VARIABLE err2
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
)
if(NOT rv2 EQUAL 0)
    message(FATAL_ERROR "mp4info failed (${rv2}) on ${OUTPUT_M4A}:\n${err2}\n${out2}")
endif()

message(STATUS "Big image test completed for ${OUTPUT_M4A}")
