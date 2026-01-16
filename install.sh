#!/bin/bash

# 1. Determine Latest Version Tag
REPO="GalaxyHaze/Nova"
API_URL="https://api.github.com/repos/$REPO/releases/latest"

# Get tag name from JSON response
VERSION=$(curl -s $API_URL | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/')

if [ -z "$VERSION" ]; then
    echo "Error: Could not fetch the latest version from GitHub."
    exit 1
fi

echo "Installing Nova version: $VERSION"

# 2. Detect OS and Architecture
OS="$(uname -s)"
ARCH="$(uname -m)"

FILE_NAME=""

case "$OS" in
  Linux*)  FILE_NAME="nova-linux-amd64" ;;
  Darwin*) FILE_NAME="nova-macos-amd64" ;;
  CYGWIN*|MINGW*|MSYS*) FILE_NAME="nova-windows-amd64.exe" ;;
  *)      echo "OS not supported"; exit 1 ;;
esac

DOWNLOAD_URL="https://github.com/$REPO/releases/download/$VERSION/$FILE_NAME"

# 3. Download the binary
OUTPUT_NAME="Nova"
if [[ "$OS" == CYGWIN* ]] || [[ "$OS" == MINGW* ]] || [[ "$OS" == MSYS* ]]; then
    OUTPUT_NAME="Nova.exe"
fi

echo "Downloading from $DOWNLOAD_URL..."
curl -L -o "$OUTPUT_NAME" "$DOWNLOAD_URL"

# 4. Install to /usr/local/bin (requires sudo)
if [ "$OS" = "Darwin" ] || [ "$OS" = "Linux" ]; then
    chmod +x "$OUTPUT_NAME"
    sudo mv "$OUTPUT_NAME" /usr/local/bin/Nova
    echo "Installation complete! Run 'Nova --help' to get started."
else
    echo "Download complete. Please move '$OUTPUT_NAME' to a folder in your PATH."
fi