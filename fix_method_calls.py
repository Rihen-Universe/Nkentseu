#!/usr/bin/env python3
"""
Fix all method calls in NkFontLibrary.cpp that use the renamed Cache/Atlas methods
"""

import re
from pathlib import Path

CALL_REPLACEMENTS = {
    'mCache.invalidateFont': 'mCache.InvalidateFont',
    'mAtlas.insertGlyph': 'mAtlas.InsertGlyph',
    'mAtlas.isReady': 'mAtlas.IsReady',
    'mCache.getBitmap': 'mCache.GetBitmap',
}

def process_file(filepath: str) -> int:
    """Process a file and fix method calls."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        original = content
        changes = 0
        
        for old_call, new_call in CALL_REPLACEMENTS.items():
            # Match method calls - they should be followed by (
            pattern = re.escape(old_call) + r'(?=\s*\()'
            matches = len(re.findall(pattern, content))
            if matches > 0:
                content = re.sub(pattern, new_call, content)
                changes += matches
                print(f"  Fixed: {old_call} → {new_call} ({matches} occurrences)")
        
        if content != original:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(content)
        
        return changes
        
    except Exception as e:
        print(f"  ERROR: {str(e)}")
        return 0

def main():
    print("=" * 80)
    print("Fixing method calls in NkFontLibrary.cpp")
    print("=" * 80)
    print()
    
    lib_file = r"e:\Projets\2026\Nkentseu\Nkentseu\Modules\Runtime\NKFont\src\NKFont\Core\NkFontLibrary.cpp"
    
    if not Path(lib_file).exists():
        print(f"Error: File not found: {lib_file}")
        return
    
    changes = process_file(lib_file)
    
    print()
    print("=" * 80)
    print(f"Total method calls fixed: {changes}")
    print("=" * 80)

if __name__ == "__main__":
    main()
