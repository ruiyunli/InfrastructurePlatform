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
    # check if cmake is available
    cmake_exe = shutil.which("cmake")
    if cmake_exe is None:
        raise RuntimeError("CMake executable not found on PATH. Please install CMake first.")

    # define variable
    configure_cmd = None
    platform_name = None

    system = platform.system()
    if system == "Windows":
        configure_cmd = [cmake_exe, "-S", ".", "-B", "build", "-G", "Visual Studio 17 2022", "-A", "x64"]
        platform_name = "x64"
    else:
        configure_cmd = [cmake_exe, "-S", ".", "-B", "build", "-G", "Unix Makefiles"]
        platform_name = "linux"

    # create build and install directories
    Path("build", platform_name).mkdir(parents=True, exist_ok=True)
    Path("install", platform_name).mkdir(parents=True, exist_ok=True)

    # configure
    run_command(configure_cmd, cwd=".")

    print("Configure completed successfully.")

if __name__ == "__main__":
    try:
        main()
        pause()
    except Exception as exc:
        print(f"Error: {exc}", file=sys.stderr)
        pause()
        sys.exit(1)
