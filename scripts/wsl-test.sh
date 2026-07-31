#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_dir"

required=(cmake ninja riscv64-unknown-elf-gcc qemu-system-riscv32)
missing=()
for command_name in "${required[@]}"; do
    if ! command -v "$command_name" >/dev/null 2>&1; then
        missing+=("$command_name")
    fi
done

if ((${#missing[@]} != 0)); then
    printf 'Missing WSL commands: %s\n' "${missing[*]}" >&2
    printf 'On Ubuntu/Debian install: cmake ninja-build gcc-riscv64-unknown-elf qemu-system-misc\n' >&2
    exit 2
fi

cmake --preset qemu-riscv32-freertos
cmake --build --preset qemu-riscv32-freertos
ctest --preset qemu-riscv32-freertos
