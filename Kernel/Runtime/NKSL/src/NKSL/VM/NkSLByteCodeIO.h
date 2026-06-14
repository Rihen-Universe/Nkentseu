#pragma once
// =============================================================================
// NkSLByteCodeIO.h — Sérialisation du bytecode NkSL (.nkbc)
// =============================================================================
// Permet de TRANSFORMER un NkSLByteProgram en blob binaire (et inversement), pour
// le STOCKER dans un fichier `.nkbc`, le LIVRER comme asset, puis le CHARGER et
// l'INTERPRÉTER au runtime via NkSLVM — sans aucune recompilation ni toolchain.
//
//   compile NkSL → NkSLByteProgram → Serialize → fichier .nkbc
//   fichier .nkbc → Deserialize → NkSLByteProgram → NkSLVM::Execute
//
// Format : magic "NKBC" + version + stage + tables (code/constants/in/out/uni/samplers).
// Indépendant de l'endianness de la plateforme cible (écrit en little-endian).
// =============================================================================
#include "NKSL/VM/NkSLByteCode.h"

namespace nkentseu {

    // Sérialise `prog` en blob binaire (alloué dans `out`). true si OK.
    bool NkSLByteCodeSerialize(const NkSLByteProgram& prog, NkVector<uint8>& out);

    // Désérialise un blob (data,size) vers `prog`. false si magic/version invalide.
    bool NkSLByteCodeDeserialize(const uint8* data, usize size, NkSLByteProgram& prog);

    // Raccourcis fichier (via NKFileSystem). Retournent false en cas d'échec I/O.
    bool NkSLByteCodeSaveFile(const NkSLByteProgram& prog, const char* path);
    bool NkSLByteCodeLoadFile(const char* path, NkSLByteProgram& prog);

} // namespace nkentseu
