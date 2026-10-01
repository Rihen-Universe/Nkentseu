#pragma once
// -----------------------------------------------------------------------------
// @File    NkContentBrowserInterne.h
// @Brief   LES AIDES DU DESSIN DU NAVIGATEUR, partagees par ses deux fichiers de
//          dessin : le mixte (`NkContentBrowserDraw.cpp`) et la variante Unreal
//          (`NkContentBrowserUnreal.cpp`). INTERNE au kit : aucune application
//          ne l'inclut.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// ⚠️ POURQUOI UN EN-TETE ET PAS UNE COPIE (2026-10-01) : ces fonctions vivaient
//    dans un espace ANONYME de `NkContentBrowserDraw.cpp`. La variante Unreal en a
//    besoin (filtre, tri, silhouettes, vignettes, pont de l'arbre) ; les recopier
//    aurait fait deux verites qui divergent au premier ajout de nature. Elles
//    sont seulement NOMMEES (espace `cbi`) et declarees ici ; leurs corps n'ont
//    pas bouge d'une ligne.
// ⚠️ MEME CLASSE DE NEUTRALITE que le modele : rien de NKGui n'entre ici.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkContentBrowserModel.h"

namespace nkentseu {
	namespace editorkit {

		/// Le dessin de la variante Unreal (defini dans `NkContentBrowserUnreal.cpp`).
		/// `NkDrawContentBrowser` y aiguille quand la variante effective l'est.
		NkContentBrowserResult NkDrawContentBrowserUnreal(NkComponentPaint &p, const NkComponentInput &in,
														  const NkPaintRect &rect, NkContentBrowserModel &m,
														  const NkContentBrowserStyle &s,
														  const NkContentBrowserHooks &hooks);

		namespace cbi {

			bool NkBrowserMemeChemin(const NkString &a, const NkString &b);
			bool PassesFilter(const NkAssetEntry &e, const char *filter);
			bool PassesKinds(const NkContentBrowserModel &m, const NkAssetEntry &e);
			const char *Label(const NkAssetEntry &e);
			bool SortBefore(const NkAssetEntry &a, const NkAssetEntry &b, NkBrowserTri cle);
			uint32 PutUInt(char *out, uint32 cap, uint32 at, uint32 v);
			uint32 PutStr(char *out, uint32 cap, uint32 at, const char *s);
			bool TextButton(NkComponentPaint &p, const NkComponentInput &in, const NkPaintRect &r,
							const char *label, uint16 bg, uint16 txt, uint16 outline, float32 rounding);
			const NkComponentInstance &EmbeddedTreeValues(bool defautOuvert);
			bool DrawVignette(NkComponentPaint &p, const NkPaintRect &r, const NkAssetEntry &e, int32 index,
							  const NkContentBrowserHooks &hooks);
			/// `rgba != 0` : la couleur CHOISIE prime sur le role (dossier colore).
			void Silhouette(NkComponentPaint &p, const NkPaintRect &r, NkAssetIcone genre, uint16 role,
							uint8 contenu = 0, uint32 rgba = 0);
			NkAssetIcone IconeDe(const NkAssetEntry &e);

			/// Le pont des evenements de l'arbre embarque vers ceux du navigateur :
			/// une selection de dossier EST une navigation.
			struct TreeBridge {
					NkContentBrowserResult *res = nullptr;
					const NkContentBrowserHooks *hooks = nullptr;
					const NkTreeViewModel *folders = nullptr;
			};
			void TreeOnSelect(void *user, int32 index, const char *id);
			void TreeOnMenu(void *user, int32 index, float32 x, float32 y);

		} // namespace cbi

	} // namespace editorkit
} // namespace nkentseu
