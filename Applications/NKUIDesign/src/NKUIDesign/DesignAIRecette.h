#pragma once
// -----------------------------------------------------------------------------
// @File    DesignAIRecette.h
// @Brief   LA PREUVE DE RECETTE du pipeline IA — chaque maillon MESURE.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUE CE FICHIER PROUVE, ET POURQUOI IL EXISTE A PART
// =============================================================================
//  La recette convenue le 30/08 tient en une phrase : *une requete « un ecran de
//  connexion : un titre, deux champs avec libelles, un bouton » passe par le
//  BACKEND FICHIER, la declaration est validee, posee dans le document, visible
//  par introspection, le badge IA est en metadonnee, une annulation la retire
//  ENTIEREMENT, une edition manuelle retire le badge.*
//
//  La sonde (`Probe.h`, essais 28-30) exerce deja la porte avec le backend EN
//  CONSERVE. Ce qu'elle ne prouve PAS, et que ce fichier prouve :
//    1. le backend FICHIER avec de VRAIS fichiers — le prompt atterrit sur le
//       disque, la reponse en revient. C'est le contrat que `ia_pont.py` et
//       n'importe quel modele (Ilyana comprise, plus tard) devront honorer ;
//    2. la recette COMPLETE sur le contenu convenu (l'ecran de connexion, le
//       meme que la planche 22.0 : le jour ou la toile sait dessiner, l'IA et
//       la main produisent la meme chose) ;
//    3. l'APERCU (`Propose`/`CommitProposal`/`DiscardProposal`) : le document
//       n'est touche qu'au Commit, octet pour octet ;
//    4. le RETRAIT (`Retract`) : la greffe part ENTIEREMENT, et le document
//       redevient identique octet pour octet a ce qu'il etait avant.
//
//  ⚠️ CE QUE LA RECETTE NE PROUVE PAS, ET IL FAUT LE DIRE AVEC ELLE : le
//     registre vivant n'a aujourd'hui AUCUN composant de widget (ni bouton, ni
//     champ — seulement `content_browser` et `tree_view`, poses par les
//     panneaux). L'ecran de connexion est donc exprime en CADRES LIBELLES,
//     ce que la porte accepte (`CanGraft` accepte les cadres). La recette
//     prouve la STRUCTURE et le pipeline, pas une palette de widgets qui
//     n'existe pas encore. Le jour ou `bouton`/`champ` seront declares, la
//     reponse de reference changera de composants, pas le pipeline.
//
//  ⚠️ FICHIERS : la recette ecrit `recette_ia_prompt.txt` et
//     `recette_ia_reponse.txt` dans le repertoire courant, et les EFFACE en
//     sortant. Des chemins A ELLE, pas ceux du panneau (`nkuidesign_*.txt`) :
//     une recette qui consommerait la reponse qu'un humain est en train de
//     preparer pour le panneau serait une course, pas une mesure.
//
// OU L'APPELER : `main.cpp`, a cote de `--probe` —
//     if (NkComponentDecl::StrEq(a, "--recette-ia"))
//         return nkuidesign::RunRecetteIA();
//  (branchement volontairement NON fait ici : `main.cpp` est le territoire du
//   chantier toile au 30/08 ; le point exact est donne dans le canal.)
// -----------------------------------------------------------------------------

#include "DesignAI.h"
#include "NKEditorKit/NkAiThreadLayout.h" // le fil du panneau, et son PLAN

#include <cstdio>

namespace nkuidesign {

	/// La mesure de texte de la recette : deterministe, sans fonte.
	inline nkentseu::float32 RcMesureTexte(void *, nkentseu::editorkit::NkAiPolice,
				   const char *t) {
		nkentseu::uint32 n = 0;
		while (t && t[n]) ++n;
		return (nkentseu::float32)n * 7.f;
	}


	// ── HELPERS LOCAUX ──────────────────────────────────────────────────────
	// Prefixes `Rc` pour ne pas entrer en collision avec ceux de `Probe.h` si
	// les deux en-tetes se retrouvent dans la meme unite de compilation.

	inline bool RcSameText(const char *a, const char *b) {
		if (!a || !b)
			return a == b;
		while (*a && *b && *a == *b) {
			++a;
			++b;
		}
		return *a == 0 && *b == 0;
	}

	inline bool RcContains(const char *hay, const char *needle) {
		if (!hay || !needle || !*needle)
			return false;
		for (; *hay; ++hay) {
			const char *h = hay;
			const char *n = needle;
			while (*h && *n && *h == *n) {
				++h;
				++n;
			}
			if (!*n)
				return true;
		}
		return false;
	}

	/// Le document de depart de la recette : une racine en colonne, un noeud
	/// pose a la main. Le MINIMUM pour que « le document n'a pas bouge » ait
	/// quelque chose a comparer — un document vide rendrait ce controle a vide.
	inline void RecetteBuildBase(NkUIDocument &doc) {
		doc.NewDocument("Recette IA", NkAuthor::Humain);
		doc.nodes[0].layout.kind = NkLayoutKind::Column;
		const int32 pose = doc.AddChild(0, "", NkAuthor::Humain);
		doc.nodes[(uint32)pose].label = NkString("Posé à la main");
		doc.nodes[(uint32)pose].height.mode = NkSizeMode::Fixed;
		doc.nodes[(uint32)pose].height.value = 40.f;
	}

	/// LA REPONSE DE REFERENCE — l'ecran de connexion en cadres libelles.
	/// Sept noeuds : le bloc, le titre, deux libelles, deux champs, le bouton.
	/// Aucune coordonnee (la regle du prompt l'interdit) : tailles et
	/// agencement seulement, la position se CALCULE.
	inline const char *RecetteReponseConnexion() {
		return "Voici l'écran demandé :\n"
			   "nkuidoc 1\n"
			   "titre = Écran de connexion\n"
			   "noeud 0\n"
			   "  libelle = Connexion\n"
			   "  composant = \n"
			   "  enfants = 1 2 3 4 5 6\n"
			   "  largeur = expand 0 0 0\n"
			   "  hauteur = expand 0 0 0\n"
			   "  agencement = column\n"
			   "  ecart = 8\n"
			   "  marge = 16\n"
			   "noeud 1\n"
			   "  libelle = Titre : Connexion\n"
			   "  composant = \n"
			   "  enfants =\n"
			   "  largeur = expand 0 0 0\n"
			   "  hauteur = fixed 32 0 0\n"
			   "noeud 2\n"
			   "  libelle = Libellé : Nom d'utilisateur\n"
			   "  composant = \n"
			   "  enfants =\n"
			   "  largeur = expand 0 0 0\n"
			   "  hauteur = fixed 20 0 0\n"
			   "noeud 3\n"
			   "  libelle = Champ : Nom d'utilisateur\n"
			   "  composant = \n"
			   "  enfants =\n"
			   "  largeur = expand 0 0 0\n"
			   "  hauteur = fixed 28 0 0\n"
			   "noeud 4\n"
			   "  libelle = Libellé : Mot de passe\n"
			   "  composant = \n"
			   "  enfants =\n"
			   "  largeur = expand 0 0 0\n"
			   "  hauteur = fixed 20 0 0\n"
			   "noeud 5\n"
			   "  libelle = Champ : Mot de passe\n"
			   "  composant = \n"
			   "  enfants =\n"
			   "  largeur = expand 0 0 0\n"
			   "  hauteur = fixed 28 0 0\n"
			   "noeud 6\n"
			   "  libelle = Bouton : Se connecter\n"
			   "  composant = \n"
			   "  enfants =\n"
			   "  largeur = fixed 160 0 0\n"
			   "  hauteur = fixed 32 0 0\n";
	}

	/// Le nombre de noeuds greffes attendus — tenu a cote de la reponse pour
	/// que l'un ne bouge pas sans l'autre.
	inline uint32 RecetteNoeudsAttendus() {
		return 7u;
	}

	// ── LA RECETTE ──────────────────────────────────────────────────────────
	inline int RunRecetteIA() {
		NkString rep;
		int pass = 0, total = 0;
		auto check = [&](const char *label, bool ok, const char *detail) {
			++total;
			if (ok)
				++pass;
			rep.Append(ok ? "  [ok]   " : "  [ECHEC] ");
			rep.Append(label);
			if (detail && *detail) {
				rep.Append("  -- ");
				rep.Append(detail);
			}
			rep.Append('\n');
		};
		char buf[320];

		rep.Append("=== NKUIDesign --recette-ia : le pipeline IA, maillon par maillon ===\n");
		rep.Append("binaire compile le " __DATE__ " a " __TIME__ "\n");
		rep.Append("Backend FICHIER réel (recette_ia_prompt.txt / recette_ia_reponse.txt),\n");
		rep.Append("requête de référence : un écran de connexion — un titre, deux champs\n");
		rep.Append("avec libellés, un bouton. Composants : CADRES libellés (le registre n'a\n");
		rep.Append("pas encore de widgets — la recette prouve le pipeline, pas la palette).\n\n");

		const char *kPrompt = "recette_ia_prompt.txt";
		const char *kReponse = "recette_ia_reponse.txt";
		const char *kAsk = "un écran de connexion : un titre, deux champs avec libellés, un bouton";
		using nkentseu::NkFile;
		NkFile::Delete(kPrompt);
		NkFile::Delete(kReponse);

		NkUIDocument doc;
		RecetteBuildBase(doc);
		NkString avant;
		doc.Save(avant);

		// ── 0. CONTROLE POSITIF DU COMPARATEUR ──────────────────────────────
		// Sans lui, tous les « identique octet pour octet » d'en dessous
		// passeraient aussi avec un comparateur toujours-vrai.
		check("0. CONTRÔLE DU COMPARATEUR : identique = vrai, différent = faux",
			  RcSameText(avant.Data(), avant.Data()) && !RcSameText("a", "b") &&
				  !RcSameText("a", "ab") && RcContains("un titre pose", "titre") &&
				  !RcContains("un titre", "bouton"),
			  "");

		NkFileBackend fichier;
		fichier.promptPath = NkString(kPrompt);
		fichier.replyPath = NkString(kReponse);
		NkDesignAI ai;
		ai.SetBackend(&fichier);

		// ── 1. LE PROMPT ATTERRIT SUR LE DISQUE ─────────────────────────────
		// Premier appel SANS fichier de reponse : le backend doit ecrire le
		// prompt, dire ou coller la reponse, et le document ne doit pas bouger.
		const NkAIResult sansReponse = ai.Ask(kAsk, doc, 0);
		const NkString promptEcrit =
			NkFile::Exists(kPrompt) ? NkFile::ReadAllText(kPrompt) : NkString("");
		snprintf(buf, sizeof(buf), "verdict=%s, prompt de %u caractères",
				 NkAIVerdictName(sansReponse.verdict), (uint32)promptEcrit.Length());
		check("1. sans réponse, le verdict est « backend muet » et le prompt est ÉCRIT",
			  sansReponse.verdict == NkAIVerdict::BackendMuet && promptEcrit.Length() > 0, buf);
		check("1b. le prompt écrit porte la demande ET le format attendu",
			  RcContains(promptEcrit.Data(), kAsk) && RcContains(promptEcrit.Data(), "nkuidoc"),
			  "");
		NkString apresMuet;
		doc.Save(apresMuet);
		check("1c. et le document n'a pas bougé (octet pour octet)",
			  RcSameText(avant.Data(), apresMuet.Data()), "");

		// ── 2. LA REPONSE REVIENT PAR LE FICHIER, ET ELLE EST POSEE ─────────
		// C'est le tour deterministe : ce que ferait un modele (ou `ia_pont.py`,
		// ou une main) — poser la reponse dans le fichier convenu.
		NkFile::WriteAllText(kReponse, RecetteReponseConnexion());
		const NkAIResult pose = ai.Ask(kAsk, doc, 0);
		snprintf(buf, sizeof(buf), "verdict=%s, +%u noeud(s), %u divergence(s) au rejeu",
				 NkAIVerdictName(pose.verdict), pose.nodesAdded, pose.replayDiffs);
		check("2. la réponse du FICHIER est validée, rejouée et POSÉE dans le document",
			  pose.Accepted() && pose.nodesAdded == RecetteNoeudsAttendus() &&
				  doc.IsValidIndex(pose.graftedRoot),
			  buf);

		// ── 2b. LE VOCABULAIRE A REELLEMENT ETE LU, pas seulement tolere ────
		// Mesure du 30/08 : une premiere version de la reponse ecrivait
		// « colonne » et « fixe » (en francais) — le document se POSAIT quand
		// meme, rejeu compris, mais l'agencement restait `none` et les enfants
		// n'avaient aucun rectangle. Le lecteur du format IGNORE une valeur
		// qu'il ne connait pas au lieu de refuser. Tant que c'est ainsi, ce
		// controle est le seul temoin : il verifie que l'agencement et les
		// tailles ont ete RELUS dans le document, pas seulement acceptes.
		// (Le vocabulaire reel est anglais — `column`, `fixed`, `expand` — et
		//  le prompt de `BuildPrompt` l'engendre des bonnes fonctions de nom.)
		{
			bool kindLu = false, tailleLue = false;
			if (pose.Accepted()) {
				const NkUINode &g = doc.nodes[(uint32)pose.graftedRoot];
				kindLu = g.layout.kind == NkLayoutKind::Column;
				const NkVector<int32> &kids = g.children;
				for (uint32 i = 0; i < (uint32)kids.Size(); ++i) {
					const NkUINode &c = doc.nodes[(uint32)kids[i]];
					if (RcContains(c.label.Data(), "Titre"))
						tailleLue = c.height.mode == NkSizeMode::Fixed && c.height.value == 32.f;
				}
			}
			check("2b. l'agencement (column) et les tailles (fixed 32) ont été RELUS — un "
				  "vocabulaire ignoré en silence ne passe pas ce témoin",
				  kindLu && tailleLue, "");
		}

		// ── 3. VISIBLE PAR INTROSPECTION : l'arbre porte l'ecran ────────────
		// La Hierarchie lit `nodes[].label` et `children` — exactement ce qu'on
		// lit ici. Pas de toile requise : le releve d'introspection suffit,
		// comme convenu.
		bool titreVu = false, champ1Vu = false, champ2Vu = false, boutonVu = false;
		uint32 enfantsDirects = 0;
		if (pose.Accepted()) {
			const NkVector<int32> &kids = doc.nodes[(uint32)pose.graftedRoot].children;
			enfantsDirects = (uint32)kids.Size();
			for (uint32 i = 0; i < enfantsDirects; ++i) {
				const char *l = doc.nodes[(uint32)kids[i]].label.Data();
				if (RcContains(l, "Titre"))
					titreVu = true;
				else if (RcContains(l, "Champ : Nom"))
					champ1Vu = true;
				else if (RcContains(l, "Champ : Mot"))
					champ2Vu = true;
				else if (RcContains(l, "Bouton"))
					boutonVu = true;
			}
		}
		snprintf(buf, sizeof(buf), "%u enfant(s) directs sous la greffe", enfantsDirects);
		check("3. l'introspection retrouve le titre, les DEUX champs et le bouton",
			  titreVu && champ1Vu && champ2Vu && boutonVu && enfantsDirects == 6, buf);

		// 3b. Et la mise en page les place TOUS : un noeud sans rectangle serait
		//     un noeud que la toile ne dessinera jamais.
		{
			NkLayoutResult lay;
			NkComputeLayout(doc, ai.replaySurface, lay);
			uint32 places = 0;
			for (uint32 i = 0; i < doc.NodeCount(); ++i)
				if (lay.Has((int32)i))
					++places;
			snprintf(buf, sizeof(buf), "%u noeud(s) sur %u ont un rectangle", places,
					 doc.NodeCount());
			check("3b. la mise en page donne un rectangle à CHAQUE noeud, greffe comprise",
				  places == doc.NodeCount(), buf);
		}

		// ── 4. LE BADGE EST EN METADONNEE, ET SEULEMENT LA ──────────────────
		const uint32 ia = doc.CountByAuthor(NkAuthor::IA);
		const bool racineHumaine = doc.nodes[0].prov.author == NkAuthor::Humain;
		const bool rejouee =
			pose.Accepted() && doc.nodes[(uint32)pose.graftedRoot].prov.verified;
		snprintf(buf, sizeof(buf), "%u noeud(s) d'origine IA, racine %s", ia,
				 racineHumaine ? "humaine" : "PAS humaine");
		check("4. le badge (auteur = ia + rejouée) couvre la greffe et RIEN d'autre",
			  ia == RecetteNoeudsAttendus() && racineHumaine && rejouee, buf);

		// 4b. Il SURVIT a l'aller-retour fichier — sinon le corpus le perd.
		{
			NkString texte;
			doc.Save(texte);
			NkUIDocument relu;
			const bool ok = relu.Load(texte.Data()) &&
							relu.CountByAuthor(NkAuthor::IA) == RecetteNoeudsAttendus();
			check("4b. la provenance survit à l'aller-retour par le texte", ok, "");
		}

		// ── 5. L'ANNULATION D'UN GESTE RETIRE TOUT ──────────────────────────
		const bool retire = NkDesignAI::Retract(doc, pose);
		NkString apresRetrait;
		doc.Save(apresRetrait);
		snprintf(buf, sizeof(buf), "retrait=%d, %u noeud(s) restants", retire ? 1 : 0,
				 doc.NodeCount());
		check("5. Retract retire la greffe ENTIÈRE et le document redevient IDENTIQUE "
			  "octet pour octet",
			  retire && RcSameText(avant.Data(), apresRetrait.Data()), buf);

		// 5b. Un second retrait avec le MEME resultat (index perime) est REFUSE
		//     et ne touche a rien : la garde d'auteur/taille fait son travail.
		{
			const bool rejoue = NkDesignAI::Retract(doc, pose);
			NkString apres2;
			doc.Save(apres2);
			check("5b. un retrait REJOUÉ (index périmé) est refusé, document intact",
				  !rejoue && RcSameText(avant.Data(), apres2.Data()), "");
		}

		// ── 6. L'EDITION MANUELLE RETIRE LE BADGE ───────────────────────────
		// On repose l'ecran (la reponse est toujours dans le fichier), puis on
		// touche UN noeud a la main : `corrected` monte, `verified` tombe. La
		// donnee `auteur = ia` RESTE — c'est elle qui fabrique le corpus — et
		// l'affichage du badge suit `author == IA && !corrected` (le point
		// d'integration est donne dans le canal).
		const NkAIResult pose2 = ai.Ask(kAsk, doc, 0);
		bool badgeRetire = false;
		if (pose2.Accepted()) {
			const NkVector<int32> &kids = doc.nodes[(uint32)pose2.graftedRoot].children;
			if (kids.Size() > 0) {
				const int32 touche = kids[0];
				doc.MarkHumanEdit(touche);
				const NkProvenance &p = doc.nodes[(uint32)touche].prov;
				badgeRetire = p.corrected && !p.verified && p.author == NkAuthor::IA;
			}
		}
		check("6. une édition manuelle pose « corrigé » et retire « rejouée » — le badge "
			  "s'éteint, la provenance reste pour le corpus",
			  pose2.Accepted() && badgeRetire, "");

		// ── 7. L'APERCU : rien n'est ecrit avant le Commit ──────────────────
		// La garantie 1 de la spec §6.2, mesuree octet pour octet.
		{
			NkString base;
			doc.Save(base);
			const NkAIResult prop = ai.Propose(kAsk, doc);
			NkString apresPropose;
			doc.Save(apresPropose);
			snprintf(buf, sizeof(buf), "verdict=%s, proposition de %u noeud(s)",
					 NkAIVerdictName(prop.verdict),
					 ai.HasProposal() ? ai.Proposal().NodeCount() : 0u);
			check("7. Propose valide et rejoue SANS toucher au document (octet pour octet)",
				  prop.Accepted() && ai.HasProposal() &&
					  ai.Proposal().NodeCount() == RecetteNoeudsAttendus() &&
					  RcSameText(base.Data(), apresPropose.Data()),
				  buf);

			// 7b. Jeter ne touche a rien non plus.
			ai.DiscardProposal();
			NkString apresJet;
			doc.Save(apresJet);
			check("7b. Discard jette la proposition, document intact",
				  !ai.HasProposal() && RcSameText(base.Data(), apresJet.Data()), "");

			// 7c. Committer sans proposition est un refus MOTIVE.
			const NkAIResult sansProp = ai.CommitProposal(doc, 0);
			check("7c. Commit sans proposition est refusé pour une raison dite",
				  !sansProp.Accepted() && sansProp.detail.Length() > 0, "");

			// 7d. Proposer puis committer POSE — par la meme porte.
			const NkAIResult prop2 = ai.Propose(kAsk, doc);
			const NkAIResult commit = prop2.Accepted() ? ai.CommitProposal(doc, 0) : NkAIResult();
			snprintf(buf, sizeof(buf), "+%u noeud(s) au Commit", commit.nodesAdded);
			check("7d. Propose puis Commit pose la greffe, badge compris",
				  commit.Accepted() && commit.nodesAdded == RecetteNoeudsAttendus() &&
					  doc.nodes[(uint32)commit.graftedRoot].prov.author == NkAuthor::IA &&
					  !ai.HasProposal(),
				  buf);

			// 7e. Et ce Commit s'annule comme un Ask : meme geste, meme preuve.
			const bool retrait2 = NkDesignAI::Retract(doc, commit);
			NkString apresRetrait2;
			doc.Save(apresRetrait2);
			check("7e. le Commit se retire d'un geste, document identique à l'avant-Propose",
				  retrait2 && RcSameText(base.Data(), apresRetrait2.Data()), "");
		}

		// ── 8. LA REPONSE EST-ELLE AFFICHEE ? ─────────────────────
		// ⚠️ CE QUI MANQUAIT A CETTE RECETTE, ET QUE PERSONNE N AVAIT REMARQUE :
		//    elle prouvait que la GENERATION marche -- 7 noeuds poses, annulables,
		//    document identique au bit -- et RIEN sur ce que l utilisateur VOIT.
		//    Or le panneau n avait qu une phrase ecrasee a chaque tour ; depuis le
		//    20/09 il a un fil. Un document juste dans un panneau muet se lit comme
		//    un outil casse.
		//
		//    On mesure donc le PLAN -- ce que le peintre PUBLIE -- et pas un etat
		//    interne du fil : c est la difference entre « le texte est range » et
		//    « le texte sera peint ».
		{
			using namespace nkentseu::editorkit;
			NkAiFil fil;
			NkAiCapacites cap = NkAiCapacites::Texte();
			cap.produitRefus = true;
			fil.Declarer(cap);
			fil.PoserPlafond(200);
			NkString pourquoi;
			
			// Ce que le panneau pousse a chaque geste, par la meme porte que lui.
			NkAiBlocDonnees d;
			d.type = NkAiBloc::Prose;
			d.texte = NkString("Generation du document lancee.");
			const bool e1 = fil.Pousser(d, pourquoi);
			check("8a. la reponse entre dans le fil du panneau", e1 && fil.Taille() == 1, "");
			
			// ⚠️ LE MAILLON QUI MANQUAIT : est-elle PUBLIEE par le peintre ?
			NkAiMetriques metr;
			NkAiPlan plan;
			NkAiFilMesurer(fil, 320.f, metr, RcMesureTexte, nullptr, plan);
			NkAiRectPublie rc;
			const uint32 id = fil.Taille() ? fil.At(0).id : 0u;
			const bool vu = plan.Trouver(id, NkAiPiece::Texte, rc);
			snprintf(buf, sizeof(buf), "publiee a (%.0f,%.0f) sur %.0f px", (double)rc.x,
				  (double)rc.y, (double)rc.w);
			check("8b. et le peintre la PUBLIE -- ce n est pas qu un etat range",
				  vu && rc.w > 0.f && rc.h > 0.f, buf);
			
			// 8c. ⚠️ CONTROLE NEGATIF : le fil REFUSE un bloc que le porteur ne
			//     declare pas produire, et il le dit. Sans lui, un fil qui accepterait
			//     tout passerait 8a et 8b sans rien prouver.
			// ⚠️ 21/09 : NKUIDesign DECLARE desormais l'outil (une generation : la
			//    demande en entree, la reponse du modele en sortie). Le controle
			//    negatif prend donc un type qu'il NE produit toujours pas : la
			//    reflexion -- aucun de ses dorsaux n'en emet.
			NkAiBlocDonnees o;
			o.type = NkAiBloc::Reflexion;
			o.titre = NkString("Thinking");
			o.texte = NkString("...");
			NkString motif;
			const bool refuse = !fil.Pousser(o, motif);
			check("8c. controle negatif : un bloc non declare est refuse AVEC son motif",
				  refuse && motif.Length() > 0 && fil.Taille() == 1, "");
		}

		// ── FIN : on ne laisse rien trainer ─────────────────────────────────
		NkFile::Delete(kPrompt);
		NkFile::Delete(kReponse);

		snprintf(buf, sizeof(buf), "\n=== RÉSULTAT : %d / %d ===\n", pass, total);
		rep.Append(buf);

		fputs(rep.Data(), stdout);
		fflush(stdout);
		// Meme raison que la sonde : une application `windowedapp` sur Windows
		// n'a pas toujours de console attachee.
		NkFile::WriteAllText("nkuidesign_recette_ia.txt", rep.Data());
		return (pass == total) ? 0 : 1;
	}

} // namespace nkuidesign
