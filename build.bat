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

set "cmakeConfiguration=Debug"
if /i "%buildConfiguration%"=="release" set "cmakeConfiguration=Release"
set "threadedRecordingCmake=ON"
if /i "%threadedRecording%"=="off" set "threadedRecordingCmake=OFF"
set "buildTestsCmake=ON"
if /i "%buildTests%"=="off" set "buildTestsCmake=OFF"
set "enableCoverageCmake=OFF"
if /i "%enableCoverage%"=="on" set "enableCoverageCmake=ON"
set "nativeBuildDirectory=%scriptDirectory%Native\build-root-%buildConfiguration%-threaded-%threadedRecording%"
set "coreLibraryFile=%scriptDirectory%Core\build\libs\BarriEwwCore-0.1.0.jar"
set "nativeLibraryFile=%nativeBuildDirectory%\ffm\BarriEwwNativeFfm.dll"

set "firstFailureCode=0"
for %%M in (%moduleSequence%) do call :processModule %%M
exit /b %firstFailureCode%

:processModule
if not "%firstFailureCode%"=="0" exit /b 0
call :validateManifest %~1
set "moduleExitCode=%errorlevel%"
if not "%moduleExitCode%"=="0" goto recordModuleFailure
echo.
echo == Building %~1 ==
call :buildModule %~1
set "moduleExitCode=%errorlevel%"
if not "%moduleExitCode%"=="0" goto recordModuleFailure
exit /b 0

:recordModuleFailure
set "firstFailureCode=%moduleExitCode%"
exit /b 0

:buildModule
if /i "%~1"=="native" goto buildNative
if /i "%~1"=="core" goto buildCore
if /i "%~1"=="mod" goto buildMod
if /i "%~1"=="editor" goto buildEditor
exit /b 2

:buildNative
echo + cmake -S "%scriptDirectory%Native" -B "%nativeBuildDirectory%" -DCMAKE_BUILD_TYPE=%cmakeConfiguration% -DBARRIEWW_BACKEND_VULKAN=ON -DTHREADED_RECORDING=%threadedRecordingCmake% -DBARRIEWW_BUILD_TESTS=%buildTestsCmake% -DBARRIEWW_ENABLE_COVERAGE=%enableCoverageCmake%
call cmake -S "%scriptDirectory%Native" -B "%nativeBuildDirectory%" -DCMAKE_BUILD_TYPE=%cmakeConfiguration% -DBARRIEWW_BACKEND_VULKAN=ON -DTHREADED_RECORDING=%threadedRecordingCmake% -DBARRIEWW_BUILD_TESTS=%buildTestsCmake% -DBARRIEWW_ENABLE_COVERAGE=%enableCoverageCmake%
if errorlevel 1 exit /b !errorlevel!
echo + cmake --build "%nativeBuildDirectory%" --config %cmakeConfiguration%
call cmake --build "%nativeBuildDirectory%" --config %cmakeConfiguration%
if errorlevel 1 exit /b !errorlevel!
if /i "%buildTests%"=="on" (
    echo + ctest --test-dir "%nativeBuildDirectory%" -C %cmakeConfiguration% --output-on-failure
    call ctest --test-dir "%nativeBuildDirectory%" -C %cmakeConfiguration% --output-on-failure
    if errorlevel 1 exit /b !errorlevel!
)
exit /b 0

:buildCore
set "coreTasks=clean assemble"
set "coreNativeLibraryArgument="
if /i "%buildTests%"=="on" set "coreTasks=clean test assemble"
if /i "%enableCoverage%"=="on" (
    if not exist "!nativeLibraryFile!" (
        call :fail "Core coverage requires the matching Native FFM library: !nativeLibraryFile!"
        exit /b 2
    )
    set "coreTasks=!coreTasks! nativeIntegrationTest jacocoTestReport jacocoTestCoverageVerification"
    set "coreNativeLibraryArgument=-PbarriewwNativeLibraryPath="!nativeLibraryFile!""
)
echo + "%scriptDirectory%Core\gradlew.bat" -p "%scriptDirectory%Core" !coreTasks! !coreNativeLibraryArgument! -PbarriewwConfiguration=%buildConfiguration% -PbarriewwBackend=%selectedBackend% -PbarriewwThreadedRecording=%threadedRecording% --no-daemon
call "%scriptDirectory%Core\gradlew.bat" -p "%scriptDirectory%Core" !coreTasks! !coreNativeLibraryArgument! -PbarriewwConfiguration=%buildConfiguration% -PbarriewwBackend=%selectedBackend% -PbarriewwThreadedRecording=%threadedRecording% --no-daemon
exit /b %errorlevel%

:buildMod
set "modTasks=clean assemble"
if /i "%buildTests%"=="on" set "modTasks=clean test assemble"
if /i "%enableCoverage%"=="on" set "modTasks=!modTasks! jacocoTestReport jacocoTestCoverageVerification"
if not exist "!coreLibraryFile!" (
    call :fail "Mod packaging requires the selected Core jar: !coreLibraryFile!"
    exit /b 2
)
if not exist "!nativeLibraryFile!" (
    call :fail "Mod packaging requires the selected Native FFM library: !nativeLibraryFile!"
    exit /b 2
)
call :run "%scriptDirectory%Mod\gradlew.bat" -p "%scriptDirectory%Mod" !modTasks! -PbarriewwCoreLibraryPath="!coreLibraryFile!" -PbarriewwNativeLibraryPath="!nativeLibraryFile!" -PbarriewwConfiguration=%buildConfiguration% -PbarriewwBackend=%selectedBackend% -PbarriewwThreadedRecording=%threadedRecording% --no-daemon
exit /b %errorlevel%

:buildEditor
echo + npm --prefix "%scriptDirectory%Editor" ci
call npm --prefix "%scriptDirectory%Editor" ci
if errorlevel 1 exit /b !errorlevel!
if /i "%buildTests%"=="on" (
    if /i "%enableCoverage%"=="on" (
        echo + npm --prefix "%scriptDirectory%Editor" run test -- --coverage
        call npm --prefix "%scriptDirectory%Editor" run test -- --coverage
        if errorlevel 1 exit /b !errorlevel!
    ) else (
        echo + npm --prefix "%scriptDirectory%Editor" test
        call npm --prefix "%scriptDirectory%Editor" test
        if errorlevel 1 exit /b !errorlevel!
    )
)
echo + npm --prefix "%scriptDirectory%Editor" run build
call npm --prefix "%scriptDirectory%Editor" run build
exit /b %errorlevel%

:validateManifest
if /i "%~1"=="native" goto validateNativeManifest
if /i "%~1"=="core" goto validateCoreManifest
if /i "%~1"=="mod" goto validateModManifest
if /i "%~1"=="editor" goto validateEditorManifest
exit /b 2

:validateNativeManifest
if exist "%scriptDirectory%Native\CMakeLists.txt" exit /b 0
call :fail "Native/CMakeLists.txt is missing"
exit /b 2

:validateCoreManifest
if not exist "%scriptDirectory%Core\settings.gradle.kts" goto coreManifestMissing
if not exist "%scriptDirectory%Core\gradlew.bat" goto coreWrapperMissing
exit /b 0

:coreManifestMissing
call :fail "Core Gradle manifest is missing"
exit /b 2

:coreWrapperMissing
call :fail "Core Gradle wrapper is missing"
exit /b 2

:validateModManifest
if not exist "%scriptDirectory%Mod\settings.gradle.kts" goto modManifestMissing
if not exist "%scriptDirectory%Mod\gradlew.bat" goto modWrapperMissing
exit /b 0

:modManifestMissing
call :fail "Mod build manifest is missing; Mod is not implemented"
exit /b 2

:modWrapperMissing
call :fail "Mod Gradle wrapper is missing; Mod is not implemented"
exit /b 2

:validateEditorManifest
if exist "%scriptDirectory%Editor\package.json" exit /b 0
call :fail "Editor/package.json is missing; Editor is not implemented"
exit /b 2

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
