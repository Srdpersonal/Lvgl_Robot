import os

Import("env")

env.BuildSources(
    os.path.join("$BUILD_DIR", ".pio","libdeps","Lvgl_Robot","lvgl", "build"),
    os.path.join("$PROJECT_DIR", ".pio","libdeps","Lvgl_Robot","lvgl","demos")
)
