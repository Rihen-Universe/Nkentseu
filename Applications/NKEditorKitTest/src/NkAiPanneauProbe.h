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
		static nkgui::NkGuiFont texte, mono;
		if (!init) {
			init = true;
			(void)ctx.Init(W, H);
			(void)texte.LoadEmbedded(NkEmbeddedFontId::Inter, 13.f);
			(void)mono.LoadEmbedded(NkEmbeddedFontId::Cousine, 12.f, false);
			mono.texId = texte.TexId() + 3u;
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
			pc.PoserPolices(nullptr, &mono);
			(void)pan.Dessiner(ctx, pc, {0.f, 0.f, (float32)W, (float32)H}, false);
			const nkgui::NkGuiFont *pol[2] = {&texte, &mono};
			const NkAiImageResultat r = NkAiEcrireImage(ctx.dl, W, H, 0.f, 0.f, (float32)W, (float32)H, pol, 2,
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
		{
			pan.occupe = false;
			pan.OuvrirMenu(NkAiMenu::Fournisseurs);
			const NkAiImageResultat r2 = image("Build/panneau_ia/kit_695_menu.png");
			const uint32 lignes = pan.planChrome.Compter(NkAiPiece::MenuLigne);
			pan.OuvrirMenu(NkAiMenu::Modeles, 0);
			(void)image("Build/panneau_ia/kit_695_modeles.png");
			const uint32 lignes2 = pan.planChrome.Compter(NkAiPiece::MenuLigne);
			pan.FermerMenus();
			Essai(b, "24g", r2.ok && lignes == 4u && lignes2 == 3u,
				  "fournisseurs : 3 + « Ajouter une IA… » ; modeles de Claude : retour + 2");
		}
		// 24h — LE PANNEAU ETROIT DU MODELEUR (276 px) : rien ne sort.
		{
			ctx.BeginFrame(1.f / 60.f);
			NkGuiComponentPaint pc(ctx, theme);
			pc.PoserPolices(nullptr, &mono);
			(void)pan.Dessiner(ctx, pc, {0.f, 0.f, 276.f, (float32)H}, false);
			bool dedans = true;
			for (uint32 i = 0; i < pan.planChrome.Pieces(); ++i) {
				const NkAiRectPublie &q = pan.planChrome.Piece(i);
				if (q.piece != NkAiPiece::Filet && (q.x < 0.f || q.x + q.w > 276.5f))
					dedans = false;
			}
			const nkgui::NkGuiFont *pol[2] = {&texte, &mono};
			const NkAiImageResultat r3 =
				NkAiEcrireImage(ctx.dl, W, H, 0.f, 0.f, 276.f, (float32)H, pol, 2, theme.Get(NkRole::PanelBg),
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
