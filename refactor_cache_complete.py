#!/usr/bin/env python3
"""
Complete refactoring of Cache folder: NkGlyphCache.h/.cpp
Converts ALL remaining camelCase methods to PascalCase
"""

import re
from pathlib import Path
from typing import Dict, Set, Tuple

# Comprehensive mapping for Cache folder methods
CACHE_METHODS = {
    # NkGlyphCache privées
    'hashBucket': 'HashBucket',
    'findBucket': 'FindBucket',
    'unlinkLru': 'UnlinkLru',
    'linkFront': 'LinkFront',
    'moveToFront': 'MoveToFront',
    'evictLru': 'EvictLru',
    'acquireSlot': 'AcquireSlot',
    'getBitmap': 'GetBitmap',
    'invalidateFont': 'InvalidateFont',
    
    # NkAtlasPage
    'pack': 'Pack',
    'blit': 'Blit',
    
    # NkGlyphAtlas
    'getUV': 'GetUV',
    'setUploadCallback': 'SetUploadCallback',
    'openNewPage': 'OpenNewPage',
    'lookupSearch': 'LookupSearch',
    'sortLookup': 'SortLookup',
    'insertGlyph': 'InsertGlyph',
}

def process_file(filepath: str) -> Tuple[bool, str, int]:
    """Process a file: convert camelCase to PascalCase."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        original_content = content
        changes_made = 0
        
        for old_name, new_name in CACHE_METHODS.items():
            # Match method calls: methodName(
            pattern = r'\b' + re.escape(old_name) + r'(?=\s*\()'
            count = len(re.findall(pattern, content))
            if count > 0:
                content = re.sub(pattern, new_name, content)
                changes_made += count
            
            # Match method declarations: methodName(
            pattern2 = r'\b' + re.escape(old_name) + r'(?=\s*\([\w\s,&*]*\)\s*(?:const|{|;|override|noexcept))'
            count2 = len(re.findall(pattern2, content))
            if count2 > 0:
                content = re.sub(pattern2, new_name, content)
                changes_made += count2
        
        if content == original_content:
            return True, f"✓ {Path(filepath).name:40} (no changes needed)", 0
        
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        
        return True, f"✓ {Path(filepath).name:40} ({changes_made} conversions)", changes_made
        
    except Exception as e:
        return False, f"✗ {Path(filepath).name:40} ERROR: {str(e)}", 0

def main():
    print("=" * 80)
    print("NKFont Cache Folder: Complete PascalCase Refactoring")
    print("=" * 80)
    print()
    
    cache_path = Path(r"e:\Projets\2026\Nkentseu\Nkentseu\Modules\Runtime\NKFont\src\NKFont\Cache")
    
    if not cache_path.exists():
        print(f"Error: Cache path not found: {cache_path}")
        return
    
    files = sorted(list(cache_path.glob("*.h")) + list(cache_path.glob("*.cpp")))
    
    total_changes = 0
    for filepath in files:
        success, message, changes = process_file(str(filepath))
        print(message)
        if success:
            total_changes += changes
    
    print()
    print("=" * 80)
    print(f"Total methods converted: {total_changes}")
    print("=" * 80)

if __name__ == "__main__":
    main()
