#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_dir"

required=(cmake ninja arm-none-eabi-gcc qemu-system-arm)
missing=()
for command_name in "${required[@]}"; do
    if ! command -v "$command_name" >/dev/null 2>&1; then
        missing+=("$command_name")
    fi
done

if (("${#missing[@]}" != 0)); then
    printf 'Missing WSL commands: %s\n' "${missing[*]}" >&2
    printf 'Install: cmake ninja-build gcc-arm-none-eabi qemu-system-arm\n' >&2
    exit 2
fi

cmake --preset qemu-arm32-freertos
cmake --build --preset qemu-arm32-freertos
ctest --preset qemu-arm32-freertos
