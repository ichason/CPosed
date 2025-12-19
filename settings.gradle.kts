@file:Suppress("UnstableApiUsage")
enableFeaturePreview("TYPESAFE_PROJECT_ACCESSORS")

pluginManagement {
    repositories {
        gradlePluginPortal()
        google()
        mavenCentral()
    }
}

dependencyResolutionManagement {
    repositoriesMode = RepositoriesMode.FAIL_ON_PROJECT_REPOS
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "LSPosed"
include(
    ":app",
    ":core",
    ":daemon",
    ":dex2oat",
    ":hiddenapi:stubs",
    ":hiddenapi:bridge",
    ":magisk-loader",
    ":libxposed:api",
    ":libxposed:service",
    ":libxposed:compat",
    ":services:manager-service",
    ":services:daemon-service",
)
