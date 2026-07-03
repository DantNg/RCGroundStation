@echo off
rem Auto-build cho Tram Dieu Khien Mat Dat (C++/Qt6) tren Windows (cmd.exe).
rem
rem Tu do Qt (ban mingw_64 trong C:\Qt), Ninja va toolchain MinGW cua Qt.
rem Ghi de bang bien moi truong QT_PREFIX / MINGW_DIR neu cai o noi khac.
rem Ban Linux / Raspberry Pi (va Git Bash) dung build.sh.
rem
rem   build.bat            cau hinh + build Release
rem   build.bat --clean    xoa build roi dung lai tu dau
rem   build.bat --debug    build kieu Debug
rem   build.bat --run      build xong chay luon
rem   build.bat -j 4       gioi han so luong bien dich
rem
rem File nay CO Y giu thuan ASCII: cmd.exe doc file lenh theo byte, ky tu da byte
rem (UTF-8) se lam lech con tro va vo parser. Vi vay khong dau, khong chcp.
setlocal enableextensions
cd /d "%~dp0"

set "BUILD_DIR=build"
set "BUILD_TYPE=Release"
set "DO_CLEAN=0"
set "DO_RUN=0"
set "JOBS="

:parse
if "%~1"=="" goto after
if /i "%~1"=="--clean"   set "DO_CLEAN=1"
if /i "%~1"=="--debug"   set "BUILD_TYPE=Debug"
if /i "%~1"=="--release" set "BUILD_TYPE=Release"
if /i "%~1"=="--run"     set "DO_RUN=1"
if /i "%~1"=="--help"    goto help
if /i "%~1"=="-h"        goto help
rem -j lay gia tri o tham so ke tiep (%~2), roi shift phu de bo qua no.
if /i "%~1"=="-j" set "JOBS=%~2"
if /i "%~1"=="-j" shift
shift
goto parse
:after

if not defined JOBS set "JOBS=%NUMBER_OF_PROCESSORS%"
if not defined JOBS set "JOBS=4"

rem -- do Qt (mingw_64) --
if not defined QT_PREFIX (
    for /f "delims=" %%d in ('dir /b /ad "C:\Qt" 2^>nul') do (
        if exist "C:\Qt\%%d\mingw_64\bin\qmake.exe" set "QT_PREFIX=C:\Qt\%%d\mingw_64"
    )
)
rem -- do toolchain MinGW cua Qt --
if not defined MINGW_DIR (
    for /f "delims=" %%d in ('dir /b /ad "C:\Qt\Tools" 2^>nul ^| findstr /i "mingw"') do (
        if exist "C:\Qt\Tools\%%d\bin\g++.exe" set "MINGW_DIR=C:\Qt\Tools\%%d"
    )
)

set "CMAKE_ARGS=-S . -B %BUILD_DIR% -DCMAKE_BUILD_TYPE=%BUILD_TYPE%"

rem -- trinh sinh: uu tien Ninja --
set "NINJA="
where ninja >nul 2>nul && set "NINJA=1"
if not defined NINJA if defined MINGW_DIR if exist "%MINGW_DIR%\bin\ninja.exe" (
    set "PATH=%MINGW_DIR%\bin;%PATH%"
    set "NINJA=1"
)
if defined NINJA set "CMAKE_ARGS=%CMAKE_ARGS% -G Ninja"

if defined QT_PREFIX (
    echo Qt:      %QT_PREFIX%
    set "CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_PREFIX_PATH=%QT_PREFIX:\=/%"
    set "PATH=%QT_PREFIX%\bin;%PATH%"
) else (
    echo [canh bao] Khong tim thay Qt trong C:\Qt - dat QT_PREFIX neu cau hinh loi.
)
if defined MINGW_DIR (
    echo MinGW:   %MINGW_DIR%
    set "CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_CXX_COMPILER=%MINGW_DIR:\=/%/bin/g++.exe -DCMAKE_C_COMPILER=%MINGW_DIR:\=/%/bin/gcc.exe"
    set "PATH=%MINGW_DIR%\bin;%PATH%"
)

rem -- build --
if "%DO_CLEAN%"=="1" if exist "%BUILD_DIR%" (
    echo Xoa %BUILD_DIR%\ ...
    rmdir /s /q "%BUILD_DIR%"
)

echo Cau hinh (%BUILD_TYPE%) ...
cmake %CMAKE_ARGS%
if errorlevel 1 goto fail

echo Bien dich (-j%JOBS%) ...
cmake --build "%BUILD_DIR%" -j %JOBS%
if errorlevel 1 goto fail

echo OK: %BUILD_DIR%\LiteGCS.exe
if "%DO_RUN%"=="1" (
    echo Chay %BUILD_DIR%\LiteGCS.exe ...
    "%BUILD_DIR%\LiteGCS.exe"
)
endlocal
exit /b 0

:fail
echo [LOI] Build that bai.
endlocal
exit /b 1

:help
echo build.bat [--clean] [--debug ^| --release] [--run] [-j N]
echo   --clean   xoa build roi dung lai tu dau
echo   --debug   build kieu Debug (mac dinh Release)
echo   --run     build xong chay luon
echo   -j N      so luong bien dich (mac dinh = so CPU)
echo Ghi de: dat bien QT_PREFIX / MINGW_DIR truoc khi chay.
endlocal
exit /b 0
