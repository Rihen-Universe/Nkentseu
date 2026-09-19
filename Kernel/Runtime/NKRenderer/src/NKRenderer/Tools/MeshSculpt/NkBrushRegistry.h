#pragma once
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/MeshSculpt/NkBrushRegistry.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   Le registre des brosses : ce que l'interface ENUMERE, et l'unique
//          porte par laquelle une brosse existe.
//
// POURQUOI IL EXISTE
//   Pour la meme raison que `NkComponentRegistry` dans NKEditorKit : « une liste
//   ecrite en dur dans l'editeur aurait affiche les bons noms sans qu'une seule
//   declaration soit lue -- c'est-a-dire aurait MARCHE en ne prouvant rien ».
//   Le registre est ce qui FORCE l'interface a passer par la donnee.
//
// ⚠️ CE QU'IL FAIT DIFFEREMMENT DU REGISTRE DU KIT, ET POURQUOI
//   `NkComponentRegistry::Register` fait `gItems[gCount++] = &d;` -- il garde un
//   POINTEUR. C'est correct chez lui : il enumere des declarations STATIQUES,
//   compilees, qui vivent aussi longtemps que le programme. Et c'est la faute
//   que le depot a payee en defaut de segmentation MOUVANT le 01/09, quand une
//   declaration est arrivee par valeur et que le registre a survecu au
//   temporaire.
//
//   ICI LA SITUATION EST INVERSE : les descripteurs viennent de FICHIERS. Ils
//   n'ont aucune duree de vie statique a offrir. Copier le patron du kit ici
//   serait donc reproduire la faute plutot que la lecon. Le registre POSSEDE
//   ses descripteurs (`NkVector<NkBrushDesc>`, POD copiable, aucune allocation
//   interne), et
//
//     ⚠️ IL NE REND JAMAIS DE POINTEUR VERS SON STOCKAGE.
//
//   Un `const NkBrushDesc*` vers l'interieur d'un `NkVector` serait invalide par
//   le prochain `Add` qui reallouerait -- un pointeur pendant dont la victime
//   serait l'appelant, pas nous. `At()` rend donc une COPIE. Le descripteur
//   fait quelques centaines d'octets : le cout est reel et il est le prix de ne
//   pas avoir de question de duree de vie du tout.
//
// ⚠️ PAS DE SINGLETON, ET C'EST DELIBERE. Le registre du kit est statique parce
//    que ses declarations s'enregistrent depuis des constructeurs globaux. Ici
//    le chargement est EXPLICITE au lancement : une instance possedee par
//    l'application suffit, et elle rend le registre eprouvable dans un banc
//    sans etat global a remettre a zero entre deux cas.
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKRenderer/Tools/MeshSculpt/NkBrushDesc.h"

namespace nkentseu {
	namespace renderer {

		class NkBrushRegistry {
			public:
				// ⚠️ PAS DE PLAFOND FIXE. Celui du kit (64) a deja coute une fois :
				//    « supprimer le doublon aurait remis le plafond de 64 imports ».
				//    Un catalogue de brosses est exactement ce qui grandit quand
				//    l'outil sert.
				NkBrushRegistry() noexcept = default;

				// Ajoute ou REMPLACE (idempotent sur `name`). Rend l'index, ou -1
				// si le descripteur est invalide.
				// Idempotent parce qu'une brosse de l'utilisateur qui reprend le nom
				// d'une brosse livree doit la REMPLACER dans la liste -- c'est ce
				// qu'attend quelqu'un qui personnalise « Dessiner », et c'est deja
				// la regle des themes du produit.
				int32 Add(const NkBrushDesc &d) noexcept;

				// Lit un descripteur depuis du TEXTE et l'ajoute. Rend le motif de
				// refus ; en cas de refus, RIEN n'est ajoute.
				NkBrushParse AddFromText(const char *text, uint32 len, char *errBuf, uint32 errCap) noexcept;

				[[nodiscard]] uint16 Count() const noexcept {
					return (uint16)mItems.Size();
				}

				// Rend une COPIE (cf. l'en-tete). false si l'index est hors borne.
				[[nodiscard]] bool At(uint16 i, NkBrushDesc &out) const noexcept;

				// Index de la brosse portant ce nom, ou -1.
				[[nodiscard]] int32 IndexOf(const char *name) const noexcept;

				void Clear() noexcept {
					mItems.Clear();
				}

			private:
				NkVector<NkBrushDesc> mItems;
		};

	} // namespace renderer
} // namespace nkentseu
