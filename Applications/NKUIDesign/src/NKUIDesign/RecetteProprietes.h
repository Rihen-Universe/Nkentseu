#pragma once
// -----------------------------------------------------------------------------
// @File    RecetteProprietes.h
// @Brief   `--recette-proprietes` : LES LISTES DE PROPRIETES (remplissages,
//          bordures, effets) EXERCEES PAR LE GESTE, avec une vraie souris.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  ⚠️ POURQUOI CE BANC EXISTE, ET POURQUOI IL PILOTE UNE SOURIS
// =============================================================================
//  Rodolf, 2026-09-02 : *« la suppression d'une bordure fait planter »*. L'ajout
//  marchait, le retrait non.
//
//  🔴 J'AVAIS CONCLU, PAR LA SEULE LECTURE, QUE LES TROIS SECTIONS ETAIENT
//     SAINES -- et j'avais tort. Mon raisonnement etait : « la boucle descend
//     (`i = nb-1-vi`), donc apres un retrait les tours SUIVANTS ont un indice
//     plus petit, jamais hors bornes ». Il est juste, et il ne couvre que la
//     moitie du probleme : **les BORDURES ont DEUX LIGNES par entree**, et la
//     seconde s'execute APRES la poubelle, DANS LA MEME ITERATION. Ce n'est pas
//     le tour suivant qui deborde, c'est la suite du tour courant.
//
//     *J'ai verifie ce qui se passe entre deux tours, pas ce qui se passe dans
//     un tour.* D'ou ce banc : une lecture qui conclut « c'est sain » ne vaut
//     pas une souris qui clique.
//
//  ⚠️ ET LE PIRE CAS EST LE PREMIER CLIC : la boucle dessine du DERNIER au
//     PREMIER, donc au premier tour `i` vaut le dernier indice. Retirer cette
//     entree-la ramene la taille a `i` exactement, et la ligne 2 lit
//     `borders[i]` -- UNE CASE AU-DELA. La ligne du haut est justement celle
//     qu'on clique en premier : le defaut tombe a coup sur.
//
//  COMMENT LE BANC VISE SANS DEVINER : il dessine la section, lit le RELEVE
//  d'introspection pour retrouver le rectangle du champ hexa de la ligne du
//  haut, et en DEDUIT la poubelle par la geometrie des colonnes
//  (`ColonnesDe`) : `poubX = hexX + hexW + 46`. Aucun pixel n'est devine, et le
//  jour ou les colonnes bougent, le banc suit.
// -----------------------------------------------------------------------------

#include <cstdio>
#include <cstring>

#include "Panels.h"

namespace nkuidesign {

	/// L'acces de banc au corps des sections de proprietes (declare ami).
	struct RecetteProprietesAcces {

			/// Le rectangle du champ hexa le plus HAUT parmi les notes de saisie.
			/// ⚠️ ON PREND LE PLUS HAUT PARCE QUE LA LISTE SE DESSINE A L'ENVERS :
			///    la ligne du haut porte l'indice le PLUS GRAND, c'est-a-dire
			///    celle dont le retrait deborde. Viser la ligne du bas ne
			///    reproduirait rien.
			static bool ChampLePlusHaut(const nkgui::NkGuiContext &ctx, nkgui::NkRect &out) {
				nkentseu::int32 n = 0;
				const nkgui::NkGuiNote *notes = nkgui::NkGuiIntrospectNotes(ctx, n);
				bool trouve = false;
				for (nkentseu::int32 i = 0; i < n; ++i) {
					if (notes[i].nature != nkgui::NkGuiNature::Champ)
						continue;
					if (notes[i].rect.w <= 0.f || notes[i].rect.h <= 0.f)
						continue;
					if (!trouve || notes[i].rect.y < out.y) {
						out = notes[i].rect;
						trouve = true;
					}
				}
				return trouve;
			}

			/// Une image : entree posee AVANT `BeginFrame` (c'est lui qui calcule
			/// les transitions), section dessinee dans une region connue.
			static void Image(nkgui::NkGuiContext &ctx, InspectorPanel &insp, float32 mx,
							  float32 my, bool clic, int32 section) {
				ctx.input.mousePos = {mx, my};
				ctx.input.mouseDown[0] = clic;
				ctx.BeginFrame(1.f / 60.f);
				ctx.BeginLayout({0.f, 0.f, 300.f, 800.f});
				if (section == 0)
					insp.CorpsBordures(ctx);
				else if (section == 1)
					insp.CorpsRemplissages(ctx);
				else
					insp.CorpsEffets(ctx);
				ctx.EndFrame();
			}

			/// Le geste complet : trouver la poubelle de la ligne du HAUT, la
			/// cliquer.
			///
			/// ⚠️ ON BALAIE EN Y AU LIEU DE CALCULER LA LIGNE, ET C'EST UNE
			///    CORRECTION MESUREE. Ma premiere version deduisait la position
			///    du rectangle du champ hexa. Elle marchait pour les remplissages
			///    et les bordures -- et **rien** pour les effets, dont la ligne 1
			///    (type, oeil, poubelle) ne porte AUCUN champ de saisie : le banc
			///    visait alors le champ de flou de la ligne 2, cliquait dans le
			///    vide, et rapportait « il reste 2 effets » comme si le geste
			///    avait echoue. *Un banc qui vise a cote accuse le code au lieu de
			///    s'accuser lui-meme.*
			///
			///    Le balayage, lui, ne suppose aucune mise en page : il fait ce
			///    que fait une main -- promener la souris jusqu'a ce que ca
			///    reponde. Il reste deterministe (meme ordre, meme premier trouve)
			///    et il survivra a un changement de colonnes.
			///
			/// ⚠️ EN X, LA COLONNE EST CONNUE ET UNIQUE : `ColonnesDe` pose
			///    `poubX = r.x + r.w - 48` et `oeilX = r.x + r.w - 28`. Vingt
			///    pixels separent les deux icones, qui en font quatorze : viser le
			///    centre de la poubelle ne peut pas atteindre l'oeil.
			///
			/// @param nbAvant la taille de la liste avant le geste : le balayage
			///        s'arrete des qu'elle DIMINUE, jamais sur une position.
			static bool CliquerPoubelleDuHaut(nkgui::NkGuiContext &ctx, InspectorPanel &insp,
											  int32 section, const NkUINode &n,
											  uint32 nbAvant) {
				auto taille = [&]() -> uint32 {
					return section == 0	  ? (uint32)n.borders.Size()
						   : section == 1 ? (uint32)n.fills.Size()
										  : (uint32)n.effets.Size();
				};
				// ⚠️ ON BALAIE LES DEUX AXES, ET C'EST LA TROISIEME VERSION DE
				//    CETTE FONCTION -- les deux precedentes SUPPOSAIENT une
				//    geometrie. La premiere deduisait la poubelle du rectangle du
				//    champ hexa (juste pour deux sections sur trois : les effets
				//    n'ont aucun champ sur leur ligne 1). La seconde calculait
				//    `x = largeur - 48` en supposant que la rangee occupe toute la
				//    region -- elle ne l'occupe pas, `NextItemRect` applique ses
				//    propres marges. *Deux fois, le banc a vise a cote et rapporte
				//    « le geste a echoue » : un harnais qui se trompe de cible
				//    accuse le code au lieu de s'accuser lui-meme.*
				//
				//    Le balayage ne suppose plus RIEN. Il fait ce que fait une
				//    main qui cherche : promener la souris jusqu'a ce que ca
				//    reponde. Le pas est de 3 px, l'icone en fait 14 : on ne peut
				//    pas la manquer.
				//
				// ⚠️ ET L'ARRET SE FAIT SUR L'EFFET, PAS SUR UNE POSITION : des
				//    que la liste DIMINUE, on a clique la poubelle. Un balayage
				//    qui s'arreterait sur des coordonnees attendues serait
				//    exactement la supposition qu'on vient de retirer deux fois.
				for (float32 py = 2.f; py < 340.f; py += 3.f) {
					for (float32 px = 120.f; px < 300.f; px += 3.f) {
						// le survol se resout sur l'image PRECEDENTE : poser la
						// souris, PUIS presser -- comme une vraie main, et comme
						// le harnais des menus l'a etabli.
						Image(ctx, insp, px, py, false, section);
						Image(ctx, insp, px, py, true, section);
						Image(ctx, insp, px, py, false, section);
						if (taille() < nbAvant)
							return true;
					}
				}
				return false;
			}

			static nkentseu::int32 Lancer() {
				using namespace nkentseu;
				int32 echecs = 0, total = 0;
				auto check = [&](const char *nom, bool ok, const char *det) {
					printf("%s  %s  -- %s\n", ok ? "OK   " : "ECHEC", nom, det);
					++total;
					if (!ok)
						++echecs;
				};

				static nkgui::NkGuiContext ctx;
				if (!ctx.Init(300, 800)) {
					printf("ECHEC  le contexte sans fenetre a refuse\n");
					return 1;
				}
				nkgui::NkGuiIntrospectActiver(ctx, true);

				// ── LE DOCUMENT D'ESSAI : un rectangle, DEUX bordures ─────────
				// ⚠️ DEUX, PAS UNE, ET C'EST LA DONNEE QUI PERMET D'EXPRIMER
				//    L'ECART. Avec une seule bordure, la retirer laisse la liste
				//    VIDE : `simple` redevient vrai a l'image suivante et la ligne
				//    2 prend sa branche sans indice -- le defaut ne se voit pas.
				//    Avec deux, le retrait de celle du haut laisse `i` a UNE CASE
				//    AU-DELA de la taille. *Troisieme fois aujourd'hui qu'une
				//    donnee d'essai decide si un banc mesure quelque chose.*
				static DesignState st;
				st.doc.NewDocument("proprietes", NkAuthor::Humain);
				const int32 pg = st.doc.AddChild(0, "", NkAuthor::Humain);
				const int32 rc = st.doc.AddChild(pg, "", NkAuthor::Humain);
				st.doc.nodes[(uint32)rc].shape = NkString("rect");
				st.doc.nodes[(uint32)rc].label = NkString("Rect");
				{
					NkBordure b1;
					b1.couleur = NkString("#111111");
					b1.epaisseur = 1.f;
					b1.position = NkBordurePos::Interieur;
					NkBordure b2;
					b2.couleur = NkString("#222222");
					b2.epaisseur = 7.f; // reconnaissable : c'est LUI qui doit survivre
					b2.position = NkBordurePos::Exterieur;
					st.doc.nodes[(uint32)rc].borders.PushBack(b1);
					st.doc.nodes[(uint32)rc].borders.PushBack(b2);
				}
				st.SelectSingle(rc);
				static InspectorPanel insp(&st);

				// ── 1. LE GESTE : cliquer la poubelle de la ligne du HAUT ──────
				// Avant la correction, ce clic lit `borders[1]` sur une liste qui
				// vient de tomber a 1 element -- une case au-dela.
				const bool vise = CliquerPoubelleDuHaut(ctx, insp, 0, st.doc.nodes[(uint32)rc], 2u);
				const uint32 reste = (uint32)st.doc.nodes[(uint32)rc].borders.Size();
				// une image de plus : la ligne 2 de la ligne survivante se
				// redessine, et c'est la qu'un indice perime se paierait.
				Image(ctx, insp, -500.f, -500.f, false, 0);
				const bool uneSeule = reste == 1u;
				// ⚠️ ET C'EST LA BONNE QUI RESTE. Un retrait qui emporterait la
				//    mauvaise laisserait aussi UNE bordure : le compte seul ne
				//    discrimine pas. On reconnait la survivante a son epaisseur.
				const bool bonneRestante =
					uneSeule && st.doc.nodes[(uint32)rc].borders[0].epaisseur == 1.f
					&& st.doc.nodes[(uint32)rc].borders[0].position == NkBordurePos::Interieur;
				char d1[192];
				snprintf(d1, sizeof(d1), "poubelle visee=%d ; il reste %u bordure(s)=%d ; la "
										 "survivante est la BONNE (ep=%.0f, pos=%d)=%d",
						 vise ? 1 : 0, reste, uneSeule ? 1 : 0,
						 uneSeule ? (double)st.doc.nodes[(uint32)rc].borders[0].epaisseur : 0.0,
						 uneSeule ? (int)st.doc.nodes[(uint32)rc].borders[0].position : -1,
						 bonneRestante ? 1 : 0);
				check("1. BORDURES : cliquer la poubelle de la ligne du HAUT retire CETTE "
					  "bordure, sans lire au-dela de la liste (la ligne 2 suit le retrait dans "
					  "la MEME iteration)",
					  vise && uneSeule && bonneRestante, d1);

				// ── 2. LE CHEMIN FRERE : REMPLISSAGES, deux entrees ───────────
				// ⚠️ ON NE CONCLUT PAS « ils sont saufs » PAR LA LECTURE : on
				//    refait le meme geste. Une seule ligne par entree DEVRAIT les
				//    proteger -- le banc le verifie au lieu de le supposer.
				st.doc.nodes[(uint32)rc].borders.Clear();
				{
					NkRemplissage f1;
					f1.couleur = NkString("#aaaaaa");
					f1.opacite = 40.f;
					NkRemplissage f2;
					f2.couleur = NkString("#bbbbbb");
					f2.opacite = 90.f;
					st.doc.nodes[(uint32)rc].fills.PushBack(f1);
					st.doc.nodes[(uint32)rc].fills.PushBack(f2);
				}
				const bool viseF = CliquerPoubelleDuHaut(ctx, insp, 1, st.doc.nodes[(uint32)rc], 2u);
				const uint32 resteF = (uint32)st.doc.nodes[(uint32)rc].fills.Size();
				Image(ctx, insp, -500.f, -500.f, false, 1);
				const bool bonF = resteF == 1u
								  && st.doc.nodes[(uint32)rc].fills[0].opacite == 40.f;
				char d2[176];
				snprintf(d2, sizeof(d2), "poubelle visee=%d ; il reste %u remplissage(s) ; la "
										 "survivante porte l'opacite %.0f (attendu 40)=%d",
						 viseF ? 1 : 0, resteF,
						 resteF == 1u ? (double)st.doc.nodes[(uint32)rc].fills[0].opacite : 0.0,
						 bonF ? 1 : 0);
				check("2. REMPLISSAGES : le MEME geste, mesure et non suppose -- une seule ligne "
					  "par entree, donc rien ne suit le retrait dans l'iteration",
					  viseF && bonF, d2);

				// ── 3. LE TROISIEME FRERE : EFFETS ────────────────────────────
				// Ils sortent par `return` apres le retrait -- une SORTIE FRANCHE
				// qui protege tout ce qui suit, y compris ce que le prochain
				// ajoutera. Le banc le verifie aussi.
				st.doc.nodes[(uint32)rc].fills.Clear();
				{
					NkEffet e1;
					e1.type = NkEffetType::OmbrePortee;
					e1.flou = 3.f;
					NkEffet e2;
					e2.type = NkEffetType::OmbrePortee;
					e2.flou = 11.f;
					st.doc.nodes[(uint32)rc].effets.PushBack(e1);
					st.doc.nodes[(uint32)rc].effets.PushBack(e2);
				}
				const bool viseE = CliquerPoubelleDuHaut(ctx, insp, 2, st.doc.nodes[(uint32)rc], 2u);
				const uint32 resteE = (uint32)st.doc.nodes[(uint32)rc].effets.Size();
				Image(ctx, insp, -500.f, -500.f, false, 2);
				const bool bonE = resteE == 1u
								  && st.doc.nodes[(uint32)rc].effets[0].flou == 3.f;
				char d3[176];
				snprintf(d3, sizeof(d3), "poubelle visee=%d ; il reste %u effet(s) ; le survivant "
										 "porte le flou %.0f (attendu 3)=%d",
						 viseE ? 1 : 0, resteE,
						 resteE == 1u ? (double)st.doc.nodes[(uint32)rc].effets[0].flou : 0.0,
						 bonE ? 1 : 0);
				check("3. EFFETS : le MEME geste -- leur sortie franche apres le retrait protege "
					  "la suite de l'iteration",
					  viseE && bonE, d3);

				printf("\nRECETTE PROPRIETES : %d/%d %s\n", total - echecs, total,
					   echecs == 0 ? "PROUVEE" : "EN ECHEC");
				return echecs == 0 ? 0 : 1;
			}
	};

	inline nkentseu::int32 NkRecetteProprietes() {
		return RecetteProprietesAcces::Lancer();
	}

} // namespace nkuidesign
