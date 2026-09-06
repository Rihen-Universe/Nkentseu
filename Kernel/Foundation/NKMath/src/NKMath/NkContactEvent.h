#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkContactEvent.h — un CONTACT PUBLIÉ (2026-09-06).
// ROADMAP_PRODUITS.md §6.6, palier 1 : « les contacts sont résolus, pas publiés ».
//
// Pourquoi ICI, dans NKMath : exactement la raison de NkIForceField.h, et la même
// géométrie de dépendances. Le fluide (NkSPHSolver) vit dans NKRenderer, le tissu
// (NkCloth) dans NKPhysics, et NKRenderer NE DÉPEND PAS de NKPhysics (mesuré le
// 06/09 : `rendererDeps` ne contient pas NKPhysics ; NKPhysics ne contient pas
// NKRenderer). La seule couche que les deux consomment est la Foundation. Un
// contact est une position, une normale et une vitesse : c'est de la géométrie.
//
// CONTRAT — trois choses, et la troisième est celle qu'on oublie :
//   1. `normal` est UNITAIRE et pointe DU SOLIDE VERS LE FLUIDE (la direction
//      dans laquelle la particule est repoussée). C'est la convention de
//      NkBodySDF (gradient de la distance signée, positif dehors).
//   2. `normalSpeed` est la vitesse d'APPROCHE, en m/s, TOUJOURS >= 0 : c'est
//      -(v_rel . n) mesuré AVANT la résolution. Un contact qui s'éloigne n'est
//      pas publié. Le consommateur n'a pas à connaître le signe du producteur.
//   3. La file est BORNÉE. Une file pleine DIT ce qu'elle a perdu (`Dropped()`)
//      -- elle ne perd rien en silence. Porte du dépôt : « un repli qui préserve
//      `success` n'est pas un repli, c'est un mensonge » ; ici, un `Push` qui
//      rend faux et un compteur qui monte.
//
// CE QUE CE FICHIER NE FAIT PAS : il ne consomme pas. La LOI d'éclaboussure
// (combien de gouttes, dans quelle direction) est dans NkSplashLaw.h, à côté ;
// le producteur SPH est dans NKRenderer/Tools/VFX/NkSPHSolver.cpp.
// =============================================================================
#include "NKMath/NkVec.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace math {

		// Un contact résolu par un solveur, tel qu'il est publié à qui veut le lire.
		struct NkContactEvent {
				NkVec3f position = {0.f, 0.f, 0.f}; // m, point de contact (sur la surface)
				NkVec3f normal = {0.f, 1.f, 0.f};	// unitaire, du SOLIDE vers le FLUIDE
				NkVec3f velocity = {0.f, 0.f, 0.f}; // m/s, vitesse relative COMPLÈTE avant résolution
				float32 normalSpeed = 0.f;			// m/s, vitesse d'APPROCHE = -(velocity . normal), >= 0
				float32 time = 0.f;					// s, horloge du producteur au moment du contact
				uint32 index = 0;					// indice de la particule chez le producteur
				uint32 surface = 0;					// identifiant de la surface touchée (0 = non renseigné)
		};

		// =====================================================================
		// File BORNÉE d'événements de contact.
		//
		// Un pas de simulation la vide (`Clear`) puis la remplit ; le consommateur
		// la lit entre deux pas. Aucune allocation en régime établi : la capacité
		// est réservée une fois.
		//
		// ⚠️ DÉBORDEMENT. `Push` rend FAUX quand la file est pleine et incrémente
		// `Dropped()`. `Total()` dit combien de contacts ont été PROPOSÉS sur le
		// pas -- c'est la population que le consommateur doit citer quand il
		// rapporte un chiffre : « 128 gouttes sur 300 contacts, dont 172 perdus »
		// n'est pas « 128 gouttes ».
		// =====================================================================
		class NkContactQueue {
			public:
				void Reserve(uint32 capacity) {
					mBuf.Resize((size_t)capacity);
					mCapacity = capacity;
					Clear();
				}

				// Vide la file ET remet les compteurs du pas à zéro.
				void Clear() noexcept {
					mCount = 0;
					mDropped = 0;
					mTotal = 0;
				}

				// Faux = la file était pleine (l'événement est PERDU, et compté).
				bool Push(const NkContactEvent &e) {
					++mTotal;
					if (mCount >= mCapacity) {
						++mDropped;
						return false;
					}
					mBuf[(size_t)mCount] = e;
					++mCount;
					return true;
				}

				uint32 Count() const noexcept {
					return mCount;
				}
				uint32 Dropped() const noexcept {
					return mDropped;
				}
				uint32 Total() const noexcept {
					return mTotal;
				}
				uint32 Capacity() const noexcept {
					return mCapacity;
				}
				bool Overflowed() const noexcept {
					return mDropped != 0;
				}
				const NkContactEvent *Data() const noexcept {
					return mBuf.Data();
				}
				const NkContactEvent &operator[](uint32 i) const noexcept {
					return mBuf[(size_t)i];
				}

			private:
				NkVector<NkContactEvent> mBuf;
				uint32 mCapacity = 0;
				uint32 mCount = 0;
				uint32 mDropped = 0;
				uint32 mTotal = 0;
		};

	} // namespace math
} // namespace nkentseu
