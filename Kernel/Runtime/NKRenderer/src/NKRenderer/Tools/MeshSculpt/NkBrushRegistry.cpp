#include "pch.h"
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/MeshSculpt/NkBrushRegistry.cpp
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   Implementation du registre des brosses.
// -----------------------------------------------------------------------------

#include "NKRenderer/Tools/MeshSculpt/NkBrushRegistry.h"

namespace nkentseu {
	namespace renderer {

		namespace {
			bool NameEq(const char *a, const char *b) noexcept {
				for (;; ++a, ++b) {
					if (*a != *b)
						return false;
					if (*a == 0)
						return true;
				}
			}
		} // namespace

		int32 NkBrushRegistry::Add(const NkBrushDesc &d) noexcept {
			// UNE BROSSE SANS IDENTITE N'ENTRE PAS. Sans ce refus, deux fichiers
			// fautifs apparaitraient comme deux lignes vides dans la liste, et la
			// seule facon de les distinguer serait leur position.
			if (!d.valid || d.name[0] == 0)
				return -1;

			for (uint32 i = 0; i < mItems.Size(); ++i)
				if (NameEq(mItems[i].name, d.name)) {
					mItems[i] = d; // REMPLACE : la surcharge utilisateur gagne
					return (int32)i;
				}
			mItems.PushBack(d);
			return (int32)(mItems.Size() - 1);
		}

		NkBrushParse NkBrushRegistry::AddFromText(const char *text, uint32 len, char *errBuf,
												  uint32 errCap) noexcept {
			NkBrushDesc d;
			const NkBrushParse r = ParseBrushDesc(text, len, d, errBuf, errCap);
			if (r != NkBrushParse::NK_BRUSH_OK)
				return r; // ⚠️ RIEN n'est ajoute sur un refus : pas de demi-brosse
			Add(d);
			return NkBrushParse::NK_BRUSH_OK;
		}

		bool NkBrushRegistry::At(uint16 i, NkBrushDesc &out) const noexcept {
			if ((uint32)i >= mItems.Size())
				return false;
			out = mItems[i]; // COPIE -- cf. l'en-tete : aucun pointeur ne sort d'ici
			return true;
		}

		int32 NkBrushRegistry::IndexOf(const char *name) const noexcept {
			if (!name)
				return -1;
			for (uint32 i = 0; i < mItems.Size(); ++i)
				if (NameEq(mItems[i].name, name))
					return (int32)i;
			return -1;
		}

	} // namespace renderer
} // namespace nkentseu
