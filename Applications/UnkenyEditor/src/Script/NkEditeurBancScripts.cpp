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
//   (c6)  (2026-10-02) LES ONGLETS DE DOCUMENT, dans la vraie trame (le bogue de
//         Rihen : « des qu'on a ouvert un Blueprint, je n'arrive plus a acceder a
//         la scene ») : le Blueprint ouvert a SON onglet, a droite de la scene, et
//         sa page est dessinee (pas la vue) ; UN CLIC SUR L'ONGLET DE LA SCENE la
//         ramene : la vue se dessine, plus la page ; une image et une page
//         Animation ouvertes s'ajoutent a LA MEME barre (scene en premier, aucun
//         chevauchement, rien hors de la fenetre) ; un clic sur l'onglet du
//         Blueprint le ramene ; le rouvrir ne fait pas de second onglet ; un
//         second Blueprint a le sien, ses DEUX graphes restent distincts ; la
//         croix d'un onglet devant le ferme et son voisin de gauche passe devant ;
//         Ctrl+W ferme le document devant ; la scene, elle, ne quitte jamais la
//         barre. Contre-epreuve de mutation : le corps rendu a l'ancienne regle
//         (« un Blueprint ouvert prend la place de la vue », NkEditeurGrapheOuvert)
//         -> (c6b) ECHEC (la vue ne revient pas), cite dans le commit.
//
// LE WORKSPACE JENGA ET NKCODE (01/10 soir, capture de Rihen 21:28 : NKCode
// ouvert sur le script disait « Aucun workspace ») :
//   (ws1)  le projet ouvert a son workspace `Portes.jenga`, les deux reperes, le
//         depot de l'editeur -- et JENGA LE LIT (`jenga info` : « Scripts »,
//         SharedLib). Contre-epreuve : NK_WS_MUTATION=illisible -> ECHEC
//   (ws2)  mis a jour SANS ECRASER : des lignes de l'utilisateur avant et apres
//         la partie generee, une partie perimee (autre depot) -> refaite, les
//         lignes intactes ; un .jenga sans reperes n'est pas touche (octet pour
//         octet). Contre-epreuve : NK_WS_MUTATION=ecrase -> ECHEC
//   (ws3)  `jenga build` du workspace (la commande de « Construire » dans
//         NKCode) -> l'editeur RECHARGE A CHAUD la DLL de Jenga (la copie chargee
//         est sous Intermediaire/Scripts/Jenga/) et, en Jouer, la porte rouge
//         s'ouvre. Contre-epreuve : NK_WS_MUTATION=jenga -> ECHEC
//   (ws4)  le lancement de NKCode : « "<Portes.jenga>" "<PorteCpp.cpp>" », et son
//         dossier de travail est la racine du depot (ses icones y sont).
//         Contre-epreuve : NK_WS_MUTATION=dossier -> ECHEC
//   Sans jenga dans le PATH : ws1 et ws3 rendent INDETERMINE, jamais vert.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurAssets.h"
#include "Editeur/NkEditeurBancTrame.h"
#include "Editeur/NkEditeurContenu.h"
#include "Editeur/NkEditeurDocuments.h"
#include "NKImage/Core/NkImage.h"
#include "Script/NkBpCatalogue.h"
#include "Script/NkEditeurExemplePortes.h"
#include "Script/NkEditeurGraphe.h"
#include "Script/NkEditeurScripts.h"
#include "Script/NkEditeurScriptsUi.h"
#include "Script/NkEditeurWorkspaceCpp.h"
#include "Livraison/NkEditeurConstruire.h"
#include "Livraison/NkEditeurMoteur.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "Unkeny/Entree/NkUnkenyActionsStandard.h"
#include "Unkeny/Jeu/NkUnkenyControleurs.h"

#include <cstdio>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#if defined(_WIN32)
#include <process.h> // _getpid : le dossier temporaire du banc est a ce processus
#else
#include <unistd.h>
#endif

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
				// ⚠️ (01/10 soir) UN DOSSIER PAR PROCESSUS. Il etait commun a tous :
				//    deux `--selftest` en meme temps (deux worktrees) s'effacaient le
				//    projet l'un a l'autre -- vu : un workspace disparu sous (ws2).
#if defined(_WIN32)
				const unsigned pid = static_cast<unsigned>(_getpid());
#else
				const unsigned pid = static_cast<unsigned>(getpid());
#endif
				return d + NkString::Format("/unkeny_banc_scripts_%u/", pid);
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

			// ── (c6) les onglets de document ──────────────────────────────────
			int32 OngletDe(const NkEditeurInterface &ui, const NkDocumentOuvert &d) {
				for (uint32 i = 0; i < ui.documents.ordre.Size(); ++i) {
					if (ui.documents.ordre[i] == d) {
						return static_cast<int32>(i);
					}
				}
				return -1;
			}
			nkgui::NkVec2 Milieu(const nkgui::NkRect &r) {
				return nkgui::NkVec2{r.x + r.w * 0.5f, r.y + r.h * 0.5f};
			}
			/// Ce que le CORPS a dessine a une trame, comme un oeil le voit : la VUE
			/// (elle pose l'appareil a chaque dessin) et la PAGE d'un graphe (elle
			/// pose son canevas).
			struct NkCorpsVu {
					bool vue = false;
					bool graphe = false;
			};
			NkCorpsVu TrameVue(NkEditeurBancTrame &T, NkEditeurScripts &s) {
				T.Ui().appareilEcran = nkgui::NkRect{0.f, 0.f, 0.f, 0.f};
				for (uint32 i = 0; i < s.graphes.Size(); ++i) {
					s.graphes[i]->zoneToile = nkgui::NkRect{0.f, 0.f, 0.f, 0.f};
				}
				T.Trame();
				NkCorpsVu v;
				v.vue = T.Ui().appareilEcran.w > 0.f;
				for (uint32 i = 0; i < s.graphes.Size(); ++i) {
					v.graphe = v.graphe || s.graphes[i]->zoneToile.w > 0.f;
				}
				return v;
			}

			void BancOngletsDocuments(NkEditeurBancTrame &T, NkEditeurModele &m, NkEditeurScripts &s, const NkString &projet) {
				NkEditeurInterface &ui = T.Ui();
				NkEditeurCadre c = T.Cadre();
				T.Fermer();
				// (c6a) Le Blueprint neuf de (c5) : son onglet, devant ; sa page dessinee.
				NkEditeurGrapheEtat *bp1 = s.courant;
				const NkDocumentOuvert d1 = bp1 != nullptr ? NkDoc(NkGenreDocument::NK_BLUEPRINT, bp1->id) : NkDocScene();
				NkCorpsVu v = TrameVue(T, s);
				const int32 p1 = OngletDe(ui, d1);
				const bool aDroite = p1 >= 0 && static_cast<uint32>(p1) < ui.documents.rects.Size() &&
									 ui.ongletScene.x + ui.ongletScene.w <= ui.documents.rects[static_cast<uint32>(p1)].x;
				Temoin(bp1 != nullptr && aDroite && NkEditeurBlueprintDevant(ui) && v.graphe && !v.vue,
					   "(c6a) le Blueprint a SON onglet, a droite de la scene ; sa page est devant", static_cast<float32>(p1));

				// (c6b) LE BOGUE : l'onglet de la scene la RAMENE.
				T.Clic(0, ui.ongletScene.x + 24.f, ui.ongletScene.y + ui.ongletScene.h * 0.5f);
				v = TrameVue(T, s);
				Temoin(NkEditeurSceneDevant(ui) && v.vue && !v.graphe && OngletDe(ui, d1) >= 0,
					   "(c6b) un clic sur l'onglet de la SCENE la ramene : la vue, plus la page", v.vue ? 1.f : 0.f);

				// (c6c) Une image et une page Animation : LA MEME barre.
				{
					uint8 px[4 * 4 * 4];
					for (int32 i = 0; i < 16; ++i) {
						const bool clair = ((i % 4) + (i / 4)) % 2 == 0;
						px[i * 4 + 0] = clair ? 230u : 40u;
						px[i * 4 + 1] = clair ? 230u : 40u;
						px[i * 4 + 2] = clair ? 230u : 40u;
						px[i * 4 + 3] = 255u;
					}
					NkImage img = NkImage::Wrap(px, 4, 4, NkImagePixelFormat::NK_RGBA32);
					(void)img.SavePNG((projet + "Contenu/Damier.png").CStr());
				}
				const bool image = NkEditeurOuvrirAsset(c, "Contenu/Damier.png") && NkEditeurOngletAssetActif(ui) >= 0;
				T.Trame();
				NkEditeurExecuter(c, NK_A_ANIM_ANIMATION);
				T.Trame();
				T.Trame();
				const bool anim = NkEditeurDocAnimActif(ui) != nullptr;
				const NkDocuments &D = ui.documents;
				bool range = D.ordre.Size() == 3u && D.rects.Size() == 3u;
				float32 xPrec = ui.ongletScene.x + ui.ongletScene.w;
				for (uint32 i = 0; range && i < D.rects.Size(); ++i) {
					range = D.rects[i].x >= xPrec - 0.5f && D.rects[i].x + D.rects[i].w <= T.W;
					xPrec = D.rects[i].x + D.rects[i].w;
				}
				const bool genres = D.ordre.Size() == 3u && D.ordre[0].genre == NkGenreDocument::NK_BLUEPRINT &&
									D.ordre[1].genre == NkGenreDocument::NK_ASSET && D.ordre[2].genre == NkGenreDocument::NK_ANIM;
				Temoin(image && anim && range && genres,
					   "(c6c) image + Animation : LA MEME barre (scene, Blueprint, image, Animation), rangee", static_cast<float32>(D.ordre.Size()));

				// (c6d) Un clic sur l'onglet du Blueprint le ramene devant.
				const int32 q1 = OngletDe(ui, d1);
				if (q1 >= 0) {
					const nkgui::NkRect r = D.rects[static_cast<uint32>(q1)];
					T.Clic(0, r.x + 30.f, r.y + r.h * 0.5f);
				}
				v = TrameVue(T, s);
				Temoin(D.actif == d1 && v.graphe && !v.vue, "(c6d) un clic sur l'onglet du Blueprint le ramene (sa page, pas la vue)",
					   static_cast<float32>(q1));

				// (c6e) Le rouvrir ne fait PAS de second onglet ; un AUTRE Blueprint a le sien.
				T.Clic(0, ui.ongletScene.x + 24.f, ui.ongletScene.y + ui.ongletScene.h * 0.5f);
				const uint32 n0 = D.ordre.Size();
				NkEditeurScriptOuvrirAsset(c, "Contenu/Scripts/NouveauBlueprint.nkbp");
				const bool meme = D.ordre.Size() == n0 && D.actif == d1 && s.graphes.Size() == 1u;
				NkEditeurScriptOuvrirAsset(c, "Contenu/Scripts/PorteBlueprint.nkbp");
				T.Trame();
				NkEditeurGrapheEtat *bp2 = s.courant;
				const NkDocumentOuvert d2 = bp2 != nullptr ? NkDoc(NkGenreDocument::NK_BLUEPRINT, bp2->id) : NkDocScene();
				const bool deux = bp2 != nullptr && bp1 != nullptr && bp2 != bp1 && s.graphes.Size() == 2u && D.ordre.Size() == n0 + 1u &&
								  D.actif == d2 && !(bp2->ref == bp1->ref);
				Temoin(meme && deux, "(c6e) rouvrir REACTIVE son onglet ; un second Blueprint a le sien (2 graphes)",
					   static_cast<float32>(s.graphes.Size()));

				// (c6f) La croix de l'onglet devant le ferme ; son voisin de GAUCHE passe devant.
				const int32 p2 = OngletDe(ui, d2);
				const NkDocumentOuvert gauche = p2 > 0 ? D.ordre[static_cast<uint32>(p2 - 1)] : NkDocScene();
				const nk_uint64 id2 = bp2 != nullptr ? bp2->id : 0;
				if (p2 >= 0) {
					const nkgui::NkVec2 x = Milieu(D.croix[static_cast<uint32>(p2)]);
					T.Clic(0, x.x, x.y);
				}
				T.Trame();
				Temoin(p2 > 0 && OngletDe(ui, d2) < 0 && NkEditeurGrapheParId(s, id2) == nullptr && D.actif == gauche &&
						   s.graphes.Size() == 1u,
					   "(c6f) la croix ferme l'onglet devant ; son voisin de gauche passe devant", static_cast<float32>(D.ordre.Size()));

				// (c6g) Ctrl+W ferme le document devant, un a un ; la scene reste, seule.
				for (int32 k = 0; k < 8 && !NkEditeurSceneDevant(ui); ++k) {
					T.Touche(nkgui::NkGuiKey::W, true);
				}
				Temoin(NkEditeurSceneDevant(ui) && D.ordre.Empty() && s.graphes.Empty() && ui.onglets.Empty() && ui.pagesAnim.docs.Empty(),
					   "(c6g) Ctrl+W ferme le document devant, un a un ; la scene reste, seule", static_cast<float32>(D.ordre.Size()));
				NkEditeurFermerTousOnglets(c);
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
				// (2026-10-01) La porte est un DOCUMENT : sa variable « hauteur » passe
				// a 3 (l'ancienne forme changeait le « 0 2 » d'un noeud + (vec2)).
				const int32 vh = s.Graphe().doc.TrouverVariable("hauteur");
				if (vh >= 0) {
					s.Graphe().doc.variables[static_cast<uint32>(vh)].defaut = "3";
				}
				const bool ok = NkEditeurCompilerGraphe(s, m);
				for (int32 k = 0; k < 10; ++k) {
					Trame(m, s, a, 0.f, journal);
				}
				const NkVarScript *v = NkScriptVariable(*m.scene.Monde().Get<NkScript2D>(ParNom(m.scene, "Zone bleue")), 0, "ouverte");
				std::printf("    (e16) compile=%d « %s », porte bleue +%.2f\n", ok ? 1 : 0, s.Graphe().message.CStr(),
							static_cast<double>(Y(m.scene, "Porte bleue") - yBleue0));
				if (!ok) {
					FILE *f = std::fopen(s.Graphe().chemin.CStr(), "wb");
					std::printf("    (e16) fopen(%s) = %p errno=%d\n", s.Graphe().chemin.CStr(), static_cast<void *>(f), errno);
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
				Temoin(NkFile::Exists((projet + "Contenu/Scripts/NouveauBlueprint.nkbp").CStr()) && s.Graphe().ouvert,
					   "(c5) « + Ajouter > Blueprint » : l'asset est cree, sa page s'ouvre", 0.f);

				// ── (e18) la page du graphe (l'editeur a la UE5), hors ecran ──
				// Clic droit dans le VIDE de la toile : le menu des noeuds ; la
				// recherche a le clavier (« Afficher ») ; un clic sur l'entree pose.
				NkEditeurBlueprintEtat &bp = s.Graphe();
				T.Trame();
				const nkgui::NkRect zt = bp.zoneToile;
				T.Clic(1, zt.x + zt.w * 0.82f, zt.y + zt.h * 0.82f);
				T.Taper("Afficher");
				const uint32 avant = bp.Graphe().NodeCount();
				int32 cible = -1;
				for (uint32 i = 0; i < bp.menu.ids.Size(); ++i) {
					const int32 id = bp.menu.ids[i];
					if (id >= 0 && static_cast<uint32>(id) < bp.menuEntrees.Size() && bp.menuEntrees[static_cast<uint32>(id)].cle == "bp.natif:unkeny.journal.afficher") {
						cible = static_cast<int32>(i);
					}
				}
				std::printf("    (e18) menu ouvert=%d, %u entree(s) visibles\n", bp.menu.ouvert ? 1 : 0, static_cast<unsigned>(bp.menu.ids.Size()));
				if (cible >= 0) {
					const nkgui::NkRect r = bp.menu.rects[static_cast<uint32>(cible)];
					T.Clic(0, r.x + r.w * 0.5f, r.y + r.h * 0.5f);
				}
				const uint32 apres = bp.Graphe().NodeCount();
				Temoin(cible >= 0 && apres == avant + 1u, "(e18) clic droit dans le vide, « Afficher », un clic : le noeud est pose", static_cast<float32>(apres));
				// Le Debut du Blueprint neuf, et le noeud pose : tirer « suite » -> « exec ».
				graph::NkNodeId debut = graph::NK_NODE_INVALID, pose = bp.Toile().selection;
				for (uint32 i = 0; i < bp.Graphe().RawNodeCount(); ++i) {
					const graph::NkNode *n = bp.Graphe().RawNodeAt(i);
					if (n != nullptr && n->alive && n->type == "bp.ev.debut") {
						debut = n->id;
					}
				}
				const graph::NkNode *nd = bp.Graphe().Find(debut);
				graph::NkNode *np = bp.Graphe().Find(pose);
				bool relie = false, refusNomme = false;
				if (nd != nullptr && np != nullptr) {
					np->x = nd->x + 320.f; // a cote, pour viser sans chevauchement
					np->y = nd->y + 240.f;
					T.Trame();
					const nkgui::NkRect zone = bp.zoneToile;
					const editorkit::NkStyleCanevas st;
					nkgui::NkVec2 a0, b0;
					const int32 ks = nd->FindSocket("suite", graph::NkSocketDir::Output);
					const int32 ke = np->FindSocket("exec", graph::NkSocketDir::Input);
					if (editorkit::NkCanevasPrise(bp.Toile(), zone, *nd, ks, st, a0) && editorkit::NkCanevasPrise(bp.Toile(), zone, *np, ke, st, b0)) {
						// Le fil existant (Debut -> Afficher du modele) part de « suite » :
						// d'abord le couper (clic droit sur la prise), puis tirer.
						T.Clic(1, a0.x, a0.y);
						T.Glisser(a0.x, a0.y, b0.x, b0.y, 6);
						const graph::NkLink *l = bp.Graphe().IncomingOf(pose, ke);
						relie = l != nullptr && l->fromNode == debut;
					}
					// Vers une entree de DONNEE (« texte ») : refus nomme.
					nkgui::NkVec2 c0;
					const int32 kt = np->FindSocket("texte", graph::NkSocketDir::Input);
					if (editorkit::NkCanevasPrise(bp.Toile(), zone, *nd, ks, st, a0) && editorkit::NkCanevasPrise(bp.Toile(), zone, *np, kt, st, c0)) {
						T.Glisser(a0.x, a0.y, c0.x, c0.y, 6);
						refusNomme = std::strstr(bp.Toile().refus.CStr(), "famille") != nullptr || std::strstr(bp.Toile().refus.CStr(), "family") != nullptr;
						std::printf("    (e18) refus : %s\n", bp.Toile().refus.CStr());
					}
				}
				Temoin(relie, "(e18) tirer un fil « suite » -> « exec » dans la page le relie", 0.f);
				Temoin(refusNomme, "(e18) vers une entree de DONNEE : le refus se nomme", 0.f);
				const bool compile2 = NkEditeurCompilerGraphe(s, m);
				Temoin(compile2, "(e18) le graphe edite a la souris compile et s'enregistre", 0.f);

				// ── (c6) LES ONGLETS DE DOCUMENT (2026-10-02, le bogue de Rihen : « des
				//    qu'on a ouvert un Blueprint, je n'arrive plus a acceder a la scene ») ──
				BancOngletsDocuments(T, m, s, projet);
				al.Delete(pt);
			}

			// ── (ws1..ws4) le workspace Jenga du projet, et NKCode ouvert dessus ──
			{
				if (std::getenv("NK_WS_MUTATION") != nullptr) {
					std::printf("    MUTATION DE BANC NK_WS_MUTATION=%s : un temoin ws doit ROUGIR\n", std::getenv("NK_WS_MUTATION"));
				}
				const NkString ws = NkEditeurWorkspaceChemin(projet.CStr());
				const NkString depot = NkTrouverDepot();
				const bool jenga = !NkVersionJenga().Empty();
				// (ws1) ecrit a l'ouverture (le releve du debut du banc), lisible par Jenga.
				{
					const NkString texte = NkFile::ReadAllText(ws.CStr());
					NkString d = depot;
					while (d.Length() > 1u && (d.CStr()[d.Length() - 1u] == '/' || d.CStr()[d.Length() - 1u] == '\\')) {
						d = NkString(d.CStr(), d.Length() - 1u);
					}
					for (usize i = 0; i < d.Length(); ++i) {
						if (d.CStr()[i] == '\\') {
							const_cast<char *>(d.CStr())[i] = '/';
						}
					}
					const bool ecrit = NkFile::Exists(ws.CStr()) && std::strstr(texte.CStr(), NK_WS_DEBUT) != nullptr &&
									   std::strstr(texte.CStr(), NK_WS_FIN) != nullptr && !d.Empty() &&
									   std::strstr(texte.CStr(), (NkString("NK = r\"") + d + "\"").CStr()) != nullptr &&
									   s.workspace == ws;
					if (!jenga) {
						Temoin(ecrit, "(ws1) l'ouverture ecrit Portes.jenga (reperes, depot de l'editeur)", 0.f);
						Indetermine("(ws1) Jenga LIT le workspace (jenga info)", "jenga introuvable dans le PATH");
					} else {
						NkVector<NkString> sortie;
						const int32 code = NkEditeurJengaSurWorkspace(ws.CStr(), "info", sortie);
						bool scripts = false;
						for (uint32 i = 0; i < sortie.Size(); ++i) {
							scripts = scripts || (std::strstr(sortie[i].CStr(), "Scripts") != nullptr &&
												  std::strstr(sortie[i].CStr(), "SharedLib") != nullptr);
						}
						if (code != 0 || !scripts) {
							for (uint32 i = 0; i < sortie.Size() && i < 8u; ++i) {
								std::printf("    jenga info : %s\n", sortie[i].CStr());
							}
						}
						Temoin(ecrit && code == 0 && scripts,
							   "(ws1) Portes.jenga ecrit a l'ouverture et LU par Jenga (Scripts, SharedLib)", static_cast<float32>(code));
					}
				}
				// (ws2) ce que l'utilisateur ecrit n'est jamais ecrase.
				{
					const NkString original = NkFile::ReadAllText(ws.CStr());
					const char *enTete = "# MA NOTE, en tete : a garder\n";
					const char *aMoi = "\n# MES LIGNES, apres la partie generee : a garder\nMON_REGLAGE = 42\n";
					NkString perime;
					const bool reperes = NkEditeurWorkspaceRemplacer(original, NkEditeurWorkspacePartie("Portes", "D:/un/autre/depot"), perime);
					if (!reperes) {
						std::printf("    (ws2) le workspace ouvert n'a pas ses reperes (%u octets) :\n%s\n", static_cast<unsigned>(original.Length()),
									original.CStr());
					}
					NkFile::WriteAllText(ws.CStr(), (NkString(enTete) + perime + aMoi).CStr());
					const NkWorkspaceCpp r = NkEditeurAssurerWorkspaceCpp(projet.CStr(), depot.CStr());
					const NkString apres = NkFile::ReadAllText(ws.CStr());
					NkString attendu;
					NkEditeurWorkspaceRemplacer(NkString(enTete) + perime + aMoi, NkEditeurWorkspacePartie("Portes", depot.CStr()), attendu);
					const bool garde = r.etat == NkEtatWorkspace::NK_MIS_A_JOUR && apres == attendu &&
									   std::strstr(apres.CStr(), enTete) != nullptr && std::strstr(apres.CStr(), "MON_REGLAGE = 42") != nullptr &&
									   std::strstr(apres.CStr(), "D:/un/autre/depot") == nullptr;
					// Un workspace a soi, SANS reperes : pas un octet ne change.
					const char *sien = "from Jenga import *\n# le workspace de l'utilisateur, sans reperes\nwith workspace(\"Sien\"):\n    pass\n";
					NkFile::WriteAllText(ws.CStr(), sien);
					const NkWorkspaceCpp r2 = NkEditeurAssurerWorkspaceCpp(projet.CStr(), depot.CStr());
					const bool respecte = r2.etat == NkEtatWorkspace::NK_RESPECTE && NkFile::ReadAllText(ws.CStr()) == NkString(sien) &&
										  std::strstr(r2.message.CStr(), "supprimez") != nullptr;
					std::printf("    (ws2) mise a jour : etat %d, lignes gardees %d ; sans reperes : etat %d, intact %d\n",
								static_cast<int>(r.etat), garde ? 1 : 0, static_cast<int>(r2.etat), respecte ? 1 : 0);
					Temoin(garde, "(ws2) partie generee refaite, lignes de l'utilisateur GARDEES", static_cast<float32>(r.etat));
					Temoin(respecte, "(ws2) un .jenga sans reperes n'est pas touche, le Journal dit quoi faire", static_cast<float32>(r2.etat));
					// Le workspace du projet redevient celui de l'editeur.
					NkFile::Delete(ws.CStr());
					NkEditeurAssurerWorkspaceCpp(projet.CStr(), depot.CStr());
				}
				// (ws3) Construire par Jenga -> la DLL de Jenga rechargee a chaud.
				if (!jenga || !cpp) {
					Indetermine("(ws3) jenga build -> DLL de Jenga rechargee a chaud", !jenga ? "jenga introuvable dans le PATH" : "clang++ introuvable");
				} else {
					NkVector<NkString> sortie;
					const NkString args = NkString::Format("build --target %s --config Debug --platform %s", NK_WS_PROJET, NkEditeurWorkspacePlateforme());
					const int32 code = NkEditeurJengaSurWorkspace(ws.CStr(), args.CStr(), sortie);
					if (code != 0) {
						for (uint32 i = 0; i < sortie.Size(); ++i) {
							std::printf("    jenga build : %s\n", sortie[i].CStr());
						}
					}
					const uint32 avant = s.rechargesJenga;
					for (int32 k = 0; k < 4; ++k) { // deux releves suffisent (vu, puis stable)
						NkEditeurScriptsTrame(s, m, nullptr, 0.5f);
						Vider(s, journal);
					}
					const NkString copie = s.modules.Copie();
					const bool recharge = s.rechargesJenga == avant + 1u && Journal(s, journal, "construite par Jenga") &&
										  std::strstr(copie.CStr(), NK_WS_SORTIE) != nullptr;
					std::printf("    (ws3) jenga build : code %d ; module charge : %s\n", code, copie.CStr());
					Temoin(code == 0 && recharge, "(ws3) jenga build du workspace : la DLL de Jenga est RECHARGEE a chaud",
						   static_cast<float32>(s.modules.Generation()));
					// ... et c'est elle qui ouvre la porte rouge en Jouer.
					const float32 yRouge = Y(m.scene, "Porte rouge");
					const uint32 lignes = journal.Size();
					NkEditeurJouer(m);
					for (int32 k = 0; k < 330; ++k) {
						Trame(m, s, a, 1.f, journal);
					}
					bool ouverte = false;
					for (uint32 i = lignes; i < journal.Size(); ++i) {
						ouverte = ouverte || std::strstr(journal[i].CStr(), "La porte rouge s'ouvre (C++)") != nullptr;
					}
					const float32 dy = Y(m.scene, "Porte rouge") - yRouge;
					Temoin(recharge && ouverte && dy > 0.5f, "(ws3) en Jouer, la DLL de Jenga ouvre la porte rouge", dy);
					NkEditeurArreter(m);
					NkEditeurScriptsTrame(s, m, nullptr, 1.f / 60.f);
					Vider(s, journal);
				}
				// (ws4) NKCode lance SUR le workspace et le script, depuis la racine du depot.
				{
					const NkString script = projet + "Contenu/Scripts/PorteCpp.cpp";
					const NkLancementNKCode l = NkEditeurLancementNKCode(ws.CStr(), script.CStr());
					const bool args = std::strstr(l.arguments.CStr(), (NkString("\"") + ws + "\"").CStr()) == l.arguments.CStr() &&
									  std::strstr(l.arguments.CStr(), "PorteCpp.cpp\"") != nullptr;
					const bool icones = !l.dossier.Empty() &&
										NkDirectory::Exists((l.dossier + (l.dossier.EndsWith("/") ? "" : "/") + "Applications/NKCode/data/textures").CStr());
					std::printf("    (ws4) NKCode : %s %s (dossier %s)\n", l.exe.Empty() ? "(non construit)" : l.exe.CStr(), l.arguments.CStr(),
								l.dossier.CStr());
					if (depot.Empty()) {
						Temoin(args, "(ws4) NKCode recoit le workspace PUIS le script", 0.f);
						Indetermine("(ws4) dossier de travail de NKCode = le depot", "depot introuvable");
					} else {
						Temoin(args && icones, "(ws4) NKCode : workspace PUIS script, lance depuis le depot (ses icones)", 0.f);
					}
				}
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
