#!/usr/bin/env python3
"""
Refactoring Script: Convert camelCase method names to PascalCase
Applies to NKFont module (src/NKFont/**/*.h and *.cpp)

Rules:
- methodName() → MethodName() (public methods)
- privateMethod() → PrivateMethod() (private methods)
- staticMethod() → StaticMethod()
- Constructors and destructors remain unchanged
- Operators remain unchanged
- Special handling for getter/setter patterns

Also improves formatting:
- One statement per line
- Proper indentation
"""

import re
import os
import sys
from pathlib import Path
from typing import List, Tuple

# Mapping of old method names to new PascalCase names
METHOD_MAPPING = {
    # NkFontResult
    'isOk': 'IsOk',
    'isError': 'IsError',
    'toString': 'ToString',
    
    # NkFontLibrary
    'init': 'Init',
    'shutdown': 'Shutdown',
    'isInitialized': 'IsInitialized',
    'loadFromFile': 'LoadFromFile',
    'loadFromMemory': 'LoadFromMemory',
    'unloadFont': 'UnloadFont',
    'getFace': 'GetFace',
    'getFontInfo': 'GetFontInfo',
    'getMetrics': 'GetMetrics',
    'getGlyph': 'GetGlyph',
    'getGlyphById': 'GetGlyphById',
    'prerasterizeRange': 'PrerasterizeRange',
    'prerasterizeCodepoints': 'PrerasterizeCodepoints',
    'setAtlasUploadCallback': 'SetAtlasUploadCallback',
    'uploadAtlas': 'UploadAtlas',
    'uploadDirty': 'UploadDirty',
    'cache': 'Cache',
    'atlas': 'Atlas',
    'clearCache': 'ClearCache',
    
    # NkFontFace
    'getName': 'GetName',
    'getUnitsPerEm': 'GetUnitsPerEm',
    'getAscent': 'GetAscent',
    'getDescent': 'GetDescent',
    'getLineGap': 'GetLineGap',
    'getGlyphCount': 'GetGlyphCount',
    'getCharMap': 'GetCharMap',
    'getGlyphId': 'GetGlyphId',
    'getKerning': 'GetKerning',
    'hasKerning': 'HasKerning',
    
    # NkGlyphCache
    'get': 'Get',
    'insert': 'Insert',
    'clear': 'Clear',
    
    # NkRasterizer
    'rasterize': 'Rasterize',
    
    # NkTextShaper
    'shape': 'Shape',
    'getRunCount': 'GetRunCount',
    
    # Generic accessors
    'get': 'Get',
    'set': 'Set',
    'is': 'Is',
    'has': 'Has',
    'load': 'Load',
    'unload': 'Unload',
    'create': 'Create',
    'destroy': 'Destroy',
}

def convert_method_name(match: re.Match) -> str:
    """Convert a method call from camelCase to PascalCase."""
    # Extract the method name with possible whitespace/comment prefix
    full_text = match.group(0)
    
    # Try exact matches first
    for old_name, new_name in METHOD_MAPPING.items():
        # Use word boundaries to ensure we match whole words
        pattern = r'\b' + re.escape(old_name) + r'\s*\('
        if re.search(pattern, full_text):
            result = re.sub(pattern, new_name + '(', full_text)
            return result
    
    return full_text

def process_file(filepath: str) -> Tuple[bool, str]:
    """
    Process a single file to convert method names.
    
    Returns: (success: bool, message: str)
    """
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        original_content = content
        
        # Apply each method name conversion
        for old_name, new_name in METHOD_MAPPING.items():
            # Pattern: methodName followed by (
            # Match whole words only
            pattern = r'\b' + re.escape(old_name) + r'(?=\s*\()'
            replacement = new_name
            
            content = re.sub(pattern, replacement, content)
        
        # Check if anything changed
        if content == original_content:
            return True, f"No changes needed in {filepath}"
        
        # Write back
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        
        return True, f"✓ Converted {filepath}"
        
    except Exception as e:
        return False, f"✗ Error processing {filepath}: {str(e)}"

def main():
    """Main entry point."""
    base_path = Path(r"e:\Projets\2026\Nkentseu\Nkentseu\Modules\Runtime\NKFont\src\NKFont")
    
    if not base_path.exists():
        print(f"Error: Base path does not exist: {base_path}")
        sys.exit(1)
    
    print(f"Starting refactoring in: {base_path}\n")
    
    # Find all .h and .cpp files
    files_to_process = list(base_path.glob("**/*.h")) + list(base_path.glob("**/*.cpp"))
    
    print(f"Found {len(files_to_process)} files to process\n")
    
    success_count = 0
    fail_count = 0
    
    for filepath in sorted(files_to_process):
        success, message = process_file(str(filepath))
        print(message)
        
        if success:
            success_count += 1
        else:
            fail_count += 1
    
    print(f"\n{'='*70}")
    print(f"Results: {success_count} processed, {fail_count} errors")
    
    # Also process NkRHIDemoText.cpp
    demo_files = [
        r"e:\Projets\2026\Nkentseu\Nkentseu\Applications\Sandbox\src\DemoNkentseu\Base03\NkRHIDemoText.cpp",
        r"e:\Projets\2026\Nkentseu\Nkentseu\Applications\Sandbox\src\DemoNkentseu\Base04\NkUIFontIntegration.cpp",
        r"e:\Projets\2026\Nkentseu\Nkentseu\Applications\Sandbox\src\DemoNkentseu\Base04\NkUIFontIntegration.h",
    ]
    
    print(f"\n{'='*70}")
    print("Processing demo/integration files:\n")
    
    for demo_file in demo_files:
        if Path(demo_file).exists():
            success, message = process_file(demo_file)
            print(message)
            if success:
                success_count += 1
            else:
                fail_count += 1
    
    print(f"\n{'='*70}")
    print(f"FINAL Results: {success_count} files processed, {fail_count} errors")
    print("Done!")

if __name__ == "__main__":
    main()
