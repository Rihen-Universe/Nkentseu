#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkUnkenyBancTas.h — UNE SCENE DE BANC SUR LE TAS, UTILISEE COMME UNE LOCALE
//
// POURQUOI (2026-10-01) : une NkScene pese de l'ordre du Mo (monde ECS,
// physique, particules...), et en Debug le compilateur ne recouvre pas les
// emplacements des blocs successifs d'une meme fonction : la trame d'un banc
// est la SOMME de toutes ses scenes. NkUnkenyLancerBancStructure en declarait
// 37 (scenes, ressources, photos) ; apres la fusion du 2026-10-01 (temoins
// o1-o3 de l'entite active) sa trame a depasse les 32 Mo de pile donnes a
// Physic2D sous macOS : EXC_BAD_ACCESS dans ___chkstk_darwin, a l'entree, sans
// une ligne de sortie (CI macOS, run 36832302040).
//
// Ici l'objet vit sur le tas ; la locale devient une REFERENCE du meme nom
// (NK_BANC_SUR_TAS), si bien que le corps des temoins ne change pas d'une
// lettre : ce qu'ils testent reste exactement ce qu'ils testaient.
// =============================================================================

namespace nkentseu {
	namespace unkeny {

		template <typename T> class NkSurLeTas {
			public:
				NkSurLeTas() : mObjet(new T()) {
				}

				~NkSurLeTas() {
					delete mObjet;
				}

				NkSurLeTas(const NkSurLeTas &) = delete;
				NkSurLeTas &operator=(const NkSurLeTas &) = delete;

				T &operator*() {
					return *mObjet;
				}

			private:
				T *mObjet;
		};

	} // namespace unkeny
} // namespace nkentseu

// `NK_BANC_SUR_TAS(NkScene, s);` remplace `NkScene s;` : `s` reste une NkScene&.
#define NK_BANC_SUR_TAS(Type, nom)                                                                                     \
	::nkentseu::unkeny::NkSurLeTas<Type> nom##_surLeTas;                                                               \
	Type &nom = *nom##_surLeTas
