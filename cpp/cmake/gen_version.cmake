# Sinh litegcs_version.h = commit git ngắn của mã nguồn (chạy mỗi lần build,
# chỉ ghi file khi đổi để không build lại thừa). Không có git ⇒ "unknown".
#   cmake -DSRC=<thư mục nguồn> -DOUT=<file .h> -P gen_version.cmake
set(VERSION "unknown")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short=7 HEAD
        WORKING_DIRECTORY ${SRC}
        OUTPUT_VARIABLE GIT_REV
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE GIT_RES
        ERROR_QUIET)
    if(GIT_RES EQUAL 0 AND NOT GIT_REV STREQUAL "")
        set(VERSION "${GIT_REV}")
    endif()
endif()

set(CONTENT "#pragma once\n#define LITEGCS_VERSION \"${VERSION}\"\n")
set(OLD "")
if(EXISTS "${OUT}")
    file(READ "${OUT}" OLD)
endif()
if(NOT OLD STREQUAL CONTENT)
    file(WRITE "${OUT}" "${CONTENT}")
endif()
