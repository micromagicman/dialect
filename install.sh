#!/bin/sh
# Установщик Dialect: скачивает нативный бинарь из GitHub Releases
# и кладёт его в ~/.local/bin (или в $DIALECT_INSTALL_DIR).
#
#   curl -fsSL https://raw.githubusercontent.com/micromagicman/dialect/main/install.sh | sh
#
# Версия задаётся через DIALECT_VERSION (например v0.1.0); по умолчанию — latest.

set -eu

REPO="micromagicman/dialect"
VERSION="${DIALECT_VERSION:-latest}"
INSTALL_DIR="${DIALECT_INSTALL_DIR:-$HOME/.local/bin}"

# --- определить платформу и целевой ассет ---
OS="$(uname -s)"
ARCH="$(uname -m)"

case "$ARCH" in
    x86_64|amd64) ARCH="x86_64" ;;
    arm64|aarch64) ARCH="arm64" ;;
    *) echo "dialect: неподдерживаемая архитектура: $ARCH" >&2; exit 1 ;;
esac

case "$OS" in
    Linux)  ASSET="dialect-linux-x86_64" ;;
    Darwin)
        case "$ARCH" in
            arm64)  ASSET="dialect-macos-arm64" ;;
            x86_64) ASSET="dialect-macos-x86_64" ;;
        esac
        ;;
    *) echo "dialect: неподдерживаемая ОС: $OS" >&2; exit 1 ;;
esac

# --- скачать ---
URL="https://github.com/$REPO/releases/${VERSION}/download/${ASSET}"
echo "dialect: скачиваю $URL"
TMP="$(mktemp)"
curl -fL --progress-bar -o "$TMP" "$URL"
chmod +x "$TMP"

# --- установить ---
mkdir -p "$INSTALL_DIR"
mv "$TMP" "$INSTALL_DIR/dialect"

echo "dialect: установлен в $INSTALL_DIR/dialect"
case ":$PATH:" in
    *":$INSTALL_DIR:"*) ;;
    *) echo "dialect: добавь $INSTALL_DIR в PATH:"; echo "  export PATH=\"\$PATH:$INSTALL_DIR\"" ;;
esac
