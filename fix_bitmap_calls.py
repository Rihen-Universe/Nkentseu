#!/usr/bin/env python3
"""
Fix NkBitmapBuffer method calls in Cache and Library files
"""

import re
from pathlib import Path

BITMAP_CALLS = {
    'bitmap.isReady()': 'bitmap.IsReady()',
    'bitmap.data()': 'bitmap.Data()',
    'bitmap.stride()': 'bitmap.Stride()',
    'bitmap.byteSize()': 'bitmap.ByteSize()',
}

def process_file(filepath: str) -> int:
    """Fix method calls."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        original = content
        changes = 0
        
        for old_call, new_call in BITMAP_CALLS.items():
            count = content.count(old_call)
            if count > 0:
                content = content.replace(old_call, new_call)
                changes += count
        
        if content != original:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(content)
        
        return changes
        
    except Exception as e:
        print(f"  ERROR in {Path(filepath).name}: {str(e)}")
        return 0

def main():
    print("=" * 80)
    print("Fixing NkBitmapBuffer method calls")
    print("=" * 80)
    print()
    
    files_to_fix = [
        r"e:\Projets\2026\Nkentseu\Nkentseu\Modules\Runtime\NKFont\src\NKFont\Cache\NkGlyphCache.cpp",
        r"e:\Projets\2026\Nkentseu\Nkentseu\Modules\Runtime\NKFont\src\NKFont\Core\NkFontLibrary.cpp",
    ]
    
    total = 0
    for filepath in files_to_fix:
        if Path(filepath).exists():
            changes = process_file(filepath)
            name = Path(filepath).name
            if changes > 0:
                print(f"  ✓ {name:45} ({changes} calls fixed)")
                total += changes
            else:
                print(f"  ○ {name:45} (no changes)")
        else:
            print(f"  ✗ {name:45} NOT FOUND")
    
    print()
    print("=" * 80)
    print(f"Total calls fixed: {total}")
    print("=" * 80)

if __name__ == "__main__":
    main()
