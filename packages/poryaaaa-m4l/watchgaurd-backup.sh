#!/usr/bin/env bash
  # wg-backup.sh — pull a config backup from a WatchGuard Firebox via SSH
  set -euo pipefail

  FIREBOX_HOST="172.16.1.5"
  FIREBOX_USER="${FIREBOX_USER:-admin}"      # use a dedicated backup admin if possible
  SSH_PORT="${SSH_PORT:-4118}"
  BACKUP_DIR="${BACKUP_DIR:-$HOME/wg-backups}"
  STAMP="$(date +%Y%m%d-%H%M%S)"

  mkdir -p "$BACKUP_DIR"

  # 1. SSH in and trigger the export to the Firebox's local /pending/ store
  #    (exact command verified against your Fireware version — see note below)
  ssh -p "$SSH_PORT" "$FIREBOX_USER@$FIREBOX_HOST" \
      "export configuration backup-${STAMP}.xml"   # << TO VERIFY for your version

  # 2. SCP the file back
  scp -P "$SSH_PORT" \
      "$FIREBOX_USER@$FIREBOX_HOST:/pending/backup-${STAMP}.xml" \
      "$BACKUP_DIR/backup-${STAMP}.xml"

  # 3. Optional: clean up on the Firebox
  ssh -p "$SSH_PORT" "$FIREBOX_USER@$FIREBOX_HOST" \
      "delete /pending/backup-${STAMP}.xml" || true

  echo "Saved $BACKUP_DIR/backup-${STAMP}.xml"