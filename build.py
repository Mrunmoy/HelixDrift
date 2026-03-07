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
    cmake_cfg = (
        "cmake -S . -B build/host -G Ninja "
        "-DHELIXDRIFT_BUILD_TESTS=ON -DHELIXDRIFT_BUILD_NRF_EXAMPLES=OFF"
    )
    run(nix_wrap(cmake_cfg), repo_root)
    run(nix_wrap("cmake --build build/host --parallel"), repo_root)
    if run_tests:
        run(nix_wrap("ctest --test-dir build/host --output-on-failure"), repo_root)


def build_nrf(repo_root):
    run("git submodule update --init external/SensorFusion", repo_root)
    run("rm -rf build/nrf", repo_root)
    cmake_cfg = (
        "cmake -S . -B build/nrf -G Ninja "
        "-DCMAKE_TOOLCHAIN_FILE=tools/toolchains/arm-none-eabi-gcc.cmake "
        "-DHELIXDRIFT_BUILD_TESTS=OFF -DHELIXDRIFT_BUILD_NRF_EXAMPLES=ON"
    )
    run(nix_wrap(cmake_cfg), repo_root)
    run(nix_wrap("cmake --build build/nrf --parallel"), repo_root)


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
        "--nrf-only", action="store_true",
        help="Build nRF target only."
    )
    parser.add_argument(
        "--clean", action="store_true",
        help="Remove build directory before building."
    )

    args = parser.parse_args()
    repo_root = os.path.dirname(os.path.abspath(__file__))

    if args.host_only and args.nrf_only:
        print("cannot combine --host-only and --nrf-only", file=sys.stderr)
        return 2

    if args.clean:
        run("rm -rf build", repo_root)

    if not args.nrf_only:
        build_host(repo_root, args.test)

    if not args.host_only:
        build_nrf(repo_root)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
