#!/usr/bin/env python3
"""
Refactor NkBinaryReader methods to PascalCase
"""

import re
from pathlib import Path

READER_METHODS = {
    # Seek/Tell
    'seek': 'Seek',
    'tell': 'Tell',
    'ok': 'Ok',
    'size': 'Size',
    'remaining': 'Remaining',
    
    # Read methods
    'readU8': 'ReadU8',
    'readI8': 'ReadI8',
    'readU16': 'ReadU16',
    'readI16': 'ReadI16',
    'readU32': 'ReadU32',
    'readI32': 'ReadI32',
    'readBytes': 'ReadBytes',
    'readFixed': 'ReadFixed',
    'readF2Dot14': 'ReadF2Dot14',
    
    # Peek methods
    'peek': 'Peek',
    'peekU16': 'PeekU16',
    'peekU32': 'PeekU32',
    'peekI16': 'PeekI16',
    
    # Skip
    'skip': 'Skip',
    
    # Pointer access
    'ptr': 'Ptr',
    'ptrAt': 'PtrAt',
}

def process_file(filepath: str) -> int:
    """Process file and convert method names."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        original = content
        changes = 0
        
        for old_name, new_name in READER_METHODS.items():
            # Match method calls: name(
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
    print("NkBinaryReader: Complete PascalCase Refactoring")
    print("=" * 80)
    print()
    
    # Only the NKFont version
    reader_file = r"e:\Projets\2026\Nkentseu\Nkentseu\Modules\Runtime\NKFont\src\NKFont\Core\NkBinaryReader.h"
    
    if Path(reader_file).exists():
        changes = process_file(reader_file)
        if changes > 0:
            print(f"  ✓ NkBinaryReader.h                         ({changes} conversions)")
        else:
            print(f"  ○ NkBinaryReader.h                         (no changes)")
    else:
        print(f"  ✗ NkBinaryReader.h                         NOT FOUND")
    
    print()
    print("=" * 80)
    print(f"Total conversions: {changes}")
    print("=" * 80)

if __name__ == "__main__":
    main()
