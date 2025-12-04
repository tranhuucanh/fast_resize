#!/bin/bash
# Update version across all project files for FastResize

set -e

if [ -z "$1" ]; then
    echo "Usage: $0 <version>"
    echo "Example: $0 1.0.1"
    exit 1
fi

NEW_VERSION="$1"

echo "🔄 Updating version to $NEW_VERSION..."

# Update VERSION file
echo "$NEW_VERSION" > VERSION

# Update CMakeLists.txt
sed -i.bak "s/project(fast_resize VERSION [0-9.][0-9.]*/project(fast_resize VERSION $NEW_VERSION/" CMakeLists.txt
rm -f CMakeLists.txt.bak

# Update src/fastresize.cpp
sed -i.bak "s/#define FASTRESIZE_VERSION \"[^\"]*\"/#define FASTRESIZE_VERSION \"$NEW_VERSION\"/" src/fastresize.cpp
rm -f src/fastresize.cpp.bak

# Update include/fastresize.h
if grep -q "FASTRESIZE_VERSION" include/fastresize.h; then
    sed -i.bak "s/#define FASTRESIZE_VERSION \"[^\"]*\"/#define FASTRESIZE_VERSION \"$NEW_VERSION\"/" include/fastresize.h
    rm -f include/fastresize.h.bak
fi

# Update Ruby version file (create if doesn't exist)
if [ ! -f "bindings/ruby/lib/fastresize/version.rb" ]; then
    mkdir -p bindings/ruby/lib/fastresize
    cat > bindings/ruby/lib/fastresize/version.rb <<EOF
# frozen_string_literal: true

module FastResize
  VERSION = "$NEW_VERSION"
end
EOF
else
    sed -i.bak "s/VERSION = \"[^\"]*\"/VERSION = \"$NEW_VERSION\"/" bindings/ruby/lib/fastresize/version.rb
    rm -f bindings/ruby/lib/fastresize/version.rb.bak
fi

# Update gemspec (both old and new names for compatibility)
if [ -f "fast_resize.gemspec" ]; then
    sed -i.bak "s/spec.version[[:space:]]*=[[:space:]]*\"[^\"]*\"/spec.version       = \"$NEW_VERSION\"/" fast_resize.gemspec
    rm -f fast_resize.gemspec.bak
fi
if [ -f "fastresize.gemspec" ]; then
    sed -i.bak "s/spec.version[[:space:]]*=[[:space:]]*\"[^\"]*\"/spec.version       = \"$NEW_VERSION\"/" fastresize.gemspec
    rm -f fastresize.gemspec.bak
fi

# Update documentation
for doc in README.md docs/*.md; do
    if [ -f "$doc" ]; then
        # Update version in installation examples and version references
        sed -i.bak "s/v[0-9]\+\.[0-9]\+\.[0-9]\+/v$NEW_VERSION/g" "$doc"
        sed -i.bak "s/fast_resize-[0-9]\+\.[0-9]\+\.[0-9]\+/fast_resize-$NEW_VERSION/g" "$doc"
        sed -i.bak "s/fastresize-[0-9]\+\.[0-9]\+\.[0-9]\+/fastresize-$NEW_VERSION/g" "$doc"
        rm -f "$doc.bak"
    fi
done

echo "✅ Version updated to $NEW_VERSION in all files"
echo ""
echo "Changed files:"
git diff --name-only
