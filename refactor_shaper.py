#!/usr/bin/env python3
"""
Complete refactoring of Shaper folder: NkTextShaper.h/.cpp
Converts all camelCase methods to PascalCase
"""

import re
from pathlib import Path

# All methods in NkTextShaper
SHAPER_METHODS = {
    # Public methods
    'shapeUtf8': 'ShapeUtf8',
    'measureUtf8': 'MeasureUtf8',
    'getCaretPosition': 'GetCaretPosition',
    'shapeCodepoints': 'ShapeCodepoints',
    
    # Private methods
    'beginLine': 'BeginLine',
    'endLine': 'EndLine',
    'shapeOne': 'ShapeOne',
    'shouldWrap': 'ShouldWrap',
    'quantizeSubpixel': 'QuantizeSubpixel',
}

# Other getters in NkGlyphRun and NkShapeResult
STRUCT_METHODS = {
    'addGlyph': 'AddGlyph',
    'currentLine': 'CurrentLine',
}

def process_file(filepath: str) -> int:
    """Process a file: convert camelCase to PascalCase."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        original = content
        changes = 0
        
        all_methods = {**SHAPER_METHODS, **STRUCT_METHODS}
        
        for old_name, new_name in all_methods.items():
            # Match method calls and declarations
            # Pattern: word boundary + method name + (
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
    print("NKFont Shaper Folder: Complete PascalCase Refactoring")
    print("=" * 80)
    print()
    
    shaper_path = Path(r"e:\Projets\2026\Nkentseu\Nkentseu\Modules\Runtime\NKFont\src\NKFont\Shaper")
    
    if not shaper_path.exists():
        print(f"Error: Shaper path not found: {shaper_path}")
        return
    
    files = sorted(list(shaper_path.glob("*.h")) + list(shaper_path.glob("*.cpp")))
    
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
