# Validate that a muxed file with a non-zero first chapter start encodes that offset
# in the text track's stts using Bento4 mp4dump (JSON output).
#
# Inputs:
#  - INPUT_M4A: source audio
#  - OUTPUT_M4A: output path
#  - CHAPTER_JSON: chapter profile (expects non-zero first start)
#  - CHAPTERFORGE_BIN: chapterforge_cli
#  - MP4DUMP_PATH: path to Bento4 mp4dump

if(NOT EXISTS "${INPUT_M4A}")
  message(FATAL_ERROR "INPUT_M4A missing: ${INPUT_M4A}")
endif()
if(NOT EXISTS "${CHAPTER_JSON}")
  message(FATAL_ERROR "CHAPTER_JSON missing: ${CHAPTER_JSON}")
endif()
if(NOT DEFINED CHAPTERFORGE_BIN)
  message(FATAL_ERROR "CHAPTERFORGE_BIN not set")
endif()
if(NOT MP4DUMP_PATH OR NOT EXISTS "${MP4DUMP_PATH}")
  message(STATUS "Skipping offset test: mp4dump (Bento4) not available")
  return()
endif()

execute_process(
  COMMAND "${CHAPTERFORGE_BIN}" "${INPUT_M4A}" "${CHAPTER_JSON}" "${OUTPUT_M4A}" --log-level warn
  RESULT_VARIABLE mux_rc
)
if(mux_rc)
  message(FATAL_ERROR "chapterforge_cli failed with code ${mux_rc}")
endif()

if(NOT EXISTS "${OUTPUT_M4A}")
  message(FATAL_ERROR "OUTPUT_M4A missing after mux: ${OUTPUT_M4A}")
endif()

# Grab mp4dump verbose text and assert the text-track stts contains the derived duration
# (first chapter delta = 3000ms, derived from start 1500 -> next 4500).
execute_process(
  COMMAND "${MP4DUMP_PATH}" --verbosity 3 "${OUTPUT_M4A}"
  RESULT_VARIABLE dump_rc
  OUTPUT_VARIABLE dump_text
)
if(dump_rc)
  message(FATAL_ERROR "mp4dump failed with code ${dump_rc}")
endif()

string(FIND "${dump_text}" "sample_duration = 3000" has_duration)
if(has_duration EQUAL -1)
  message(FATAL_ERROR "Expected text stts to include sample_duration=3000; mp4dump output did not show it.")
endif()

message(STATUS "Offset chapter verified via mp4dump: found sample_duration=3000")
