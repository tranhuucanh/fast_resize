#!/bin/bash

# FastResize - Release Orchestration Script
# Creates a new release with version bumping

set -e

if [ -z "$1" ]; then
    echo "Usage: $0 <version>"
    echo "Example: $0 1.0.1"
    exit 1
fi

VERSION="$1"

echo "🚀 FastResize Release Script"
echo "============================="
echo "Version: $VERSION"
echo ""

# 1. Verify clean working tree
if [ -n "$(git status --porcelain)" ]; then
    echo "❌ ERROR: Working directory not clean"
    echo "Please commit or stash your changes first"
    git status --short
    exit 1
fi

# 2. Update version in all files
echo "📝 Updating version to $VERSION..."
./scripts/update-version.sh "$VERSION"

# 3. Commit version changes
echo "💾 Committing version changes..."
git add -A
git commit -m "chore: bump version to $VERSION"

# 4. Create annotated tag
echo "🏷️  Creating git tag v$VERSION..."
git tag -a "v$VERSION" -m "Release v$VERSION"

# 5. Show what will be pushed
echo ""
echo "📋 Ready to push:"
echo "  - Commit: $(git log -1 --oneline)"
echo "  - Tag: v$VERSION"
echo ""

# 6. Ask for confirmation
read -p "Push to origin and trigger release? (y/N) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "🚀 Pushing to origin..."
    git push origin master
    git push origin "v$VERSION"

    echo ""
    echo "✅ Release v$VERSION initiated!"
    echo ""
    echo "📊 GitHub Actions will now:"
    echo "  1. Build binaries for all platforms"
    echo "  2. Create GitHub Release"
    echo "  3. Update Homebrew tap"
    echo "  4. Publish to RubyGems"
    echo ""
    echo "🔗 Monitor progress at:"
    echo "   https://github.com/tranhuucanh/fast_resize/actions"
else
    echo "❌ Release cancelled. To undo version changes:"
    echo "   git reset HEAD~1"
    echo "   git tag -d v$VERSION"
fi
