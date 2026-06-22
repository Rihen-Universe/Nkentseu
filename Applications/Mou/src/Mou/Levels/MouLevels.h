// =============================================================================
// Levels/MouLevels.h
// Chargeur de niveaux JSON via le MOTEUR (NkJSONReader + NkArchive::GetObjectArray).
// Format : objet racine avec un tableau "levels" d'objets plats. Rééquilibrage
// SANS recompiler.
//
//   assets/levels/couleurs.json :
//   { "levels": [ { "colors": 2, "fruits": 3 }, { "colors": 3, "fruits": 4 } ] }
//
// PIÈGE corrigé : NkStringView(buf) sur un char[N] prend la longueur N-1 (zéros
// de fin inclus) -> le lecteur JSON croit à du "trailing content". Toujours passer
// NkStringView(buf, longueurReelle).
// =============================================================================
#pragma once

#ifndef MOU_LEVELS_H
#define MOU_LEVELS_H

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKSerialization/NkArchive.h"

namespace mou {

    class MouLevels {
    public:
        /// Charge assets/levels/<name> (ex. "couleurs.json"). @return nb de niveaux (0 si échec).
        nkentseu::int32 Load(const char* name) noexcept;
        nkentseu::int32 Count() const noexcept { return static_cast<nkentseu::int32>(mRecs.Size()); }

        /// Entier nommé du niveau, ou @p def si absent/hors-bornes.
        nkentseu::int32 GetInt(nkentseu::int32 level, const char* key, nkentseu::int32 def) const noexcept;
        /// Chaîne nommée -> out (taille cap). @return false si absente.
        bool GetStr(nkentseu::int32 level, const char* key, char* out, nkentseu::usize cap) const noexcept;

    private:
        nkentseu::NkVector<nkentseu::NkArchive> mRecs;
        nkentseu::int32 Clamp(nkentseu::int32 i) const noexcept;
    };

}  // namespace mou

#endif // MOU_LEVELS_H
