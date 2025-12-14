cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED GOLDEN_M4A OR NOT DEFINED OUTPUT_M4A)
    message(FATAL_ERROR "GOLDEN_M4A and OUTPUT_M4A must be set")
endif()

if(NOT EXISTS "${GOLDEN_M4A}")
    message(FATAL_ERROR "Golden file not found: ${GOLDEN_M4A}")
endif()
if(NOT EXISTS "${OUTPUT_M4A}")
    message(FATAL_ERROR "Output file not found: ${OUTPUT_M4A}")
endif()

find_program(FFPROBE_PATH ffprobe)
find_program(MP4DUMP_PATH mp4dump)
if(NOT FFPROBE_PATH)
    message(FATAL_ERROR "ffprobe not found; install it or set FFPROBE_PATH")
endif()
if(NOT MP4DUMP_PATH)
    message(FATAL_ERROR "mp4dump not found; install it or set MP4DUMP_PATH")
endif()

set(DEBUG_DIR "${CMAKE_BINARY_DIR}/video_debug")
file(MAKE_DIRECTORY "${DEBUG_DIR}")

set(GOLDEN_BASE "${DEBUG_DIR}/golden")
set(OUTPUT_BASE "${DEBUG_DIR}/output")

message(STATUS "[video_debug] dumping ffprobe + mp4dump for video tracks")

execute_process(
    COMMAND "${FFPROBE_PATH}" -v error -select_streams v:0 -show_streams -show_format "${GOLDEN_M4A}"
    OUTPUT_FILE "${GOLDEN_BASE}_ffprobe.txt"
)
execute_process(
    COMMAND "${FFPROBE_PATH}" -v error -select_streams v:0 -show_streams -show_format "${OUTPUT_M4A}"
    OUTPUT_FILE "${OUTPUT_BASE}_ffprobe.txt"
)

execute_process(
    COMMAND "${MP4DUMP_PATH}" --verbosity 3 "${GOLDEN_M4A}"
    OUTPUT_FILE "${GOLDEN_BASE}_mp4dump.txt"
)
execute_process(
    COMMAND "${MP4DUMP_PATH}" --verbosity 3 "${OUTPUT_M4A}"
    OUTPUT_FILE "${OUTPUT_BASE}_mp4dump.txt"
)

message(STATUS "[video_debug] dumps written to ${DEBUG_DIR}")
