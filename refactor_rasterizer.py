#!/usr/bin/env python3
"""
Complete refactoring of Rasterizer folder: NkRasterizer.h/.cpp
Converts all remaining camelCase methods to PascalCase
"""

import re
from pathlib import Path

# All methods in NkRasterizer
RASTERIZER_METHODS = {
    # Public methods
    'init': 'Init',
    'rasterizeBitmap': 'RasterizeBitmap',
    'rasterizeSdf': 'RasterizeSdf',
    'rasterizeMsdf': 'RasterizeMsdf',
    
    # Private/helper methods
    'setup': 'Setup',
    'cleanup': 'Cleanup',
    'computeSignedDistance': 'ComputeSignedDistance',
    'computeMsdf': 'ComputeMsdf',
    'fillBuffer': 'FillBuffer',
    'edgeDistance': 'EdgeDistance',
    'contourDistance': 'ContourDistance',
}

def process_file(filepath: str) -> int:
    """Process a file: convert camelCase to PascalCase."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        original = content
        changes = 0
        
        for old_name, new_name in RASTERIZER_METHODS.items():
            # Match method calls and declarations: name(
            pattern = r'\b' + re.escape(old_name) + r'(?=\s*\()'
            count = len(re.findall(pattern, content))
            if count > 0:
                content = re.sub(pattern, new_name, content)
                changes += count
        
        if content == original:
            return 0
        
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        
        return changes
        
    except Exception as e:
        print(f"  ERROR: {str(e)}")
        return 0

def main():
    print("=" * 80)
    print("NKFont Rasterizer Folder: Complete PascalCase Refactoring")
    print("=" * 80)
    print()
    
    rasterizer_path = Path(r"e:\Projets\2026\Nkentseu\Nkentseu\Modules\Runtime\NKFont\src\NKFont\Rasterizer")
    
    if not rasterizer_path.exists():
        print(f"Error: Rasterizer path not found: {rasterizer_path}")
        return
    
    files = sorted(list(rasterizer_path.glob("*.h")) + list(rasterizer_path.glob("*.cpp")))
    
    total_changes = 0
    for filepath in files:
        changes = process_file(str(filepath))
        name = filepath.name
        if changes > 0:
            print(f"  ✓ {name:40} ({changes} conversions)")
            total_changes += changes
        else:
            print(f"  ○ {name:40} (no changes)")
    
    print()
    print("=" * 80)
    print(f"Total methods converted: {total_changes}")
    print("=" * 80)

if __name__ == "__main__":
    main()
