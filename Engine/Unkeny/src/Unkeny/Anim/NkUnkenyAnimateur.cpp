// =============================================================================
// NkUnkenyAnimateur.cpp — le pont entre NkAnimateur2D et la HFSM de NKAnima
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Unkeny/Anim/NkUnkenyAnimateur.h"

#include "NKECS/World/NkWorld.h"
#include "NKLogger/NkLog.h"
#include "NKMemory/NKMemory.h"
#include "Unkeny/Anim/NkUnkenyProprietes.h"
#include "Unkeny/Anim/NkUnkenySpriteAnim.h"
#include "Unkeny/Scene/NkUnkenyActif.h"

#include <cstring>

namespace nkentseu {
	namespace unkeny {

		using anim::NkAnimStateMachine;
		using Cond = anim::NkAnimStateMachine::NkCondKind;
		using Genre = anim::NkAnimStateMachine::NkParamKind;

		namespace {
			/// Copie bornee : les noms sont des tableaux FIXES, et une copie non
			/// bornee ecrirait dans le champ suivant.
			void CopierNom(char *dst, int32 taille, const char *src) noexcept {
				int32 i = 0;
				if (src != nullptr) {
					for (; i < taille - 1 && src[i] != '\0'; ++i) {
						dst[i] = src[i];
					}
				}
				dst[i] = '\0';
			}

			Genre VersNKAnima(NkGenreParamAnim g) noexcept {
				switch (g) {
					case NkGenreParamAnim::NK_BOOL:        return Genre::BOOL;
					case NkGenreParamAnim::NK_DECLENCHEUR: return Genre::TRIGGER;
					default:                               return Genre::FLOAT;
				}
			}

			NkGenreParamAnim DepuisNKAnima(Genre g) noexcept {
				switch (g) {
					case Genre::BOOL:    return NkGenreParamAnim::NK_BOOL;
					case Genre::TRIGGER: return NkGenreParamAnim::NK_DECLENCHEUR;
					default:             return NkGenreParamAnim::NK_REEL;
				}
			}

			NkAnimStateMachine ModelePlateforme() {
				NkAnimStateMachine m;
				const int32 sol = m.AddSubMachine("Sol");
				const int32 idle = m.AddEmptyState("idle", sol);
				const int32 marche = m.AddEmptyState("marche", sol);
				const int32 air = m.AddSubMachine("Air");
				const int32 saut = m.AddEmptyState("saut", air);
				const int32 chute = m.AddEmptyState("chute", air);
				m.SetStateTag(idle, 0);
				m.SetStateTag(marche, 1);
				m.SetStateTag(saut, 2);
				m.SetStateTag(chute, 3);
				// Declares d'abord, dans cet ordre : c'est celui de l'inspecteur.
				m.DeclareParam("vitesse", Genre::FLOAT, 0.f);
				m.DeclareParam("auSol", Genre::BOOL, 1.f);
				m.DeclareParam("saut", Genre::TRIGGER, 0.f);
				m.DeclareParam("vitesseVerticale", Genre::FLOAT, 0.f);
				// Fondus nuls : une image de sprite ne se fond pas.
				int32 t = m.AddTransitionEx(idle, marche, 0.f);
				m.AddCondition(t, "vitesse", Cond::FLOAT_GREATER, 0.1f);
				t = m.AddTransitionEx(marche, idle, 0.f);
				m.AddCondition(t, "vitesse", Cond::FLOAT_LESS, 0.1f);
				// Le saut l'emporte sur « plus au sol » : l'impulsion qui decolle
				// le personnage arrive souvent dans la meme trame que le saut.
				t = m.AddTransitionEx(sol, saut, 0.f, 1);
				m.AddCondition(t, "saut", Cond::TRIGGER);
				t = m.AddTransitionEx(sol, chute, 0.f);
				m.AddCondition(t, "auSol", Cond::BOOL_FALSE);
				t = m.AddTransitionEx(saut, chute, 0.f);
				m.AddCondition(t, "vitesseVerticale", Cond::FLOAT_LESS, 0.f);
				// Atterrir en courant : directement sur « marche », sans une
				// trame d'idle au milieu. Sinon, l'entree de Sol.
				t = m.AddTransitionEx(air, marche, 0.f, 1);
				m.AddCondition(t, "auSol", Cond::BOOL_TRUE);
				m.AddCondition(t, "vitesseVerticale", Cond::FLOAT_LESS, 0.01f);
				m.AddCondition(t, "vitesse", Cond::FLOAT_GREATER, 0.1f);
				t = m.AddTransitionEx(air, sol, 0.f);
				m.AddCondition(t, "auSol", Cond::BOOL_TRUE);
				m.AddCondition(t, "vitesseVerticale", Cond::FLOAT_LESS, 0.01f);
				return m;
			}

			struct NkModele {
					char nom[NK_UNKENY_ANIM_MODELE_MAX] = {};
					NkAnimStateMachine *machine = nullptr;
			};

			/// Le registre vit le temps du processus.
			///
			/// ⚠️ L'allocateur est TOUCHE dans le constructeur, et ce n'est pas
			/// decoratif : les statiques se detruisent dans l'ordre inverse de la
			/// FIN de leur construction. L'allocateur construit pendant la notre
			/// finit avant nous, donc meurt apres nous — nos Delete trouvent un
			/// allocateur vivant a la sortie du programme.
			struct NkRegistre {
					NkVector<NkModele> modeles;
					NkRegistre() {
						(void)memory::NkGetDefaultAllocator();
					}
					~NkRegistre() {
						for (uint32 i = 0; i < modeles.Size(); ++i) {
							memory::NkGetDefaultAllocator().Delete(modeles[i].machine);
						}
					}
			};

			NkModele *Trouver(NkRegistre &r, const char *nom) {
				if (nom == nullptr) {
					return nullptr;
				}
				for (uint32 i = 0; i < r.modeles.Size(); ++i) {
					if (std::strncmp(r.modeles[i].nom, nom, NK_UNKENY_ANIM_MODELE_MAX) == 0) {
						return &r.modeles[i];
					}
				}
				return nullptr;
			}

			bool Enregistrer(NkRegistre &r, const char *nom, const NkAnimStateMachine &machine) {
				if (nom == nullptr || nom[0] == '\0') {
					logger.Warn("[unkeny] modele d'animateur refuse : nom vide");
					return false;
				}
				if (NkModele *m = Trouver(r, nom)) {
					*m->machine = machine;
					return true;
				}
				NkModele m;
				CopierNom(m.nom, NK_UNKENY_ANIM_MODELE_MAX, nom);
				m.machine = memory::NkGetDefaultAllocator().New<NkAnimStateMachine>(machine);
				if (m.machine == nullptr) {
					logger.Error("[unkeny] modele d'animateur : allocation IMPOSSIBLE");
					return false;
				}
				r.modeles.PushBack(m);
				return true;
			}

			NkRegistre &Registre() {
				static NkRegistre r;
				static bool fourni = false;
				if (!fourni) {
					fourni = true;
					Enregistrer(r, "plateforme", ModelePlateforme());
				}
				return r;
			}

			/// Les parametres declares du modele, avec leurs defauts, dans le
			/// composant — pour que l'inspecteur les montre et que le jeu n'ait
			/// qu'a les modifier.
			void RemplirParams(NkAnimateur2D &a, const NkAnimStateMachine &m) {
				const uint32 n = m.GetParamCount();
				for (uint32 i = 0; i < n && a.nbParams < NK_UNKENY_ANIM_PARAMS_MAX; ++i) {
					if (a.Parametre(m.GetParamName(i).CStr()) >= 0) {
						continue;
					}
					NkParamAnimateur2D &p = a.params[a.nbParams++];
					CopierNom(p.nom, NK_UNKENY_ANIM_NOM_MAX, m.GetParamName(i).CStr());
					p.genre = DepuisNKAnima(m.GetParamKind(i));
					p.valeur = m.GetParamValue(i);
				}
			}
		} // namespace

		// =====================================================================
		int32 NkAnimateur2D::Parametre(const char *nom) const noexcept {
			if (nom == nullptr) {
				return -1;
			}
			for (int32 i = 0; i < nbParams; ++i) {
				if (std::strncmp(params[i].nom, nom, NK_UNKENY_ANIM_NOM_MAX) == 0) {
					return i;
				}
			}
			return -1;
		}

		namespace {
			bool Ecrire(NkAnimateur2D &a, const char *nom, NkGenreParamAnim genre, float32 v) noexcept {
				int32 i = a.Parametre(nom);
				if (i < 0) {
					if (nom == nullptr || a.nbParams >= NK_UNKENY_ANIM_PARAMS_MAX) {
						// On le DIT : un parametre qui disparait en silence donne
						// « le personnage ne saute jamais » sans aucune piste.
						logger.Warn("[unkeny] animateur : parametre '{0}' refuse (plus de place)", nom != nullptr ? nom : "");
						return false;
					}
					i = a.nbParams++;
					CopierNom(a.params[i].nom, NK_UNKENY_ANIM_NOM_MAX, nom);
					a.params[i].genre = genre;
				}
				a.params[i].valeur = v;
				return true;
			}
		} // namespace

		bool NkAnimateur2D::Poser(const char *nom, float32 v) noexcept {
			return Ecrire(*this, nom, NkGenreParamAnim::NK_REEL, v);
		}

		bool NkAnimateur2D::PoserBool(const char *nom, bool v) noexcept {
			return Ecrire(*this, nom, NkGenreParamAnim::NK_BOOL, v ? 1.f : 0.f);
		}

		bool NkAnimateur2D::Declencher(const char *nom) noexcept {
			return Ecrire(*this, nom, NkGenreParamAnim::NK_DECLENCHEUR, 1.f);
		}

		float32 NkAnimateur2D::Valeur(const char *nom) const noexcept {
			const int32 i = Parametre(nom);
			return i >= 0 ? params[i].valeur : 0.f;
		}

		NkAnimateur2D NkCreerAnimateur2D(const char *modele) {
			NkAnimateur2D a;
			CopierNom(a.modele, NK_UNKENY_ANIM_MODELE_MAX, modele);
			if (const NkAnimStateMachine *m = NkModeleAnimateur(modele)) {
				RemplirParams(a, *m);
			}
			return a;
		}

		// =====================================================================
		bool NkEnregistrerModeleAnimateur(const char *nom, const NkAnimStateMachine &machine) {
			return Enregistrer(Registre(), nom, machine);
		}

		bool NkChargerModeleAnimateur(const char *nom, const char *chemin) {
			if (chemin == nullptr) {
				return false;
			}
			NkAnimStateMachine m;
			// Sans resolveur : un modele d'animateur n'a que des etats vides
			// (voir l'en-tete) ; un clip nomme dedans resterait vide, et NKAnima
			// le dit.
			if (!m.LoadBinary(NkString(chemin))) {
				logger.Warn("[unkeny] modele d'animateur '{0}' non lu : {1}", nom != nullptr ? nom : "", chemin);
				return false;
			}
			return Enregistrer(Registre(), nom, m);
		}

		NkAnimStateMachine *NkModeleAnimateur(const char *nom) {
			NkModele *m = Trouver(Registre(), nom);
			return m != nullptr ? m->machine : nullptr;
		}

		uint32 NkNbModelesAnimateur() {
			return Registre().modeles.Size();
		}

		const char *NkNomModeleAnimateur(uint32 i) {
			NkRegistre &r = Registre();
			return i < r.modeles.Size() ? r.modeles[i].nom : nullptr;
		}

		NkString NkEtatAnimateur2D(const NkAnimateur2D &a) {
			const NkAnimStateMachine *m = NkModeleAnimateur(a.modele);
			NkString chemin;
			if (m == nullptr || a.execution.current < 0 || a.execution.current >= m->GetStateCount()) {
				return chemin;
			}
			// Du haut vers le bas : on remonte les parents, puis on ecrit a l'envers.
			int32 pile[NkAnimStateMachine::NK_MAX_DEPTH];
			int32 n = 0;
			for (int32 s = a.execution.current; s >= 0 && n < NkAnimStateMachine::NK_MAX_DEPTH; s = m->GetStateParent(s)) {
				pile[n++] = s;
			}
			for (int32 k = n - 1; k >= 0; --k) {
				if (k != n - 1) {
					chemin.Append("/");
				}
				chemin.Append(m->GetStateName(pile[k]).CStr());
			}
			return chemin;
		}

		// =====================================================================
		void NkAvancerAnimateurs(ecs::NkWorld &monde, float32 dt) {
			monde.Query<NkAnimateur2D>().ForEach([&](ecs::NkEntityId id, NkAnimateur2D &a) {
				// Une entite ETEINTE (NkUnkenyActif.h) ne s'anime pas.
				if (a.enPause || !NkEntiteActive(monde, id)) {
					return;
				}
				NkAnimStateMachine *m = NkModeleAnimateur(a.modele);
				if (m == nullptr) {
					if (!a.modeleAbsent) {
						logger.Warn("[unkeny] animateur : modele '{0}' inconnu (NkEnregistrerModeleAnimateur)", a.modele);
						a.modeleAbsent = true;
					}
					return;
				}
				a.modeleAbsent = false;
				if (a.nbParams == 0) {
					RemplirParams(a, *m);
				}

				// ── Le modele prend l'etat de CE personnage ──────────────────
				// Les parametres d'abord a leur defaut : sans cela, le « saut »
				// d'un personnage traite avant resterait pose pour celui-ci.
				m->ResetParams();
				int32 index[NK_UNKENY_ANIM_PARAMS_MAX];
				for (int32 i = 0; i < a.nbParams; ++i) {
					index[i] = m->FindParam(NkString(a.params[i].nom), VersNKAnima(a.params[i].genre));
					if (index[i] >= 0) {
						m->SetParamValue(static_cast<uint32>(index[i]), a.params[i].valeur);
					}
				}
				const int32 avant = a.execution.current;
				m->SetRuntime(a.execution);
				m->Update(dt);
				a.execution = m->GetRuntime();
				// Un declencheur consomme par une transition revient a 0 DANS le
				// composant : c'est ce que le jeu et l'inspecteur lisent.
				for (int32 i = 0; i < a.nbParams; ++i) {
					if (index[i] >= 0 && a.params[i].genre == NkGenreParamAnim::NK_DECLENCHEUR) {
						a.params[i].valeur = m->GetParamValue(static_cast<uint32>(index[i]));
					}
				}

				// ── L'etat -> le clip, seulement quand l'etat CHANGE ─────────
				const int32 etat = a.execution.current;
				if (etat == avant || etat < 0) {
					return;
				}
				// (2026-10-01) Un etat qui designe une ANIMATION par son nom (page
				// Animateur : « creer un etat a partir d'une animation ») la fait jouer
				// au clip de proprietes de l'entite, s'il y en a un.
				if (m->GetStateRefKind(etat) == 1 && !m->GetStateRef(etat).Empty()) {
					if (NkClipProprietes2D *cp = monde.Get<NkClipProprietes2D>(id)) {
						cp->Jouer(m->GetStateRef(etat).CStr());
					}
				}
				NkAnimSprite2D *s = monde.Get<NkAnimSprite2D>(id);
				if (s == nullptr) {
					return; // un animateur sans sprite anime : l'etat sert au jeu seul
				}
				const int32 clip = m->GetStateTag(etat);
				if (clip < 0 || clip >= s->nbClips) {
					logger.Warn("[unkeny] animateur : l'etat '{0}' veut le clip {1}, le sprite n'en a que {2}",
								m->GetStateName(etat).CStr(), clip, static_cast<int32>(s->nbClips));
					return;
				}
				s->Jouer(static_cast<uint8>(clip));
			});
		}

	} // namespace unkeny
} // namespace nkentseu
