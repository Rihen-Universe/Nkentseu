// -----------------------------------------------------------------------------
// @File    NkComponentRegistry.cpp
// @Brief   Le registre des composants declares de la bibliotheque.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// POURQUOI IL EXISTE, ET POURQUOI SEULEMENT MAINTENANT
//   Le devis du 18/08 le declarait sans le definir, et le disait. Il a fallu le
//   definir des que `NKUIDesign` a du LISTER ce qui existe : une liste ecrite en
//   dur dans l'editeur aurait affiche les bons noms sans qu'une seule
//   declaration soit lue — c'est-a-dire aurait « marche » en ne prouvant rien.
//   Le registre est ce qui force l'editeur a passer par la declaration.
//
// CE QU'IL NE FAIT PAS, ET C'EST LA FRONTIERE
//   Il n'instancie rien, ne construit rien, ne possede rien. Il ENUMERE des
//   pointeurs vers des constantes de duree de vie statique. La fabrique par nom
//   (« construis-moi un objet de ce type ») est le domaine de `NKReflection`, et
//   elle y reste — cf. le bloc FRONTIERE de `NkComponentDecl.h`.
//
// ⚠️ AUCUNE ALLOCATION, PLAFOND FIXE. Un tableau statique de 64 pointeurs. Le
//    but n'est pas l'economie : c'est de n'avoir aucun ordre d'initialisation
//    statique a redouter, puisque `Register` peut etre appele depuis n'importe
//    quel constructeur global.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkComponentDecl.h"

namespace nkentseu {
	namespace editorkit {

		namespace {
			// « Construct on first use » applique a un tableau brut : les deux
			// variables sont des POD a initialisation constante, donc initialisees
			// avant tout code dynamique. C'est ce qui rend `Register` sur depuis un
			// constructeur global.
			const NkComponentDecl *gItems[NkComponentRegistry::kMaxComponents] = {};
			uint16 gCount = 0;
		} // namespace

		void NkComponentRegistry::Register(const NkComponentDecl &d) {
			if (!d.name || !d.name[0])
				return;
			// IDEMPOTENT SUR LE NOM. Sans ca, une declaration incluse depuis deux
			// unites de traduction apparaitrait deux fois dans la liste de
			// l'editeur — un defaut qui ne se verrait qu'a l'ecran, donc tard.
			for (uint16 i = 0; i < gCount; ++i)
				if (NkComponentDecl::StrEq(gItems[i]->name, d.name)) {
					gItems[i] = &d;
					return;
				}
			if (gCount >= kMaxComponents)
				return; // plafond atteint : on ignore, on n'ecrase pas
			gItems[gCount++] = &d;
		}

		uint16 NkComponentRegistry::Count() {
			return gCount;
		}

		const NkComponentDecl *NkComponentRegistry::At(uint16 i) {
			return (i < gCount) ? gItems[i] : nullptr;
		}

		const NkComponentDecl *NkComponentRegistry::Find(const char *name) {
			if (!name)
				return nullptr;
			for (uint16 i = 0; i < gCount; ++i)
				if (NkComponentDecl::StrEq(gItems[i]->name, name))
					return gItems[i];
			return nullptr;
		}

	} // namespace editorkit
} // namespace nkentseu
