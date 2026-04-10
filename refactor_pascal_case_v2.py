#!/usr/bin/env python3
"""
Enhanced refactoring: Converts REMAINING camelCase methods to PascalCase
Targets NKFont and integration files
"""

import re
import os
import sys
from pathlib import Path
from typing import List, Tuple

# Extended mapping including the ones we missed
METHOD_MAPPING = {
    # NkFontFace
    'getInfo': 'GetInfo',
    'getGlyphIdForIndex': 'GetGlyphIdForIndex',
    'loadGlyphMetrics': 'LoadGlyphMetrics',
    'loadGlyphOutline': 'LoadGlyphOutline',
    'hasGlyph': 'HasGlyph',
    'getRawData': 'GetRawData',
    'getFontId': 'GetFontId',
    
    # NkGlyphCache
    'capacity': 'Capacity',
    'count': 'Count',
    'evictCount': 'EvictCount',
    'canInsert': 'CanInsert',
    
    # NkRasterizer  
    'rasterizeBitmap': 'RasterizeBitmap',
    'rasterizeSdf': 'RasterizeSdf',
    'rasterizeMsdf': 'RasterizeMsdf',
    
    # NkTextShaper
    'shape': 'Shape',
    'getRunCount': 'GetRunCount',
    'getRun': 'GetRun',
    
    # NkTtfLoader / NkBdfLoader
    'load': 'Load',
    'unload': 'Unload',
    'getMaxFaceCount': 'GetMaxFaceCount',
    'getFaceCount': 'GetFaceCount',
    
    # Generic
    'push': 'Push',
    'pop': 'Pop',
    'reset': 'Reset',
    'resize': 'Resize',
}

def process_file(filepath: str) -> Tuple[bool, str]:
    """Process a single file to convert remaining method names."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        original_content = content
        changes_made = 0
        
        # Apply each remaining method name conversion
        for old_name, new_name in METHOD_MAPPING.items():
            # Pattern: methodName followed by (
            # Match whole words only
            pattern = r'\b' + re.escape(old_name) + r'(?=\s*\()'
            replacement = new_name
            
            # Count replacements
            count = len(re.findall(pattern, content))
            if count > 0:
                content = re.sub(pattern, replacement, content)
                changes_made += count
        
        # Check if anything changed
        if content == original_content:
            return True, f"  ✓ {Path(filepath).name:40} (no changes needed)"
        
        # Write back
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        
        return True, f"  ✓ {Path(filepath).name:40} ({changes_made} method(s) converted)"
        
    except Exception as e:
        return False, f"  ✗ {Path(filepath).name:40} ERROR: {str(e)}"

def main():
    """Main entry point."""
    base_path = Path(r"e:\Projets\2026\Nkentseu\Nkentseu\Modules\Runtime\NKFont\src\NKFont")
    
    if not base_path.exists():
        print(f"Error: Base path does not exist: {base_path}")
        sys.exit(1)
    
    print("=" * 80)
    print("NKFont PascalCase Refactoring - Phase 2 (Remaining Methods)")
    print("=" * 80)
    print()
    
    # Find all .h and .cpp files
    files_to_process = sorted(list(base_path.glob("**/*.h")) + list(base_path.glob("**/*.cpp")))
    
    print(f"Found {len(files_to_process)} files to process\n")
    print("NKFont Module:")
    print("-" * 80)
    
    success_count = 0
    fail_count = 0
    total_changes = 0
    
    for filepath in files_to_process:
        success, message = process_file(str(filepath))
        print(message)
        
        if success:
            success_count += 1
            # Extract change count
            if '(' in message and ')' in message:
                try:
                    change_str = message[message.rfind('(')+1:message.rfind(')')]
                    if 'method' in change_str:
                        num = int(change_str.split()[0])
                        total_changes += num
                except:
                    pass
        else:
            fail_count += 1
    
    # Also process demo files
    demo_files = [
        r"e:\Projets\2026\Nkentseu\Nkentseu\Applications\Sandbox\src\DemoNkentseu\Base03\NkRHIDemoText.cpp",
        r"e:\Projets\2026\Nkentseu\Nkentseu\Applications\Sandbox\src\DemoNkentseu\Base04\NkUIFontIntegration.cpp",
        r"e:\Projets\2026\Nkentseu\Nkentseu\Applications\Sandbox\src\DemoNkentseu\Base04\NkUIFontIntegration.h",
    ]
    
    print()
    print("Demo & Integration Files:")
    print("-" * 80)
    
    for demo_file in demo_files:
        if Path(demo_file).exists():
            success, message = process_file(demo_file)
            print(message)
            if success:
                success_count += 1
            else:
                fail_count += 1
    
    print()
    print("=" * 80)
    print(f"Results: {success_count} files processed, {fail_count} errors")
    print(f"Total methods converted: {total_changes}")
    print("=" * 80)

if __name__ == "__main__":
    main()
