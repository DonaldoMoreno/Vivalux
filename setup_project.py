#!/usr/bin/env python3
"""
Vivalux Project Setup Script
Configures and builds the Vivalux projection mapping application
for Windows, macOS, and Linux.
"""

import os
import sys
import platform
import subprocess
import shutil
from pathlib import Path


class ProjectSetup:
    """Handles project setup and build across multiple platforms."""

    def __init__(self):
        self.project_root = Path(__file__).parent.absolute()
        self.build_dir = self.project_root / "build"
        self.os_type = platform.system()
        self.is_macos = self.os_type == "Darwin"
        self.is_windows = self.os_type == "Windows"
        self.is_linux = self.os_type == "Linux"

    def print_header(self, text):
        """Print a formatted header."""
        print("\n" + "=" * 70)
        print(f"  {text}")
        print("=" * 70)

    def print_step(self, text):
        """Print a step indicator."""
        print(f"\n➜ {text}")

    def print_success(self, text):
        """Print success message."""
        print(f"✓ {text}")

    def print_error(self, text):
        """Print error message."""
        print(f"✗ {text}")

    def run_command(self, cmd, description=""):
        """Execute a shell command."""
        if description:
            self.print_step(description)

        try:
            if isinstance(cmd, str):
                # For shell commands with pipes
                result = subprocess.run(
                    cmd, shell=True, check=True, capture_output=False, text=True
                )
            else:
                # For command lists
                result = subprocess.run(
                    cmd, check=True, capture_output=False, text=True
                )

            return True
        except subprocess.CalledProcessError as e:
            self.print_error(f"Command failed: {' '.join(cmd) if isinstance(cmd, list) else cmd}")
            return False
        except Exception as e:
            self.print_error(f"Error executing command: {str(e)}")
            return False

    def detect_platform(self):
        """Detect and display current platform."""
        self.print_header("PLATFORM DETECTION")

        print(f"OS: {self.os_type}")
        print(f"Architecture: {platform.machine()}")
        print(f"Python Version: {sys.version.split()[0]}")

        if self.is_macos:
            print(f"macOS Version: {platform.mac_ver()[0]}")
        elif self.is_windows:
            print(f"Windows Version: {platform.win32_ver()[0]}")

        return True

    def check_dependencies(self):
        """Check for required build tools."""
        self.print_header("DEPENDENCY CHECK")

        required_tools = ["cmake", "git"]
        missing_tools = []

        for tool in required_tools:
            if shutil.which(tool):
                self.print_success(f"Found {tool}")
            else:
                self.print_error(f"Missing {tool}")
                missing_tools.append(tool)

        # Platform-specific checks
        if self.is_macos:
            if shutil.which("brew"):
                self.print_success("Found brew")
            else:
                self.print_error("Missing brew (needed for macOS dependencies)")
                missing_tools.append("brew")

        if missing_tools:
            self.print_error(f"\nPlease install missing tools: {', '.join(missing_tools)}")
            return False

        self.print_success("All required tools found")
        return True

    def clean_build_directory(self):
        """Remove existing build directory."""
        self.print_step("Cleaning previous build directory")

        if self.build_dir.exists():
            try:
                shutil.rmtree(self.build_dir)
                self.print_success("Build directory removed")
            except Exception as e:
                self.print_error(f"Failed to remove build directory: {str(e)}")
                return False
        else:
            self.print_success("No previous build directory found")

        return True

    def create_build_directory(self):
        """Create build directory."""
        self.print_step("Creating build directory")

        try:
            self.build_dir.mkdir(parents=True, exist_ok=True)
            self.print_success("Build directory created")
            return True
        except Exception as e:
            self.print_error(f"Failed to create build directory: {str(e)}")
            return False

    def configure_cmake(self):
        """Run CMake configuration."""
        self.print_header("CMAKE CONFIGURATION")

        if self.is_macos:
            return self._configure_cmake_macos()
        elif self.is_windows:
            return self._configure_cmake_windows()
        else:
            return self._configure_cmake_linux()

    def _configure_cmake_linux(self):
        """Configure CMake for Linux."""
        self.print_step("Configuring CMake for Linux (OpenGL)")

        vcpkg_toolchain = self.project_root / "vcpkg" / "scripts" / "buildsystems" / "vcpkg.cmake"

        if not vcpkg_toolchain.exists():
            self.print_error(f"vcpkg toolchain not found at {vcpkg_toolchain}")
            self.print_step("Attempting standard CMake configuration...")
            cmd = [
                "cmake",
                "-S", str(self.project_root),
                "-B", str(self.build_dir),
                "-DCMAKE_BUILD_TYPE=Release",
            ]
        else:
            cmd = [
                "cmake",
                "-S", str(self.project_root),
                "-B", str(self.build_dir),
                "-DCMAKE_BUILD_TYPE=Release",
                f"-DCMAKE_TOOLCHAIN_FILE={vcpkg_toolchain}",
            ]

        return self.run_command(cmd, "Running CMake configuration")

    def _configure_cmake_macos(self):
        """Configure CMake for macOS."""
        self.print_step("Configuring CMake for macOS (Vulkan + MoltenVK)")

        vcpkg_toolchain = self.project_root / "vcpkg" / "scripts" / "buildsystems" / "vcpkg.cmake"

        if not vcpkg_toolchain.exists():
            self.print_error(f"vcpkg toolchain not found at {vcpkg_toolchain}")
            self.print_error("Please ensure vcpkg is properly initialized")
            return False

        cmd = [
            "cmake",
            "-S", str(self.project_root),
            "-B", str(self.build_dir),
            "-DCMAKE_BUILD_TYPE=Release",
            f"-DCMAKE_TOOLCHAIN_FILE={vcpkg_toolchain}",
        ]

        return self.run_command(cmd, "Running CMake configuration")

    def _configure_cmake_windows(self):
        """Configure CMake for Windows."""
        self.print_step("Configuring CMake for Windows (OpenGL with Visual Studio)")

        vcpkg_toolchain = self.project_root / "vcpkg" / "scripts" / "buildsystems" / "vcpkg.cmake"

        if not vcpkg_toolchain.exists():
            self.print_error(f"vcpkg toolchain not found at {vcpkg_toolchain}")
            cmd = [
                "cmake",
                "-S", str(self.project_root),
                "-B", str(self.build_dir),
                "-DCMAKE_BUILD_TYPE=Release",
            ]
        else:
            cmd = [
                "cmake",
                "-S", str(self.project_root),
                "-B", str(self.build_dir),
                "-DCMAKE_BUILD_TYPE=Release",
                f"-DCMAKE_TOOLCHAIN_FILE={vcpkg_toolchain}",
            ]

        return self.run_command(cmd, "Running CMake configuration")

    def build_project(self):
        """Build the project."""
        self.print_header("BUILD")

        if self.is_windows:
            return self._build_windows()
        else:
            return self._build_unix()

    def _build_unix(self):
        """Build on macOS/Linux."""
        cmd = [
            "cmake",
            "--build", str(self.build_dir),
            "--config", "Release",
        ]
        return self.run_command(cmd, "Building project")

    def _build_windows(self):
        """Build on Windows."""
        cmd = [
            "cmake",
            "--build", str(self.build_dir),
            "--config", "Release",
        ]
        return self.run_command(cmd, "Building project")

    def setup_vcpkg(self):
        """Setup vcpkg if not already done."""
        vcpkg_exe = self.project_root / "vcpkg" / ("vcpkg.exe" if self.is_windows else "vcpkg")

        if not vcpkg_exe.exists():
            self.print_header("VCPKG SETUP")
            self.print_step("vcpkg not found, attempting to initialize...")

            vcpkg_script = self.project_root / "vcpkg" / ("bootstrap-vcpkg.bat" if self.is_windows else "bootstrap-vcpkg.sh")

            if vcpkg_script.exists():
                if self.is_windows:
                    success = self.run_command(str(vcpkg_script), "Bootstrapping vcpkg")
                else:
                    success = self.run_command(f"bash {vcpkg_script}", "Bootstrapping vcpkg")

                if not success:
                    self.print_error("Failed to bootstrap vcpkg")
                    return False
            else:
                self.print_error(f"vcpkg bootstrap script not found at {vcpkg_script}")
                return False

        return True

    def run_setup(self):
        """Execute the complete setup."""
        self.print_header("VIVALUX PROJECT SETUP")
        print(f"Project root: {self.project_root}")

        # Step 1: Detect platform
        if not self.detect_platform():
            return False

        # Step 2: Check dependencies
        if not self.check_dependencies():
            return False

        # Step 3: Setup vcpkg if needed
        if not self.setup_vcpkg():
            return False

        # Step 4: Clean build directory
        if not self.clean_build_directory():
            return False

        # Step 5: Create build directory
        if not self.create_build_directory():
            return False

        # Step 6: Configure CMake
        if not self.configure_cmake():
            return False

        # Step 7: Build project
        if not self.build_project():
            return False

        # Success
        self.print_header("SETUP COMPLETE")
        self.print_success("Project configured and built successfully!")

        self._print_next_steps()
        return True

    def _print_next_steps(self):
        """Print next steps for running the application."""
        print("\n" + "=" * 70)
        print("  NEXT STEPS")
        print("=" * 70)

        if self.is_windows:
            executable = self.build_dir / "src" / "Release" / "VivaLux.exe"
            print(f"\n1. Run the application:")
            print(f"   {executable}")
        else:
            executable = self.build_dir / "src" / "VivaLux"
            print(f"\n1. Run the application:")
            print(f"   {executable}")

        print("\n2. For development, use:")
        print(f"   cd {self.build_dir}")
        print(f"   cmake --build . --config Release")

        print("\n3. To clean and rebuild:")
        print(f"   python3 {self.project_root / 'setup_project.py'}")

        print("\n" + "=" * 70 + "\n")


def main():
    """Main entry point."""
    try:
        setup = ProjectSetup()
        success = setup.run_setup()
        sys.exit(0 if success else 1)
    except KeyboardInterrupt:
        print("\n\nSetup cancelled by user")
        sys.exit(1)
    except Exception as e:
        print(f"\nUnexpected error: {str(e)}")
        sys.exit(1)


if __name__ == "__main__":
    main()
