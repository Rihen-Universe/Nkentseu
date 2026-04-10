#!/usr/bin/env python3
"""
Complete refactoring of remaining camelCase methods in Loaders folder
"""

import re
from pathlib import Path

# Comprehensive mapping for all loaders
LOADERS_METHODS = {
    # NkBdfLoader - public interface
    'canLoad': 'CanLoad',
    'load': 'Load',
    'unload': 'Unload',
    'getMaxFaceCount': 'GetMaxFaceCount',
    'getFaceCount': 'GetFaceCount',
    
    # NkBdfLoader - private methods
    'ensurePools': 'EnsurePools',
    'freePools': 'FreePools',
    'parseHeader': 'ParseHeader',
    'parseGlyph': 'ParseGlyph',
    'buildCmap': 'BuildCmap',
    'isHexChar': 'IsHexChar',
    'hexVal': 'HexVal',
    'decodeHex': 'DecodeHex',
    'isNewline': 'IsNewline',
    'getPixel': 'GetPixel',
    
    # NkTtfLoader - public interface
    # (already done, but included for completeness)
    
    # NkTtfLoader - private methods
    'parseTableDirectory': 'ParseTableDirectory',
    'closeContour': 'CloseContour',
    'addPoint': 'AddPoint',
    'decodeSimpleGlyph': 'DecodeSimpleGlyph',
    'decodeCompositeGlyph': 'DecodeCompositeGlyph',
    'cmapFmt4Lookup': 'CmapFmt4Lookup',
    'cmapFmt12Lookup': 'CmapFmt12Lookup',
    'copyMacRoman': 'CopyMacRoman',
    'copyAsciiFromUtf16BE': 'CopyAsciiFromUtf16BE',
    'isEmpty': 'IsEmpty',
    'pairCount': 'PairCount',
    'lookupPixels': 'LookupPixels',
    'resolveGlyfOffset': 'ResolveGlyfOffset',
}

def process_file(filepath: str) -> int:
    """Process a file: convert camelCase to PascalCase."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        original = content
        changes = 0
        
        for old_name, new_name in LOADERS_METHODS.items():
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
    print("NKFont Loaders: Complete PascalCase Refactoring (All Methods)")
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
