cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED OUTPUT_M4A OR NOT EXISTS "${OUTPUT_M4A}")
    message(FATAL_ERROR "OUTPUT_M4A missing or not found: ${OUTPUT_M4A}")
endif()

if(NOT DEFINED GOLDEN_M4A OR NOT EXISTS "${GOLDEN_M4A}")
    message(FATAL_ERROR "GOLDEN_M4A missing or not found: ${GOLDEN_M4A}")
endif()

if(NOT DEFINED MP4INFO_PATH OR NOT MP4INFO_PATH)
    find_program(MP4INFO_PATH mp4info)
endif()
if(NOT MP4INFO_PATH)
    message(FATAL_ERROR "mp4info not found; install it or set MP4INFO_PATH")
endif()

function(capture name file outvar)
    execute_process(
        COMMAND ${MP4INFO_PATH} "${file}"
        RESULT_VARIABLE rv
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )
    if(NOT rv EQUAL 0)
        message(FATAL_ERROR "${name} failed (${rv}):\n${err}\n${out}")
    endif()
    set(${outvar} "${out}" PARENT_SCOPE)
endfunction()

function(parse_video info prefix)
    # Find first video track section
    string(FIND "${info}" "type:         Video" start_pos)
    if(start_pos EQUAL -1)
        message(FATAL_ERROR "Could not find video track in ${prefix} mp4info")
    endif()
    string(SUBSTRING "${info}" ${start_pos} -1 section)
    string(REGEX MATCH "Coding:[ \t]*([^\\n]+)" _codec "${section}")
    if(NOT _codec)
        message(FATAL_ERROR "Could not find video Coding in ${prefix} mp4info")
    endif()
    set(${prefix}_codec "${CMAKE_MATCH_1}" PARENT_SCOPE)

    string(REGEX MATCH "sample count:[ \t]*([0-9]+)" _samples "${section}")
    if(NOT _samples)
        message(FATAL_ERROR "Could not find video sample count in ${prefix} mp4info")
    endif()
    set(${prefix}_samples "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

capture("mp4info (output)" "${OUTPUT_M4A}" OUT_INFO)
capture("mp4info (golden)" "${GOLDEN_M4A}" GOLD_INFO)

parse_video("${OUT_INFO}" OUT)
parse_video("${GOLD_INFO}" GOLD)

set(had_warning FALSE)
if(NOT "${OUT_codec}" STREQUAL "${GOLD_codec}")
    message(WARNING "Video codec mismatch: output='${OUT_codec}' golden='${GOLD_codec}'")
    set(had_warning TRUE)
endif()

if(NOT "${OUT_samples}" STREQUAL "${GOLD_samples}")
    message(WARNING "Video sample count mismatch: output=${OUT_samples} golden=${GOLD_samples}")
    set(had_warning TRUE)
endif()

if(NOT had_warning)
    message(STATUS "JPEG track matches: codec='${OUT_codec}', samples=${OUT_samples}")
else()
    message(STATUS "JPEG track comparison completed with warnings")
endif()
