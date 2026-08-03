pluginManagement {
    val fabricLoomVersion = providers.gradleProperty("fabricLoomVersion").get()
    plugins {
        id("net.fabricmc.fabric-loom") version fabricLoomVersion
    }
    repositories {
        maven("https://maven.fabricmc.net/")
        mavenCentral()
        gradlePluginPortal()
    }
}

rootProject.name = "BarriEwwMod"
