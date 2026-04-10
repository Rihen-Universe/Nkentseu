#!/usr/bin/env python3
"""
Complete refactoring of Core folder: Convert remaining camelCase methods
"""

import re
from pathlib import Path

# Core folder methods
CORE_METHODS = {
    # NkGlyphOutline (in NkGlyphMetrics)
    'addPoint': 'AddPoint',
    'closeContour': 'CloseContour',
    'isEmpty': 'IsEmpty',
    
    # NkFontLoader interface
    'canLoad': 'CanLoad',
    'load': 'Load',
    'unload': 'Unload',
    'getMaxFaceCount': 'GetMaxFaceCount',
    'getFaceCount': 'GetFaceCount',
}

def process_file(filepath: str) -> int:
    """Process a file: convert camelCase to PascalCase."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        original = content
        changes = 0
        
        for old_name, new_name in CORE_METHODS.items():
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
    print("NKFont Core: PascalCase Refactoring")
    print("=" * 80)
    print()
    
    core_path = Path(r"e:\Projets\2026\Nkentseu\Nkentseu\Modules\Runtime\NKFont\src\NKFont\Core")
    
    if not core_path.exists():
        print(f"Error: Core path not found: {core_path}")
        return
    
    files = sorted(list(core_path.glob("*.h")) + list(core_path.glob("*.cpp")))
    
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
