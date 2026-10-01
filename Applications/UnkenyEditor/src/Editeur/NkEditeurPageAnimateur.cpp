//
// NkEditeurPageAnimateur.cpp
// =============================================================================
// Description :
//   LA PAGE ANIMATEUR : un controleur d'animation (.nkanimctl, la machine a
//   etats HIERARCHIQUE de NKAnima) dessine en GRAPHE par le composant du kit
//   (NKEditorKit/Components/NkStateGraphModel.h).
//     - en-tete : le document, l'entite observee, Jouer / Arreter, « Appliquer
//       a l'entite », Enregistrer ;
//     - le graphe : etats (crees depuis les animations du Contenu), transitions,
//       conditions, parametres, sous-machines ;
//     - EN JEU : l'etat actif de l'entite s'allume, le fondu se lit, la
//       transition qui tire clignote, et les parametres se reglent EN DIRECT
//       (ils vont au NkAnimateur2D de l'entite) ; un apercu montre l'entite.
//
// ⚠️ LE MODELE QUE JOUE L'ENTITE EST LE GRAPHE, A CHAQUE CHANGEMENT : le graphe
//    est recompile (NkMachineDepuisGraphe) et ENREGISTRE sous le nom du
//    document (NkEnregistrerModeleAnimateur). Une entite dont le NkAnimateur2D
//    porte ce nom voit donc ce qu'on dessine, sans passer par le disque.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurInterface.h"
#include "Editeur/NkEditeurPagesAnim.h"
#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKEditorKit/Components/NkGuiComponentPaint.h"
#include "NKEditorKit/NkEditorTextField.h"
#include "Unkeny/Anim/NkUnkenyAnimateur.h"
#include "Unkeny/Anim/NkUnkenyProprietes.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		using nkgui::NkColor;
		using nkgui::NkRect;
		using nkgui::NkVec2;
		using editorkit::NkRole;

		namespace {
			constexpr float32 HAUTEUR_EN_TETE = 34.f;

			editorkit::NkStateGraphStyle StyleGraphe() {
				editorkit::NkStateGraphStyle s;
				s.canvasBg = (uint16)NkRole::CanvasBg;
				s.gridLine = (uint16)NkRole::GridLine;
				s.panelBg = (uint16)NkRole::PanelBg;
				s.headerBg = (uint16)NkRole::PanelHeader;
				s.border = (uint16)NkRole::Border;
				s.text = (uint16)NkRole::Text;
				s.textMuted = (uint16)NkRole::TextMuted;
				s.textOnAccent = (uint16)NkRole::TextOnAccent;
				s.accent = (uint16)NkRole::AccentUi;
				s.nodeBody = (uint16)NkRole::NodeBody;
				s.nodeHeader = (uint16)NkRole::NodeDataHeader;
				s.nodeEntry = (uint16)NkRole::AccentSel;
				// Une sous-machine ne se confond pas avec l'etat d'ENTREE (ambre).
				s.subMachine = (uint16)NkRole::AccentAI;
				s.wire = (uint16)NkRole::NodeWire;
				s.pseudoEntry = (uint16)NkRole::StatusOk;
				s.pseudoAny = (uint16)NkRole::TypeAnim;
				s.paramBool = (uint16)NkRole::AccentUi;
				s.paramFloat = (uint16)NkRole::StatusOk;
				s.paramTrigger = (uint16)NkRole::AccentSel;
				s.buttonBg = (uint16)NkRole::ButtonBg;
				s.inputBg = (uint16)NkRole::InputBg;
				s.blendTree = (uint16)NkRole::TypeAnim; // (01/10 soir) un arbre de melange
				return s;
			}

			NkString NomDe(NkEditeurModele &m, ecs::NkEntityId id) {
				const NkEtiquette *e = m.scene.Monde().IsAlive(id) ? m.scene.Monde().Get<NkEtiquette>(id) : nullptr;
				return e != nullptr && e->nom[0] != '\0' ? NkString(e->nom) : NkString("(sans nom)");
			}

			/// Le graphe COMPILE et enregistre sous le nom du document : c'est ce
			/// que joue une entite qui porte ce modele (voir l'en-tete).
			void Compiler(NkDocAnim &d) {
				// (01/10 soir) un CONTROLEUR : la base, ses couches, ses arbres de melange.
				anim::NkAnimController controleur;
				if (NkControleurDepuisGraphe(d.graphe, controleur, d.noeudDeEtat)) {
					unkeny::NkEnregistrerControleurAnimateur(d.nom.CStr(), controleur);
				}
			}

			/// Le NkAnimateur2D de l'entite observee, s'il joue CE controleur.
			unkeny::NkAnimateur2D *AnimateurObserve(NkEditeurModele &m, const NkDocAnim &d) {
				const ecs::NkEntityId id = NkEditeurCibleDoc(m, d);
				unkeny::NkAnimateur2D *a = m.scene.Monde().IsAlive(id) ? m.scene.Monde().Get<unkeny::NkAnimateur2D>(id) : nullptr;
				if (a == nullptr || std::strncmp(a->modele, d.nom.CStr(), unkeny::NK_UNKENY_ANIM_MODELE_MAX - 1) != 0) {
					return nullptr;
				}
				return a;
			}

			/// Le champ de renommage, par-dessus tout. Entree valide, Echap annule,
			/// un clic ailleurs valide (le contrat de NkOverlayTextField).
			void Renommer(NkEditeurCadre &c, NkDocAnim &d) {
				if (!d.renomme) {
					return;
				}
				if (d.renommeChoisir) {
					// Tout le nom CHOISI : la frappe le remplace (comme F2 partout).
					c.ctx.input.wantSelectAll = true;
					d.renommeChoisir = false;
				}
				const nkgui::NkGuiInput &in = c.ctx.input;
				const NkRect r{d.renommeRect.x, d.renommeRect.y + (d.renommeRect.h - 22.f) * 0.5f,
							   d.renommeRect.w > 120.f ? d.renommeRect.w : 120.f, 22.f};
				auto &dl = c.ctx.dlOverlay;
				dl.AddRectFilled(r, c.pal.champ, 3.f);
				dl.AddRect(r, c.pal.accent, 1.f, 3.f);
				editorkit::NkOverlayFieldStyle st;
				st.fond = false;
				st.bord = false;
				st.texte = c.pal.texte;
				st.utf8 = true;
				editorkit::NkOverlayTextField(c.ctx, dl, c.police, NkRect{r.x + 5.f, r.y, r.w - 8.f, r.h}, d.tampon,
											  static_cast<int32>(sizeof(d.tampon)), true, &st);
				const bool valider = in.KeyPressed(nkgui::NkGuiKey::Enter) || (in.mouseClicked[0] && !NkEditeurDans(r, in.mousePos));
				const bool annuler = in.KeyPressed(nkgui::NkGuiKey::Escape);
				if (annuler) {
					d.renomme = false;
				} else if (valider) {
					const NkString nom(d.tampon);
					bool ok = false;
					if (d.renommeParam >= 0) {
						ok = d.graphe.RenameParam(static_cast<uint32>(d.renommeParam), nom);
					} else if (d.renommeNoeud != 0) {
						ok = d.graphe.RenameNode(d.renommeNoeud, nom);
					}
					if (!ok && !nom.Empty()) {
						NkEditeurAnnoncer(c.m, NkString::Format("« %s » : ce nom est deja pris a ce niveau", nom.CStr()).CStr());
					}
					d.renomme = false;
				}
			}
		} // namespace

		// =====================================================================
		// L'ANIMATEUR EN JEU
		// =====================================================================
		void NkEditeurSuivreAnimateur(NkEditeurCadre &c, NkDocAnim &d) {
			NkEditeurModele &m = c.m;
			editorkit::NkStateGraphModel &g = d.graphe;
			d.modifie = g.revision != d.revisionEnregistree || d.chemin.Empty();
			g.firedAge += c.ui.dt;
			unkeny::NkAnimateur2D *a = m.etat != NkEtatJeu::NK_EDITION ? AnimateurObserve(m, d) : nullptr;
			g.live = a != nullptr;
			if (a == nullptr) {
				g.liveState = g.liveNext = 0;
				g.liveFade = 0.f;
				d.etatVu = -1;
				for (uint32 i = 0; i < (uint32)g.params.Size(); ++i) {
					g.params[i].value = g.params[i].defaultValue;
				}
				return;
			}
			const anim::NkAnimStateMachine::NkRuntime &rt = a->execution;
			auto noeud = [&](int32 etat) -> nk_uint64 {
				return (etat >= 0 && etat < (int32)d.noeudDeEtat.Size()) ? d.noeudDeEtat[(uint32)etat] : 0;
			};
			g.liveState = noeud(rt.current);
			g.liveNext = noeud(rt.next);
			g.liveFade = (rt.next >= 0 && rt.fadeDur > 1e-6f) ? rt.fadeT / rt.fadeDur : 0.f;
			// La transition qui vient de TIRER : celle qui mene a l'etat neuf depuis
			// l'ancien (ou depuis un niveau qui le contient, ou de n'importe ou).
			if (rt.current != d.etatVu && d.etatVu >= 0 && rt.current >= 0) {
				const nk_uint64 avant = noeud(d.etatVu);
				for (uint32 t = 0; t < (uint32)g.transitions.Size(); ++t) {
					const editorkit::NkGraphTransition &tr = g.transitions[t];
					const bool versIci = tr.to == g.liveState || g.IsUnder(g.liveState, tr.to);
					const bool depuis = tr.any || tr.from == avant || g.IsUnder(avant, tr.from);
					if (versIci && depuis) {
						g.firedTransition = tr.id;
						g.firedAge = 0.f;
						break;
					}
				}
			}
			d.etatVu = rt.current;
			// Les valeurs VIVANTES des parametres, telles que l'entite les porte.
			for (uint32 i = 0; i < (uint32)g.params.Size(); ++i) {
				const int32 k = a->Parametre(g.params[i].name.CStr());
				g.params[i].value = k >= 0 ? a->params[k].valeur : g.params[i].defaultValue;
			}
		}

		// =====================================================================
		// LA PAGE
		// =====================================================================
		void NkEditeurDessinerPageAnimateur(NkEditeurCadre &c, NkDocAnim &d, const NkRect &zone) {
			NkEditeurModele &m = c.m;
			NkPagesAnim &pa = c.ui.pagesAnim;
			auto &dl = c.ctx.dl;
			editorkit::NkStateGraphModel &g = d.graphe;
			NkEditeurSuivreAnimateur(c, d);
			const ecs::NkEntityId cible = NkEditeurCibleDoc(m, d);

			// ── L'en-tete ──────────────────────────────────────────────────────
			const NkRect tete{zone.x, zone.y, zone.w, HAUTEUR_EN_TETE};
			dl.AddRectFilled(tete, c.pal.entete);
			dl.AddRectFilled(NkRect{tete.x, tete.y + tete.h - 1.f, tete.w, 1.f}, c.pal.bord);
			const float32 ty = tete.y + (tete.h - renderer::NkTexteHauteurLigne(c.police, 16.f)) * 0.5f;
			float32 x = tete.x + 12.f;
			const NkColor nature(c.theme.Get(NkRole::NodeActionHeader));
			dl.AddRectFilled(NkRect{x, tete.y + tete.h * 0.5f - 5.f, 10.f, 10.f}, nature, 2.f);
			x += 18.f;
			const NkString titre = NkString::Format("Animateur  %s", d.nom.CStr());
			renderer::NkTexte(dl, c.police, x, ty, titre.CStr(), c.pal.texte);
			x += renderer::NkTexteLargeur(c.police, titre.CStr());
			if (d.modifie) {
				dl.AddCircleFilled(NkVec2{x + 8.f, tete.y + tete.h * 0.5f}, 3.5f, c.pal.selection);
			}
			x += 24.f;
			const NkString objet =
				NkString::Format("Entité observée : %s", m.scene.Monde().IsAlive(cible) ? NomDe(m, cible).CStr() : "(aucune)");
			renderer::NkTexte(dl, c.police, x, ty, objet.CStr(), c.pal.attenue);
			x += renderer::NkTexteLargeur(c.police, objet.CStr()) + 8.f;
			const float32 bh = tete.h - 8.f;
			auto bouton = [&](NkBoutonPage b, const char *texte, bool enfonce, bool actif, float32 bx) {
				const float32 w = renderer::NkTexteLargeur(c.police, texte) + 18.f;
				const NkRect r{bx, tete.y + 4.f, w, bh};
				pa.boutons[(uint8)b] = r;
				return NkEditeurBouton(c, r, texte, enfonce, actif);
			};
			const bool sel = m.aSelection && m.scene.Monde().IsAlive(m.selection);
			if (bouton(NkBoutonPage::NK_SELECTION, "Utiliser la sélection", false, sel, x) && sel) {
				d.cibleUid = m.scene.AssurerUid(m.selection);
			}
			float32 xd = tete.x + tete.w - 10.f;
			auto boutonDroite = [&](NkBoutonPage b, const char *texte, bool enfonce, bool actif) {
				const float32 w = renderer::NkTexteLargeur(c.police, texte) + 18.f;
				xd -= w;
				const bool clic = bouton(b, texte, enfonce, actif, xd);
				xd -= 6.f;
				return clic;
			};
			if (boutonDroite(NkBoutonPage::NK_ENREGISTRER, "Enregistrer", false, !g.nodes.Empty())) {
				NkEditeurEnregistrerDocAnim(m, d);
			}
			const bool vivant = m.scene.Monde().IsAlive(cible);
			if (boutonDroite(NkBoutonPage::NK_ATTACHER, "Appliquer à l'entité", false, vivant && !g.nodes.Empty())) {
				// Le modele compile sous le nom du document, et l'entite le porte : un
				// NkAnimateur2D NEUF (ses parametres sont ceux du modele).
				Compiler(d);
				m.scene.Monde().Add<unkeny::NkAnimateur2D>(cible, unkeny::NkCreerAnimateur2D(d.nom.CStr()));
				NkEditeurAnnoncer(m, NkString::Format("%s joue maintenant « %s »", NomDe(m, cible).CStr(), d.nom.CStr()).CStr());
			}
			const bool enJeu = m.etat != NkEtatJeu::NK_EDITION;
			if (boutonDroite(NkBoutonPage::NK_JOUER, enJeu ? "Arrêter" : "Jouer", enJeu, true)) {
				NkEditeurExecuter(c, enJeu ? NK_A_ARRETER : NK_A_JOUER);
			}

			// ── Le graphe ─────────────────────────────────────────────────────
			const NkRect corps{zone.x, tete.y + tete.h, zone.w, zone.y + zone.h - tete.y - tete.h};
			// L'apercu de l'entite, en bas a droite de la toile du graphe (la toile
			// de la trame d'avant : le kit la calcule).
			const editorkit::NkPaintRect &toile = pa.graphe.canvas;
			NkRect apercu{0.f, 0.f, 0.f, 0.f};
			if (toile.w > 420.f && toile.h > 260.f && vivant) {
				apercu = NkRect{toile.x + toile.w - 250.f, toile.y + toile.h - 190.f, 240.f, 180.f};
			}
			pa.apercu = apercu;
			editorkit::NkComponentInput ci = NkEditeurEntreeComposant(c.ctx);
			const bool masque = d.renomme || c.ui.menu != NkMenuEditeur::NK_AUCUN ||
								(apercu.w > 0.f && NkEditeurDans(apercu, c.ctx.input.mousePos));
			if (masque) {
				ci.mouseX = ci.mouseY = -10000.f;
				ci.mousePressed = ci.mouseReleased = ci.doubleClick = false;
				ci.wheel = 0.f;
			}
			if (!d.grapheCadre && toile.w > 1.f) {
				g.FrameLevel(toile.w, toile.h);
				d.grapheCadre = true;
			}
			// (01/10 soir) Les MASQUES proposes aux couches : les objets nommes sous
			// l'entite observee (« Torse » couvre « Torse » et ses descendants).
			g.masks.Clear();
			if (m.scene.Monde().IsAlive(cible)) {
				NkVector<ecs::NkEntityId> pile, enfants;
				pile.PushBack(cible);
				for (uint32 k = 0; k < (uint32)pile.Size() && k < 256u; ++k) {
					if (k > 0) {
						NkString chemin;
						if (unkeny::NkCheminCible(m.scene, cible, pile[k], chemin) && !chemin.Empty()) {
							g.masks.PushBack(chemin);
						}
					}
					m.scene.Enfants(pile[k], enfants);
					for (uint32 e = 0; e < (uint32)enfants.Size(); ++e) {
						pile.PushBack(enfants[e]);
					}
				}
			}
			editorkit::NkGuiComponentPaint peintre(c.ctx, c.theme);
			const uint32 avant = g.revision;
			pa.graphe = editorkit::NkDrawStateGraph(peintre, ci, editorkit::NkPaintRect{corps.x, corps.y, corps.w, corps.h}, g,
													 StyleGraphe());
			// La definition a change : le modele que joue l'entite suit.
			if (g.revision != avant) {
				Compiler(d);
				d.modifie = true;
			}
			// L'apercu : une valeur vivante changee va a l'entite.
			if (pa.graphe.paramValueChanged && pa.graphe.paramIndex >= 0 && pa.graphe.paramIndex < (int32)g.params.Size()) {
				if (unkeny::NkAnimateur2D *a = AnimateurObserve(m, d)) {
					const editorkit::NkGraphParam &p = g.params[(uint32)pa.graphe.paramIndex];
					if (p.kind == (uint8)editorkit::NkGraphParamKind::Trigger) {
						a->Declencher(p.name.CStr());
					} else if (p.kind == (uint8)editorkit::NkGraphParamKind::Bool) {
						a->PoserBool(p.name.CStr(), p.value > 0.5f);
					} else {
						a->Poser(p.name.CStr(), p.value);
					}
				}
			}
			if (pa.graphe.renameRequested) {
				d.renomme = true;
				d.renommeNoeud = pa.graphe.renameNode;
				d.renommeParam = pa.graphe.renameParam;
				const NkString courant = d.renommeParam >= 0 && d.renommeParam < (int32)g.params.Size()
											 ? g.params[(uint32)d.renommeParam].name
											 : (g.Node(d.renommeNoeud) != nullptr ? g.Node(d.renommeNoeud)->name : NkString());
				std::snprintf(d.tampon, sizeof(d.tampon), "%s", courant.CStr());
				d.renommeRect = NkRect{pa.graphe.renameRect.x, pa.graphe.renameRect.y, pa.graphe.renameRect.w, pa.graphe.renameRect.h};
				// Le champ s'ouvre a la trame SUIVANTE : le clic qui l'a demande ne
				// doit pas le valider aussitot (« un clic ailleurs valide »).
				d.renommeChoisir = true;
			} else {
				Renommer(c, d);
			}
			if (apercu.w > 0.f) {
				NkEditeurDessinerApercuAnim(c, cible, apercu);
				const unkeny::NkAnimateur2D *a = m.scene.Monde().Get<unkeny::NkAnimateur2D>(cible);
				const char *etat = "";
				NkString chemin;
				if (a == nullptr) {
					etat = "pas d'animateur : « Appliquer à l'entité »";
				} else if (std::strncmp(a->modele, d.nom.CStr(), unkeny::NK_UNKENY_ANIM_MODELE_MAX - 1) != 0) {
					etat = "elle joue un autre contrôleur";
				} else if (enJeu) {
					chemin = unkeny::NkEtatAnimateur2D(*a);
					etat = chemin.CStr();
				} else {
					etat = "« Jouer » pour voir les transitions";
				}
				renderer::NkTexte(dl, c.petite, apercu.x + 8.f, apercu.y + apercu.h - 18.f, etat, c.pal.attenue);
			}
		}

	} // namespace editeur
} // namespace nkentseu
