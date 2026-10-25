@echo off
rem Build HOTD2 Arcade VR for the Quest: this Flycast fork, arm64, optimised "vr" build type.
rem Needs JDK 17 and the Android SDK with NDK 29 and CMake 3.22.1. A Microsoft JDK 17 in
rem Program Files is picked up; otherwise set JAVA_HOME. ANDROID_HOME defaults to
rem %LOCALAPPDATA%\Android\Sdk.
setlocal
for /d %%J in ("%ProgramFiles%\Microsoft\jdk-17*") do set "JAVA_HOME=%%J"
if not defined ANDROID_HOME set "ANDROID_HOME=%LOCALAPPDATA%\Android\Sdk"
cd /d "%~dp0..\shell\android-studio" || exit /b 1
rem Full path: a bare gradlew.bat isn't found when NoDefaultCurrentDirectoryInExePath is set.
call "%CD%\gradlew.bat" assembleVr --console=plain %* || exit /b 1
echo.
echo Built: %~dp0..\shell\android-studio\flycast\build\outputs\apk\vr\flycast-vr.apk
