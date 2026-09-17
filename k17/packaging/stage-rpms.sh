#!/usr/bin/env bash
# Portable adaptation of the prototype staging recipe, not an installer.
set -euo pipefail
src=$(cd -- "$(dirname -- "$0")/../.." && pwd)
build=$(realpath "${1:?Usage: stage-rpms.sh BUILD_DIR NEW_OUTPUT_DIR}")
output=$(realpath -m "${2:?Usage: stage-rpms.sh BUILD_DIR NEW_OUTPUT_DIR}")
test ! -e "$output" || { echo 'Output directory must not already exist' >&2; exit 1; }
kver=$(cat "$build/include/config/kernel.release")
test -s "$build/arch/x86/boot/bzImage"
test -s "$build/Module.symvers"
mkdir -p "$output/stage/usr/lib/modules/$kver" "$output/stage/usr/src/kernels/$kver" "$output/SOURCES" "$output/SPECS"
stage=$output/stage
make -C "$src" O="$build" INSTALL_MOD_PATH="$stage/usr" INSTALL_MOD_STRIP=1 DEPMOD=true modules_install
cp "$build/arch/x86/boot/bzImage" "$stage/usr/lib/modules/$kver/vmlinuz"
cp "$build/System.map" "$stage/usr/lib/modules/$kver/System.map"
cp "$build/.config" "$stage/usr/lib/modules/$kver/config"
gzip -c "$build/Module.symvers" > "$stage/usr/lib/modules/$kver/symvers.gz"
make -C "$src" O="$build" run-command KBUILD_RUN_COMMAND="$src/scripts/package/install-extmod-build $stage/usr/src/kernels/$kver"
ln -sfn "/usr/src/kernels/$kver" "$stage/usr/lib/modules/$kver/build"
if test -L "$stage/usr/lib/modules/$kver/source"; then unlink "$stage/usr/lib/modules/$kver/source"; fi
sed "s/^%global kver .*/%global kver $kver/" "$src/k17/packaging/kernel-k17-vrr4.spec" > "$output/SPECS/kernel-k17-vrr4.spec"
tar czf "$output/SOURCES/kernel-vrr4-stage.tar.gz" -C "$stage" usr
(cd "$output/SOURCES" && sha256sum kernel-vrr4-stage.tar.gz > SHA256SUMS)
printf 'Prototype staged in %s; RPM spec is for the archived 7.2.4 experiment only.\n' "$output"
