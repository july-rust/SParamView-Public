# Japanese glyphs cannot rely on optional OS language packs, especially ARM64 CI.
# Pin the upstream Google Fonts revision and verify the exact Git blob identity.
find_package(Git REQUIRED)
set(SPARAMVIEW_JP_FONT "${CMAKE_BINARY_DIR}/fonts/NotoSansJP-Variable.ttf")
set(_jp_blob "cdd8f083c1f5928ff3361f8cda4d3fc9462cbe89")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/fonts")
if(NOT EXISTS "${SPARAMVIEW_JP_FONT}")
  file(DOWNLOAD
    "https://raw.githubusercontent.com/google/fonts/295d98a7a0c17c68f1341eaeea354e7960ea70d3/ofl/notosansjp/NotoSansJP%5Bwght%5D.ttf"
    "${SPARAMVIEW_JP_FONT}.tmp" TLS_VERIFY ON TIMEOUT 120 STATUS _jp_status)
  list(GET _jp_status 0 _jp_code)
  if(NOT _jp_code EQUAL 0)
    file(REMOVE "${SPARAMVIEW_JP_FONT}.tmp")
    message(FATAL_ERROR "Pinned Japanese font download failed: ${_jp_status}")
  endif()
  file(RENAME "${SPARAMVIEW_JP_FONT}.tmp" "${SPARAMVIEW_JP_FONT}")
endif()
execute_process(COMMAND "${GIT_EXECUTABLE}" hash-object "${SPARAMVIEW_JP_FONT}"
  RESULT_VARIABLE _jp_result OUTPUT_VARIABLE _jp_actual OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _jp_result EQUAL 0 OR NOT _jp_actual STREQUAL _jp_blob)
  file(REMOVE "${SPARAMVIEW_JP_FONT}")
  message(FATAL_ERROR "Pinned Japanese font blob identity mismatch")
endif()
install(FILES "${SPARAMVIEW_JP_FONT}" DESTINATION fonts)
if(WIN32)
  install(FILES "${SPARAMVIEW_JP_FONT}" DESTINATION tests/fonts)
endif()
