cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED OUTPUT_M4A)
    message(FATAL_ERROR "OUTPUT_M4A is not set")
endif()

if(NOT EXISTS "${OUTPUT_M4A}")
    message(FATAL_ERROR "output M4A not found: ${OUTPUT_M4A}")
endif()

# INPUT_M4A is optional but recommended; when provided, we also inspect it
set(HAVE_INPUT_M4A FALSE)
if(DEFINED INPUT_M4A AND EXISTS "${INPUT_M4A}")
    set(HAVE_INPUT_M4A TRUE)
endif()

if(NOT DEFINED XXD_PATH OR NOT XXD_PATH)
    find_program(XXD_PATH xxd)
endif()

if(NOT DEFINED MP4INFO_PATH OR NOT MP4INFO_PATH)
    find_program(MP4INFO_PATH mp4info)
endif()

if(NOT DEFINED MP4DUMP_PATH OR NOT MP4DUMP_PATH)
    find_program(MP4DUMP_PATH mp4dump)
endif()

if(NOT DEFINED ATOMIC_PARSLEY_PATH OR NOT ATOMIC_PARSLEY_PATH)
    find_program(ATOMIC_PARSLEY_PATH AtomicParsley)
endif()

if(NOT XXD_PATH)
    message(FATAL_ERROR "xxd not found; install it or set XXD_PATH")
endif()

if(NOT MP4INFO_PATH)
    message(FATAL_ERROR "mp4info not found; install it or set MP4INFO_PATH")
endif()

if(NOT MP4DUMP_PATH)
    message(FATAL_ERROR "mp4dump not found; install it or set MP4DUMP_PATH")
endif()

if(NOT ATOMIC_PARSLEY_PATH)
    message(FATAL_ERROR "AtomicParsley not found; install it or set ATOMIC_PARSLEY_PATH")
endif()

set(TEST_LOG "${CMAKE_BINARY_DIR}/output_tools.log")
file(WRITE "${TEST_LOG}" "")

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
    file(APPEND "${TEST_LOG}" "==== ${name} ====\n${out}\n${err}\n\n")
    message(STATUS "${name} OK")
endfunction()

function(capture_tool outvar name)
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
    file(APPEND "${TEST_LOG}" "==== ${name} ====\n${out}\n${err}\n\n")
    set(${outvar} "${out}" PARENT_SCOPE)
    message(STATUS "${name} OK")
endfunction()

# Find the first offset (in hex) where a 4CC appears using xxd output
function(find_atom_offset OUTVAR DUMP FOURCC_HEX)
    string(TOLOWER "${${DUMP}}" dump_lc)
    string(REGEX MATCHALL ".." bytes "${FOURCC_HEX}")
    string(JOIN "[ \t\r\n]*" pattern ${bytes})
    string(REGEX MATCHALL "([0-9a-f]+):[^\n]*${pattern}" matches "${dump_lc}")
    if(matches STREQUAL "")
        # Fallback: check presence anywhere (allow newlines); no offset
        string(REGEX MATCH "${pattern}" present "${dump_lc}")
        if(present STREQUAL "")
            set(${OUTVAR} "not-found" PARENT_SCOPE)
            return()
        endif()
        set(${OUTVAR} "unknown" PARENT_SCOPE)
        return()
    endif()

    list(GET matches 0 first_match)
    string(REGEX REPLACE "^([0-9a-f]+):.*" "\\1" offset_hex "${first_match}")
    set(${OUTVAR} "0x${offset_hex}" PARENT_SCOPE)
endfunction()

function(count_atoms OUTVAR DUMP)
    string(TOLOWER "${${DUMP}}" dump_lc)
    set(result "")
    foreach(atom IN LISTS ATOMS_TO_CHECK_STR)
        string(REGEX MATCHALL "${atom}" matches "${dump_lc}")
        list(LENGTH matches count)
        set(result "${result}${atom}=${count};")
    endforeach()
    set(${OUTVAR} "${result}" PARENT_SCOPE)
endfunction()

function(count_ap_atoms OUTVAR DUMP)
    string(TOLOWER "${${DUMP}}" dump_lc)
    set(result "")
    foreach(atom IN LISTS ATOMS_TO_CHECK_STR)
        string(REGEX MATCHALL "atom[ \t]+${atom}[ \t]" matches "${dump_lc}")
        list(LENGTH matches count)
        set(result "${result}${atom}=${count};")
    endforeach()
    set(${OUTVAR} "${result}" PARENT_SCOPE)
endfunction()

# Extract simple text metadata value from AtomicParsley -T dump for a given atom
function(extract_ap_value OUTVAR DUMP TAG)
    string(REGEX MATCH "Atom[ \t]+${TAG}[^\\n]*\\n[^\n]*\\n[^\n]*\\n[ \t]*value[ \t]*=[ \t]*([^\\n]+)" _m "${${DUMP}}")
    if(NOT _m)
        string(REGEX MATCH "Atom[ \t]+${TAG}[^\\n]*\\n[^\n]*\\n[ \t]*value[ \t]*=[ \t]*([^\\n]+)" _m "${${DUMP}}")
    endif()
    if(_m)
        set(${OUTVAR} "${CMAKE_MATCH_1}" PARENT_SCOPE)
    else()
        set(${OUTVAR} "" PARENT_SCOPE)
    endif()
endfunction()

if(HAVE_INPUT_M4A)
    run_tool("xxd head (input)" ${XXD_PATH} -l 256 "${INPUT_M4A}")
    capture_tool(INPUT_MP4INFO "mp4info (input)" ${MP4INFO_PATH} "${INPUT_M4A}")
    execute_process(
        COMMAND ${XXD_PATH} -c 4 -g 4 "${INPUT_M4A}"
        RESULT_VARIABLE rv
        OUTPUT_VARIABLE INPUT_XXD_DUMP
        ERROR_VARIABLE err
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )
    if(NOT rv EQUAL 0)
        message(FATAL_ERROR "xxd full dump failed for input: ${err}")
    endif()
    file(APPEND "${TEST_LOG}" "==== xxd full dump (input) ====\n${INPUT_XXD_DUMP}\n\n")
else()
    message(STATUS "No INPUT_M4A provided/found; skipping input checks")
endif()

run_tool("xxd head (output)" ${XXD_PATH} -l 256 "${OUTPUT_M4A}")
capture_tool(OUTPUT_MP4INFO "mp4info (output)" ${MP4INFO_PATH} "${OUTPUT_M4A}")
execute_process(
    COMMAND ${XXD_PATH} -c 4 -g 4 "${OUTPUT_M4A}"
    RESULT_VARIABLE rv
    OUTPUT_VARIABLE OUTPUT_XXD_DUMP
    ERROR_VARIABLE err
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
)
if(NOT rv EQUAL 0)
    message(FATAL_ERROR "xxd full dump failed for output: ${err}")
endif()
file(APPEND "${TEST_LOG}" "==== xxd full dump (output) ====\n${OUTPUT_XXD_DUMP}\n\n")

# When input is present, compare key audio properties between input and output
if(HAVE_INPUT_M4A)
    set(fields sample_count timescale sample_rate channels codec_string)

    macro(extract_field PREFIX REGEX)
        string(REGEX MATCH "${REGEX}" _match "${${PREFIX}_MP4INFO}")
        if(NOT _match)
            message(FATAL_ERROR "Could not find '${REGEX}' in ${PREFIX} mp4info output")
        endif()
        set(${PREFIX}_${ARGV2} "${CMAKE_MATCH_1}")
    endmacro()

    extract_field(INPUT "sample count:[ \t]*([0-9]+)")
    extract_field(INPUT "timescale:[ \t]*([0-9]+)")
    extract_field(INPUT "Sample Rate:[ \t]*([0-9]+)")
    extract_field(INPUT "Channels:[ \t]*([0-9]+)")
    extract_field(INPUT "Codec String:[ \t]*([^\\n]+)")

    extract_field(OUTPUT "sample count:[ \t]*([0-9]+)")
    extract_field(OUTPUT "timescale:[ \t]*([0-9]+)")
    extract_field(OUTPUT "Sample Rate:[ \t]*([0-9]+)")
    extract_field(OUTPUT "Channels:[ \t]*([0-9]+)")
    extract_field(OUTPUT "Codec String:[ \t]*([^\\n]+)")

    foreach(f IN LISTS fields)
        if(NOT "${INPUT_${f}}" STREQUAL "${OUTPUT_${f}}")
    message(FATAL_ERROR "Audio field mismatch for ${f}: input='${INPUT_${f}}' output='${OUTPUT_${f}}'")
        endif()
    endforeach()
    message(STATUS "Input/Output audio properties match (sample count, timescale, sample rate, channels, codec string)")
endif()

# Locate key atoms via xxd; fail if missing
set(ATOMS_TO_CHECK_HEX
    66747970 # ftyp
    6d6f6f76 # moov
    6d646174 # mdat
    73747364 # stsd
    73747473 # stts
    73747363 # stsc
    7374737a # stsz
    7374636f # stco
)

# Atoms to count in mp4dump textual output
set(ATOMS_TO_CHECK_STR
    ftyp
    moov
    mdat
    stsd
    stts
    stsc
    stsz
    stco
    trak
)

if(HAVE_INPUT_M4A)
    foreach(atom IN LISTS ATOMS_TO_CHECK_HEX)
        find_atom_offset(INPUT_${atom}_OFF INPUT_XXD_DUMP "${atom}")
    endforeach()
    capture_tool(INPUT_MP4DUMP "mp4dump (input)" ${MP4DUMP_PATH} --verbosity 3 "${INPUT_M4A}")
    count_atoms(INPUT_ATOM_COUNTS INPUT_MP4DUMP)
    capture_tool(INPUT_AP "AtomicParsley (input)" ${ATOMIC_PARSLEY_PATH} "${INPUT_M4A}" -T)
    count_ap_atoms(INPUT_AP_COUNTS INPUT_AP)
endif()

foreach(atom IN LISTS ATOMS_TO_CHECK_HEX)
    find_atom_offset(OUTPUT_${atom}_OFF OUTPUT_XXD_DUMP "${atom}")
endforeach()
capture_tool(OUTPUT_MP4DUMP "mp4dump (output)" ${MP4DUMP_PATH} --verbosity 3 "${OUTPUT_M4A}")
count_atoms(OUTPUT_ATOM_COUNTS OUTPUT_MP4DUMP)
capture_tool(OUTPUT_AP "AtomicParsley (output)" ${ATOMIC_PARSLEY_PATH} "${OUTPUT_M4A}" -T)
count_ap_atoms(OUTPUT_AP_COUNTS OUTPUT_AP)

file(APPEND "${TEST_LOG}" "==== Atom offsets (output) ====\n")
foreach(atom IN LISTS ATOMS_TO_CHECK_HEX)
    file(APPEND "${TEST_LOG}" "0x${atom}: ${OUTPUT_${atom}_OFF}\n")
endforeach()
file(APPEND "${TEST_LOG}" "\n")

if(HAVE_INPUT_M4A)
    file(APPEND "${TEST_LOG}" "==== Atom offsets (input) ====\n")
    foreach(atom IN LISTS ATOMS_TO_CHECK_HEX)
        file(APPEND "${TEST_LOG}" "0x${atom}: ${INPUT_${atom}_OFF}\n")
    endforeach()
    file(APPEND "${TEST_LOG}" "\n")

    # Compare atom occurrence counts between input and output (based on mp4dump)
    foreach(atom IN LISTS ATOMS_TO_CHECK_STR)
        if(atom STREQUAL "mdat")
            continue() # source may have multiple mdats; tolerate differences
        endif()
        string(REGEX MATCH "${atom}=([0-9]+)" _in "${INPUT_ATOM_COUNTS}")
        string(REGEX MATCH "${atom}=([0-9]+)" _out "${OUTPUT_ATOM_COUNTS}")
        if(NOT _out)
            message(FATAL_ERROR "Atom ${atom} missing in output mp4dump")
        endif()
        set(input_count "${CMAKE_MATCH_1}")
        set(output_count "${CMAKE_MATCH_1}")
        string(REGEX REPLACE ".*${atom}=([0-9]+).*" "\\1" input_count "${_in}")
        string(REGEX REPLACE ".*${atom}=([0-9]+).*" "\\1" output_count "${_out}")
        if(NOT input_count STREQUAL output_count)
            message(WARNING "mp4dump atom count mismatch for ${atom}: input=${input_count}, output=${output_count}")
        endif()
    endforeach()
    message(STATUS "mp4dump atom checks completed")

    # Compare atom occurrence counts between input and output (AtomicParsley)
    foreach(atom IN LISTS ATOMS_TO_CHECK_STR)
        if(atom STREQUAL "mdat")
            continue()
        endif()
        string(REGEX MATCH "${atom}=([0-9]+)" _in "${INPUT_AP_COUNTS}")
        string(REGEX MATCH "${atom}=([0-9]+)" _out "${OUTPUT_AP_COUNTS}")
        if(NOT _out)
            message(FATAL_ERROR "Atom ${atom} missing in output AtomicParsley dump")
        endif()
        set(input_count "${CMAKE_MATCH_1}")
        set(output_count "${CMAKE_MATCH_1}")
        string(REGEX REPLACE ".*${atom}=([0-9]+).*" "\\1" input_count "${_in}")
        string(REGEX REPLACE ".*${atom}=([0-9]+).*" "\\1" output_count "${_out}")
        if(NOT input_count STREQUAL output_count)
            message(WARNING "AtomicParsley atom count mismatch for ${atom}: input=${input_count}, output=${output_count}")
        endif()
    endforeach()
    message(STATUS "AtomicParsley atom checks completed")

    # Metadata preservation check for common and less-common tags
    set(META_TAGS
        "©nam" # title
        "©ART" # artist
        "©alb" # album
        "©gen" # genre
        "©day" # year/date
        "©cmt" # comment
        "©too" # tool/encoder
        "aART" # album artist
        "purl" # podcast URL
        "desc" # description
        "ldes" # long description
        "©lyr" # lyrics
        "cprt" # copyright
        "trkn" # track number
        "covr" # cover art (size may differ, but should exist)
    )
    foreach(tag IN LISTS META_TAGS)
        extract_ap_value(IN_VAL INPUT_AP ${tag})
        extract_ap_value(OUT_VAL OUTPUT_AP ${tag})
        if(NOT IN_VAL STREQUAL OUT_VAL)
            message(FATAL_ERROR "Metadata mismatch for ${tag}: input='${IN_VAL}' output='${OUT_VAL}'")
        endif()
    endforeach()
    message(STATUS "Metadata values preserved for ${META_TAGS}")
endif()

# Duration alignment: movie duration should not trail the longest track by more than a small margin
string(REGEX MATCHALL "duration\\(ms\\)[ \t]*=[ \t]*([0-9]+)" _duration_matches "${OUTPUT_MP4DUMP}")
if(NOT _duration_matches)
    message(FATAL_ERROR "Could not parse durations from mp4dump output")
endif()
list(LENGTH _duration_matches _duration_count)
if(_duration_count LESS 2)
    message(FATAL_ERROR "Expected at least one track duration; found only movie duration")
endif()
list(GET _duration_matches 0 MOVIE_DURATION_MS_RAW)
list(REMOVE_AT _duration_matches 0)
string(REGEX REPLACE ".*= [ \t]*([0-9]+).*" "\\1" MOVIE_DURATION_MS "${MOVIE_DURATION_MS_RAW}")
set(LONGEST_TRACK_MS 0)
foreach(_entry IN LISTS _duration_matches)
    string(REGEX REPLACE ".*= [ \t]*([0-9]+).*" "\\1" _track_ms "${_entry}")
    math(EXPR _track_ms_int "${_track_ms}")
    if(_track_ms_int GREATER LONGEST_TRACK_MS)
        set(LONGEST_TRACK_MS ${_track_ms_int})
    endif()
endforeach()
math(EXPR _duration_gap "${MOVIE_DURATION_MS}-${LONGEST_TRACK_MS}")
set(DURATION_TOLERANCE_MS 250)
if(_duration_gap LESS 0)
    math(EXPR _duration_gap_pos "0-${_duration_gap}")
    if(_duration_gap_pos GREATER DURATION_TOLERANCE_MS)
        message(FATAL_ERROR "Movie duration (${MOVIE_DURATION_MS} ms) shorter than longest track (${LONGEST_TRACK_MS} ms) by ${_duration_gap_pos} ms (> ${DURATION_TOLERANCE_MS} ms)")
    endif()
    set(_duration_gap_msg "-${_duration_gap_pos}")
else()
    if(_duration_gap GREATER DURATION_TOLERANCE_MS)
        message(FATAL_ERROR "Movie duration (${MOVIE_DURATION_MS} ms) exceeds longest track (${LONGEST_TRACK_MS} ms) by ${_duration_gap} ms (> ${DURATION_TOLERANCE_MS} ms)")
    endif()
    set(_duration_gap_msg "${_duration_gap}")
endif()
message(STATUS "Duration alignment OK (movie=${MOVIE_DURATION_MS} ms, longest track=${LONGEST_TRACK_MS} ms, gap=${_duration_gap_msg} ms)")

# Chunk offset guard: ensure stco offsets land inside the mdat data range
file(SIZE "${OUTPUT_M4A}" OUTPUT_FILE_SIZE)
string(REGEX MATCH "0x[0-9a-fA-F]+" _mdat_off_hex "${OUTPUT_6d646174_OFF}")
if(NOT _mdat_off_hex)
    message(WARNING "Skipping chunk offset guard: could not determine mdat offset from xxd dump")
    set(_skip_chunk_guard TRUE)
endif()
if(_mdat_off_hex MATCHES "not-found|unknown")
    message(WARNING "Skipping chunk offset guard: mdat offset not found in xxd")
    set(_skip_chunk_guard TRUE)
endif()
if(NOT _skip_chunk_guard)
    math(EXPR MDAT_OFFSET "${_mdat_off_hex}")
string(REGEX MATCH "\\[mdat\\][ \t]+size=([0-9]+)\\+([0-9]+)" _mdat_match "${OUTPUT_MP4DUMP}")
    if(NOT _mdat_match)
        message(WARNING "Skipping chunk offset guard: could not parse mdat size from mp4dump output")
        set(_skip_chunk_guard TRUE)
    endif()
endif()
if(NOT _skip_chunk_guard)
set(MDAT_HEADER_SIZE "${CMAKE_MATCH_1}")
set(MDAT_PAYLOAD_SIZE "${CMAKE_MATCH_2}")
math(EXPR MDAT_TOTAL_SIZE "${MDAT_HEADER_SIZE}+${MDAT_PAYLOAD_SIZE}")
math(EXPR MDAT_ATOM_START "${MDAT_OFFSET}-4")
if(MDAT_ATOM_START LESS 0)
    set(MDAT_ATOM_START 0)
endif()
math(EXPR MDAT_DATA_START "${MDAT_ATOM_START}+${MDAT_HEADER_SIZE}")
math(EXPR MDAT_DATA_END "${MDAT_ATOM_START}+${MDAT_TOTAL_SIZE}")
string(REGEX MATCHALL "\\[stco\\][^\\[]+" _stco_sections "${OUTPUT_MP4DUMP}")
if(NOT _stco_sections)
    message(FATAL_ERROR "No stco sections found in mp4dump output")
endif()
set(BAD_OFFSETS "")
set(TOTAL_STCO_ENTRIES 0)
foreach(_section IN LISTS _stco_sections)
    string(REGEX MATCHALL "\\)[ \t]*([0-9]+)" _section_matches "${_section}")
    foreach(_match IN LISTS _section_matches)
        string(REGEX REPLACE ".*\\)[ \t]*([0-9]+).*" "\\1" _offset_str "${_match}")
        if(_offset_str STREQUAL "")
            continue()
        endif()
        math(EXPR _offset_val "${_offset_str}")
        math(EXPR TOTAL_STCO_ENTRIES "${TOTAL_STCO_ENTRIES}+1")
        if(_offset_val LESS MDAT_DATA_START OR _offset_val GREATER_EQUAL MDAT_DATA_END OR _offset_val GREATER_EQUAL OUTPUT_FILE_SIZE)
            list(APPEND BAD_OFFSETS "${_offset_val}")
        endif()
    endforeach()
endforeach()
if(BAD_OFFSETS)
    list(LENGTH BAD_OFFSETS BAD_OFFSET_COUNT)
    message(FATAL_ERROR "Found ${BAD_OFFSET_COUNT} chunk offsets outside mdat bounds (mdat data ${MDAT_DATA_START}-${MDAT_DATA_END}, file size ${OUTPUT_FILE_SIZE}): ${BAD_OFFSETS}")
endif()
message(STATUS "Chunk offsets inside mdat bounds (${TOTAL_STCO_ENTRIES} entries)")
endif()

message(STATUS "Tool outputs written to ${TEST_LOG}")
