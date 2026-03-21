# convert_to_nkentseu_format.py
import sys
import os
from datetime import datetime

def convert_spv_to_header(input_file, output_file, var_name=None):
    """
    Convertit un fichier SPIR-V en tableau C++ au format nkentseu
    """
    try:
        # Lire le fichier binaire
        with open(input_file, 'rb') as f:
            data = f.read()
        
        # Vérifier la taille
        if len(data) % 4 != 0:
            print(f"Attention: La taille ({len(data)} bytes) n'est pas multiple de 4")
            # Compléter avec des zéros si nécessaire
            padding = 4 - (len(data) % 4)
            data += b'\x00' * padding
            print(f"  -> Complété à {len(data)} bytes")
        
        # Générer le nom de variable si non spécifié
        if var_name is None:
            base_name = os.path.splitext(os.path.basename(input_file))[0]
            # Convertir en PascalCase/CamelCase pour le nom de variable
            var_name = 'kVkRHI' + base_name[0].upper() + base_name[1:] + 'Spv'
        
        # Convertir en uint32_t (little-endian)
        words = []
        for i in range(0, len(data), 4):
            if i + 3 < len(data):
                # Little-endian: octet 0 = poids faible
                val = (data[i+3] << 24) | (data[i+2] << 16) | (data[i+1] << 8) | data[i]
                words.append(f"0x{val:08X}")
        
        # Calculer le nombre de mots
        word_count = len(words)
        
        # Générer le header
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write(f"// Auto-generated from {input_file}\n")
            f.write(f"// File size: {len(data)} bytes\n")
            f.write(f"// Generated on {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"// Word count: {word_count}\n\n")
            
            f.write(f"static const nkentseu::uint32 {var_name}[] = {{\n")
            
            # Écrire par lignes de 8 valeurs
            for i in range(0, len(words), 8):
                line = words[i:i+8]
                # Formater avec exactement 8 valeurs par ligne, séparées par ", "
                formatted_line = "    " + ", ".join(line)
                if i + 8 < len(words):
                    formatted_line += ","
                f.write(formatted_line + "\n")
            
            f.write("};\n")
            f.write(f"static const nkentseu::uint32 {var_name}WordCount = (nkentseu::uint32)(sizeof({var_name})/sizeof({var_name}[0]));\n")
        
        print(f"✅ Conversion réussie!")
        print(f"   Entrée: {input_file} ({len(data)} bytes)")
        print(f"   Sortie: {output_file}")
        print(f"   Variable: {var_name}[{word_count}]")
        return True
        
    except Exception as e:
        print(f"❌ Erreur: {e}")
        return False

def main():
    if len(sys.argv) < 2:
        print("Usage: python convert_to_nkentseu_format.py input.spv [output.hpp] [variable_name]")
        print("\nExemples:")
        print("  python convert_to_nkentseu_format.py vert.spv")
        print("  python convert_to_nkentseu_format.py vert.spv shader.hpp")
        print("  python convert_to_nkentseu_format.py vert.spv shader.hpp kVkRHIMonShaderSpv")
        print("\nSi non spécifié, le nom de variable sera généré automatiquement:")
        print("  vert.spv  -> kVkRHIVertSpv")
        print("  frag.spv  -> kVkRHIFragSpv")
        print("  shadow.spv -> kVkRHIShadowSpv")
        sys.exit(1)
    
    input_file = sys.argv[1]
    
    # Vérifier que le fichier d'entrée existe
    if not os.path.exists(input_file):
        print(f"❌ Erreur: Le fichier {input_file} n'existe pas")
        sys.exit(1)
    
    # Déterminer le fichier de sortie
    if len(sys.argv) >= 3:
        output_file = sys.argv[2]
    else:
        # Par défaut: remplacer l'extension par .hpp
        output_file = os.path.splitext(input_file)[0] + ".hpp"
    
    # Déterminer le nom de variable
    if len(sys.argv) >= 4:
        var_name = sys.argv[3]
    else:
        var_name = None  # Sera généré automatiquement
    
    # Lancer la conversion
    if convert_spv_to_header(input_file, output_file, var_name):
        sys.exit(0)
    else:
        sys.exit(1)

if __name__ == "__main__":
    main()