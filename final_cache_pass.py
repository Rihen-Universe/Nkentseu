#!/usr/bin/env python3
"""
Complete method conversion for Cache files: getters, definitions and calls
"""

import re
from pathlib import Path

# All methods that neeed Conversion
ALL_METHODS = {
    # Getters 
    'isReady': 'IsReady',
    'pageCount': 'PageCount',
    'glyphCount': 'GlyphCount',
    'page': 'Page',
    'setUploadCallback': 'SetUploadCallback',
}

def process_file(filepath: str) -> int:
    """Convert all method names in a file."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        original = content
        changes = 0
        
        for old_name, new_name in ALL_METHODS.items():
            # Match direct: methodName() 
            pattern = r'\b' + re.escape(old_name) + r'(?=\s*\()'
            count = len(re.findall(pattern, content))
            if count > 0:
                content = re.sub(pattern, new_name, content)
                changes += count
        
        if content != original:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(content)
        
        return changes
        
    except Exception as e:
        print(f"  ERROR: {str(e)}")
        return 0

def main():
    print("=" * 80)
    print("Final pass: Converting remaining getters and calls")
    print("=" * 80)
    print()
    
    files = [
        r"e:\Projets\2026\Nkentseu\Nkentseu\Modules\Runtime\NKFont\src\NKFont\Cache\NkGlyphCache.h",
        r"e:\Projets\2026\Nkentseu\Nkentseu\Modules\Runtime\NKFont\src\NKFont\Cache\NkGlyphCache.cpp",
        r"e:\Projets\2026\Nkentseu\Nkentseu\Modules\Runtime\NKFont\src\NKFont\Core\NkFontLibrary.cpp",
    ]
    
    total = 0
    for file_path in files:
        if Path(file_path).exists():
            changes = process_file(file_path)
            name = Path(file_path).name
            if changes > 0:
                print(f"  ✓ {name:45} ({changes} conversions)")
                total += changes
            else:
                print(f"  ○ {name:45} (no changes)")
    
    print()
    print("=" * 80)
    print(f"Total conversions: {total}")
    print("=" * 80)

if __name__ == "__main__":
    main()
