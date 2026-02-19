#!/bin/bash

# Script to create symbolic links for MkDocs documentation
# This automatically discovers all .md files and creates appropriate links
# Run this script from the project root: ./scripts/create_docs_links.sh

# Change to project root directory (parent of scripts/)
cd "$(dirname "$0")/.."

echo "🔍 Discovering .md files in the repository..."

# Clean and recreate only the mkdocs structure (preserve other docs/ content)
echo "🧹 Cleaning previous mkdocs links..."
rm -rf docs/mkdocs
mkdir -p docs/mkdocs

echo "📁 Creating docs/mkdocs directory structure..."

# Find all .md files, excluding certain directories and files
mdfiles=$(find . -name "*.md" \
    -not -path "./docs/*" \
    -not -path "./site/*" \
    -not -path "./venv/*" \
    -not -path "./build/*" \
    -not -path "./bin/*" \
    -not -path "./lib/*" \
    -not -path "./.git/*" \
    -not -path "./logs/*" \
    -not -path "./scripts/compression/*" \
    -not -name "CHANGELOG.md" \
    -not -name "CONTRIBUTING.md" \
    -not -name "LICENSE.md" \
    | sort)

echo "📄 Found markdown files:"
echo "$mdfiles" | sed 's/^/  /'

echo ""
echo "🔗 Creating symbolic links in docs/mkdocs/..."

cd docs/mkdocs

# Handle root README specially as index.md (MkDocs convention)
if [[ -f "../../README.md" ]]; then
    echo "  📌 Root README.md -> index.md"
    ln -sf ../../README.md index.md
fi

# Process all markdown files
echo "$mdfiles" | while read -r mdfile; do
    # Skip the root README (already handled as index.md)
    if [[ "$mdfile" == "./README.md" ]]; then
        continue
    fi
    
    # Remove leading ./
    clean_path=${mdfile#./}
    
    # Get directory path
    target_dir=$(dirname "$clean_path")
    
    # Create directory structure in docs/mkdocs/ if needed
    if [[ "$target_dir" != "." ]]; then
        mkdir -p "$target_dir"
        echo "  📁 Created directory: $target_dir"
    fi
    
    # Calculate relative path back to the original file
    # Count directory levels in the target path to determine how many ../ we need
    if [[ "$target_dir" != "." ]]; then
        # Count slashes in clean_path to determine depth
        depth=$(echo "$clean_path" | tr -cd '/' | wc -c)
        # From docs/mkdocs/path/to/file.md, we need (2 + depth) levels up
        up_levels=$((depth + 2))
        relative_path=""
        for ((i=0; i<up_levels; i++)); do
            relative_path="../$relative_path"
        done
        relative_path="${relative_path}$clean_path"
    else
        # For files in root: docs/mkdocs/file.md -> ../../file.md
        relative_path="../../$clean_path"
    fi
    
    echo "  🔗 $mdfile -> $clean_path (via $relative_path)"
    ln -sf "$relative_path" "$clean_path"
done

cd ../..

echo ""
echo "✅ Documentation links created successfully!"
echo "📊 Summary:"
total_mdfiles=$(echo "$mdfiles" | wc -l)
echo "  📄 Total markdown files processed: $total_mdfiles"
echo "  📁 MkDocs directory: docs/mkdocs/"
echo "  🔗 All markdown files now accessible to MkDocs"
echo "  💾 Original docs/ resources preserved"
echo "  🏗️ Original file structure preserved exactly"
echo ""
echo "🚀 Next steps:"
echo "  📖 Serve docs: make docs/serve"
echo "  🏗️  Build docs: make docs/build" 
