cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED OUTPUT_M4A)
    message(FATAL_ERROR "OUTPUT_M4A is not set")
endif()
if(NOT EXISTS "${OUTPUT_M4A}")
    message(FATAL_ERROR "output M4A not found: ${OUTPUT_M4A}")
endif()

set(LOG "${CMAKE_BINARY_DIR}/strict_validation.log")
file(WRITE "${LOG}" "")

function(run_tool name)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE rv
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )
    if(NOT rv EQUAL 0)
        message(FATAL_ERROR "${name} failed (${rv}):\n${err}\n${out}")
    endif()
    file(APPEND "${LOG}" "==== ${name} ====\n${out}\n${err}\n\n")
    message(STATUS "${name} OK")
endfunction()

# Required tools
if(NOT XXD_PATH)
    find_program(XXD_PATH xxd)
endif()
if(NOT MP4INFO_PATH)
    find_program(MP4INFO_PATH mp4info)
endif()
if(NOT MP4DUMP_PATH)
    find_program(MP4DUMP_PATH mp4dump)
endif()
if(NOT ATOMIC_PARSLEY_PATH)
    find_program(ATOMIC_PARSLEY_PATH AtomicParsley)
endif()

foreach(req IN ITEMS XXD_PATH MP4INFO_PATH MP4DUMP_PATH ATOMIC_PARSLEY_PATH)
    if(NOT ${req})
        message(FATAL_ERROR "${req} not found; set ${req} or install the tool")
    endif()
endforeach()

# Optional tools
if(NOT FFPROBE_PATH)
    find_program(FFPROBE_PATH ffprobe)
endif()
if(NOT MP4BOX_PATH)
    find_program(MP4BOX_PATH MP4Box)
endif()

# 1) Basic structure dumps
run_tool("xxd head" ${XXD_PATH} -l 256 "${OUTPUT_M4A}")
run_tool("mp4info" ${MP4INFO_PATH} "${OUTPUT_M4A}")
run_tool("mp4dump" ${MP4DUMP_PATH} "${OUTPUT_M4A}")
run_tool("AtomicParsley -T" ${ATOMIC_PARSLEY_PATH} "${OUTPUT_M4A}" -T)

# 2) Optional deeper checks
if(FFPROBE_PATH)
    run_tool("ffprobe format/streams" ${FFPROBE_PATH} -v error -show_format -show_streams "${OUTPUT_M4A}")
else()
    message(STATUS "ffprobe not found; skipping ffprobe validation")
endif()

if(MP4BOX_PATH)
    run_tool("MP4Box -info" ${MP4BOX_PATH} -info "${OUTPUT_M4A}")
else()
    message(STATUS "MP4Box not found; skipping MP4Box validation")
endif()

message(STATUS "Strict validation log: ${LOG}")
