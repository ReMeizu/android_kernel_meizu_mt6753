#!/sbin/sh
#
# Recovery-side helper for the conservative m2note GSI layout:
#   p23 system   2 GiB
#   p24 cache    64 MiB
#   p25 userdata moved right by 176 MiB, then grown to the new end
#
# This is intentionally not an Android-userspace script. Run it only from TWRP
# or an equivalent recovery, after a host-side userdata/GPT backup exists.

set -eu

MODE="${1:-preflight}"
ALLOW="${M2NOTE_REPARTITION_ALLOW:-}"

DISK="/dev/block/mmcblk0"
SYSTEM_PART="/dev/block/mmcblk0p23"
CACHE_PART="/dev/block/mmcblk0p24"
DATA_PART="/dev/block/mmcblk0p25"

OLD_SYSTEM_START=1474560
OLD_SYSTEM_SIZE=3145728
OLD_CACHE_START=4620288
OLD_CACHE_SIZE=819200
OLD_DATA_START=5439488
OLD_DATA_SIZE=25062400
FLASHINFO_START=30501888
FLASHINFO_SIZE=32768

NEW_SYSTEM_START=1474560
NEW_SYSTEM_END=5668863
NEW_CACHE_START=5668864
NEW_CACHE_END=5799935
NEW_DATA_START=5799936
NEW_DATA_END=30501887

SECTORS_PER_4K=8
BLOCK_SIZE=4096
DATA_MARGIN_BLOCKS="${DATA_MARGIN_BLOCKS:-4096}"
COPY_CHUNK_BLOCKS="${COPY_CHUNK_BLOCKS:-32768}"
BACKUP_DIR="${BACKUP_DIR:-/tmp/m2note-repartition-2g}"

die() {
  echo "ERROR: $*" >&2
  exit 1
}

usage() {
  cat <<'EOF'
Usage:
  sh tools/m2note_repartition_2g_recovery.sh preflight
  M2NOTE_REPARTITION_ALLOW=M2NOTE_2G_DATA_MOVE_OK sh tools/m2note_repartition_2g_recovery.sh move
  M2NOTE_REPARTITION_ALLOW=M2NOTE_2G_DATA_MOVE_OK sh tools/m2note_repartition_2g_recovery.sh finish

Modes:
  preflight  Verify recovery tools and current partition geometry; no writes.
  move       Unmount, fsck+shrink userdata, copy userdata bytes forward,
             rewrite GPT to 2 GiB system + 64 MiB cache, then stop.
             Reboot recovery immediately after this stage.
  finish     After recovery reboot, fsck+grow userdata and format cache.

Hard stop:
  move/finish require M2NOTE_REPARTITION_ALLOW=M2NOTE_2G_DATA_MOVE_OK.
EOF
}

need() {
  command -v "$1" >/dev/null 2>&1 || die "missing required tool: $1"
}

read_sys() {
  cat "$1" 2>/dev/null || die "cannot read $1"
}

check_eq() {
  label="$1"
  actual="$2"
  expected="$3"
  [ "$actual" = "$expected" ] || die "$label mismatch: expected $expected, got $actual"
}

is_mounted() {
  grep -q " $1 " /proc/mounts
}

run_e2fsck() {
  part="$1"
  set +e
  e2fsck -fy "$part"
  rc="$?"
  set -e
  [ "$rc" = "0" ] || [ "$rc" = "1" ] || die "e2fsck $part failed with rc=$rc"
}

require_recovery_context() {
  boot_completed="$(getprop sys.boot_completed 2>/dev/null || true)"
  [ "$boot_completed" != "1" ] || die "Android userspace detected; boot TWRP/recovery first"
}

require_tools() {
  need cat
  need grep
  need sed
  need dd
  need e2fsck
  need resize2fs
  need sgdisk
  need sync
  need umount
}

check_old_geometry() {
  check_eq "p23 start" "$(read_sys /sys/class/block/mmcblk0p23/start)" "$OLD_SYSTEM_START"
  check_eq "p23 size" "$(read_sys /sys/class/block/mmcblk0p23/size)" "$OLD_SYSTEM_SIZE"
  check_eq "p24 start" "$(read_sys /sys/class/block/mmcblk0p24/start)" "$OLD_CACHE_START"
  check_eq "p24 size" "$(read_sys /sys/class/block/mmcblk0p24/size)" "$OLD_CACHE_SIZE"
  check_eq "p25 start" "$(read_sys /sys/class/block/mmcblk0p25/start)" "$OLD_DATA_START"
  check_eq "p25 size" "$(read_sys /sys/class/block/mmcblk0p25/size)" "$OLD_DATA_SIZE"
  check_eq "p26 start" "$(read_sys /sys/class/block/mmcblk0p26/start)" "$FLASHINFO_START"
  check_eq "p26 size" "$(read_sys /sys/class/block/mmcblk0p26/size)" "$FLASHINFO_SIZE"
}

check_new_geometry() {
  check_eq "p23 start" "$(read_sys /sys/class/block/mmcblk0p23/start)" "$NEW_SYSTEM_START"
  check_eq "p23 size" "$(read_sys /sys/class/block/mmcblk0p23/size)" "$((NEW_SYSTEM_END - NEW_SYSTEM_START + 1))"
  check_eq "p24 start" "$(read_sys /sys/class/block/mmcblk0p24/start)" "$NEW_CACHE_START"
  check_eq "p24 size" "$(read_sys /sys/class/block/mmcblk0p24/size)" "$((NEW_CACHE_END - NEW_CACHE_START + 1))"
  check_eq "p25 start" "$(read_sys /sys/class/block/mmcblk0p25/start)" "$NEW_DATA_START"
  check_eq "p25 size" "$(read_sys /sys/class/block/mmcblk0p25/size)" "$((NEW_DATA_END - NEW_DATA_START + 1))"
}

print_layout() {
  echo "target layout:"
  echo "  system   ${NEW_SYSTEM_START}..${NEW_SYSTEM_END}"
  echo "  cache    ${NEW_CACHE_START}..${NEW_CACHE_END}"
  echo "  userdata ${NEW_DATA_START}..${NEW_DATA_END}"
}

unmount_targets() {
  for m in /data /cache /system /sdcard; do
    if is_mounted "$m"; then
      umount "$m" || true
    fi
  done
  for m in /data /cache /system; do
    is_mounted "$m" && die "$m is still mounted"
  done
}

backup_gpt() {
  mkdir -p "$BACKUP_DIR"
  disk_sectors="$(read_sys /sys/class/block/mmcblk0/size)"
  last_skip="$((disk_sectors - 2048))"
  dd if="$DISK" of="$BACKUP_DIR/mmcblk0-first-2048.bin" bs=512 count=2048
  dd if="$DISK" of="$BACKUP_DIR/mmcblk0-last-2048.bin" bs=512 skip="$last_skip" count=2048
  sgdisk --backup "$BACKUP_DIR/gpt-before.bin" "$DISK"
  echo "GPT backups written to $BACKUP_DIR; pull these to the host before trusting the run."
}

userdata_min_blocks() {
  resize2fs -P "$DATA_PART" 2>&1 | sed -n 's/.*: \([0-9][0-9]*\)$/\1/p'
}

move_userdata_bytes() {
  new_data_sectors="$((NEW_DATA_END - NEW_DATA_START + 1))"
  new_data_blocks="$((new_data_sectors / SECTORS_PER_4K))"
  shrink_blocks="$((new_data_blocks - DATA_MARGIN_BLOCKS))"
  old_start_blocks="$((OLD_DATA_START / SECTORS_PER_4K))"
  new_start_blocks="$((NEW_DATA_START / SECTORS_PER_4K))"

  min_blocks="$(userdata_min_blocks)"
  [ -n "$min_blocks" ] || die "cannot parse resize2fs -P output for userdata"
  [ "$min_blocks" -lt "$shrink_blocks" ] || die "userdata minimum $min_blocks blocks does not fit shrink target $shrink_blocks"

  echo "userdata min blocks: $min_blocks"
  echo "shrinking userdata to $shrink_blocks filesystem blocks"
  resize2fs "$DATA_PART" "$shrink_blocks"
  run_e2fsck "$DATA_PART"

  remaining="$shrink_blocks"
  while [ "$remaining" -gt 0 ]; do
    chunk="$COPY_CHUNK_BLOCKS"
    [ "$remaining" -lt "$chunk" ] && chunk="$remaining"
    offset="$((remaining - chunk))"
    src="$((old_start_blocks + offset))"
    dst="$((new_start_blocks + offset))"
    echo "copy blocks offset=$offset count=$chunk src=$src dst=$dst"
    dd if="$DISK" of="$DISK" bs="$BLOCK_SIZE" skip="$src" seek="$dst" count="$chunk"
    remaining="$offset"
  done
  sync
}

rewrite_gpt() {
  sgdisk \
    --delete=25 \
    --delete=24 \
    --delete=23 \
    --new=23:${NEW_SYSTEM_START}:${NEW_SYSTEM_END} \
    --typecode=23:0700 \
    --change-name=23:system \
    --new=24:${NEW_CACHE_START}:${NEW_CACHE_END} \
    --typecode=24:0700 \
    --change-name=24:cache \
    --new=25:${NEW_DATA_START}:${NEW_DATA_END} \
    --typecode=25:0700 \
    --change-name=25:userdata \
    "$DISK"
  sync
}

format_cache() {
  if command -v mke2fs >/dev/null 2>&1; then
    mke2fs -t ext4 -F "$CACHE_PART"
  elif command -v make_ext4fs >/dev/null 2>&1; then
    make_ext4fs "$CACHE_PART"
  else
    die "no ext4 formatter found for cache"
  fi
}

require_allowed() {
  [ "$ALLOW" = "M2NOTE_2G_DATA_MOVE_OK" ] || die "set M2NOTE_REPARTITION_ALLOW=M2NOTE_2G_DATA_MOVE_OK to run $MODE"
}

case "$MODE" in
  preflight)
    require_recovery_context
    require_tools
    check_old_geometry
    print_layout
    echo "preflight OK; no writes performed"
    ;;
  move)
    require_allowed
    require_recovery_context
    require_tools
    check_old_geometry
    unmount_targets
    backup_gpt
    run_e2fsck "$DATA_PART"
    move_userdata_bytes
    rewrite_gpt
    echo "move stage complete. Reboot recovery now, then run finish mode."
    ;;
  finish)
    require_allowed
    require_recovery_context
    require_tools
    check_new_geometry
    unmount_targets
    run_e2fsck "$DATA_PART"
    resize2fs "$DATA_PART"
    run_e2fsck "$DATA_PART"
    format_cache
    sync
    echo "finish stage complete. Verify system/cache/data block sizes before flashing GSI."
    ;;
  -h|--help|help)
    usage
    ;;
  *)
    usage
    die "unknown mode: $MODE"
    ;;
esac
