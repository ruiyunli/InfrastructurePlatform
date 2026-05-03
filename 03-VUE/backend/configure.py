#!/usr/bin/env python3
import platform
import shutil
import subprocess
import sys
from pathlib import Path

def pause():
    input("Press Enter to continue...")


def run_command(command, cwd=None):
    print(f"Running: {' '.join(command)}")
    result = subprocess.run(command, cwd=cwd, check=False)
    if result.returncode != 0:
        raise RuntimeError(f"Command failed with exit code {result.returncode}: {' '.join(command)}")


def main():
    root = Path(__file__).resolve().parent
    print(f"Working directory: {Path.cwd()}")
    print(f"Root location: {root}")

    # check if cmake is available
    cmake_exe = shutil.which("cmake")
    if cmake_exe is None:
        raise RuntimeError("CMake executable not found on PATH. Please install CMake first.")

    # define variable
    configure_cmd = None
    platform_name = None

    system = platform.system()
    if system == "Windows":
        platform_name = "x64"
        build_dir = root / "build" / platform_name
        toolchain_file = root / "vcpkg" / "scripts" / "buildsystems" / "vcpkg.cmake"
        configure_cmd = [cmake_exe, "-S", str(root), "-B", str(build_dir), "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file}"]
    else:
        platform_name = "linux"
        build_dir = root / "build" / platform_name
        toolchain_file = root / "vcpkg" / "scripts" / "buildsystems" / "vcpkg.cmake"
        configure_cmd = [cmake_exe, "-S", str(root), "-B", str(build_dir), "-G", "Unix Makefiles", f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file}"]

    # create build and install directories
    (root / "build" / platform_name).mkdir(parents=True, exist_ok=True)

    # configure solution
    run_command(configure_cmd, cwd=str(root))

    print("Configure Solution completed successfully.")


if __name__ == "__main__":
    try:
        main()
        pause()
    except Exception as exc:
        print(f"Error: {exc}", file=sys.stderr)
        pause()
        sys.exit(1)
