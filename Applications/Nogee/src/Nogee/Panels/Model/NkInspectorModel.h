#pragma once
// =============================================================================
// Nogee/Panels/Model/NkInspectorModel.h — MODELE NEUTRE de l'inspecteur
// =============================================================================
// Aucune bibliotheque d'interface incluse. Cf. NkConsoleModel.h pour la raison
// d'etre de ce dossier.
//
// Contenu : quelles sections de composants sont dépliées, par nom. C'est l'etat
// que l'inspecteur garde entre deux images ; il ne depend pas du dessin.
//
// ⚠️ CE QUI RESTE AU PANNEAU : le choix du widget par TYPE de champ
// (`RenderField`) depend de la bibliotheque d'affichage et n'a rien a faire
// ici. Ce modele ne connait que des noms de sections et des booleens.
//
// Le panneau HERITE de ce modele (cf. NkSceneTreeModel.h pour le pourquoi).
// =============================================================================

#include "NKCore/NkTypes.h"
#include "Noge/ECS/NkEcsUtil.h" // NkStrEqual / NkStrNCpy — source unique du depot

namespace nkentseu {
	namespace noge {

		struct NkInspectorModel {
				static constexpr nk_uint32 kMaxSections = 32;

				char mSectionNames[kMaxSections][64] = {};
				bool mSectionOpen[kMaxSections] = {};
				nk_uint32 mSectionCount = 0;

				// Renvoie l'etat de la section ; l'ENREGISTRE ouverte si inconnue.
				bool IsSectionOpen(const char *name) noexcept {
					for (nk_uint32 i = 0; i < mSectionCount; ++i)
						if (NkStrEqual(mSectionNames[i], name))
							return mSectionOpen[i];
					// Nouvelle section — ouverte par defaut
					if (mSectionCount < kMaxSections) {
						NkStrNCpy(mSectionNames[mSectionCount], name, 63);
						mSectionOpen[mSectionCount] = true;
						++mSectionCount;
					}
					return true;
				}

				void SetSectionOpen(const char *name, bool open) noexcept {
					for (nk_uint32 i = 0; i < mSectionCount; ++i) {
						if (NkStrEqual(mSectionNames[i], name)) {
							mSectionOpen[i] = open;
							return;
						}
					}
				}
		};

	} // namespace noge
} // namespace nkentseu
