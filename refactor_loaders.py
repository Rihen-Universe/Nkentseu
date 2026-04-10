#!/usr/bin/env python3
"""
Complete refactoring of Loaders folder: NkTtfLoader + NkBdfLoader
Converts all remaining camelCase methods to PascalCase
"""

import re
from pathlib import Path

# Methods in NkTtfLoader and NkBdfLoader
LOADER_METHODS = {
    # Generic loader interface
    'load': 'Load',
    'unload': 'Unload',
    'getMaxFaceCount': 'GetMaxFaceCount',
    'getFaceCount': 'GetFaceCount',
    
    # Parser helpers (private methods)
    'readU8': 'ReadU8',
    'readU16': 'ReadU16',
    'readU32': 'ReadU32',
    'readI8': 'ReadI8',
    'readI16': 'ReadI16',
    'readI32': 'ReadI32',
    'readFixed': 'ReadFixed',
    'readF2Dot14': 'ReadF2Dot14',
    'peek': 'Peek',
    'skip': 'Skip',
    'seek': 'Seek',
    'parseHead': 'ParseHead',
    'parseHhea': 'ParseHhea',
    'parseMaxp': 'ParseMaxp',
    'parseOs2': 'ParseOs2',
    'parseName': 'ParseName',
    'parseCmap': 'ParseCmap',
    'parseGlyf': 'ParseGlyf',
    'parseKern': 'ParseKern',
    'parseGsub': 'ParseGsub',
    'parseGpos': 'ParseGpos',
    'parsePost': 'ParsePost',
    'parseHmtx': 'ParseHmtx',
    'parseVmtx': 'ParseVmtx',
    'parseBitmap': 'ParseBitmap',
}

def process_file(filepath: str) -> int:
    """Process a file: convert camelCase to PascalCase."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        original = content
        changes = 0
        
        for old_name, new_name in LOADER_METHODS.items():
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
    print("NKFont Loaders Folder: Complete PascalCase Refactoring")
    print("=" * 80)
    print()
    
    loaders_path = Path(r"e:\Projets\2026\Nkentseu\Nkentseu\Modules\Runtime\NKFont\src\NKFont\Loaders")
    
    if not loaders_path.exists():
        print(f"Error: Loaders path not found: {loaders_path}")
        return
    
    files = sorted(list(loaders_path.glob("*.h")) + list(loaders_path.glob("*.cpp")))
    
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
