#!/usr/bin/env python3
"""
Comprehensive audit of Loaders folder for remaining camelCase methods
"""

import re
from pathlib import Path

# All known camelCase method patterns to check for
CAMELCASE_PATTERNS = {
    r'\b(load|unload|init|parse|read|write|seek|skip|peek|get|set|is|has|clear|reset|add|remove|insert|delete|find|search|sort|merge|split|join|compare|copy|move|rename|exist|valid|empty|full|next|previous|first|last|begin|end|count|size|length|index|position|offset|point|line|polygon|segment|curve|path|shape|color|pixel|buffer|array|matrix|vector|queue|stack|list|map|table|tree|graph|node|edge|vertex)\()\b'
}

def scan_file_for_camelcase(filepath: str) -> list:
    """Scan file for potential camelCase methods."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Find method declarations and calls
        # Pattern: word followed by (
        pattern = r'\b([a-z][a-zA-Z0-9]*)\s*\('
        matches = re.findall(pattern, content)
        
        # Filter to only camelCase patterns
        camelcase_methods = []
        for match in set(matches):
            # Exclude common keywords
            if match not in ['if', 'for', 'while', 'switch', 'return', 'new', 'delete', 'sizeof', 'static_cast', 'const_cast', 'reinterpret_cast', 'dynamic_cast']:
                # Check if it has at least one uppercase letter (true camelCase)
                if any(c.isupper() for c in match):
                    camelcase_methods.append(match)
        
        return sorted(set(camelcase_methods))
        
    except Exception as e:
        print(f"  ERROR: {str(e)}")
        return []

def main():
    print("=" * 80)
    print("NKFont Loaders: Audit for remaining camelCase methods")
    print("=" * 80)
    print()
    
    loaders_path = Path(r"e:\Projets\2026\Nkentseu\Nkentseu\Modules\Runtime\NKFont\src\NKFont\Loaders")
    
    if not loaders_path.exists():
        print(f"Error: Loaders path not found: {loaders_path}")
        return
    
    files = sorted(list(loaders_path.glob("*.h")) + list(loaders_path.glob("*.cpp")))
    
    total_methods_found = 0
    for filepath in files:
        methods = scan_file_for_camelcase(str(filepath))
        name = filepath.name
        if methods:
            print(f"  ✓ {name:40} ({len(methods)} camelCase methods found)")
            for method in methods[:10]:  # Show first 10
                print(f"      - {method}")
            if len(methods) > 10:
                print(f"      ... and {len(methods) - 10} more")
            total_methods_found += len(methods)
        else:
            print(f"  ✓ {name:40} (all PascalCase ✓)")
    
    print()
    print("=" * 80)
    print(f"Total camelCase methods found: {total_methods_found}")
    print("=" * 80)

if __name__ == "__main__":
    main()
