#pragma once
// -----------------------------------------------------------------------------
// @File    Applications/NKEditorKitTest/src/NkAiPlanProbe.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   FAMILLE 22 — le plan du fil du panneau IA, eprouve SANS FENETRE.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// ⚠️ CE QUE CETTE SONDE FAIT DIFFEREMMENT, ET C'EST TOUT SON INTERET :
//    elle ne recalcule AUCUNE position. Elle interroge le plan PRODUIT PAR LE
//    PEINTRE, par le NOM du bloc et le NOM de la piece. Une sonde qui referait
//    le calcul mesurerait sa propre copie du calcul, et verdirait sur un peintre
//    casse dont elle partage l'erreur.
//
// ⚠️ ET ELLE NE PEUT PAS ETRE « EN AMONT DE LA PASSE ». Le plan n'existe qu'une
//    fois `NkAiFilMesurer` passee ; avant, il n'y a pas de zeros a lire, il n'y
//    a RIEN. La faute payee le 20/09 sur le panneau du modeleur -- un temoin
//    place avant la peinture qui lisait [0..0] et concluait « atteignable »
//    parce que 1571 >= 0 -- n'a pas de place dans cette forme.
//
// LA MESURE DE TEXTE EST DETERMINISTE, ET C'EST ASSUME. On n'eprouve pas la
// fonte : on eprouve la GEOMETRIE -- indentation, repli, troncature, ordre,
// designation. Elle est la meme avec la vraie fonte, et c'est elle qui porte
// tous les defauts qu'on sait nommer.
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkAiThread.h"
#include "NKEditorKit/NkAiThreadLayout.h"
#include "NKEditorKit/NkEditorTiroirMode.h" // le mode du tiroir : voile ou pas
#include <stdio.h>

namespace aiplanprobe {

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

	/// Largeur = nombre de caracteres x un pas par police.
	inline float32 Mesure(void *, NkAiPolice pol, const char *t) {
		if (!t)
			return 0.f;
		uint32 n = 0;
		while (t[n] != '\0')
			++n;
		const float32 pas = (pol == NkAiPolice::Grasse)		  ? 7.6f
							: (pol == NkAiPolice::ChasseFixe) ? 7.2f
															  : 7.0f;
		return (float32)n * pas;
	}

	/// Egalite a 0,01 px pres. ⚠️ Locale : `Pres` existe aussi dans la sonde
	/// de peinture, et partager un nom entre deux sondes rendrait le jour ou
	/// l une change son epsilon invisible dans l autre.
	inline bool PresPlan(float32 a, float32 b) {
		const float32 d = a - b;
		return d < 0.01f && d > -0.01f;
	}

	inline Bilan Sonder() {
		Bilan b;
		const NkAiMetriques M;
		const float32 W = 695.f; // la largeur de la capture de reference

		NkAiCapacites cap = NkAiCapacites::Texte();
		cap.produitOutil = true;
		cap.produitRefus = true;

		// Le fil de travail : une demande, puis une etape d'outil depliee.
		NkAiFil fil;
		fil.Declarer(cap);
		NkString pq;
		NkAiBlocDonnees dem;
		dem.type = NkAiBloc::Demande;
		dem.texte = NkString("noublie pas que on doit aussi brancher claude");
		(void)fil.Pousser(dem, pq);
		const uint32 idDem = fil.At(0).id;

		NkAiBlocDonnees outil;
		outil.type = NkAiBloc::Outil;
		outil.titre = NkString("Bash");
		outil.texte = NkString("Deposer une troisieme variante sans recompiler");
		outil.entree = NkString("cd D:/Projets && md5sum Build/Bin/NKSculptHarness");
		outil.sortie = NkString("depose.");
		outil.replie = false;
		(void)fil.Pousser(outil, pq);
		const uint32 idOutil = fil.At(1).id;

		NkAiPlan plan;
		NkAiFilMesurer(fil, W, M, Mesure, nullptr, plan);

		// 22a — LA DEMANDE N'EST PAS SUR LE RAIL. La capture la met a x=24 et les
		// blocs du fil a x=55 : elle est MOINS indentee, et elle n'a pas de puce.
		{
			NkAiRectPublie cadre, texteOutil;
			const bool a = plan.Trouver(idDem, NkAiPiece::Cadre, cadre);
			const bool c = plan.Trouver(idOutil, NkAiPiece::Titre, texteOutil);
			Essai(b, "22a",
				  a && c && cadre.x < texteOutil.x && !plan.Possede(idDem, NkAiPiece::Puce),
				  "la demande est MOINS indentee que le fil, et n'a pas de puce");
			printf("         demande x=%.0f | bloc du fil x=%.0f\n", (double)cadre.x,
				   (double)texteOutil.x);
		}

		// 22b — LES DEUX COMPARTIMENTS, DANS LE BON ORDRE, AVEC LES ROLES DU THEME.
		// C'est le lien entre le lot d'hier et celui-ci : le plan ne SAIT pas poser
		// une couleur, il ne sait nommer qu'un role -- une valeur en dur y serait
		// impossible a ecrire.
		{
			NkAiRectPublie in, out;
			const bool a = plan.Trouver(idOutil, NkAiPiece::FondIn, in);
			const bool c = plan.Trouver(idOutil, NkAiPiece::FondOut, out);
			Essai(b, "22b",
				  a && c && in.role == NkRole::CodeBg && out.role == NkRole::CodeOutBg &&
					  out.y >= in.y + in.h,
				  "FondIn (code_bg) puis FondOut (code_out_bg), la sortie SOUS l'entree");
		}

		// 22c — UN BLOC REPLIE NE PUBLIE AUCUN COMPARTIMENT. Pas un rectangle de
		// hauteur nulle : RIEN. Un rectangle plat serait quand meme reclame au
		// registre de clics et volerait le survol du bloc voisin.
		{
			NkAiFil f2;
			f2.Declarer(cap);
			NkString p2;
			NkAiBlocDonnees o = outil;
			o.replie = true;
			(void)f2.Pousser(o, p2);
			NkAiPlan pl2;
			NkAiFilMesurer(f2, W, M, Mesure, nullptr, pl2);
			const uint32 id = f2.At(0).id;
			Essai(b, "22c",
				  !pl2.Possede(id, NkAiPiece::FondIn) && !pl2.Possede(id, NkAiPiece::FondOut) &&
					  pl2.Possede(id, NkAiPiece::Titre),
				  "replie : aucun compartiment publie, le titre reste");
		}

		// 22d — L'ESTOMPE EST PUBLIEE SI ET SEULEMENT SI ON A TRONQUE. Une estompe
		// permanente dirait « il y a la suite » sur un compartiment complet.
		{
			NkAiFil court, longue;
			court.Declarer(cap);
			longue.Declarer(cap);
			NkString p3;
			NkAiBlocDonnees a = outil, c = outil;
			a.sortie = NkString("une ligne");
			c.sortie = NkString("l1");
			for (int i = 0; i < 9; ++i)
				c.sortie.Append("\nligne de plus");
			(void)court.Pousser(a, p3);
			(void)longue.Pousser(c, p3);
			NkAiPlan pa, pb;
			NkAiFilMesurer(court, W, M, Mesure, nullptr, pa);
			NkAiFilMesurer(longue, W, M, Mesure, nullptr, pb);
			Essai(b, "22d",
				  !pa.Possede(court.At(0).id, NkAiPiece::Estompe) &&
					  pb.Possede(longue.At(0).id, NkAiPiece::Estompe),
				  "estompe publiee SI ET SEULEMENT SI le compartiment a ete tronque");
		}

		// 22e — AUCUN RECTANGLE NE DEBORDE. Les retraits sont exprimes depuis les
		// bords, donc un panneau etroit doit rester juste.
		{
			bool dedans = true;
			NkAiPlan etroit;
			NkAiFilMesurer(fil, 320.f, M, Mesure, nullptr, etroit);
			for (uint32 i = 0; i < etroit.Pieces(); ++i) {
				const NkAiRectPublie &r = etroit.Piece(i);
				if (r.x < 0.f || r.x + r.w > 320.f)
					dedans = false;
			}
			Essai(b, "22e", dedans && etroit.Pieces() > 0,
				  "panneau de 320 px : aucune piece ne sort des bords");
		}

		// 22f — PANNEAU TROP ETROIT : PLAN VIDE, PAS PLAN FAUX. Des largeurs
		// negatives produiraient des rectangles retournes, que le peintre
		// dessinerait sans broncher.
		{
			NkAiPlan minus;
			NkAiFilMesurer(fil, 40.f, M, Mesure, nullptr, minus);
			Essai(b, "22f", minus.Pieces() == 0 && minus.Hauteur() == 0.f,
				  "panneau de 40 px : plan VIDE plutot que des rectangles retournes");
		}

		// ═══════════════════════════════════════════════════════════════════════
		// 22g / 22h — LA DESIGNATION PAR LE NOM, ET C'EST LE CORRECTIF DU JOUR.
		//
		// J'ai livre la veille `NkAiFil::Basculer(uint32 i)` : un bloc designe par
		// sa POSITION. Or la fenetre glissante du meme fichier decale toutes les
		// positions -- sans que personne n'insere rien, le fil se vide par le haut.
		// Le scenario reel : le peintre releve le bloc clique, une reponse arrive
		// dans l'intervalle, le fil glisse, et c'est un AUTRE bloc qui se deplie.
		// ═══════════════════════════════════════════════════════════════════════
		{
			NkAiFil f;
			f.Declarer(cap);
			f.PoserPlafond(3);
			NkString p;
			for (int i = 0; i < 3; ++i) {
				NkAiBlocDonnees o = outil;
				o.replie = true;
				char t[24];
				snprintf(t, sizeof(t), "etape %d", i);
				o.texte = NkString(t);
				(void)f.Pousser(o, p);
			}
			const uint32 idVise = f.At(2).id; // le DERNIER, celui qu'on veut deplier
			NkAiPlan avant;
			NkAiFilMesurer(f, W, M, Mesure, nullptr, avant);
			NkAiRectPublie cible;
			const bool ok = avant.Trouver(idVise, NkAiPiece::Titre, cible);

			uint32 idClique = 0;
			const bool touche =
				ok && NkAiFilBlocSous(avant, cible.x + 2.f, cible.y + 2.f, idClique);
			Essai(b, "22g", touche && idClique == idVise,
				  "le clic rend l'IDENTIFIANT du bloc vise, pas sa position");

			// Et MAINTENANT la reponse arrive : le fil glisse d'un cran. L'ancien
			// indice 2 designe desormais un autre bloc ; l'identifiant, non.
			NkAiBlocDonnees neuf = outil;
			neuf.replie = true;
			neuf.texte = NkString("etape 3");
			(void)f.Pousser(neuf, p);
			f.BasculerParId(idClique);
			uint32 idx = 0;
			const bool trouve = f.TrouverParId(idVise, idx);
			const bool bonBloc = trouve && !f.At(idx).replie;
			// Et le bloc que l'ANCIEN indice aurait touche reste replie.
			const bool voisinIntact = (f.At(2).id == idVise) || f.At(2).replie;
			Essai(b, "22h", bonBloc && voisinIntact,
				  "apres glissement, l'identifiant deplie le BON bloc -- l'indice aurait rate");
			printf("         bloc vise id=%u, retrouve a l'indice %u apres glissement\n", idVise,
				   idx);
		}

		// 22i — CONTROLE NEGATIF DE 22h. Un identifiant SORTI de la fenetre ne
		// bascule rien -- surtout pas un voisin. Un repli au hasard serait pire
		// qu'un geste sans effet : l'utilisateur croirait avoir vu.
		{
			NkAiFil f;
			f.Declarer(cap);
			f.PoserPlafond(2);
			NkString p;
			NkAiBlocDonnees o = outil;
			o.replie = true;
			(void)f.Pousser(o, p);
			const uint32 idPerdu = f.At(0).id;
			(void)f.Pousser(o, p);
			(void)f.Pousser(o, p); // le premier est evince
			f.BasculerParId(idPerdu);
			const bool rienDeplie = f.At(0).replie && f.At(1).replie;
			uint32 idx = 0;
			Essai(b, "22i", !f.TrouverParId(idPerdu, idx) && rienDeplie,
				  "controle negatif : un id sorti de la fenetre ne bascule AUCUN voisin");
		}

		// 22j — LA MESURE EST PURE : deux appels identiques rendent le meme plan.
		// Sans ca, la sonde mesurerait un etat et le peintre en dessinerait un
		// autre, et aucun des deux ne serait fautif tout seul.
		{
			NkAiPlan a, c;
			NkAiFilMesurer(fil, W, M, Mesure, nullptr, a);
			NkAiFilMesurer(fil, W, M, Mesure, nullptr, c);
			bool pareil = a.Pieces() == c.Pieces() && a.Hauteur() == c.Hauteur();
			for (uint32 i = 0; pareil && i < a.Pieces(); ++i)
				pareil = a.Piece(i).blocId == c.Piece(i).blocId &&
						 a.Piece(i).piece == c.Piece(i).piece && a.Piece(i).x == c.Piece(i).x &&
						 a.Piece(i).y == c.Piece(i).y && a.Piece(i).h == c.Piece(i).h;
			Essai(b, "22j", pareil && a.Pieces() > 0,
				  "deux mesures des memes entrees rendent EXACTEMENT le meme plan");
			printf("         plan de reference : %u pieces, hauteur %.0f px\n", a.Pieces(),
				   (double)a.Hauteur());
		}

		// 22k — LES BLOCS NE SE CHEVAUCHENT PAS. Deux blocs qui se recouvrent se
		// volent leurs clics, et rien a l'ecran ne le dit tant que les fonds sont
		// opaques.
		{
			NkAiRectPublie d, t;
			const bool a = plan.Trouver(idDem, NkAiPiece::Cadre, d);
			const bool c = plan.Trouver(idOutil, NkAiPiece::Titre, t);
			Essai(b, "22k", a && c && t.y >= d.y + d.h,
				  "le bloc suivant commence SOUS le precedent, sans recouvrement");
		}

		// ═══════════════════════════════════════════════════════════════════════
		// 22l / 22m / 22n — LE TIROIR NE VOILE PLUS CE SUR QUOI ON TRAVAILLE.
		//
		// Rodolf, 20/09 : le tiroir de NKUIDesign assombrissait toute la toile
		// pendant qu'il demandait de la modifier. Le voile dit « reponds a ceci
		// avant de continuer » -- un panneau de conversation ne dit pas ca : on y
		// tape EN REGARDANT ce qu'on modifie.
		//
		// ⚠️ CES TROIS ESSAIS N'EXISTENT QUE PARCE QUE LA DECISION A ETE SORTIE
		//    DU PEINTRE. Tant qu'elle vivait dans `DrawRailDrawers`, il fallait
		//    une fenetre pour l'atteindre -- donc elle n'etait jamais mesuree, et
		//    c'est exactement pour ca qu'un voile sans usage modal a survecu si
		//    longtemps a cote d'un panneau qu'il empechait d'utiliser.
		// ═══════════════════════════════════════════════════════════════════════
		{
			typedef nkentseu::editorkit::NkEditorTiroirMode Mode;
			Essai(b, "22l",
				!NkEditorTiroirVoile(Mode::Travail) &&
					NkEditorTiroirVoile(Mode::Modal),
				"un tiroir de TRAVAIL ne voile pas ; une MODALE voile");
		}
		{
			typedef nkentseu::editorkit::NkEditorTiroirMode Mode;
			// Le voile se VOIT, la reclamation non -- et c'est elle qui empeche
			// vraiment de travailler dessous. Corriger l'un sans l autre aurait
			// rendu un panneau qui a l'air utilisable et ne l'est pas.
			Essai(b, "22m",
				!NkEditorTiroirReclameLeCorps(Mode::Travail, false) &&
					NkEditorTiroirReclameLeCorps(Mode::Modal, false),
				"un tiroir de TRAVAIL ne reclame que lui-meme ; une MODALE prend le corps");
		}
		{
			typedef nkentseu::editorkit::NkEditorTiroirMode Mode;
			// R20 : PENDANT UN GLISSER, meme une modale ne reclame que soi. Sans
			// ca la cible du depot, dessous, ne recoit jamais la souris -- mesure
			// du 06/09 : souris masquee, cible jamais ouverte, 0 composant pose.
			Essai(b, "22n", !NkEditorTiroirReclameLeCorps(Mode::Modal, true),
				"controle negatif : en glisser, meme la MODALE lache le corps (R20)");
		}

		// 22o / 22p — L'EFFET MESURE EST SUR LA LIGNE, ET IL NE SE TRONQUE PAS.
		//
		// Revele par l'integration de NK3DModeler AVANT la premiere ligne de
		// migration : son panneau affiche « <demande>   .   faces 6 -> 384 » -- le
		// texte ET l'effet sur une seule ligne. Mon contrat ne savait le rendre
		// pour AUCUN type : `Outil` ignorait `effet`, et un bloc `Effet` n'affiche
		// QUE l'effet, sans le texte.
		{
			NkAiCapacites c = NkAiCapacites::Texte();
			c.produitOutil = true;
			NkAiFil f;
			f.Declarer(c);
			NkString p;
			NkAiBlocDonnees o;
			o.type = NkAiBloc::Outil;
			o.titre = NkString("subdivise");
			o.texte = NkString("subdivise le cube deux fois");
			o.effet = NkString("faces 6 -> 384");
			(void)f.Pousser(o, p);
			NkAiPlan pl;
			NkAiFilMesurer(f, W, M, Mesure, nullptr, pl);
			const uint32 id = f.At(0).id;
			NkAiRectPublie te, ef;
			const bool a = pl.Trouver(id, NkAiPiece::Texte, te);
			const bool c2 = pl.Trouver(id, NkAiPiece::Effet, ef);
			Essai(b, "22o", a && c2 && ef.y == te.y && ef.x > te.x,
				"l'effet est publie sur la MEME ligne que le texte, a sa droite");
		}
		{
			// ⚠️ QUAND LA PLACE MANQUE, C'EST LA PROSE QUI CEDE. « faces 6 -> 384 »
			//    est le FAIT ; la phrase est le commentaire. Un panneau etroit doit
			//    perdre le commentaire, jamais la mesure.
			NkAiCapacites c = NkAiCapacites::Texte();
			c.produitOutil = true;
			NkAiFil f;
			f.Declarer(c);
			NkString p;
			NkAiBlocDonnees o;
			o.type = NkAiBloc::Outil;
			o.titre = NkString("x");
			o.texte = NkString("une phrase de commentaire assez longue pour ne pas tenir");
			o.effet = NkString("faces 6 -> 384");
			(void)f.Pousser(o, p);
			NkAiPlan pl;
			NkAiFilMesurer(f, 200.f, M, Mesure, nullptr, pl);
			const uint32 id = f.At(0).id;
			NkAiRectPublie ef;
			const bool aEffet = pl.Trouver(id, NkAiPiece::Effet, ef);
			Essai(b, "22p", aEffet && ef.w > 0.f,
				"panneau etroit : l'effet est publie quand meme -- la prose cede la premiere");
		}

		// ════════════════════════════════════════════════════════════
		// 22q..22v — L EN-TETE ET LE COMPOSEUR
		// ════════════════════════════════════════════════════════════
		{
			// 22q — UNE ICONE NON DECLAREE N EST PAS PUBLIEE. Meme regle que les
			//       blocs du fil : une horloge qui ouvrirait une liste vide est le
			//       defaut des dix controles sans usage.
			NkAiPlan pl;
			NkAiEnteteDecl muet; // le porteur ne declare RIEN
			const float32 h = NkAiEnteteMesurer("Revision documents RIHEN SARL", W, 0.f, muet, M, pl);
			Essai(b, "22q",
				h > 0.f && !pl.Possede(0u, NkAiPiece::IconeHistorique) &&
					!pl.Possede(0u, NkAiPiece::IconeNouvelle) &&
					pl.Possede(0u, NkAiPiece::TitreConversation),
				"porteur muet : aucune icone publiee, le titre reste");
		}
		{
			// 22r — CONTROLE NEGATIF : declarees, elles apparaissent. Sans lui, un
			//       en-tete qui ne publierait JAMAIS d icone passerait 22q.
			NkAiPlan pl;
			NkAiEnteteDecl d;
			d.porteHistorique = true;
			d.porteNouvelle = true;
			(void)NkAiEnteteMesurer("Sujet", W, 0.f, d, M, pl);
			NkAiRectPublie ih, inv, ti;
			const bool a = pl.Trouver(0u, NkAiPiece::IconeHistorique, ih);
			const bool c = pl.Trouver(0u, NkAiPiece::IconeNouvelle, inv);
			const bool e = pl.Trouver(0u, NkAiPiece::TitreConversation, ti);
			Essai(b, "22r", a && c && e && inv.x > ih.x && ih.x > ti.x + ti.w - 1.f,
				"declarees : historique puis nouvelle, a DROITE du titre");
		}
		{
			// 22s — LE TITRE SE TRONQUE, IL NE REPOUSSE PAS LES ICONES. Un sujet
			//       long ne doit pas faire disparaitre un geste.
			NkAiPlan p1, p2;
			NkAiEnteteDecl d;
			d.porteHistorique = true;
			d.porteNouvelle = true;
			(void)NkAiEnteteMesurer("court", W, 0.f, d, M, p1);
			(void)NkAiEnteteMesurer("un sujet beaucoup beaucoup beaucoup plus long que la place",
					   W, 0.f, d, M, p2);
			NkAiRectPublie a1, a2, t1, t2;
			(void)p1.Trouver(0u, NkAiPiece::IconeHistorique, a1);
			(void)p2.Trouver(0u, NkAiPiece::IconeHistorique, a2);
			(void)p1.Trouver(0u, NkAiPiece::TitreConversation, t1);
			(void)p2.Trouver(0u, NkAiPiece::TitreConversation, t2);
			Essai(b, "22s", a1.x == a2.x && t1.w == t2.w,
				"les icones ne bougent pas avec la longueur du titre");
		}
		{
			// 22t — LE COMPOSEUR DE LA CAPTURE, AU PIXEL. Quatre lignes tapees : le
			//       cadre fait 154 px (14 + 4 x 20 + 26 + 34) et son sommet tombe a
			//       1116 dans un panneau de 1292 -- la ou la capture le met.
			// ⚠️ 21/09 : IL GRANDIT AVEC LE TEXTE. La version du 20/09 avait une
			//    hauteur FIXE de 155 px ; la capture montre un cadre qui porte ce qu'on
			//    tape, et c'est ce que cet essai mesure maintenant.
			NkAiPlan pl;
			NkAiComposeurDecl d;
			d.texte = "une\ndeux\ntrois\nquatre";
			d.invite = "Posez votre question";
			const float32 y = NkAiComposeurMesurer(d, W, 1292.f, M, Mesure, nullptr, pl);
			NkAiRectPublie cad;
			const bool a = pl.Trouver(0u, NkAiPiece::ComposeurCadre, cad);
			Essai(b, "22t", a && cad.h == 154.f && y == 1116.f && y == 1292.f - cad.h - M.margeBas,
				"4 lignes : cadre de 154 px, sommet a 1116 -- la capture, au pixel");
			printf("         composeur : sommet a %.0f, hauteur %.0f\n", (double)y, (double)cad.h);
		}
		{
			// 22u — L INVITE N EST PAS DU TEXTE. Un champ vide qui porterait le role
			//       `Text` se lirait comme un champ rempli.
			NkAiPlan vide, plein;
			NkAiComposeurDecl d0, d1;
			d0.texte = "";
			d0.invite = "Posez votre question";
			d1.texte = "subdivise le cube";
			d1.invite = "Posez votre question";
			(void)NkAiComposeurMesurer(d0, W, 1292.f, M, Mesure, nullptr, vide);
			(void)NkAiComposeurMesurer(d1, W, 1292.f, M, Mesure, nullptr, plein);
			NkAiRectPublie a, c;
			const bool x = vide.Trouver(0u, NkAiPiece::ComposeurTexte, a);
			const bool y2 = plein.Trouver(0u, NkAiPiece::ComposeurTexte, c);
			Essai(b, "22u",
				x && y2 && a.role == NkRole::TextMuted && c.role == NkRole::Text &&
					a.source == NkAiSource::Invite && c.source == NkAiSource::Saisie,
				"l invite porte le role ATTENUE et la source INVITE, le texte saisi le role plein");
		}
		{
			// 22v — CE QUI N A PAS DE SOURCE N EST PAS PUBLIE. Un porteur qui ne
			//       declare ni « + », ni « / », ni duree, ni lieu, ni mode n obtient
			//       QUE le cadre, sa ligne et l envoi -- jamais une icone qui
			//       n ouvre rien (la regle des blocs, appliquee a la barre). Et
			//       AUCUN micro : aucune reconnaissance vocale dans le depot.
			NkAiPlan muet, plein;
			NkAiComposeurDecl d0;
			d0.texte = "x";
			(void)NkAiComposeurMesurer(d0, W, 1292.f, M, Mesure, nullptr, muet);
			NkAiComposeurDecl d1 = d0;
			d1.portePlus = true;
			d1.porteCommandes = true;
			d1.duree = "57m";
			d1.lieu = "local";
			d1.modele = "Ollama · qwen2.5";
			d1.mode = "Auto";
			(void)NkAiComposeurMesurer(d1, W, 1292.f, M, Mesure, nullptr, plein);
			const bool rienDeTrop = muet.Compter(NkAiPiece::BoutonPlus) == 0 &&
									muet.Compter(NkAiPiece::BoutonCommandes) == 0 &&
									muet.Compter(NkAiPiece::Horloge) == 0 &&
									muet.Compter(NkAiPiece::PastilleLieu) == 0 &&
									muet.Compter(NkAiPiece::PastilleMode) == 0 &&
									muet.Compter(NkAiPiece::Envoi) == 1;
			const bool toutDeclare = plein.Compter(NkAiPiece::BoutonPlus) == 1 &&
									 plein.Compter(NkAiPiece::BoutonCommandes) == 1 &&
									 plein.Compter(NkAiPiece::Horloge) == 1 &&
									 plein.Compter(NkAiPiece::PastilleLieu) == 1 &&
									 plein.Compter(NkAiPiece::PastilleModele) == 1 &&
									 plein.Compter(NkAiPiece::PastilleMode) == 1;
			Essai(b, "22v", rienDeTrop && toutDeclare,
				"porteur muet : cadre, texte et envoi seulement ; declares : les six pieces de la barre");
		}
		{
			// 22y — LE RAIL RELIE LES PUCES DU FIL, ET NE TRAVERSE PAS LA DEMANDE.
			NkAiFil f;
			f.Declarer(cap);
			NkString p;
			NkAiBlocDonnees o = outil;
			o.replie = true;
			(void)f.Pousser(o, p);
			(void)f.Pousser(o, p);
			NkAiBlocDonnees dd = dem;
			(void)f.Pousser(dd, p);
			(void)f.Pousser(o, p);
			NkAiPlan pl;
			NkAiFilMesurer(f, W, M, Mesure, nullptr, pl);
			NkAiRectPublie p0, p1, r;
			const bool a = pl.Trouver(f.At(0).id, NkAiPiece::Puce, p0) &&
						   pl.Trouver(f.At(1).id, NkAiPiece::Puce, p1) && pl.Trouver(0u, NkAiPiece::Rail, r);
			Essai(b, "22y",
				a && pl.Compter(NkAiPiece::Rail) == 1 && PresPlan(r.y, p0.y + p0.h * 0.5f) &&
					PresPlan(r.y + r.h, p1.y + p1.h * 0.5f),
				"un filet de puce a puce ; la demande l interrompt (1 rail pour 3 puces)");
		}
		{
			// 22z — LE CODE NE SE REPLIE JAMAIS. Une ligne de 400 caracteres reste
			//       UNE ligne, coupee au bord par le rognage -- la capture coupe net.
			NkAiFil f;
			f.Declarer(cap);
			NkString p;
			NkAiBlocDonnees o = outil;
			o.entree = NkString("");
			for (int i = 0; i < 40; ++i)
				o.entree.Append("0123456789");
			o.sortie = NkString("ok");
			(void)f.Pousser(o, p);
			NkAiPlan pl;
			NkAiFilMesurer(f, 320.f, M, Mesure, nullptr, pl);
			Essai(b, "22z", pl.Compter(NkAiPiece::TexteIn) == 1 && pl.Compter(NkAiPiece::BoiteOutil) == 1 &&
								pl.Compter(NkAiPiece::Separateur) == 1,
				"400 caracteres d entree : UNE ligne ; IN et OUT dans UNE boite, coupes d un filet");
		}
		{
			// 22aa — LE CODE EN LIGNE ET LE GRAS DE LA PROSE. Les marques ne se
			//        peignent pas ; le code porte son fond.
			NkAiFil f;
			f.Declarer(cap);
			NkString p;
			NkAiBlocDonnees pr;
			pr.type = NkAiBloc::Prose;
			pr.texte = NkString("`durcir` apparait **sans recompilation** et le banc l eprouve");
			(void)f.Pousser(pr, p);
			NkAiPlan pl;
			NkAiFilMesurer(f, W, M, Mesure, nullptr, pl);
			uint32 gras = 0, fixe = 0, marque = 0;
			for (uint32 i = 0; i < pl.Pieces(); ++i) {
				const NkAiRectPublie &r = pl.Piece(i);
				if (r.piece != NkAiPiece::Fragment)
					continue;
				if (r.police == NkAiPolice::Grasse)
					++gras;
				if (r.police == NkAiPolice::ChasseFixe)
					++fixe;
				const char *t = f.At(0).texte.CStr() + r.debut;
				for (uint32 k = 0; k < r.longueur; ++k)
					if (t[k] == '`' || t[k] == '*')
						++marque;
			}
			Essai(b, "22aa", gras == 1 && fixe == 1 && marque == 0 && pl.Compter(NkAiPiece::FondCode) == 1,
				"un fragment gras, un a chasse fixe sur son fond, et AUCUNE marque peinte");
		}
		{
			// 22ab — « ANNULER CETTE ACTION » N EST PUBLIE QUE SUR LE BLOC DESIGNE.
			//        Notre pile a UN cran : un bouton sur une ligne ancienne annulerait
			//        la derniere en affichant le texte d une autre.
			NkAiFil f;
			f.Declarer(cap);
			NkString p;
			NkAiBlocDonnees o = outil;
			o.replie = true;
			(void)f.Pousser(o, p);
			(void)f.Pousser(o, p);
			NkAiActionsFil act;
			act.blocId = f.At(1).id;
			act.libelle[0] = "Annuler cette action";
			NkAiPlan pl;
			NkAiFilMesurer(f, W, M, Mesure, nullptr, pl, &act);
			NkAiRectPublie bt;
			const bool a = pl.TrouverIndice(f.At(1).id, NkAiPiece::Action, 0u, bt);
			Essai(b, "22ab", a && pl.Compter(NkAiPiece::Action) == 1 && !pl.Possede(f.At(0).id, NkAiPiece::Action),
				"un seul bouton, sur le bloc designe, avec SON rectangle publie");
		}
		{
			// 22ac — PANNEAU ETROIT (276 px, le modeleur) : la barre CEDE la duree
			//        puis le lieu, jamais l envoi ni le modele.
			NkAiPlan pl;
			NkAiComposeurDecl d;
			d.texte = "x";
			d.portePlus = true;
			d.porteCommandes = true;
			d.duree = "57m";
			d.lieu = "local";
			d.modele = "Local · ia_verbe";
			(void)NkAiComposeurMesurer(d, 276.f, 800.f, M, Mesure, nullptr, pl);
			bool dedans = true;
			for (uint32 i = 0; i < pl.Pieces(); ++i)
				if (pl.Piece(i).x + pl.Piece(i).w > 276.f - M.margeComposeur + 0.5f)
					dedans = false;
			Essai(b, "22ac",
				dedans && pl.Compter(NkAiPiece::Envoi) == 1 && pl.Compter(NkAiPiece::PastilleModele) == 1 &&
					pl.Compter(NkAiPiece::Horloge) == 0,
				"276 px : l envoi et le modele restent, la duree cede, rien ne sort du cadre");
		}

		{
			// 22w — L ECHELLE D INTERFACE. NK3DModeler passe chacune de ses
			//       longueurs par `S(px) = px * gUiScale`. Le kit est en pixels
			//       bruts : sans `Echelle()`, le fil resterait a 100 % pendant que
			//       le reste du panneau grandit.
			// ⚠️ CET ESSAI N EXISTE QUE PARCE QUE LE DEFAUT EST INVISIBLE ICI :
			//    `gUiScale` vaut 1 sur cette machine. Il ne se montrerait que chez
			//    quelqu un qui travaille a 125 %. *Un defaut qui ne se montre que
			//    chez un autre ne se trouve pas, il se subit.*
			NkAiFil f;
			NkAiCapacites cap = NkAiCapacites::Texte();
			cap.produitOutil = true;
			f.Declarer(cap);
			NkString p;
			NkAiBlocDonnees o;
			o.type = NkAiBloc::Outil;
			o.titre = NkString("Bash");
			o.texte = NkString("une etape");
			(void)f.Pousser(o, p);
			NkAiMetriques un, deux;
			deux.Echelle(2.f);
			NkAiPlan p1, p2;
			NkAiFilMesurer(f, 695.f, un, Mesure, nullptr, p1);
			NkAiFilMesurer(f, 695.f, deux, Mesure, nullptr, p2);
			const uint32 id = f.At(0).id;
			NkAiRectPublie a, c;
			const bool ok = p1.Trouver(id, NkAiPiece::Titre, a) &&
				  p2.Trouver(id, NkAiPiece::Titre, c);
			Essai(b, "22w", ok && PresPlan(c.x, a.x * 2.f) && PresPlan(c.h, a.h * 2.f),
				"a l echelle 2, les retraits et les hauteurs doublent");
			printf("         titre : x=%.0f h=%.0f a 100%% | x=%.0f h=%.0f a 200%%\n",
				   (double)a.x, (double)a.h, (double)c.x, (double)c.h);
		}
		{
			// 22x — CONTROLE NEGATIF : `lignesMax` est un COMPTE, pas une longueur.
			//       La mettre a l echelle afficherait DEUX FOIS PLUS de texte a
			//       200 % au lieu de l afficher deux fois plus gros.
			NkAiMetriques deux;
			const uint32 avant = deux.lignesMax;
			deux.Echelle(2.f);
			Essai(b, "22x", deux.lignesMax == avant,
				"controle negatif : le PLAFOND DE LIGNES ne suit pas l echelle");
		}

		return b;
	}

} // namespace aiplanprobe
