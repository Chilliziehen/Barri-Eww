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
    useJUnitPlatform()
    // Repository-level shared fixtures (cross-language golden files, ADR-0002).
    systemProperty(
        "barrieww.testDataDirectory",
        layout.projectDirectory.dir("../TestData").asFile.absolutePath)
}

tasks.jacocoTestReport {
    dependsOn(tasks.test)
    reports {
        xml.required = true
    }
}
