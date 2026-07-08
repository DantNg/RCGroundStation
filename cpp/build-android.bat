@echo off
rem Build APK Android cho Tram Dieu Khien Mat Dat (C++/Qt6) tren Windows.
rem
rem YEU CAU cai truoc (mot lan):
rem   1. Kit "Qt cho Android" (chay C:\Qt\MaintenanceTool.exe -> Add Components ->
rem      Qt 6.10.3 -> "Android"). Sinh thu muc C:\Qt\6.10.3\android_arm64_v8a.
rem   2. Android SDK (Platform API 34) + NDK + build-tools + platform-tools.
rem      Cach de nhat: cai qua Qt Creator (Tools -> Devices -> Android) hoac
rem      Android Studio. Ghi nho duong dan SDK va NDK.
rem   3. OpenJDK 17 (Temurin/Microsoft) va bien JAVA_HOME tro toi no.
rem
rem DAT BIEN MOI TRUONG (sua duong dan cho may ban) roi chay build-android.bat:
rem   set QT_ANDROID=C:\Qt\6.10.3\android_arm64_v8a
rem   set QT_HOST_PATH=C:\Qt\6.10.3\mingw_64
rem   set ANDROID_SDK_ROOT=%LOCALAPPDATA%\Android\Sdk
rem   set ANDROID_NDK_ROOT=%LOCALAPPDATA%\Android\Sdk\ndk\<phien-ban>
rem   set JAVA_HOME=C:\Program Files\Eclipse Adoptium\jdk-17...
rem
rem   build-android.bat            cau hinh + build APK (Release)
rem   build-android.bat --clean    xoa build-android roi lam lai
rem   build-android.bat --install  build xong cai vao thiet bi qua adb
rem
rem Giu ASCII (cmd.exe doc theo byte).
setlocal enableextensions
cd /d "%~dp0"

set "BUILD_DIR=build-android"
set "DO_CLEAN=0"
set "DO_INSTALL=0"

:parse
if "%~1"=="" goto after
if /i "%~1"=="--clean"   set "DO_CLEAN=1"
if /i "%~1"=="--install" set "DO_INSTALL=1"
if /i "%~1"=="--help"    goto help
if /i "%~1"=="-h"        goto help
shift
goto parse
:after

rem -- kiem tra bien moi truong bat buoc --
if not defined QT_ANDROID (
    for /f "delims=" %%d in ('dir /b /ad "C:\Qt" 2^>nul') do (
        if exist "C:\Qt\%%d\android_arm64_v8a\bin\qmake" set "QT_ANDROID=C:\Qt\%%d\android_arm64_v8a"
    )
)
if not defined QT_HOST_PATH (
    for /f "delims=" %%d in ('dir /b /ad "C:\Qt" 2^>nul') do (
        if exist "C:\Qt\%%d\mingw_64\bin\qmake.exe" set "QT_HOST_PATH=C:\Qt\%%d\mingw_64"
    )
)
if not defined ANDROID_SDK_ROOT set "ANDROID_SDK_ROOT=%LOCALAPPDATA%\Android\Sdk"

if not defined QT_ANDROID (
    echo [LOI] Chua tim thay kit Qt cho Android. Cai qua MaintenanceTool roi
    echo       dat: set QT_ANDROID=C:\Qt\6.10.3\android_arm64_v8a
    goto fail
)
if not exist "%QT_ANDROID%\lib\cmake\Qt6\qt.toolchain.cmake" (
    echo [LOI] Khong thay qt.toolchain.cmake trong %QT_ANDROID%.
    echo       Kiem tra lai QT_ANDROID.
    goto fail
)
if not exist "%ANDROID_SDK_ROOT%" (
    echo [LOI] Khong thay Android SDK tai %ANDROID_SDK_ROOT%.
    echo       Dat: set ANDROID_SDK_ROOT=duong-dan-SDK
    goto fail
)

echo QT_ANDROID:       %QT_ANDROID%
echo QT_HOST_PATH:     %QT_HOST_PATH%
echo ANDROID_SDK_ROOT: %ANDROID_SDK_ROOT%
if defined ANDROID_NDK_ROOT echo ANDROID_NDK_ROOT: %ANDROID_NDK_ROOT%

if "%DO_CLEAN%"=="1" if exist "%BUILD_DIR%" (
    echo Xoa %BUILD_DIR%\ ...
    rmdir /s /q "%BUILD_DIR%"
)

echo Cau hinh (preset android-arm64) ...
cmake --preset android-arm64
if errorlevel 1 goto fail

echo Bien dich + dong goi APK ...
cmake --build "%BUILD_DIR%" --target apk
if errorlevel 1 goto fail

echo.
echo OK. APK o: %BUILD_DIR%\android-build\ (tim file .apk trong build\outputs\apk)
dir /s /b "%BUILD_DIR%\*.apk" 2>nul

if "%DO_INSTALL%"=="1" (
    echo Cai APK qua adb ...
    for /f "delims=" %%f in ('dir /s /b "%BUILD_DIR%\*-debug.apk" 2^>nul') do set "APK=%%f"
    if defined APK "%ANDROID_SDK_ROOT%\platform-tools\adb" install -r "%APK%"
)
endlocal
exit /b 0

:fail
echo [LOI] Build Android that bai.
endlocal
exit /b 1

:help
echo build-android.bat [--clean] [--install]
echo   Can bien moi truong: QT_ANDROID, QT_HOST_PATH, ANDROID_SDK_ROOT, ANDROID_NDK_ROOT, JAVA_HOME
echo   --clean    xoa build-android roi lam lai
echo   --install  build xong cai vao thiet bi qua adb
endlocal
exit /b 0
