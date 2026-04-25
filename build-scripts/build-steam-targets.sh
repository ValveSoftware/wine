#!/usr/bin/env bash

set -euo pipefail

usage() {
  cat <<'EOF'
Build only lsteamclient and steam_helper artifacts for CI.

Usage:
  bash build-scripts/build-steam-targets.sh [options]

Options:
  --arch <x86_64|aarch64|arm64ec>   Target architecture (default: x86_64)
  --enable-16kb-pages               Enable 16KB page size build mode
  --skip-autogen                    Do not run autogen.sh
  --skip-tools-build                Do not build wine-tools (step0)
  --skip-sysvshm                    Skip android_sysvshm build for aarch64
  --jobs <n>                        Parallel make jobs (default: nproc)
  --output-root <path>              Artifact root directory
  --help                            Show this help

Examples:
  bash build-scripts/build-steam-targets.sh --arch x86_64 --enable-16kb-pages
  bash build-scripts/build-steam-targets.sh --arch aarch64 --output-root "$GITHUB_WORKSPACE/artifacts"
EOF
}

ARCH="x86_64"
ENABLE_16KB_PAGES=0
RUN_AUTOGEN=1
RUN_TOOLS_BUILD=1
RUN_SYSVSHM=1
JOBS="$(nproc)"
OUTPUT_ROOT="${GITHUB_WORKSPACE:-$(pwd)}/steam-build-artifacts"

while [ "$#" -gt 0 ]; do
  case "$1" in
    --arch)
      ARCH="${2:-}"
      shift 2
      ;;
    --enable-16kb-pages)
      ENABLE_16KB_PAGES=1
      shift
      ;;
    --skip-autogen)
      RUN_AUTOGEN=0
      shift
      ;;
    --skip-tools-build)
      RUN_TOOLS_BUILD=0
      shift
      ;;
    --skip-sysvshm)
      RUN_SYSVSHM=0
      shift
      ;;
    --jobs)
      JOBS="${2:-}"
      shift 2
      ;;
    --output-root)
      OUTPUT_ROOT="${2:-}"
      shift 2
      ;;
    --help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage
      exit 1
      ;;
  esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT"

case "$ARCH" in
  x86_64)
    STEP_SCRIPT="build-scripts/build-step-x86_64.sh"
    ARTIFACT_ARCH="x86_64"
    ;;
  aarch64|arm64ec|arm64)
    STEP_SCRIPT="build-scripts/build-step-arm64ec.sh"
    ARTIFACT_ARCH="arm64ec"
    ;;
  *)
    echo "Unsupported arch: $ARCH" >&2
    usage
    exit 1
    ;;
esac

EXTRA_STEP_ARGS=()
if [ "$ENABLE_16KB_PAGES" -eq 1 ]; then
  EXTRA_STEP_ARGS+=("--enable-16kb-pages")
fi

echo "=== Steam targets build config ==="
echo "ARCH: $ARCH"
echo "STEP_SCRIPT: $STEP_SCRIPT"
echo "JOBS: $JOBS"
echo "OUTPUT_ROOT: $OUTPUT_ROOT"
echo "ENABLE_16KB_PAGES: $ENABLE_16KB_PAGES"
echo "RUN_AUTOGEN: $RUN_AUTOGEN"
echo "RUN_TOOLS_BUILD: $RUN_TOOLS_BUILD"
echo "RUN_SYSVSHM: $RUN_SYSVSHM"
echo "=================================="

if [ "$RUN_AUTOGEN" -eq 1 ]; then
  echo "Running autogen.sh..."
  bash autogen.sh
fi

if [ "$RUN_TOOLS_BUILD" -eq 1 ]; then
  echo "Building wine-tools..."
  bash build-scripts/build-step0.sh
fi

if [ "$ARCH" != "x86_64" ] && [ "$RUN_SYSVSHM" -eq 1 ]; then
  echo "Building android_sysvshm for $ARCH..."
  bash "$STEP_SCRIPT" "${EXTRA_STEP_ARGS[@]}" --build-sysvshm
fi

echo "Configuring build..."
bash "$STEP_SCRIPT" "${EXTRA_STEP_ARGS[@]}" --configure

echo "Building lsteamclient and steam_helper only..."
if ! make -j"$JOBS" lsteamclient steam_helper; then
  echo "Top-level targets failed, retrying subdirectory builds..."
  make -j"$JOBS" -C lsteamclient
  make -j"$JOBS" -C steam_helper
fi

ARTIFACT_DIR="$OUTPUT_ROOT/$ARTIFACT_ARCH"
mkdir -p "$ARTIFACT_DIR"

copy_if_exists() {
  local path="$1"
  if [ -f "$path" ]; then
    cp "$path" "$ARTIFACT_DIR/"
    echo "Collected artifact: $path"
    return 0
  fi
  return 1
}

LSTEAMCLIENT_OK=0
STEAM_HELPER_OK=0

copy_if_exists "lsteamclient/lsteamclient.dll" && LSTEAMCLIENT_OK=1 || true
copy_if_exists "lsteamclient/lsteamclient.so" && LSTEAMCLIENT_OK=1 || true
copy_if_exists "lsteamclient/lsteamclient.dll.so" && LSTEAMCLIENT_OK=1 || true

copy_if_exists "steam_helper/steam.exe" && STEAM_HELPER_OK=1 || true
copy_if_exists "steam_helper/steam.exe.so" && STEAM_HELPER_OK=1 || true

if [ "$LSTEAMCLIENT_OK" -ne 1 ]; then
  echo "Error: no lsteamclient artifact was produced." >&2
  exit 1
fi

if [ "$STEAM_HELPER_OK" -ne 1 ]; then
  echo "Error: no steam_helper artifact was produced." >&2
  exit 1
fi

echo "Artifacts saved in: $ARTIFACT_DIR"
ls -lah "$ARTIFACT_DIR"
