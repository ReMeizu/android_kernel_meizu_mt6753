#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-}"
SERIAL="${M2NOTE_SERIAL:-810BBMM22D7S}"
BUILD_STATION="${BUILD_STATION:-/home/n8n/build-station}"
HELPER="$BUILD_STATION/.codex/skills/build-station-api/scripts/forge_api.py"
OUT_DIR="${OUT_DIR:-$BUILD_STATION/docs/run_reports}"
STAMP="${STAMP:-$(date -u +%Y-%m-%d_%H%M%S)}"

V35_IMAGE="/srv/forge/android/export/m2note_flash_captures/artifacts/m2note-boot-v35-fresh-marker-panic.img"
V35_SHA="8a1ddccce9b409544d035025de0c6ef316e12595b49a2072688b1d961874207d"
V36_IMAGE="/srv/forge/android/export/m2note_flash_captures/artifacts/m2note-boot-v36-sdio-ocr-trace.img"
V36_SHA="3390732d7c73db8e387423325c041ca00f6eb8980dbb7bc6c8df75111e5d55fb"
V37_IMAGE="/srv/forge/android/export/m2note_flash_captures/artifacts/m2note-boot-v37-consys6735-noautok.img"
V37_SHA="5e104109187d4dec83110704c08795adbfa935a571d02cd1fcd7395093ed8835"
V38_IMAGE="/srv/forge/android/export/m2note_flash_captures/artifacts/m2note-boot-v38-panic-timeout.img"
V38_SHA="d519a87cde4d883a7f1c41df217171159f704328d8c30c2e5fd1cc095aa42ee6"
V39_IMAGE="/srv/forge/android/export/m2note_flash_captures/artifacts/m2note-boot-v39-soidle-guard.img"
V39_SHA="0847ffcbe5af1a99c0bf8c81e846dbcd70530e1a8045f56bb2a5a3de46719f32"
V40_IMAGE="/srv/forge/android/export/m2note_flash_captures/artifacts/m2note-boot-v40-safe-recovery-reset.img"
V40_SHA="0fc04fd31367a6791d77d1f9dbdaeed5b986e08b3ecdea7f0749e9f26537c8ee"
V41_IMAGE="/srv/forge/android/export/m2note_flash_captures/artifacts/m2note-boot-v41-longer-boot-window.img"
V41_SHA="384480181d62c254c9d10e16f6f589ab2baa12e41cd80223cdee1fd3ac502185"
V42_IMAGE="/srv/forge/android/export/m2note_flash_captures/artifacts/m2note-boot-v42-adbd-hwc-isolation.img"
V42_SHA="7f640afc195b5b93e27f999ceb7cffbc1d52d5aae796e3964bd94e2b9b8e8fb1"
V43_IMAGE="/srv/forge/android/export/m2note_flash_captures/artifacts/m2note-boot-v43-adbd-keep-hwc-defaults.img"
V43_SHA="c0c79ffcb61e348e0430749217c456847a21d26d98a5104d146d1fef75974524"
V50_IMAGE="/srv/forge/android/export/m2note_flash_captures/artifacts/m2note-boot-v50-audio-soc-card.img"
V50_SHA="10459aff8527455d4734f9b43e83c15d12ace49ef2817eca0e6b93c72cfe9e77"
V51_IMAGE="/srv/forge/android/export/m2note_flash_captures/artifacts/m2note-boot-v51-asoc-trace.img"
V51_SHA="29c60d7d5f2ca11721c5bfd36096088ee5b8418857495a183a20243753e1be66"
V52_IMAGE="/srv/forge/android/export/m2note_flash_captures/artifacts/m2note-boot-v52-arizona-power-spi.img"
V52_SHA="781ef9b51c29d2404c3c4bc2b1114b313b91fe8cd4af5dc6ecb2e122857221e6"
V53_IMAGE="/srv/forge/android/export/m2note_flash_captures/artifacts/m2note-boot-v53-spigpio-dct-pins.img"
V53_SHA="ec9df5ef919cd7380229c2760d48f03c49502f24d37f47225ded766510b77a25"
V54_IMAGE="/srv/forge/android/export/m2note_flash_captures/artifacts/m2note-boot-v54-spigpio-gpio-mode.img"
V54_SHA="bdf16aee10a9a12eee3dd932f1c56fb7eb5c497b0d018c303f357ddd54cb58ef"
V55_IMAGE="/srv/forge/android/export/m2note_flash_captures/artifacts/m2note-boot-v55-arizona-ldoena-active-low.img"
V55_SHA="e9c026c6f5464f22d816f36543c31c2bb72ee72e42bf6f256542fac6cdcf3ec9"
V56_IMAGE="/srv/forge/android/export/m2note_flash_captures/artifacts/m2note-boot-v56-mt6630-spi-dummy-devmap.img"
V56_SHA="35a22136fdf4404a64d040d65fae5060e7bfd3e58cbc893f91a4c8abeeef9ea2"
V57_IMAGE="/srv/forge/android/export/m2note_flash_captures/artifacts/m2note-boot-v57-bootdevice-byname-nvram.img"
V57_SHA="05af7eb83d705e784fa5a72456c07b0d1e8a0cc7d948c5f33dd587d235d12c4a"
V58_IMAGE="/srv/forge/android/export/m2note_flash_captures/artifacts/m2note-boot-v58-profile-cur0-bootcomplete.img"
V58_SHA="5404923b87583834f8154cee0e400b49ecef6700f440850d0c86c18e31a2bd03"
V59_IMAGE="/srv/forge/android/export/m2note_flash_captures/artifacts/m2note-boot-v59-display-rising-edge.img"
V59_SHA="a45e7bcc627dcd3b60bb014cd0069959ff80fc340dbb3f50d2d01c76da38e925"
V60_IMAGE="/srv/forge/android/export/m2note_flash_captures/artifacts/m2note-boot-v60-late-nvram-alias.img"
V60_SHA="d0d22159cc1cce9dc0654130531f94d665ef47251559c310d9c89b80c9c2a248"
V61_IMAGE="/srv/forge/android/export/m2note_flash_captures/artifacts/m2note-boot-v61-direct-nvram-alias.img"
V61_SHA="4f57cea5c8876e70b492d406127aa56ffd7ab28c7a363ede826c5e9f11ceace8"
V62_IMAGE="/srv/forge/android/export/m2note_flash_captures/artifacts/m2note-boot-v62-audio-isolate-florida.img"
V62_SHA="ddf42fbd1c92a785230cbdcce00c1901ae1f21af66fe2947f3b12c3399a8737f"
V63_IMAGE="/srv/forge/android/export/m2note_flash_captures/artifacts/m2note-boot-v63-camera-sub-bus-s5k3l2xx.img"
V63_SHA="1e388aa0009314961c8081dc6501d45def5969e43e75046dc3fefd5d86dcaf0f"
V64_IMAGE="/srv/forge/android/export/m2note_flash_captures/artifacts/m2note-boot-v64-camera-stock-power.img"
V64_SHA="d1720c44d2e423a84e4176d4727ff3eb3e0c7a142e146b798cf8411900095f61"
V65_IMAGE="/srv/forge/android/export/m2note_flash_captures/artifacts/m2note-boot-v65-sdio2-pin-mode.img"
V65_SHA="db2e0416c17a17a6e9017c7d974708f884d458eb303f8e87a57c852205e2f788"
V34_IMAGE="/srv/forge/android/export/m2note_flash_captures/artifacts/m2note-boot-v34-aee-sram-panic-hardtimeout.img"
V34_SHA="573f9daa0ef74ad4e09c2c5a5694591d435c8ba7ae64098cef57b096ded4943e"
V32_IMAGE="/srv/forge/android/export/m2note_flash_captures/artifacts/m2note-boot-v32-autorecovery-hardtimer.img"
V32_SHA="fc776273a93abc5d8d06fbaf19dff3e52eb5372d813b572317a3d06d38853c90"

usage() {
  cat <<'EOF'
Usage: tools/m2note_flash_cycle.sh <status|v65-test|v64-test|v63-test|v62-test|v61-test|v60-test|v59-test|v58-test|v57-test|v56-test|v55-test|v54-test|v53-test|v52-test|v51-test|v50-test|v43-test|v42-test|v41-test|v40-test|v39-test|v38-test|v37-test|v36-test|v35-test|v34-test|rollback-v32>

Environment:
  FORGE_API_PASSWORD     API password, defaults to admin for local station.
  M2NOTE_SERIAL          target serial, defaults to 810BBMM22D7S.
  OUT_DIR                run report directory, defaults to Build Station docs/run_reports.
  WAIT_TIMEOUT           pre-flash recovery wait seconds, defaults to 900.
  POST_REBOOT_TIMEOUT    post-reboot device/recovery wait seconds, defaults to 420.
  INTERVAL               poll interval seconds, defaults to 10.

The flash modes call Build Station local-agent only. They wait for the exact
target serial in recovery before any clear/flash/reboot step and write evidence
sidecars next to the JSON report.
EOF
}

require_helper() {
  if [[ ! -f "$HELPER" ]]; then
    echo "missing Build Station helper: $HELPER" >&2
    exit 2
  fi
}

check_image() {
  local image="$1"
  local expected="$2"
  local actual
  if [[ ! -f "$image" ]]; then
    echo "missing boot image: $image" >&2
    exit 2
  fi
  actual="$(sha256sum "$image" | awk '{print $1}')"
  if [[ "$actual" != "$expected" ]]; then
    echo "sha256 mismatch for $image: expected=$expected actual=$actual" >&2
    exit 2
  fi
}

run_helper() {
  cd "$BUILD_STATION"
  FORGE_API_PASSWORD="${FORGE_API_PASSWORD:-admin}" python3 "$HELPER" "$@"
}

flash_cycle() {
  local label="$1"
  local image="$2"
  local sha="$3"
  local out="$OUT_DIR/${STAMP}_m2note_${label}_flash_cycle.json"
  check_image "$image" "$sha"
  run_helper local-agent flash-boot-cycle \
    --serial "$SERIAL" \
    --expected-device m2note \
    --image-path "$image" \
    --expected-sha256 "$sha" \
    --wait-timeout "${WAIT_TIMEOUT:-900}" \
    --post-reboot-timeout "${POST_REBOOT_TIMEOUT:-420}" \
    --interval "${INTERVAL:-10}" \
    --collect-after-reboot \
    --runtime-evidence-seconds "${RUNTIME_EVIDENCE_SECONDS:-120}" \
    --out "$out"
}

require_helper

case "$MODE" in
  status)
    run_helper local-agent adb-devices
    ;;
  v37-test)
    flash_cycle "v37" "$V37_IMAGE" "$V37_SHA"
    ;;
  v38-test)
    flash_cycle "v38" "$V38_IMAGE" "$V38_SHA"
    ;;
  v39-test)
    flash_cycle "v39" "$V39_IMAGE" "$V39_SHA"
    ;;
  v40-test)
    flash_cycle "v40" "$V40_IMAGE" "$V40_SHA"
    ;;
  v41-test)
    flash_cycle "v41" "$V41_IMAGE" "$V41_SHA"
    ;;
  v42-test)
    flash_cycle "v42" "$V42_IMAGE" "$V42_SHA"
    ;;
  v43-test)
    flash_cycle "v43" "$V43_IMAGE" "$V43_SHA"
    ;;
  v50-test)
    flash_cycle "v50" "$V50_IMAGE" "$V50_SHA"
    ;;
  v51-test)
    flash_cycle "v51" "$V51_IMAGE" "$V51_SHA"
    ;;
  v52-test)
    flash_cycle "v52" "$V52_IMAGE" "$V52_SHA"
    ;;
  v53-test)
    flash_cycle "v53" "$V53_IMAGE" "$V53_SHA"
    ;;
  v54-test)
    flash_cycle "v54" "$V54_IMAGE" "$V54_SHA"
    ;;
  v55-test)
    flash_cycle "v55" "$V55_IMAGE" "$V55_SHA"
    ;;
  v56-test)
    flash_cycle "v56" "$V56_IMAGE" "$V56_SHA"
    ;;
  v57-test)
    flash_cycle "v57" "$V57_IMAGE" "$V57_SHA"
    ;;
  v58-test)
    flash_cycle "v58" "$V58_IMAGE" "$V58_SHA"
    ;;
  v59-test)
    flash_cycle "v59" "$V59_IMAGE" "$V59_SHA"
    ;;
  v60-test)
    flash_cycle "v60" "$V60_IMAGE" "$V60_SHA"
    ;;
  v61-test)
    flash_cycle "v61" "$V61_IMAGE" "$V61_SHA"
    ;;
  v62-test)
    flash_cycle "v62" "$V62_IMAGE" "$V62_SHA"
    ;;
  v63-test)
    flash_cycle "v63" "$V63_IMAGE" "$V63_SHA"
    ;;
  v64-test)
    flash_cycle "v64" "$V64_IMAGE" "$V64_SHA"
    ;;
  v65-test)
    flash_cycle "v65" "$V65_IMAGE" "$V65_SHA"
    ;;
  v35-test)
    flash_cycle "v35" "$V35_IMAGE" "$V35_SHA"
    ;;
  v36-test)
    flash_cycle "v36" "$V36_IMAGE" "$V36_SHA"
    ;;
  v34-test)
    flash_cycle "v34" "$V34_IMAGE" "$V34_SHA"
    ;;
  rollback-v32)
    flash_cycle "rollback_v32" "$V32_IMAGE" "$V32_SHA"
    ;;
  ""|-h|--help|help)
    usage
    ;;
  *)
    usage >&2
    exit 2
    ;;
esac
