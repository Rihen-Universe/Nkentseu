#pragma once
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

		// 24d — UNE CONVERSATION PAR ASSISTANT. Changer ramene SA conversation.
		{
			NkString pq;
			pan.occupe = false;
			const uint32 avant = pan.Fil().Taille();
			const bool c1 = pan.Choisir(1, pq);
			const bool vide = pan.Fil().Taille() == 0;
			NkAiBlocDonnees d;
			d.type = NkAiBloc::Demande;
			d.texte = NkString("subdivise le cube");
			(void)pan.Fil().Pousser(d, pq);
			const bool c0 = pan.Choisir(0, pq);
			const bool retrouve = pan.Fil().Taille() == avant;
			const bool c1b = pan.Choisir(1, pq);
			const bool sienne = pan.Fil().Taille() == 1 && pan.Fil().At(0).texte == NkString("subdivise le cube");
			(void)pan.Choisir(0, pq);
			Essai(b, "24d", c1 && vide && c0 && retrouve && c1b && sienne,
				  "Claude -> Local : fil vide ; retour : les 10 blocs de Claude ; Local garde SA demande");
		}
		// 24e — ON NE CHANGE PAS PENDANT UN TOUR, et le refus est NOMME.
		{
			NkString pq;
			pan.occupe = true;
			const bool refuse = !pan.Choisir(1, pq) && pq.Length() > 0 && pan.Actif() == 0;
			pan.occupe = false;
			Essai(b, "24e", refuse, "pendant un tour, changer d'assistant est REFUSE avec son motif");
		}
		// 24f — LE FIL VIVANT D'UN HOTE : `Lier` range et pose, sans copie perdue.
		{
			NkAiPanneau p2;
			Declarer(p2);
			NkAiFil vivant;
			vivant.Declarer(p2.capacites);
			NkString pq;
			NkAiBlocDonnees d;
			d.type = NkAiBloc::Demande;
			d.texte = NkString("chez claude");
			(void)vivant.Pousser(d, pq);
			p2.Lier(&vivant);
			(void)p2.Choisir(1, pq);
			const bool vide = vivant.Taille() == 0;
			d.texte = NkString("chez local");
			(void)vivant.Pousser(d, pq);
			(void)p2.Choisir(0, pq);
			const bool ok = vivant.Taille() == 1 && vivant.At(0).texte == NkString("chez claude") &&
							p2.FilDe(1).Taille() == 1 && p2.FilDe(1).At(0).texte == NkString("chez local");
			Essai(b, "24f", vide && ok, "le fil de l'HOTE suit l'assistant choisi ; l'autre est range, intact");
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
			Essai(b, "24c", pan.Epingle() == pan.Fil().At(0).id,
				  "276 px, fil defile : la demande du tour est epinglee en tete");
		}
		return b;
	}

} // namespace aipanneauprobe
