#!/usr/bin/env python3
"""
Fix remaining .ok() and similar calls in NkTtfLoader
"""

import re
from pathlib import Path

# These are small getters that need fixing
GETTER_FIXES = {
    '.ok()': '.Ok()',
    '.size()': '.Size()',
    '.tell()': '.Tell()',
    '.remaining()': '.Remaining()',
    '.ptr()': '.Ptr()',
}

def process_file(filepath: str) -> int:
    """Fix getter calls."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        original = content
        changes = 0
        
        for old_call, new_call in GETTER_FIXES.items():
            count = content.count(old_call)
            if count > 0:
                content = content.replace(old_call, new_call)
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
    print("Fixing getter calls in NkTtfLoader")
    print("=" * 80)
    print()
    
    ttf_file = r"e:\Projets\2026\Nkentseu\Nkentseu\Modules\Runtime\NKFont\src\NKFont\Loaders\NkTtfLoader.cpp"
    
    if Path(ttf_file).exists():
        changes = process_file(ttf_file)
        if changes > 0:
            print(f"  ✓ NkTtfLoader.cpp                         ({changes} calls fixed)")
        else:
            print(f"  ○ NkTtfLoader.cpp                         (no changes)")
    else:
        print(f"  ✗ NkTtfLoader.cpp                         NOT FOUND")
    
    print()
    print("=" * 80)
    print(f"Total calls fixed: {changes}")
    print("=" * 80)

if __name__ == "__main__":
    main()
