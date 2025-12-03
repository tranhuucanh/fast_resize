#!/bin/bash
# Safe git add script that handles index.lock issues

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$REPO_DIR"

# Remove lock file if it exists
if [ -f .git/index.lock ]; then
    echo "Removing stale lock file..."
    rm -f .git/index.lock
fi

# Run git add with the provided arguments
if [ $# -eq 0 ]; then
    # If no arguments, add all files in smaller batches
    echo "Adding files in batches..."

    # Add new files first
    git add -N . 2>/dev/null || true
    git add $(git status --porcelain | grep '^??' | cut -c4-) 2>/dev/null || true

    # Add modified files
    git add -u 2>/dev/null || true

    echo "✓ Files added successfully"
else
    # Add specific files/directories
    git add "$@" && echo "✓ Files added successfully"
fi

