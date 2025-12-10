cmake_minimum_required(VERSION 3.16)

foreach(var IN ITEMS INPUT_M4A CHAPTER_JSON OUTPUT_M4A CHAPTERFORGE_BIN)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "${var} is not set")
    endif()
endforeach()

if(NOT EXISTS "${INPUT_M4A}")
    message(FATAL_ERROR "INPUT_M4A not found: ${INPUT_M4A}")
endif()
if(NOT EXISTS "${CHAPTER_JSON}")
    message(FATAL_ERROR "CHAPTER_JSON not found: ${CHAPTER_JSON}")
endif()

set(FASTSTART FALSE)
if(DEFINED FASTSTART_FLAG)
    if(FASTSTART_FLAG)
        set(FASTSTART TRUE)
    endif()
endif()

# Optional tool paths forwarded to the inner check script
if(NOT DEFINED XXD_PATH)
    find_program(XXD_PATH xxd)
endif()
if(NOT DEFINED MP4INFO_PATH)
    find_program(MP4INFO_PATH mp4info)
endif()
if(NOT DEFINED MP4DUMP_PATH)
    find_program(MP4DUMP_PATH mp4dump)
endif()
if(NOT DEFINED ATOMIC_PARSLEY_PATH)
    find_program(ATOMIC_PARSLEY_PATH AtomicParsley)
endif()

set(MUX_ARGS "${INPUT_M4A}" "${CHAPTER_JSON}" "${OUTPUT_M4A}")
if(FASTSTART)
    list(APPEND MUX_ARGS "--faststart")
endif()

execute_process(
    COMMAND "${CHAPTERFORGE_BIN}" ${MUX_ARGS}
    RESULT_VARIABLE rv
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
)
if(NOT rv EQUAL 0)
    message(FATAL_ERROR "ChapterForge failed (${rv}):\n${err}\n${out}")
endif()
message(STATUS "ChapterForge OK (${OUTPUT_M4A})")

# Now run the existing tool checks with the freshly built output
set(OUTPUT_M4A "${OUTPUT_M4A}")
set(INPUT_M4A "${INPUT_M4A}")

include("${CMAKE_CURRENT_LIST_DIR}/check_output_m4a.cmake")
