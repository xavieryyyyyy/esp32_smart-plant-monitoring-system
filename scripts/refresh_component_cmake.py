# This script runs on the computer before building, not on the ESP32.
# PlatformIO gives us env, which holds the project and build-folder settings.
# If a component build file changed, ask PlatformIO to refresh its CMake setup.
Import("env")

from pathlib import Path

project = Path(env.subst("$PROJECT_DIR"))
build = Path(env.subst("$PROJECT_BUILD_DIR")) / env["PIOENV"]
ninja_file = build / "build.ninja"

inputs = list(project.glob("components/**/CMakeLists.txt"))
inputs += list(project.glob("components/**/idf_component.yml"))

if ninja_file.exists():
    configured_at = ninja_file.stat().st_mtime_ns

    if any(path.stat().st_mtime_ns > configured_at for path in inputs):
        # Update the root file's modification time so PlatformIO notices a change.
        (project / "CMakeLists.txt").touch()
        print("Component configuration changed: refreshing CMake")