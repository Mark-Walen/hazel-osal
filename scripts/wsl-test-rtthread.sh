#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
rt_root="$repo_dir/src/third-party/rt-thread"
bsp_dir="$rt_root/bsp/qemu-virt64-riscv"

required=(cmake scons riscv64-unknown-elf-gcc qemu-system-riscv64)
missing=()
for command_name in "${required[@]}"; do
    if ! command -v "$command_name" >/dev/null 2>&1; then
        missing+=("$command_name")
    fi
done

if ((${#missing[@]} != 0)); then
    printf 'Missing WSL commands: %s\n' "${missing[*]}" >&2
    printf 'On Ubuntu/Debian install: scons gcc-riscv64-unknown-elf qemu-system-misc\n' >&2
    exit 2
fi

mkdir -p "$repo_dir/build"
work_dir="$(mktemp -d "$repo_dir/build/rtthread-qemu.XXXXXX")"
overlay_root="$work_dir/rt-root"
cleanup() {
    if [[ "${OSAL_KEEP_BUILD:-0}" == "1" ]]; then
        printf 'Kept RT-Thread build at %s\n' "$work_dir"
    else
        cmake -E remove_directory "$work_dir"
    fi
}
trap cleanup EXIT

cmake -E copy_directory "$bsp_dir" "$work_dir"
mkdir -p "$overlay_root/libcpu/risc-v"
cmake -E copy \
    "$repo_dir/tests/rtthread_qemu/libcpu.SConscript" \
    "$overlay_root/libcpu/SConscript"
cmake -E copy \
    "$repo_dir/tests/rtthread_qemu/riscv.SConscript" \
    "$overlay_root/libcpu/risc-v/SConscript"
cmake -E create_symlink "$rt_root/include" "$overlay_root/include"
cmake -E create_symlink "$rt_root/src" "$overlay_root/src"
cmake -E create_symlink "$rt_root/components" "$overlay_root/components"
cmake -E create_symlink "$rt_root/tools" "$overlay_root/tools"
cmake -E create_symlink \
    "$rt_root/libcpu/risc-v/common" \
    "$overlay_root/libcpu/risc-v/common"
cmake -E create_symlink \
    "$rt_root/libcpu/risc-v/virt64" \
    "$overlay_root/libcpu/risc-v/virt64"
mkdir -p "$overlay_root/libcpu/risc-v/common64"
cmake -E copy \
    "$repo_dir/tests/rtthread_qemu/common64.SConscript" \
    "$overlay_root/libcpu/risc-v/common64/SConscript"
cmake -E copy "$repo_dir/tests/osal_test.c" "$work_dir/applications/main.c"
cmake -E copy \
    "$repo_dir/tests/rtthread_qemu/SConscript" \
    "$work_dir/applications/SConscript"
cmake -E copy \
    "$repo_dir/tests/rtthread_qemu/driver.SConscript" \
    "$work_dir/driver/SConscript"
cmake -E copy \
    "$repo_dir/tests/rtthread_qemu/rtconfig.h" \
    "$work_dir/rtconfig.h"
cmake -E copy \
    "$repo_dir/tests/rtthread_qemu/rtconfig.py" \
    "$work_dir/rtconfig.py"

(
    cd "$work_dir"
    RTT_ROOT="$overlay_root" \
        RTTHREAD_SOURCE_ROOT="$rt_root" \
        OSAL_ROOT="$repo_dir" \
        scons -j"$(nproc)"
)

output_file="$work_dir/qemu-output.log"
set +e
timeout 20 qemu-system-riscv64 \
        -machine virt \
        -smp 1 \
        -nographic \
        -kernel "$work_dir/rtthread.elf" 2>&1 | tee "$output_file"
status="${PIPESTATUS[0]}"
set -e

if ! grep -a -q "PASS: OSAL tests" "$output_file" ||
        grep -a -q "FAIL:" "$output_file"; then
    exit 1
fi
