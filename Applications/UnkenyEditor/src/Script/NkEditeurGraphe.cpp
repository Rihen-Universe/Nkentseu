// -----------------------------------------------------------------------------
// FICHIER: UnkenyEditor/Script/NkEditeurGraphe.cpp
// DESCRIPTION: La page du graphe d'un Blueprint : palette, canevas du kit,
//              compilation, erreur designee sur son noeud.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Script/NkEditeurGraphe.h"

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurInterface.h"
#include "Script/NkBpCatalogue.h"
#include "Script/NkEditeurScripts.h"

#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKEditorKit/NkEditorTextField.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		using nkgui::NkColor;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		namespace {
			/// Les couleurs des TYPES, celles d'UE5 (booleen rouge, entier
			/// turquoise, reel vert, vecteur jaune, objet bleu, texte rose).
			NkColor CouleurType(void *, const graph::NkNodeGraph &g, graph::NkTypeId t) {
				const NkString *nom = g.TypeName(t);
				switch (NkBpTypeDeNom(nom != nullptr ? nom->CStr() : nullptr)) {
					case unkeny::NkTypeBp::NK_BOOLEEN:
						return NkColor(200, 40, 40, 255);
					case unkeny::NkTypeBp::NK_ENTIER:
						return NkColor(30, 220, 170, 255);
					case unkeny::NkTypeBp::NK_REEL:
						return NkColor(120, 230, 60, 255);
					case unkeny::NkTypeBp::NK_VEC2:
						return NkColor(250, 200, 40, 255);
					case unkeny::NkTypeBp::NK_ENTITE:
						return NkColor(60, 150, 250, 255);
					case unkeny::NkTypeBp::NK_TEXTE:
						return NkColor(240, 90, 210, 255);
					default:
						return NkColor(160, 160, 160, 255);
				}
			}
			NkColor CouleurCategorie(const NkProtoBp *p) {
				if (p == nullptr) {
					return NkColor(70, 70, 70, 255);
				}
				switch (p->genre) {
					case NkGenreNoeudBp::NK_EVENEMENT:
						return NkColor(140, 30, 30, 255);
					case NkGenreNoeudBp::NK_SI:
					case NkGenreNoeudBp::NK_SEQUENCE:
						return NkColor(85, 85, 85, 255);
					case NkGenreNoeudBp::NK_LIRE_VAR:
					case NkGenreNoeudBp::NK_ECRIRE_VAR:
						return NkColor(30, 110, 100, 255);
					case NkGenreNoeudBp::NK_MATH:
						return NkColor(60, 90, 50, 255);
					case NkGenreNoeudBp::NK_SOI:
						return NkColor(40, 80, 140, 255);
					default: {
						uint32 n = 0;
						const unkeny::NkNatifBp &x = unkeny::NkNatifsBp(n)[p->natif];
						return x.pur ? NkColor(50, 100, 60, 255) : NkColor(35, 70, 130, 255);
					}
				}
			}
			NkColor CouleurEntete(void *, const graph::NkNode &n) {
				return CouleurCategorie(NkBpProto(n.type.CStr()));
			}
			NkString TexteDefaut(void *, const graph::NkNodeGraph &g, const graph::NkNode &n, const graph::NkSocket &s) {
				const NkString *nom = g.TypeName(s.type);
				if (NkBpTypeDeNom(nom != nullptr ? nom->CStr() : nullptr) == unkeny::NkTypeBp::NK_ENTITE) {
					return NkString(); // une entite libre vaut « soi » : pas de champ
				}
				NkString t = NkBpTexteDefaut(g, n, s);
				if (t.Empty() && NkBpTypeDeNom(nom != nullptr ? nom->CStr() : nullptr) == unkeny::NkTypeBp::NK_TEXTE) {
					t = "…"; // un texte vide reste saisissable
				}
				return t;
			}
			bool PoserDefaut(void *, graph::NkNodeGraph &g, graph::NkNodeId n, const char *prise, const char *texte) {
				const char *t = std::strcmp(texte, "…") == 0 ? "" : texte;
				return NkBpPoserDefaut(g, n, prise, t);
			}
			editorkit::NkDomaineCanevas Domaine() {
				editorkit::NkDomaineCanevas d;
				d.CouleurType = &CouleurType;
				d.CouleurEntete = &CouleurEntete;
				d.TexteDefaut = &TexteDefaut;
				d.PoserDefaut = &PoserDefaut;
				return d;
			}
			editorkit::NkStyleCanevas Style(const NkPaletteEditeur &pal) {
				editorkit::NkStyleCanevas s;
				s.fond = NkColor(22, 22, 24, 255);
				s.texte = pal.texte;
				s.attenue = pal.attenue;
				s.selection = pal.selection;
				s.champ = pal.champ;
				return s;
			}
			bool ContientSansCasse(const char *texte, const char *motif) {
				return motif == nullptr || motif[0] == '\0' || NkEditeurContientSansCasse(texte, motif);
			}
		} // namespace

		bool NkEditeurGrapheOuvert(const NkEditeurModele &m) {
			return m.scripts != nullptr && m.scripts->courant != nullptr;
		}

		NkEditeurGrapheEtat *NkEditeurGrapheParId(NkEditeurScripts &s, nk_uint64 id) {
			for (uint32 i = 0; i < s.graphes.Size(); ++i) {
				if (s.graphes[i]->id == id) {
					return s.graphes[i];
				}
			}
			return nullptr;
		}

		bool NkEditeurGrapheDevenirCourant(NkEditeurScripts &s, nk_uint64 id) {
			NkEditeurGrapheEtat *e = NkEditeurGrapheParId(s, id);
			if (e != nullptr) {
				s.courant = e;
			}
			return e != nullptr;
		}

		bool NkEditeurOuvrirGraphe(NkEditeurScripts &s, NkEditeurModele &m, const char *chemin, NkEditeurInterface *ui) {
			// Ouvert deux fois : le MEME onglet revient (ses modifications, son
			// historique, sa vue sont gardes). Compare par REFERENCE du projet
			// (« Contenu/Scripts/Porte.nkbp ») : deux ecritures du meme chemin absolu
			// (barres, majuscules du disque) ne font pas deux onglets.
			const NkString ref = chemin != nullptr ? NkEditeurRefScript(m, chemin) : NkString();
			for (uint32 i = 0; chemin != nullptr && i < s.graphes.Size(); ++i) {
				if (s.graphes[i]->chemin == NkString(chemin) || (!ref.Empty() && s.graphes[i]->ref == ref)) {
					s.courant = s.graphes[i];
					if (ui != nullptr) {
						NkEditeurActiverDocument(m, *ui, NkDoc(NkGenreDocument::NK_BLUEPRINT, s.graphes[i]->id));
					}
					return true;
				}
			}
			NkString err;
			graph::NkNodeGraph g;
			if (!NkBpOuvrir(chemin, g, &err)) {
				NkEditeurAnnoncer(m, NkString::Format("Blueprint illisible : %s", err.CStr()).CStr());
				return false;
			}
			NkEditeurGrapheEtat *pe = memory::NkGetDefaultAllocator().New<NkEditeurGrapheEtat>();
			NkEditeurGrapheEtat &e = *pe;
			e.id = s.prochainGraphe++;
			e.graphe = g;
			e.chemin = chemin;
			e.ref = NkEditeurRefScript(m, chemin);
			e.historique.Reset(e.graphe);
			e.canevas = editorkit::NkEtatCanevas();
			e.ouvert = true;
			e.modifie = false;
			e.erreur = false;
			e.message = NkString::Format("Ouvert : %s — clic droit dans le vide puis un nœud de la palette pour l'y poser ; « Compiler » enregistre.",
										 e.ref.CStr());
			e.cadrer = true;
			s.graphes.PushBack(pe);
			s.courant = pe;
			// Son onglet entre dans la barre, au premier plan.
			if (ui != nullptr) {
				NkEditeurActiverDocument(m, *ui, NkDoc(NkGenreDocument::NK_BLUEPRINT, e.id));
			}
			return true;
		}

		bool NkEditeurDetruireGraphe(NkEditeurScripts &s, nk_uint64 id) {
			for (uint32 i = 0; i < s.graphes.Size(); ++i) {
				if (s.graphes[i]->id != id) {
					continue;
				}
				NkEditeurGrapheEtat *e = s.graphes[i];
				s.graphes.Erase(s.graphes.Begin() + i);
				if (s.courant == e) {
					// Le courant devient le dernier ouvert qui reste (ou aucun).
					s.courant = s.graphes.Empty() ? nullptr : s.graphes[s.graphes.Size() - 1];
				}
				memory::NkGetDefaultAllocator().Delete(e);
				return true;
			}
			return false;
		}

		void NkEditeurFermerGraphe(NkEditeurScripts &s) {
			if (s.courant != nullptr) {
				NkEditeurDetruireGraphe(s, s.courant->id);
			}
		}

		bool NkEditeurCompilerGraphe(NkEditeurScripts &s, NkEditeurModele &m) {
			if (s.courant == nullptr) {
				return false;
			}
			NkEditeurGrapheEtat &e = *s.courant;
			NkErreurBp err;
			unkeny::NkModuleBp module;
			const bool ok = NkBpEnregistrer(e.chemin.CStr(), e.graphe, err, &module);
			e.canevas.enErreur = graph::NK_NODE_INVALID;
			if (ok) {
				NkString refus;
				s.registre.EnregistrerBlueprint(e.ref.CStr(), module, &refus);
				const uint32 migrees = s.hote.MigrerInstances();
				e.erreur = false;
				e.modifie = false;
				e.message = NkString::Format("Compilé et enregistré : %u fonction(s), %u variable(s)%s", static_cast<unsigned>(module.fonctions.Size()),
											 static_cast<unsigned>(module.variables.Size()),
											 migrees > 0u ? " — rechargé dans la partie en cours" : "");
				s.journal.PushBack(NkString::Format("[Blueprint] %s compilé et enregistré", e.ref.CStr()));
				// Le relevé du disque ne le relira pas une seconde fois.
				for (uint32 i = 0; i < s.blueprints.Size(); ++i) {
					if (s.blueprints[i] == e.ref) {
						s.empreintesBp[i] = -1; // remis au prochain releve, sans recharger (meme contenu)
					}
				}
			} else {
				e.erreur = true;
				e.canevas.enErreur = err.noeud;
				const graph::NkNode *n = e.graphe.Find(err.noeud);
				// La vue va au noeud fautif : on VOIT l'erreur, entouree de rouge.
				if (n != nullptr && e.zoneCanevas.w > 0.f && e.canevas.zoom > 0.f) {
					e.canevas.vueX = n->x - e.zoneCanevas.w / e.canevas.zoom * 0.3f;
					e.canevas.vueY = n->y - e.zoneCanevas.h / e.canevas.zoom * 0.3f;
				}
				e.message = n != nullptr ? NkString::Format("Erreur sur « %s » : %s", n->label.CStr(), err.message.CStr())
										 : NkString::Format("Erreur : %s", err.message.CStr());
				s.journal.PushBack(NkString::Format("[Blueprint] %s : %s", e.ref.CStr(), e.message.CStr()));
				s.montrerJournal = true;
			}
			(void)m;
			return ok;
		}

		graph::NkNodeId NkEditeurGrapheAjouter(NkEditeurScripts &s, const char *type) {
			if (s.courant == nullptr) {
				return graph::NK_NODE_INVALID;
			}
			NkEditeurGrapheEtat &e = *s.courant;
			float32 x = e.ajoutX, y = e.ajoutY;
			if (!e.ajoutPose) {
				// Au centre de ce qu'on regarde.
				const NkVec2 c = editorkit::NkCanevasDepuisEcran(
					e.canevas, e.zoneCanevas, NkVec2{e.zoneCanevas.x + e.zoneCanevas.w * 0.4f, e.zoneCanevas.y + e.zoneCanevas.h * 0.4f});
				x = c.x;
				y = c.y;
			}
			const graph::NkNodeId n = NkBpCreerNoeud(e.graphe, type, x, y);
			if (n != graph::NK_NODE_INVALID) {
				e.canevas.selection = n;
				e.historique.Commit(e.graphe);
				e.modifie = true;
				e.ajoutPose = false;
			}
			return n;
		}

		void NkEditeurDessinerGraphe(NkEditeurCadre &c) {
			if (c.m.scripts == nullptr || c.m.scripts->courant == nullptr) {
				return;
			}
			NkEditeurScripts &s = *c.m.scripts;
			NkEditeurGrapheEtat &e = *s.courant;
			nkgui::NkGuiDrawList &dl = c.ctx.dl;
			nkgui::NkGuiInput &in = c.ctx.input;
			const NkRect page = c.ui.vue;
			const bool surPage = NkEditeurDans(page, in.mousePos);
			dl.AddRectFilled(page, c.pal.fond);

			// ── La barre : titre, Compiler, Enregistrer, Fermer ──
			const float32 hb = 32.f;
			const NkRect barre{page.x, page.y, page.w, hb};
			dl.AddRectFilled(barre, c.pal.entete);
			dl.AddRectFilled(NkRect{barre.x, barre.y + hb - 1.f, barre.w, 1.f}, c.pal.bord);
			const NkString titre = NkString::Format("Blueprint — %s%s", e.ref.CStr(), e.modifie ? "  •" : "");
			renderer::NkTexte(dl, c.police, barre.x + 12.f, barre.y + 7.f, titre.CStr(), c.pal.texte, page.w * 0.5f);
			const NkRect fermer{barre.x + barre.w - 84.f, barre.y + 4.f, 76.f, hb - 8.f};
			const NkRect compiler{fermer.x - 196.f, barre.y + 4.f, 188.f, hb - 8.f};
			e.boutonCompiler = compiler;
			e.boutonFermer = fermer;
			if (NkEditeurBouton(c, compiler, "Compiler et enregistrer", false, true)) {
				NkEditeurCompilerGraphe(s, c.m);
			}
			if (NkEditeurBouton(c, fermer, "Fermer", false, true)) {
				// Par la barre : son onglet part, son voisin de gauche (ou la scene)
				// passe devant.
				NkEditeurFermerDocument(c.m, c.ui, NkDoc(NkGenreDocument::NK_BLUEPRINT, e.id));
				return;
			}

			// ── La palette, a gauche ──
			const float32 lp = 200.f;
			const float32 hs = 26.f;
			const NkRect palette{page.x, page.y + hb, lp, page.h - hb - hs};
			dl.AddRectFilled(palette, c.pal.panneau);
			dl.AddRectFilled(NkRect{palette.x + palette.w - 1.f, palette.y, 1.f, palette.h}, c.pal.bord);
			const NkRect recherche{palette.x + 8.f, palette.y + 8.f, palette.w - 16.f, 24.f};
			if (in.mouseClicked[0]) {
				e.filtreFocus = NkEditeurDans(recherche, in.mousePos);
			}
			if (e.filtreFocus && in.KeyPressed(nkgui::NkGuiKey::Escape)) {
				e.filtre[0] = '\0';
				e.filtreFocus = false;
			}
			dl.AddRectFilled(recherche, c.pal.champ, 3.f);
			dl.AddRect(recherche, e.filtreFocus ? c.pal.accent : c.pal.bord, 1.f, 3.f);
			if (e.filtre[0] == '\0' && !e.filtreFocus) {
				renderer::NkTexte(dl, c.petite, recherche.x + 6.f, recherche.y + 5.f, "Rechercher un nœud", c.pal.attenue);
			}
			editorkit::NkOverlayFieldStyle st;
			st.fond = false;
			st.bord = false;
			st.texte = c.pal.texte;
			st.utf8 = true;
			editorkit::NkOverlayTextField(c.ctx, dl, c.police, NkRect{recherche.x + 4.f, recherche.y, recherche.w - 8.f, recherche.h}, e.filtre,
										  static_cast<int32>(sizeof(e.filtre)), e.filtreFocus, &st);
			const NkRect liste{palette.x, recherche.y + recherche.h + 6.f, palette.w, palette.y + palette.h - recherche.y - recherche.h - 6.f};
			if (NkEditeurDans(liste, in.mousePos) && in.wheel != 0.f) {
				e.defil -= in.wheel * 40.f;
				in.wheel = 0.f;
			}
			e.defil = e.defil < 0.f ? 0.f : e.defil;
			dl.PushClipRect(liste);
			const NkVector<NkProtoBp> &protos = NkBpProtos();
			e.paletteRects.Clear();
			e.paletteProtos.Clear();
			float32 y = liste.y - e.defil;
			const NkString *categorie = nullptr;
			const float32 lr = 22.f;
			for (uint32 i = 0; i < protos.Size(); ++i) {
				const NkProtoBp &p = protos[i];
				if (!ContientSansCasse(p.libelle.CStr(), e.filtre) && !ContientSansCasse(p.categorie.CStr(), e.filtre)) {
					continue;
				}
				if (categorie == nullptr || !(*categorie == p.categorie)) {
					categorie = &p.categorie;
					y += 4.f;
					renderer::NkTexte(dl, c.petite, liste.x + 10.f, y + 2.f, p.categorie.CStr(), c.pal.accent);
					y += 20.f;
				}
				const NkRect r{liste.x + 6.f, y, liste.w - 12.f, lr};
				const bool survol = NkEditeurDans(r, in.mousePos) && NkEditeurDans(liste, in.mousePos);
				if (survol) {
					dl.AddRectFilled(r, c.pal.boutonSurvol, 3.f);
				}
				dl.AddRectFilled(NkRect{r.x + 4.f, r.y + 6.f, 4.f, lr - 12.f}, CouleurCategorie(&p));
				renderer::NkTexte(dl, c.petite, r.x + 14.f, r.y + 4.f, p.libelle.CStr(), c.pal.texte, r.w - 18.f);
				e.paletteRects.PushBack(r);
				e.paletteProtos.PushBack(static_cast<int32>(i));
				if (survol && in.mouseClicked[0]) {
					NkEditeurGrapheAjouter(s, p.type.CStr());
					in.mouseClicked[0] = false;
				}
				y += lr + 2.f;
			}
			dl.PopClipRect();

			// ── Le canevas ──
			const NkRect zone{palette.x + palette.w, page.y + hb, page.w - palette.w, page.h - hb - hs};
			e.zoneCanevas = zone;
			if (e.cadrer && zone.w > 50.f && zone.h > 50.f) {
				editorkit::NkCanevasCadrer(e.canevas, zone, e.graphe, Style(c.pal));
				e.cadrer = false;
			}
			const editorkit::NkStyleCanevas style = Style(c.pal);
			const editorkit::NkDomaineCanevas domaine = Domaine();
			editorkit::NkCanevasNoeuds(dl, in, c.police, zone, e.graphe, e.canevas, style, domaine);
			if (e.canevas.modifie) {
				e.historique.Commit(e.graphe);
				e.modifie = true;
			}
			if (e.canevas.menuDemande) {
				e.ajoutPose = true;
				e.ajoutX = e.canevas.menuX;
				e.ajoutY = e.canevas.menuY;
				e.message = "Point choisi : cliquez un nœud de la palette pour l'y poser";
				e.erreur = false;
			}
			if (e.ajoutPose) {
				const NkVec2 p = editorkit::NkCanevasVersEcran(e.canevas, zone, e.ajoutX, e.ajoutY);
				dl.AddCircle(p, 6.f, c.pal.accent, 2.f);
			}

			// ── La barre d'etat : la derniere compilation ──
			const NkRect etat{page.x, page.y + page.h - hs, page.w, hs};
			dl.AddRectFilled(etat, c.pal.entete);
			const NkColor ce = e.erreur ? NkColor(240, 90, 80, 255) : c.pal.texte;
			renderer::NkTexte(dl, c.petite, etat.x + 10.f, etat.y + 6.f, e.message.CStr(), ce, etat.w - 20.f);

			// ── Le clavier, tant que la souris est sur la page ──
			if (surPage && !e.filtreFocus && e.canevas.saisieNoeud == graph::NK_NODE_INVALID) {
				if (in.ctrlDown && in.KeyPressed(nkgui::NkGuiKey::Z)) {
					if (e.historique.Undo(e.graphe)) {
						e.modifie = true;
					}
					in.keyInit[static_cast<int32>(nkgui::NkGuiKey::Z)] = false;
				} else if (in.ctrlDown && in.KeyPressed(nkgui::NkGuiKey::Y)) {
					if (e.historique.Redo(e.graphe)) {
						e.modifie = true;
					}
					in.keyInit[static_cast<int32>(nkgui::NkGuiKey::Y)] = false;
				}
			}
			// Un champ de la page a le clavier : les raccourcis de l'editeur se taisent.
			if (e.filtreFocus || e.canevas.saisieNoeud != graph::NK_NODE_INVALID) {
				c.ui.toucheChamp = true;
			}
			// Suppr, Entree, Echap sur la page sont au graphe, pas a la scene.
			if (surPage) {
				in.keyInit[static_cast<int32>(nkgui::NkGuiKey::Delete)] = false;
			}
		}

	} // namespace editeur
} // namespace nkentseu
