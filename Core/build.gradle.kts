import java.io.File
import java.nio.file.Path
import org.gradle.api.provider.Provider

// Barri-Eww Core build (spec §3.1: Gradle, Java SE 25; §4.1: JUnit 5; §4.5: JaCoCo).
plugins {
    `java-library`
    jacoco
}

group = "barrieww"
version = "0.1.0"

java {
    toolchain {
        languageVersion = JavaLanguageVersion.of(25)
    }
}

repositories {
    mavenCentral()
}

dependencies {
    testImplementation(platform("org.junit:junit-bom:5.11.4"))
    testImplementation("org.junit.jupiter:junit-jupiter")
    testRuntimeOnly("org.junit.platform:junit-platform-launcher")
}

tasks.test {
    useJUnitPlatform {
        excludeTags("nativeIntegration")
    }
    // Repository-level shared fixtures (cross-language golden files, ADR-0002).
    systemProperty(
        "barrieww.testDataDirectory",
        layout.projectDirectory.dir("../TestData").asFile.absolutePath)
}

val nativeLibraryFileProvider: Provider<File> = providers.provider {
    val nativeLibraryPathProperty = providers.gradleProperty(
        "barriewwNativeLibraryPath").orNull
        ?: throw GradleException(
            "nativeIntegrationTest requires -PbarriewwNativeLibraryPath=<absolute-library-file>")
    val nativeLibraryPath = Path.of(nativeLibraryPathProperty)
    if (!nativeLibraryPath.isAbsolute) {
        throw GradleException(
            "barriewwNativeLibraryPath must be absolute: $nativeLibraryPathProperty")
    }
    val nativeLibraryFile = nativeLibraryPath.normalize().toFile()
    if (!nativeLibraryFile.isFile) {
        throw GradleException(
            "barriewwNativeLibraryPath must name an existing file: $nativeLibraryFile")
    }
    nativeLibraryFile
}

val nativeIntegrationTest = tasks.register<Test>("nativeIntegrationTest") {
    description = "Runs the real Java-to-Native FFM integration tests."
    group = LifecycleBasePlugin.VERIFICATION_GROUP
    testClassesDirs = sourceSets.test.get().output.classesDirs
    classpath = sourceSets.test.get().runtimeClasspath
    useJUnitPlatform {
        includeTags("nativeIntegration")
    }
    jvmArgs("--enable-native-access=ALL-UNNAMED")
    systemProperty(
        "barrieww.testDataDirectory",
        layout.projectDirectory.dir("../TestData").asFile.absolutePath)
    shouldRunAfter(tasks.test)
    inputs.property(
        "nativeFfmSharedLibraryPath",
        providers.gradleProperty("barriewwNativeLibraryPath"))
    inputs.file(nativeLibraryFileProvider)
        .withPropertyName("nativeFfmSharedLibrary")
        .withPathSensitivity(PathSensitivity.NONE)
    doFirst {
        systemProperty(
            "barrieww.nativeLibraryPath",
            nativeLibraryFileProvider.get().absolutePath)
    }
}

tasks.jacocoTestReport {
    dependsOn(tasks.test, nativeIntegrationTest)
    // Only the main-library test suites contribute to the §4.5 coverage report; the demo
    // source set's demoTest exec is deliberately excluded.
    executionData(tasks.test.get(), nativeIntegrationTest.get())
    reports {
        xml.required = true
    }
}

tasks.jacocoTestCoverageVerification {
    dependsOn(tasks.test, nativeIntegrationTest)
    executionData(tasks.test.get(), nativeIntegrationTest.get())
    violationRules {
        rule {
            limit {
                counter = "LINE"
                value = "COVEREDRATIO"
                minimum = "0.90".toBigDecimal()
            }
        }
    }
}

// ── Visible demo (ADR-0004): Java LWJGL/GLFW bootstrap + presentation FFM binding. ──
// The window-dependent presentation runtime binding and the GLFW app live here, not in the
// main library, so they are integration/orchestration glue outside the §4.5 main-library
// coverage gate (jacoco reports over the main source set only).
val lwjglVersion = "3.3.4"
val lwjglNatives = when {
    org.gradle.internal.os.OperatingSystem.current().isWindows -> "natives-windows"
    org.gradle.internal.os.OperatingSystem.current().isLinux -> "natives-linux"
    org.gradle.internal.os.OperatingSystem.current().isMacOsX -> "natives-macos"
    else -> throw GradleException("The visible demo supports Windows, Linux and macOS only")
}

val demo = sourceSets.create("demo") {
    compileClasspath += sourceSets.main.get().output
    runtimeClasspath += sourceSets.main.get().output
}
val demoTest = sourceSets.create("demoTest") {
    compileClasspath += sourceSets.main.get().output + demo.output
    runtimeClasspath += sourceSets.main.get().output + demo.output
}

configurations["demoImplementation"].extendsFrom(configurations.implementation.get())
configurations["demoTestImplementation"].extendsFrom(
    configurations["demoImplementation"], configurations.testImplementation.get())
configurations["demoTestRuntimeOnly"].extendsFrom(configurations.testRuntimeOnly.get())

dependencies {
    "demoImplementation"(platform("org.lwjgl:lwjgl-bom:$lwjglVersion"))
    "demoImplementation"("org.lwjgl:lwjgl")
    "demoImplementation"("org.lwjgl:lwjgl-glfw")
    "demoImplementation"("org.lwjgl:lwjgl-vulkan")
    "demoRuntimeOnly"("org.lwjgl:lwjgl::$lwjglNatives")
    "demoRuntimeOnly"("org.lwjgl:lwjgl-glfw::$lwjglNatives")
}

val demoTestTask = tasks.register<Test>("demoTest") {
    description = "Runs the demo presentation-runtime FFM binding tests."
    group = LifecycleBasePlugin.VERIFICATION_GROUP
    testClassesDirs = demoTest.output.classesDirs
    classpath = demoTest.runtimeClasspath
    useJUnitPlatform()
    jvmArgs("--enable-native-access=ALL-UNNAMED")
    inputs.property(
        "nativeFfmSharedLibraryPath",
        providers.gradleProperty("barriewwNativeLibraryPath"))
    inputs.file(nativeLibraryFileProvider)
        .withPropertyName("nativeFfmSharedLibrary")
        .withPathSensitivity(PathSensitivity.NONE)
    doFirst {
        systemProperty(
            "barrieww.nativeLibraryPath",
            nativeLibraryFileProvider.get().absolutePath)
    }
}

tasks.register<JavaExec>("runVisibleDemo") {
    description = "Runs the visible cross-platform presentation demo (requires a display)."
    group = ApplicationPlugin.APPLICATION_GROUP
    mainClass = "barrieww.core.demo.VisibleClearDemo"
    classpath = demo.runtimeClasspath
    jvmArgs("--enable-native-access=ALL-UNNAMED")
    doFirst {
        systemProperty(
            "barrieww.nativeLibraryPath",
            nativeLibraryFileProvider.get().absolutePath)
    }
}
