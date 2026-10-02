// -----------------------------------------------------------------------------
// FICHIER: UnkenyEditor/Script/NkEditeurBlueprint.cpp
// DESCRIPTION: L'editeur de Blueprint a la UE5 (NkEditeurBlueprint.h) : mise
//              en page, barre d'outils, Mon Blueprint, onglets et toile,
//              menus, glisser-deposer, Details, resultats, Simuler.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Script/NkEditeurBlueprint.h"

#include "Script/NkBpExpression.h"

#include "NKEditorKit/NkEditorTextField.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		using editorkit::NkJetonsNodal;
		using nkgui::NkColor;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		namespace {
			// Les sections de Mon Blueprint.
			enum : int32 { SEC_GRAPHES = 0, SEC_FONCTIONS, SEC_MACROS, SEC_VARIABLES, SEC_REPARTITEURS };
			// Les genres de petit menu.
			enum : uint8 {
				POP_TYPE_VAR = 1,
				POP_TYPE_PARAM,
				POP_NOEUD,
				POP_DEPOSER_VAR,
				POP_ELEMENT,
				POP_PORTEE,
				POP_SNIPPETS,
				POP_TYPE_PRISE_CODE,
				POP_TYPE_RESULTAT,
				POP_DEPOSER_REP
			};
			// Les champs des Details (identifiants stables d'une trame a l'autre).
			enum : int32 {
				CH_NOM = 1,
				CH_DEFAUT,
				CH_DEFAUT_Y,
				CH_INFOBULLE,
				CH_CATEGORIE,
				CH_DESCRIPTION,
				CH_TITRE_NOEUD,
				CH_PARAM = 100,	  ///< + 2 * indice (+1 : sortie)
				CH_PRISE = 400,	  ///< + indice de prise (noeud de code)
				CH_VAR_DEFAUT = 700 ///< + indice de variable (« Valeurs par defaut »)
			};

			const NkJetonsNodal &J() {
				return editorkit::NkJetonsNodalParDefaut();
			}
			bool Dans(const NkRect &r, const NkVec2 &p) {
				return p.x >= r.x && p.y >= r.y && p.x < r.x + r.w && p.y < r.y + r.h;
			}
			void Texte(nkgui::NkGuiDrawList &dl, nkgui::NkGuiFont *f, float32 x, float32 top, const char *s, const NkColor &c, float32 ech = 1.f,
					   float32 maxW = -1.f) {
				if (f == nullptr || f->Face() == nullptr || s == nullptr || s[0] == '\0') {
					return;
				}
				const NkVec2 base{x, top + f->Ascent() * ech};
				if (ech > 0.97f && ech < 1.03f) {
					dl.AddText(f->Face(), f->TexId(), base, s, c, maxW);
				} else {
					dl.AddTextScaled(f->Face(), f->TexId(), base, s, c, ech, maxW);
				}
			}
			float32 Largeur(nkgui::NkGuiFont *f, const char *s, float32 ech = 1.f) {
				return f != nullptr && f->Face() != nullptr && s != nullptr ? f->MeasureWidth(s) * ech : 0.f;
			}
			float32 Ligne(nkgui::NkGuiFont *f) {
				return f != nullptr && f->Face() != nullptr ? f->LineHeight() : 14.f;
			}
			/// Un bouton de la barre (charte : contour #33333C, l'orange pour le principal).
			bool Bouton(nkgui::NkGuiContext &ctx, nkgui::NkGuiFont *f, const NkRect &r, const char *lib, bool principal = false, bool actif = false) {
				nkgui::NkGuiDrawList &dl = ctx.dl;
				const bool sur = Dans(r, ctx.input.mousePos);
				const NkColor fond = actif ? NkColor(58, 40, 18, 255) : (sur ? J().menuSurvol : J().sectionFond);
				dl.AddRectFilled(r, fond, 3.f);
				dl.AddRect(r, principal || actif ? J().exec : J().bord, 1.f, 3.f);
				const float32 lw = Largeur(f, lib, 0.92f);
				Texte(dl, f, r.x + (r.w - lw) * 0.5f, r.y + (r.h - Ligne(f) * 0.92f) * 0.5f, lib, J().texteEntete, 0.92f);
				if (sur && ctx.input.mouseClicked[0]) {
					ctx.input.mouseClicked[0] = false;
					return true;
				}
				return false;
			}
			/// La couleur de pastille d'une entree de menu, par sa categorie.
			NkColor CouleurCategorie(const NkString &c) {
				const NkJetonsNodal &s = J();
				const char *p = c.CStr();
				auto commence = [&](const char *x) {
					return std::strncmp(p, x, std::strlen(x)) == 0;
				};
				if (commence("Variables") || commence("Fonctions") || commence("Macros") || commence("Fonction")) {
					return s.entete[static_cast<uint32>(editorkit::NkNatureNoeud::NK_VARIABLE)];
				}
				if (commence("Calcul") || commence("Maths") || commence("Texte") || commence("Couleur")) {
					return s.entete[static_cast<uint32>(editorkit::NkNatureNoeud::NK_VALEUR)];
				}
				if (commence("Entité")) {
					return s.entete[static_cast<uint32>(editorkit::NkNatureNoeud::NK_CONTEXTE)];
				}
				if (commence("Code")) {
					return s.exec;
				}
				if (commence("Utilitaires")) {
					return s.attenue;
				}
				return s.entete[static_cast<uint32>(editorkit::NkNatureNoeud::NK_FLUX)];
			}
			/// Les toiles suivent les graphes du document (ajout, suppression).
			void Synchroniser(NkEditeurBlueprintEtat &e) {
				while (e.toiles.Size() < e.doc.graphes.Size()) {
					e.toiles.PushBack(editorkit::NkEtatCanevas());
					e.cadrer.PushBack(true);
				}
				while (e.toiles.Size() > e.doc.graphes.Size()) {
					e.toiles.PopBack();
					e.cadrer.PopBack();
				}
				for (uint32 i = 0; i < e.onglets.Size();) {
					if (e.onglets[i] >= e.doc.graphes.Size()) {
						e.onglets.Erase(e.onglets.Begin() + i);
					} else {
						++i;
					}
				}
				if (e.onglets.Empty()) {
					e.onglets.PushBack(0u);
				}
				if (e.actif >= e.doc.graphes.Size()) {
					e.actif = 0u;
				}
			}
			/// Un graphe supprime : les indices au-dela descendent d'un cran.
			void GrapheSupprime(NkEditeurBlueprintEtat &e, uint32 g) {
				for (uint32 i = 0; i < e.onglets.Size();) {
					if (e.onglets[i] == g) {
						e.onglets.Erase(e.onglets.Begin() + i);
						continue;
					}
					if (e.onglets[i] > g) {
						--e.onglets[i];
					}
					++i;
				}
				if (g < e.toiles.Size()) {
					e.toiles.Erase(e.toiles.Begin() + g);
					e.cadrer.Erase(e.cadrer.Begin() + g);
				}
				e.actif = e.actif == g ? 0u : (e.actif > g ? e.actif - 1u : e.actif);
				Synchroniser(e);
			}
			const char *LibelleGenreGraphe(NkGenreGrapheBp g) {
				return g == NkGenreGrapheBp::NK_FONCTION ? "Fonction" : (g == NkGenreGrapheBp::NK_MACRO ? "Macro" : "Graphe");
			}
		} // namespace

		// =====================================================================
		// Charger, retenir, onglets
		// =====================================================================
		void NkEditeurBlueprintCharger(NkEditeurBlueprintEtat &e) {
			e.onglets.Clear();
			e.onglets.PushBack(0u);
			e.actif = 0u;
			e.toiles.Clear();
			e.cadrer.Clear();
			Synchroniser(e);
			e.historique.Reset(e.doc);
			e.modifie = false;
			e.etatCompil = 0u;
			e.erreur = false;
			e.derniereErreur = NkErreurBp();
			e.choix = NkChoixBp::NK_RIEN;
			e.choixIndice = -1;
			e.champ = -1;
			e.resultats.Clear();
			e.trouves.Clear();
			e.codeNoeud = graph::NK_NODE_INVALID;
			e.menu.ouvert = false;
			e.popup.ouvert = false;
			e.mon = editorkit::NkEtatMonBlueprint();
		}

		void NkEditeurBlueprintRetenir(NkEditeurBlueprintEtat &e) {
			e.historique.Commit(e.doc);
			e.modifie = true;
			e.etatCompil = e.etatCompil == 2u ? 2u : 0u; // « a compiler »
		}

		void NkEditeurBlueprintOnglet(NkEditeurBlueprintEtat &e, uint32 g) {
			Synchroniser(e);
			if (g >= e.doc.graphes.Size()) {
				return;
			}
			bool ouvert = false;
			for (uint32 i = 0; i < e.onglets.Size(); ++i) {
				ouvert = ouvert || e.onglets[i] == g;
			}
			if (!ouvert) {
				e.onglets.PushBack(g);
			}
			e.actif = g;
		}

		/// Amene le noeud `n` a l'ecran -- SEULEMENT s'il n'y est pas deja en entier
		/// (sinon la vue ne bouge pas : on garde son reperage) ; il vient au centre.
		void NkEditeurBlueprintMontrer(NkEditeurBlueprintEtat &e, editorkit::NkEtatCanevas &t, const graph::NkNode &n) {
			const NkRect z = e.zoneToile;
			if (z.w <= 0.f || t.zoom <= 0.f) {
				return;
			}
			const NkRect r = editorkit::NkCanevasRectNoeud(t, z, n, J());
			const float32 m = 16.f;
			if (r.x >= z.x + m && r.y >= z.y + m && r.x + r.w <= z.x + z.w - m && r.y + r.h <= z.y + z.h - m) {
				return;
			}
			t.vueX = n.x + r.w / t.zoom * 0.5f - z.w / t.zoom * 0.5f;
			t.vueY = n.y + r.h / t.zoom * 0.5f - z.h / t.zoom * 0.5f;
		}

		bool NkEditeurBlueprintCompiler(NkEditeurBlueprintEtat &e, NkHoteBlueprint &hote) {
			NkErreurBp err;
			NkString message;
			const bool ok = hote.Compiler != nullptr && hote.Compiler(hote.donnees, e, err, message);
			e.resultats.Clear();
			e.derniereErreur = err;
			for (uint32 i = 0; i < e.toiles.Size(); ++i) {
				e.toiles[i].enErreur = graph::NK_NODE_INVALID;
				e.toiles[i].messageErreur.Clear();
			}
			NkLigneResultatBp l;
			if (ok) {
				e.etatCompil = 1u;
				e.erreur = false;
				e.modifie = false;
				e.message = message;
				l.genre = 1u;
				l.texte = message;
				e.resultats.PushBack(l);
			} else {
				e.etatCompil = 2u;
				e.erreur = true;
				const graph::NkNode *n = err.graphe < e.doc.graphes.Size() ? e.doc.graphes[err.graphe].graphe.Find(err.noeud) : nullptr;
				e.message = n != nullptr ? NkString::Format("Erreur sur « %s » (%s) : %s", n->label.CStr(), e.doc.graphes[err.graphe].nom.CStr(), err.message.CStr())
										 : NkString::Format("Erreur : %s", err.message.CStr());
				l.genre = 2u;
				l.texte = e.message;
				l.graphe = static_cast<int32>(err.graphe);
				l.noeud = err.noeud;
				e.resultats.PushBack(l);
				if (n != nullptr) {
					// La toile du graphe fautif s'ouvre, sur le noeud, entoure de rouge.
					NkEditeurBlueprintOnglet(e, err.graphe);
					editorkit::NkEtatCanevas &t = e.toiles[err.graphe];
					t.enErreur = err.noeud;
					t.messageErreur = err.message;
					t.Choisir(err.noeud);
					e.choix = NkChoixBp::NK_NOEUD; // les Details montrent le noeud fautif
					NkEditeurBlueprintMontrer(e, t, *n);
					e.cadrer[err.graphe] = false;
				}
				e.ongletBas = 0;
			}
			if (hote.Journal != nullptr) {
				hote.Journal(hote.donnees, NkString::Format("[Blueprint] %s : %s", e.ref.CStr(), e.message.CStr()).CStr(), !ok);
			}
			return ok;
		}

		// =====================================================================
		// Poser un noeud, le menu de la toile
		// =====================================================================
		void NkEditeurBlueprintEntrees(NkEditeurBlueprintEtat &e, bool contexte, graph::NkNodeId n, int32 k, NkVector<NkEntreeMenuBp> &sortie) {
			NkVector<NkEntreeMenuBp> tout;
			NkBpEntreesMenu(e.doc, e.actif, tout);
			graph::NkNodeGraph &g = e.Graphe();
			const graph::NkNode *p = contexte ? g.Find(n) : nullptr;
			if (p == nullptr || k < 0 || static_cast<uint32>(k) >= p->sockets.Size()) {
				sortie = tout;
				return;
			}
			const graph::NkSocket &so = p->sockets[static_cast<uint32>(k)];
			const bool exec = so.family == graph::NkSocketFamily::Exec;
			const bool sort = so.dir == graph::NkSocketDir::Output;
			sortie.Clear();
			for (uint32 i = 0; i < tout.Size(); ++i) {
				const NkVector<NkString> &cote = sort ? tout[i].entrees : tout[i].sorties;
				bool garde = false;
				for (uint32 q = 0; q < cote.Size() && !garde; ++q) {
					const bool cexec = cote[q] == "exec";
					if (cexec != exec) {
						continue;
					}
					if (exec) {
						garde = true;
						continue;
					}
					const graph::NkTypeId t = g.FindType(cote[q].CStr());
					garde = t != graph::NK_TYPE_INVALID && (sort ? g.Accepts(t, so.type) : g.Accepts(so.type, t));
				}
				if (garde) {
					sortie.PushBack(tout[i]);
				}
			}
			// Un relais DU TYPE de la prise.
			NkEntreeMenuBp r;
			const NkString *tn = g.TypeName(so.type);
			const NkString type = exec ? NkString("exec") : (tn != nullptr ? *tn : NkString("reel"));
			r.cle = NkString("bp.relais:") + type;
			r.libelle = NkString::Format("Nœud de réacheminement (%s)", NkBpLibelleType(type.CStr()));
			r.categorie = "Utilitaires";
			r.entrees.PushBack(type);
			r.sorties.PushBack(type);
			sortie.PushBack(r);
		}

		graph::NkNodeId NkEditeurBlueprintPoser(NkEditeurBlueprintEtat &e, const char *cle, float32 x, float32 y, graph::NkNodeId depuis, int32 prise) {
			graph::NkNodeGraph &g = e.Graphe();
			const graph::NkNodeId n = NkBpCreerParCle(e.doc, g, cle, x, y);
			if (n == graph::NK_NODE_INVALID) {
				return n;
			}
			const graph::NkNode *a = g.Find(depuis);
			if (a != nullptr && prise >= 0 && static_cast<uint32>(prise) < a->sockets.Size()) {
				const graph::NkSocket sa = a->sockets[static_cast<uint32>(prise)];
				const bool sort = sa.dir == graph::NkSocketDir::Output;
				// Une sortie d'execution n'a qu'UNE suite : l'ancienne cede (UE5).
				if (sort && sa.family == graph::NkSocketFamily::Exec) {
					for (uint32 i = 0; i < g.LinkCount(); ++i) {
						const graph::NkLink *l = g.LinkAt(i);
						if (l != nullptr && l->alive && l->fromNode == depuis && l->fromSocket == prise) {
							g.Disconnect(l->id);
						}
					}
				}
				const graph::NkNode *b = g.Find(n);
				for (uint32 j = 0; b != nullptr && j < b->sockets.Size(); ++j) {
					const graph::NkSocket &sb = b->sockets[j];
					if (sb.dir == sa.dir || sb.family != sa.family) {
						continue;
					}
					const NkString nb = sb.name;
					const graph::NkLinkError err = sort ? g.Connect(depuis, sa.name.CStr(), n, nb.CStr()) : g.Connect(n, nb.CStr(), depuis, sa.name.CStr());
					if (err == graph::NkLinkError::Ok) {
						break;
					}
				}
			}
			e.Toile().Choisir(n);
			e.choix = NkChoixBp::NK_NOEUD;
			NkEditeurBlueprintRetenir(e);
			return n;
		}

		// =====================================================================
		// Reduire en fonction
		// =====================================================================
		int32 NkEditeurBlueprintReduireEnFonction(NkEditeurBlueprintEtat &e) {
			graph::NkNodeGraph &g = e.Graphe();
			editorkit::NkEtatCanevas &t = e.Toile();
			NkVector<graph::NkNodeId> choix;
			for (uint32 i = 0; i < t.choix.Size(); ++i) {
				const graph::NkNode *n = g.Find(t.choix[i]);
				if (n == nullptr) {
					continue;
				}
				const NkGenreNoeudBp genre = NkBpGenre(*n);
				if (genre == NkGenreNoeudBp::NK_EVENEMENT || genre == NkGenreNoeudBp::NK_FN_ENTREE || genre == NkGenreNoeudBp::NK_FN_RETOUR ||
					genre == NkGenreNoeudBp::NK_COMMENTAIRE || genre == NkGenreNoeudBp::NK_REP_EVENEMENT) {
					continue;
				}
				choix.PushBack(n->id);
			}
			if (choix.Empty()) {
				return -1;
			}
			auto dedans = [&](graph::NkNodeId id) {
				for (uint32 i = 0; i < choix.Size(); ++i) {
					if (choix[i] == id) {
						return true;
					}
				}
				return false;
			};
			// Les fils qui TRAVERSENT la frontiere.
			struct Fil {
					graph::NkNodeId de, vers;
					NkString priseDe, priseVers, type;
					bool exec;
			};
			NkVector<Fil> entrants, sortants;
			float32 x0 = 1e30f, y0 = 1e30f;
			for (uint32 i = 0; i < choix.Size(); ++i) {
				const graph::NkNode *n = g.Find(choix[i]);
				x0 = n->x < x0 ? n->x : x0;
				y0 = n->y < y0 ? n->y : y0;
			}
			for (uint32 i = 0; i < g.LinkCount(); ++i) {
				const graph::NkLink *l = g.LinkAt(i);
				if (l == nullptr || !l->alive || dedans(l->fromNode) == dedans(l->toNode)) {
					continue;
				}
				const graph::NkNode *a = g.Find(l->fromNode);
				const graph::NkNode *b = g.Find(l->toNode);
				Fil f;
				f.de = a->id;
				f.vers = b->id;
				f.priseDe = a->sockets[static_cast<uint32>(l->fromSocket)].name;
				f.priseVers = b->sockets[static_cast<uint32>(l->toSocket)].name;
				f.exec = a->sockets[static_cast<uint32>(l->fromSocket)].family == graph::NkSocketFamily::Exec;
				const NkString *tn = g.TypeName(a->sockets[static_cast<uint32>(l->fromSocket)].type);
				f.type = tn != nullptr ? *tn : NkString("reel");
				(dedans(l->toNode) ? entrants : sortants).PushBack(f);
			}
			// La fonction : une entree par fil de donnee entrant, une sortie par
			// prise de donnee qui sort.
			const uint32 gf = NkBpAjouterFonction(e.doc, "FonctionReduite");
			NkGrapheBp &fn = e.doc.graphes[gf];
			NkVector<NkString> nomsEntrees, nomsSorties;
			for (uint32 i = 0; i < entrants.Size(); ++i) {
				if (entrants[i].exec) {
					continue;
				}
				NkString nom = entrants[i].priseVers;
				for (uint32 k = 2; ; ++k) {
					bool pris = false;
					for (uint32 q = 0; q < fn.entrees.Size(); ++q) {
						pris = pris || fn.entrees[q].nom == nom;
					}
					if (!pris) {
						break;
					}
					nom = NkString::Format("%s_%u", entrants[i].priseVers.CStr(), static_cast<unsigned>(k));
				}
				NkParamBp p;
				p.nom = nom;
				p.type = entrants[i].type;
				fn.entrees.PushBack(p);
				nomsEntrees.PushBack(nom);
			}
			for (uint32 i = 0; i < sortants.Size(); ++i) {
				if (sortants[i].exec) {
					nomsSorties.PushBack(NkString());
					continue;
				}
				// Une meme prise source qui part vers deux noeuds : UNE sortie.
				NkString nom;
				for (uint32 q = 0; q < i; ++q) {
					if (!sortants[q].exec && sortants[q].de == sortants[i].de && sortants[q].priseDe == sortants[i].priseDe) {
						nom = nomsSorties[q];
					}
				}
				if (nom.Empty()) {
					nom = sortants[i].priseDe;
					for (uint32 k = 2; ; ++k) {
						bool pris = false;
						for (uint32 q = 0; q < fn.sorties.Size(); ++q) {
							pris = pris || fn.sorties[q].nom == nom;
						}
						if (!pris) {
							break;
						}
						nom = NkString::Format("%s_%u", sortants[i].priseDe.CStr(), static_cast<unsigned>(k));
					}
					NkParamBp p;
					p.nom = nom;
					p.type = sortants[i].type;
					fn.sorties.PushBack(p);
				}
				nomsSorties.PushBack(nom);
			}
			NkBpRafraichirNoeuds(e.doc);
			// Les noeuds DEMENAGENT dans la fonction (copier, coller, dans l'ordre).
			graph::NkNodeGraph &gfn = e.doc.graphes[gf].graphe;
			graph::NkNodeGraph &gsrc = e.doc.graphes[e.actif].graphe;
			editorkit::NkEtatCanevas tmp;
			tmp.choix = choix;
			const NkString texte = editorkit::NkCanevasCopier(gsrc, tmp, editorkit::NkDomaineCanevas());
			editorkit::NkEtatCanevas colle;
			editorkit::NkCanevasColler(gfn, colle, texte.CStr(), 300.f, 120.f);
			auto nouveau = [&](graph::NkNodeId ancien) -> graph::NkNodeId {
				for (uint32 i = 0; i < choix.Size() && i < colle.choix.Size(); ++i) {
					if (choix[i] == ancien) {
						return colle.choix[i];
					}
				}
				return graph::NK_NODE_INVALID;
			};
			graph::NkNodeId entree = graph::NK_NODE_INVALID, retour = graph::NK_NODE_INVALID;
			for (uint32 i = 0; i < gfn.RawNodeCount(); ++i) {
				const graph::NkNode *n = gfn.RawNodeAt(i);
				if (n != nullptr && n->alive) {
					entree = n->type == NK_BP_FN_ENTREE ? n->id : entree;
					retour = n->type == NK_BP_FN_RETOUR ? n->id : retour;
				}
			}
			for (uint32 i = 0; i < gfn.LinkCount(); ++i) {
				const graph::NkLink *l = gfn.LinkAt(i);
				if (l != nullptr && l->alive && l->fromNode == entree) {
					gfn.Disconnect(l->id); // Entree -> Retour de la fonction neuve
				}
			}
			if (graph::NkNode *r = gfn.Find(retour)) {
				r->x = 300.f + 900.f;
			}
			uint32 ke = 0;
			bool execEntre = false;
			for (uint32 i = 0; i < entrants.Size(); ++i) {
				const Fil &f = entrants[i];
				if (f.exec) {
					if (!execEntre) {
						gfn.Connect(entree, "suite", nouveau(f.vers), f.priseVers.CStr());
						execEntre = true;
					}
					continue;
				}
				gfn.Connect(entree, nomsEntrees[ke++].CStr(), nouveau(f.vers), f.priseVers.CStr());
			}
			bool execSort = false;
			for (uint32 i = 0; i < sortants.Size(); ++i) {
				const Fil &f = sortants[i];
				if (f.exec) {
					if (!execSort) {
						gfn.Connect(nouveau(f.de), f.priseDe.CStr(), retour, "exec");
						execSort = true;
					}
					continue;
				}
				gfn.Connect(nouveau(f.de), f.priseDe.CStr(), retour, nomsSorties[i].CStr());
			}
			// Dans le graphe d'origine : un APPEL a leur place, les fils rebranches.
			for (uint32 i = 0; i < choix.Size(); ++i) {
				gsrc.RemoveNode(choix[i]);
			}
			const graph::NkNodeId appel = NkBpCreerAppel(e.doc, gsrc, e.doc.graphes[gf].nom.CStr(), x0, y0);
			ke = 0;
			execEntre = false;
			for (uint32 i = 0; i < entrants.Size(); ++i) {
				const Fil &f = entrants[i];
				if (f.exec) {
					gsrc.Connect(f.de, f.priseDe.CStr(), appel, "exec");
					continue;
				}
				gsrc.Connect(f.de, f.priseDe.CStr(), appel, nomsEntrees[ke++].CStr());
			}
			for (uint32 i = 0; i < sortants.Size(); ++i) {
				const Fil &f = sortants[i];
				if (f.exec) {
					gsrc.Connect(appel, "suite", f.vers, f.priseVers.CStr()); // la premiere suite gagne
					continue;
				}
				gsrc.Connect(appel, nomsSorties[i].CStr(), f.vers, f.priseVers.CStr());
			}
			Synchroniser(e);
			e.Toile().Choisir(appel);
			NkEditeurBlueprintRetenir(e);
			return static_cast<int32>(gf);
		}

		// =====================================================================
		// Chercher
		// =====================================================================
		void NkEditeurBlueprintChercher(NkEditeurBlueprintEtat &e, const char *texte) {
			e.trouves.Clear();
			if (texte == nullptr || texte[0] == '\0') {
				return;
			}
			for (uint32 i = 0; i < e.doc.variables.Size(); ++i) {
				if (editorkit::NkContientReplie(e.doc.variables[i].nom.CStr(), texte)) {
					NkLigneResultatBp l;
					l.texte = NkString::Format("Variable  %s  (%s)", e.doc.variables[i].nom.CStr(), NkBpLibelleType(e.doc.variables[i].type.CStr()));
					e.trouves.PushBack(l);
				}
			}
			for (uint32 gi = 0; gi < e.doc.graphes.Size(); ++gi) {
				const graph::NkNodeGraph &g = e.doc.graphes[gi].graphe;
				for (uint32 i = 0; i < g.RawNodeCount(); ++i) {
					const graph::NkNode *n = g.RawNodeAt(i);
					if (n == nullptr || !n->alive) {
						continue;
					}
					NkString ou;
					if (editorkit::NkContientReplie(n->label.CStr(), texte)) {
						ou = n->label;
					}
					const NkString code = NkBpCodeNoeud(g, *n);
					if (ou.Empty() && !code.Empty() && editorkit::NkContientReplie(code.CStr(), texte)) {
						ou = NkString::Format("%s : %s", n->label.CStr(), code.CStr());
					}
					for (uint32 k = 0; ou.Empty() && k < n->sockets.Size(); ++k) {
						const graph::NkGraphValue &v = n->sockets[k].defaultValue;
						if (v.IsSet() && !v.text.Empty() && editorkit::NkContientReplie(v.text.CStr(), texte)) {
							ou = NkString::Format("%s . %s = « %s »", n->label.CStr(), n->sockets[k].name.CStr(), v.text.CStr());
						}
					}
					if (!ou.Empty()) {
						NkLigneResultatBp l;
						l.texte = NkString::Format("%s  ›  %s", e.doc.graphes[gi].nom.CStr(), ou.CStr());
						l.graphe = static_cast<int32>(gi);
						l.noeud = n->id;
						e.trouves.PushBack(l);
					}
				}
			}
		}

		// =====================================================================
		// Les Details
		// =====================================================================
		namespace {
			struct Details {
					NkEditeurBlueprintEtat &e;
					nkgui::NkGuiContext &ctx;
					nkgui::NkGuiFont *f;
					NkRect r;
					float32 y = 0.f;
					float32 lh = 14.f;
					bool change = false;
					int32 nbListes = 0; ///< les listes de la trame : id -1000 - rang (bancs, captures)

					void Titre(const char *t) {
						nkgui::NkGuiDrawList &dl = ctx.dl;
						y += 6.f;
						const NkRect b{r.x, y, r.w, 24.f};
						dl.AddRectFilled(b, J().sectionFond);
						dl.AddTriangleFilled(NkVec2{b.x + 8.f, b.y + 9.f}, NkVec2{b.x + 15.f, b.y + 9.f}, NkVec2{b.x + 11.5f, b.y + 14.f}, J().triangleTitre);
						Texte(dl, f, b.x + 22.f, b.y + (24.f - lh * 0.9f) * 0.5f, t, J().sectionTexte, 0.9f);
						y += 28.f;
					}
					void Note(const char *t, NkColor c = NkColor(0, 0, 0, 0)) {
						Texte(ctx.dl, f, r.x + 12.f, y, t, c.a == 0 ? J().attenue : c, 0.82f, r.w - 20.f);
						y += lh * 0.95f;
					}
					/// Un champ de texte : rend true quand une saisie est VALIDEE.
					bool Champ(const char *lib, int32 id, const char *valeur, NkString &sortie, float32 largeurLib = 104.f) {
						nkgui::NkGuiDrawList &dl = ctx.dl;
						nkgui::NkGuiInput &in = ctx.input;
						Texte(dl, f, r.x + 12.f, y + (22.f - lh * 0.92f) * 0.5f, lib, J().texte, 0.92f, largeurLib - 8.f);
						const NkRect c{r.x + largeurLib, y, r.w - largeurLib - 10.f, 22.f};
						e.champsIds.PushBack(id);
						e.champsRects.PushBack(c);
						const bool focus = e.champ == id;
						dl.AddRectFilled(c, J().champ, 2.f);
						dl.AddRect(c, focus ? J().exec : J().champBord, 1.f, 2.f);
						bool valide = false;
						if (!focus) {
							Texte(dl, f, c.x + 6.f, c.y + (22.f - lh * 0.92f) * 0.5f, valeur, J().texte, 0.92f, c.w - 10.f);
							if (Dans(c, in.mousePos) && in.mouseClicked[0]) {
								e.champ = id;
								std::snprintf(e.tampon, sizeof(e.tampon), "%s", valeur != nullptr ? valeur : "");
								in.mouseClicked[0] = false;
							}
						} else {
							editorkit::NkOverlayFieldStyle st;
							st.fond = false;
							st.bord = false;
							st.texte = J().texteEntete;
							st.utf8 = true;
							editorkit::NkOverlayTextField(ctx, dl, f, NkRect{c.x + 2.f, c.y, c.w - 4.f, c.h}, e.tampon, static_cast<int32>(sizeof(e.tampon)), true, &st);
							if (in.KeyPressed(nkgui::NkGuiKey::Enter) || in.KeyPressed(nkgui::NkGuiKey::Tab) || (in.mouseClicked[0] && !Dans(c, in.mousePos))) {
								sortie = e.tampon;
								valide = true;
								e.champ = -1;
								in.keyInit[static_cast<int32>(nkgui::NkGuiKey::Enter)] = false;
							} else if (in.KeyPressed(nkgui::NkGuiKey::Escape)) {
								e.champ = -1;
								in.keyInit[static_cast<int32>(nkgui::NkGuiKey::Escape)] = false;
							}
						}
						y += 26.f;
						return valide;
					}
					bool Case(const char *lib, bool valeur, bool &sortie) {
						nkgui::NkGuiDrawList &dl = ctx.dl;
						Texte(dl, f, r.x + 12.f, y + (22.f - lh * 0.92f) * 0.5f, lib, J().texte, 0.92f);
						const NkRect c{r.x + r.w - 30.f, y + 4.f, 14.f, 14.f};
						dl.AddRectFilled(c, valeur ? J().exec : J().champ, 2.f);
						dl.AddRect(c, J().champBord, 1.f, 2.f);
						if (valeur) {
							dl.AddLine(NkVec2{c.x + 3.f, c.y + 7.f}, NkVec2{c.x + 6.f, c.y + 10.f}, NkColor(30, 20, 8, 255), 1.8f);
							dl.AddLine(NkVec2{c.x + 6.f, c.y + 10.f}, NkVec2{c.x + 11.f, c.y + 4.f}, NkColor(30, 20, 8, 255), 1.8f);
						}
						const NkRect zone{r.x, y, r.w, 22.f};
						y += 26.f;
						if (Dans(zone, ctx.input.mousePos) && ctx.input.mouseClicked[0]) {
							ctx.input.mouseClicked[0] = false;
							sortie = !valeur;
							return true;
						}
						return false;
					}
					/// Une liste deroulante (le type) : rend true au clic (l'appelant ouvre le menu).
					bool Liste(const char *lib, const char *type, NkRect &rect, bool grise = false) {
						nkgui::NkGuiDrawList &dl = ctx.dl;
						Texte(dl, f, r.x + 12.f, y + (22.f - lh * 0.92f) * 0.5f, lib, J().texte, 0.92f);
						rect = NkRect{r.x + 104.f, y, r.w - 114.f, 22.f};
						e.champsIds.PushBack(-1000 - nbListes++);
						e.champsRects.PushBack(rect);
						const bool sur = Dans(rect, ctx.input.mousePos);
						dl.AddRectFilled(rect, sur && !grise ? J().menuSurvol : J().champ, 2.f);
						dl.AddRect(rect, J().champBord, 1.f, 2.f);
						const NkColor ct = editorkit::NkCouleurTypeNodal(type);
						const char *gl = editorkit::NkGlypheTypeNodal(type);
						const NkRect p{rect.x + 5.f, rect.y + 5.f, Largeur(f, gl, 0.7f) + 8.f, 12.f};
						dl.AddRectFilled(p, editorkit::NkAlphaNodal(ct, 0.24f), 2.f);
						Texte(dl, f, p.x + 4.f, p.y + (12.f - lh * 0.7f) * 0.5f, gl, ct, 0.7f);
						Texte(dl, f, p.x + p.w + 6.f, rect.y + (22.f - lh * 0.92f) * 0.5f, NkBpLibelleType(type), grise ? J().attenue : J().texte, 0.92f);
						const float32 cx = rect.x + rect.w - 10.f, cy = rect.y + 11.f;
						dl.AddTriangleFilled(NkVec2{cx - 3.5f, cy - 2.f}, NkVec2{cx + 3.5f, cy - 2.f}, NkVec2{cx, cy + 2.5f}, J().attenue);
						y += 26.f;
						if (!grise && sur && ctx.input.mouseClicked[0]) {
							ctx.input.mouseClicked[0] = false;
							return true;
						}
						return false;
					}
					bool Lien(const char *lib, NkColor c = NkColor(0, 0, 0, 0)) {
						nkgui::NkGuiDrawList &dl = ctx.dl;
						const NkRect z{r.x + 10.f, y, r.w - 20.f, 22.f};
						const bool sur = Dans(z, ctx.input.mousePos);
						dl.AddRectFilled(z, sur ? NkColor(38, 38, 46, 255) : NkColor(30, 30, 36, 255), 2.f);
						const float32 lw = Largeur(f, lib, 0.88f);
						Texte(dl, f, z.x + (z.w - lw) * 0.5f, z.y + (22.f - lh * 0.88f) * 0.5f, lib, c.a == 0 ? J().attenue : c, 0.88f);
						y += 26.f;
						if (sur && ctx.input.mouseClicked[0]) {
							ctx.input.mouseClicked[0] = false;
							return true;
						}
						return false;
					}
			};

			/// Ouvre la liste des types (variable, parametre, prise de code).
			void OuvrirTypes(NkEditeurBlueprintEtat &e, uint8 genre, int32 cible, const NkRect &r, bool exec) {
				e.popup = NkPopupBp();
				e.popup.ouvert = true;
				e.popup.genre = genre;
				e.popup.cible = cible;
				e.popup.x = r.x;
				e.popup.y = r.y + r.h + 2.f;
				uint32 nb = 0;
				const char *const *t = NkBpTypesDeclarables(nb);
				for (uint32 i = 0; i < nb; ++i) {
					e.popup.libelles.PushBack(NkString(NkBpLibelleType(t[i])));
					e.popup.ids.PushBack(static_cast<int32>(i));
					e.popup.grises.PushBack(false);
				}
				if (exec) {
					e.popup.libelles.PushBack(NkString("Exécution"));
					e.popup.ids.PushBack(1000);
					e.popup.grises.PushBack(false);
				}
				e.popup.libelles.PushBack(NkString("Tableau de… (prochain lot)"));
				e.popup.ids.PushBack(-1);
				e.popup.grises.PushBack(true);
			}

			/// La valeur par defaut d'une variable : son editeur selon le type.
			void EditerDefaut(Details &D, NkVariableDeclBp &v, int32 id) {
				NkEditeurBlueprintEtat &e = D.e;
				if (v.type == "booleen") {
					bool b = v.defaut == "vrai" || v.defaut == "1" || v.defaut == "true";
					bool nb = b;
					if (D.Case("Valeur par défaut", b, nb)) {
						v.defaut = nb ? "vrai" : "faux";
						D.change = true;
					}
					return;
				}
				if (v.type == "entite") {
					D.Note("Valeur par défaut : aucune — chaque instance");
					D.Note("choisit son entité (Détails > Scripts).");
					return;
				}
				NkString sortie;
				if (D.Champ(v.type == "couleur" ? "Couleur (#RRVVBBAA)" : "Valeur par défaut", id, v.defaut.CStr(), sortie)) {
					unkeny::NkValeurBp val;
					if (NkBpValeurDeTexte(v.type.CStr(), sortie.CStr(), val)) {
						v.defaut = sortie;
						D.change = true;
					} else {
						e.message = NkString::Format("« %s » : illisible pour un %s", sortie.CStr(), NkBpLibelleType(v.type.CStr()));
						e.erreur = true;
					}
				}
				if (v.type == "couleur") {
					uint32 rvba = 0xFFFFFFFFu;
					NkBpLireCouleur(v.defaut.CStr(), rvba);
					D.ctx.dl.AddRectFilled(NkRect{D.r.x + 104.f, D.y - 2.f, D.r.w - 114.f, 8.f}, NkColor(rvba), 2.f);
					D.y += 10.f;
				}
			}

			void ParamsEditeur(Details &D, NkVector<NkParamBp> &params, const char *titre, int32 base, bool exec) {
				NkEditeurBlueprintEtat &e = D.e;
				D.Note(titre, J().texteEntete);
				for (uint32 i = 0; i < params.Size(); ++i) {
					NkString nom;
					const int32 id = base + static_cast<int32>(i);
					if (D.Champ(NkString::Format("  %u", static_cast<unsigned>(i + 1u)).CStr(), CH_PARAM + id * 2, params[i].nom.CStr(), nom, 40.f)) {
						const NkString cle = NkBpCleDeNom(nom.CStr(), 32u);
						bool pris = cle.Empty() || cle == "exec" || cle == "suite";
						for (uint32 q = 0; q < params.Size(); ++q) {
							pris = pris || (q != i && params[q].nom == cle);
						}
						if (!pris) {
							params[i].nom = cle;
							D.change = true;
						}
					}
					NkRect rl;
					D.y -= 2.f;
					if (D.Liste("      type", params[i].type.CStr(), rl)) {
						OuvrirTypes(e, POP_TYPE_PARAM, id, rl, exec);
					}
					const NkRect x{D.r.x + D.r.w - 22.f, D.y - 52.f, 16.f, 16.f};
					const bool sur = Dans(x, D.ctx.input.mousePos);
					D.ctx.dl.AddLine(NkVec2{x.x + 4.f, x.y + 4.f}, NkVec2{x.x + 12.f, x.y + 12.f}, sur ? J().erreur : J().attenue, 1.4f);
					D.ctx.dl.AddLine(NkVec2{x.x + 12.f, x.y + 4.f}, NkVec2{x.x + 4.f, x.y + 12.f}, sur ? J().erreur : J().attenue, 1.4f);
					if (sur && D.ctx.input.mouseClicked[0]) {
						params.Erase(params.Begin() + i);
						D.ctx.input.mouseClicked[0] = false;
						D.change = true;
						return;
					}
				}
				if (D.Lien(base >= 1000 ? "+  ajouter une sortie" : "+  ajouter une entrée")) {
					NkParamBp p;
					for (char c = 'a'; c <= 'z'; ++c) {
						const char n[2] = {c, '\0'};
						bool pris = false;
						for (uint32 q = 0; q < params.Size(); ++q) {
							pris = pris || params[q].nom == n;
						}
						if (!pris) {
							p.nom = n;
							break;
						}
					}
					p.type = "reel";
					params.PushBack(p);
					D.change = true;
				}
			}
		} // namespace

		namespace {
			void DessinerDetails(NkEditeurBlueprintEtat &e, nkgui::NkGuiContext &ctx, nkgui::NkGuiFont *f, const NkRect &r) {
				nkgui::NkGuiDrawList &dl = ctx.dl;
				dl.AddRectFilled(r, J().panneauFond);
				dl.AddLine(NkVec2{r.x, r.y}, NkVec2{r.x, r.y + r.h}, J().bord, 1.f);
				Texte(dl, f, r.x + 10.f, r.y + 8.f, "Détails", J().sectionTexte, 0.95f);
				dl.PushClipRect(NkRect{r.x, r.y + 30.f, r.w, r.h - 30.f});
				Details D{e, ctx, f, r};
				D.lh = Ligne(f);
				D.y = r.y + 30.f - e.defilDetails;
				if (Dans(r, ctx.input.mousePos) && ctx.input.wheel != 0.f) {
					e.defilDetails -= ctx.input.wheel * 30.f;
					e.defilDetails = e.defilDetails < 0.f ? 0.f : e.defilDetails;
					ctx.input.wheel = 0.f;
				}
				e.champsIds.Clear();
				e.champsRects.Clear();
				NkString s;
				switch (e.choix) {
					case NkChoixBp::NK_VARIABLE: {
						if (e.choixIndice < 0 || static_cast<uint32>(e.choixIndice) >= e.doc.variables.Size()) {
							break;
						}
						NkVariableDeclBp &v = e.doc.variables[static_cast<uint32>(e.choixIndice)];
						D.Titre("Variable");
						if (D.Champ("Nom", CH_NOM, v.nom.CStr(), s)) {
							if (NkBpRenommerVariable(e.doc, static_cast<uint32>(e.choixIndice), s.CStr())) {
								D.change = true;
							} else {
								e.message = NkString::Format("Nom refusé : « %s » (vide, pris, ou plus de %u caractères)", s.CStr(),
															 static_cast<unsigned>(unkeny::NK_UNKENY_VAR_NOM_MAX - 1u));
								e.erreur = true;
							}
						}
						NkRect rl;
						if (D.Liste("Type", v.type.CStr(), rl)) {
							OuvrirTypes(e, POP_TYPE_VAR, e.choixIndice, rl, false);
						}
						bool inst = v.instance;
						if (D.Case("Modifiable par instance", v.instance, inst)) {
							v.instance = inst;
							D.change = true;
						}
						if (D.Champ("Info-bulle", CH_INFOBULLE, v.infobulle.CStr(), s)) {
							v.infobulle = s;
							D.change = true;
						}
						if (D.Champ("Catégorie", CH_CATEGORIE, v.categorie.CStr(), s)) {
							v.categorie = s;
							D.change = true;
						}
						D.Titre("Valeur par défaut");
						EditerDefaut(D, v, CH_DEFAUT);
						D.y += 4.f;
						D.Note(v.instance ? "Chaque entité qui porte ce script peut la changer" : "Toutes les instances partent de cette valeur.");
						if (v.instance) {
							D.Note("dans ses Détails > Scripts.");
						}
						break;
					}
					case NkChoixBp::NK_GRAPHE: {
						if (e.choixIndice <= 0 || static_cast<uint32>(e.choixIndice) >= e.doc.graphes.Size()) {
							break;
						}
						const uint32 gi = static_cast<uint32>(e.choixIndice);
						NkGrapheBp &gr = e.doc.graphes[gi];
						D.Titre(LibelleGenreGraphe(gr.genre));
						if (D.Champ("Nom", CH_NOM, gr.nom.CStr(), s)) {
							if (NkBpRenommerGraphe(e.doc, gi, s.CStr())) {
								D.change = true;
							} else {
								e.message = NkString::Format("Nom refusé : « %s »", s.CStr());
								e.erreur = true;
							}
						}
						if (D.Champ("Description", CH_INFOBULLE, gr.infobulle.CStr(), s)) {
							gr.infobulle = s;
							D.change = true;
						}
						if (D.Champ("Catégorie", CH_CATEGORIE, gr.categorie.CStr(), s)) {
							gr.categorie = s;
							D.change = true;
						}
						if (gr.genre == NkGenreGrapheBp::NK_FONCTION) {
							bool pure = gr.pure;
							if (D.Case("Pure (sans exécution)", gr.pure, pure)) {
								gr.pure = pure;
								D.change = true;
							}
						}
						D.Titre("Entrées");
						const uint32 avantE = static_cast<uint32>(gr.entrees.Size());
						ParamsEditeur(D, gr.entrees, "", 0, gr.genre == NkGenreGrapheBp::NK_MACRO);
						D.Titre("Sorties");
						ParamsEditeur(D, gr.sorties, "", 1000, gr.genre == NkGenreGrapheBp::NK_MACRO);
						(void)avantE;
						if (D.change) {
							NkBpRafraichirNoeuds(e.doc);
						}
						break;
					}
					case NkChoixBp::NK_REPARTITEUR: {
						if (e.choixIndice < 0 || static_cast<uint32>(e.choixIndice) >= e.doc.repartiteurs.Size()) {
							break;
						}
						const uint32 ri = static_cast<uint32>(e.choixIndice);
						NkRepartiteurBp &rp = e.doc.repartiteurs[ri];
						D.Titre("Répartiteur d'événement");
						if (D.Champ("Nom", CH_NOM, rp.nom.CStr(), s)) {
							if (NkBpRenommerRepartiteur(e.doc, ri, s.CStr())) {
								D.change = true;
							}
						}
						if (D.Champ("Info-bulle", CH_INFOBULLE, rp.infobulle.CStr(), s)) {
							rp.infobulle = s;
							D.change = true;
						}
						D.Titre("Paramètres");
						ParamsEditeur(D, rp.params, "", 0, false);
						if (D.change) {
							NkBpRafraichirNoeuds(e.doc);
						}
						D.y += 4.f;
						D.Note("« Appeler » le diffuse à une entité (soi par défaut) ;");
						D.Note("ses scripts le reçoivent par « Événement ... ».");
						break;
					}
					case NkChoixBp::NK_NOEUD: {
						graph::NkNodeGraph &g = e.Graphe();
						graph::NkNode *n = g.Find(e.Toile().selection);
						if (n == nullptr) {
							D.Note("Choisissez un nœud, une variable ou une fonction.");
							break;
						}
						const NkGenreNoeudBp genre = NkBpGenre(*n);
						D.Titre(genre == NkGenreNoeudBp::NK_COMMENTAIRE ? "Cadre" : "Nœud");
						if (genre == NkGenreNoeudBp::NK_COMMENTAIRE) {
							if (D.Champ("Titre", CH_TITRE_NOEUD, n->label.CStr(), s)) {
								n->label = s;
								D.change = true;
							}
							break;
						}
						D.Note(n->label.CStr(), J().texteEntete);
						const NkProtoBp *p = NkBpProto(n->type.CStr());
						if (p != nullptr && p->aide != nullptr && p->aide[0] != '\0') {
							D.Note(p->aide);
						}
						if (genre == NkGenreNoeudBp::NK_EXPRESSION || genre == NkGenreNoeudBp::NK_SI_EXPRESSION || genre == NkGenreNoeudBp::NK_CODE) {
							D.Titre("Entrées (lues par leur nom)");
							for (uint32 k = 0; k < n->sockets.Size(); ++k) {
								const graph::NkSocket &so = n->sockets[k];
								if (so.family == graph::NkSocketFamily::Exec || so.dir != graph::NkSocketDir::Input) {
									continue;
								}
								const NkString nomPrise = so.name;
								const NkString *tn = g.TypeName(so.type);
								NkRect rl;
								if (D.Liste(nomPrise.CStr(), tn != nullptr ? tn->CStr() : "reel", rl)) {
									OuvrirTypes(e, POP_TYPE_PRISE_CODE, static_cast<int32>(k), rl, false);
								}
							}
							if (D.Lien("+  ajouter une entrée")) {
								graph::NkNodeId id = n->id;
								NkBpCodeAjouterPrise(g, id, NkBpCodeNomLibre(*n, graph::NkSocketDir::Input).CStr(), "reel", graph::NkSocketDir::Input);
								e.Toile().Choisir(id);
								D.change = true;
								break;
							}
							if (genre == NkGenreNoeudBp::NK_CODE) {
								D.Titre("Sorties (écrites par le code)");
								for (uint32 k = 0; k < n->sockets.Size(); ++k) {
									const graph::NkSocket &so = n->sockets[k];
									if (so.family == graph::NkSocketFamily::Exec || so.dir != graph::NkSocketDir::Output) {
										continue;
									}
									const NkString *tn = g.TypeName(so.type);
									NkRect rl;
									if (D.Liste(so.name.CStr(), tn != nullptr ? tn->CStr() : "reel", rl)) {
										OuvrirTypes(e, POP_TYPE_PRISE_CODE, static_cast<int32>(k), rl, false);
									}
								}
								if (D.Lien("+  ajouter une sortie")) {
									graph::NkNodeId id = n->id;
									NkBpCodeAjouterPrise(g, id, NkBpCodeNomLibre(*n, graph::NkSocketDir::Output).CStr(), "reel", graph::NkSocketDir::Output);
									e.Toile().Choisir(id);
									D.change = true;
									break;
								}
							}
							if (genre == NkGenreNoeudBp::NK_EXPRESSION) {
								for (uint32 k = 0; k < n->sockets.Size(); ++k) {
									if (n->sockets[k].dir == graph::NkSocketDir::Output) {
										const NkString *tn = g.TypeName(n->sockets[k].type);
										NkRect rl;
										D.Titre("Résultat");
										if (D.Liste("type", tn != nullptr ? tn->CStr() : "reel", rl)) {
											OuvrirTypes(e, POP_TYPE_PRISE_CODE, static_cast<int32>(k), rl, false);
										}
									}
								}
							}
							D.Titre("Le langage");
							D.Note("+ - * /  < <= > >= == !=  &&  ||  !   v.x  v.y");
							D.Note("vec2 abs min max arrondi(x, n) borner interpoler");
							D.Note("racine sin cos puissance entier reel texte");
							D.Note("afficher position teleporter impulsion par_nom…");
							D.Note("les variables et les fonctions du Blueprint");
							break;
						}
						// Les valeurs saisies des entrees libres.
						bool titre = false;
						for (uint32 k = 0; k < n->sockets.Size(); ++k) {
							const graph::NkSocket &so = n->sockets[k];
							if (so.dir != graph::NkSocketDir::Input || so.family != graph::NkSocketFamily::Data) {
								continue;
							}
							if (!titre) {
								D.Titre("Entrées");
								titre = true;
							}
							bool liee = false;
							for (uint32 q = 0; q < g.LinkCount() && !liee; ++q) {
								const graph::NkLink *l = g.LinkAt(q);
								liee = l != nullptr && l->alive && l->toNode == n->id && l->toSocket == static_cast<int32>(k);
							}
							const NkString *tn = g.TypeName(so.type);
							if (liee || (tn != nullptr && *tn == "entite")) {
								D.Note(NkString::Format("%s : %s", so.name.CStr(), liee ? "branchée" : "soi (libre)").CStr());
								continue;
							}
							const NkString nomPrise = so.name;
							if (D.Champ(nomPrise.CStr(), CH_PRISE + static_cast<int32>(k), NkBpTexteDefaut(g, *n, so).CStr(), s)) {
								if (NkBpPoserDefaut(g, n->id, nomPrise.CStr(), s.CStr())) {
									D.change = true;
								}
							}
						}
						break;
					}
					case NkChoixBp::NK_CLASSE: {
						D.Titre("Réglages de classe");
						D.Note("Classe parente : Script 2D (NkScript2D)", J().texteEntete);
						if (D.Champ("Description", CH_DESCRIPTION, e.doc.description.CStr(), s)) {
							e.doc.description = s;
							D.change = true;
						}
						if (D.Champ("Catégorie", CH_CATEGORIE, e.doc.categorie.CStr(), s)) {
							e.doc.categorie = s;
							D.change = true;
						}
						D.Titre("Contenu");
						D.Note(NkString::Format("%u variable(s), %u fonction(s) et macro(s), %u répartiteur(s)", static_cast<unsigned>(e.doc.variables.Size()),
												static_cast<unsigned>(e.doc.graphes.Size() - 1u), static_cast<unsigned>(e.doc.repartiteurs.Size()))
								   .CStr());
						D.Note(NkString::Format("Fichier : %s", e.ref.CStr()).CStr());
						break;
					}
					case NkChoixBp::NK_DEFAUTS: {
						D.Titre("Valeurs par défaut de la classe");
						if (e.doc.variables.Empty()) {
							D.Note("Aucune variable : « + » dans Mon Blueprint > Variables.");
						}
						for (uint32 i = 0; i < e.doc.variables.Size(); ++i) {
							NkVariableDeclBp &v = e.doc.variables[i];
							D.Note(NkString::Format("%s  (%s%s)", v.nom.CStr(), NkBpLibelleType(v.type.CStr()), v.instance ? ", par instance" : "").CStr(),
								   J().texteEntete);
							EditerDefaut(D, v, CH_VAR_DEFAUT + static_cast<int32>(i));
						}
						break;
					}
					default:
						D.Note("Choisissez une variable, une fonction ou un nœud.");
						D.Note("Barre d'outils : Réglages de classe, Valeurs par défaut.");
						break;
				}
				dl.PopClipRect();
				if (D.change) {
					NkEditeurBlueprintRetenir(e);
				}
			}

			// ── Le petit menu surgissant ──
			int32 DessinerPopup(NkEditeurBlueprintEtat &e, nkgui::NkGuiContext &ctx, nkgui::NkGuiFont *f) {
				NkPopupBp &p = e.popup;
				if (!p.ouvert) {
					return -2;
				}
				nkgui::NkGuiDrawList &dl = ctx.dl;
				nkgui::NkGuiInput &in = ctx.input;
				float32 w = 160.f;
				for (uint32 i = 0; i < p.libelles.Size(); ++i) {
					const float32 l = Largeur(f, p.libelles[i].CStr()) + 30.f;
					w = l > w ? l : w;
				}
				const float32 rh = 24.f;
				NkRect r{p.x, p.y, w, static_cast<float32>(p.libelles.Size()) * rh + 8.f};
				const NkRect ecran = e.zone;
				if (r.x + r.w > ecran.x + ecran.w) {
					r.x = ecran.x + ecran.w - r.w - 4.f;
				}
				if (r.y + r.h > ecran.y + ecran.h) {
					r.y = ecran.y + ecran.h - r.h - 4.f;
				}
				dl.AddRectFilled(NkRect{r.x + 3.f, r.y + 5.f, r.w, r.h}, J().menuOmbre, 4.f);
				dl.AddRectFilled(r, J().menuFond, 4.f);
				dl.AddRect(r, J().menuBord, 1.f, 4.f);
				p.rects.Clear();
				int32 choisi = -1;
				for (uint32 i = 0; i < p.libelles.Size(); ++i) {
					const NkRect ri{r.x + 4.f, r.y + 4.f + static_cast<float32>(i) * rh, r.w - 8.f, rh};
					p.rects.PushBack(ri);
					const bool sur = Dans(ri, in.mousePos) && !p.grises[i];
					if (sur) {
						dl.AddRectFilled(ri, J().menuSurvol, 3.f);
					}
					if (p.libelles[i].CStr()[0] == '-' && p.libelles[i].Length() == 1u) {
						dl.AddLine(NkVec2{ri.x + 4.f, ri.y + rh * 0.5f}, NkVec2{ri.x + ri.w - 4.f, ri.y + rh * 0.5f}, J().bord, 1.f);
						continue;
					}
					Texte(dl, f, ri.x + 10.f, ri.y + (rh - Ligne(f)) * 0.5f, p.libelles[i].CStr(), p.grises[i] ? J().attenue : J().texte);
					if (sur && in.mouseClicked[0]) {
						choisi = p.ids[i];
					}
				}
				if (in.mouseClicked[0] || in.mouseClicked[1]) {
					const bool dedans = Dans(r, in.mousePos);
					in.mouseClicked[0] = false;
					in.mouseClicked[1] = false;
					if (!dedans) {
						p.ouvert = false;
						return -2;
					}
					if (choisi >= 0) {
						p.ouvert = false;
						return choisi;
					}
				}
				if (in.KeyPressed(nkgui::NkGuiKey::Escape)) {
					p.ouvert = false;
					in.keyInit[static_cast<int32>(nkgui::NkGuiKey::Escape)] = false;
					return -2;
				}
				return choisi;
			}
		} // namespace

		// =====================================================================
		// LA TRAME
		// =====================================================================
		namespace {
			void AppliquerPopup(NkEditeurBlueprintEtat &e, uint8 genre, int32 cible, int32 id, float32 gx, float32 gy) {
				uint32 nb = 0;
				const char *const *types = NkBpTypesDeclarables(nb);
				const char *type = id >= 0 && static_cast<uint32>(id) < nb ? types[id] : (id == 1000 ? "exec" : nullptr);
				switch (genre) {
					case POP_TYPE_VAR:
						if (type != nullptr && cible >= 0 && static_cast<uint32>(cible) < e.doc.variables.Size()) {
							NkVariableDeclBp &v = e.doc.variables[static_cast<uint32>(cible)];
							if (!(v.type == type)) {
								v.type = type;
								v.defaut.Clear(); // l'ancienne valeur ne veut plus rien dire
								NkBpRafraichirNoeuds(e.doc);
								NkEditeurBlueprintRetenir(e);
							}
						}
						break;
					case POP_TYPE_PARAM: {
						NkVector<NkParamBp> *params = nullptr;
						if (e.choix == NkChoixBp::NK_GRAPHE && e.choixIndice > 0 && static_cast<uint32>(e.choixIndice) < e.doc.graphes.Size()) {
							params = cible >= 1000 ? &e.doc.graphes[static_cast<uint32>(e.choixIndice)].sorties : &e.doc.graphes[static_cast<uint32>(e.choixIndice)].entrees;
						} else if (e.choix == NkChoixBp::NK_REPARTITEUR && e.choixIndice >= 0 && static_cast<uint32>(e.choixIndice) < e.doc.repartiteurs.Size()) {
							params = &e.doc.repartiteurs[static_cast<uint32>(e.choixIndice)].params;
						}
						const uint32 k = static_cast<uint32>(cible >= 1000 ? cible - 1000 : cible);
						if (params != nullptr && type != nullptr && k < params->Size()) {
							(*params)[k].type = type;
							NkBpRafraichirNoeuds(e.doc);
							NkEditeurBlueprintRetenir(e);
						}
						break;
					}
					case POP_TYPE_PRISE_CODE: {
						graph::NkNodeGraph &g = e.Graphe();
						graph::NkNode *n = g.Find(e.Toile().selection);
						if (n != nullptr && type != nullptr && cible >= 0 && static_cast<uint32>(cible) < n->sockets.Size() && !(NkString(type) == "exec")) {
							const NkString nom = n->sockets[static_cast<uint32>(cible)].name;
							const graph::NkSocketDir dir = n->sockets[static_cast<uint32>(cible)].dir;
							graph::NkNodeId id2 = n->id;
							if (NkBpCodeTypePrise(g, id2, nom.CStr(), dir, type)) {
								e.Toile().Choisir(id2);
								NkEditeurBlueprintRetenir(e);
							}
						}
						break;
					}
					case POP_DEPOSER_VAR:
						if (cible >= 0 && static_cast<uint32>(cible) < e.doc.variables.Size()) {
							const NkString nom = e.doc.variables[static_cast<uint32>(cible)].nom;
							NkEditeurBlueprintPoser(e, (NkString(id == 0 ? "bp.var.get:" : "bp.var.set:") + nom).CStr(), gx, gy);
						}
						break;
					case POP_DEPOSER_REP:
						if (cible >= 0 && static_cast<uint32>(cible) < e.doc.repartiteurs.Size()) {
							const NkString nom = e.doc.repartiteurs[static_cast<uint32>(cible)].nom;
							NkEditeurBlueprintPoser(e, (NkString(id == 0 ? "bp.rep.appeler:" : "bp.rep.evenement:") + nom).CStr(), gx, gy);
						}
						break;
					case POP_PORTEE:
						if (id >= 0) {
							NkEditeurBlueprintOnglet(e, static_cast<uint32>(id));
						}
						break;
					case POP_SNIPPETS: {
						graph::NkNodeGraph &g = e.Graphe();
						graph::NkNode *n = g.Find(static_cast<graph::NkNodeId>(cible));
						uint32 ns = 0;
						const char *const *sn = NkBpSnippets(ns, n != nullptr && n->type == NK_BP_CODE);
						if (n != nullptr && id >= 0 && static_cast<uint32>(id) < ns) {
							NkString code = e.codeNoeud == n->id ? NkString(e.code) : NkBpCodeNoeud(g, *n);
							if (n->type == NK_BP_CODE) {
								code = code.Empty() ? NkString(sn[id]) : code + "\n" + sn[id];
							} else {
								code = sn[id]; // une expression : le snippet la remplace
							}
							if (e.codeNoeud == n->id) {
								std::snprintf(e.code, sizeof(e.code), "%s", code.CStr());
								e.caret = static_cast<uint32>(std::strlen(e.code));
							} else {
								NkBpPoserCodeNoeud(g, n->id, code.CStr());
								NkEditeurBlueprintRetenir(e);
							}
						}
						break;
					}
					case POP_ELEMENT: {
						// cible = section * 100000 + id ; id du menu : 0 renommer, 1 supprimer,
						// 2 dupliquer, 3 ouvrir.
						const int32 sec = cible / 100000, el = cible % 100000;
						if (id == 0) {
							NkString nom;
							if (sec == SEC_VARIABLES && static_cast<uint32>(el) < e.doc.variables.Size()) {
								nom = e.doc.variables[static_cast<uint32>(el)].nom;
							} else if ((sec == SEC_FONCTIONS || sec == SEC_MACROS) && static_cast<uint32>(el) < e.doc.graphes.Size()) {
								nom = e.doc.graphes[static_cast<uint32>(el)].nom;
							} else if (sec == SEC_REPARTITEURS && static_cast<uint32>(el) < e.doc.repartiteurs.Size()) {
								nom = e.doc.repartiteurs[static_cast<uint32>(el)].nom;
							}
							editorkit::NkMonBlueprintRenommer(e.mon, sec, el, nom.CStr());
						} else if (id == 1) {
							if (sec == SEC_VARIABLES) {
								NkBpSupprimerVariable(e.doc, static_cast<uint32>(el));
							} else if (sec == SEC_FONCTIONS || sec == SEC_MACROS) {
								NkBpSupprimerGraphe(e.doc, static_cast<uint32>(el));
								GrapheSupprime(e, static_cast<uint32>(el));
							} else if (sec == SEC_REPARTITEURS) {
								NkBpSupprimerRepartiteur(e.doc, static_cast<uint32>(el));
							}
							e.choix = NkChoixBp::NK_RIEN;
							NkEditeurBlueprintRetenir(e);
						} else if (id == 2 && sec == SEC_VARIABLES && static_cast<uint32>(el) < e.doc.variables.Size()) {
							NkVariableDeclBp copie = e.doc.variables[static_cast<uint32>(el)];
							copie.nom = e.doc.NomLibre(copie.nom.CStr());
							e.doc.variables.PushBack(copie);
							NkEditeurBlueprintRetenir(e);
						} else if (id == 3 && (sec == SEC_FONCTIONS || sec == SEC_MACROS || sec == SEC_GRAPHES)) {
							NkEditeurBlueprintOnglet(e, static_cast<uint32>(el));
						}
						break;
					}
					case POP_NOEUD: {
						graph::NkNodeGraph &g = e.Graphe();
						editorkit::NkEtatCanevas &t = e.Toile();
						const editorkit::NkDomaineCanevas d = NkEditeurBlueprintDomaine(e);
						switch (id) {
							case 0:
								editorkit::NkCanevasSupprimerChoix(g, t, d);
								break;
							case 1: {
								float32 bx = 0.f, by = 0.f, bw = 0.f, bh = 0.f;
								const NkString texte = editorkit::NkCanevasCopier(g, t, d);
								if (editorkit::NkCanevasBoiteChoix(g, t, J(), bx, by, bw, bh)) {
									editorkit::NkCanevasColler(g, t, texte.CStr(), bx + 56.f, by + 76.f);
								}
								break;
							}
							case 2:
								t.pressePapiers = editorkit::NkCanevasCopier(g, t, d);
								break;
							case 3:
								t.pressePapiers = editorkit::NkCanevasCopier(g, t, d);
								editorkit::NkCanevasSupprimerChoix(g, t, d);
								break;
							case 4: // rompre les liens
								for (uint32 q = 0; q < t.choix.Size(); ++q) {
									for (uint32 i = 0; i < g.LinkCount(); ++i) {
										const graph::NkLink *l = g.LinkAt(i);
										if (l != nullptr && l->alive && (l->fromNode == t.choix[q] || l->toNode == t.choix[q])) {
											g.Disconnect(l->id);
										}
									}
								}
								break;
							case 5:
								editorkit::NkCanevasAligner(g, t, 0);
								break;
							case 6:
								editorkit::NkCanevasAligner(g, t, 2);
								break;
							case 7:
								editorkit::NkCanevasRedresser(g, t, J());
								break;
							case 8: {
								float32 bx = 0.f, by = 0.f, bw = 320.f, bh = 180.f;
								editorkit::NkCanevasBoiteChoix(g, t, J(), bx, by, bw, bh);
								t.Choisir(NkBpCreerCommentaire(g, "Commentaire", bx, by, bw, bh));
								break;
							}
							case 9: {
								const int32 gf = NkEditeurBlueprintReduireEnFonction(e);
								if (gf > 0) {
									e.choix = NkChoixBp::NK_GRAPHE;
									e.choixIndice = gf;
								}
								break;
							}
							case 10: {
								// Ouvrir la fonction / la macro du noeud.
								const graph::NkNode *n = g.Find(t.selection);
								if (n != nullptr) {
									const int32 gi = n->type == NK_BP_MACRO ? e.doc.TrouverGraphe(NkBpPropTexte(g, *n, "macro").CStr(), NkGenreGrapheBp::NK_MACRO)
																			: e.doc.TrouverGraphe(NkBpPropTexte(g, *n, "fonction").CStr(), NkGenreGrapheBp::NK_FONCTION);
									if (gi > 0) {
										NkEditeurBlueprintOnglet(e, static_cast<uint32>(gi));
									}
								}
								return;
							}
							default:
								return;
						}
						NkEditeurBlueprintRetenir(e);
						break;
					}
					default:
						break;
				}
			}

			/// La console de simulation (charte NKUIDesign : « Console de simulation »).
			void Console(NkEditeurBlueprintEtat &e, NkVector<NkLigneResultatBp> &lignes) {
				lignes.Clear();
				if (e.trace == nullptr) {
					NkLigneResultatBp l;
					l.texte = "« Simuler » joue la scène et allume la trace : chaque événement s'écrit ici, la toile montre les fils qui ont servi.";
					lignes.PushBack(l);
					return;
				}
				const NkVector<unkeny::NkPassageBp> &ps = e.trace->passages;
				for (uint32 i = ps.Size() > 40u ? ps.Size() - 40u : 0u; i < ps.Size(); ++i) {
					const unkeny::NkPassageBp &p = ps[i];
					NkString ev = "événement";
					const NkVector<NkProtoBp> &protos = NkBpProtos();
					for (uint32 k = 0; k < protos.Size(); ++k) {
						if (protos[k].genre == NkGenreNoeudBp::NK_EVENEMENT && protos[k].evenement == p.genre) {
							ev = protos[k].libelle;
						}
					}
					if (p.genre == NK_UNK_EV_PERSONNALISE) {
						ev = NkString("Événement ") + p.parametre;
					}
					NkString dernier = "?";
					const uint32 gi = NkBpGrapheDuCode(p.dernier);
					if (gi < e.doc.graphes.Size()) {
						if (const graph::NkNode *n = e.doc.graphes[gi].graphe.Find(NkBpNoeudDuCode(p.dernier))) {
							dernier = n->label;
						}
					}
					NkLigneResultatBp l;
					l.genre = p.faute ? 2u : 1u;
					l.texte = NkString::Format("%7.2f s   %-12s   %s  ->  %s   (%u nœuds)%s", static_cast<double>(p.temps), p.entite.CStr(), ev.CStr(), dernier.CStr(),
											   static_cast<unsigned>(p.nbNoeuds), p.faute ? "   FAUTE" : "");
					l.graphe = static_cast<int32>(gi);
					l.noeud = NkBpNoeudDuCode(p.dernier);
					lignes.PushBack(l);
				}
				if (lignes.Empty()) {
					NkLigneResultatBp l;
					l.texte = "Aucun événement n'est encore passé par ce Blueprint.";
					lignes.PushBack(l);
				}
			}

			void AllerAuNoeud(NkEditeurBlueprintEtat &e, int32 graphe, graph::NkNodeId n) {
				if (graphe < 0 || static_cast<uint32>(graphe) >= e.doc.graphes.Size()) {
					return;
				}
				NkEditeurBlueprintOnglet(e, static_cast<uint32>(graphe));
				editorkit::NkEtatCanevas &t = e.toiles[static_cast<uint32>(graphe)];
				const graph::NkNode *p = e.doc.graphes[static_cast<uint32>(graphe)].graphe.Find(n);
				if (p == nullptr) {
					return;
				}
				t.Choisir(n);
				e.choix = NkChoixBp::NK_NOEUD;
				NkEditeurBlueprintMontrer(e, t, *p);
				e.cadrer[static_cast<uint32>(graphe)] = false;
			}

			/// L'edition du CODE d'un noeud : le clavier, puis valider.
			void EditionCode(NkEditeurBlueprintEtat &e, nkgui::NkGuiInput &in) {
				if (e.codeNoeud == graph::NK_NODE_INVALID) {
					return;
				}
				auto valider = [&]() {
					if (e.codeGraphe < e.doc.graphes.Size()) {
						graph::NkNodeGraph &g = e.doc.graphes[e.codeGraphe].graphe;
						if (const graph::NkNode *n = g.Find(e.codeNoeud)) {
							if (!(NkBpCodeNoeud(g, *n) == e.code)) {
								NkBpPoserCodeNoeud(g, e.codeNoeud, e.code);
								NkEditeurBlueprintRetenir(e);
							}
						}
					}
					e.codeNoeud = graph::NK_NODE_INVALID;
				};
				uint32 n = static_cast<uint32>(std::strlen(e.code));
				e.caret = e.caret > n ? n : e.caret;
				for (int32 i = 0; i < in.charCount; ++i) {
					const uint32 cp = in.chars[i];
					if (cp < 32u) {
						continue;
					}
					char b[4];
					uint32 nb = 0;
					if (cp < 0x80u) {
						b[nb++] = static_cast<char>(cp);
					} else if (cp < 0x800u) {
						b[nb++] = static_cast<char>(0xC0u | (cp >> 6));
						b[nb++] = static_cast<char>(0x80u | (cp & 0x3Fu));
					} else {
						b[nb++] = static_cast<char>(0xE0u | (cp >> 12));
						b[nb++] = static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
						b[nb++] = static_cast<char>(0x80u | (cp & 0x3Fu));
					}
					if (n + nb + 1u >= sizeof(e.code)) {
						break;
					}
					std::memmove(e.code + e.caret + nb, e.code + e.caret, n - e.caret + 1u);
					std::memcpy(e.code + e.caret, b, nb);
					e.caret += nb;
					n += nb;
				}
				in.charCount = 0;
				auto avant = [&](uint32 p) {
					while (p > 0u && (static_cast<unsigned char>(e.code[p - 1u]) & 0xC0u) == 0x80u) {
						--p;
					}
					return p > 0u ? p - 1u : 0u;
				};
				auto apres = [&](uint32 p) {
					if (p >= n) {
						return n;
					}
					++p;
					while (p < n && (static_cast<unsigned char>(e.code[p]) & 0xC0u) == 0x80u) {
						++p;
					}
					return p;
				};
				if (in.KeyPressedRepeat(nkgui::NkGuiKey::Backspace) && e.caret > 0u) {
					const uint32 a = avant(e.caret);
					std::memmove(e.code + a, e.code + e.caret, n - e.caret + 1u);
					e.caret = a;
				}
				n = static_cast<uint32>(std::strlen(e.code));
				if (in.KeyPressedRepeat(nkgui::NkGuiKey::Delete) && e.caret < n) {
					const uint32 b = apres(e.caret);
					std::memmove(e.code + e.caret, e.code + b, n - b + 1u);
				}
				if (in.KeyPressedRepeat(nkgui::NkGuiKey::Left) && e.caret > 0u) {
					e.caret = avant(e.caret); // le debut du caractere precedent
				}
				if (in.KeyPressedRepeat(nkgui::NkGuiKey::Right)) {
					e.caret = apres(e.caret);
				}
				if (in.KeyPressed(nkgui::NkGuiKey::Home)) {
					while (e.caret > 0u && e.code[e.caret - 1u] != '\n') {
						--e.caret;
					}
				}
				if (in.KeyPressed(nkgui::NkGuiKey::End)) {
					n = static_cast<uint32>(std::strlen(e.code));
					while (e.caret < n && e.code[e.caret] != '\n') {
						++e.caret;
					}
				}
				if (in.KeyPressed(nkgui::NkGuiKey::Enter)) {
					if (in.ctrlDown) {
						valider();
					} else {
						n = static_cast<uint32>(std::strlen(e.code));
						if (n + 2u < sizeof(e.code)) {
							std::memmove(e.code + e.caret + 1u, e.code + e.caret, n - e.caret + 1u);
							e.code[e.caret++] = '\n';
						}
					}
				}
				if (in.KeyPressed(nkgui::NkGuiKey::Escape)) {
					valider();
				}
				e.clignote = in.KeyPressed(nkgui::NkGuiKey::Left) || in.KeyPressed(nkgui::NkGuiKey::Right) ? 0.f : e.clignote;
				for (int32 k = 0; k < nkgui::NkGuiInput::KeyCount; ++k) {
					in.keyInit[k] = false; // toutes les touches sont au code
				}
			}
		} // namespace

		bool NkEditeurBlueprintDessiner(NkEditeurBlueprintEtat &e, NkHoteBlueprint &hote, nkgui::NkGuiContext &ctx, nkgui::NkGuiFont *f, const NkRect &r) {
			Synchroniser(e);
			nkgui::NkGuiDrawList &dl = ctx.dl;
			nkgui::NkGuiInput &in = ctx.input;
			const NkJetonsNodal &s = J();
			const float32 lh = Ligne(f);
			e.zone = r;
			e.police = f;
			const bool surEditeur = Dans(r, in.mousePos);
			e.trace = hote.Trace != nullptr ? hote.Trace(hote.donnees, e.ref.CStr()) : nullptr;
			e.simulation = hote.EnJeu != nullptr && hote.EnJeu(hote.donnees);
			bool clavier = false;

			// Un MENU ou une fenetre surgissante ouverte garde la souris : les panneaux
			// dessines AVANT elle (barre, Mon Blueprint, Details, toile) ne voient pas
			// ses clics -- sinon le clic sur « Réel » d'un choix de type tombait aussi
			// sur la carte des Details dessous, et le choix etait perdu. Un clic hors
			// d'elle la ferme seulement (comme UE5). Les clics reviennent au point 7.
			const bool flottant = e.menu.ouvert || e.popup.ouvert;
			bool clicsGardes[3] = {in.mouseClicked[0], in.mouseClicked[1], in.mouseClicked[2]};
			bool doubleGarde = in.mouseDoubleClicked[0];
			const float32 moletteGardee = in.wheel;
			if (flottant) {
				in.mouseClicked[0] = in.mouseClicked[1] = in.mouseClicked[2] = false;
				in.mouseDoubleClicked[0] = false;
				in.wheel = 0.f;
			}

			// ── 0. Le CODE d'un noeud en cours d'edition a le clavier ──
			if (e.codeNoeud != graph::NK_NODE_INVALID) {
				// Un clic HORS de son bloc valide l'edition.
				if (in.mouseClicked[0] && e.codeGraphe == e.actif) {
					const graph::NkNode *n = e.Graphe().Find(e.codeNoeud);
					bool dansBloc = false;
					if (n != nullptr) {
						for (uint32 i = 0; i < e.Toile().geom.Size(); ++i) {
							const editorkit::NkGeomNoeud &gg = e.Toile().geom[i];
							if (gg.id == n->id) {
								const NkRect rn = editorkit::NkCanevasRectNoeud(e.Toile(), e.zoneToile, *n, s);
								dansBloc = Dans(NkRect{rn.x, rn.y + gg.blocY * e.Toile().zoom, rn.w, gg.blocH * e.Toile().zoom}, in.mousePos);
							}
						}
					}
					if (!dansBloc) {
						if (e.codeGraphe < e.doc.graphes.Size()) {
							graph::NkNodeGraph &g = e.doc.graphes[e.codeGraphe].graphe;
							if (g.Find(e.codeNoeud) != nullptr && !(NkBpCodeNoeud(g, *g.Find(e.codeNoeud)) == e.code)) {
								NkBpPoserCodeNoeud(g, e.codeNoeud, e.code);
								NkEditeurBlueprintRetenir(e);
							}
						}
						e.codeNoeud = graph::NK_NODE_INVALID;
					}
				}
				if (e.codeNoeud != graph::NK_NODE_INVALID) {
					EditionCode(e, in);
					clavier = true;
				}
			}

			// ── 1. La MISE EN PAGE ──
			const float32 hb = 42.f, lm = 268.f, ld = 330.f, hbas = 170.f, ho = 28.f;
			const NkRect barre{r.x, r.y, r.w, hb};
			e.rectMon = NkRect{r.x, r.y + hb, lm, r.h - hb - hbas};
			e.rectDetails = NkRect{r.x + r.w - ld, r.y + hb, ld, r.h - hb - hbas};
			e.rectBas = NkRect{r.x, r.y + r.h - hbas, r.w, hbas};
			const NkRect onglets{e.rectMon.x + lm, r.y + hb, r.w - lm - ld, ho};
			e.zoneToile = NkRect{onglets.x, onglets.y + ho, onglets.w, r.h - hb - hbas - ho};
			dl.PushClipRect(r);
			dl.AddRectFilled(r, s.fond);

			// ── 2. La BARRE D'OUTILS ──
			dl.AddRectFilled(barre, s.panneauFond);
			dl.AddLine(NkVec2{barre.x, barre.y + hb - 0.5f}, NkVec2{barre.x + barre.w, barre.y + hb - 0.5f}, s.bord, 1.f);
			{
				float32 x = barre.x + 10.f;
				const float32 by = barre.y + 7.f, bh = hb - 14.f;
				// Compiler : son ETAT (a compiler / compile / en erreur).
				e.boutons[NK_BP_COMPILER] = NkRect{x, by, 116.f, bh};
				if (Bouton(ctx, f, e.boutons[NK_BP_COMPILER], "     Compiler", true)) {
					NkEditeurBlueprintCompiler(e, hote);
				}
				{
					const NkVec2 c{x + 16.f, by + bh * 0.5f};
					const NkColor col = e.etatCompil == 1u ? NkColor(92, 196, 98, 255) : (e.etatCompil == 2u ? s.erreur : NkColor(232, 196, 64, 255));
					dl.AddCircleFilled(c, 8.f, col);
					if (e.etatCompil == 1u) {
						dl.AddLine(NkVec2{c.x - 4.f, c.y}, NkVec2{c.x - 1.f, c.y + 3.f}, NkColor(16, 30, 16, 255), 2.f);
						dl.AddLine(NkVec2{c.x - 1.f, c.y + 3.f}, NkVec2{c.x + 4.5f, c.y - 3.5f}, NkColor(16, 30, 16, 255), 2.f);
					} else if (e.etatCompil == 2u) {
						dl.AddLine(NkVec2{c.x - 3.5f, c.y - 3.5f}, NkVec2{c.x + 3.5f, c.y + 3.5f}, NkColor(40, 10, 10, 255), 2.f);
						dl.AddLine(NkVec2{c.x + 3.5f, c.y - 3.5f}, NkVec2{c.x - 3.5f, c.y + 3.5f}, NkColor(40, 10, 10, 255), 2.f);
					} else {
						Texte(dl, f, c.x - Largeur(f, "?", 0.9f) * 0.5f, c.y - lh * 0.45f, "?", NkColor(40, 32, 8, 255), 0.9f);
					}
				}
				x += 116.f + 8.f;
				e.boutons[NK_BP_ENREGISTRER] = NkRect{x, by, 104.f, bh};
				if (Bouton(ctx, f, e.boutons[NK_BP_ENREGISTRER], e.modifie ? "Enregistrer •" : "Enregistrer")) {
					NkEditeurBlueprintCompiler(e, hote); // enregistrer = compiler et ecrire (le module suit le graphe)
				}
				x += 104.f + 8.f;
				e.boutons[NK_BP_RECHERCHER] = NkRect{x, by, 100.f, bh};
				if (Bouton(ctx, f, e.boutons[NK_BP_RECHERCHER], "Rechercher", false, e.ongletBas == 1)) {
					e.ongletBas = 1;
					e.rechercheFocus = true;
				}
				x += 100.f + 18.f;
				dl.AddLine(NkVec2{x - 9.f, by + 3.f}, NkVec2{x - 9.f, by + bh - 3.f}, s.bord, 1.f);
				e.boutons[NK_BP_REGLAGES] = NkRect{x, by, 150.f, bh};
				if (Bouton(ctx, f, e.boutons[NK_BP_REGLAGES], "Réglages de classe", false, e.choix == NkChoixBp::NK_CLASSE)) {
					e.choix = NkChoixBp::NK_CLASSE;
				}
				x += 150.f + 8.f;
				e.boutons[NK_BP_DEFAUTS] = NkRect{x, by, 150.f, bh};
				if (Bouton(ctx, f, e.boutons[NK_BP_DEFAUTS], "Valeurs par défaut", false, e.choix == NkChoixBp::NK_DEFAUTS)) {
					e.choix = NkChoixBp::NK_DEFAUTS;
				}
				x += 150.f + 18.f;
				dl.AddLine(NkVec2{x - 9.f, by + 3.f}, NkVec2{x - 9.f, by + bh - 3.f}, s.bord, 1.f);
				e.boutons[NK_BP_SIMULER] = NkRect{x, by, 112.f, bh};
				if (Bouton(ctx, f, e.boutons[NK_BP_SIMULER], e.simulation ? "     Arrêter" : "     Simuler", false, e.simulation)) {
					if (hote.Simuler != nullptr) {
						if (!e.simulation && e.etatCompil != 1u) {
							NkEditeurBlueprintCompiler(e, hote); // on simule ce qu'on voit
						}
						hote.Simuler(hote.donnees, !e.simulation);
						e.ongletBas = 2;
					}
				}
				{
					const float32 cx = x + 18.f, cy = by + bh * 0.5f;
					if (e.simulation) {
						dl.AddRectFilled(NkRect{cx - 5.f, cy - 5.f, 10.f, 10.f}, s.erreur, 1.f);
					} else {
						dl.AddTriangleFilled(NkVec2{cx - 4.f, cy - 6.f}, NkVec2{cx + 6.f, cy}, NkVec2{cx - 4.f, cy + 6.f}, NkColor(92, 196, 98, 255));
					}
				}
				// A droite : le titre, et Fermer.
				e.boutons[NK_BP_FERMER] = NkRect{barre.x + barre.w - 84.f, by, 74.f, bh};
				if (Bouton(ctx, f, e.boutons[NK_BP_FERMER], "Fermer")) {
					if (hote.Fermer != nullptr) {
						dl.PopClipRect();
						hote.Fermer(hote.donnees);
						return true;
					}
				}
				const NkString titre = NkString::Format("%s  ·  Blueprint (Script 2D)%s", e.nom.CStr(), e.modifie ? "  •" : "");
				const float32 tw = Largeur(f, titre.CStr());
				Texte(dl, f, barre.x + barre.w - 96.f - tw, barre.y + (hb - lh) * 0.5f, titre.CStr(), s.sectionTexte);
			}

			// ── 3. MON BLUEPRINT ──
			NkVector<editorkit::NkSectionPanneau> sections;
			{
				editorkit::NkSectionPanneau gr;
				gr.titre = "Graphes";
				gr.plus = false;
				editorkit::NkElementPanneau ev;
				ev.libelle = e.doc.graphes[0].nom;
				ev.id = 0;
				gr.elements.PushBack(ev);
				sections.PushBack(gr);
				editorkit::NkSectionPanneau fn, ma, va, re;
				fn.titre = "Fonctions";
				ma.titre = "Macros";
				va.titre = "Variables";
				re.titre = "Répartiteurs d'événements";
				for (uint32 i = 1; i < e.doc.graphes.Size(); ++i) {
					editorkit::NkElementPanneau el;
					el.libelle = e.doc.graphes[i].nom;
					el.id = static_cast<int32>(i);
					NkString sig;
					for (uint32 k = 0; k < e.doc.graphes[i].entrees.Size(); ++k) {
						sig += k > 0u ? NkString(", ") : NkString();
						sig += e.doc.graphes[i].entrees[k].nom;
					}
					el.sousTexte = NkString::Format("%s(%s)", e.doc.graphes[i].pure ? "pure " : "", sig.CStr());
					(e.doc.graphes[i].genre == NkGenreGrapheBp::NK_MACRO ? ma : fn).elements.PushBack(el);
				}
				for (uint32 i = 0; i < e.doc.variables.Size(); ++i) {
					editorkit::NkElementPanneau el;
					el.libelle = e.doc.variables[i].nom;
					el.id = static_cast<int32>(i);
					el.pastille = true;
					el.couleur = editorkit::NkCouleurTypeNodal(e.doc.variables[i].type.CStr());
					el.glyphe = editorkit::NkGlypheTypeNodal(e.doc.variables[i].type.CStr());
					el.sousTexte = NkString(NkBpLibelleType(e.doc.variables[i].type.CStr()));
					el.oeil = e.doc.variables[i].instance; // l'oeil d'UE5 : modifiable par instance
					va.elements.PushBack(el);
				}
				for (uint32 i = 0; i < e.doc.repartiteurs.Size(); ++i) {
					editorkit::NkElementPanneau el;
					el.libelle = e.doc.repartiteurs[i].nom;
					el.id = static_cast<int32>(i);
					el.sousTexte = NkString::Format("%u param.", static_cast<unsigned>(e.doc.repartiteurs[i].params.Size()));
					re.elements.PushBack(el);
				}
				sections.PushBack(fn);
				sections.PushBack(ma);
				sections.PushBack(va);
				sections.PushBack(re);
			}
			// Le choix de Mon Blueprint suit celui des Details.
			const editorkit::NkResultatMonBlueprint mon = editorkit::NkMonBlueprint(ctx, dl, f, e.rectMon, sections, e.mon, s, "Mon Blueprint");
			dl.AddLine(NkVec2{e.rectMon.x + e.rectMon.w - 0.5f, e.rectMon.y}, NkVec2{e.rectMon.x + e.rectMon.w - 0.5f, e.rectMon.y + e.rectMon.h}, s.bord, 1.f);
			clavier = clavier || mon.clavierPris;
			if (mon.ajouter >= 0) {
				switch (mon.ajouter) {
					case SEC_FONCTIONS: {
						const uint32 g = NkBpAjouterFonction(e.doc);
						Synchroniser(e);
						NkEditeurBlueprintOnglet(e, g);
						e.choix = NkChoixBp::NK_GRAPHE;
						e.choixIndice = static_cast<int32>(g);
						e.mon.choixSection = SEC_FONCTIONS;
						e.mon.choixId = static_cast<int32>(g);
						editorkit::NkMonBlueprintRenommer(e.mon, SEC_FONCTIONS, static_cast<int32>(g), e.doc.graphes[g].nom.CStr());
						break;
					}
					case SEC_MACROS: {
						const uint32 g = NkBpAjouterMacro(e.doc);
						Synchroniser(e);
						NkEditeurBlueprintOnglet(e, g);
						e.choix = NkChoixBp::NK_GRAPHE;
						e.choixIndice = static_cast<int32>(g);
						editorkit::NkMonBlueprintRenommer(e.mon, SEC_MACROS, static_cast<int32>(g), e.doc.graphes[g].nom.CStr());
						break;
					}
					case SEC_VARIABLES: {
						const uint32 v = NkBpAjouterVariable(e.doc);
						e.choix = NkChoixBp::NK_VARIABLE;
						e.choixIndice = static_cast<int32>(v);
						e.mon.choixSection = SEC_VARIABLES;
						e.mon.choixId = static_cast<int32>(v);
						editorkit::NkMonBlueprintRenommer(e.mon, SEC_VARIABLES, static_cast<int32>(v), e.doc.variables[v].nom.CStr());
						break;
					}
					case SEC_REPARTITEURS: {
						const uint32 rr = NkBpAjouterRepartiteur(e.doc);
						e.choix = NkChoixBp::NK_REPARTITEUR;
						e.choixIndice = static_cast<int32>(rr);
						editorkit::NkMonBlueprintRenommer(e.mon, SEC_REPARTITEURS, static_cast<int32>(rr), e.doc.repartiteurs[rr].nom.CStr());
						break;
					}
					default:
						break;
				}
				NkEditeurBlueprintRetenir(e);
			}
			if (mon.choisiSection >= 0 && !mon.renomme) {
				switch (mon.choisiSection) {
					case SEC_GRAPHES:
						e.choix = NkChoixBp::NK_CLASSE;
						if (mon.doubleClic) {
							NkEditeurBlueprintOnglet(e, 0u);
						}
						break;
					case SEC_FONCTIONS:
					case SEC_MACROS:
						e.choix = NkChoixBp::NK_GRAPHE;
						e.choixIndice = mon.choisiId;
						if (mon.doubleClic) {
							NkEditeurBlueprintOnglet(e, static_cast<uint32>(mon.choisiId));
						}
						break;
					case SEC_VARIABLES:
						e.choix = NkChoixBp::NK_VARIABLE;
						e.choixIndice = mon.choisiId;
						break;
					case SEC_REPARTITEURS:
						e.choix = NkChoixBp::NK_REPARTITEUR;
						e.choixIndice = mon.choisiId;
						break;
					default:
						break;
				}
				e.champ = -1;
			}
			if (mon.renomme) {
				bool ok = false;
				if (mon.choisiSection == SEC_VARIABLES) {
					ok = NkBpRenommerVariable(e.doc, static_cast<uint32>(mon.choisiId), mon.nouveauNom.CStr());
				} else if (mon.choisiSection == SEC_FONCTIONS || mon.choisiSection == SEC_MACROS) {
					ok = NkBpRenommerGraphe(e.doc, static_cast<uint32>(mon.choisiId), mon.nouveauNom.CStr());
				} else if (mon.choisiSection == SEC_REPARTITEURS) {
					ok = NkBpRenommerRepartiteur(e.doc, static_cast<uint32>(mon.choisiId), mon.nouveauNom.CStr());
				}
				if (ok) {
					NkEditeurBlueprintRetenir(e);
				} else {
					e.message = NkString::Format("Nom refusé : « %s » (vide, déjà pris, ou trop long)", mon.nouveauNom.CStr());
					e.erreur = true;
				}
			}
			if (mon.supprimer) {
				AppliquerPopup(e, POP_ELEMENT, mon.choisiSection * 100000 + mon.choisiId, 1, 0.f, 0.f);
			}
			if (mon.menu && mon.choisiSection > SEC_GRAPHES) {
				e.popup = NkPopupBp();
				e.popup.ouvert = true;
				e.popup.genre = POP_ELEMENT;
				e.popup.x = mon.menuX;
				e.popup.y = mon.menuY;
				e.popup.cible = mon.choisiSection * 100000 + mon.choisiId;
				const char *libs[] = {"Renommer      F2", "Supprimer      Suppr", "Dupliquer", "Ouvrir"};
				for (int32 i = 0; i < 4; ++i) {
					const bool utile = (i != 2 || mon.choisiSection == SEC_VARIABLES) && (i != 3 || mon.choisiSection == SEC_FONCTIONS || mon.choisiSection == SEC_MACROS);
					if (utile) {
						e.popup.libelles.PushBack(NkString(libs[i]));
						e.popup.ids.PushBack(i);
						e.popup.grises.PushBack(false);
					}
				}
			}
			// Le GLISSER-DEPOSER d'un element sur la toile (UE5 : Ctrl = lire, Alt = ecrire).
			if (mon.depose && Dans(e.zoneToile, NkVec2{mon.x, mon.y})) {
				const NkVec2 p = editorkit::NkCanevasDepuisEcran(e.Toile(), e.zoneToile, NkVec2{mon.x, mon.y});
				if (mon.glisseSection == SEC_VARIABLES && static_cast<uint32>(mon.glisseId) < e.doc.variables.Size()) {
					const NkString nom = e.doc.variables[static_cast<uint32>(mon.glisseId)].nom;
					if (in.ctrlDown || in.altDown) {
						NkEditeurBlueprintPoser(e, (NkString(in.ctrlDown ? "bp.var.get:" : "bp.var.set:") + nom).CStr(), p.x, p.y);
					} else {
						e.popup = NkPopupBp();
						e.popup.ouvert = true;
						e.popup.genre = POP_DEPOSER_VAR;
						e.popup.x = mon.x;
						e.popup.y = mon.y;
						e.popup.gx = p.x;
						e.popup.gy = p.y;
						e.popup.cible = mon.glisseId;
						e.popup.libelles.PushBack(NkString("Lire ") + nom);
						e.popup.libelles.PushBack(NkString("Écrire ") + nom);
						e.popup.ids.PushBack(0);
						e.popup.ids.PushBack(1);
						e.popup.grises.PushBack(false);
						e.popup.grises.PushBack(false);
					}
				} else if ((mon.glisseSection == SEC_FONCTIONS || mon.glisseSection == SEC_MACROS) && static_cast<uint32>(mon.glisseId) < e.doc.graphes.Size()) {
					const NkGrapheBp &g = e.doc.graphes[static_cast<uint32>(mon.glisseId)];
					NkEditeurBlueprintPoser(e, (NkString(g.genre == NkGenreGrapheBp::NK_MACRO ? "bp.macro:" : "bp.appel:") + g.nom).CStr(), p.x, p.y);
				} else if (mon.glisseSection == SEC_REPARTITEURS && static_cast<uint32>(mon.glisseId) < e.doc.repartiteurs.Size()) {
					e.popup = NkPopupBp();
					e.popup.ouvert = true;
					e.popup.genre = POP_DEPOSER_REP;
					e.popup.x = mon.x;
					e.popup.y = mon.y;
					e.popup.gx = p.x;
					e.popup.gy = p.y;
					e.popup.cible = mon.glisseId;
					const NkString nom = e.doc.repartiteurs[static_cast<uint32>(mon.glisseId)].nom;
					e.popup.libelles.PushBack(NkString("Appeler ") + nom);
					e.popup.libelles.PushBack(NkString("Événement ") + nom);
					e.popup.ids.PushBack(0);
					e.popup.ids.PushBack(1);
					e.popup.grises.PushBack(false);
					e.popup.grises.PushBack(e.actif != 0u);
				}
			}

			// ── 4. Les ONGLETS de graphe et la TOILE ──
			dl.AddRectFilled(onglets, s.panneauFond);
			e.rectsOnglets.Clear();
			{
				float32 x = onglets.x + 6.f;
				for (uint32 i = 0; i < e.onglets.Size(); ++i) {
					const uint32 gi = e.onglets[i];
					const NkString nom = e.doc.graphes[gi].nom;
					const float32 w = Largeur(f, nom.CStr(), 0.92f) + (gi > 0u ? 44.f : 28.f);
					const NkRect ro{x, onglets.y + 3.f, w, ho - 3.f};
					e.rectsOnglets.PushBack(ro);
					const bool act = gi == e.actif;
					dl.AddRectFilled(ro, act ? s.fond : (Dans(ro, in.mousePos) ? s.elementSurvol : s.sectionFond), 3.f);
					if (act) {
						dl.AddRectFilled(NkRect{ro.x, ro.y, ro.w, 2.f}, s.exec);
					}
					Texte(dl, f, ro.x + 12.f, ro.y + (ro.h - lh * 0.92f) * 0.5f, nom.CStr(), act ? s.texteEntete : s.attenue, 0.92f);
					bool ferme = false;
					if (gi > 0u) {
						const NkRect rx{ro.x + ro.w - 20.f, ro.y + (ro.h - 14.f) * 0.5f, 14.f, 14.f};
						const bool surx = Dans(rx, in.mousePos);
						dl.AddLine(NkVec2{rx.x + 3.f, rx.y + 3.f}, NkVec2{rx.x + 11.f, rx.y + 11.f}, surx ? s.texteEntete : s.attenue, 1.3f);
						dl.AddLine(NkVec2{rx.x + 11.f, rx.y + 3.f}, NkVec2{rx.x + 3.f, rx.y + 11.f}, surx ? s.texteEntete : s.attenue, 1.3f);
						if (surx && in.mouseClicked[0]) {
							e.onglets.Erase(e.onglets.Begin() + i);
							e.actif = e.actif == gi ? e.onglets[0] : e.actif;
							in.mouseClicked[0] = false;
							ferme = true;
						}
					}
					if (!ferme && Dans(ro, in.mousePos) && in.mouseClicked[0]) {
						e.actif = gi;
						in.mouseClicked[0] = false;
					}
					if (ferme) {
						break;
					}
					x += w + 4.f;
				}
			}
			{
				editorkit::NkEtatCanevas &t = e.Toile();
				t.portee = e.doc.graphes[e.actif].nom;
				t.clavierAilleurs = clavier || e.champ >= 0 || e.rechercheFocus || e.menu.ouvert || e.popup.ouvert || mon.clavierPris;
				// Un menu ouvert garde la souris : la toile ne la voit pas.
				nkgui::NkGuiInput copie = in;
				const bool masque = e.menu.ouvert || e.popup.ouvert || Dans(e.rectMon, in.mousePos) || Dans(e.rectDetails, in.mousePos);
				if (masque) {
					in.mouseClicked[0] = in.mouseClicked[1] = in.mouseClicked[2] = false;
					in.mouseDoubleClicked[0] = false;
					in.wheel = 0.f;
				}
				const editorkit::NkDomaineCanevas d = NkEditeurBlueprintDomaine(e);
				const graph::NkNodeId avantChoix = t.selection;
				// Le fil lache reste visible tant que son menu est ouvert (UE5).
				const bool attente = e.menu.ouvert && e.menuPrise >= 0 && e.menuNoeud != graph::NK_NODE_INVALID;
				t.filAttenteNoeud = attente ? e.menuNoeud : graph::NK_NODE_INVALID;
				t.filAttentePrise = attente ? e.menuPrise : -1;
				t.filAttenteX = e.menuX;
				t.filAttenteY = e.menuY;
				editorkit::NkCanevasNoeuds(dl, in, f, e.zoneToile, e.Graphe(), t, s, d);
				if (masque) {
					in.mouseClicked[0] = copie.mouseClicked[0];
					in.mouseClicked[1] = copie.mouseClicked[1];
					in.mouseClicked[2] = copie.mouseClicked[2];
					in.mouseDoubleClicked[0] = copie.mouseDoubleClicked[0];
					in.wheel = copie.wheel;
				}
				clavier = clavier || t.clavierPris;
				// Cadrer APRES le dessin : la geometrie des noeuds est mesuree (sinon
				// les tailles par defaut font un zoom faux). La vue suit a la trame suivante.
				if (e.cadrer[e.actif] && e.zoneToile.w > 50.f && e.zoneToile.h > 50.f && !t.geom.Empty()) {
					editorkit::NkCanevasCadrer(t, e.zoneToile, e.Graphe(), s);
					e.cadrer[e.actif] = false;
				}
				if (t.modifie) {
					NkBpRafraichirNoeuds(e.doc); // un noeud de code refait : ses appels suivent
					NkEditeurBlueprintRetenir(e);
				}
				if (t.selection != avantChoix && t.selection != graph::NK_NODE_INVALID) {
					e.choix = NkChoixBp::NK_NOEUD;
					e.champ = -1;
					// Un noeud de variable choisi : la variable aussi (dans Mon Blueprint).
					if (const graph::NkNode *n = e.Graphe().Find(t.selection)) {
						if (n->type == NK_BP_VAR_LIRE || n->type == NK_BP_VAR_ECRIRE) {
							const int32 v = e.doc.TrouverVariable(NkBpPropTexte(e.Graphe(), *n, "var").CStr());
							e.mon.choixSection = SEC_VARIABLES;
							e.mon.choixId = v;
						}
					}
				}
				switch (t.demande) {
					case editorkit::NkDemandeCanevas::NK_MENU_TOILE:
						e.menuX = t.menuX;
						e.menuY = t.menuY;
						e.menuNoeud = graph::NK_NODE_INVALID;
						e.menuPrise = -1;
						e.menu.Ouvrir(t.menuEcranX, t.menuEcranY);
						e.menu.sensibleContexte = true;
						break;
					case editorkit::NkDemandeCanevas::NK_MENU_PRISE: {
						e.menuX = t.menuX;
						e.menuY = t.menuY;
						e.menuNoeud = t.menuNoeud;
						e.menuPrise = t.menuPrise;
						const graph::NkNode *n = e.Graphe().Find(t.menuNoeud);
						NkString ctxTexte;
						if (n != nullptr && t.menuPrise >= 0) {
							const graph::NkSocket &so = n->sockets[static_cast<uint32>(t.menuPrise)];
							const NkString *tn = e.Graphe().TypeName(so.type);
							const char *type = so.family == graph::NkSocketFamily::Exec ? "exec" : (tn != nullptr ? tn->CStr() : "?");
							ctxTexte = so.dir == graph::NkSocketDir::Output ? "qui acceptent" : "qui fournissent";
							e.menu.Ouvrir(t.menuEcranX, t.menuEcranY, ctxTexte.CStr());
							e.menu.contexteGlyphe = NkString(editorkit::NkGlypheTypeNodal(type)) + "  " + NkBpLibelleType(type);
							e.menu.contexteCouleur = editorkit::NkCouleurTypeNodal(type);
							e.menu.sensibleContexte = true;
						}
						break;
					}
					case editorkit::NkDemandeCanevas::NK_MENU_NOEUD: {
						e.popup = NkPopupBp();
						e.popup.ouvert = true;
						e.popup.genre = POP_NOEUD;
						e.popup.x = t.menuEcranX;
						e.popup.y = t.menuEcranY;
						const graph::NkNode *n = e.Graphe().Find(t.menuNoeud);
						const bool ouvrable = n != nullptr && (n->type == NK_BP_APPEL || n->type == NK_BP_MACRO);
						const char *libs[] = {"Supprimer      Suppr", "Dupliquer      Ctrl+D", "Copier      Ctrl+C", "Couper      Ctrl+X", "Rompre les liens",
											  "Aligner à gauche      Maj+A", "Aligner en haut      Maj+W", "Redresser      Q", "Cadre autour      C",
											  "Réduire en fonction", "Ouvrir son graphe"};
						for (int32 i = 0; i < 11; ++i) {
							if (i == 10 && !ouvrable) {
								continue;
							}
							e.popup.libelles.PushBack(NkString(libs[i]));
							e.popup.ids.PushBack(i);
							e.popup.grises.PushBack(i == 9 && e.doc.graphes[e.actif].genre != NkGenreGrapheBp::NK_EVENEMENTS && e.actif != 0u);
						}
						break;
					}
					case editorkit::NkDemandeCanevas::NK_DOUBLE_CLIC: {
						const graph::NkNode *n = e.Graphe().Find(t.menuNoeud);
						if (n != nullptr && (n->type == NK_BP_APPEL || n->type == NK_BP_MACRO)) {
							AppliquerPopup(e, POP_NOEUD, -1, 10, 0.f, 0.f);
						} else if (n != nullptr && (n->type == NK_BP_VAR_LIRE || n->type == NK_BP_VAR_ECRIRE)) {
							e.choix = NkChoixBp::NK_VARIABLE;
							e.choixIndice = e.doc.TrouverVariable(NkBpPropTexte(e.Graphe(), *n, "var").CStr());
						}
						break;
					}
					case editorkit::NkDemandeCanevas::NK_PORTEE: {
						e.popup = NkPopupBp();
						e.popup.ouvert = true;
						e.popup.genre = POP_PORTEE;
						e.popup.x = t.menuEcranX;
						e.popup.y = t.menuEcranY;
						for (uint32 i = 0; i < e.doc.graphes.Size(); ++i) {
							e.popup.libelles.PushBack(NkString::Format("%s   (%s)", e.doc.graphes[i].nom.CStr(), LibelleGenreGraphe(e.doc.graphes[i].genre)));
							e.popup.ids.PushBack(static_cast<int32>(i));
							e.popup.grises.PushBack(false);
						}
						break;
					}
					default:
						break;
				}
			}
			// Le fantome d'un glisser depuis Mon Blueprint.
			if (mon.glisseEnCours) {
				NkString lib;
				if (mon.glisseSection == SEC_VARIABLES && static_cast<uint32>(mon.glisseId) < e.doc.variables.Size()) {
					lib = e.doc.variables[static_cast<uint32>(mon.glisseId)].nom;
				} else if ((mon.glisseSection == SEC_FONCTIONS || mon.glisseSection == SEC_MACROS) && static_cast<uint32>(mon.glisseId) < e.doc.graphes.Size()) {
					lib = e.doc.graphes[static_cast<uint32>(mon.glisseId)].nom;
				} else if (mon.glisseSection == SEC_REPARTITEURS && static_cast<uint32>(mon.glisseId) < e.doc.repartiteurs.Size()) {
					lib = e.doc.repartiteurs[static_cast<uint32>(mon.glisseId)].nom;
				}
				const float32 w = Largeur(f, lib.CStr()) + 24.f;
				const NkRect g{mon.x + 12.f, mon.y + 6.f, w, 22.f};
				dl.AddRectFilled(g, editorkit::NkAlphaNodal(s.corps, 0.92f), 11.f);
				dl.AddRect(g, s.exec, 1.f, 11.f);
				Texte(dl, f, g.x + 12.f, g.y + (22.f - lh) * 0.5f, lib.CStr(), s.texteEntete);
				if (mon.glisseSection == SEC_VARIABLES) {
					Texte(dl, f, g.x, g.y + 26.f, "Ctrl : lire  ·  Alt : écrire", s.attenue, 0.8f);
				}
			}

			// ── 5. Les DETAILS ──
			DessinerDetails(e, ctx, f, e.rectDetails);
			clavier = clavier || e.champ >= 0;

			// ── 6. EN BAS : compilation, recherche, console de simulation ──
			{
				const NkRect b = e.rectBas;
				dl.AddRectFilled(b, s.panneauFond);
				dl.AddLine(NkVec2{b.x, b.y + 0.5f}, NkVec2{b.x + b.w, b.y + 0.5f}, s.bord, 1.f);
				const char *noms[3] = {"Résultats de compilation", "Résultats de recherche", "Console de simulation"};
				float32 x = b.x + 8.f;
				for (int32 i = 0; i < 3; ++i) {
					const float32 w = Largeur(f, noms[i], 0.9f) + 22.f;
					const NkRect ro{x, b.y + 4.f, w, 22.f};
					const bool act = e.ongletBas == i;
					dl.AddRectFilled(ro, act ? s.sectionFond : s.panneauFond, 3.f);
					if (act) {
						dl.AddRectFilled(NkRect{ro.x, ro.y + ro.h - 2.f, ro.w, 2.f}, s.exec);
					}
					Texte(dl, f, ro.x + 11.f, ro.y + (22.f - lh * 0.9f) * 0.5f, noms[i], act ? s.texteEntete : s.attenue, 0.9f);
					if (Dans(ro, in.mousePos) && in.mouseClicked[0]) {
						e.ongletBas = i;
						in.mouseClicked[0] = false;
					}
					x += w + 4.f;
				}
				float32 y = b.y + 32.f;
				NkVector<NkLigneResultatBp> console;
				const NkVector<NkLigneResultatBp> *lignes = &e.resultats;
				if (e.ongletBas == 1) {
					const NkRect rr{b.x + 10.f, y, 340.f, 22.f};
					dl.AddRectFilled(rr, s.champ, 2.f);
					dl.AddRect(rr, e.rechercheFocus ? s.exec : s.champBord, 1.f, 2.f);
					if (in.mouseClicked[0] && Dans(b, in.mousePos)) {
						e.rechercheFocus = Dans(rr, in.mousePos);
					}
					if (e.recherche[0] == '\0' && !e.rechercheFocus) {
						Texte(dl, f, rr.x + 6.f, rr.y + (22.f - lh * 0.88f) * 0.5f, "Chercher dans ce Blueprint (Ctrl+F)", s.attenue, 0.88f);
					}
					char avant[64];
					std::snprintf(avant, sizeof(avant), "%s", e.recherche);
					editorkit::NkOverlayFieldStyle st;
					st.fond = false;
					st.bord = false;
					st.texte = s.texteEntete;
					st.utf8 = true;
					editorkit::NkOverlayTextField(ctx, dl, f, NkRect{rr.x + 2.f, rr.y, rr.w - 4.f, rr.h}, e.recherche, static_cast<int32>(sizeof(e.recherche)),
												  e.rechercheFocus, &st);
					if (std::strcmp(avant, e.recherche) != 0) {
						NkEditeurBlueprintChercher(e, e.recherche);
					}
					if (e.rechercheFocus) {
						clavier = true;
						if (in.KeyPressed(nkgui::NkGuiKey::Escape)) {
							e.rechercheFocus = false;
						}
					}
					Texte(dl, f, rr.x + rr.w + 12.f, rr.y + (22.f - lh * 0.88f) * 0.5f,
						  NkString::Format("%u résultat(s)", static_cast<unsigned>(e.trouves.Size())).CStr(), s.attenue, 0.88f);
					y += 28.f;
					lignes = &e.trouves;
				} else if (e.ongletBas == 2) {
					Console(e, console);
					lignes = &console;
					if (e.trace != nullptr) {
						Texte(dl, f, b.x + b.w - 330.f, b.y + 8.f,
							  NkString::Format("%u événement(s) tracé(s)%s", static_cast<unsigned>(e.trace->evenements), e.simulation ? "  ·  en jeu" : "").CStr(),
							  s.attenue, 0.86f);
					}
				} else if (e.resultats.Empty()) {
					Texte(dl, f, b.x + 12.f, y, e.etatCompil == 0u ? "À compiler : « Compiler » (F5) ou Ctrl+S." : e.message.CStr(), s.attenue, 0.9f);
				}
				dl.PushClipRect(NkRect{b.x, y, b.w, b.y + b.h - y});
				if (Dans(NkRect{b.x, y, b.w, b.y + b.h - y}, in.mousePos) && in.wheel != 0.f) {
					e.defilBas -= in.wheel * 20.f;
					e.defilBas = e.defilBas < 0.f ? 0.f : e.defilBas;
					in.wheel = 0.f;
				}
				float32 yy = y - (e.ongletBas == 2 ? 0.f : e.defilBas);
				if (e.ongletBas == 2 && lignes->Size() * 20u > static_cast<uint32>(b.y + b.h - y)) {
					yy = b.y + b.h - static_cast<float32>(lignes->Size()) * 20.f - 4.f; // la console suit le dernier
				}
				for (uint32 i = 0; i < lignes->Size(); ++i) {
					const NkLigneResultatBp &l = (*lignes)[i];
					const NkRect rl{b.x + 6.f, yy, b.w - 12.f, 20.f};
					const bool sur = Dans(rl, in.mousePos) && l.graphe >= 0;
					if (sur) {
						dl.AddRectFilled(rl, s.elementSurvol, 2.f);
					}
					const NkColor c = l.genre == 2u ? s.erreur : (l.genre == 1u ? NkColor(110, 200, 120, 255) : s.texte);
					// La marque DESSINEE (la police n'a ni ✓ ni ✗).
					if (l.genre == 2u) {
						editorkit::NkIconeCroix(dl, rl.x + 12.f, rl.y + 10.f, 12.f, c);
					} else if (l.genre == 1u) {
						editorkit::NkIconeCoche(dl, rl.x + 12.f, rl.y + 10.f, 12.f, c);
					} else {
						dl.AddCircleFilled(NkVec2{rl.x + 12.f, rl.y + 10.f}, 2.f, s.attenue);
					}
					Texte(dl, f, rl.x + 24.f, rl.y + (20.f - lh * 0.9f) * 0.5f, l.texte.CStr(), l.genre == 2u ? c : s.texte, 0.9f, rl.w - 30.f);
					if (sur && in.mouseClicked[0]) {
						AllerAuNoeud(e, l.graphe, l.noeud);
						in.mouseClicked[0] = false;
					}
					yy += 20.f;
				}
				dl.PopClipRect();
			}

			// ── 7. Les MENUS, par-dessus tout ──
			if (flottant) {
				in.mouseClicked[0] = clicsGardes[0];
				in.mouseClicked[1] = clicsGardes[1];
				in.mouseClicked[2] = clicsGardes[2];
				in.mouseDoubleClicked[0] = doubleGarde;
				in.wheel = moletteGardee;
			}
			if (e.menu.ouvert) {
				NkEditeurBlueprintEntrees(e, e.menu.sensibleContexte && e.menuNoeud != graph::NK_NODE_INVALID, e.menuNoeud, e.menuPrise, e.menuEntrees);
				NkVector<editorkit::NkEntreeMenuNoeud> entrees;
				for (uint32 i = 0; i < e.menuEntrees.Size(); ++i) {
					editorkit::NkEntreeMenuNoeud x;
					x.libelle = e.menuEntrees[i].libelle;
					x.categorie = e.menuEntrees[i].categorie;
					x.aide = e.menuEntrees[i].aide;
					x.couleur = CouleurCategorie(e.menuEntrees[i].categorie);
					x.id = static_cast<int32>(i);
					entrees.PushBack(x);
				}
				dl.PushClipRect(r);
				const int32 choix = editorkit::NkMenuNoeuds(ctx, dl, f, e.menu, entrees, s, "Toutes les actions pour ce Blueprint");
				dl.PopClipRect();
				if (choix >= 0 && static_cast<uint32>(choix) < e.menuEntrees.Size()) {
					const NkString cle = e.menuEntrees[static_cast<uint32>(choix)].cle;
					NkEditeurBlueprintPoser(e, cle.CStr(), e.menuX, e.menuY, e.menuNoeud, e.menuPrise);
				}
				clavier = true;
			}
			if (e.popup.ouvert) {
				const uint8 genre = e.popup.genre;
				const int32 cible = e.popup.cible;
				const float32 gx = e.popup.gx, gy = e.popup.gy;
				const int32 id = DessinerPopup(e, ctx, f);
				if (id >= 0 || (id == -1 && !e.popup.ouvert)) {
					if (id >= 0) {
						AppliquerPopup(e, genre, cible, id, gx, gy);
					}
				}
			}

			// ── 8. Le CLAVIER de l'editeur (la souris sur lui, aucun champ) ──
			if (surEditeur && !clavier) {
				if (in.ctrlDown && in.KeyPressed(nkgui::NkGuiKey::S)) {
					NkEditeurBlueprintCompiler(e, hote);
					in.keyInit[static_cast<int32>(nkgui::NkGuiKey::S)] = false;
				} else if (in.KeyPressed(nkgui::NkGuiKey::F5)) {
					NkEditeurBlueprintCompiler(e, hote);
				} else if (in.ctrlDown && in.KeyPressed(nkgui::NkGuiKey::Z)) {
					if (in.shiftDown ? e.historique.Redo(e.doc) : e.historique.Undo(e.doc)) {
						Synchroniser(e);
						e.modifie = true;
						e.etatCompil = 0u;
					}
					in.keyInit[static_cast<int32>(nkgui::NkGuiKey::Z)] = false;
				} else if (in.ctrlDown && in.KeyPressed(nkgui::NkGuiKey::Y)) {
					if (e.historique.Redo(e.doc)) {
						Synchroniser(e);
						e.modifie = true;
						e.etatCompil = 0u;
					}
					in.keyInit[static_cast<int32>(nkgui::NkGuiKey::Y)] = false;
				} else if (in.ctrlDown && in.KeyPressed(nkgui::NkGuiKey::F)) {
					e.ongletBas = 1;
					e.rechercheFocus = true;
					in.keyInit[static_cast<int32>(nkgui::NkGuiKey::F)] = false;
				}
			}
			// Le message (un nom refuse...) : une ligne au bas de la toile.
			if (e.erreur && !e.message.Empty() && e.etatCompil != 2u) {
				Texte(dl, f, e.zoneToile.x + 64.f, e.zoneToile.y + e.zoneToile.h - lh - 30.f, e.message.CStr(), s.erreur, 0.92f, e.zoneToile.w - 80.f);
			}
			dl.PopClipRect();
			// L'editeur garde la souris : rien ne traverse vers l'application.
			if (surEditeur) {
				in.mouseClicked[0] = in.mouseClicked[1] = in.mouseClicked[2] = false;
				in.wheel = 0.f;
			}
			return clavier || surEditeur;
		}

	} // namespace editeur
} // namespace nkentseu
