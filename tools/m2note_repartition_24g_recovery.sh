#!/sbin/sh
#
# Recovery-side helper for the m2note Q/R GSI layout:
#   p23 system   2.4 GiB
#   p24 cache    128 MiB
#   p25 userdata moved right by 681,156,608 bytes, then grown to the new end
#
# This is intentionally not an Android-userspace script. Run it only from TWRP
# or an equivalent recovery, after a host-side userdata/GPT backup exists.

set -eu

MODE="${1:-preflight}"
ALLOW="${M2NOTE_REPARTITION_ALLOW:-}"
BACKUP_PULLED="${M2NOTE_REPARTITION_BACKUP_PULLED:-}"

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
NEW_SYSTEM_END=6507727
NEW_CACHE_START=6507728
NEW_CACHE_END=6769871
NEW_DATA_START=6769872
NEW_DATA_END=30501887

SECTORS_PER_4K=8
BLOCK_SIZE=4096
DATA_MARGIN_BLOCKS="${DATA_MARGIN_BLOCKS:-4096}"
COPY_CHUNK_BLOCKS="${COPY_CHUNK_BLOCKS:-32768}"
BACKUP_DIR="${BACKUP_DIR:-/tmp/m2note-repartition-24g}"
GPT_PATCH_DIR="${GPT_PATCH_DIR:-/tmp/m2note-gpt-patch-24g}"

GPT_PRIMARY_HEADER="gpt-primary-header-lba1.bin"
GPT_PRIMARY_ENTRIES="gpt-primary-entries-lba2-count7.bin"
GPT_BACKUP_ENTRIES="gpt-backup-entries-lba30534656-count7.bin"
GPT_BACKUP_HEADER="gpt-backup-header-lba30535679.bin"

GPT_PRIMARY_HEADER_SHA="209b8b883c9e6d47696e450e1d65597757e3755ddc0d2b0b6634c5a2e37f3c84"
GPT_ENTRIES_SHA="5f70ee90d6261577c24212c136bac555fc3748be9bc510e3d736bb3f70e26719"
GPT_BACKUP_HEADER_SHA="58ea68764db8374130a4825a9bf4abb962c0d152d3a99c6448b86d1d2f9a3039"

GPT_PRIMARY_HEADER_LBA=1
GPT_PRIMARY_ENTRIES_LBA=2
GPT_PRIMARY_ENTRIES_SECTORS=7
GPT_BACKUP_ENTRIES_LBA=30534656
GPT_BACKUP_ENTRIES_SECTORS=7
GPT_BACKUP_HEADER_LBA=30535679

die() {
  echo "ERROR: $*" >&2
  exit 1
}

usage() {
  cat <<'EOF'
Usage:
  sh m2note_repartition_24g_recovery.sh preflight
  sh m2note_repartition_24g_recovery.sh backup
  M2NOTE_REPARTITION_ALLOW=M2NOTE_24G_DATA_MOVE_OK \
  M2NOTE_REPARTITION_BACKUP_PULLED=YES \
    sh m2note_repartition_24g_recovery.sh move
  M2NOTE_REPARTITION_ALLOW=M2NOTE_24G_DATA_MOVE_OK \
    sh m2note_repartition_24g_recovery.sh finish

Modes:
  preflight  Verify recovery tools and current partition geometry; no writes.
  backup     Capture GPT/header backups into /tmp; pull them to host before move.
  move       Unmount, fsck+shrink userdata, copy userdata bytes forward,
             write precomputed GPT blobs for 2.4 GiB system + 128 MiB cache,
             then stop.
             Reboot recovery immediately after this stage.
  finish     After recovery reboot, fsck+grow userdata and format cache.

Hard stop:
  move requires M2NOTE_REPARTITION_ALLOW=M2NOTE_24G_DATA_MOVE_OK and
  M2NOTE_REPARTITION_BACKUP_PULLED=YES.
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
  need sha256sum
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
  check_eq "p26 start" "$(read_sys /sys/class/block/mmcblk0p26/start)" "$FLASHINFO_START"
  check_eq "p26 size" "$(read_sys /sys/class/block/mmcblk0p26/size)" "$FLASHINFO_SIZE"
}

print_layout() {
  echo "target layout:"
  echo "  system   ${NEW_SYSTEM_START}..${NEW_SYSTEM_END}"
  echo "  cache    ${NEW_CACHE_START}..${NEW_CACHE_END}"
  echo "  userdata ${NEW_DATA_START}..${NEW_DATA_END}"
  echo "  userdata shift sectors=$((NEW_DATA_START - OLD_DATA_START)) bytes=$(((NEW_DATA_START - OLD_DATA_START) * 512))"
}

unmount_targets() {
  pass=0
  while [ "$pass" -lt 8 ]; do
    grep '^/dev/block/mmcblk0p2[345] ' /proc/mounts | while read dev mnt rest; do
      echo "umount $mnt ($dev)"
      umount "$mnt" || true
    done
    grep -q '^/dev/block/mmcblk0p2[345] ' /proc/mounts || break
    pass="$((pass + 1))"
  done
  if grep -q '^/dev/block/mmcblk0p2[345] ' /proc/mounts; then
    grep '^/dev/block/mmcblk0p2[345] ' /proc/mounts
    die "system/cache/userdata block device is still mounted"
  fi
  return 0
}

backup_gpt() {
  mkdir -p "$BACKUP_DIR"
  disk_sectors="$(read_sys /sys/class/block/mmcblk0/size)"
  last_skip="$((disk_sectors - 2048))"
  dd if="$DISK" of="$BACKUP_DIR/mmcblk0-first-2048.bin" bs=512 count=2048
  dd if="$DISK" of="$BACKUP_DIR/mmcblk0-last-2048.bin" bs=512 skip="$last_skip" count=2048
  fdisk -l "$DISK" >"$BACKUP_DIR/fdisk-before.txt" 2>&1 || true
  sync
  echo "GPT backups written to $BACKUP_DIR; pull this directory to the host before move."
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
  echo "new userdata blocks: $new_data_blocks"
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

check_blob() {
  file="$1"
  expected_size="$2"
  expected_sha="$3"
  path="$GPT_PATCH_DIR/$file"
  [ -f "$path" ] || die "missing GPT patch blob: $path"
  actual_size="$(wc -c <"$path" | sed 's/ //g')"
  [ "$actual_size" = "$expected_size" ] || die "$file size mismatch: expected $expected_size got $actual_size"
  actual_sha="$(sha256sum "$path" | sed 's/ .*//')"
  [ "$actual_sha" = "$expected_sha" ] || die "$file sha mismatch: expected $expected_sha got $actual_sha"
}

require_gpt_patch_blobs() {
  check_blob "$GPT_PRIMARY_HEADER" 512 "$GPT_PRIMARY_HEADER_SHA"
  check_blob "$GPT_PRIMARY_ENTRIES" 3584 "$GPT_ENTRIES_SHA"
  check_blob "$GPT_BACKUP_ENTRIES" 3584 "$GPT_ENTRIES_SHA"
  check_blob "$GPT_BACKUP_HEADER" 512 "$GPT_BACKUP_HEADER_SHA"
}

rewrite_gpt() {
  require_gpt_patch_blobs
  dd if="$GPT_PATCH_DIR/$GPT_PRIMARY_ENTRIES" of="$DISK" bs=512 seek="$GPT_PRIMARY_ENTRIES_LBA" count="$GPT_PRIMARY_ENTRIES_SECTORS"
  dd if="$GPT_PATCH_DIR/$GPT_PRIMARY_HEADER" of="$DISK" bs=512 seek="$GPT_PRIMARY_HEADER_LBA" count=1
  dd if="$GPT_PATCH_DIR/$GPT_BACKUP_ENTRIES" of="$DISK" bs=512 seek="$GPT_BACKUP_ENTRIES_LBA" count="$GPT_BACKUP_ENTRIES_SECTORS"
  dd if="$GPT_PATCH_DIR/$GPT_BACKUP_HEADER" of="$DISK" bs=512 seek="$GPT_BACKUP_HEADER_LBA" count=1
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
  [ "$ALLOW" = "M2NOTE_24G_DATA_MOVE_OK" ] || die "set M2NOTE_REPARTITION_ALLOW=M2NOTE_24G_DATA_MOVE_OK to run $MODE"
}

require_backup_pulled() {
  [ -f "$BACKUP_DIR/mmcblk0-first-2048.bin" ] || die "run backup mode first"
  [ -f "$BACKUP_DIR/mmcblk0-last-2048.bin" ] || die "run backup mode first"
  [ "$BACKUP_PULLED" = "YES" ] || die "pull $BACKUP_DIR to host, then set M2NOTE_REPARTITION_BACKUP_PULLED=YES"
}

case "$MODE" in
  preflight)
    require_recovery_context
    require_tools
    require_gpt_patch_blobs
    check_old_geometry
    print_layout
    echo "preflight OK; no writes performed"
    ;;
  backup)
    require_recovery_context
    require_tools
    check_old_geometry
    backup_gpt
    ;;
  move)
    require_allowed
    require_backup_pulled
    require_recovery_context
    require_tools
    check_old_geometry
    unmount_targets
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
