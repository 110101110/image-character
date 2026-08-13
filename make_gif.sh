#!/usr/bin/env bash

set -euo pipefail

magick_bin=$(command -v magick 2>/dev/null || true)
for candidate in /opt/homebrew/bin/magick /usr/local/bin/magick /Users/zqt/homebrew/bin/magick; do
    if [[ -z "$magick_bin" && -x "$candidate" ]]; then
        magick_bin=$candidate
    fi
done

if [[ -z "$magick_bin" ]]; then
    echo "Error: ImageMagick is not installed."
    echo "Install it with: brew install imagemagick"
    exit 1
fi

if (( $# != 3 )); then
    echo "Usage: $0 OUTPUT.gif DELAY FRAME_DIRECTORY"
    echo "Example: $0 animation.gif 10 /tmp/ascii_gif_frames"
    echo "DELAY is measured in 1/100 second."
    exit 1
fi

output_file=$1
frame_delay=$2
frame_directory=$3

if [[ ! -d "$frame_directory" ]]; then
    echo "Error: frame directory does not exist: $frame_directory"
    exit 1
fi

shopt -s nullglob
input_files=("$frame_directory"/frame_*.png)
if (( ${#input_files[@]} == 0 )); then
    echo "Error: no frame_*.png files found in $frame_directory"
    exit 1
fi
if (( ${#input_files[@]} > 16 )); then
    echo "Error: at most 16 frames are supported"
    exit 1
fi

"$magick_bin" \
    -delay "$frame_delay" \
    -loop 0 \
    "${input_files[@]}" \
    -layers Optimize \
    "$output_file"

echo "Created: $output_file"
