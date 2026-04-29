#!/usr/bin/env python3
# =============================================================================
# FICHIER: Tools/EmbedFont.py
# DESCRIPTION: Génère un fichier C header contenant les données compressées
#              d'un fichier TTF/OTF, pour inclusion dans NKFont/Embedded/.
#
# Usage :
#   python3 Tools/EmbedFont.py <fichier.ttf> [nom_symbolique]
#
# Exemples :
#   python3 Tools/EmbedFont.py Resources/Fonts/ProggyClean.ttf
#   python3 Tools/EmbedFont.py Resources/Fonts/Roboto-Regular.ttf Roboto
#
# Sortie :
#   NKFont/Embedded/<NomSymbolique>_data.h
#
# Le fichier généré contient :
#   - Les données compressées (deflate/zlib) du TTF en tableau C.
#   - Les tailles originale et compressée.
#   - Un snippet d'intégration dans NkFontEmbedded.cpp.
#
# Dépendances : Python 3.6+, aucune lib externe.
# =============================================================================

import sys
import os
import zlib
import struct

# =============================================================================
# CONFIGURATION
# =============================================================================

OUTPUT_DIR = "./scripts/Embedded"
MAX_COLS   = 16  # octets par ligne dans le tableau C

# =============================================================================
# ENCODAGE BASE85 (compatible avec l'encodeur ImGui)
# Réduction supplémentaire de ~20% par rapport au hex pur.
# =============================================================================

BASE85_CHARS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!#$%&()*+-;<=>?@^_`{|}~"

def to_base85(data: bytes) -> str:
    """Encode des bytes en base85 (5 chars par 4 bytes)."""
    # Pad à multiple de 4
    pad = (-len(data)) % 4
    data_padded = data + b'\x00' * pad
    result = []
    for i in range(0, len(data_padded), 4):
        n = struct.unpack('>I', data_padded[i:i+4])[0]
        s = ''
        for _ in range(5):
            s = BASE85_CHARS[n % 85] + s
            n //= 85
        result.append(s)
    # Retire les chars de padding
    encoded = ''.join(result)
    return encoded[:len(encoded) - pad] if pad else encoded

# =============================================================================
# GÉNÉRATION DU HEADER C
# =============================================================================

def generate_header(ttf_path: str, font_name: str, output_path: str):
    """Génère un fichier .h avec les données compressées du TTF."""

    print(f"[EmbedFont] Lecture de '{ttf_path}'...")
    with open(ttf_path, 'rb') as f:
        original_data = f.read()

    original_size = len(original_data)
    print(f"[EmbedFont] Taille originale : {original_size:,} octets ({original_size/1024:.1f} Ko)")

    # Compression zlib (niveau 9 = maximum)
    compressed_data = zlib.compress(original_data, 9)
    compressed_size = len(compressed_data)
    ratio = (1.0 - compressed_size / original_size) * 100.0
    print(f"[EmbedFont] Taille compressée : {compressed_size:,} octets ({compressed_size/1024:.1f} Ko) — {ratio:.1f}% de réduction")

    # Génère le tableau C en hex (le plus portable)
    hex_lines = []
    for i in range(0, compressed_size, MAX_COLS):
        chunk = compressed_data[i:i+MAX_COLS]
        hex_lines.append('    ' + ', '.join(f'0x{b:02X}' for b in chunk) + ',')

    # Génère le base85 (optionnel, plus compact)
    b85 = to_base85(compressed_data)
    b85_lines = []
    for i in range(0, len(b85), 64):
        b85_lines.append(f'    "{b85[i:i+64]}"')

    # ── Contenu du header ─────────────────────────────────────────────────
    guard = f"NK_NKFONT_EMBEDDED_{font_name.upper()}_DATA_H"

    lines = [
        f"// Auto-généré par Tools/EmbedFont.py — NE PAS ÉDITER",
        f"// Source  : {os.path.basename(ttf_path)}",
        f"// Taille  : {original_size:,} octets (original), {compressed_size:,} (zlib)",
        f"// Compression : {ratio:.1f}%",
        f"//",
        f"// Intégration dans NkFontEmbedded.cpp :",
        f"//   #include \"{font_name}_data.h\"",
        f"//   {{ \"{font_name}\", s{font_name}CompressedData, s{font_name}CompressedSize,",
        f"//     s{font_name}OriginalSize, NkFontKind::Vector, 0.f, \"License\" }},",
        f"",
        f"#pragma once",
        f"#ifndef {guard}",
        f"#define {guard}",
        f"",
        f"// ============================================================",
        f"// DONNÉES COMPRESSÉES (zlib/deflate)",
        f"// ============================================================",
        f"",
        f"static const nkft_uint32 s{font_name}OriginalSize    = {original_size}u;",
        f"static const nkft_uint32 s{font_name}CompressedSize  = {compressed_size}u;",
        f"",
        f"// Tableau de bytes compressés (hex) :",
        f"static const nkft_uint8  s{font_name}CompressedData[{compressed_size}] = {{",
    ]
    lines.extend(hex_lines)
    lines.append("};")
    lines.append("")
    lines.append(f"// Version Base85 (20% plus compact, optionnel) :")
    lines.append(f"// static const char* s{font_name}Base85 =")
    lines.extend(b85_lines[:1])  # juste la première ligne comme exemple
    lines.append(f"// ... ({len(b85_lines)} lignes total)")
    lines.append("")
    lines.append(f"#endif // {guard}")
    lines.append("")
    lines.append("// ============================================================")
    lines.append("// Copyright © 2024-2026 Rihen. All rights reserved.")
    lines.append("// ============================================================")

    # ── Écriture ──────────────────────────────────────────────────────────
    os.makedirs(os.path.dirname(output_path) if os.path.dirname(output_path) else '.', exist_ok=True)
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))

    print(f"[EmbedFont] Header généré : '{output_path}'")
    print(f"[EmbedFont] Taille du header : {os.path.getsize(output_path):,} octets")

# =============================================================================
# SNIPPET D'INTÉGRATION AUTOMATIQUE
# =============================================================================

def print_integration_snippet(font_name: str, kind: str, native_size: float):
    """Affiche le code à copier dans NkFontEmbedded.cpp."""
    print()
    print("=" * 60)
    print("SNIPPET D'INTÉGRATION dans NkFontEmbedded.cpp :")
    print("=" * 60)
    print()
    print(f"// 1. Dans les includes :")
    print(f'#include "{font_name}_data.h"')
    print()
    print(f"// 2. Dans sRegistry[] :")
    print(f"{{")
    print(f'    "{font_name}",')
    print(f"    s{font_name}CompressedData,")
    print(f"    s{font_name}CompressedSize,")
    print(f"    s{font_name}OriginalSize,")
    print(f"    NkFontKind::{kind},")
    print(f"    {native_size}f,  // taille native (0 si vectoriel)")
    print(f'    "License ici"')
    print(f"}},")
    print()
    print("// 3. Dans NkEmbeddedFontId (NkFontEmbedded.h) :")
    print(f"    {font_name} = N, // ajoutez à la fin")
    print()

# =============================================================================
# MAIN
# =============================================================================

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 EmbedFont.py <fichier.ttf> [NomSymbolique]")
        print()
        print("Exemples:")
        print("  python3 EmbedFont.py Resources/Fonts/ProggyClean.ttf")
        print("  python3 EmbedFont.py Resources/Fonts/Roboto-Regular.ttf Roboto")
        sys.exit(1)

    ttf_path  = sys.argv[1]
    font_name = sys.argv[2] if len(sys.argv) > 2 else \
                os.path.splitext(os.path.basename(ttf_path))[0] \
                  .replace('-','').replace('_','').replace(' ','')

    if not os.path.isfile(ttf_path):
        print(f"Erreur : fichier introuvable '{ttf_path}'")
        sys.exit(1)

    output_path = os.path.join(OUTPUT_DIR, f"{font_name}_data.h")
    generate_header(ttf_path, font_name, output_path)

    # Détecte grossièrement le type (bitmap si < 50Ko)
    size = os.path.getsize(ttf_path)
    kind = "Bitmap" if size < 60000 else "Vector"
    native = "13.f" if kind == "Bitmap" else "0.f"

    print_integration_snippet(font_name, kind, float(native.rstrip('f')))

if __name__ == '__main__':
    main()
