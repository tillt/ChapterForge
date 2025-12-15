cmake_minimum_required(VERSION 3.16)

set(INPUT_M4A "" CACHE FILEPATH "Input M4A")
set(CHAPTER_JSON "" CACHE FILEPATH "Chapter JSON")
set(OUTPUT_M4A "" CACHE FILEPATH "Output M4A")
set(CHAPTERFORGE_BIN "" CACHE FILEPATH "chapterforge_cli path")
set(MP4DUMP_PATH "" CACHE FILEPATH "mp4dump path")

if(NOT EXISTS "${INPUT_M4A}")
  message(FATAL_ERROR "INPUT_M4A not found: ${INPUT_M4A}")
endif()
if(NOT EXISTS "${CHAPTER_JSON}")
  message(FATAL_ERROR "CHAPTER_JSON not found: ${CHAPTER_JSON}")
endif()
if(NOT EXISTS "${CHAPTERFORGE_BIN}")
  message(FATAL_ERROR "CHAPTERFORGE_BIN not found: ${CHAPTERFORGE_BIN}")
endif()
if(NOT EXISTS "${MP4DUMP_PATH}")
  message(STATUS "Skipping url_text payload check: mp4dump not available")
  return()
endif()

execute_process(
  COMMAND "${CHAPTERFORGE_BIN}" "${INPUT_M4A}" "${CHAPTER_JSON}" "${OUTPUT_M4A}"
  RESULT_VARIABLE mux_res
)
if(NOT mux_res EQUAL 0)
  message(FATAL_ERROR "chapterforge_cli failed: ${mux_res}")
endif()

find_program(STRINGS_PATH strings)
if(NOT STRINGS_PATH)
  message(FATAL_ERROR "strings utility not found on PATH")
endif()

execute_process(
  COMMAND "${STRINGS_PATH}" "${OUTPUT_M4A}"
  RESULT_VARIABLE strings_res
  OUTPUT_VARIABLE strings_out
)
if(NOT strings_res EQUAL 0)
  message(FATAL_ERROR "strings failed: ${strings_res}")
endif()

string(FIND "${strings_out}" "custom-url-text-one" pos_one)
if(pos_one EQUAL -1)
  message(FATAL_ERROR "URL text 'custom-url-text-one' not found in output (strings scan)")
endif()

string(FIND "${strings_out}" "custom-url-text-two" pos_two)
if(pos_two EQUAL -1)
  message(FATAL_ERROR "URL text 'custom-url-text-two' not found in output (strings scan)")
endif()

string(FIND "${strings_out}" "https://urltext.test/one" pos_href_one)
if(pos_href_one EQUAL -1)
  message(FATAL_ERROR "URL href for chapter 1 not found in output (strings scan)")
endif()

string(FIND "${strings_out}" "https://urltext.test/two" pos_href_two)
if(pos_href_two EQUAL -1)
  message(FATAL_ERROR "URL href for chapter 2 not found in output (strings scan)")
endif()

message(STATUS "URL text payloads found in tx3g samples")
