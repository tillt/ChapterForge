cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED PROJECT_ROOT)
    message(FATAL_ERROR "PROJECT_ROOT not set")
endif()

set(TESTDATA_DIR "${PROJECT_ROOT}/testdata")

set(missing FALSE)
# Audio
file(GLOB AUDIO_CHECK "${TESTDATA_DIR}/input*.m4a" "${TESTDATA_DIR}/input*.aac")
if(AUDIO_CHECK STREQUAL "")
    set(missing TRUE)
endif()
# Images (ensure at least one jpg)
file(GLOB IMAGE_CHECK "${TESTDATA_DIR}/images/*.jpg")
if(IMAGE_CHECK STREQUAL "")
    set(missing TRUE)
endif()
# JSON
file(GLOB JSON_CHECK "${TESTDATA_DIR}/chapters*.json")
if(JSON_CHECK STREQUAL "")
    set(missing TRUE)
endif()

if(missing)
    message(STATUS "[generate_assets] Missing fixtures; running generators...")
    execute_process(
        COMMAND bash -c "cd \"${PROJECT_ROOT}\" && bash scripts/generate_test_audio.sh && bash scripts/generate_test_images.sh && bash scripts/generate_test_json.sh"
        RESULT_VARIABLE rv
    )
    if(NOT rv EQUAL 0)
        message(FATAL_ERROR "generate_assets failed with ${rv}")
    endif()
else()
    message(STATUS "[generate_assets] Fixtures present; skipping generation.")
endif()
