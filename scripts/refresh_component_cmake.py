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
        # Trigger PlatformIO's existing CMake refresh check.
        (project / "CMakeLists.txt").touch()
        print("Component configuration changed: refreshing CMake")