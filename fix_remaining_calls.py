#!/usr/bin/env python3
"""
Fix all remaining method calls in NkTtfLoader
"""

import re
from pathlib import Path

# All remaining calls to fix (both . and method names)
REMAINING_CALLS = {
    '.ptrAt(': '.PtrAt(',
    '.peekU16(': '.PeekU16(',
    '.peekU32(': '.PeekU32(',
    '.peekI16(': '.PeekI16(',
}

def process_file(filepath: str) -> int:
    """Fix remaining method calls."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        original = content
        changes = 0
        
        for old_call, new_call in REMAINING_CALLS.items():
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
    print("Fixing remaining method calls in NkTtfLoader")
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
