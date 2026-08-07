import java.io.File
import java.nio.file.Path
import java.util.Locale
import java.util.zip.ZipFile
import org.gradle.api.provider.Provider

val mockitoAgent = configurations.create("mockitoAgent")

fun selectNativeResourcePath(
    operatingSystemName: String,
    architectureName: String): String {
    val normalizedOperatingSystemName = operatingSystemName.lowercase(Locale.ROOT)
    val normalizedArchitectureName = architectureName.lowercase(Locale.ROOT)
    val isSupportedArchitecture = normalizedArchitectureName == "amd64" ||
        normalizedArchitectureName == "x86_64"
    if (!isSupportedArchitecture) {
        throw GradleException(
            "Native packaging supports only x86_64 architecture: $normalizedArchitectureName")
    }
    return when {
        normalizedOperatingSystemName.startsWith("windows") ->
            "barrieww/native/windows-x86_64/BarriEwwNativeFfm.dll"
        normalizedOperatingSystemName.startsWith("linux") ->
            "barrieww/native/linux-x86_64/libBarriEwwNativeFfm.so"
        else -> throw GradleException(
            "Native packaging supports only Windows and Linux: $normalizedOperatingSystemName")
    }
}

plugins {
    id("net.fabricmc.fabric-loom")
    `java-library`
    jacoco
}

group = "barrieww"
version = property("modVersion") as String
val modVersionValue = version.toString()

base {
    archivesName = "BarriEwwMod"
}

java {
    toolchain {
        languageVersion = JavaLanguageVersion.of(25)
    }
    withSourcesJar()
}

repositories {
    mavenCentral()
}

val defaultCoreLibraryFile = layout.projectDirectory.file(
    "../Core/build/libs/BarriEwwCore-0.1.0.jar").asFile
val coreLibraryFileProvider: Provider<File> = providers.gradleProperty(
    "barriewwCoreLibraryPath").map { coreLibraryPathProperty ->
        val coreLibraryPath = Path.of(coreLibraryPathProperty)
        if (!coreLibraryPath.isAbsolute) {
            throw GradleException(
                "barriewwCoreLibraryPath must be absolute: $coreLibraryPathProperty")
        }
        val coreLibraryFile = coreLibraryPath.normalize().toFile()
        if (!coreLibraryFile.isFile) {
            throw GradleException(
                "barriewwCoreLibraryPath must name an existing Core jar: $coreLibraryFile")
        }
        coreLibraryFile
    }.orElse(providers.provider {
        if (!defaultCoreLibraryFile.isFile) {
            throw GradleException(
                "Core jar is required. Supply -PbarriewwCoreLibraryPath=<absolute-core-jar> " +
                    "or build ${defaultCoreLibraryFile.absolutePath}")
        }
        defaultCoreLibraryFile
    })
val coreLibraryFiles = files(coreLibraryFileProvider)

dependencies {
    minecraft("com.mojang:minecraft:${property("minecraftVersion")}")
    implementation("net.fabricmc:fabric-loader:${property("fabricLoaderVersion")}")
    implementation("net.fabricmc.fabric-api:fabric-api:${property("fabricApiVersion")}")

    implementation(coreLibraryFiles)
    testImplementation(platform("org.junit:junit-bom:5.11.4"))
    testImplementation("org.junit.jupiter:junit-jupiter")
    testImplementation("org.mockito:mockito-core:5.18.0")
    mockitoAgent("org.mockito:mockito-core:5.18.0") { isTransitive = false }
    testRuntimeOnly("org.junit.platform:junit-platform-launcher")
}

val buildConfigurationProvider = providers.gradleProperty(
    "barriewwConfiguration").orElse("debug").map { buildConfiguration ->
        if (buildConfiguration !in setOf("debug", "release")) {
            throw GradleException(
                "barriewwConfiguration must be debug or release: $buildConfiguration")
        }
        buildConfiguration
    }
val renderingBackendProvider = providers.gradleProperty(
    "barriewwBackend").orElse("vulkan").map { renderingBackend ->
        if (renderingBackend != "vulkan") {
            throw GradleException(
                "barriewwBackend currently supports only vulkan: $renderingBackend")
        }
        renderingBackend
    }
val threadedRecordingProvider = providers.gradleProperty(
    "barriewwThreadedRecording").orElse("on").map { threadedRecording ->
        if (threadedRecording !in setOf("on", "off")) {
            throw GradleException(
                "barriewwThreadedRecording must be on or off: $threadedRecording")
        }
        threadedRecording
    }

val nativeLibraryFileProvider: Provider<File> = providers.gradleProperty(
    "barriewwNativeLibraryPath").map { nativeLibraryPathProperty ->
        val nativeLibraryPath = Path.of(nativeLibraryPathProperty)
        if (!nativeLibraryPath.isAbsolute) {
            throw GradleException(
                "barriewwNativeLibraryPath must be absolute: $nativeLibraryPathProperty")
        }
        val nativeLibraryFile = nativeLibraryPath.normalize().toFile()
        if (!nativeLibraryFile.isFile) {
            throw GradleException(
                "barriewwNativeLibraryPath must name an existing Native FFM library: " +
                    nativeLibraryFile)
        }
        nativeLibraryFile
    }.orElse(providers.provider {
        throw GradleException(
            "Native FFM library is required for packaging. Supply " +
                "-PbarriewwNativeLibraryPath=<absolute-native-library>")
    })

val nativeResourcePathProvider: Provider<String> = providers.provider {
    selectNativeResourcePath(
        System.getProperty("os.name"),
        System.getProperty("os.arch"))
}
val validatedNativeLibraryFileProvider = nativeLibraryFileProvider.zip(
    nativeResourcePathProvider) { nativeLibraryFile, nativeResourcePath ->
        val expectedNativeLibraryFileName = nativeResourcePath.substringAfterLast('/')
        if (nativeLibraryFile.name != expectedNativeLibraryFileName) {
            throw GradleException(
                "barriewwNativeLibraryPath must name $expectedNativeLibraryFileName for this platform: " +
                    nativeLibraryFile)
        }
        nativeLibraryFile
    }

val validatePackagingArtifacts = tasks.register("validatePackagingArtifacts") {
    inputs.file(coreLibraryFileProvider)
        .withPropertyName("coreLibrary")
        .withPathSensitivity(PathSensitivity.NONE)
    inputs.file(validatedNativeLibraryFileProvider)
        .withPropertyName("nativeFfmLibrary")
        .withPathSensitivity(PathSensitivity.NONE)
    inputs.property("nativeResourcePath", nativeResourcePathProvider)
}

tasks.withType<JavaCompile>().configureEach {
    options.release = 25
}

tasks.test {
    useJUnitPlatform()
    workingDir(layout.buildDirectory)
    jvmArgs(
        "--enable-native-access=ALL-UNNAMED",
        "-javaagent:${mockitoAgent.asPath}")
}

tasks.processResources {
    inputs.property("modVersion", modVersionValue)
    filesMatching("fabric.mod.json") {
        expand("version" to modVersionValue)
    }
    if (providers.gradleProperty("barriewwNativeLibraryPath").isPresent) {
        from(validatedNativeLibraryFileProvider) {
            into(nativeResourcePathProvider.map { nativeResourcePath ->
                nativeResourcePath.substringBeforeLast('/')
            })
        }
    }
}

tasks.jacocoTestReport {
    dependsOn(tasks.test)
    reports {
        xml.required = true
    }
}

tasks.jacocoTestCoverageVerification {
    dependsOn(tasks.test)
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

val verifyNativeResourceSelectionLocaleIndependence = tasks.register(
    "verifyNativeResourceSelectionLocaleIndependence") {
    doLast {
        val originalDefaultLocale = Locale.getDefault()
        try {
            Locale.setDefault(Locale.forLanguageTag("tr-TR"))
            val nativeResourcePath = selectNativeResourcePath("WINDOWS", "AMD64")
            if (nativeResourcePath !=
                "barrieww/native/windows-x86_64/BarriEwwNativeFfm.dll") {
                throw GradleException(
                    "Native resource selection changed under the Turkish locale: " +
                        nativeResourcePath)
            }
        } finally {
            Locale.setDefault(originalDefaultLocale)
        }
    }
}

tasks.check {
    dependsOn(tasks.jacocoTestCoverageVerification)
    dependsOn(verifyNativeResourceSelectionLocaleIndependence)
}

tasks.jar {
    dependsOn(validatePackagingArtifacts)
    inputs.property("buildConfiguration", buildConfigurationProvider)
    inputs.property("renderingBackend", renderingBackendProvider)
    inputs.property("threadedRecording", threadedRecordingProvider)
    from(coreLibraryFileProvider.map { coreLibraryFile -> zipTree(coreLibraryFile) }) {
        exclude("META-INF/MANIFEST.MF")
    }
    manifest {
        attributes(
            "BarriEww-Configuration" to buildConfigurationProvider.get(),
            "BarriEww-Backend" to renderingBackendProvider.get(),
            "BarriEww-Threaded-Recording" to threadedRecordingProvider.get())
    }
}

val remapJar = tasks.register("remapJar") {
    description = "Builds the distributable jar; Minecraft 26.2 is unobfuscated."
    group = LifecycleBasePlugin.BUILD_GROUP
    dependsOn(tasks.jar)
}

// Entries the host-image presentation path cannot run without. A jar that builds but
// omits any of them fails only at game start, so packaging asserts them here instead.
val requiredRemappedJarEntryProvider: Provider<List<String>> =
    nativeResourcePathProvider.map { nativeResourcePath ->
        listOf(
            // Host target lifecycle and surface takeover mixins.
            "barrieww/mod/mixin/GameRendererMixin.class",
            "barrieww/mod/mixin/VulkanGpuSurfaceMixin.class",
            // Generation tracking and host binding extraction.
            "barrieww/mod/MainRenderTargetGenerationTracker.class",
            "barrieww/mod/MainRenderTargetResizeListener.class",
            "barrieww/mod/MinecraftHostImagePresentationBindingExtractor.class",
            "barrieww/mod/PresentationTakeoverCoordinator.class",
            // Core classes merged into the mod jar.
            "barrieww/core/interoperability/HostImagePresentationBinding.class",
            "barrieww/core/interoperability/PresentationImageFormat.class",
            "barrieww/core/interoperability/NativePresentationRuntime.class",
            // Declarations and the platform native library.
            "barrieww.mixins.json",
            "fabric.mod.json",
            nativeResourcePath)
    }

val verifyRemappedJarContents = tasks.register("verifyRemappedJarContents") {
    description = "Asserts the distributable jar carries every host-image presentation entry."
    group = LifecycleBasePlugin.VERIFICATION_GROUP
    dependsOn(remapJar)
    inputs.file(tasks.jar.flatMap { jarTask -> jarTask.archiveFile })
        .withPropertyName("distributableJar")
        .withPathSensitivity(PathSensitivity.NONE)
    inputs.property("requiredEntries", requiredRemappedJarEntryProvider)
    outputs.upToDateWhen { true }
    val distributableJarFileProvider = tasks.jar.flatMap { jarTask -> jarTask.archiveFile }
    val requiredEntries = requiredRemappedJarEntryProvider
    doLast {
        val distributableJarFile = distributableJarFileProvider.get().asFile
        val presentEntryNames = ZipFile(distributableJarFile).use { zipFile ->
            zipFile.entries().asSequence().map { entry -> entry.name }.toSet()
        }
        val missingEntryNames = requiredEntries.get().filterNot(presentEntryNames::contains)
        if (missingEntryNames.isNotEmpty()) {
            throw GradleException(
                "Distributable jar ${distributableJarFile.name} is missing required entries:\n"
                    + missingEntryNames.joinToString("\n") { entryName -> " - $entryName" })
        }
    }
}

// Registered after verifyRemappedJarContents so the check wiring needs no forward reference.
tasks.check {
    dependsOn(verifyRemappedJarContents)
}

tasks.assemble {
    dependsOn(remapJar)
}

loom {
    runs {
        named("client") {
            jvmArguments.add("--enable-native-access=ALL-UNNAMED")
        }
    }
}

tasks.named<JavaExec>("runClient") {
    args("--graphicsBackend", "vulkan")
}
