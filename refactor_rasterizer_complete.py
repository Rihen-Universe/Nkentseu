#!/usr/bin/env python3
"""
Complete refactoring of Rasterizer folder: Fix remaining camelCase methods
"""

import re
from pathlib import Path

# All remaining methods to convert
REMAINING_METHODS = {
    # NkBitmapBuffer struct methods
    'isReady': 'IsReady',
    'stride': 'Stride',
    'byteSize': 'ByteSize',
    'data': 'Data',  # Both overloads will be matched
    
    # NkRasterizer private methods
    'fillCoverage': 'FillCoverage',
    'downsample': 'Downsample',
    'signedDistToSegment': 'SignedDistToSegment',
    'distToQuadBezier': 'DistToQuadBezier',
    'insideOutside': 'InsideOutside',
}

def process_file(filepath: str) -> int:
    """Process a file: convert camelCase to PascalCase."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        original = content
        changes = 0
        
        for old_name, new_name in REMAINING_METHODS.items():
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
    print("NKFont Rasterizer: Complete PascalCase Refactoring (Remaining Methods)")
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
