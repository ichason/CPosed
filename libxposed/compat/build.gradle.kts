plugins {
    alias(libs.plugins.agp.lib)
}

android {
    namespace = "io.github.libxposed"

    lint {
        checkReleaseBuilds = false
        abortOnError = true
    }
}

dependencies {
    compileOnly(libs.androidx.annotation)
}
