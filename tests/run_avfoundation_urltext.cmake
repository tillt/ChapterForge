cmake_minimum_required(VERSION 3.16)

set(INPUT_M4A "" CACHE FILEPATH "Input M4A")
set(CHAPTER_JSON "" CACHE FILEPATH "Chapter JSON")
set(OUTPUT_M4A "" CACHE FILEPATH "Output M4A")
set(CHAPTERFORGE_BIN "" CACHE FILEPATH "chapterforge_cli path")
set(PROJECT_ROOT "" CACHE FILEPATH "Project root")

if(NOT EXISTS "${INPUT_M4A}")
  message(FATAL_ERROR "INPUT_M4A not found: ${INPUT_M4A}")
endif()
if(NOT EXISTS "${CHAPTER_JSON}")
  message(FATAL_ERROR "CHAPTER_JSON not found: ${CHAPTER_JSON}")
endif()
if(NOT EXISTS "${CHAPTERFORGE_BIN}")
  message(FATAL_ERROR "CHAPTERFORGE_BIN not found: ${CHAPTERFORGE_BIN}")
endif()

execute_process(
  COMMAND "${CHAPTERFORGE_BIN}" "${INPUT_M4A}" "${CHAPTER_JSON}" "${OUTPUT_M4A}"
  RESULT_VARIABLE mux_res
)
if(NOT mux_res EQUAL 0)
  message(FATAL_ERROR "chapterforge_cli failed: ${mux_res}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
          AVF_INPUT=${INPUT_M4A}
          AVF_JSON=${CHAPTER_JSON}
          AVF_OUT=${OUTPUT_M4A}
          AVF_BIN=${CHAPTERFORGE_BIN}
          /bin/bash "${PROJECT_ROOT}/tests/avfoundation_urltext.sh"
  WORKING_DIRECTORY "${PROJECT_ROOT}"
  RESULT_VARIABLE avf_res
)
if(NOT avf_res EQUAL 0)
  message(FATAL_ERROR "AVFoundation url_text test failed: ${avf_res}")
endif()

message(STATUS "AVFoundation URL text test passed")
