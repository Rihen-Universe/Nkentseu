#pragma once
// -----------------------------------------------------------------------------
// @File    Applications/NKEditorKitTest/src/NkAiPlanProbe.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
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

		return b;
	}

} // namespace aiplanprobe
