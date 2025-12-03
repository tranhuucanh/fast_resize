#!/bin/bash
# Script to create test images by copying base image
# Usage: ./create_test_images.sh [number_of_copies]

BASE_IMAGE="images/input/base.png"
OUTPUT_DIR="images/input"
NUM_COPIES=${1:-1000}  # Default 1000 copies

# Check if base image exists
if [ ! -f "$BASE_IMAGE" ]; then
    echo "ERROR: Base image not found: $BASE_IMAGE"
    echo "Please place a base image at $BASE_IMAGE first"
    exit 1
fi

# Create output directory if it doesn't exist
mkdir -p "$OUTPUT_DIR"

# Remove old test images
echo "Removing old test images..."
rm -f "$OUTPUT_DIR"/img_*.jpg

echo "Creating $NUM_COPIES copies of $BASE_IMAGE..."
echo "Output directory: $OUTPUT_DIR"
echo

# Get file info
FILE_SIZE=$(ls -lh "$BASE_IMAGE" | awk '{print $5}')
echo "Base image size: $FILE_SIZE"
echo

# Create copies with progress indicator
for i in $(seq 1 $NUM_COPIES); do
    # Format number with leading zeros (e.g., 0001, 0002, ...)
    NUM=$(printf "%04d" $i)

    # Copy file
    cp "$BASE_IMAGE" "$OUTPUT_DIR/img_$NUM.png"

    # Progress indicator every 100 images
    if [ $((i % 100)) -eq 0 ]; then
        echo -n "."
    fi
done

echo
echo
echo "✓ Created $NUM_COPIES test images"
echo "  Files: img_0001.jpg to img_$(printf "%04d" $NUM_COPIES).jpg"
echo "  Location: $OUTPUT_DIR"

# Calculate total size
TOTAL_SIZE=$(du -sh "$OUTPUT_DIR" | awk '{print $1}')
echo "  Total size: $TOTAL_SIZE"
echo
echo "Ready to run benchmark!"
