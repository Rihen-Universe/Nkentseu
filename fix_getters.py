#!/usr/bin/env python3
"""
Complete conversion of all getters and remaining methods in Cache files
"""

import re
from pathlib import Path

# Comprehensive getters and helper methods
GETTER_MAPPINGS = {
    # Getters with inline definitions
    'isReady': 'IsReady',
    'pageCount': 'PageCount',
    'glyphCount': 'GlyphCount',
    'page': 'Page',
    'isValid': 'IsValid',
    'isInAtlas': 'IsInAtlas',
    'isWhitespace': 'IsWhitespace',
    'entryCount': 'EntryCount',
    'hitRate': 'HitRate',
    'evictions': 'Evictions',
    'hits': 'Hits',
    'misses': 'Misses',
}

def process_file(filepath: str) -> int:
    """Process a file and convert getters."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        original = content
        changes = 0
        
        for old_name, new_name in GETTER_MAPPINGS.items():
            # Match standalone getter calls: name()
            # Match in function definitions: name() {
            pattern1 = r'\b' + re.escape(old_name) + r'(?=\s*\(\s*\))'
            new_changes = len(re.findall(pattern1, content))
            if new_changes > 0:
                content = re.sub(pattern1, new_name, content)
                changes += new_changes
        
        if content != original:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(content)
            return changes
        return 0
        
    except Exception as e:
        print(f"  ERROR: {str(e)}")
        return 0

def main():
    print("=" * 80)
    print("Converting remaining getters and helper methods")
    print("= " * 80)
    print()
    
    files_to_check = [
        r"e:\Projets\2026\Nkentseu\Nkentseu\Modules\Runtime\NKFont\src\NKFont\Cache\NkGlyphCache.h",
        r"e:\Projets\2026\Nkentseu\Nkentseu\Modules\Runtime\NKFont\src\NKFont\Cache\NkGlyphCache.cpp",
        r"e:\Projets\2026\Nkentseu\Nkentseu\Modules\Runtime\NKFont\src\NKFont\Core\NkFontLibrary.cpp",
    ]
    
    total_changes = 0
    for filepath in files_to_check:
        if Path(filepath).exists():
            changes = process_file(filepath)
            if changes > 0:
                print(f"  ✓ {Path(filepath).name:40} ({changes} conversions)")
                total_changes += changes
            else:
                print(f"  ✓ {Path(filepath).name:40} (no changes needed)")
        else:
            print(f"  ✗ {Path(filepath).name:40} NOT FOUND")
    
    print()
    print("=" * 80)
    print(f"Total getters converted: {total_changes}")
    print("=" * 80)

if __name__ == "__main__":
    main()
