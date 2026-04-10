#!/usr/bin/env python3
"""
Fix method calls in demo files that use NkTextShaper methods
"""

import re
from pathlib import Path

SHAPER_CALLS = {
    'shapeUtf8': 'ShapeUtf8',
    'measureUtf8': 'MeasureUtf8',
    'getCaretPosition': 'GetCaretPosition',
    'shapeCodepoints': 'ShapeCodepoints',
}

def process_file(filepath: str) -> int:
    """Fix method calls in a file."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        original = content
        changes = 0
        
        for old_call, new_call in SHAPER_CALLS.items():
            # Match method calls: .methodName(
            pattern = r'\b' + re.escape(old_call) + r'(?=\s*\()'
            count = len(re.findall(pattern, content))
            if count > 0:
                content = re.sub(pattern, new_call, content)
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
    print("Fixing method calls in demo/integration files")
    print("=" * 80)
    print()
    
    demo_files = [
        r"e:\Projets\2026\Nkentseu\Nkentseu\Applications\Sandbox\src\DemoNkentseu\Base03\NkRHIDemoText.cpp",
        r"e:\Projets\2026\Nkentseu\Nkentseu\Applications\Sandbox\src\DemoNkentseu\Base04\NkUIFontIntegration.cpp",
    ]
    
    total = 0
    for filepath in demo_files:
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
