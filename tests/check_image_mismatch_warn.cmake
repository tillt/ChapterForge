cmake_minimum_required(VERSION 3.16)

set(CHAPTERFORGE_BIN "" CACHE FILEPATH "chapterforge_cli path")
set(INPUT_M4A "" CACHE FILEPATH "Input M4A")
set(CHAPTER_JSON "" CACHE FILEPATH "Chapter JSON")
set(OUTPUT_M4A "" CACHE FILEPATH "Output M4A")

if(NOT EXISTS "${CHAPTERFORGE_BIN}")
  message(FATAL_ERROR "CHAPTERFORGE_BIN not found: ${CHAPTERFORGE_BIN}")
endif()
if(NOT EXISTS "${INPUT_M4A}")
  message(FATAL_ERROR "INPUT_M4A not found: ${INPUT_M4A}")
endif()
if(NOT EXISTS "${CHAPTER_JSON}")
  message(FATAL_ERROR "CHAPTER_JSON not found: ${CHAPTER_JSON}")
endif()

execute_process(
  COMMAND "${CHAPTERFORGE_BIN}" "${INPUT_M4A}" "${CHAPTER_JSON}" "${OUTPUT_M4A}" --log-level warn
  RESULT_VARIABLE mux_res
  OUTPUT_VARIABLE mux_out
  ERROR_VARIABLE mux_err
)

if(NOT mux_res EQUAL 0)
  message(FATAL_ERROR "chapterforge_cli failed: ${mux_res}")
endif()

string(CONCAT all_log "${mux_out}" "\n" "${mux_err}")
if(NOT all_log MATCHES "differing from first image")
  message(FATAL_ERROR "Expected dimension mismatch warning not found in log")
endif()

message(STATUS "Image mismatch warning was emitted as expected")
