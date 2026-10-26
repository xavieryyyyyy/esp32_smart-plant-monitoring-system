# This script runs on the computer before building, not on the ESP32.
# PlatformIO gives us env, which holds the project and build-folder settings.
# If a component build file changed, ask PlatformIO to refresh its CMake setup.
# Ask PlatformIO/SCons for its existing build environment; this script is not run as a normal
# standalone Python program.
Import("env")

# Use Path objects to join folders and inspect file modification times.
from pathlib import Path

# Find the project folder from the build environment instead of assuming the terminal working
# directory.
project = Path(env.subst("$PROJECT_DIR"))
# Find this selected environment's build folder, where its generated build.ninja lives.
build = Path(env.subst("$PROJECT_BUILD_DIR")) / env["PIOENV"]
# Point to Ninja's generated build instructions; their timestamp marks the last configuration pass.
ninja_file = build / "build.ninja"

# Watch every component CMakeLists.txt, because it decides which C files are compiled and linked.
inputs = list(project.glob("components/**/CMakeLists.txt"))
# Also watch dependency manifests, because changing a managed library may require reconfiguration.
inputs += list(project.glob("components/**/idf_component.yml"))

# Only compare timestamps after a previous CMake build file actually exists.
if ninja_file.exists():
    # Read the last configuration time in nanoseconds to avoid losing closely spaced edits.
    configured_at = ninja_file.stat().st_mtime_ns

    # Check whether any watched input is newer than the generated Ninja configuration.
    if any(path.stat().st_mtime_ns > configured_at for path in inputs):
        # Update the root file's modification time so PlatformIO notices a change.
        # Touch only the root build file timestamp; this asks PlatformIO to refresh CMake without
        # rewriting its contents.
        (project / "CMakeLists.txt").touch()
        # Explain the refresh in build output so this automatic action is visible.
        print("Component configuration changed: refreshing CMake")