//
// NkUnkenyEffets.h
// =============================================================================
// Description :
//   Les particules VISUELLES d'une scene 2D : feu, fumee, etincelles, pluie,
//   neige, explosion. NkEmetteur2D (Scene/NkUnkenyComposants.h) porte la
//   recette ; NkEffets2D, possede par la scene, porte l'etat : les particules
//   vivantes et l'horloge de chaque emetteur.
//
// Caracteristiques :
//   - DETERMINISTE : chaque emetteur a son propre generateur (xorshift32) seme
//     par NkEmetteur2D::graine, et le temps avance par PAS FIXE (1/60 s). Meme
//     graine et meme temps ecoule : memes particules, au bit pres.
//   - PLAFONNE deux fois : par emetteur (`maxParticules`) et pour toute la
//     scene (`plafond`). Une naissance refusee est COMPTEE (Refusees()) : un
//     plafond qui mord sans rien dire passe pour un defaut du preset.
//   - Aucune allocation par trame une fois le reservoir a sa taille.
//   - Ne touche a RIEN de la simulation : ni corps, ni matiere, ni transform
//     (temoin f6 de la feuille de route).
//
// POURQUOI ICI, ET NON NKVFX (mesure du 2026-09-30)
//   NKVFX depend de NKRenderer (NkVertex3D) et Unkeny n'a pas le droit de le
//   tirer ; les emetteurs de Pong (std::rand, sans graine) et de Mou (420
//   particules figees, trois genres) sont des copies de jeu. Les valeurs des
//   presets reprennent celles de Noge (NkParticleEmitter.h), en metres.
//
// Ce fichier compile SANS NKGui : le dessin est dans Rendu/NkUnkenyRenduEffets.h.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYEFFETS_H__
#define __NKENTSEU_UNKENY_NKUNKENYEFFETS_H__

#include "NKContainers/Sequential/NkVector.h"
#include "NKCore/NkTypes.h"
#include "Unkeny/Scene/NkUnkenyComposants.h"

namespace nkentseu {
	namespace unkeny {

		class NkScene;

		/// Une particule vivante. Elle emporte ce qu'il faut pour vivre SANS son
		/// emetteur : une explosion dont l'entite est detruite finit sa course.
		struct NkParticuleEffet2D {
				NkVec2f pos{0.f, 0.f};
				NkVec2f vit{0.f, 0.f};
				NkVec2f gravite{0.f, 0.f};
				float32 frein = 0.f;
				float32 age = 0.f;
				float32 vie = 1.f;
				float32 taille0 = 0.1f;
				float32 taille1 = 0.1f;
				uint32 c0 = 0xFFFFFFFFu;
				uint32 c1 = 0xFFFFFFFFu;
				uint32 c2 = 0xFFFFFF00u;
				uint64 emetteur = 0u; ///< l'entite (NkEntityId::Pack) qui l'a emise
				NkFormeParticule2D forme = NkFormeParticule2D::NK_DOUCE;
				bool additif = false;
		};

		/// L'horloge d'un emetteur. Refaite (a la graine) si l'entite change
		/// d'identifiant : apres Restaurer, un effet repart de zero.
		struct NkEtatEmetteur2D {
				uint64 entite = 0u;
				uint32 alea = 1u;		   ///< etat du xorshift32
				float32 accumulateur = 0.f; ///< fraction de particule en attente
				float32 age = 0.f;			///< secondes depuis la naissance de l'effet
				float32 vacillement = 0.f;	///< 0..1, suit le hasard en douceur
				uint32 vivantes = 0u;		///< recompte a chaque pas
				bool rafaleFaite = false;
				bool vu = false;
		};

		class NkEffets2D {
			public:
				/// Plafond de TOUTE la scene, tous emetteurs confondus.
				uint32 plafond = 4096u;
				/// Le pas interne. Fixe : c'est ce qui rend l'effet independant de
				/// la cadence d'affichage, et le banc reproductible.
				float32 pasFixe = 1.f / 60.f;

				/// Oublie toutes les particules et toutes les horloges.
				void Vider();

				/// Avance de `dt` secondes, par pas fixes (8 au plus par appel : au-dela,
				/// le retard est jete, comme celui de la physique).
				void Avancer(NkScene &scene, float32 dt);

				/// Un seul pas fixe (le banc s'en sert pour compter juste).
				void Pas(NkScene &scene);

				/// Rejoue l'effet d'une entite depuis le debut (rafale comprise).
				void Rejouer(uint64 entite);

				const NkVector<NkParticuleEffet2D> &Particules() const noexcept {
					return mParticules;
				}
				uint32 NbParticules() const noexcept {
					return static_cast<uint32>(mParticules.Size());
				}
				/// Naissances refusees par un plafond depuis le dernier Vider.
				uint32 Refusees() const noexcept {
					return mRefusees;
				}
				/// L'horloge de l'emetteur porte par `entite`, ou nul.
				const NkEtatEmetteur2D *Etat(uint64 entite) const noexcept;

				/// Le facteur de la LUMIERE LIEE a un emetteur, 0..1 : l'activite de
				/// l'effet (une explosion s'eteint) fois son vacillement. Un emetteur
				/// en boucle jamais avance vaut 1 (le feu pose dans l'editeur
				/// eclaire deja) ; un effet a usage unique jamais joue vaut 0.
				float32 FacteurLumiere(uint64 entite, const NkEmetteur2D &e) const noexcept;

			private:
				NkEtatEmetteur2D &EtatDe(uint64 entite, uint32 graine);
				void Naitre(NkEtatEmetteur2D &etat, const NkEmetteur2D &e, const NkTransform2D &t);

				NkVector<NkParticuleEffet2D> mParticules;
				NkVector<NkEtatEmetteur2D> mEtats;
				float32 mAccumulateur = 0.f;
				uint32 mRefusees = 0u;
		};

		/// Les reglages d'un preset, prets a poser dans un NkEmetteur2D. La graine
		/// n'y est pas : elle reste celle de l'emetteur.
		NkEmetteur2D NkPresetEmetteur2D(NkPresetEffet2D preset);

		/// « Feu », « Fumée »... (UTF-8), pour l'editeur et les journaux.
		const char *NkNomPresetEffet2D(NkPresetEffet2D preset) noexcept;

	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYEFFETS_H__
