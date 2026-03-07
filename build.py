#!/usr/bin/env python3
import argparse
import os
import shlex
import subprocess
import sys


def run(cmd, cwd):
    print("+", cmd)
    subprocess.run(cmd, cwd=cwd, shell=True, check=True)


def nix_wrap(inner_cmd):
    return "nix develop --command bash -lc " + shlex.quote(inner_cmd)


def build_host(repo_root, run_tests):
    run("git submodule update --init external/SensorFusion", repo_root)
    cmake_cfg = (
        "cmake -S . -B build/host -G Ninja "
        "-DHELIXDRIFT_BUILD_TESTS=ON"
    )
    run(nix_wrap(cmake_cfg), repo_root)
    run(nix_wrap("cmake --build build/host --parallel"), repo_root)
    if run_tests:
        run(nix_wrap("ctest --test-dir build/host --output-on-failure"), repo_root)

def build_esp32s3(repo_root):
    run("git submodule update --init external/SensorFusion third_party/esp-idf", repo_root)
    run("bash tools/esp/setup_idf.sh", repo_root)
    run("rm -rf build/esp32s3", repo_root)
    esp_cmd = (
        "source third_party/esp-idf/export.sh && "
        "idf.py -C examples/esp32s3-mocap-node "
        "-B build/esp32s3 "
        "set-target esp32s3 build"
    )
    run(f"bash -lc {shlex.quote(esp_cmd)}", repo_root)


def main():
    parser = argparse.ArgumentParser(
        description="HelixDrift build orchestrator."
    )
    parser.add_argument(
        "-t", "--test", action="store_true",
        help="Run host test suite after host build."
    )
    parser.add_argument(
        "--host-only", action="store_true",
        help="Build host targets only."
    )
    parser.add_argument(
        "--esp32s3-only", action="store_true",
        help="Build ESP32-S3 target only."
    )
    parser.add_argument(
        "--clean", action="store_true",
        help="Remove build directory before building."
    )

    args = parser.parse_args()
    repo_root = os.path.dirname(os.path.abspath(__file__))

    selected = int(args.host_only) + int(args.esp32s3_only)
    if selected > 1:
        print("cannot combine --host-only and --esp32s3-only", file=sys.stderr)
        return 2

    if args.clean:
        run("rm -rf build", repo_root)

    if not args.esp32s3_only:
        build_host(repo_root, args.test)

    if not args.host_only:
        build_esp32s3(repo_root)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
