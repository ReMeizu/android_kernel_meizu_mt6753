#!/usr/bin/env bash
set -euo pipefail

RAW_IMAGE="${1:-}"
SERIAL="${M2NOTE_SERIAL:-810BBMM22D7S}"
SYSTEM_BLOCK="${M2NOTE_SYSTEM_BLOCK:-/dev/block/mmcblk0p23}"
ADB_BIN="${ADB_BIN:-adb}"
VERIFY_BLOCKS="${M2NOTE_VERIFY_BLOCKS:-256}"
REBOOT_AFTER="${M2NOTE_REBOOT_AFTER_FLASH:-0}"

usage() {
  cat <<'EOF'
Usage: tools/m2note_flash_system_raw_direct.sh /path/to/system.raw.img

Environment:
  ADB_SERVER_SOCKET          optional reverse ADB server, e.g. tcp:127.0.0.1:15038
  M2NOTE_SERIAL              target serial, defaults to 810BBMM22D7S
  M2NOTE_SYSTEM_BLOCK        target block node, defaults to /dev/block/mmcblk0p23
  M2NOTE_VERIFY_BLOCKS       4K blocks to verify from start, defaults to 256 (1 MiB)
  M2NOTE_REBOOT_AFTER_FLASH  set to 1 to reboot after verification

This writes the raw image directly with adb sync:
  adb push system.raw.img /dev/block/mmcblk0p23

The script uses recovery shell only for non-streaming guards, sync, and
device-side sha256 verification. It does not stream image data through shell.
EOF
}

die() {
  echo "error: $*" >&2
  exit 1
}

if [[ -z "$RAW_IMAGE" || "$RAW_IMAGE" == "-h" || "$RAW_IMAGE" == "--help" ]]; then
  usage
  exit 0
fi

[[ -f "$RAW_IMAGE" ]] || die "missing raw image: $RAW_IMAGE"

raw_size="$(stat -c '%s' "$RAW_IMAGE")"
raw_sha="$(sha256sum "$RAW_IMAGE" | awk '{print $1}')"
host_first_sha="$(dd if="$RAW_IMAGE" bs=4096 count="$VERIFY_BLOCKS" 2>/dev/null | sha256sum | awk '{print $1}')"

echo "target_serial=$SERIAL"
echo "system_block=$SYSTEM_BLOCK"
echo "raw_image=$RAW_IMAGE"
echo "raw_size=$raw_size"
echo "raw_sha256=$raw_sha"
echo "host_first_${VERIFY_BLOCKS}x4k_sha256=$host_first_sha"

"$ADB_BIN" -s "$SERIAL" reconnect >/dev/null 2>&1 || true
sleep 2
state="$("$ADB_BIN" -s "$SERIAL" get-state 2>/dev/null || true)"
[[ "$state" == "recovery" || "$state" == "device" ]] || die "unexpected adb state: ${state:-missing}"
echo "adb_state=$state"

remote_info="$("$ADB_BIN" -s "$SERIAL" shell "ls -l '$SYSTEM_BLOCK'; blockdev --getsize64 '$SYSTEM_BLOCK' 2>/dev/null || true" | tr -d '\r')"
echo "$remote_info"
remote_size="$(printf '%s\n' "$remote_info" | tail -n 1)"
[[ "$remote_size" =~ ^[0-9]+$ ]] || die "cannot read remote block size"
(( remote_size >= raw_size )) || die "raw image does not fit: raw=$raw_size block=$remote_size"

"$ADB_BIN" -s "$SERIAL" shell "umount /system 2>/dev/null || true; umount /system_root 2>/dev/null || true; sync"

echo "writing_raw_to_block=1"
"$ADB_BIN" -s "$SERIAL" push "$RAW_IMAGE" "$SYSTEM_BLOCK"
"$ADB_BIN" -s "$SERIAL" shell sync

dev_first_sha="$("$ADB_BIN" -s "$SERIAL" shell "dd if='$SYSTEM_BLOCK' bs=4096 count='$VERIFY_BLOCKS' 2>/dev/null | sha256sum | awk '{print \$1}'" | tr -d '\r')"
echo "dev_first_${VERIFY_BLOCKS}x4k_sha256=$dev_first_sha"
[[ "$dev_first_sha" == "$host_first_sha" ]] || die "first-block verification mismatch"

echo "verified_first_blocks=1"
if [[ "$REBOOT_AFTER" == "1" ]]; then
  echo "rebooting=1"
  "$ADB_BIN" -s "$SERIAL" reboot
fi
