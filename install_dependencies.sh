#!/usr/bin/env bash

set -euo pipefail

readonly expected_architecture="arm64"
readonly homebrew_prefix="/opt/homebrew"
readonly brew_binary="$homebrew_prefix/bin/brew"

echo "ASCII Signature dependency installer"
echo

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "Error: this installer supports macOS only."
    exit 1
fi

if [[ "$(uname -m)" != "$expected_architecture" ]]; then
    echo "Error: this installer requires Apple Silicon arm64."
    echo "Detected architecture: $(uname -m)"
    exit 1
fi

if ! xcode-select -p >/dev/null 2>&1; then
    echo "Apple Command Line Tools are not installed."
    echo "Opening the macOS installer..."
    xcode-select --install || true
    echo
    echo "Finish the Command Line Tools installation, then run:"
    echo "  make install-deps"
    exit 0
fi

if [[ ! -x "$brew_binary" ]]; then
    echo "Homebrew was not found at $homebrew_prefix."
    echo "Installing Homebrew..."
    /bin/bash -c \
        "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
fi

if [[ ! -x "$brew_binary" ]]; then
    echo "Error: Homebrew installation did not create $brew_binary"
    exit 1
fi

echo "Updating Homebrew metadata..."
"$brew_binary" update

echo "Installing GLFW and ImageMagick..."
"$brew_binary" install glfw imagemagick

if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo "Initializing Git submodules..."
    git submodule update --init --recursive
fi

required_paths=(
    "$homebrew_prefix/opt/glfw/include/GLFW/glfw3.h"
    "$homebrew_prefix/opt/glfw/lib/libglfw.3.dylib"
    "$homebrew_prefix/bin/magick"
)

for required_path in "${required_paths[@]}"; do
    if [[ ! -e "$required_path" ]]; then
        echo "Error: dependency installation is incomplete:"
        echo "  $required_path"
        exit 1
    fi
done

echo
echo "Installed dependencies:"
"$brew_binary" list --versions glfw imagemagick
echo
echo "Dependencies are ready. Build and run with:"
echo "  make"
echo "  ./image_character"
