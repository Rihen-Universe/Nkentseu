#pragma once
// =============================================================================
// NkShaderIncludeResolver.h  — M.5 Material Functions
//
// Préprocesseur d'include pour shaders GLSL. Résout les directives :
//     #include "Include/NkFresnelSchlick.glsli"
//     #include <NkToonRamp.glsli>
// en inlinant récursivement le contenu du fichier référencé.
//
// Convention de recherche :
//   - Chemin absolu (commençant par "Resources/") : utilisé tel quel.
//   - Chemin commençant par "Include/" : relatif à "Resources/NKRenderer/Shaders/".
//   - Chemin nu (ex: "NkFresnelSchlick.glsli") : cherché dans
//     "Resources/NKRenderer/Shaders/Include/".
//
// Anti-cycle : un Set des fichiers déjà inclus est tenu pendant la résolution
// d'un même appel ; un fichier inclus 2 fois sera silencieusement skip.
//
// Les fichiers .glsli ne contiennent ni #version ni void main() — uniquement
// des fonctions/macros utilitaires GLSL réutilisables.
// =============================================================================
#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace renderer {

        class NkShaderIncludeResolver {
            public:
                // Résout les #include dans `source`. `currentFilePath` est le chemin
                // du fichier en cours (utilisé pour les includes relatifs au fichier).
                // Retourne la source avec les inclusions inlinées. Si une inclusion
                // échoue (fichier introuvable), elle est remplacée par un commentaire
                // d'erreur et un warning est logué.
                static NkString Resolve(const NkString& source,
                                        const NkString& currentFilePath = "");

            private:
                // Recherche le fichier à inclure parmi les chemins de fallback.
                // Retourne le chemin résolu (ou string vide si introuvable).
                static NkString ResolveIncludePath(const NkString& includeArg,
                                                    const NkString& currentFilePath);

                // Lit le contenu d'un fichier (sans strip d'annotations — les .glsli
                // ne sont pas censés contenir d'annotations @xxx).
                static NkString ReadRaw(const NkString& path);

                // Étape interne récursive avec tracking des fichiers déjà visités
                // (anti-cycle, pas de double-inclusion).
                static NkString ResolveRecursive(const NkString& source,
                                                  const NkString& currentFilePath,
                                                  NkVector<NkString>& visited,
                                                  int depth);
        };

    } // namespace renderer
} // namespace nkentseu
