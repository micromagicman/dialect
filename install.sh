#!/bin/sh
# Установщик Dialect.
#
#   curl -fsSL https://raw.githubusercontent.com/micromagicman/dialect/main/install.sh | sh
#
# Режимы:
#   - по умолчанию (latest) или DIALECT_VERSION=vX.Y.Z — скачивает готовый
#     нативный бинарь из GitHub Releases;
#   - DIALECT_VERSION=develop — собирает из исходников ветки develop
#     (нужны git + cc/gcc).
#
# Куда ставит: ~/.local/bin (или $DIALECT_INSTALL_DIR).

set -eu

REPO="micromagicman/dialect"
VERSION="${DIALECT_VERSION:-latest}"
INSTALL_DIR="${DIALECT_INSTALL_DIR:-$HOME/.local/bin}"

mkdir -p "$INSTALL_DIR"

# --- режим сборки из исходников (develop) ---
if [ "$VERSION" = "develop" ] || [ "$VERSION" = "source" ]; then
    echo "dialect: собираю из исходников (develop)…"
    for tool in git make cc; do
        command -v "$tool" >/dev/null 2>&1 || { echo "dialect: не найден $tool — установи его" >&2; exit 1; }
    done
    TMPDIR="$(mktemp -d)"
    trap 'rm -rf "$TMPDIR"' EXIT
    git clone --depth 1 --branch develop "https://github.com/$REPO.git" "$TMPDIR/src"
    make -C "$TMPDIR/src"
    cp "$TMPDIR/src/dialect" "$INSTALL_DIR/dialect"
    echo "dialect: установлен (develop) в $INSTALL_DIR/dialect"
    exit 0
fi

# --- режим готового бинаря из Releases ---
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

URL="https://github.com/$REPO/releases/${VERSION}/download/${ASSET}"
echo "dialect: скачиваю $URL"
TMP="$(mktemp)"
curl -fL --progress-bar -o "$TMP" "$URL"
chmod +x "$TMP"
mv "$TMP" "$INSTALL_DIR/dialect"

echo "dialect: установлен в $INSTALL_DIR/dialect"
case ":$PATH:" in
    *":$INSTALL_DIR:"*) ;;
    *) echo "dialect: добавь $INSTALL_DIR в PATH:"; echo "  export PATH=\"\$PATH:$INSTALL_DIR\"" ;;
esac
