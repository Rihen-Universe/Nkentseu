//
// NkEditeurMaillage.cpp
// =============================================================================
// Description :
//   Le maillage 2D dans l'editeur : CREER, les GESTES du modele (tous retenus,
//   Ctrl+Z), l'asset .nkmesh2d, les documents de la fenetre d'edition et les
//   actions (voir NkEditeurMaillage.h). La page elle-meme se dessine dans
//   NkEditeurPageMaillage.cpp.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurMaillage.h"

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurContenu.h"
#include "Editeur/NkEditeurDocuments.h"
#include "Editeur/NkEditeurInterface.h"
#include "Editeur/NkEditeurReferences.h"

#include "NKEditorKit/Components/NkContentBrowserDisque.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKMemory/NKMemory.h"
#include "Unkeny/Maillage/NkUnkenyMaillageFichier.h"
#include "Unkeny/Maillage/NkUnkenyMaillagePhysique.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		namespace {
			NkMaillage2D *Maillage(NkEditeurModele &m, ecs::NkEntityId id) {
				return m.scene.Monde().IsAlive(id) ? m.scene.Monde().Get<NkMaillage2D>(id) : nullptr;
			}

			/// Le maillage sur le TAS (6,5 Ko : la pile des gestes n'en a que faire).
			struct NkTasMaillage {
					NkMaillage2D *p = nullptr;
					NkTasMaillage() {
						p = memory::NkGetDefaultAllocator().New<NkMaillage2D>();
					}
					~NkTasMaillage() {
						memory::NkGetDefaultAllocator().Delete(p);
					}
			};

			NkString NomEntite(NkEditeurModele &m, ecs::NkEntityId id) {
				if (const NkEtiquette *e = m.scene.Monde().Get<NkEtiquette>(id)) {
					if (e->nom[0] != '\0') {
						return NkString(e->nom);
					}
				}
				return NkString("Maillage");
			}

			/// Un geste sur le maillage de `id` : RETENU, applique a une copie, ecrit
			/// si `geste` rend vrai. La copie protege d'un geste qui echoue a moitie.
			template <typename F> bool Geste(NkEditeurModele &m, ecs::NkEntityId id, F &&geste) {
				NkMaillage2D *ml = Maillage(m, id);
				if (ml == nullptr || m.etat != NkEtatJeu::NK_EDITION) {
					return false;
				}
				NkTasMaillage copie;
				if (copie.p == nullptr) {
					return false;
				}
				*copie.p = *ml;
				if (!geste(*copie.p)) {
					return false;
				}
				NkEditeurRetenir(m);
				// Relu APRES la photo (Photographier ne deplace rien, mais on ne garde
				// jamais un pointeur de composant a travers un geste de scene).
				ml = Maillage(m, id);
				if (ml == nullptr) {
					return false;
				}
				*ml = *copie.p;
				return true;
			}
		} // namespace

		// =====================================================================
		// CREER
		// =====================================================================
		bool NkEditeurCreerMaillage(NkEditeurModele &m, ecs::NkEntityId id, NkSourceMaillage source, int32 densite) {
			ecs::NkWorld &w = m.scene.Monde();
			if (!w.IsAlive(id) || m.etat != NkEtatJeu::NK_EDITION) {
				return false;
			}
			if (source == NkSourceMaillage::NK_AUTO) {
				source = w.Has<NkSprite2D>(id) ? NkSourceMaillage::NK_SPRITE
											   : (w.Has<NkRenduForme2D>(id) ? NkSourceMaillage::NK_FORME : NkSourceMaillage::NK_VIDE);
			}
			NkTasMaillage pm;
			if (pm.p == nullptr) {
				return false;
			}
			NkMaillage2D &ml = *pm.p;
			NkOptionsMaillage2D o;
			o.densite = densite;
			bool ok = false;
			const char *annonce = "";
			switch (source) {
				case NkSourceMaillage::NK_SPRITE: {
					const NkSprite2D *s = w.Get<NkSprite2D>(id);
					if (s == nullptr) {
						return false;
					}
					int32 tw = 0, th = 0;
					const uint8 *px = s->texId != 0u && m.textures.Taille(s->texId, tw, th) ? m.textures.Pixels(s->texId) : nullptr;
					ok = NkMaillageDepuisSprite2D(ml, *s, px, tw, th, o);
					annonce = px != nullptr ? "Maillage 2D cree depuis le contour du sprite (le sprite est masque ; Ctrl+Z annule)"
											: "Maillage 2D cree depuis le rectangle du sprite (sans image)";
					break;
				}
				case NkSourceMaillage::NK_FORME: {
					const NkRenduForme2D *f = w.Get<NkRenduForme2D>(id);
					ok = f != nullptr && NkMaillageDepuisForme2D(ml, *f, o);
					annonce = "Maillage 2D cree depuis la forme (la forme est masquee ; Ctrl+Z annule)";
					break;
				}
				default:
					NkMaillageVider2D(ml);
					ok = true;
					annonce = "Maillage 2D vide : posez ses sommets dans sa fenetre d'edition (outil Ajouter)";
					break;
			}
			if (!ok) {
				NkEditeurAnnoncer(m, source == NkSourceMaillage::NK_SPRITE ? "Maillage : l'image du sprite n'a aucun pixel opaque"
																			 : "Maillage : cette forme n'a pas d'interieur (une ligne ?)");
				return false;
			}
			NkEditeurRetenir(m);
			// Ce que le maillage REMPLACE a l'ecran est masque, pas retire.
			if (source == NkSourceMaillage::NK_SPRITE) {
				if (NkSprite2D *s = w.Get<NkSprite2D>(id)) {
					s->visible = false;
				}
			} else if (source == NkSourceMaillage::NK_FORME) {
				if (NkRenduForme2D *f = w.Get<NkRenduForme2D>(id)) {
					f->visible = false;
				}
			}
			if (w.Has<NkMaillage2D>(id)) {
				w.Set<NkMaillage2D>(id, ml);
			} else {
				w.Add<NkMaillage2D>(id, ml);
			}
			NkEditeurAnnoncer(m, annonce);
			return true;
		}

		// =====================================================================
		// LES GESTES
		// =====================================================================
		bool NkEditeurMaillageDeplacer(NkEditeurModele &m, ecs::NkEntityId id, const uint8 *choisis, const NkVec2f &delta) {
			if (choisis == nullptr) {
				return false;
			}
			return Geste(m, id, [&](NkMaillage2D &ml) {
				bool un = false;
				for (uint32 i = 0; i < ml.nbSommets; ++i) {
					if (choisis[i] != 0u) {
						ml.positions[i] = ml.positions[i] + delta;
						un = true;
					}
				}
				return un;
			});
		}

		int32 NkEditeurMaillageAjouter(NkEditeurModele &m, ecs::NkEntityId id, const NkVec2f &local) {
			int32 s = -1;
			Geste(m, id, [&](NkMaillage2D &ml) {
				s = NkMaillageAjouterSommet2D(ml, local);
				return s >= 0;
			});
			NkEditeurAnnoncer(m, s >= 0 ? "Sommet ajoute" : "Sommet refuse (deja un sommet ici, ou maillage plein : 128 sommets)");
			return s;
		}

		uint32 NkEditeurMaillageSupprimer(NkEditeurModele &m, ecs::NkEntityId id, const uint8 *choisis) {
			uint32 n = 0;
			Geste(m, id, [&](NkMaillage2D &ml) {
				n = NkMaillageRetirerSommets2D(ml, choisis);
				return n > 0u;
			});
			if (n > 0u) {
				NkEditeurAnnoncer(m, NkString::Format("%u sommet(s) supprime(s)", n).CStr());
			}
			return n;
		}

		int32 NkEditeurMaillageCouper(NkEditeurModele &m, ecs::NkEntityId id, uint32 a, uint32 b) {
			int32 s = -1;
			Geste(m, id, [&](NkMaillage2D &ml) {
				s = NkMaillageCouperArete2D(ml, a, b);
				return s >= 0;
			});
			if (s >= 0) {
				NkEditeurAnnoncer(m, "Arete coupee en son milieu");
			}
			return s;
		}

		bool NkEditeurMaillageTrianguler(NkEditeurModele &m, ecs::NkEntityId id) {
			const bool ok = Geste(m, id, [&](NkMaillage2D &ml) {
				const bool forme = ml.nbTriangles > 0u;
				return NkMaillageTrianguler2D(ml, forme);
			});
			NkEditeurAnnoncer(m, ok ? "Triangule (Delaunay, la forme gardee)" : "Trianguler : il faut au moins 3 sommets");
			return ok;
		}

		bool NkEditeurMaillageRefaire(NkEditeurModele &m, ecs::NkEntityId id, int32 densite) {
			const bool ok = Geste(m, id, [&](NkMaillage2D &ml) {
				NkOptionsMaillage2D o;
				o.densite = densite;
				return NkMaillageRefaire2D(ml, o);
			});
			NkEditeurAnnoncer(m, ok ? NkString::Format("Maillage refait a la densite %d (les parties reviennent a « Corps »)", densite).CStr()
									: "Refaire : il faut un maillage avec des triangles");
			return ok;
		}

		int32 NkEditeurMaillageFairePartie(NkEditeurModele &m, ecs::NkEntityId id, const uint8 *choisis) {
			int32 k = -1;
			Geste(m, id, [&](NkMaillage2D &ml) {
				k = NkMaillageFairePartie2D(ml, choisis);
				return k >= 0;
			});
			NkEditeurAnnoncer(m, k >= 0 ? NkString::Format("Partie %d faite : sa couture est dedoublee (choisissez sa physique a droite)", k).CStr()
										: "Faire une partie : choisissez des sommets qui ferment des triangles (8 parties au plus)");
			return k;
		}

		bool NkEditeurMaillageRetirerPartie(NkEditeurModele &m, ecs::NkEntityId id, uint32 k) {
			return Geste(m, id, [&](NkMaillage2D &ml) { return NkMaillageRetirerPartie2D(ml, k); });
		}

		bool NkEditeurMaillagePhysique(NkEditeurModele &m, ecs::NkEntityId id, uint32 k, NkPhysiquePartie2D p) {
			return Geste(m, id, [&](NkMaillage2D &ml) {
				if (k >= ml.nbParties || ml.parties[k].physique == static_cast<uint8>(p)) {
					return false;
				}
				ml.parties[k].physique = static_cast<uint8>(p);
				return true;
			});
		}

		int32 NkEditeurMaillageLier(NkEditeurModele &m, ecs::NkEntityId id, uint32 a, uint32 b, NkGenreLienParties2D g) {
			int32 l = -1;
			Geste(m, id, [&](NkMaillage2D &ml) {
				l = NkMaillageAjouterLien2D(ml, a, b, g);
				return l >= 0;
			});
			return l;
		}

		bool NkEditeurMaillageDelier(NkEditeurModele &m, ecs::NkEntityId id, uint32 k) {
			return Geste(m, id, [&](NkMaillage2D &ml) { return NkMaillageRetirerLien2D(ml, k); });
		}

		// =====================================================================
		// L'ASSET .nkmesh2d
		// =====================================================================
		NkString NkEditeurMaillageEnregistrerAsset(NkEditeurModele &m, ecs::NkEntityId id) {
			NkMaillage2D *ml = Maillage(m, id);
			if (ml == nullptr) {
				return NkString();
			}
			NkString dossier = NkEditeurDossierContenu(m);
			dossier.Append("/Maillages");
			NkDirectory::CreateRecursive(dossier.CStr());
			NkString nom = NomEntite(m, id);
			nom.Append(".");
			nom.Append(NkAssetExtensionFor(NkAssetType::Mesh2D));
			// Un fichier deja la n'est pas ecrase : un nom libre (« Gelee_2.nkmesh2d »).
			const NkString chemin = editorkit::NkDisqueCheminLibre(dossier.CStr(), nom.CStr());
			if (chemin.Empty() || !NkSauverMaillage2D(*ml, chemin.CStr(), m.RessourcesScene())) {
				NkEditeurAnnoncer(m, "Ecriture du maillage impossible");
				return NkString();
			}
			// La source du composant le retient (ce n'est pas un geste de forme : pas d'historique).
			const NkString nav = NkEditeurNavigateurDe(m, chemin.CStr());
			std::snprintf(ml->source, sizeof(ml->source), "%s", nav.Empty() ? chemin.CStr() : nav.CStr());
			NkEditeurAnnoncer(m, NkString::Format("Maillage enregistre : %s", nav.Empty() ? chemin.CStr() : nav.CStr()).CStr());
			return chemin;
		}

		bool NkEditeurMaillageUtiliserAsset(NkEditeurModele &m, ecs::NkEntityId id, const char *chemin) {
			if (chemin == nullptr || !m.scene.Monde().IsAlive(id) || m.etat != NkEtatJeu::NK_EDITION) {
				return false;
			}
			const NkString absolu = NkFile::Exists(chemin) ? NkString(chemin) : NkEditeurCheminContenuAbsolu(m, chemin);
			NkTasMaillage pm;
			NkString err;
			NkRessourcesScene r = m.RessourcesScene();
			if (pm.p == nullptr || !NkChargerMaillage2D(*pm.p, absolu.CStr(), m.scene, r, &err)) {
				NkEditeurAnnoncer(m, NkString::Format("Maillage illisible : %s", err.CStr()).CStr());
				return false;
			}
			const NkString nav = NkEditeurNavigateurDe(m, absolu.CStr());
			std::snprintf(pm.p->source, sizeof(pm.p->source), "%s", nav.Empty() ? absolu.CStr() : nav.CStr());
			NkEditeurRetenir(m);
			ecs::NkWorld &w = m.scene.Monde();
			if (w.Has<NkMaillage2D>(id)) {
				w.Set<NkMaillage2D>(id, *pm.p);
			} else {
				w.Add<NkMaillage2D>(id, *pm.p);
			}
			NkEditeurAnnoncer(m, NkString::Format("Maillage pose : %s", nav.CStr()).CStr());
			return true;
		}

		ecs::NkEntityId NkEditeurPoserMaillageAsset(NkEditeurModele &m, const char *chemin, const NkVec2f &position) {
			if (chemin == nullptr || m.etat != NkEtatJeu::NK_EDITION) {
				return ecs::NkEntityId::Invalid();
			}
			// Le nom de l'entite : celui du fichier, sans dossier ni extension.
			const char *debut = chemin;
			for (const char *p = chemin; *p != '\0'; ++p) {
				if (*p == '/' || *p == '\\') {
					debut = p + 1;
				}
			}
			char nom[32] = {};
			std::snprintf(nom, sizeof(nom), "%s", debut);
			if (char *point = std::strrchr(nom, '.')) {
				*point = '\0';
			}
			NkEditeurRetenir(m);
			const ecs::NkEntityId e = m.scene.Creer(nom, position);
			// Le pose sans une seconde photo : l'entite ET son maillage sont UN geste.
			const NkString absolu = NkFile::Exists(chemin) ? NkString(chemin) : NkEditeurCheminContenuAbsolu(m, chemin);
			NkTasMaillage pm;
			NkString err;
			NkRessourcesScene r = m.RessourcesScene();
			if (pm.p == nullptr || !NkChargerMaillage2D(*pm.p, absolu.CStr(), m.scene, r, &err)) {
				m.scene.Detruire(e);
				NkEditeurAnnoncer(m, NkString::Format("Maillage illisible : %s", err.CStr()).CStr());
				return ecs::NkEntityId::Invalid();
			}
			const NkString nav = NkEditeurNavigateurDe(m, absolu.CStr());
			std::snprintf(pm.p->source, sizeof(pm.p->source), "%s", nav.Empty() ? absolu.CStr() : nav.CStr());
			m.scene.Monde().Add<NkMaillage2D>(e, *pm.p);
			m.selection = e;
			m.aSelection = true;
			return e;
		}

		// =====================================================================
		// LES DOCUMENTS
		// =====================================================================
		NkDocMaillage *NkEditeurDocMaillage(NkEditeurInterface &ui, nk_uint64 id) {
			for (uint32 i = 0; i < ui.pagesMaillage.docs.Size(); ++i) {
				if (ui.pagesMaillage.docs[i].id == id) {
					return &ui.pagesMaillage.docs[i];
				}
			}
			return nullptr;
		}

		NkDocMaillage *NkEditeurDocMaillageActif(NkEditeurInterface &ui) {
			const NkDocumentOuvert d = NkEditeurDocumentActif(ui);
			return d.genre == NkGenreDocument::NK_MAILLAGE ? NkEditeurDocMaillage(ui, d.cle) : nullptr;
		}

		ecs::NkEntityId NkEditeurCibleMaillage(NkEditeurModele &m, const NkDocMaillage &d) {
			const ecs::NkEntityId e = d.cibleUid != 0u ? m.scene.EntiteParUid(d.cibleUid) : ecs::NkEntityId::Invalid();
			return e.IsValid() && m.scene.Monde().Has<NkMaillage2D>(e) ? e : ecs::NkEntityId::Invalid();
		}

		bool NkEditeurOuvrirMaillage(NkEditeurModele &m, NkEditeurInterface &ui, ecs::NkEntityId id) {
			if (Maillage(m, id) == nullptr) {
				NkEditeurAnnoncer(m, "Cette entite n'a pas de maillage 2D (Ajouter un composant > Maillage 2D)");
				return false;
			}
			const uint64 uid = m.scene.AssurerUid(id);
			for (uint32 i = 0; i < ui.pagesMaillage.docs.Size(); ++i) {
				if (ui.pagesMaillage.docs[i].cibleUid == uid) {
					return NkEditeurActiverDocument(m, ui, NkDoc(NkGenreDocument::NK_MAILLAGE, ui.pagesMaillage.docs[i].id));
				}
			}
			NkDocMaillage d;
			d.id = ui.pagesMaillage.prochainId++;
			d.cibleUid = uid;
			ui.pagesMaillage.docs.PushBack(d);
			return NkEditeurActiverDocument(m, ui, NkDoc(NkGenreDocument::NK_MAILLAGE, d.id));
		}

		bool NkEditeurDetruireDocMaillage(NkEditeurInterface &ui, nk_uint64 id) {
			for (uint32 i = 0; i < ui.pagesMaillage.docs.Size(); ++i) {
				if (ui.pagesMaillage.docs[i].id == id) {
					ui.pagesMaillage.docs.Erase(ui.pagesMaillage.docs.Begin() + i);
					return true;
				}
			}
			return false;
		}

		NkString NkEditeurLibelleMaillage(NkEditeurModele &m, const NkDocMaillage &d) {
			const ecs::NkEntityId e = d.cibleUid != 0u ? m.scene.EntiteParUid(d.cibleUid) : ecs::NkEntityId::Invalid();
			return NkString::Format("Maillage : %s", e.IsValid() ? NomEntite(m, e).CStr() : "(perdu)");
		}

		// =====================================================================
		// LES ACTIONS
		// =====================================================================
		bool NkEditeurActionMaillage(NkEditeurCadre &c, int32 action) {
			if (action < NK_A_MAILLAGE || action > NK_A_MAILLAGE_FIN) {
				return false;
			}
			NkEditeurModele &m = c.m;
			const bool sel = m.aSelection && m.scene.Monde().IsAlive(m.selection);
			if (action >= NK_A_MAILLAGE && action <= NK_A_MAILLAGE + static_cast<int32>(NkSourceMaillage::NK_VIDE)) {
				if (sel && NkEditeurCreerMaillage(m, m.selection, static_cast<NkSourceMaillage>(action - NK_A_MAILLAGE))) {
					NkEditeurOuvrirMaillage(m, c.ui, m.selection);
				}
				return true;
			}
			if (action == NK_A_MAILLAGE_EDITER) {
				if (sel) {
					NkEditeurOuvrirMaillage(m, c.ui, m.selection);
				}
				return true;
			}
			if (action == NK_A_MAILLAGE_ASSET) {
				if (sel) {
					NkEditeurMaillageEnregistrerAsset(m, m.selection);
					c.ui.maillagesFrais = false;
					c.ui.contenuPerime = true;
				}
				return true;
			}
			if (action >= NK_A_MAILLAGE_UTILISER && action < NK_A_MAILLAGE_UTILISER + 20) {
				const uint32 k = static_cast<uint32>(action - NK_A_MAILLAGE_UTILISER);
				if (sel && k < c.ui.maillagesProposes.Size()) {
					NkEditeurMaillageUtiliserAsset(m, m.selection, c.ui.maillagesProposes[k].CStr());
				}
				return true;
			}
			return true;
		}

	} // namespace editeur
} // namespace nkentseu
