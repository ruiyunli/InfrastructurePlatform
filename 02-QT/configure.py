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
        configure_cmd = [cmake_exe, "-S", str(root), "-B", str(build_dir), "-G", "Visual Studio 17 2022", "-A", "x64"]
    else:
        platform_name = "linux"
        build_dir = root / "build" / platform_name
        configure_cmd = [cmake_exe, "-S", str(root), "-B", str(build_dir), "-G", "Unix Makefiles"]

    # create build and install directories
    (root / "build" / platform_name).mkdir(parents=True, exist_ok=True)
    (root / "build" / platform_name / "conan").mkdir(parents=True, exist_ok=True)
    (root / "install" / platform_name).mkdir(parents=True, exist_ok=True)

    # configure
    run_command(configure_cmd, cwd=str(root))

    print("Configure completed successfully.")

if __name__ == "__main__":
    try:
        main()
        pause()
    except Exception as exc:
        print(f"Error: {exc}", file=sys.stderr)
        pause()
        sys.exit(1)
