#pragma once
#include "NKFileSystem/NkFile.h"
// -----------------------------------------------------------------------------
// @File    Applications/NKEditorKitTest/src/NkAiPanneauProbe.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   FAMILLE 24 — le PANNEAU IA du kit, entier : une conversation par
//          assistant, les menus, et son IMAGE a cote de la capture cible.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// ⚠️ CETTE FAMILLE ECRIT UNE IMAGE, ET C'EST ELLE QUI COMPTE.
//    Les familles 22 et 23 etaient vertes pendant que Rodolf voyait autre chose.
//    Ici le panneau est rendu dans une vraie liste NKGui, avec les vraies
//    polices embarquees, puis rasterise sans GPU : `Build/panneau_ia/kit_695.png`
//    se lit A COTE de `Screenshot 2026-09-20 111040.png` (695 x 1292, la meme
//    taille). Les essais ci-dessous ne remplacent pas ce regard : ils disent
//    seulement que l'image a ete ecrite et que ses gestes tiennent.
//
// ⚠️ CE N'EST PAS LA PREUVE DES APPLICATIONS. Chaque application rend SON
//    panneau par sa propre porte (NK_AI_IMAGE) ; ceci est l'image du KIT seul,
//    celle qui sert a regler la forme sans relancer trois applications.
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkAiPanneau.h"
#include "NKEditorKit/NkAiPanneauImage.h"
#include "NKEditorKit/Components/NkGuiComponentPaint.h"
#include "NKFileSystem/NkDirectory.h"
#include <stdio.h>

namespace aipanneauprobe {

	using namespace nkentseu;
	using namespace nkentseu::editorkit;

	struct Bilan {
			uint32 ok = 0;
			uint32 total = 0;
	};

	inline void Essai(Bilan &b, const char *id, bool cond, const char *quoi) {
		++b.total;
		if (cond)
			++b.ok;
		printf("  [%s] %-6s %s\n", cond ? " ok " : "FAIL", id, quoi);
	}

	/// Le contenu de la capture : memes types de blocs, dans le meme ordre.
	inline void Remplir(NkAiFil &f) {
		NkString p;
		auto pousser = [&](NkAiBloc t, const char *titre, const char *texte, const char *in, const char *out,
						   bool replie) {
			NkAiBlocDonnees b;
			b.type = t;
			b.titre = NkString(titre ? titre : "");
			b.texte = NkString(texte ? texte : "");
			b.entree = NkString(in ? in : "");
			b.sortie = NkString(out ? out : "");
			b.replie = replie;
			(void)f.Pousser(b, p);
		};
		pousser(NkAiBloc::Demande, nullptr,
				"noublie pas que on doit aussi brancher claude comme on a brancher claude a nkcode pour pouvoir "
				"utiliser claude dans nos outils si on le souhaite.",
				nullptr, nullptr, false);
		pousser(NkAiBloc::Outil, "Bash", "Deposer une troisieme variante sans recompiler",
				"cd D:/Projets/2026/Nkentseu/Nkentseu-sculpt && md5sum Build/Bin/Release-Windows/NKSculptHarness/NKSculptHarness.exe\n"
				"# ==============================================================================\n"
				"# Durcir -- LA PREUVE QUE LA REGLE DE RODOLF TIENT.\n"
				"# ------------------------------------------------------------------------------",
				"depose.\n"
				"   - creuser          libelle=\"Creuser\" op=0 profil=0 rayon=0.250 force=0.50 sens=-1\n"
				"   - dessiner         libelle=\"Dessiner\" op=0 profil=0 rayon=0.250 force=0.50 sens=+1\n"
				"   - durcir           libelle=\"Durcir\" op=2 profil=0 rayon=0.250 force=0.50 sens=+1",
				false);
		pousser(NkAiBloc::Reflexion, "Thinking", "le banc doit relire le catalogue", nullptr, nullptr, true);
		pousser(NkAiBloc::Reflexion, "Thinking", "deux rouges : lesquels", nullptr, nullptr, true);
		pousser(NkAiBloc::Prose, nullptr,
				"`durcir` apparait — **sans recompilation** — et le banc l'eprouve aussitot (+58 criteres). Mais 2 "
				"rouges apparaissent : je regarde lesquels.",
				nullptr, nullptr, true);
		pousser(NkAiBloc::Outil, "Bash", "Voir les rouges et l'empreinte",
				"cd D:/Projets/2026/Nkentseu/Nkentseu-sculpt && md5sum Build/Bin/Release-Windows/NKSculptHarness/NKSculptHarness.exe",
				"f36ee6d3cb688c7ee6f444adce2eaad2 *Build/Bin/Release-Windows/NKSculptHarness/NKSculptHarness.exe\n"
				"  [ROUGE] lisser : la rugosite DESCEND franchement       rugosite 0.180995 -> 0.376434 (--1\n"
				"  [ROUGE] lisser : deux passages ne remontent pas        0.180995 -> 0.376434 -> 0.780101\n"
				"  [info] 2 rouges sur 60",
				false);
		pousser(NkAiBloc::Outil, "Bash", "Smoke test the Claude CLI one-shot call",
				"cd \"C:/Users/Rihen/AppData/Local/Temp/claude/d--Rihen-Rodolf/b7dabf70-0179-4d58-8a52-4b8b56\"",
				"real    0m14.512s\nuser    0m0.015s\nsys     0m0.015s\n", false);
		pousser(NkAiBloc::Reflexion, "Thinking", "l'ordre alphabetique", nullptr, nullptr, true);
		pousser(NkAiBloc::Prose, nullptr,
				"Le depot d'un fichier vient de **casser mon propre critere** — et il a raison : je prenais « la "
				"premiere brosse SMOOTH », or l'ordre alphabetique met `durcir` avant `lisser`. Je testais `durcir` en "
				"exigeant qu'elle lisse. C'est « un indice n'est pas un nom », applique a moi.",
				nullptr, nullptr, true);
		pousser(NkAiBloc::Outil, "Bash", "Localiser les deux recherches",
				"cd D:/Projets/2026/Nkentseu/Nkentseu-sculpt && grep -n \"b.op == NkSculptOp::NK_SCULPT_OP_SMOOTH\" main.cpp",
				"997:                    if (reg.At(bi, b) && b.op == NkSculptOp::NK_SCULPT_OP_SMOOTH) {\n"
				"1028:                        if (reg.At(bi, b) && b.op == NkSculptOp::NK_SCULPT_OP_NORMAL) {",
				false);
	}

	inline void Declarer(NkAiPanneau &pan) {
		NkAiFournisseurDesc a;
		a.cle = NkString("claude");
		a.nom = NkString("Claude");
		a.distant = true;
		NkAiModeleDesc m1;
		m1.nom = NkString("Opus 5 (1M)");
		m1.detail = NkString("High");
		NkAiModeleDesc m2;
		m2.nom = NkString("Sonnet");
		a.modeles.PushBack(m1);
		a.modeles.PushBack(m2);
		pan.fournisseurs.PushBack(a);
		NkAiFournisseurDesc b;
		b.cle = NkString("local");
		b.nom = NkString("Local");
		NkAiModeleDesc m3;
		m3.nom = NkString("qwen2.5-coder 7B");
		b.modeles.PushBack(m3);
		pan.fournisseurs.PushBack(b);
		NkAiFournisseurDesc c;
		c.cle = NkString("ollama");
		c.nom = NkString("Ollama");
		c.pret = false;
		c.motif = NkString("Lancez « ollama serve »");
		pan.fournisseurs.PushBack(c);
		NkAiModeDesc md;
		md.nom = NkString("Auto");
		md.detail = NkString("applique sans demander");
		pan.modes.PushBack(md);
		NkAiCommandeDesc cmd;
		cmd.nom = NkString("/compact");
		cmd.insertion = NkString("/compact ");
		pan.commandes.PushBack(cmd);
		pan.portePlus = true;
		NkAiCapacites cap = NkAiCapacites::Texte();
		cap.produitOutil = true;
		cap.produitRefus = true;
		cap.produitReflexion = true;
		pan.capacites = cap;
	}

	inline Bilan Sonder() {
		Bilan b;
		const int32 W = 695, H = 1292;
		static nkgui::NkGuiContext ctx;
		static bool init = false;
		static nkgui::NkGuiFont texte, mono, corps;
		if (!init) {
			init = true;
			(void)ctx.Init(W, H);
			(void)texte.LoadEmbedded(NkEmbeddedFontId::Inter, 13.f);
			(void)mono.LoadEmbedded(NkEmbeddedFontId::Cousine, 12.f, false);
			mono.texId = texte.TexId() + 3u;
			// LE CORPS DU PANNEAU (21/09) : Inter 15, la taille que la capture ecrit
			// (96 caracteres par ligne la ou Inter 13 en met 110).
			(void)corps.LoadEmbedded(NkEmbeddedFontId::Inter, 15.f);
			corps.texId = texte.TexId() + 4u;
			ctx.font = &texte;
		}
		const NkTheme theme = NkTheme::Dark();
		(void)NkDirectory::CreateRecursive("Build/panneau_ia");

		NkAiPanneau pan;
		Declarer(pan);
		Remplir(pan.Fil());
		pan.PoserSaisie("qu ce soit nk code ou nk3dmodeler ou nkuidesign ou nimprote quel de nos app qui utilise une ia je "
						"veux que le panneau soit exactement comme ceci donc meme design meme emplacement dans sa pastille "
						"meme comportement stp donc redesign totalement tut pour suivre ce panneau en capture.");
		pan.occupe = true;
		pan.fileAttente = true; // la capture : un tour en vol, et l'envoi reste une fleche
		pan.maintenant = 3420.0; // 57 minutes apres le debut de la conversation
		pan.phase = 0.2f;

		auto image = [&](const char *chemin) {
			ctx.BeginFrame(1.f / 60.f);
			NkGuiComponentPaint pc(ctx, theme);
			pc.PoserPolices(nullptr, &mono, &corps);
			(void)pan.Dessiner(ctx, pc, {0.f, 0.f, (float32)W, (float32)H}, false);
			if (!chemin) { // une image pour PUBLIER le plan, sans fichier
				ctx.EndFrame();
				NkAiImageResultat vide;
				vide.ok = true;
				return vide;
			}
			const nkgui::NkGuiFont *pol[3] = {&texte, &mono, &corps};
			const NkAiImageResultat r = NkAiEcrireImage(ctx.dl, W, H, 0.f, 0.f, (float32)W, (float32)H, pol, 3,
														theme.Get(NkRole::PanelBg), chemin);
			ctx.EndFrame();
			printf("         %s\n", r.message);
			return r;
		};
		// La premiere image pose le debut de la conversation ; la seconde a 57 min.
		pan.maintenant = 0.0;
		(void)image("Build/panneau_ia/kit_695_amorce.png");
		pan.maintenant = 3420.0;
		const NkAiImageResultat r1 = image("Build/panneau_ia/kit_695.png");
		Essai(b, "24a", r1.ok && r1.texturesInconnues == 0,
			  "l'image du panneau est ecrite, sans AUCUNE texture inconnue (tout ce qui s'y voit est rendu)");

		// 24b — L'EN-TETE, LE FIL ET LE COMPOSEUR DE LA CAPTURE SONT PUBLIES.
		{
			const NkAiPlan &c = pan.planChrome;
			const NkAiPlan &f = pan.planFil;
			const bool ok = c.Compter(NkAiPiece::IconeHistorique) == 1 && c.Compter(NkAiPiece::IconeNouvelle) == 1 &&
							c.Compter(NkAiPiece::ComposeurCadre) == 1 && c.Compter(NkAiPiece::Envoi) == 1 &&
							c.Compter(NkAiPiece::PastilleModele) == 1 && c.Compter(NkAiPiece::Activite) == 1 &&
							f.Compter(NkAiPiece::BoiteOutil) == 4 && f.Compter(NkAiPiece::Rail) > 0 &&
							f.Compter(NkAiPiece::FondCode) >= 3;
			Essai(b, "24b", ok, "titre + 2 icones, 4 boites d'outil, rail, code en ligne, composeur, envoi, activite");
		}

		// 24d — (Q8) CHANGER DE FOURNISSEUR OU DE MODELE NE VIDE JAMAIS LE FIL : c'est
		//       un reglage du chat. (Remplace l'ancien 24d, qui ENCODAIT le defaut :
		//       « Claude -> Local : fil vide ».)
		{
			NkString pq;
			pan.occupe = false;
			const uint32 avant = pan.Fil().Taille();
			const bool c1 = pan.Choisir(1, pq);
			const bool garde1 = pan.Fil().Taille() == avant;
			pan.fournisseurs[1].modele = 1;
			const bool c0 = pan.Choisir(0, pq);
			const bool garde0 = pan.Fil().Taille() == avant;
			Essai(b, "24d", avant > 0u && c1 && garde1 && c0 && garde0,
				  "Claude puis Local, et un autre modele : le fil reste le MEME, bloc pour bloc");
		}
		// 24e — ON NE CHANGE PAS PENDANT UN TOUR, et le refus est NOMME.
		{
			NkString pq;
			pan.occupe = true;
			const bool refuse = !pan.Choisir(1, pq) && pq.Length() > 0 && pan.Actif() == 0;
			pan.occupe = false;
			Essai(b, "24e", refuse, "pendant un tour, changer d'assistant est REFUSE avec son motif");
		}
		// 24f — (Q8) TROIS CHATS, UN CHANGEMENT DE MODELE AU MILIEU DE L'UN, DES
		//       BASCULES, et un RELANCEMENT : chaque chat relu intact -- fil, brouillon,
		//       modele. Le fil vivant appartient a l'HOTE (`Lier`, comme le modeleur).
		{
			const char *kChemin = "Build/panneau_ia/kit_chats.txt";
			NkFile::Delete(kChemin);
			NkString pq;
			NkAiFil vivant;
			bool ok = true;
			{
				NkAiPanneau p2;
				Declarer(p2);
				p2.Lier(&vivant);
				p2.cheminChats = NkString(kChemin);
				auto demande = [&](const char *t) {
					NkAiBlocDonnees d;
					d.type = NkAiBloc::Demande;
					d.texte = NkString(t);
					(void)p2.Fil().Pousser(d, pq);
				};
				(void)p2.Choisir(1, pq); // le chat un parle a Local
				demande("chat un");
				p2.PoserSaisie("brouillon un");
				p2.NouvelleConversation();
				ok = ok && vivant.Taille() == 0 && NkString(p2.Saisie()) == NkString("");
				demande("chat deux");
				(void)p2.Choisir(0, pq); // un changement de fournisseur ET de modele AU MILIEU du chat deux
				p2.fournisseurs[0].modele = 1;
				demande("chat deux, suite");
				ok = ok && vivant.Taille() == 2;
				p2.NouvelleConversation();
				demande("chat trois");
				p2.Rouvrir(0); // le plus ancien : chat un
				ok = ok && vivant.Taille() == 1 && vivant.At(0).texte == NkString("chat un") &&
					 NkString(p2.Saisie()) == NkString("brouillon un") && p2.Actif() == 1;
				ok = ok && p2.EnregistrerChats(kChemin);
			}
			// LE RELANCEMENT : un panneau NEUF relit le fichier.
			NkAiFil vivant2;
			NkAiPanneau p3;
			Declarer(p3);
			p3.Lier(&vivant2);
			const bool lu = p3.ChargerChats(kChemin);
			bool relu = lu && p3.Chats() == 3u && vivant2.Taille() == 1 && vivant2.At(0).texte == NkString("chat un") &&
						NkString(p3.Saisie()) == NkString("brouillon un");
			// le chat deux : ses DEUX demandes, et le modele choisi en cours de route
			p3.BasculerChat(1);
			relu = relu && vivant2.Taille() == 2 && vivant2.At(1).texte == NkString("chat deux, suite") &&
				   p3.Actif() == 0 && p3.fournisseurs[0].modele == 1;
			p3.BasculerChat(2);
			relu = relu && vivant2.Taille() == 1 && vivant2.At(0).texte == NkString("chat trois");
			printf("         chats : ecrits=%d relus=%d (3 chats, bascules, modele change au milieu)\n", ok ? 1 : 0,
				   relu ? 1 : 0);
			Essai(b, "24f", ok && relu,
				  "trois chats, un modele change en route, des bascules, un relancement : chacun relu INTACT");
		}
		// 24g — LE MENU A DEUX NIVEAUX, avec « Ajouter une IA… » en bas.
		//       (21/09, Q5 : un titre de section en tete ; les modeles portent
		//       l'Effort en pied de liste, comme 065128.)
		{
			pan.occupe = false;
			pan.OuvrirMenu(NkAiMenu::Fournisseurs);
			const NkAiImageResultat r2 = image("Build/panneau_ia/kit_695_menu.png");
			const uint32 lignes = pan.planChrome.Compter(NkAiPiece::MenuLigne);
			pan.OuvrirMenu(NkAiMenu::Modeles, 0);
			(void)image("Build/panneau_ia/kit_695_modeles.png");
			const uint32 lignes2 = pan.planChrome.Compter(NkAiPiece::MenuLigne);
			const uint32 curseurs = pan.planChrome.Compter(NkAiPiece::Curseur);
			pan.FermerMenus();
			Essai(b, "24g", r2.ok && lignes == 5u && lignes2 == 5u && curseurs == 1u,
				  "fournisseurs : titre + 3 + « Ajouter une IA… » ; modeles de Claude : retour + titre + 2 + Effort");
		}

		// ── Q5 : LES PROPRIETES DU MODELE ─────────────────────────────────────
		// Un clic est ECRIT dans l'etat d'entree de NKGui (mousePos, mouseClicked) :
		// aucune entree n'est injectee sur la machine.
		auto clic = [&](float32 x, float32 y) {
			ctx.BeginFrame(1.f / 60.f);
			ctx.input.mousePos = {x, y};
			ctx.input.mouseClicked[0] = true;
			NkGuiComponentPaint pc(ctx, theme);
			pc.PoserPolices(nullptr, &mono, &corps);
			const NkAiSorties s = pan.Dessiner(ctx, pc, {0.f, 0.f, (float32)W, (float32)H}, true);
			ctx.input.mouseClicked[0] = false;
			ctx.EndFrame();
			return s;
		};
		auto ligneDrapeaux = [&](NkAiPiece piece) -> uint8 {
			NkAiRectPublie q;
			return pan.planChrome.Trouver(0u, piece, q) ? q.drapeaux : (uint8)0xFFu;
		};
		// 24i — LE MENU « / » : filtre, sections, Effort en curseur, Thinking en interrupteur.
		{
			pan.OuvrirMenu(NkAiMenu::Commandes);
			const NkAiImageResultat r = image("Build/panneau_ia/kit_695_slash.png");
			uint32 sections = 0;
			for (uint32 i = 0; i < pan.planChrome.Pieces(); ++i)
				if (pan.planChrome.Piece(i).piece == NkAiPiece::MenuLigne &&
					(pan.planChrome.Piece(i).drapeaux & kAiSection) != 0u)
					++sections;
			const bool ok = r.ok && sections >= 3u && pan.planChrome.Compter(NkAiPiece::MenuFiltre) == 1u &&
							pan.planChrome.Compter(NkAiPiece::Curseur) == 1u &&
							pan.planChrome.Compter(NkAiPiece::Interrupteur) == 1u;
			printf("         « / » : %u section(s), %u ligne(s)\n", (unsigned)sections,
				   (unsigned)pan.planChrome.Compter(NkAiPiece::MenuLigne));
			Essai(b, "24i", ok, "« / » : filtre, sections titrees, Effort (curseur), Thinking (interrupteur)");
		}
		// 24j — LE FILTRE garde une section tant qu'une de ses lignes passe.
		{
			pan.PoserFiltre("think");
			(void)image("Build/panneau_ia/kit_695_slash_filtre.png");
			const uint32 n = pan.planChrome.Compter(NkAiPiece::MenuLigne);
			pan.PoserFiltre("");
			Essai(b, "24j", n == 2u, "filtre « think » : la section Modele et la seule ligne Thinking (2 lignes)");
		}
		// 24k — UNE PROPRIETE SANS OBJET EST GRISEE AVEC SON MOTIF, JAMAIS CACHEE,
		//       et son clic ne change rien.
		{
			pan.fournisseurs[0].modeles[0].motifPensee = NkString("ce modele n'annonce pas « thinking »");
			(void)image("Build/panneau_ia/kit_695_slash_grise.png");
			const uint8 d = ligneDrapeaux(NkAiPiece::Interrupteur);
			NkAiRectPublie q;
			bool change = true;
			if (pan.planChrome.Trouver(0u, NkAiPiece::Interrupteur, q)) {
				const bool avant = pan.penser;
				const NkAiSorties s = clic(q.x + q.w * 0.5f, q.y + q.h * 0.5f);
				change = s.penserChange || pan.penser != avant;
			}
			bool motif = false;
			for (uint32 i = 0; i < pan.planChrome.Pieces(); ++i)
				if (pan.planChrome.Piece(i).source == NkAiSource::MenuDetail && pan.planChrome.Piece(i).w > 10.f)
					motif = true;
			pan.fournisseurs[0].modeles[0].motifPensee = NkString();
			Essai(b, "24k", d != 0xFFu && (d & kAiEteint) != 0u && !change && motif,
				  "Thinking sans objet : present, GRISE, motif ecrit, et le clic ne change rien");
		}
		// 24l — LES REGLAGES SE FONT A LA SOURIS ET LE DISENT : l'interrupteur
		//       bascule, le curseur prend le cran sous la souris.
		{
			(void)image("Build/panneau_ia/kit_695_slash.png");
			NkAiRectPublie q;
			bool bascule = false, crans = false;
			if (pan.planChrome.Trouver(0u, NkAiPiece::Interrupteur, q)) {
				const bool avant = pan.penser;
				const NkAiSorties s = clic(q.x + q.w * 0.5f, q.y + q.h * 0.5f);
				bascule = s.penserChange && pan.penser != avant;
			}
			(void)image(nullptr);
			if (pan.planChrome.Trouver(0u, NkAiPiece::Curseur, q)) {
				const NkAiSorties s0 = clic(q.x + 1.f, q.y + q.h * 0.5f);
				const int32 e0 = pan.effort;
				(void)image(nullptr);
				const NkAiSorties s1 = clic(q.x + q.w - 1.f, q.y + q.h * 0.5f);
				crans = s0.effortChange && e0 == 0 && s1.effortChange && pan.effort == 3 &&
						pan.MenuOuvert() == NkAiMenu::Commandes;
			}
			pan.penser = true;
			Essai(b, "24l", bascule && crans,
				  "Thinking bascule ; Effort : bord gauche = cran 0, bord droit = cran 3 ; le menu reste ouvert");
			pan.FermerMenus();
		}
		// 24m — LE MENU « + » (065237) : ce que l'hote sait joindre, une entree grisee avec son motif.
		{
			NkAiEntreeDesc e1;
			e1.nom = NkString("Envoyer depuis l'ordinateur");
			e1.detail = NkString("un fichier joint a la demande");
			e1.id = 7;
			NkAiEntreeDesc e2;
			e2.nom = NkString("Une image pour l'objet 3D");
			e2.motif = NkString("la voie image -> 3D vit dans le modeleur");
			e2.id = 8;
			pan.entreesPlus.PushBack(e1);
			pan.entreesPlus.PushBack(e2);
			(void)image(nullptr);
			NkAiRectPublie q;
			bool ouvert = false, choisi = false;
			if (pan.planChrome.Trouver(0u, NkAiPiece::BoutonPlus, q)) {
				(void)clic(q.x + q.w * 0.5f, q.y + q.h * 0.5f);
				ouvert = pan.MenuOuvert() == NkAiMenu::Plus;
			}
			(void)image("Build/panneau_ia/kit_695_plus.png");
			const uint32 n = pan.planChrome.Compter(NkAiPiece::MenuLigne);
			for (uint32 i = 0; i < pan.planChrome.Pieces(); ++i) {
				const NkAiRectPublie &l = pan.planChrome.Piece(i);
				if (l.piece == NkAiPiece::MenuLigne && l.debut == 0u) {
					const NkAiSorties s = clic(l.x + 20.f, l.y + l.h * 0.5f);
					choisi = s.entreePlus == 7;
					break;
				}
			}
			Essai(b, "24m", ouvert && n == 2u && choisi, "« + » : 2 entrees ; la premiere rend son id a l'hote");
		}
		// 24n — LES MODES (065246) : nom + description, coche, Effort en pied.
		{
			pan.modes.Clear();
			const char *nm[4][2] = {{"Manuel", "demande avant chaque action"},
									{"Edition automatique", "applique les modifications sans demander"},
									{"Plan", "montre le plan en parties avant d'agir"},
									{"Auto", "enchaine jusqu'au resultat"}};
			for (int32 i = 0; i < 4; ++i) {
				NkAiModeDesc md;
				md.nom = NkString(nm[i][0]);
				md.detail = NkString(nm[i][1]);
				pan.modes.PushBack(md);
			}
			pan.mode = 2;
			pan.OuvrirMenu(NkAiMenu::Modes);
			const NkAiImageResultat r = image("Build/panneau_ia/kit_695_modes.png");
			const uint32 n = pan.planChrome.Compter(NkAiPiece::MenuLigne);
			const uint8 d = ligneDrapeaux(NkAiPiece::PastilleMode);
			pan.FermerMenus();
			Essai(b, "24n", r.ok && n == 6u && d != 0xFFu, "modes : titre + 4 + Effort, la pastille du mode dans la barre");
		}
		// 24o — LES FENETRES (065228, 065201) : Utilisation et Carte des agents.
		{
			pan.utilisation.Vider();
			pan.utilisation.Section("Modele local");
			pan.utilisation.Valeur("Modele", "qwen2.5:7b-instruct");
			pan.utilisation.Valeur("Charge en memoire", "oui, 4.7 Go en memoire video");
			pan.utilisation.Barre("Memoire video", "4.7 / 8.0 Go", 0.59f);
			pan.utilisation.Section("Dernier tour");
			pan.utilisation.Valeur("Jetons generes", "212");
			pan.utilisation.Valeur("Vitesse", "38.4 jetons/s");
			pan.utilisation.Valeur("Duree", "5.5 s");
			pan.utilisation.Note("Rien n'a quitte cette machine.");
			pan.OuvrirFenetre(1);
			const NkAiImageResultat r = image("Build/panneau_ia/kit_695_utilisation.png");
			const bool pieces =
				pan.planChrome.Compter(NkAiPiece::Fenetre) == 1u && pan.planChrome.Compter(NkAiPiece::Barre) == 1u;
			NkAiRectPublie q;
			bool ferme = false;
			if (pan.planChrome.Trouver(0u, NkAiPiece::FenetreFermer, q)) {
				const NkAiSorties s = clic(q.x + q.w * 0.5f, q.y + q.h * 0.5f);
				ferme = s.fenetreFermee && pan.FenetreOuverte() == 0u;
			}
			pan.carte.Vider();
			pan.carte.Section("Tour en cours : « une chaise »");
			pan.carte.Valeur("1. Planifier en parties", "fait · 1.2 s");
			pan.carte.Valeur("2. Assise", "fait · 3 verbes");
			pan.carte.Valeur("3. Pieds", "en cours");
			pan.carte.Valeur("4. Dossier", "a venir");
			pan.carte.Barre("Progression", "2 / 4", 0.5f, NkRole::StatusOk);
			pan.OuvrirFenetre(2);
			const NkAiImageResultat r2 = image("Build/panneau_ia/kit_695_carte.png");
			pan.OuvrirFenetre(0);
			Essai(b, "24o", r.ok && r2.ok && pieces && ferme,
				  "Utilisation (barre de memoire) et Carte des agents : peintes, et la croix les ferme");
		}
		// ── Q8 : SELECTIONNER ET COPIER ───────────────────────────────────────
		// Le presse-papiers du banc : ce que le panneau ECRIT, relu tel quel.
		static NkString sPressePapiers;
		ctx.clipboardSetFn = [](void *, const char *t) { sPressePapiers = NkString(t ? t : ""); };
		ctx.clipboardGetFn = [](void *, NkString &o) { o = sPressePapiers; };
		auto trame = [&](float32 x, float32 y, bool appui, bool clicNeuf, bool copier, bool toutSel,
						 const char *chemin) {
			ctx.BeginFrame(1.f / 60.f);
			ctx.input.mousePos = {x, y};
			ctx.input.mouseDown[0] = appui;
			ctx.input.mouseClicked[0] = clicNeuf;
			ctx.input.wantCopy = copier;
			ctx.input.wantSelectAll = toutSel;
			NkGuiComponentPaint pc(ctx, theme);
			pc.PoserPolices(nullptr, &mono, &corps);
			const NkAiSorties s = pan.Dessiner(ctx, pc, {0.f, 0.f, (float32)W, (float32)H}, true);
			if (chemin) {
				const nkgui::NkGuiFont *pol[3] = {&texte, &mono, &corps};
				const NkAiImageResultat r = NkAiEcrireImage(ctx.dl, W, H, 0.f, 0.f, (float32)W, (float32)H, pol, 3,
															theme.Get(NkRole::PanelBg), chemin);
				printf("         %s\n", r.message);
			}
			ctx.input.mouseClicked[0] = false;
			ctx.input.mouseDown[0] = false;
			ctx.EndFrame();
			return s;
		};
		// deux morceaux de texte VISIBLES de deux blocs differents
		(void)image(nullptr);
		float32 ax = 0.f, ay = 0.f, bx = 0.f, by = 0.f;
		uint32 blocA = 0u;
		bool aOk = false, bOk = false;
		for (uint32 i = 0; i < pan.planFil.Pieces(); ++i) {
			const NkAiRectPublie &q = pan.planFil.Piece(i);
			if (q.piece != NkAiPiece::Fragment)
				continue;
			const float32 sx = q.x + pan.filOx, sy = q.y + pan.filOy;
			if (!pan.vueFil.Contains(sx + 2.f, sy + q.h * 0.5f) || !pan.vueFil.Contains(sx + 2.f, sy + q.h - 1.f))
				continue;
			if (!aOk) {
				ax = sx + 2.f;
				ay = sy + q.h * 0.5f;
				blocA = q.blocId;
				aOk = true;
			} else if (q.blocId != blocA) {
				bx = sx + q.w - 2.f;
				by = sy + q.h * 0.5f;
				bOk = true;
			}
		}
		// 24p — LE GLISSER SELECTIONNE SUR PLUSIEURS BLOCS, et Ctrl+C le copie.
		{
			sPressePapiers = NkString();
			(void)trame(ax, ay, true, true, false, false, nullptr);
			(void)trame((ax + bx) * 0.5f, (ay + by) * 0.5f, true, false, false, false, nullptr);
			(void)trame(bx, by, true, false, false, false, nullptr);
			(void)trame(bx, by, false, false, false, false, "Build/panneau_ia/kit_695_selection.png");
			const uint32 surl = pan.planFil.Compter(NkAiPiece::Surlignage);
			const NkAiSorties s = trame(bx, by, false, false, true, false, nullptr);
			printf("         selection : %u morceau(x) surligne(s), %u octet(s) copies\n", (unsigned)surl,
				   (unsigned)sPressePapiers.Length());
			Essai(b, "24p", aOk && bOk && surl >= 2u && s.copie && sPressePapiers.Length() > 10u &&
								  sPressePapiers == pan.TexteSelectionne(),
				  "glisser sur deux blocs : surlignage, et Ctrl+C met CE texte au presse-papiers");
		}
		// 24q — Ctrl+A SELECTIONNE TOUT LE FIL (le fil a la main apres le glisser).
		{
			sPressePapiers = NkString();
			(void)trame(bx, by, false, false, false, true, nullptr);
			(void)trame(bx, by, false, false, true, false, nullptr);
			const bool tout = sPressePapiers.Find("durcir", 0) != NkString::npos &&
							  sPressePapiers.Find("NK_SCULPT_OP_NORMAL", 0) != NkString::npos;
			Essai(b, "24q", tout, "Ctrl+A puis Ctrl+C : le fil ENTIER, du premier bloc au dernier");
		}
		// 24r — LE BOUTON « COPIER » d'un bloc, au survol.
		{
			sPressePapiers = NkString();
			pan.EffacerSelection();
			(void)trame(ax, ay, false, false, false, false, "Build/panneau_ia/kit_695_copier.png");
			NkAiRectPublie bc;
			bool vu = pan.planFil.Trouver(blocA, NkAiPiece::BoutonCopier, bc);
			NkAiSorties s;
			if (vu)
				s = trame(bc.x + pan.filOx + 5.f, bc.y + pan.filOy + 5.f, true, true, false, false, nullptr);
			uint32 idx = 0;
			const bool attendu = pan.Fil().TrouverParId(blocA, idx) &&
								 sPressePapiers.Find(pan.Fil().At(idx).texte.Length() ? pan.Fil().At(idx).texte.CStr()
																						: pan.Fil().At(idx).titre.CStr(),
													 0) != NkString::npos;
			Essai(b, "24r", vu && s.copie && attendu, "au survol, le bloc porte « copier », et il copie CE bloc");
		}
		// 24s — LE COMPOSEUR : Ctrl+A, Ctrl+C, et la frappe REMPLACE la selection.
		{
			sPressePapiers = NkString();
			pan.PoserSaisie("une demande a copier");
			ctx.inputId = pan.IdComposeur();
			(void)trame(-10.f, -10.f, false, false, false, true, nullptr);
			(void)trame(-10.f, -10.f, false, false, true, false, "Build/panneau_ia/kit_695_composeur_sel.png");
			const bool copie = sPressePapiers == NkString("une demande a copier");
			ctx.BeginFrame(1.f / 60.f);
			ctx.input.PushChar((uint32)'x');
			ctx.input.wantSelectAll = false;
			{
				NkGuiComponentPaint pc(ctx, theme);
				pc.PoserPolices(nullptr, &mono, &corps);
				(void)pan.Dessiner(ctx, pc, {0.f, 0.f, (float32)W, (float32)H}, true);
			}
			ctx.EndFrame();
			const bool remplace = NkString(pan.Saisie()) == NkString("x");
			ctx.inputId = nkgui::NKGUI_ID_NONE;
			// la demande longue de la capture revient : 24c en a besoin (le composeur
			// haut fait deborder le fil a 276 px -- la condition de l'epinglage).
			pan.PoserSaisie("qu ce soit nk code ou nk3dmodeler ou nkuidesign ou nimprote quel de nos app qui utilise une "
							"ia je veux que le panneau soit exactement comme ceci donc meme design meme emplacement dans "
							"sa pastille meme comportement stp donc redesign totalement tut pour suivre ce panneau en "
							"capture.");
			Essai(b, "24s", copie && remplace, "composeur : Ctrl+A + Ctrl+C copie la demande ; une touche la REMPLACE");
		}
		// 24t — (Q8) UNE IMAGE JOINTE : refusee si ce n'est pas une image, vignette dans
		//       le composeur, partie avec la demande, puis vignette DANS la demande.
		{
			const char *kImg = "Build/panneau_ia/kit_image_jointe.png";
			{
				NkImage img;
				if (img.Create(64u, 40u, math::NkColor(), 4) && img.Pixels()) {
					uint8 *px = img.Pixels();
					for (int32 y = 0; y < 40; ++y)
						for (int32 x = 0; x < 64; ++x) {
							uint8 *p = px + y * img.Stride() + x * 4;
							p[0] = (uint8)(x * 4);
							p[1] = (uint8)(y * 6);
							p[2] = (uint8)(x < 32 ? 220 : 40);
							p[3] = 255;
						}
					(void)img.SavePNG(kImg);
				}
			}
			NkString pq;
			pan.accepteImages = true;
			const bool refuse = !pan.JoindreImage("Build/panneau_ia/kit_chats.txt", pq) && pq.Length() > 0;
			const bool joint = pan.JoindreImage(kImg, pq);
			pan.PoserSaisie("fais un ecran comme cette image");
			(void)trame(-10.f, -10.f, false, false, false, false, "Build/panneau_ia/kit_695_image_composeur.png");
			const bool vignette = pan.planChrome.Compter(NkAiPiece::ImageJointe) == 1u &&
								  pan.planChrome.Compter(NkAiPiece::RetirerImage) == 1u;
			NkAiRectPublie env;
			NkAiSorties s;
			if (pan.planChrome.Trouver(0u, NkAiPiece::Envoi, env))
				s = trame(env.x + env.w * 0.5f, env.y + env.h * 0.5f, true, true, false, false, nullptr);
			const bool partie = s.envoyer && s.images.Size() == 1u && pan.ImagesJointes().Size() == 0u;
			// l'HOTE pose la demande, comme les applications
			NkAiBlocDonnees d;
			d.type = NkAiBloc::Demande;
			d.texte = s.texte;
			(void)pan.Fil().Pousser(d, pq);
			(void)trame(-10.f, -10.f, false, false, false, false, nullptr);
			(void)trame(-10.f, -10.f, false, false, false, false, "Build/panneau_ia/kit_695_image_demande.png");
			const NkAiBlocDonnees &der = pan.Fil().At(pan.Fil().Taille() - 1);
			const bool dansDemande = der.images.Size() == 1u && pan.planFil.Compter(NkAiPiece::ImageJointe) >= 1u;
			printf("         image : refus=%d jointe=%d vignette=%d partie=%d dans-la-demande=%d (%s)\n", refuse ? 1 : 0,
				   joint ? 1 : 0, vignette ? 1 : 0, partie ? 1 : 0, dansDemande ? 1 : 0, pq.CStr());
			Essai(b, "24t", refuse && joint && vignette && partie && dansDemande,
				  "image : un non-image refuse avec motif ; vignette au composeur ; elle part et s'affiche dans la demande");
			pan.PoserSaisie("qu ce soit nk code ou nk3dmodeler ou nkuidesign ou nimprote quel de nos app qui utilise une "
							"ia je veux que le panneau soit exactement comme ceci donc meme design meme emplacement dans "
							"sa pastille meme comportement stp donc redesign totalement tut pour suivre ce panneau en "
							"capture.");
		}
		// 24h — LE PANNEAU ETROIT DU MODELEUR (276 px) : rien ne sort.
		{
			ctx.BeginFrame(1.f / 60.f);
			NkGuiComponentPaint pc(ctx, theme);
			pc.PoserPolices(nullptr, &mono, &corps);
			(void)pan.Dessiner(ctx, pc, {0.f, 0.f, 276.f, (float32)H}, false);
			bool dedans = true;
			for (uint32 i = 0; i < pan.planChrome.Pieces(); ++i) {
				const NkAiRectPublie &q = pan.planChrome.Piece(i);
				if (q.piece != NkAiPiece::Filet && (q.x < 0.f || q.x + q.w > 276.5f))
					dedans = false;
			}
			const nkgui::NkGuiFont *pol[3] = {&texte, &mono, &corps};
			const NkAiImageResultat r3 =
				NkAiEcrireImage(ctx.dl, W, H, 0.f, 0.f, 276.f, (float32)H, pol, 3, theme.Get(NkRole::PanelBg),
								"Build/panneau_ia/kit_276.png");
			ctx.EndFrame();
			printf("         %s\n", r3.message);
			Essai(b, "24h", dedans && r3.ok, "276 px : aucune piece du chrome ne sort du panneau");
			// 24c — LA DEMANDE EST EPINGLEE : a 276 px le fil deborde et a defile,
			//       la question du tour reste en tete (la capture).
			printf("         epingle=%u premier=%u (%s) hauteur=%.0f vue=%.0f\n", (unsigned)pan.Epingle(),
				   (unsigned)pan.Fil().At(0).id, NkAiBlocNom(pan.Fil().At(0).type), (double)pan.planFil.Hauteur(),
				   (double)pan.vueFil.h);
			Essai(b, "24c", pan.Epingle() == pan.Fil().At(0).id,
				  "276 px, fil defile : la demande du tour est epinglee en tete");
		}
		return b;
	}

} // namespace aipanneauprobe
