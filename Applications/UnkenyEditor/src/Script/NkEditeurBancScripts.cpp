// =============================================================================
// NkEditeurBancScripts.cpp — le banc des SCRIPTS de l'editeur (2026-10-01)
//
// Document 01, § 9.5 (e13-e18) et § 9.3 (c4), sur un VRAI projet ecrit dans le
// dossier temporaire : l'exemple « Portes » (Script/NkEditeurExemplePortes.h).
// Sans fenetre ni GPU ; la page du graphe est peinte hors ecran par la trame
// de banc (NkEditeurBancTrame.h). Le C++ est compile par le VRAI clang++ : sans
// lui, les temoins C++ rendent INDETERMINE, jamais vert.
//
// PRE-ENREGISTREMENT :
//   (e13) l'exemple s'ouvre ; Jouer, le Joueur marche a GAUCHE : la porte
//         bleue (Blueprint) monte de 2 m ; a DROITE : la porte rouge (C++)
//         monte de 2 m ; Arreter : les portes et la variable « ouverte »
//         reviennent
//   (e14) graphe construit par code « Debut -> Afficher "Bonjour..." »,
//         compile : le journal le porte au premier pas
//   (e15) graphe invalide : une force sous Tick -> erreur NOMMEE sur son
//         noeud ; un nom de variable vide -> erreur sur son noeud
//   (e16) le Blueprint de la porte RECOMPILE pendant JEU : la variable
//         « ouverte » est gardee par nom (la porte ne remonte pas)
//   (e17) Enregistrer / Ouvrir : le script, son ordre et la valeur editee de
//         sa variable reviennent
//   (e18) la page du graphe hors ecran : un clic sur la palette pose un noeud ;
//         tirer un fil de « suite » vers « exec » le relie ; vers une entree de
//         DONNEE, le refus se NOMME (famille)
//   (c4)  C++ A CHAUD : v2 du source enregistree pendant JEU -> recompile,
//         recharge, « Recharge » voit l'etat GARDE (ouverte = vrai) ; (c4n) v3
//         qui ne compile pas : l'erreur au journal avec fichier:ligne, v2 reste
//   (c5)  « + Ajouter > Script C++ » cree le fichier (modele commente) et il
//         COMPILE ; « + Ajouter > Blueprint » cree un .nkbp qui s'ouvre dans la
//         page du graphe
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurBancTrame.h"
#include "Editeur/NkEditeurContenu.h"
#include "Script/NkBpCatalogue.h"
#include "Script/NkEditeurExemplePortes.h"
#include "Script/NkEditeurGraphe.h"
#include "Script/NkEditeurScripts.h"
#include "Script/NkEditeurScriptsUi.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "Unkeny/Entree/NkUnkenyActionsStandard.h"
#include "Unkeny/Jeu/NkUnkenyControleurs.h"

#include <cstdio>
#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		namespace {
			int32 gR = 0, gE = 0, gI = 0;
			void Temoin(bool ok, const char *quoi, float32 v) {
				std::printf("  [%s] %-70s %10.4f\n", ok ? " OK " : "ECHEC", quoi, static_cast<double>(v));
				(ok ? gR : gE)++;
			}
			void Indetermine(const char *quoi, const char *pourquoi) {
				std::printf("  [ ?? ] %-70s INDETERMINE (%s)\n", quoi, pourquoi);
				++gI;
			}
			float32 Absf(float32 v) {
				return v < 0.f ? -v : v;
			}
			ecs::NkEntityId ParNom(NkScene &s, const char *nom) {
				ecs::NkEntityId t = ecs::NkEntityId::Invalid();
				s.Monde().Query<NkEtiquette>().ForEach([&](ecs::NkEntityId id, NkEtiquette &e) {
					if (!t.IsValid() && std::strcmp(e.nom, nom) == 0) {
						t = id;
					}
				});
				return t;
			}
			float32 Y(NkScene &s, const char *nom) {
				const ecs::NkEntityId id = ParNom(s, nom);
				const NkTransform2D *t = id.IsValid() ? s.Monde().Get<NkTransform2D>(id) : nullptr;
				return t != nullptr ? t->position.y : -999.f;
			}
			float32 X(NkScene &s, const char *nom) {
				const ecs::NkEntityId id = ParNom(s, nom);
				const NkTransform2D *t = id.IsValid() ? s.Monde().Get<NkTransform2D>(id) : nullptr;
				return t != nullptr ? t->position.x : -999.f;
			}
			bool Journal(const NkEditeurScripts &s, const NkVector<NkString> &copie, const char *motif) {
				(void)s;
				for (uint32 i = 0; i < copie.Size(); ++i) {
					if (std::strstr(copie[i].CStr(), motif) != nullptr) {
						return true;
					}
				}
				return false;
			}
			/// Une trame de jeu : l'axe « Avancer » pose, le service des scripts.
			void Trame(NkEditeurModele &m, NkEditeurScripts &s, unkeny::NkActions &a, float32 axe, NkVector<NkString> &journal) {
				a.NouvelleTrame();
				a.Poser(unkeny::NK_ACTION_AVANCER, axe);
				NkEditeurAvancer(m, 1.f / 60.f);
				NkEditeurScriptsTrame(s, m, nullptr, 1.f / 60.f);
				for (uint32 i = 0; i < s.journal.Size(); ++i) {
					journal.PushBack(s.journal[i]);
				}
				s.journal.Clear();
			}
			void Vider(NkEditeurScripts &s, NkVector<NkString> &journal) {
				for (uint32 i = 0; i < s.journal.Size(); ++i) {
					journal.PushBack(s.journal[i]);
				}
				s.journal.Clear();
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
				return d + "/unkeny_banc_scripts/";
			}
			/// Compile tout de suite et attend (le banc ne joue pas avec le temps).
			bool CompilerEtAttendre(NkEditeurScripts &s, NkEditeurModele &m, NkVector<NkString> &journal) {
				NkEditeurScriptsReleverCpp(s, m);
				if (!NkEditeurScriptsCompiler(s, m)) {
					Vider(s, journal);
					return false;
				}
				NkEditeurScriptsSuivreCompilation(s, m, true);
				Vider(s, journal);
				return s.etat == NkEtatCompilation::NK_REUSSIE;
			}
		} // namespace

		int32 NkEditeurLancerBancScripts() {
			gR = gE = gI = 0;
			std::printf("\nUnkenyEditor — banc des SCRIPTS (Blueprint, C++ a chaud, page du graphe)\n\n");
			memory::NkAllocator &al = memory::NkGetDefaultAllocator();
			const NkString racine = Temporaire();
			NkDirectory::Delete(racine.CStr(), true);
			const NkString projet = racine + "Portes/";
			NkString err;
			const NkString scene = NkEditeurEcrireExemplePortes(projet.CStr(), &err);
			Temoin(!scene.Empty(), "(e13) l'exemple Portes s'ecrit (scene, Blueprint, source C++)", 0.f);
			if (scene.Empty()) {
				std::printf("    %s\n", err.CStr());
				std::printf("\nBANC SCRIPTS EDITEUR EN ECHEC : %d reussis, %d echec\n", gR, gE);
				return 1;
			}
			NkEditeurModele *pm = al.New<NkEditeurModele>();
			NkEditeurModele &m = *pm;
			NkCreerRessourcesSim(m.ressources, &m.textures, nullptr);
			NkEditeurNouvelleScene(m);
			NkEditeurScripts *ps = al.New<NkEditeurScripts>();
			NkEditeurScripts &s = *ps;
			s.ouvrirTexteExterne = false; // aucune fenetre ne s'ouvre pendant un banc
			unkeny::NkActions *pa = al.New<unkeny::NkActions>();
			unkeny::NkActions &a = *pa;
			unkeny::NkLiaisons *pl = al.New<unkeny::NkLiaisons>();
			unkeny::NkLiaisonsStandard(*pl);
			m.chemin = scene;
			const bool ouverte = NkEditeurOuvrir(m);
			NkEditeurScriptsDemarrer(s, m, &a, pl);
			unkeny::NkAjouterControleurs2D(m.scene, &a);
			NkVector<NkString> journal;
			NkEditeurScriptsTrame(s, m, nullptr, 1.f); // le releve : le .nkbp, les .cpp
			Vider(s, journal);
			const bool bpConnu = s.registre.Trouver("Contenu/Scripts/PorteBlueprint.nkbp") != 0u;
			Temoin(ouverte && bpConnu, "(e13) la scene s'ouvre, le Blueprint du projet est au registre", 0.f);
			const NkString clang = NkEditeurScriptsCompilateur();
			bool cpp = false;
			if (clang.Empty()) {
				Indetermine("(c4) compilation du C++ du projet", "clang++ introuvable");
			} else {
				if (s.etat == NkEtatCompilation::NK_EN_COURS) {
					NkEditeurScriptsSuivreCompilation(s, m, true);
					Vider(s, journal);
				}
				cpp = s.registre.Trouver("cpp:PorteCpp") != 0u && s.etat == NkEtatCompilation::NK_REUSSIE;
				if (!cpp) {
					for (uint32 i = 0; i < s.sortie.Size(); ++i) {
						std::printf("    clang : %s\n", s.sortie[i].CStr());
					}
				}
				Temoin(cpp, "(c4) PorteCpp.cpp compile par clang++, DLL chargee, classe cpp:PorteCpp", static_cast<float32>(s.compilations));
			}

			// ── (e13) Jouer : a gauche la porte bleue, a droite la rouge ──
			const float32 yBleue0 = Y(m.scene, "Porte bleue"), yRouge0 = Y(m.scene, "Porte rouge");
			NkEditeurJouer(m);
			for (int32 k = 0; k < 150; ++k) {
				Trame(m, s, a, -1.f, journal);
			}
			const float32 dBleue = Y(m.scene, "Porte bleue") - yBleue0;
			const float32 xGauche = X(m.scene, "Joueur");
			for (int32 k = 0; k < 330; ++k) {
				Trame(m, s, a, 1.f, journal);
			}
			const float32 dRouge = Y(m.scene, "Porte rouge") - yRouge0;
			std::printf("    joueur a gauche x=%.2f puis x=%.2f ; porte bleue +%.2f, porte rouge +%.2f\n", static_cast<double>(xGauche),
						static_cast<double>(X(m.scene, "Joueur")), static_cast<double>(dBleue), static_cast<double>(dRouge));
			Temoin(Absf(dBleue - 2.f) < 0.01f && Journal(s, journal, "La porte s'ouvre (Blueprint)"),
				   "(e13) le Joueur entre dans la zone bleue : le BLUEPRINT ouvre la porte bleue", dBleue);
			if (cpp) {
				Temoin(Absf(dRouge - 2.f) < 0.01f && Journal(s, journal, "La porte rouge s'ouvre (C++)"),
					   "(e13) puis dans la zone rouge : le C++ ouvre la porte rouge", dRouge);
			}
			// La porte ouverte LAISSE PASSER (sa collision a suivi : un corps statique
			// teleporte est refait, NkScene::TeleporterEntite).
			Temoin(xGauche < -8.6f && X(m.scene, "Joueur") > 8.6f, "(e13) le Joueur PASSE sous les deux portes ouvertes", xGauche);
			const ecs::NkEntityId zb = ParNom(m.scene, "Zone bleue");
			const NkVarScript *vo = zb.IsValid() ? NkScriptVariable(*m.scene.Monde().Get<NkScript2D>(zb), 0, "ouverte") : nullptr;
			Temoin(vo != nullptr && vo->valeur.x == 1.f, "(e13) la variable « ouverte » du Blueprint vaut vrai", vo != nullptr ? vo->valeur.x : -1.f);

			// ── (c4) le C++ A CHAUD, pendant JEU ──
			if (cpp) {
				const NkString source = projet + "Contenu/Scripts/PorteCpp.cpp";
				NkString v2 = NkString(NkEditeurSourcePorteCpp());
				const char *ancre = "\t\tvoid Debut() override {";
				const char *p = std::strstr(v2.CStr(), ancre);
				NkString texte = NkString(v2.CStr(), static_cast<usize>(p - v2.CStr())) +
								 "\t\tvoid Recharge() override {\n\t\t\tAfficher(etat.ouverte ? \"v2 : etat garde (ouverte)\" : \"v2 : etat perdu\");\n\t\t}\n" +
								 NkString(p);
				NkFile::WriteAllText(source.CStr(), texte.CStr());
				const bool v2ok = CompilerEtAttendre(s, m, journal);
				Trame(m, s, a, 0.f, journal);
				Temoin(v2ok && Journal(s, journal, "v2 : etat garde (ouverte)") && Journal(s, journal, "rechargés à chaud"),
					   "(c4) v2 enregistree pendant Jouer : rechargee, l'etat prive est GARDE", static_cast<float32>(s.modules.Generation()));
				// v3 : une faute de frappe.
				NkString v3 = texte;
				const char *q = std::strstr(v3.CStr(), "etat.ouverte = true;");
				NkString casse = NkString(v3.CStr(), static_cast<usize>(q - v3.CStr())) + "etat.ouverte = vrai;" + NkString(q + 20);
				NkFile::WriteAllText(source.CStr(), casse.CStr());
				const uint32 gen = s.modules.Generation();
				const bool v3ok = CompilerEtAttendre(s, m, journal);
				bool ligne = false;
				for (uint32 i = 0; i < journal.Size(); ++i) {
					ligne = ligne || (std::strstr(journal[i].CStr(), "Contenu/Scripts/PorteCpp.cpp:") != nullptr &&
									  std::strstr(journal[i].CStr(), ": error:") != nullptr);
				}
				const ecs::NkEntityId zr = ParNom(m.scene, "Zone rouge");
				Temoin(!v3ok && ligne && s.modules.Generation() == gen && s.hote.InstanceCpp(zr, 0) != nullptr && !s.hote.EnFaute(zr, 0),
					   "(c4n) v3 casse : erreur fichier:ligne au journal, v2 reste ACTIVE", static_cast<float32>(s.erreurs.Size()));
				if (!s.erreurs.Empty()) {
					std::printf("    %s:%d: %s\n", s.erreurs[0].fichier.CStr(), s.erreurs[0].ligne, s.erreurs[0].message.CStr());
				}
				NkFile::WriteAllText(source.CStr(), NkEditeurSourcePorteCpp());
				CompilerEtAttendre(s, m, journal);
			}

			// ── (e16) le Blueprint recompile pendant JEU : variable gardee ──
			{
				NkEditeurOuvrirGraphe(s, m, (projet + "Contenu/Scripts/PorteBlueprint.nkbp").CStr());
				// La porte montera de 3 m desormais ; « ouverte » est vraie : rien ne bouge.
				graph::NkNodeGraph &g = s.graphe.graphe;
				for (uint32 i = 0; i < g.RawNodeCount(); ++i) {
					const graph::NkNode *n = g.RawNodeAt(i);
					if (n != nullptr && n->alive && n->type == "bp.math.add_v") {
						NkBpPoserDefaut(g, n->id, "b", "0 3");
					}
				}
				const bool ok = NkEditeurCompilerGraphe(s, m);
				for (int32 k = 0; k < 10; ++k) {
					Trame(m, s, a, 0.f, journal);
				}
				const NkVarScript *v = NkScriptVariable(*m.scene.Monde().Get<NkScript2D>(ParNom(m.scene, "Zone bleue")), 0, "ouverte");
				std::printf("    (e16) compile=%d « %s », porte bleue +%.2f\n", ok ? 1 : 0, s.graphe.message.CStr(),
							static_cast<double>(Y(m.scene, "Porte bleue") - yBleue0));
				if (!ok) {
					FILE *f = std::fopen(s.graphe.chemin.CStr(), "wb");
					std::printf("    (e16) fopen(%s) = %p errno=%d\n", s.graphe.chemin.CStr(), static_cast<void *>(f), errno);
					if (f != nullptr) {
						std::fclose(f);
					}
				}
				Temoin(ok && v != nullptr && v->valeur.x == 1.f && Absf(Y(m.scene, "Porte bleue") - yBleue0 - 2.f) < 0.01f,
					   "(e16) Blueprint recompile en jeu : « ouverte » gardee par nom", v != nullptr ? v->valeur.x : -1.f);
				NkEditeurFermerGraphe(s);
			}

			// ── (e13) Arreter : tout revient ──
			NkEditeurArreter(m);
			NkEditeurScriptsTrame(s, m, nullptr, 1.f / 60.f);
			Vider(s, journal);
			{
				const ecs::NkEntityId z = ParNom(m.scene, "Zone bleue");
				const NkVarScript *v = z.IsValid() ? NkScriptVariable(*m.scene.Monde().Get<NkScript2D>(z), 0, "ouverte") : nullptr;
				Temoin(Absf(Y(m.scene, "Porte bleue") - yBleue0) < 1e-4f && Absf(Y(m.scene, "Porte rouge") - yRouge0) < 1e-4f &&
						   (v == nullptr || v->valeur.x == 0.f) && s.hote.NbInstances() == 0u,
					   "(e13) Arreter : portes et variable reviennent, aucune instance", static_cast<float32>(s.hote.NbInstances()));
			}

			// ── (e14) Debut -> Afficher, construit par code ──
			{
				graph::NkNodeGraph g;
				NkBpGrapheBonjour(g);
				unkeny::NkModuleBp mod;
				NkErreurBp e;
				const bool ok = NkBpCompiler(g, mod, e);
				s.registre.EnregistrerBlueprint("Contenu/Scripts/Bonjour.nkbp", mod);
				const ecs::NkEntityId id = m.scene.Creer("Salut", NkVec2f(0.f, 8.f));
				NkScript2D sc;
				NkScriptAjouter(sc, "Contenu/Scripts/Bonjour.nkbp");
				m.scene.Monde().Add<NkScript2D>(id, sc);
				NkEditeurJouer(m);
				Trame(m, s, a, 0.f, journal);
				Temoin(ok && Journal(s, journal, "[Salut] Bonjour depuis un Blueprint"), "(e14) Debut -> Afficher : « Bonjour » au premier pas", 0.f);
				NkEditeurArreter(m);
				NkEditeurScriptsTrame(s, m, nullptr, 1.f / 60.f);
				Vider(s, journal);
			}

			// ── (e15) graphes invalides : l'erreur designe son noeud ──
			{
				graph::NkNodeGraph g;
				NkBpEnregistrerTypes(g);
				const graph::NkNodeId ev = NkBpCreerNoeud(g, "bp.ev.tick", 0.f, 0.f);
				const graph::NkNodeId f = NkBpCreerNoeud(g, "bp.natif:unkeny.corps.force", 300.f, 0.f);
				NkBpPoserDefaut(g, f, "force", "0 10");
				g.Connect(ev, "suite", f, "exec");
				unkeny::NkModuleBp mod;
				NkErreurBp e;
				const bool ok = NkBpCompiler(g, mod, e);
				std::printf("    (e15) %s (noeud %u)\n", e.message.CStr(), static_cast<unsigned>(e.noeud));
				Temoin(!ok && e.noeud == f && std::strstr(e.message.CStr(), "pas permis") != nullptr,
					   "(e15) une force sous Tick : refusee, sur SON noeud", static_cast<float32>(e.noeud));
				graph::NkNodeGraph h;
				NkBpEnregistrerTypes(h);
				const graph::NkNodeId ev2 = NkBpCreerNoeud(h, "bp.ev.debut", 0.f, 0.f);
				const graph::NkNodeId w = NkBpCreerNoeud(h, "bp.var.ecrire.reel", 300.f, 0.f);
				h.Connect(ev2, "suite", w, "exec");
				NkErreurBp e2;
				const bool ok2 = NkBpCompiler(h, mod, e2);
				Temoin(!ok2 && e2.noeud == w, "(e15) une variable sans nom : erreur sur son noeud", static_cast<float32>(e2.noeud));
			}

			// ── (e17) Enregistrer / Ouvrir ──
			{
				const ecs::NkEntityId z = ParNom(m.scene, "Zone rouge");
				NkScript2D *sc = m.scene.Monde().Get<NkScript2D>(z);
				if (sc != nullptr) {
					NkScriptAjouter(*sc, "Contenu/Scripts/PorteBlueprint.nkbp");
					NkScriptPoserVariable(*sc, 0, "hauteur", NkTypeVarScript::NK_REEL, NkVec2f(3.5f, 0.f));
				}
				const bool sauve = NkEditeurSauver(m);
				NkEditeurNouvelleScene(m);
				m.chemin = scene;
				const bool relu = NkEditeurOuvrir(m);
				const ecs::NkEntityId z2 = ParNom(m.scene, "Zone rouge");
				const NkScript2D *r = z2.IsValid() ? m.scene.Monde().Get<NkScript2D>(z2) : nullptr;
				const NkVarScript *h = r != nullptr ? NkScriptVariable(*r, 0, "hauteur") : nullptr;
				Temoin(sauve && relu && r != nullptr && r->nombre == 2 && std::strcmp(r->refs[0], "cpp:PorteCpp") == 0 &&
						   std::strcmp(r->refs[1], "Contenu/Scripts/PorteBlueprint.nkbp") == 0 && h != nullptr && h->valeur.x == 3.5f,
					   "(e17) Enregistrer / Ouvrir : scripts, ordre et valeur editee reviennent", h != nullptr ? h->valeur.x : -1.f);
			}

			// ── (c5) « + Ajouter > Script C++ / Blueprint » ──
			{
				NkEditeurBancTrame *pt = al.New<NkEditeurBancTrame>(m);
				NkEditeurBancTrame &T = *pt;
				NkEditeurCadre c = T.Cadre();
				T.Ui().contenuProjet = true;
				T.Ui().contenuDossier = "Scripts";
				NkEditeurActionScript(c, NK_A_SCRIPT + NK_SCRIPT_NOUVEAU_CPP);
				const bool fichier = NkFile::Exists((projet + "Contenu/Scripts/NouveauScript.cpp").CStr());
				bool compile = false;
				if (cpp) {
					compile = CompilerEtAttendre(s, m, journal) && s.registre.Trouver("cpp:NouveauScript") != 0u;
					Temoin(fichier && compile, "(c5) « + Ajouter > Script C++ » : le modele est cree ET compile", 0.f);
				} else {
					Temoin(fichier, "(c5) « + Ajouter > Script C++ » : le modele est cree", 0.f);
				}
				NkEditeurActionScript(c, NK_A_SCRIPT + NK_SCRIPT_NOUVEAU_BP);
				Temoin(NkFile::Exists((projet + "Contenu/Scripts/NouveauBlueprint.nkbp").CStr()) && s.graphe.ouvert,
					   "(c5) « + Ajouter > Blueprint » : l'asset est cree, sa page s'ouvre", 0.f);

				// ── (e18) la page du graphe, hors ecran ──
				// La recherche de la palette (comme si on l'avait tapee).
				std::snprintf(s.graphe.filtre, sizeof(s.graphe.filtre), "%s", "Afficher");
				T.Trame();
				const uint32 avant = s.graphe.graphe.NodeCount();
				int32 cible = -1;
				for (uint32 i = 0; i < s.graphe.paletteProtos.Size(); ++i) {
					const NkProtoBp &p = NkBpProtos()[static_cast<uint32>(s.graphe.paletteProtos[i])];
					if (p.type == "bp.natif:unkeny.journal.afficher") {
						cible = static_cast<int32>(i);
					}
				}
				if (cible >= 0) {
					const nkgui::NkRect r = s.graphe.paletteRects[static_cast<uint32>(cible)];
					T.Clic(0, r.x + r.w * 0.5f, r.y + r.h * 0.5f);
				}
				const uint32 apres = s.graphe.graphe.NodeCount();
				Temoin(cible >= 0 && apres == avant + 1u, "(e18) un clic sur la palette pose un noeud", static_cast<float32>(apres));
				// Le Debut du Blueprint neuf, et le noeud pose : tirer « suite » -> « exec ».
				graph::NkNodeId debut = graph::NK_NODE_INVALID, pose = s.graphe.canevas.selection;
				for (uint32 i = 0; i < s.graphe.graphe.RawNodeCount(); ++i) {
					const graph::NkNode *n = s.graphe.graphe.RawNodeAt(i);
					if (n != nullptr && n->alive && n->type == "bp.ev.debut") {
						debut = n->id;
					}
				}
				const graph::NkNode *nd = s.graphe.graphe.Find(debut);
				graph::NkNode *np = s.graphe.graphe.Find(pose);
				bool relie = false, refusNomme = false;
				if (nd != nullptr && np != nullptr) {
					np->x = nd->x + 300.f; // a cote, pour viser sans chevauchement
					np->y = nd->y + 220.f;
					T.Trame();
					const nkgui::NkRect zone = s.graphe.zoneCanevas;
					const editorkit::NkStyleCanevas st;
					nkgui::NkVec2 a0, b0;
					const int32 ks = nd->FindSocket("suite", graph::NkSocketDir::Output);
					const int32 ke = np->FindSocket("exec", graph::NkSocketDir::Input);
					const uint32 liensAvant = s.graphe.graphe.LinkCount();
					if (editorkit::NkCanevasPrise(s.graphe.canevas, zone, *nd, ks, st, a0) &&
						editorkit::NkCanevasPrise(s.graphe.canevas, zone, *np, ke, st, b0)) {
						// Le fil existant (Debut -> Afficher du modele) part de « suite » :
						// d'abord le couper (clic droit sur la prise), puis tirer.
						T.Clic(1, a0.x, a0.y);
						T.Glisser(a0.x, a0.y, b0.x, b0.y, 6);
						relie = s.graphe.graphe.LinkCount() >= 1u && s.graphe.graphe.LinkCount() <= liensAvant;
						const graph::NkLink *l = s.graphe.graphe.IncomingOf(pose, ke);
						relie = l != nullptr && l->fromNode == debut;
					}
					// Vers une entree de DONNEE (« texte ») : refus nomme.
					nkgui::NkVec2 c0;
					const int32 kt = np->FindSocket("texte", graph::NkSocketDir::Input);
					if (editorkit::NkCanevasPrise(s.graphe.canevas, zone, *nd, ks, st, a0) &&
						editorkit::NkCanevasPrise(s.graphe.canevas, zone, *np, kt, st, c0)) {
						T.Glisser(a0.x, a0.y, c0.x, c0.y, 6);
						refusNomme = std::strstr(s.graphe.canevas.refus.CStr(), "famille") != nullptr ||
									 std::strstr(s.graphe.canevas.refus.CStr(), "family") != nullptr;
						std::printf("    (e18) refus : %s\n", s.graphe.canevas.refus.CStr());
					}
				}
				Temoin(relie, "(e18) tirer un fil « suite » -> « exec » dans la page le relie", 0.f);
				Temoin(refusNomme, "(e18) vers une entree de DONNEE : le refus se nomme", 0.f);
				const bool compile2 = NkEditeurCompilerGraphe(s, m);
				Temoin(compile2, "(e18) le graphe edite a la souris compile et s'enregistre", 0.f);
				al.Delete(pt);
			}

			NkEditeurScriptsArreter(s);
			al.Delete(pl);
			al.Delete(pa);
			al.Delete(ps);
			al.Delete(pm);
			NkDirectory::Delete(racine.CStr(), true);
			std::printf("\n%s : %d reussis, %d echec, %d indetermine(s)\n", gE == 0 ? "BANC SCRIPTS EDITEUR REUSSI" : "BANC SCRIPTS EDITEUR EN ECHEC",
						gR, gE, gI);
			// (2026-10-01) L'editeur de Blueprint a la UE5 : compte a part.
			const int32 blueprint = NkEditeurLancerBancBlueprint();
			return (gE == 0 ? 0 : 1) | blueprint;
		}

	} // namespace editeur
} // namespace nkentseu
