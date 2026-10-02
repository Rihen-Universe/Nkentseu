// =============================================================================
// NkEditeurBancBlueprint.cpp — le banc de l'EDITEUR DE BLUEPRINT a la UE5
// (2026-10-01, demande de Rihen : « declaration de variables, menu contextuel,
// creer les methodes... a la Unreal Engine 5 »). Lance par --selftest (a la
// suite du banc des scripts de l'editeur).
//
// PRE-ENREGISTREMENT (ecrit AVANT le premier chiffre) :
//   (u1)  un document : variable « compteur » (entier, 5, par instance), une
//         fonction « Doubler(x) -> r » (r = x * 2), « Debut -> compteur =
//         Doubler(compteur) -> Afficher("compteur = " + compteur) » : COMPILE ;
//         joue : compteur vaut 10 dans le composant, le journal le dit
//   (u1n) CONTRE-EPREUVE : le meme sans le fil du Retour -> 0 (le resultat
//         n'est pas invente) ; et la fonction mutee (x * 3) -> 15, pas 10
//   (u2)  la MEME classe sur deux entites, « compteur » 5 et 7 : 10 et 14
//         (une valeur PAR INSTANCE)
//   (u3)  une fonction qui applique une force, appelee sous Tick : refusee,
//         l'erreur DESIGNE l'appel dans le graphe d'evenements
//   (u4)  une fonction PURE appelee deux fois dans une meme expression
//   (u5)  une fonction qui s'appelle sans fin : FAUTE « pile d'appels », la
//         scene continue, le noeud est designe
//   (u6)  enregistrer / rouvrir : le meme document (instantane identique) ;
//         l'ANCIEN .nkbp de la porte s'ouvre en document d'un seul graphe et
//         compile
//   (u7)  renommer une variable : ses noeuds suivent ; changer son type : les
//         noeuds sont refaits, le fil incompatible tombe
//   (u8)  macro inseree DEUX fois ; boucle Pour 1..10 = 55 ; variables texte,
//         couleur, entite ; un REPARTITEUR appele sur une autre entite
//   (u9)  la PORTE reecrite (variables declarees + fonction OuvrirPorte) :
//         sur deux zones avec deux portes, chacune ouvre LA SIENNE
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Script/NkBpCatalogue.h"
#include "Script/NkBpDocument.h"
#include "Script/NkBpExpression.h"
#include "Script/NkEditeurScripts.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKGui/Core/NkGuiDrawListRaster.h"
#include "NKMemory/NKMemory.h"
#include "Unkeny/Scene/NkUnkenyScene.h"
#include "Unkeny/Script/NkUnkenyScripts.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		using namespace nkentseu::unkeny;

		namespace {
			int32 gR = 0, gE = 0;
			void Temoin(bool ok, const char *quoi, float32 v) {
				std::printf("  [%s] %-72s %10.4f\n", ok ? " OK " : "ECHEC", quoi, static_cast<double>(v));
				(ok ? gR : gE)++;
			}
			float32 Absf(float32 v) {
				return v < 0.f ? -v : v;
			}
			graph::NkNodeId Trouver(const graph::NkNodeGraph &g, const char *type) {
				for (uint32 i = 0; i < g.RawNodeCount(); ++i) {
					const graph::NkNode *n = g.RawNodeAt(i);
					if (n != nullptr && n->alive && n->type == type) {
						return n->id;
					}
				}
				return graph::NK_NODE_INVALID;
			}
			/// Coupe les fils de la prise `prise` (sortie) de `n` : une sortie
			/// d'execution n'a qu'une suite, il faut la liberer avant de la rebrancher.
			void Couper(graph::NkNodeGraph &g, graph::NkNodeId n, const char *prise) {
				const graph::NkNode *p = g.Find(n);
				const int32 k = p != nullptr ? p->FindSocket(prise, graph::NkSocketDir::Output) : -1;
				for (uint32 i = 0; i < g.LinkCount(); ++i) {
					const graph::NkLink *l = g.LinkAt(i);
					if (l != nullptr && l->alive && l->fromNode == n && l->fromSocket == k) {
						g.Disconnect(l->id);
					}
				}
			}
			bool JournalContient(const NkHoteScripts2D &h, const char *motif) {
				for (uint32 i = 0; i < h.Journal().Size(); ++i) {
					if (std::strstr(h.Journal()[i].texte.CStr(), motif) != nullptr) {
						return true;
					}
				}
				return false;
			}
			NkString Temporaire() {
				const char *t = std::getenv("TEMP");
				if (t == nullptr) {
					t = std::getenv("TMPDIR");
				}
				NkString d = t != nullptr ? NkString(t) : NkString("/tmp");
				for (usize i = 0; i < d.Length(); ++i) {
					if (d.CStr()[i] == '\\') {
						const_cast<char *>(d.CStr())[i] = '/';
					}
				}
				return d + "/unkeny_banc_blueprint/";
			}

			/// Une scene, un registre, un hote : joue `pas` pas fixes.
			struct Monde {
					memory::NkAllocator &al = memory::NkGetDefaultAllocator();
					NkScene *s = nullptr;
					NkScripts2D *r = nullptr;
					NkHoteScripts2D *h = nullptr;
					explicit Monde(bool physique = false) {
						s = al.New<NkScene>();
						NkSceneConfig cfg;
						cfg.physique = physique;
						s->Init(cfg);
						r = al.New<NkScripts2D>();
						h = al.New<NkHoteScripts2D>();
						h->Brancher(*s, *r);
					}
					~Monde() {
						h->Arreter();
						al.Delete(h);
						al.Delete(r);
						al.Delete(s);
					}
					ecs::NkEntityId Avec(const char *nom, const char *script, const NkVec2f &p = NkVec2f(0.f, 0.f)) {
						const ecs::NkEntityId id = s->Creer(nom, p);
						NkScript2D sc;
						NkScriptAjouter(sc, script);
						s->Monde().Add<NkScript2D>(id, sc);
						return id;
					}
					void Jouer(int32 pas) {
						for (int32 k = 0; k < pas; ++k) {
							s->Pas(1.f / 60.f);
						}
					}
					const NkVarScript *Var(ecs::NkEntityId id, const char *nom) {
						const NkScript2D *sc = s->Monde().Get<NkScript2D>(id);
						return sc != nullptr ? NkScriptVariable(*sc, 0, nom) : nullptr;
					}
			};

			/// Le document de (u1) : compteur, Doubler, Debut.
			void DocumentDoubler(NkDocumentBp &d, int32 facteur, bool filRetour) {
				d.Vider();
				const uint32 v = NkBpAjouterVariable(d, "compteur", "entier");
				d.variables[v].defaut = "5";
				d.variables[v].instance = true;
				const uint32 gf = NkBpAjouterFonction(d, "Doubler");
				NkParamBp x;
				x.nom = "x";
				x.type = "entier";
				d.graphes[gf].entrees.PushBack(x);
				NkParamBp r;
				r.nom = "r";
				r.type = "entier";
				d.graphes[gf].sorties.PushBack(r);
				NkBpRafraichirNoeuds(d);
				graph::NkNodeGraph &f = d.graphes[gf].graphe;
				const graph::NkNodeId entree = Trouver(f, NK_BP_FN_ENTREE);
				const graph::NkNodeId retour = Trouver(f, NK_BP_FN_RETOUR);
				const graph::NkNodeId mul = NkBpCreerNoeud(f, "bp.math.mul_i", 200.f, 120.f);
				NkBpPoserDefaut(f, mul, "b", NkString::Format("%d", facteur).CStr());
				f.Connect(entree, "x", mul, "a");
				if (filRetour) {
					f.Connect(mul, "r", retour, "r");
				}
				graph::NkNodeGraph &e = d.graphes[0].graphe;
				const graph::NkNodeId debut = NkBpCreerNoeud(e, "bp.ev.debut", 0.f, 0.f);
				const graph::NkNodeId lire = NkBpCreerParCle(d, e, "bp.var.get:compteur", 0.f, 120.f);
				const graph::NkNodeId appel = NkBpCreerParCle(d, e, "bp.appel:Doubler", 240.f, 0.f);
				const graph::NkNodeId ecrire = NkBpCreerParCle(d, e, "bp.var.set:compteur", 480.f, 0.f);
				const graph::NkNodeId txt = NkBpCreerNoeud(e, "bp.natif:unkeny.texte.de_entier", 480.f, 160.f);
				const graph::NkNodeId cat = NkBpCreerNoeud(e, "bp.natif:unkeny.texte.concatener", 700.f, 160.f);
				NkBpPoserDefaut(e, cat, "a", "compteur = ");
				const graph::NkNodeId aff = NkBpCreerNoeud(e, "bp.natif:unkeny.journal.afficher", 720.f, 0.f);
				e.Connect(debut, "suite", appel, "exec");
				e.Connect(lire, "valeur", appel, "x");
				e.Connect(appel, "suite", ecrire, "exec");
				e.Connect(appel, "r", ecrire, "valeur");
				e.Connect(ecrire, "suite", aff, "exec");
				e.Connect(ecrire, "valeur", txt, "valeur");
				e.Connect(txt, "texte", cat, "b");
				e.Connect(cat, "texte", aff, "texte");
			}

			/// Compile, enregistre au registre sous `nom` ; rend l'erreur.
			NkString Charger(Monde &w, const NkDocumentBp &d, const char *nom) {
				unkeny::NkModuleBp m;
				NkErreurBp e;
				if (!NkBpCompilerDocument(d, m, e)) {
					return e.message.Empty() ? NkString("?") : e.message;
				}
				NkString refus;
				w.r->EnregistrerBlueprint(nom, m, &refus);
				return refus;
			}
		} // namespace

		int32 NkEditeurLancerBancBlueprintModele() {
			// ── (u1) variable + fonction, compilees et executees ──
			{
				NkDocumentBp d;
				DocumentDoubler(d, 2, true);
				Monde w;
				const NkString e = Charger(w, d, "Doubler.nkbp");
				const ecs::NkEntityId a = w.Avec("A", "Doubler.nkbp");
				w.Jouer(2);
				const NkVarScript *c = w.Var(a, "compteur");
				std::printf("    (u1) %s compteur=%g\n", e.CStr(), c != nullptr ? static_cast<double>(c->valeur.x) : -1.0);
				Temoin(e.Empty() && c != nullptr && c->valeur.x == 10.f && JournalContient(*w.h, "[A] compteur = 10"),
					   "(u1) variable + fonction Doubler : compteur 5 -> 10, le journal le dit", c != nullptr ? c->valeur.x : -1.f);
			}
			{
				NkDocumentBp d;
				DocumentDoubler(d, 2, false);
				Monde w;
				const NkString e = Charger(w, d, "SansRetour.nkbp");
				const ecs::NkEntityId a = w.Avec("A", "SansRetour.nkbp");
				w.Jouer(2);
				const NkVarScript *c = w.Var(a, "compteur");
				NkDocumentBp d3;
				DocumentDoubler(d3, 3, true);
				Monde w3;
				Charger(w3, d3, "Tripler.nkbp");
				const ecs::NkEntityId b = w3.Avec("B", "Tripler.nkbp");
				w3.Jouer(2);
				const NkVarScript *c3 = w3.Var(b, "compteur");
				Temoin(e.Empty() && c != nullptr && c->valeur.x == 0.f && c3 != nullptr && c3->valeur.x == 15.f,
					   "(u1n) sans fil de Retour : 0 ; fonction mutee x*3 : 15 (le banc VOIT la difference)", c3 != nullptr ? c3->valeur.x : -1.f);
			}
			// ── (u2) une valeur PAR INSTANCE ──
			{
				NkDocumentBp d;
				DocumentDoubler(d, 2, true);
				Monde w;
				Charger(w, d, "Doubler.nkbp");
				const ecs::NkEntityId a = w.Avec("A", "Doubler.nkbp");
				const ecs::NkEntityId b = w.Avec("B", "Doubler.nkbp");
				NkScriptPoserVariable(*w.s->Monde().Get<NkScript2D>(b), 0, "compteur", NkTypeVarScript::NK_ENTIER, NkVec2f(7.f, 0.f));
				w.Jouer(2);
				const NkVarScript *ca = w.Var(a, "compteur");
				const NkVarScript *cb = w.Var(b, "compteur");
				Temoin(ca != nullptr && cb != nullptr && ca->valeur.x == 10.f && cb->valeur.x == 14.f,
					   "(u2) la meme classe, compteur 5 et 7 par instance : 10 et 14", cb != nullptr ? cb->valeur.x : -1.f);
			}
			// ── (u3) une force dans une fonction appelee sous Tick : refusee, sur l'APPEL ──
			{
				NkDocumentBp d;
				const uint32 gf = NkBpAjouterFonction(d, "Pousser");
				graph::NkNodeGraph &f = d.graphes[gf].graphe;
				const graph::NkNodeId force = NkBpCreerNoeud(f, "bp.natif:unkeny.corps.force", 220.f, 80.f);
				NkBpPoserDefaut(f, force, "force", "0 10");
				const graph::NkNodeId entree = Trouver(f, NK_BP_FN_ENTREE);
				const graph::NkNodeId retour = Trouver(f, NK_BP_FN_RETOUR);
				Couper(f, entree, "suite");
				f.Connect(entree, "suite", force, "exec");
				f.Connect(force, "suite", retour, "exec");
				graph::NkNodeGraph &e = d.graphes[0].graphe;
				const graph::NkNodeId tick = NkBpCreerNoeud(e, "bp.ev.tick", 0.f, 0.f);
				const graph::NkNodeId appel = NkBpCreerParCle(d, e, "bp.appel:Pousser", 260.f, 0.f);
				e.Connect(tick, "suite", appel, "exec");
				unkeny::NkModuleBp m;
				NkErreurBp err;
				const bool ok = NkBpCompilerDocument(d, m, err);
				std::printf("    (u3) %s (graphe %u, noeud %u ; appel %u)\n", err.message.CStr(), static_cast<unsigned>(err.graphe),
							static_cast<unsigned>(err.noeud), static_cast<unsigned>(appel));
				// Sous « Pas fixe », la meme fonction passe.
				graph::NkNode *t = e.Find(tick);
				NkDocumentBp d2 = d;
				graph::NkNodeGraph &e2 = d2.graphes[0].graphe;
				const graph::NkNodeId pf = NkBpCreerNoeud(e2, "bp.ev.pas_fixe", 0.f, 200.f);
				e2.RemoveNode(t != nullptr ? t->id : graph::NK_NODE_INVALID);
				e2.Connect(pf, "suite", appel, "exec");
				NkErreurBp err2;
				const bool ok2 = NkBpCompilerDocument(d2, m, err2);
				Temoin(!ok && err.graphe == 0u && err.noeud == appel && std::strstr(err.message.CStr(), "pas permis") != nullptr && ok2,
					   "(u3) force dans une fonction appelee sous Tick : refusee SUR L'APPEL ; sous Pas fixe : acceptee",
					   static_cast<float32>(err.noeud));
			}
			// ── (u4) une fonction PURE, deux fois dans une expression ──
			{
				NkDocumentBp d;
				const uint32 v = NkBpAjouterVariable(d, "somme", "reel");
				(void)v;
				const uint32 gf = NkBpAjouterFonction(d, "Carre");
				d.graphes[gf].pure = true;
				NkParamBp x;
				x.nom = "x";
				x.type = "reel";
				d.graphes[gf].entrees.PushBack(x);
				NkParamBp r;
				r.nom = "r";
				r.type = "reel";
				d.graphes[gf].sorties.PushBack(r);
				NkBpRafraichirNoeuds(d);
				graph::NkNodeGraph &f = d.graphes[gf].graphe;
				const graph::NkNodeId mul = NkBpCreerNoeud(f, "bp.math.mul_r", 200.f, 120.f);
				f.Connect(Trouver(f, NK_BP_FN_ENTREE), "x", mul, "a");
				f.Connect(Trouver(f, NK_BP_FN_ENTREE), "x", mul, "b");
				f.Connect(mul, "r", Trouver(f, NK_BP_FN_RETOUR), "r");
				graph::NkNodeGraph &e = d.graphes[0].graphe;
				const graph::NkNodeId debut = NkBpCreerNoeud(e, "bp.ev.debut", 0.f, 0.f);
				const graph::NkNodeId c3 = NkBpCreerParCle(d, e, "bp.appel:Carre", 0.f, 150.f);
				const graph::NkNodeId c4 = NkBpCreerParCle(d, e, "bp.appel:Carre", 0.f, 260.f);
				NkBpPoserDefaut(e, c3, "x", "3");
				NkBpPoserDefaut(e, c4, "x", "4");
				const graph::NkNodeId add = NkBpCreerNoeud(e, "bp.math.add_r", 250.f, 200.f);
				const graph::NkNodeId ecr = NkBpCreerParCle(d, e, "bp.var.set:somme", 400.f, 0.f);
				e.Connect(c3, "r", add, "a");
				e.Connect(c4, "r", add, "b");
				e.Connect(debut, "suite", ecr, "exec");
				e.Connect(add, "r", ecr, "valeur");
				Monde w;
				const NkString err = Charger(w, d, "Carre.nkbp");
				const ecs::NkEntityId a = w.Avec("A", "Carre.nkbp");
				w.Jouer(2);
				const NkVarScript *s = w.Var(a, "somme");
				Temoin(err.Empty() && s != nullptr && s->valeur.x == 25.f, "(u4) fonction PURE Carre(3) + Carre(4) = 25",
					   s != nullptr ? s->valeur.x : -1.f);
			}
			// ── (u5) recursion sans fin : faute « pile d'appels », la scene continue ──
			{
				NkDocumentBp d;
				const uint32 gf = NkBpAjouterFonction(d, "Encore");
				graph::NkNodeGraph &f = d.graphes[gf].graphe;
				const graph::NkNodeId entree = Trouver(f, NK_BP_FN_ENTREE);
				const graph::NkNodeId retour = Trouver(f, NK_BP_FN_RETOUR);
				f.RemoveNode(retour);
				const graph::NkNodeId soi = NkBpCreerParCle(d, f, "bp.appel:Encore", 260.f, 0.f);
				f.Connect(entree, "suite", soi, "exec");
				graph::NkNodeGraph &e = d.graphes[0].graphe;
				const graph::NkNodeId debut = NkBpCreerNoeud(e, "bp.ev.debut", 0.f, 0.f);
				const graph::NkNodeId appel = NkBpCreerParCle(d, e, "bp.appel:Encore", 260.f, 0.f);
				e.Connect(debut, "suite", appel, "exec");
				Monde w;
				const NkString err = Charger(w, d, "Encore.nkbp");
				const ecs::NkEntityId a = w.Avec("A", "Encore.nkbp");
				w.Jouer(3);
				const bool designe = JournalContient(*w.h, "pile d'appels") &&
									 JournalContient(*w.h, NkString::Format("noeud %u", static_cast<unsigned>(NkBpCodeNoeud(gf, soi))).CStr());
				Temoin(err.Empty() && w.h->EnFaute(a, 0) && w.h->NbFautes() == 1u && designe,
					   "(u5) recursion sans fin : faute « pile d'appels » sur le noeud d'appel, une seule", static_cast<float32>(w.h->NbFautes()));
			}
			// ── (u6) enregistrer / rouvrir ; l'ancien .nkbp ──
			{
				const NkString racine = Temporaire();
				NkDirectory::Delete(racine.CStr(), true);
				NkDirectory::CreateRecursive(racine.CStr());
				NkDocumentBp d;
				NkBpDocumentPorte(d, "Porte bleue");
				NkErreurBp e;
				const NkString chemin = racine + "PorteUE5.nkbp";
				const bool ecrit = NkBpEnregistrerDocument(chemin.CStr(), d, e);
				NkDocumentBp relu;
				NkString err;
				const bool lu = NkBpOuvrirDocument(chemin.CStr(), relu, &err);
				NkString a, b;
				NkBpInstantane(d, a);
				NkBpInstantane(relu, b);
				std::printf("    (u6) ecrit=%d (%s) lu=%d (%s) %u/%u octets\n", ecrit ? 1 : 0, e.message.CStr(), lu ? 1 : 0, err.CStr(),
							static_cast<unsigned>(a.Length()), static_cast<unsigned>(b.Length()));
				// Le jeu (le moteur) lit le MEME fichier : le module, et il saute DOCU.
				unkeny::NkScripts2D r;
				NkString refus;
				r.ChargerBlueprint("PorteUE5.nkbp", chemin.CStr(), &refus);
				const unkeny::NkDefinitionScript *def = r.Definition(r.Trouver("PorteUE5.nkbp"));
				Temoin(ecrit && lu && a == b && relu.graphes.Size() == 2u && relu.variables.Size() == 3u && def != nullptr &&
						   def->erreur.Empty() && def->programme != nullptr,
					   "(u6) enregistrer / rouvrir : instantane identique ; le moteur lit le module", static_cast<float32>(b.Length()));
				// L'ANCIEN format : un graphe seul, noeuds de variable par nom.
				graph::NkNodeGraph g;
				NkBpGraphePorte(g, "Porte bleue");
				const NkString ancien = racine + "PorteAncienne.nkbp";
				NkErreurBp e2;
				NkBpEnregistrer(ancien.CStr(), g, e2);
				NkDocumentBp vieux;
				const bool vu = NkBpOuvrirDocument(ancien.CStr(), vieux, &err);
				unkeny::NkModuleBp m;
				NkErreurBp e3;
				const bool compile = NkBpCompilerDocument(vieux, m, e3);
				Temoin(vu && vieux.graphes.Size() == 1u && vieux.variables.Empty() && compile && m.variables.Size() == 1u,
					   "(u6) l'ANCIEN .nkbp de la porte s'ouvre (un graphe) et compile tel quel", static_cast<float32>(m.variables.Size()));
				NkDirectory::Delete(racine.CStr(), true);
			}
			// ── (u7) renommer, changer de type ──
			{
				NkDocumentBp d;
				DocumentDoubler(d, 2, true);
				const bool renomme = NkBpRenommerVariable(d, 0, "total");
				uint32 suivent = 0, anciens = 0;
				const graph::NkNodeGraph &e = d.graphes[0].graphe;
				for (uint32 i = 0; i < e.RawNodeCount(); ++i) {
					const graph::NkNode *n = e.RawNodeAt(i);
					if (n != nullptr && n->alive && (n->type == NK_BP_VAR_LIRE || n->type == NK_BP_VAR_ECRIRE)) {
						const NkString v = NkBpPropTexte(e, *n, "var");
						suivent += v == "total" ? 1u : 0u;
						anciens += v == "compteur" ? 1u : 0u;
					}
				}
				unkeny::NkModuleBp m;
				NkErreurBp err;
				const bool ok = NkBpCompilerDocument(d, m, err);
				// Le type change : entier -> texte. Le fil « valeur -> x (entier) » tombe.
				d.variables[0].type = "texte";
				d.variables[0].defaut = "";
				const uint32 refaits = NkBpRafraichirNoeuds(d);
				const graph::NkNode *lire = nullptr;
				for (uint32 i = 0; i < e.RawNodeCount(); ++i) {
					const graph::NkNode *n = e.RawNodeAt(i);
					if (n != nullptr && n->alive && n->type == NK_BP_VAR_LIRE) {
						lire = n;
					}
				}
				bool filTombe = lire != nullptr;
				for (uint32 i = 0; lire != nullptr && i < e.LinkCount(); ++i) {
					const graph::NkLink *l = e.LinkAt(i);
					filTombe = filTombe && !(l != nullptr && l->alive && l->fromNode == lire->id);
				}
				const NkString *t = lire != nullptr ? e.TypeName(lire->sockets[0].type) : nullptr;
				Temoin(renomme && suivent == 2u && anciens == 0u && ok && refaits >= 2u && filTombe && t != nullptr && *t == "texte",
					   "(u7) renommer : les 2 noeuds suivent ; type -> texte : noeuds refaits, fil incompatible tombe", static_cast<float32>(refaits));
			}
			// ── (u8) macro x2, boucle Pour, texte / couleur / entite, repartiteur ──
			{
				NkDocumentBp d;
				NkBpAjouterVariable(d, "total", "entier");
				const uint32 vt = NkBpAjouterVariable(d, "nom", "texte");
				d.variables[vt].defaut = "Bob";
				const uint32 vc = NkBpAjouterVariable(d, "teinte", "couleur");
				d.variables[vc].defaut = "#11223344";
				NkBpAjouterVariable(d, "cible", "entite");
				NkBpAjouterVariable(d, "recu", "reel");
				// La macro « Ajouter3 » : exec -> total = total + 3 -> suite.
				const uint32 gm = NkBpAjouterMacro(d, "Ajouter3");
				{
					graph::NkNodeGraph &m = d.graphes[gm].graphe;
					const graph::NkNodeId me = Trouver(m, NK_BP_MACRO_ENTREE);
					const graph::NkNodeId ms = Trouver(m, NK_BP_MACRO_SORTIE);
					Couper(m, me, "exec");
					const graph::NkNodeId lt = NkBpCreerParCle(d, m, "bp.var.get:total", 0.f, 120.f);
					const graph::NkNodeId add = NkBpCreerNoeud(m, "bp.math.add_i", 200.f, 120.f);
					NkBpPoserDefaut(m, add, "b", "3");
					const graph::NkNodeId et = NkBpCreerParCle(d, m, "bp.var.set:total", 300.f, 0.f);
					m.Connect(lt, "valeur", add, "a");
					m.Connect(add, "r", et, "valeur");
					m.Connect(me, "exec", et, "exec");
					m.Connect(et, "suite", ms, "suite");
				}
				// Le repartiteur « Touche(force: reel) », appele sur « B ».
				const uint32 rr = NkBpAjouterRepartiteur(d, "Touche");
				NkParamBp pf;
				pf.nom = "force";
				pf.type = "reel";
				d.repartiteurs[rr].params.PushBack(pf);
				graph::NkNodeGraph &e = d.graphes[0].graphe;
				const graph::NkNodeId debut = NkBpCreerNoeud(e, "bp.ev.debut", 0.f, 0.f);
				const graph::NkNodeId m1 = NkBpCreerParCle(d, e, "bp.macro:Ajouter3", 200.f, 0.f);
				const graph::NkNodeId m2 = NkBpCreerParCle(d, e, "bp.macro:Ajouter3", 400.f, 0.f);
				const graph::NkNodeId pour = NkBpCreerNoeud(e, "bp.pour", 600.f, 0.f);
				NkBpPoserDefaut(e, pour, "premier", "1");
				NkBpPoserDefaut(e, pour, "dernier", "10");
				const graph::NkNodeId lt = NkBpCreerParCle(d, e, "bp.var.get:total", 600.f, 200.f);
				const graph::NkNodeId add = NkBpCreerNoeud(e, "bp.math.add_i", 800.f, 200.f);
				const graph::NkNodeId et = NkBpCreerParCle(d, e, "bp.var.set:total", 900.f, 0.f);
				e.Connect(debut, "suite", m1, "exec");
				e.Connect(m1, "suite", m2, "exec");
				e.Connect(m2, "suite", pour, "exec");
				e.Connect(pour, "corps", et, "exec");
				e.Connect(lt, "valeur", add, "a");
				e.Connect(pour, "indice", add, "b");
				e.Connect(add, "r", et, "valeur");
				// Fini : nom = nom + "!" ; teinte posee sur soi ; cible = B ; Appeler Touche(2.5) sur cible.
				const graph::NkNodeId ln = NkBpCreerParCle(d, e, "bp.var.get:nom", 900.f, 300.f);
				const graph::NkNodeId cat = NkBpCreerNoeud(e, "bp.natif:unkeny.texte.concatener", 1100.f, 300.f);
				NkBpPoserDefaut(e, cat, "b", "!");
				const graph::NkNodeId en = NkBpCreerParCle(d, e, "bp.var.set:nom", 1300.f, 0.f);
				const graph::NkNodeId pn = NkBpCreerNoeud(e, "bp.natif:unkeny.entite.par_nom", 1300.f, 300.f);
				NkBpPoserDefaut(e, pn, "nom", "B");
				const graph::NkNodeId ec = NkBpCreerParCle(d, e, "bp.var.set:cible", 1500.f, 0.f);
				const graph::NkNodeId app = NkBpCreerParCle(d, e, "bp.rep.appeler:Touche", 1700.f, 0.f);
				NkBpPoserDefaut(e, app, "force", "2.5");
				e.Connect(pour, "fini", en, "exec");
				e.Connect(ln, "valeur", cat, "a");
				e.Connect(cat, "texte", en, "valeur");
				e.Connect(en, "suite", ec, "exec");
				e.Connect(pn, "entité", ec, "valeur");
				e.Connect(ec, "suite", app, "exec");
				e.Connect(ec, "valeur", app, "cible");
				// Chez B (le meme Blueprint) : Evenement Touche -> recu = force.
				const graph::NkNodeId evt = NkBpCreerParCle(d, e, "bp.rep.evenement:Touche", 0.f, 500.f);
				const graph::NkNodeId er = NkBpCreerParCle(d, e, "bp.var.set:recu", 300.f, 500.f);
				e.Connect(evt, "suite", er, "exec");
				e.Connect(evt, "force", er, "valeur");
				Monde w;
				const NkString err = Charger(w, d, "Tout.nkbp");
				const ecs::NkEntityId a = w.Avec("A", "Tout.nkbp");
				const ecs::NkEntityId b = w.Avec("B", "Tout.nkbp");
				w.Jouer(3);
				const NkVarScript *total = w.Var(a, "total");
				const NkVarScript *nom = w.Var(a, "nom");
				const NkVarScript *teinte = w.Var(a, "teinte");
				const NkVarScript *cible = w.Var(a, "cible");
				const NkVarScript *recuB = w.Var(b, "recu");
				std::printf("    (u8) err=« %s » total=%g nom=%s teinte=%08X cible=%s recu(B)=%g\n", err.CStr(),
							total != nullptr ? static_cast<double>(total->valeur.x) : -1.0, nom != nullptr ? nom->texte : "?",
							teinte != nullptr ? static_cast<unsigned>(NkCouleurDeVar(teinte->valeur)) : 0u, cible != nullptr ? cible->texte : "?",
							recuB != nullptr ? static_cast<double>(recuB->valeur.x) : -1.0);
				// A : total = 3 + 3 + (1+...+10) = 61 ; nom « Bob! » ; cible « B ».
				Temoin(err.Empty() && total != nullptr && total->valeur.x == 61.f, "(u8) macro inseree DEUX fois (+3 +3) puis Pour 1..10 : 61",
					   total != nullptr ? total->valeur.x : -1.f);
				Temoin(nom != nullptr && std::strcmp(nom->texte, "Bob!") == 0 && teinte != nullptr && NkCouleurDeVar(teinte->valeur) == 0x11223344u &&
						   cible != nullptr && std::strcmp(cible->texte, "B") == 0,
					   "(u8) variables texte (« Bob! »), couleur (#11223344), entite (« B », par son nom)", 0.f);
				Temoin(recuB != nullptr && recuB->valeur.x == 2.5f, "(u8) repartiteur Touche(2.5) appele sur B : B le recoit",
					   recuB != nullptr ? recuB->valeur.x : -1.f);
			}
			// ── (u9) la porte REECRITE : un Blueprint, deux zones, deux portes ──
			{
				NkDocumentBp d;
				NkBpDocumentPorte(d, "Porte");
				Monde w(true);
				const NkString err = Charger(w, d, "PorteUE5.nkbp");
				// Deux portes, deux zones ; la zone de droite designe « Porte droite »
				// et monte de 3 m (par instance).
				const ecs::NkEntityId pg = w.s->Creer("Porte gauche", NkVec2f(-4.f, 1.f));
				const ecs::NkEntityId pd = w.s->Creer("Porte droite", NkVec2f(4.f, 1.f));
				const ecs::NkEntityId zg = w.Avec("Zone gauche", "PorteUE5.nkbp", NkVec2f(-2.f, 0.f));
				const ecs::NkEntityId zd = w.Avec("Zone droite", "PorteUE5.nkbp", NkVec2f(2.f, 0.f));
				NkScriptPoserTexte(*w.s->Monde().Get<NkScript2D>(zg), 0, "porte", NkTypeVarScript::NK_TEXTE, "Porte gauche");
				NkScriptPoserTexte(*w.s->Monde().Get<NkScript2D>(zd), 0, "porte", NkTypeVarScript::NK_TEXTE, "Porte droite");
				NkScriptPoserVariable(*w.s->Monde().Get<NkScript2D>(zd), 0, "hauteur", NkTypeVarScript::NK_REEL, NkVec2f(3.f, 0.f));
				w.Jouer(1);
				// Le Joueur « entre » dans chaque zone : l'evenement, livre comme le
				// ferait la physique (le banc des scripts prouve deja le contact reel).
				const ecs::NkEntityId joueur = w.s->Creer("Joueur", NkVec2f(0.f, 0.f));
				NkUnkEvenementV1 ev;
				std::memset(&ev, 0, sizeof(ev));
				ev.genre = NK_UNK_EV_ZONE_ENTREE;
				ev.autre.pack = joueur.Pack();
				ev.soiEstLaZone = 1;
				ev.nomAction = "";
				const float32 yg0 = w.s->Monde().Get<NkTransform2D>(pg)->position.y;
				const float32 yd0 = w.s->Monde().Get<NkTransform2D>(pd)->position.y;
				// Livrer par l'hote : la voie du jeu.
				w.h->LivrerEvenement(zg, ev);
				w.h->LivrerEvenement(zd, ev);
				w.h->LivrerEvenement(zd, ev); // une seconde fois : « ouverte » l'arrete
				const float32 dg = w.s->Monde().Get<NkTransform2D>(pg)->position.y - yg0;
				const float32 dd = w.s->Monde().Get<NkTransform2D>(pd)->position.y - yd0;
				std::printf("    (u9) err=« %s » gauche +%.2f, droite +%.2f\n", err.CStr(), static_cast<double>(dg), static_cast<double>(dd));
				Temoin(err.Empty() && Absf(dg - 2.f) < 1e-4f && Absf(dd - 3.f) < 1e-4f && JournalContient(*w.h, "La porte s'ouvre"),
					   "(u9) la porte REECRITE : un Blueprint, chaque zone ouvre SA porte (2 m, 3 m par instance)", dd);
			}
			// ── (u10) les noeuds de CODE : Code, Si (expression), Expression ──
			{
				auto document = [](NkDocumentBp &d, const char *condition, const char *expr) {
					d.Vider();
					NkBpAjouterVariable(d, "total", "entier");
					NkBpAjouterVariable(d, "score", "reel");
					NkBpAjouterVariable(d, "message", "texte");
					NkBpAjouterVariable(d, "double", "reel");
					graph::NkNodeGraph &e = d.graphes[0].graphe;
					const graph::NkNodeId debut = NkBpCreerNoeud(e, "bp.ev.debut", 0.f, 0.f);
					const graph::NkNodeId ca = NkBpCreerParCle(d, e, NK_BP_CODE, 240.f, 0.f);
					NkBpPoserCodeNoeud(e, ca,
									   "total = total + 3\nscore = $round((7 - 2) / 7 * 100, 1)\nmessage = \"score : \" + texte(score)\nafficher(message)");
					graph::NkNodeId si = NkBpCreerParCle(d, e, NK_BP_SI_EXPRESSION, 520.f, 0.f);
					NkBpCodeRetirerPrise(e, si, "a", graph::NkSocketDir::Input); // la condition lit les variables
					NkBpPoserCodeNoeud(e, si, condition);
					const graph::NkNodeId cg = NkBpCreerParCle(d, e, NK_BP_CODE, 780.f, -60.f);
					NkBpPoserCodeNoeud(e, cg, "afficher(\"grand\")");
					const graph::NkNodeId cp = NkBpCreerParCle(d, e, NK_BP_CODE, 780.f, 80.f);
					NkBpPoserCodeNoeud(e, cp, "afficher(\"petit\")");
					const graph::NkNodeId ls = NkBpCreerParCle(d, e, "bp.var.get:score", 780.f, 220.f);
					const graph::NkNodeId ex = NkBpCreerParCle(d, e, NK_BP_EXPRESSION, 980.f, 220.f);
					NkBpPoserCodeNoeud(e, ex, expr);
					NkBpPoserDefaut(e, ex, "b", "2");
					const graph::NkNodeId ed = NkBpCreerParCle(d, e, "bp.var.set:double", 1220.f, -60.f);
					e.Connect(debut, "suite", ca, "exec");
					e.Connect(ca, "suite", si, "exec");
					e.Connect(si, "vrai", cg, "exec");
					e.Connect(si, "faux", cp, "exec");
					e.Connect(cg, "suite", ed, "exec");
					e.Connect(ls, "valeur", ex, "a");
					e.Connect(ex, "résultat", ed, "valeur");
					return ex;
				};
				NkDocumentBp d;
				document(d, "total > 2 && score > 70", "a * b");
				Monde w;
				const NkString err = Charger(w, d, "Code.nkbp");
				const ecs::NkEntityId a = w.Avec("A", "Code.nkbp");
				w.Jouer(2);
				const NkVarScript *total = w.Var(a, "total");
				const NkVarScript *score = w.Var(a, "score");
				const NkVarScript *dbl = w.Var(a, "double");
				const NkVarScript *msg = w.Var(a, "message");
				std::printf("    (u10) err=« %s » total=%g score=%g message=%s double=%g\n", err.CStr(),
							total != nullptr ? static_cast<double>(total->valeur.x) : -1.0, score != nullptr ? static_cast<double>(score->valeur.x) : -1.0,
							msg != nullptr ? msg->texte : "?", dbl != nullptr ? static_cast<double>(dbl->valeur.x) : -1.0);
				Temoin(err.Empty() && total != nullptr && total->valeur.x == 3.f && score != nullptr && Absf(score->valeur.x - 71.4f) < 1e-4f &&
						   msg != nullptr && std::strcmp(msg->texte, "score : 71.4") == 0 && JournalContient(*w.h, "[A] score : 71.4"),
					   "(u10) noeud Code : total += 3, score = $round((7-2)/7*100, 1) = 71.4, texte, afficher", score != nullptr ? score->valeur.x : -1.f);
				Temoin(JournalContient(*w.h, "[A] grand") && !JournalContient(*w.h, "[A] petit") && dbl != nullptr && Absf(dbl->valeur.x - 142.8f) < 1e-3f,
					   "(u10) Si (expression) « total > 2 && score > 70 » -> Vrai ; Expression a * b = 142.8", dbl != nullptr ? dbl->valeur.x : -1.f);
				// Contre-epreuve : la condition mutee « total > 5 » -> Faux.
				NkDocumentBp d2;
				document(d2, "total > 5", "a * b");
				Monde w2;
				Charger(w2, d2, "Code2.nkbp");
				w2.Avec("A", "Code2.nkbp");
				w2.Jouer(2);
				Temoin(JournalContient(*w2.h, "[A] petit") && !JournalContient(*w2.h, "[A] grand"),
					   "(u10n) la condition mutee « total > 5 » : Faux (le banc VOIT la difference)", 0.f);
				// Une erreur ECRITE : la ligne, la colonne, le noeud.
				NkDocumentBp d3;
				const graph::NkNodeId fautif = document(d3, "total > 2", "a +");
				unkeny::NkModuleBp m;
				NkErreurBp e3;
				const bool ok3 = NkBpCompilerDocument(d3, m, e3);
				NkDocumentBp d4;
				const graph::NkNodeId fautif4 = document(d4, "total > 2", "a * zz");
				NkErreurBp e4;
				const bool ok4 = NkBpCompilerDocument(d4, m, e4);
				std::printf("    (u10e) %s | %s\n", e3.message.CStr(), e4.message.CStr());
				Temoin(!ok3 && e3.noeud == fautif && std::strstr(e3.message.CStr(), "ligne 1, colonne 4") != nullptr && !ok4 && e4.noeud == fautif4 &&
						   std::strstr(e4.message.CStr(), "« zz » est inconnu") != nullptr,
					   "(u10e) code faux : erreur sur SON noeud, ligne et colonne, nom inconnu nomme", static_cast<float32>(e3.noeud));
			}
			return gE;
		}

		// =====================================================================
		// LES FILS LISSES (retour de Rihen sur la capture 04, 2026-10-02 :
		// « pourquoi les connecteurs ne sont pas lisses ? »). La CAUSE : NKGui
		// tracait lignes et courbes en quads NETS (aucune frange alpha) ; seul le
		// MSAA 4x de DX11 les adoucissait a l'ecran -- le rasteriseur des
		// captures, OpenGL, Vulkan et le logiciel montraient l'escalier. Le
		// remede est dans NKGui (NkGuiDrawList::AddPolylineLisse, AddBezierCubic) ;
		// ce banc le MESURE sur le rasteriseur des captures, avec sa contre-epreuve.
		// =====================================================================
		namespace {
			struct MesureFil {
					uint32 intermediaires = 0; ///< pixels ni noirs ni blancs : le bord adouci
					uint32 pleins = 0;
					uint32 colonnes = 0, colonnesLisses = 0; ///< colonnes touchees / avec un pixel adouci
					uint32 auDela = 0;						 ///< pixels a droite du bout (le bout rond)
					float32 poids = 0.f;					 ///< la somme d'une colonne ou le fil est horizontal
			};
			/// Rasterise UN fil oblique (blanc sur noir, 240 x 120) par le rasteriseur
			/// des CAPTURES et compte ses pixels. `cubique` : AddBezierCubic (le fil
			/// de la toile) ; sinon AddPolyline sur 25 points (l'ancien fil).
			MesureFil MesurerFil(bool lisse, float32 epaisseur, bool cubique, bool boutsRonds = true) {
				memory::NkAllocator &tas = memory::NkGetDefaultAllocator();
				nkgui::NkGuiDrawList *dl = tas.New<nkgui::NkGuiDrawList>();
				nkgui::NkGuiDrawListRaster *ras = tas.New<nkgui::NkGuiDrawListRaster>();
				dl->traitsLisses = lisse;
				const nkgui::NkVec2 a{12.f, 20.f}, b{200.f, 100.f};
				const nkgui::NkVec2 c1{a.x + 94.f, a.y}, c2{b.x - 94.f, b.y};
				const nkgui::NkColor blanc{255, 255, 255, 255};
				if (cubique) {
					dl->AddBezierCubic(a, c1, c2, b, blanc, epaisseur, 0, boutsRonds);
				} else {
					nkgui::NkVec2 pts[25];
					for (int32 i = 0; i < 25; ++i) {
						pts[i] = nkgui::NkGuiDrawList::PointBezier(a, c1, c2, b, static_cast<float32>(i) / 24.f);
					}
					dl->AddPolyline(pts, 25, blanc, epaisseur);
				}
				ras->Init(240, 120);
				ras->Effacer(0x000000FFu);
				ras->Rasteriser(*dl);
				MesureFil m;
				for (int32 x = 0; x < 240; ++x) {
					bool touche = false, adouci = false;
					float32 somme = 0.f;
					for (int32 y = 0; y < 120; ++y) {
						const uint32 r = (ras->Pixel(x, y) >> 24) & 0xFFu;
						if (r > 0u) {
							touche = true;
							somme += static_cast<float32>(r) / 255.f;
							if (x > static_cast<int32>(b.x)) {
								++m.auDela;
							}
						}
						if (r > 0u && r < 255u) {
							adouci = true;
							++m.intermediaires;
						} else if (r == 255u) {
							++m.pleins;
						}
					}
					m.colonnes += touche ? 1u : 0u;
					m.colonnesLisses += adouci ? 1u : 0u;
					if (x == 22) {
						m.poids = somme; // pres du depart : le fil y est horizontal
					}
				}
				tas.Delete(ras);
				tas.Delete(dl);
				return m;
			}
		} // namespace

		void NkEditeurLancerBancFilsLisses() {
			std::printf("\n  -- Les FILS lisses (NKGui : frange alpha, cubique adaptative, bouts ronds) --\n");
			const MesureFil v = MesurerFil(true, 2.f, true);
			std::printf("    (u11) valeur 2 px : %u px adoucis, %u pleins, %u/%u colonnes adoucies, poids %.2f\n", v.intermediaires, v.pleins,
						v.colonnesLisses, v.colonnes, static_cast<double>(v.poids));
			Temoin(v.intermediaires > 0u && v.pleins > 0u && v.colonnesLisses * 100u >= v.colonnes * 95u && v.poids > 1.7f && v.poids < 2.3f,
				   "(u11) fil de VALEUR 2 px : bord adouci sur chaque colonne, poids 2 px", static_cast<float32>(v.intermediaires));
			const MesureFil x = MesurerFil(true, 3.5f, true);
			Temoin(x.intermediaires > 0u && x.colonnesLisses * 100u >= x.colonnes * 95u && x.poids > 3.2f && x.poids < 3.8f,
				   "(u11) fil d'EXECUTION 3,5 px : bord adouci, poids 3,5 px", x.poids);
			// CONTRE-EPREUVE : les MEMES fils sans frange (le chemin d'avant) -> la
			// sonde ne trouve AUCUN pixel adouci : elle voit bien l'escalier.
			const MesureFil n1 = MesurerFil(false, 2.f, true);
			const MesureFil n2 = MesurerFil(false, 2.f, false);
			std::printf("    (u11n) sans frange : %u et %u px adoucis (cubique, ancienne polyligne)\n", n1.intermediaires, n2.intermediaires);
			Temoin(n1.intermediaires == 0u && n2.intermediaires == 0u && n1.pleins > 0u,
				   "(u11n) CONTRE-EPREUVE : sans frange, 0 px adouci -- la sonde voit l'escalier", static_cast<float32>(n1.intermediaires));
			// La POINTE d'une prise d'execution (pentagone, AddConvexPolyLisse).
			{
				uint32 adoucis[2] = {0u, 0u};
				for (int32 lisse = 0; lisse < 2; ++lisse) {
					memory::NkAllocator &tas = memory::NkGetDefaultAllocator();
					nkgui::NkGuiDrawList *dl = tas.New<nkgui::NkGuiDrawList>();
					nkgui::NkGuiDrawListRaster *ras = tas.New<nkgui::NkGuiDrawListRaster>();
					dl->traitsLisses = lisse == 1;
					const float32 x0 = 10.3f, cy = 20.4f, w = 22.f, h = 26.f, xp = x0 + w * 0.58f;
					const nkgui::NkVec2 pe[5] = {{x0, cy - h * 0.5f}, {xp, cy - h * 0.5f}, {x0 + w, cy}, {xp, cy + h * 0.5f}, {x0, cy + h * 0.5f}};
					dl->AddConvexPolyLisse(pe, 5, nkgui::NkColor{255, 255, 255, 255});
					ras->Init(48, 40);
					ras->Effacer(0x000000FFu);
					ras->Rasteriser(*dl);
					for (int32 y = 0; y < 40; ++y) {
						for (int32 x = 0; x < 48; ++x) {
							const uint32 r = (ras->Pixel(x, y) >> 24) & 0xFFu;
							adoucis[lisse] += r > 0u && r < 255u ? 1u : 0u;
						}
					}
					tas.Delete(ras);
					tas.Delete(dl);
				}
				Temoin(adoucis[1] > 0u && adoucis[0] == 0u, "(u11p) pointe d'une prise d'execution : bord adouci (sans frange : 0 px)",
					   static_cast<float32>(adoucis[1]));
			}
			// Les BOUTS RONDS : le fil deborde son extremite d'un demi-disque.
			const MesureFil carre = MesurerFil(true, 3.5f, true, false);
			Temoin(x.auDela > 0u && carre.auDela == 0u, "(u11b) bouts RONDS : le demi-disque passe l'extremite (coupe nette sans eux)",
				   static_cast<float32>(x.auDela));
			// Les SEGMENTS suivent la longueur a l'ecran ; aucun angle visible.
			const nkgui::NkVec2 a{0.f, 0.f}, b{60.f, 20.f}, A{0.f, 0.f}, B{1200.f, 400.f};
			const int32 court = nkgui::NkGuiDrawList::SegmentsBezier(a, nkgui::NkVec2{30.f, 0.f}, nkgui::NkVec2{30.f, 20.f}, b);
			const nkgui::NkVec2 C1{600.f, 0.f}, C2{600.f, 400.f};
			const int32 long_ = nkgui::NkGuiDrawList::SegmentsBezier(A, C1, C2, B);
			float32 pire = 1.f; // le plus petit cosinus entre deux segments voisins
			nkgui::NkVec2 prec = A, dprec{0.f, 0.f};
			for (int32 i = 1; i <= long_; ++i) {
				const nkgui::NkVec2 q = nkgui::NkGuiDrawList::PointBezier(A, C1, C2, B, static_cast<float32>(i) / static_cast<float32>(long_));
				const nkgui::NkVec2 d{q.x - prec.x, q.y - prec.y};
				if (i > 1) {
					const float32 l = (d.x * d.x + d.y * d.y) * (dprec.x * dprec.x + dprec.y * dprec.y);
					if (l > 0.f) {
						float32 c = (d.x * dprec.x + d.y * dprec.y);
						c = c * c / l * (c < 0.f ? -1.f : 1.f);
						pire = c < pire ? c : pire;
					}
				}
				dprec = d;
				prec = q;
			}
			std::printf("    (u11s) segments : fil de 63 px -> %d, fil de 1265 px -> %d ; pire cos^2 entre voisins %.5f\n", court, long_,
						static_cast<double>(pire));
			// cos^2(4 deg) = 0.99513 : aucun coude de plus de 4 degres.
			Temoin(court >= 8 && long_ >= court * 10 && pire > 0.99513f, "(u11s) segments ADAPTATIFS a la longueur : aucun coude > 4 degres",
				   static_cast<float32>(long_));
		}

		int32 NkEditeurLancerBancBlueprint() {
			gR = gE = 0;
			std::printf("\nUnkenyEditor — banc de l'EDITEUR DE BLUEPRINT (a la UE5)\n\n");
			NkEditeurLancerBancBlueprintModele();
			NkEditeurLancerBancFilsLisses();
			std::printf("\n%s : %d reussis, %d echec\n", gE == 0 ? "BANC BLUEPRINT UE5 REUSSI" : "BANC BLUEPRINT UE5 EN ECHEC", gR, gE);
			return gE == 0 ? 0 : 1;
		}

	} // namespace editeur
} // namespace nkentseu
