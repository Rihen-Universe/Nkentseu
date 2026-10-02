//
// NkEditeurSquelette.cpp
// =============================================================================
// Description :
//   Le squelette 2D dans l'editeur (voir NkEditeurSquelette.h) : CREER, les
//   GESTES du modele (tous retenus, Ctrl+Z), l'asset .nkskel, les actions, et
//   les os dessines dans la vue de la scene. Les outils de la fenetre (Os,
//   Poids, Pose, IK) sont dans NkEditeurPageMaillage.cpp.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurSquelette.h"

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurContenu.h"
#include "Editeur/NkEditeurInterface.h"
#include "Editeur/NkEditeurMaillage.h"
#include "Editeur/NkEditeurReferences.h"

#include "NKEditorKit/Components/NkContentBrowserDisque.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKMemory/NKMemory.h"
#include "NKSerialization/Asset/NkAssetMetadata.h"
#include "Unkeny/Maillage/NkUnkenyMaillagePhysique.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		using unkeny::NkSquelette2D;

		namespace {
			/// Le squelette et le maillage sur le TAS (la pile des gestes n'en a que faire).
			struct NkTasSquelette {
					NkSquelette2D *s = nullptr;
					NkMaillage2D *m = nullptr;
					NkTasSquelette() {
						s = memory::NkGetDefaultAllocator().New<NkSquelette2D>();
						m = memory::NkGetDefaultAllocator().New<NkMaillage2D>();
					}
					~NkTasSquelette() {
						memory::NkGetDefaultAllocator().Delete(s);
						memory::NkGetDefaultAllocator().Delete(m);
					}
			};

			NkSquelette2D *Squelette(NkEditeurModele &m, ecs::NkEntityId id) {
				return m.scene.Monde().IsAlive(id) ? m.scene.Monde().Get<NkSquelette2D>(id) : nullptr;
			}

			NkString NomEntite(NkEditeurModele &m, ecs::NkEntityId id) {
				if (const NkEtiquette *e = m.scene.Monde().Get<NkEtiquette>(id)) {
					if (e->nom[0] != '\0') {
						return NkString(e->nom);
					}
				}
				return NkString("Squelette");
			}

			/// Un geste sur le squelette (et la peau) de `id` : RETENU, applique a des
			/// copies, ecrit si `geste` rend vrai. `geste(squelette, maillage ou nul)`.
			template <typename F> bool Geste(NkEditeurModele &m, ecs::NkEntityId id, F &&geste) {
				NkSquelette2D *sq = Squelette(m, id);
				if (sq == nullptr || m.etat != NkEtatJeu::NK_EDITION) {
					return false;
				}
				NkTasSquelette copie;
				if (copie.s == nullptr || copie.m == nullptr) {
					return false;
				}
				*copie.s = *sq;
				const NkMaillage2D *ml = m.scene.Monde().Get<NkMaillage2D>(id);
				if (ml != nullptr) {
					*copie.m = *ml;
				}
				if (!geste(*copie.s, ml != nullptr ? copie.m : nullptr)) {
					return false;
				}
				NkEditeurRetenir(m);
				// Relus APRES la photo : on ne garde jamais un pointeur de composant a
				// travers un geste de scene.
				sq = Squelette(m, id);
				if (sq == nullptr) {
					return false;
				}
				*sq = *copie.s;
				if (ml != nullptr) {
					if (NkMaillage2D *vif = m.scene.Monde().Get<NkMaillage2D>(id)) {
						*vif = *copie.m;
					}
				}
				return true;
			}

			/// La boite ou poser un modele : celle du maillage, sinon du sprite, sinon un metre.
			void Boite(NkEditeurModele &m, ecs::NkEntityId id, NkVec2f &lo, NkVec2f &hi) {
				lo = NkVec2f(-0.5f, -0.5f);
				hi = NkVec2f(0.5f, 0.5f);
				if (const NkMaillage2D *ml = m.scene.Monde().Get<NkMaillage2D>(id)) {
					if (ml->nbSommets > 0u) {
						lo = hi = ml->positions[0];
						for (uint32 i = 1; i < ml->nbSommets; ++i) {
							lo = NkVec2f(math::NkMin(lo.x, ml->positions[i].x), math::NkMin(lo.y, ml->positions[i].y));
							hi = NkVec2f(math::NkMax(hi.x, ml->positions[i].x), math::NkMax(hi.y, ml->positions[i].y));
						}
						return;
					}
				}
				if (const NkSprite2D *s = m.scene.Monde().Get<NkSprite2D>(id)) {
					lo = NkVec2f(-s->pivot.x * s->taille.x, -(1.f - s->pivot.y) * s->taille.y);
					hi = NkVec2f(lo.x + s->taille.x, lo.y + s->taille.y);
				}
			}
		} // namespace

		// =====================================================================
		// CREER
		// =====================================================================
		bool NkEditeurCreerSquelette(NkEditeurModele &m, ecs::NkEntityId id, int32 modele) {
			ecs::NkWorld &w = m.scene.Monde();
			if (!w.IsAlive(id) || m.etat != NkEtatJeu::NK_EDITION) {
				return false;
			}
			// La PEAU d'abord : sans maillage, le squelette n'aurait rien a deformer.
			if (!w.Has<NkMaillage2D>(id) && (w.Has<NkSprite2D>(id) || w.Has<NkRenduForme2D>(id))) {
				NkEditeurCreerMaillage(m, id, NkSourceMaillage::NK_AUTO);
			}
			NkVec2f lo, hi;
			Boite(m, id, lo, hi);
			NkTasSquelette pt;
			if (pt.s == nullptr) {
				return false;
			}
			NkSquelette2D &s = *pt.s;
			if (modele >= 0 && modele < static_cast<int32>(anim::NkSkeleton2DTemplate::NK_COUNT)) {
				if (!unkeny::NkSqueletteModele(s, static_cast<anim::NkSkeleton2DTemplate>(modele), lo, hi)) {
					NkEditeurAnnoncer(m, "Squelette : le modele ne tient pas dans cette boite");
					return false;
				}
			}
			NkEditeurRetenir(m);
			if (w.Has<NkSquelette2D>(id)) {
				w.Set<NkSquelette2D>(id, s);
			} else {
				w.Add<NkSquelette2D>(id, s);
			}
			// Les POIDS : la chaleur (Baran et Popovic), relus sur la peau vive.
			if (NkMaillage2D *ml = w.Get<NkMaillage2D>(id)) {
				if (s.nbOs > 0u && ml->nbSommets > 0u) {
					unkeny::NkAutoPoidsChaleur2D(s, *ml);
				}
			}
			NkEditeurAnnoncer(m, modele >= 0 ? NkString::Format("Squelette 2D « %s » pose (%u os), poids par la chaleur ; Ctrl+Z annule",
																 anim::NkSkeleton2DTemplateName(static_cast<anim::NkSkeleton2DTemplate>(modele)),
																 static_cast<uint32>(s.nbOs))
														 .CStr()
											 : "Squelette 2D vide : posez ses os dans la fenetre du maillage (outil Os, B)");
			return true;
		}

		// =====================================================================
		// LES GESTES
		// =====================================================================
		int32 NkEditeurSqueletteAjouterOs(NkEditeurModele &m, ecs::NkEntityId id, int32 parent, const NkVec2f &tete, const NkVec2f &queue,
										  const char *nom) {
			int32 j = -1;
			Geste(m, id, [&](NkSquelette2D &s, NkMaillage2D *) {
				j = unkeny::NkSqueletteAjouterOs(s, nom, parent, tete, queue);
				return j >= 0;
			});
			return j;
		}

		bool NkEditeurSqueletteReposOs(NkEditeurModele &m, ecs::NkEntityId id, uint32 j, const NkVec2f &tete, const NkVec2f &queue) {
			return Geste(m, id, [&](NkSquelette2D &s, NkMaillage2D *) {
				if (j >= s.nbOs) {
					return false;
				}
				unkeny::NkSqueletteReposTeteQueue(s, j, tete, queue);
				return true;
			});
		}

		bool NkEditeurSqueletteRetirerOs(NkEditeurModele &m, ecs::NkEntityId id, uint32 j) {
			return Geste(m, id, [&](NkSquelette2D &s, NkMaillage2D *ml) { return unkeny::NkSqueletteRetirerOs(s, ml, j); });
		}

		bool NkEditeurSqueletteRenommer(NkEditeurModele &m, ecs::NkEntityId id, uint32 j, const char *nom) {
			return Geste(m, id, [&](NkSquelette2D &s, NkMaillage2D *) {
				if (j >= s.nbOs || nom == nullptr || nom[0] == '\0') {
					return false;
				}
				const int32 autre = unkeny::NkSqueletteTrouverOs(s, nom);
				if (autre >= 0 && autre != static_cast<int32>(j)) {
					return false; // deux os d'un meme nom : les clips ne sauraient lequel animer
				}
				std::snprintf(s.os[j].nom, sizeof(s.os[j].nom), "%s", nom);
				return true;
			});
		}

		uint32 NkEditeurSqueletteSymetrie(NkEditeurModele &m, ecs::NkEntityId id, uint32 j) {
			uint32 n = 0;
			Geste(m, id, [&](NkSquelette2D &s, NkMaillage2D *) {
				n = unkeny::NkSqueletteSymetrie(s, j);
				return n > 0u;
			});
			NkEditeurAnnoncer(m, n > 0u ? NkString::Format("Symetrie G/D : %u os en miroir", n).CStr()
										: "Symetrie : l'os choisi ne finit ni par G ni par D");
			return n;
		}

		bool NkEditeurSqueletteModele(NkEditeurModele &m, ecs::NkEntityId id, anim::NkSkeleton2DTemplate t) {
			NkVec2f lo, hi;
			Boite(m, id, lo, hi);
			return Geste(m, id, [&](NkSquelette2D &s, NkMaillage2D *ml) {
				if (!unkeny::NkSqueletteModele(s, t, lo, hi)) {
					return false;
				}
				if (ml != nullptr && ml->nbSommets > 0u) {
					unkeny::NkAutoPoidsChaleur2D(s, *ml);
				}
				return true;
			});
		}

		bool NkEditeurSqueletteRepos(NkEditeurModele &m, ecs::NkEntityId id) {
			return Geste(m, id, [&](NkSquelette2D &s, NkMaillage2D *) {
				unkeny::NkSqueletteRetourRepos(s);
				return true;
			});
		}

		bool NkEditeurChaineIK(const NkSquelette2D &s, uint32 saisi, bool parLeBout, uint32 &mid, NkVec2f &effecteur) {
			if (saisi >= s.nbOs) {
				return false;
			}
			if (parLeBout) {
				// La QUEUE de l'os : lui et son parent.
				if (s.os[saisi].parent < 0) {
					return false;
				}
				mid = saisi;
				effecteur = NkVec2f(s.os[saisi].longueur, 0.f);
				return true;
			}
			// La TETE de l'os : son parent et son grand-parent.
			const int32 p = s.os[saisi].parent;
			if (p < 0 || s.os[p].parent < 0) {
				return false;
			}
			mid = static_cast<uint32>(p);
			effecteur = NkVec2f(s.os[saisi].px, s.os[saisi].py);
			return true;
		}

		bool NkEditeurSqueletteIK(NkEditeurModele &m, ecs::NkEntityId id, uint32 saisi, bool parLeBout, const NkVec2f &cible, int32 coude) {
			bool atteinte = false;
			Geste(m, id, [&](NkSquelette2D &s, NkMaillage2D *) {
				uint32 mid = 0;
				NkVec2f eff;
				if (!NkEditeurChaineIK(s, saisi, parLeBout, mid, eff)) {
					return false;
				}
				const bool sens = coude < 0 ? unkeny::NkSqueletteCoudePositif2D(s, mid, eff) : coude == 1;
				atteinte = unkeny::NkSqueletteIK2D(s, mid, eff, cible, sens);
				return true;
			});
			return atteinte;
		}

		bool NkEditeurSqueletteTourner(NkEditeurModele &m, ecs::NkEntityId id, uint32 j, float32 angle) {
			return Geste(m, id, [&](NkSquelette2D &s, NkMaillage2D *) {
				if (j >= s.nbOs) {
					return false;
				}
				s.os[j].pangle = anim::NkWrapAngle(angle);
				return true;
			});
		}

		bool NkEditeurSqueletteAutoPoids(NkEditeurModele &m, ecs::NkEntityId id, int32 methode) {
			const bool ok = Geste(m, id, [&](NkSquelette2D &s, NkMaillage2D *ml) {
				if (ml == nullptr || s.nbOs == 0u) {
					return false;
				}
				if (methode == 0) {
					unkeny::NkAutoPoidsChaleur2D(s, *ml);
				} else if (methode == 1) {
					unkeny::NkAutoPoidsDistance2D(s, *ml);
				} else {
					return unkeny::NkPoidsParParties2D(s, *ml) > 0u;
				}
				return true;
			});
			NkEditeurAnnoncer(m, ok ? (methode == 0 ? "Poids par la CHALEUR (diffusion le long du maillage)"
													: (methode == 1 ? "Poids par la DISTANCE aux os" : "Poids : une partie = un os (meme nom)"))
									: "Poids : il faut un maillage, des os (et, par parties, une partie au nom d'un os)");
			return ok;
		}

		bool NkEditeurSqueletteNormaliser(NkEditeurModele &m, ecs::NkEntityId id) {
			return Geste(m, id, [&](NkSquelette2D &, NkMaillage2D *ml) {
				if (ml == nullptr) {
					return false;
				}
				unkeny::NkNormaliserPoids2D(*ml);
				return true;
			});
		}

		uint32 NkEditeurSquelettePinceau(NkEditeurModele &m, ecs::NkEntityId id, uint32 os, const NkVec2f &centre, float32 rayon, float32 force,
										 bool retirer, bool retenir) {
			if (m.etat != NkEtatJeu::NK_EDITION) {
				return 0u;
			}
			if (retenir) {
				NkEditeurRetenir(m);
			}
			NkMaillage2D *ml = m.scene.Monde().Get<NkMaillage2D>(id);
			const NkSquelette2D *s = Squelette(m, id);
			if (ml == nullptr || s == nullptr || os >= s->nbOs) {
				return 0u;
			}
			return unkeny::NkPeindrePoids2D(*ml, os, centre, rayon, force, retirer);
		}

		int32 NkEditeurSqueletteEmplacement(NkEditeurModele &m, ecs::NkEntityId id, uint32 os, int32 partie, const char *nom) {
			int32 e = -1;
			Geste(m, id, [&](NkSquelette2D &s, NkMaillage2D *ml) {
				const char *n = nom;
				if ((n == nullptr || n[0] == '\0') && ml != nullptr && partie >= 0 && static_cast<uint32>(partie) < ml->nbParties) {
					n = ml->parties[partie].nom;
				}
				e = unkeny::NkSqueletteAjouterEmplacement(s, n, static_cast<int32>(os), partie);
				return e >= 0;
			});
			return e;
		}

		bool NkEditeurSqueletteAttache(NkEditeurModele &m, ecs::NkEntityId id, uint32 emplacement, int32 partie) {
			return Geste(m, id, [&](NkSquelette2D &s, NkMaillage2D *) {
				if (emplacement >= s.nbEmplacements) {
					return false;
				}
				unkeny::NkEmplacement2D &e = s.emplacements[emplacement];
				for (uint32 a = 0; a < unkeny::NK_SQUELETTE2D_ATTACHES_MAX; ++a) {
					if (e.attaches[a] == partie) {
						return false;
					}
					if (e.attaches[a] < 0) {
						e.attaches[a] = static_cast<int8>(partie);
						return true;
					}
				}
				return false;
			});
		}

		int32 NkEditeurSqueletteChaine(NkEditeurModele &m, ecs::NkEntityId id, uint32 premier, uint32 nombre) {
			int32 k = -1;
			Geste(m, id, [&](NkSquelette2D &s, NkMaillage2D *) {
				k = unkeny::NkSqueletteAjouterChaine(s, premier, nombre);
				return k >= 0;
			});
			NkEditeurAnnoncer(m, k >= 0 ? "Chaine molle : en jeu, ces os pendent et suivent (corps mous XPBD)" : "Chaine molle : refusee (4 au plus)");
			return k;
		}

		bool NkEditeurSqueletteRetirerChaine(NkEditeurModele &m, ecs::NkEntityId id, uint32 k) {
			return Geste(m, id, [&](NkSquelette2D &s, NkMaillage2D *) {
				if (k >= s.nbChaines) {
					return false;
				}
				for (uint32 q = k; q + 1u < s.nbChaines; ++q) {
					s.chaines[q] = s.chaines[q + 1u];
				}
				s.chaines[--s.nbChaines] = unkeny::NkChaineMolle2D();
				return true;
			});
		}

		NkString NkEditeurSqueletteEnregistrerAsset(NkEditeurModele &m, ecs::NkEntityId id) {
			NkSquelette2D *s = Squelette(m, id);
			if (s == nullptr) {
				return NkString();
			}
			NkString dossier = NkEditeurDossierContenu(m);
			dossier.Append("/Squelettes");
			NkDirectory::CreateRecursive(dossier.CStr());
			NkString nom = NomEntite(m, id);
			nom.Append(".");
			nom.Append(NkAssetExtensionFor(NkAssetType::SkeletalMesh));
			const NkString chemin = editorkit::NkDisqueCheminLibre(dossier.CStr(), nom.CStr());
			if (chemin.Empty() || !unkeny::NkSauverSquelette2D(*s, m.scene.Monde().Get<NkMaillage2D>(id), chemin.CStr())) {
				NkEditeurAnnoncer(m, "Ecriture du squelette impossible");
				return NkString();
			}
			const NkString nav = NkEditeurNavigateurDe(m, chemin.CStr());
			std::snprintf(s->source, sizeof(s->source), "%s", nav.Empty() ? chemin.CStr() : nav.CStr());
			NkEditeurAnnoncer(m, NkString::Format("Squelette enregistre : %s", nav.Empty() ? chemin.CStr() : nav.CStr()).CStr());
			return chemin;
		}

		bool NkEditeurSqueletteUtiliserAsset(NkEditeurModele &m, ecs::NkEntityId id, const char *chemin) {
			if (chemin == nullptr || !m.scene.Monde().IsAlive(id) || m.etat != NkEtatJeu::NK_EDITION) {
				return false;
			}
			const NkString absolu = NkFile::Exists(chemin) ? NkString(chemin) : NkEditeurCheminContenuAbsolu(m, chemin);
			NkTasSquelette pt;
			if (pt.s == nullptr || !unkeny::NkChargerSquelette2D(*pt.s, m.scene.Monde().Get<NkMaillage2D>(id), absolu.CStr())) {
				NkEditeurAnnoncer(m, "Squelette illisible (.nkskel)");
				return false;
			}
			const NkString nav = NkEditeurNavigateurDe(m, absolu.CStr());
			std::snprintf(pt.s->source, sizeof(pt.s->source), "%s", nav.Empty() ? absolu.CStr() : nav.CStr());
			NkEditeurRetenir(m);
			ecs::NkWorld &w = m.scene.Monde();
			if (w.Has<NkSquelette2D>(id)) {
				w.Set<NkSquelette2D>(id, *pt.s);
			} else {
				w.Add<NkSquelette2D>(id, *pt.s);
			}
			return true;
		}

		// =====================================================================
		// LES OS DANS LA VUE DE LA SCENE
		// =====================================================================
		void NkEditeurDessinerOsScene(NkEditeurCadre &c) {
			NkEditeurModele &m = c.m;
			if (!m.aSelection || !m.scene.Monde().IsAlive(m.selection)) {
				return;
			}
			const NkSquelette2D *s = m.scene.Monde().Get<NkSquelette2D>(m.selection);
			const NkTransform2D *t = m.scene.Monde().Get<NkTransform2D>(m.selection);
			if (s == nullptr || t == nullptr || s->nbOs == 0u || (m.etat != NkEtatJeu::NK_EDITION && !s->osVisibles)) {
				return;
			}
			auto &dl = c.ctx.dl;
			const NkVue2D &cam = m.scene.Camera();
			math::NkMat4f monde[unkeny::NK_SQUELETTE2D_OS_MAX];
			unkeny::NkSqueletteMondePose(*s, monde);
			for (uint32 j = 0; j < s->nbOs; ++j) {
				const NkVec2f a = cam.MondeVersEcran(t->VersMonde(unkeny::NkSqueletteTete(monde, j)));
				const NkVec2f b = cam.MondeVersEcran(t->VersMonde(unkeny::NkSqueletteQueue(*s, monde, j)));
				const float32 dx = b.x - a.x, dy = b.y - a.y;
				const float32 l = std::sqrt(dx * dx + dy * dy);
				if (l < 1.f) {
					dl.AddCircleFilled(nkgui::NkVec2{a.x, a.y}, 3.f, nkgui::NkColor{235, 235, 245, 200});
					continue;
				}
				const float32 ux = dx / l, uy = dy / l;
				const float32 lw = math::NkClamp(l * 0.12f, 2.5f, 7.f);
				const nkgui::NkVec2 p0{a.x, a.y}, p2{b.x, b.y};
				const nkgui::NkVec2 p1{a.x + ux * l * 0.2f - uy * lw, a.y + uy * l * 0.2f + ux * lw};
				const nkgui::NkVec2 p3{a.x + ux * l * 0.2f + uy * lw, a.y + uy * l * 0.2f - ux * lw};
				const nkgui::NkColor fond{205, 215, 240, 110}, trait{235, 240, 255, 220};
				dl.AddTriangleFilled(p0, p1, p2, fond);
				dl.AddTriangleFilled(p0, p2, p3, fond);
				dl.AddLine(p0, p1, trait, 1.f);
				dl.AddLine(p1, p2, trait, 1.f);
				dl.AddLine(p2, p3, trait, 1.f);
				dl.AddLine(p3, p0, trait, 1.f);
				dl.AddCircleFilled(p0, 2.5f, trait);
			}
		}

		// =====================================================================
		// LES ACTIONS
		// =====================================================================
		bool NkEditeurActionSquelette(NkEditeurCadre &c, int32 action) {
			if (action < NK_A_SQUELETTE || action > NK_A_SQUELETTE_FIN) {
				return false;
			}
			NkEditeurModele &m = c.m;
			const bool sel = m.aSelection && m.scene.Monde().IsAlive(m.selection);
			auto Ouvrir = [&]() {
				if (NkEditeurOuvrirMaillage(m, c.ui, m.selection)) {
					if (NkDocMaillage *d = NkEditeurDocMaillageActif(c.ui)) {
						d->outil = NkOutilMaillage::NK_OS;
					}
				}
			};
			if (action >= NK_A_SQUELETTE && action <= NK_A_SQUELETTE_VIDE) {
				const int32 modele = action == NK_A_SQUELETTE_VIDE ? -1 : action - NK_A_SQUELETTE;
				if (sel && NkEditeurCreerSquelette(m, m.selection, modele)) {
					Ouvrir();
				}
				return true;
			}
			if (action == NK_A_SQUELETTE_EDITER) {
				if (sel) {
					Ouvrir();
				}
				return true;
			}
			if (action == NK_A_SQUELETTE_ASSET) {
				if (sel) {
					NkEditeurSqueletteEnregistrerAsset(m, m.selection);
					c.ui.contenuPerime = true;
				}
				return true;
			}
			if (action == NK_A_SQUELETTE_REPOS) {
				if (sel) {
					NkEditeurSqueletteRepos(m, m.selection);
				}
				return true;
			}
			return true;
		}

	} // namespace editeur
} // namespace nkentseu
