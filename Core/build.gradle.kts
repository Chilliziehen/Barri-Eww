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
    executionData(fileTree(layout.buildDirectory.dir("jacoco")) {
        include("*.exec")
    })
    reports {
        xml.required = true
    }
}

tasks.jacocoTestCoverageVerification {
    dependsOn(tasks.test, nativeIntegrationTest)
    executionData(fileTree(layout.buildDirectory.dir("jacoco")) {
        include("*.exec")
    })
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
