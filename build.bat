@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "scriptDirectory=%~dp0"
set "selectedModule=all"
set "buildConfiguration=debug"
set "selectedBackend=vulkan"
set "threadedRecording=on"
set "buildTests=on"
set "enableCoverage=off"

:parseArguments
if "%~1"=="" goto validateArguments
if "%~1"=="--help" goto printUsage
if "%~1"=="--module" goto parseModule
if "%~1"=="--configuration" goto parseConfiguration
if "%~1"=="--backend" goto parseBackend
if "%~1"=="--threaded-recording" goto parseThreadedRecording
if "%~1"=="--tests" goto parseTests
if "%~1"=="--coverage" goto parseCoverage
call :fail "unknown option: %~1"
exit /b %errorlevel%

:parseModule
call :requireValue "%~1" "%~2" || exit /b %errorlevel%
set "selectedModule=%~2"
shift
shift
goto parseArguments

:parseConfiguration
call :requireValue "%~1" "%~2" || exit /b %errorlevel%
set "buildConfiguration=%~2"
shift
shift
goto parseArguments

:parseBackend
call :requireValue "%~1" "%~2" || exit /b %errorlevel%
set "selectedBackend=%~2"
shift
shift
goto parseArguments

:parseThreadedRecording
call :requireValue "%~1" "%~2" || exit /b %errorlevel%
set "threadedRecording=%~2"
shift
shift
goto parseArguments

:parseTests
call :requireValue "%~1" "%~2" || exit /b %errorlevel%
set "buildTests=%~2"
shift
shift
goto parseArguments

:parseCoverage
call :requireValue "%~1" "%~2" || exit /b %errorlevel%
set "enableCoverage=%~2"
shift
shift
goto parseArguments

:validateArguments
call :isOneOf "%selectedModule%" "native core mod editor all" || (
    call :fail "invalid module: %selectedModule%"
    exit /b !errorlevel!
)
call :isOneOf "%buildConfiguration%" "debug release" || (
    call :fail "invalid configuration: %buildConfiguration%"
    exit /b !errorlevel!
)
if /i not "%selectedBackend%"=="vulkan" (
    call :fail "unsupported backend: %selectedBackend%"
    exit /b !errorlevel!
)
call :isOneOf "%threadedRecording%" "on off" || (
    call :fail "invalid threaded-recording value: %threadedRecording%"
    exit /b !errorlevel!
)
call :isOneOf "%buildTests%" "on off" || (
    call :fail "invalid tests value: %buildTests%"
    exit /b !errorlevel!
)
call :isOneOf "%enableCoverage%" "on off" || (
    call :fail "invalid coverage value: %enableCoverage%"
    exit /b !errorlevel!
)
if /i "%enableCoverage%"=="on" if /i "%buildTests%"=="off" (
    call :fail "--coverage on requires --tests on"
    exit /b !errorlevel!
)

if /i "%selectedModule%"=="all" (
    set "moduleSequence=native core mod editor"
) else (
    set "moduleSequence=%selectedModule%"
)

for %%M in (%moduleSequence%) do call :validateManifest %%M || exit /b !errorlevel!

set "cmakeConfiguration=Debug"
if /i "%buildConfiguration%"=="release" set "cmakeConfiguration=Release"
set "threadedRecordingCmake=ON"
if /i "%threadedRecording%"=="off" set "threadedRecordingCmake=OFF"
set "buildTestsCmake=ON"
if /i "%buildTests%"=="off" set "buildTestsCmake=OFF"
set "enableCoverageCmake=OFF"
if /i "%enableCoverage%"=="on" set "enableCoverageCmake=ON"
set "nativeBuildDirectory=%scriptDirectory%Native\build-root-%buildConfiguration%-threaded-%threadedRecording%"

for %%M in (%moduleSequence%) do (
    echo.
    echo == Building %%M ==
    call :buildModule %%M
    if errorlevel 1 exit /b !errorlevel!
)
exit /b 0

:buildModule
if /i "%~1"=="native" call :buildNative & exit /b !errorlevel!
if /i "%~1"=="core" call :buildCore & exit /b !errorlevel!
if /i "%~1"=="mod" call :buildMod & exit /b !errorlevel!
if /i "%~1"=="editor" call :buildEditor & exit /b !errorlevel!
exit /b 2

:buildNative
call :run cmake -S "%scriptDirectory%Native" -B "%nativeBuildDirectory%" -DCMAKE_BUILD_TYPE=%cmakeConfiguration% -DBARRIEWW_BACKEND_VULKAN=ON -DTHREADED_RECORDING=%threadedRecordingCmake% -DBARRIEWW_BUILD_TESTS=%buildTestsCmake% -DBARRIEWW_ENABLE_COVERAGE=%enableCoverageCmake% || exit /b !errorlevel!
call :run cmake --build "%nativeBuildDirectory%" --config %cmakeConfiguration% || exit /b !errorlevel!
if /i "%buildTests%"=="on" call :run ctest --test-dir "%nativeBuildDirectory%" -C %cmakeConfiguration% --output-on-failure || exit /b !errorlevel!
exit /b 0

:buildCore
set "coreTasks=clean assemble"
if /i "%buildTests%"=="on" set "coreTasks=clean test"
if /i "%enableCoverage%"=="on" (
    set "nativeLibraryFile=%nativeBuildDirectory%\ffm\BarriEwwNativeFfm.dll"
    if not exist "!nativeLibraryFile!" (
        call :fail "Core coverage requires the matching Native FFM library: !nativeLibraryFile!"
        exit /b !errorlevel!
    )
    set "coreTasks=!coreTasks! nativeIntegrationTest jacocoTestReport jacocoTestCoverageVerification"
    call :run "%scriptDirectory%Core\gradlew.bat" -p "%scriptDirectory%Core" !coreTasks! -PbarriewwNativeLibraryPath="!nativeLibraryFile!" -PbarriewwConfiguration=%buildConfiguration% -PbarriewwBackend=%selectedBackend% -PbarriewwThreadedRecording=%threadedRecording% --no-daemon
) else (
    call :run "%scriptDirectory%Core\gradlew.bat" -p "%scriptDirectory%Core" !coreTasks! -PbarriewwConfiguration=%buildConfiguration% -PbarriewwBackend=%selectedBackend% -PbarriewwThreadedRecording=%threadedRecording% --no-daemon
)
exit /b %errorlevel%

:buildMod
set "modTasks=clean assemble"
if /i "%buildTests%"=="on" set "modTasks=clean test"
call :run "%scriptDirectory%Mod\gradlew.bat" -p "%scriptDirectory%Mod" %modTasks% -PbarriewwConfiguration=%buildConfiguration% -PbarriewwBackend=%selectedBackend% -PbarriewwThreadedRecording=%threadedRecording% --no-daemon
exit /b %errorlevel%

:buildEditor
call :run npm --prefix "%scriptDirectory%Editor" ci || exit /b !errorlevel!
if /i "%buildTests%"=="on" (
    if /i "%enableCoverage%"=="on" (
        call :run npm --prefix "%scriptDirectory%Editor" run test -- --coverage || exit /b !errorlevel!
    ) else (
        call :run npm --prefix "%scriptDirectory%Editor" test || exit /b !errorlevel!
    )
)
call :run npm --prefix "%scriptDirectory%Editor" run build
exit /b %errorlevel%

:validateManifest
if /i "%~1"=="native" if not exist "%scriptDirectory%Native\CMakeLists.txt" call :fail "Native/CMakeLists.txt is missing" & exit /b !errorlevel!
if /i "%~1"=="core" if not exist "%scriptDirectory%Core\settings.gradle.kts" call :fail "Core Gradle manifest is missing" & exit /b !errorlevel!
if /i "%~1"=="core" if not exist "%scriptDirectory%Core\gradlew.bat" call :fail "Core Gradle wrapper is missing" & exit /b !errorlevel!
if /i "%~1"=="mod" if not exist "%scriptDirectory%Mod\settings.gradle.kts" call :fail "Mod build manifest is missing; Mod is not implemented" & exit /b !errorlevel!
if /i "%~1"=="mod" if not exist "%scriptDirectory%Mod\gradlew.bat" call :fail "Mod Gradle wrapper is missing; Mod is not implemented" & exit /b !errorlevel!
if /i "%~1"=="editor" if not exist "%scriptDirectory%Editor\package.json" call :fail "Editor/package.json is missing; Editor is not implemented" & exit /b !errorlevel!
exit /b 0

:requireValue
if "%~2"=="" call :fail "missing value for %~1" & exit /b !errorlevel!
if "%~2:~0,2"=="--" call :fail "missing value for %~1" & exit /b !errorlevel!
exit /b 0

:isOneOf
for %%V in (%~2) do if /i "%~1"=="%%V" exit /b 0
exit /b 1

:run
echo + %*
call %*
exit /b %errorlevel%

:fail
echo error: %~1 1>&2
exit /b 2

:printUsage
echo Usage: build.bat [options]
echo.
echo Options:
echo   --module native^|core^|mod^|editor^|all
echo   --configuration debug^|release
echo   --backend vulkan
echo   --threaded-recording on^|off
echo   --tests on^|off
echo   --coverage on^|off
echo   --help
exit /b 0
