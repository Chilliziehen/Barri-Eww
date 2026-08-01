#!/usr/bin/env bash

set -euo pipefail

scriptDirectory="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
selectedModule="all"
buildConfiguration="debug"
selectedBackend="vulkan"
threadedRecording="on"
buildTests="on"
enableCoverage="off"

printUsage() {
    cat <<'USAGE'
Usage: build.sh [options]

Options:
  --module native|core|mod|editor|all
  --configuration debug|release
  --backend vulkan
  --threaded-recording on|off
  --tests on|off
  --coverage on|off
  --help
USAGE
}

fail() {
    printf 'error: %s\n' "$1" >&2
    exit 2
}

requireValue() {
    if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for $1"
    fi
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --module)
            requireValue "$@"
            selectedModule="$2"
            shift 2
            ;;
        --configuration)
            requireValue "$@"
            buildConfiguration="$2"
            shift 2
            ;;
        --backend)
            requireValue "$@"
            selectedBackend="$2"
            shift 2
            ;;
        --threaded-recording)
            requireValue "$@"
            threadedRecording="$2"
            shift 2
            ;;
        --tests)
            requireValue "$@"
            buildTests="$2"
            shift 2
            ;;
        --coverage)
            requireValue "$@"
            enableCoverage="$2"
            shift 2
            ;;
        --help)
            printUsage
            exit 0
            ;;
        *)
            fail "unknown option: $1"
            ;;
    esac
done

case "$selectedModule" in native|core|mod|editor|all) ;; *) fail "invalid module: $selectedModule" ;; esac
case "$buildConfiguration" in debug|release) ;; *) fail "invalid configuration: $buildConfiguration" ;; esac
case "$selectedBackend" in vulkan) ;; *) fail "unsupported backend: $selectedBackend" ;; esac
case "$threadedRecording" in on|off) ;; *) fail "invalid threaded-recording value: $threadedRecording" ;; esac
case "$buildTests" in on|off) ;; *) fail "invalid tests value: $buildTests" ;; esac
case "$enableCoverage" in on|off) ;; *) fail "invalid coverage value: $enableCoverage" ;; esac
if [[ "$enableCoverage" == "on" && "$buildTests" == "off" ]]; then
    fail "--coverage on requires --tests on"
fi

moduleSequence=()
if [[ "$selectedModule" == "all" ]]; then
    moduleSequence=(native core mod editor)
else
    moduleSequence=("$selectedModule")
fi

validateManifest() {
    local moduleName="$1"
    case "$moduleName" in
        native)
            [[ -f "$scriptDirectory/Native/CMakeLists.txt" ]] \
                || fail "Native/CMakeLists.txt is missing"
            ;;
        core)
            [[ -f "$scriptDirectory/Core/settings.gradle.kts" && -x "$scriptDirectory/Core/gradlew" ]] \
                || fail "Core Gradle manifest or wrapper is missing"
            ;;
        mod)
            [[ -f "$scriptDirectory/Mod/settings.gradle.kts" && -x "$scriptDirectory/Mod/gradlew" ]] \
                || fail "Mod build manifest is missing; Mod is not implemented"
            ;;
        editor)
            [[ -f "$scriptDirectory/Editor/package.json" ]] \
                || fail "Editor/package.json is missing; Editor is not implemented"
            ;;
    esac
}

cmakeConfiguration="Debug"
if [[ "$buildConfiguration" == "release" ]]; then
    cmakeConfiguration="Release"
fi
threadedRecordingCmake="ON"
[[ "$threadedRecording" == "off" ]] && threadedRecordingCmake="OFF"
buildTestsCmake="ON"
[[ "$buildTests" == "off" ]] && buildTestsCmake="OFF"
enableCoverageCmake="OFF"
[[ "$enableCoverage" == "on" ]] && enableCoverageCmake="ON"

nativeBuildDirectory="$scriptDirectory/Native/build-root-${buildConfiguration}-threaded-${threadedRecording}"
coreLibraryFile="$scriptDirectory/Core/build/libs/BarriEwwCore-0.1.0.jar"
nativeLibraryFile="$nativeBuildDirectory/ffm/libBarriEwwNativeFfm.so"
if [[ "${OS:-}" == "Windows_NT" ]]; then
    nativeLibraryFile="$nativeBuildDirectory/ffm/BarriEwwNativeFfm.dll"
fi

runCommand() {
    printf '+ '
    printf '%q ' "$@"
    printf '\n'
    "$@"
}

buildNative() {
    runCommand cmake -S "$scriptDirectory/Native" -B "$nativeBuildDirectory" \
        "-DCMAKE_BUILD_TYPE=$cmakeConfiguration" \
        "-DBARRIEWW_BACKEND_VULKAN=ON" \
        "-DTHREADED_RECORDING=$threadedRecordingCmake" \
        "-DBARRIEWW_BUILD_TESTS=$buildTestsCmake" \
        "-DBARRIEWW_ENABLE_COVERAGE=$enableCoverageCmake"
    runCommand cmake --build "$nativeBuildDirectory" --config "$cmakeConfiguration"
    if [[ "$buildTests" == "on" ]]; then
        runCommand ctest --test-dir "$nativeBuildDirectory" -C "$cmakeConfiguration" --output-on-failure
    fi
}

buildCore() {
    coreTasks=(clean assemble)
    if [[ "$buildTests" == "on" ]]; then
        coreTasks=(clean test assemble)
    fi
    if [[ "$enableCoverage" == "on" ]]; then
        [[ -f "$nativeLibraryFile" ]] \
            || fail "Core coverage requires the matching Native FFM library: $nativeLibraryFile"
        coreTasks+=(nativeIntegrationTest jacocoTestReport jacocoTestCoverageVerification)
        runCommand "$scriptDirectory/Core/gradlew" -p "$scriptDirectory/Core" \
            "${coreTasks[@]}" \
            "-PbarriewwNativeLibraryPath=$nativeLibraryFile" \
            "-PbarriewwConfiguration=$buildConfiguration" \
            "-PbarriewwBackend=$selectedBackend" \
            "-PbarriewwThreadedRecording=$threadedRecording" --no-daemon
    else
        runCommand "$scriptDirectory/Core/gradlew" -p "$scriptDirectory/Core" \
            "${coreTasks[@]}" \
            "-PbarriewwConfiguration=$buildConfiguration" \
            "-PbarriewwBackend=$selectedBackend" \
            "-PbarriewwThreadedRecording=$threadedRecording" --no-daemon
    fi
}

buildMod() {
    modTasks=(clean assemble)
    [[ "$buildTests" == "on" ]] && modTasks=(clean test assemble)
    if [[ "$enableCoverage" == "on" ]]; then
        modTasks+=(jacocoTestReport jacocoTestCoverageVerification)
    fi
    [[ -f "$coreLibraryFile" ]] \
        || fail "Mod packaging requires the selected Core jar: $coreLibraryFile"
    [[ -f "$nativeLibraryFile" ]] \
        || fail "Mod packaging requires the selected Native FFM library: $nativeLibraryFile"
    runCommand "$scriptDirectory/Mod/gradlew" -p "$scriptDirectory/Mod" \
        "${modTasks[@]}" \
        "-PbarriewwCoreLibraryPath=$coreLibraryFile" \
        "-PbarriewwNativeLibraryPath=$nativeLibraryFile" \
        "-PbarriewwConfiguration=$buildConfiguration" \
        "-PbarriewwBackend=$selectedBackend" \
        "-PbarriewwThreadedRecording=$threadedRecording" --no-daemon
}

buildEditor() {
    runCommand npm --prefix "$scriptDirectory/Editor" ci
    if [[ "$buildTests" == "on" ]]; then
        if [[ "$enableCoverage" == "on" ]]; then
            runCommand npm --prefix "$scriptDirectory/Editor" run test -- --coverage
        else
            runCommand npm --prefix "$scriptDirectory/Editor" test
        fi
    fi
    runCommand npm --prefix "$scriptDirectory/Editor" run build
}

for moduleName in "${moduleSequence[@]}"; do
    validateManifest "$moduleName"
    printf '\n== Building %s ==\n' "$moduleName"
    case "$moduleName" in
        native) buildNative ;;
        core) buildCore ;;
        mod) buildMod ;;
        editor) buildEditor ;;
    esac
done
