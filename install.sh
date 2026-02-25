#!/bin/bash

# 1. Setup Global Variables
REPO="GalaxyHaze/Kalidous"
VERSION=""
OUTPUT_NAME="kalidous"

# 2. Determine Version
if [ -n "$1" ]; then
    VERSION="$1"
    echo "Installing requested version: $VERSION"
else
    API_URL="https://api.github.com/repos/$REPO/releases/latest"
    # Fetch latest tag
    VERSION=$(curl -s "$API_URL" | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/')
    echo "No version specified. Installing latest version: $VERSION"
fi

if [ -z "$VERSION" ]; then
    echo "Error: Could not determine version."
    exit 1
fi

# 3. Detect OS and Architecture
OS="$(uname -s)"
ARCH="$(uname -m)"

# Normalize Architecture names
case "$ARCH" in
    x86_64)  ARCH="amd64" ;;
    aarch64|arm64) ARCH="arm64" ;;
    i386)    ARCH="386" ;;
    # Add other specific arch mappings if needed
    *)       echo "Unsupported architecture: $ARCH"; exit 1 ;;
esac

FILE_NAME=""

case "$OS" in
  Linux*)  FILE_NAME="kalidous-linux-$ARCH" ;;
  Darwin*) FILE_NAME="kalidous-macos-$ARCH" ;;
  # Covers Git Bash, MinGW, and MSYS on Windows
  MINGW*|MSYS*|CYGWIN*)
      FILE_NAME="kalidous-windows-$ARCH.exe"
      OUTPUT_NAME="kalidous.exe"
      ;;
  *)      echo "OS not supported: $OS"; exit 1 ;;
esac

# Safety Check
if [ -z "$FILE_NAME" ]; then
    echo "Error: Could not find a compatible binary for $OS $ARCH."
    exit 1
fi

DOWNLOAD_URL="https://github.com/$REPO/releases/download/$VERSION/$FILE_NAME"

echo "Downloading from $DOWNLOAD_URL..."

# 4. Download the binary
# -L follows redirects, -s is silent, -S shows error on fail, -o specifies output
if ! curl -fsSL "$DOWNLOAD_URL" -o "$OUTPUT_NAME"; then
    echo "Error: Failed to download binary."
    echo "Please check the URL: $DOWNLOAD_URL"
    exit 1
fi

# 5. Install
if [[ "$OS" == MINGW* ]] || [[ "$OS" == MSYS* ]] || [[ "$OS" == CYGWIN* ]]; then
    # Windows (Git Bash)
    chmod +x "$OUTPUT_NAME"
    echo "Download complete. Please move '$OUTPUT_NAME' to a folder in your PATH."
else
    # Linux / macOS
    chmod +x "$OUTPUT_NAME"
    echo "Installing Kalidous to /usr/local/bin/..."

    # Using sudo to move to system path
    if sudo mv "$OUTPUT_NAME" /usr/local/bin/kalidous; then
        echo "Installation complete! Run 'kalidous --help' to get started."
    else
        echo "Installation failed. Please check your sudo permissions."
        exit 1
    fi
fi