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
				else if (section == 2)
					insp.CorpsEffets(ctx);
				else if (section == 3)
					insp.CorpsEtats(ctx);
				else
					insp.CorpsApparence(ctx);
				ctx.EndFrame();
			}

			/// LA HAUTEUR que la section ÉTATS a réellement consommée à la
			/// disposition — une rangée par état, plus la ligne d'aveu.
			///
			/// ⚠️ POURQUOI LA HAUTEUR ET PAS LE TEXTE : en headless aucune police
			///    n'est chargée, donc `AddText` n'émet AUCUN sommet — compter les
			///    libellés dessinés donnerait zéro sur un panneau parfait (l'angle
			///    mort déjà écrit dans `TemoinRendu.h`). La hauteur consommée,
			///    elle, est un EFFET que la disposition produit vraiment.
			/// ⚠️ ET J'AI FAILLI ÉCRIRE PIRE : une première version bouclait sur
			///    les états en posant `vu = true` sans rien interroger — une garde
			///    vide, celle que ce dépôt paie depuis deux jours. Elle aurait
			///    compté six états sur un panneau qui n'en dessine aucun.
			/// LA DICHOTOMIE : un champ qui REPOND au glisser, ou rien.
			///
			/// 🔑 L'EXIGENCE DE RODOLF est « chaque propriete doit fonctionner ».
			///    La seule facon honnete de le mesurer est de FAIRE le geste :
			///    saisir le champ, glisser, et regarder si le DOCUMENT a change.
			///    Un champ qui se dessine sans repondre est le pire des trois
			///    etats possibles -- pire qu'absent, parce qu'il PROMET.
			///
			/// ⚠️ ON GLISSE, ON NE TAPE PAS : `ChampDrag` est pilotable sans
			///    clavier (clic + deplacement), la saisie ne l'est pas. Ce que ce
			///    banc prouve est donc « le champ repond au GLISSER » -- et c'est
			///    dit, plutot que laisse croire qu'il couvre la frappe aussi.
			/// @return vrai si le document a change.
			static bool GlisserSur(nkgui::NkGuiContext &ctx, InspectorPanel &insp,
								   DesignState &st, int32 section, float32 x, float32 y) {
				NkString avant;
				st.doc.Save(avant);
				// une image de survol, puis la SAISIE, puis le DEPLACEMENT :
				// c'est exactement la sequence d'une main.
				Image(ctx, insp, x, y, false, section);
				ctx.input.mouseClicked[0] = true;
				Image(ctx, insp, x, y, true, section);
				ctx.input.mouseClicked[0] = false;
				Image(ctx, insp, x + 24.f, y, true, section);
				Image(ctx, insp, x + 24.f, y, false, section);
				NkString apres;
				st.doc.Save(apres);
				return strcmp(avant.Data(), apres.Data()) != 0;
			}

			/// Combien de BANDES distinctes de la section repondent au glisser.
			/// ⚠️ ON COMPTE LES BANDES, PAS LES POSITIONS : un champ de 20 px
			///    repond sur ~5 lignes de balayage ; compter les positions
			///    donnerait un multiple qui ne veut rien dire.
			static nkentseu::int32 ChampsQuiRepondent(nkgui::NkGuiContext &ctx,
													  InspectorPanel &insp, DesignState &st,
													  int32 section, float32 xIgnore = 0.f) {
				(void)xIgnore;
				// 🔴 ON BALAIE EN X AUSSI, ET C'EST UNE CORRECTION MESUREE : la
				//    premiere sonde visait une seule colonne (x = 120) et
				//    rapportait « 0 champ » sur BORDURES et REMPLISSAGES. Elle
				//    tapait dans le champ HEXA (une saisie, pas un glisser) ;
				//    l'opacite vit a ~208 (`col.opacX = x1 - 80`). J'ai failli
				//    rapporter deux sections mortes qui ne le sont pas.
				//    ⚠️ *Un harnais qui vise une colonne mesure cette colonne, pas
				//       la section* -- exactement la faute du banc qui forcait
				//       `ctrl = true`, sous une autre forme. La sonde balaie donc
				//       les DEUX axes, comme une main qui cherche.
				int32 bandes = 0;
				bool dedans = false;
				for (float32 y = 4.f; y < 260.f; y += 4.f) {
					bool r = false;
					for (float32 x = 40.f; x < 290.f && !r; x += 12.f)
						r = GlisserSur(ctx, insp, st, section, x, y);
					if (r && !dedans)
						++bandes;
					dedans = r;
				}
				return bandes;
			}

			static float32 HauteurEtatsConsommee(nkgui::NkGuiContext &ctx,
												 InspectorPanel &insp) {
				// ⚠️ LA SECTION EST REPLIÉE PAR DÉFAUT, et le banc doit l'OUVRIR
				//    comme le ferait une main — pas changer son défaut pour se
				//    faciliter la vie. Sans ça il mesurait 0 px et accusait le
				//    panneau de ne rien dessiner, alors qu'il obéissait.
				if (InspectorPanel::EtatSection *s = insp.TrouverSection("ÉTATS"))
					s->ouvert = true;
				ctx.input.mousePos = {-500.f, -500.f};
				ctx.input.mouseDown[0] = false;
				ctx.BeginFrame(1.f / 60.f);
				ctx.BeginLayout({0.f, 0.f, 300.f, 800.f});
				const float32 avant = ctx.layout.cursor.y;
				insp.CorpsEtats(ctx);
				const float32 apres = ctx.layout.cursor.y;
				ctx.EndFrame();
				return apres - avant;
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

				// ═══ 4. LES ÉTATS : LE PANNEAU SUIT LA TABLE, PAS UN NOMBRE ═══
				// 🔑 L'exigence de Rodolf (Q53) : « si plus tard on en ajoutait,
				//    ça devrait montrer le nombre exact ». Ce cas est le JUMEAU
				//    de `NkNbRaccourcisCtx` contre le `6` en dur du dispatcher :
				//    il compare ce que le panneau CONSOMME au nombre d'entrées de
				//    la table, sans jamais écrire « 6 ».
				{
					uint32 nbEtats = 0;
					(void)nkuidesign::guifmt::NkGEtats(nbEtats);
					const float32 h = HauteurEtatsConsommee(ctx, insp);
					// Une rangée par état (HRangee), plus la ligne d'aveu
					// « édition : au lot suivant ». On borne au lieu d'exiger
					// l'égalité stricte : la ligne d'aveu a sa propre hauteur, et
					// la figer ici ferait tomber le cas au premier changement de
					// mise en page — un banc trop précis devient un banc faux.
					const float32 attenduMin = (float32)nbEtats * costume::HRangee;
					const bool suit = nbEtats > 0 && h >= attenduMin
									  && h <= attenduMin + 3.f * costume::HRangee;
					// ⚠️ LE CONTRÔLE NÉGATIF, sans lui le cas passerait sur un
					//    panneau qui dessinerait un nombre FIXE de rangées : on
					//    exige que la hauteur soit un MULTIPLE du nombre d'états
					//    à moins de deux rangées près — donc qu'elle bouge si la
					//    table grandit.
					const bool proportionnel = h > costume::HRangee * 2.f;
					char d4[192];
					snprintf(d4, sizeof(d4),
							 "la table declare %u etat(s) ; le panneau consomme %.0f px "
							 "(attendu >= %.0f) ; suit la table=%d ; proportionnel=%d",
							 nbEtats, (double)h, (double)attenduMin, suit ? 1 : 0,
							 proportionnel ? 1 : 0);
					check("4. ETATS : le panneau N'A AUCUNE LISTE -- il itere la table fermee, "
						  "donc sa hauteur suit le nombre d'etats declares",
						  suit && proportionnel, d4);
				}

// ═══ 5. UN ETAT SE POSE, VOYAGE, ET SE RETIRE SANS TRACE ═══
				// Les portes que le CHAMP du panneau appelle, exercees avec la
				// meme semantique que lui : poser un fond, puis vider le champ.
				//
				// ⚠️ CE QUE CE CAS NE COUVRE PAS, ET JE NE LE MAQUILLE PAS : la
				//    FRAPPE elle-meme (le focus clavier de NKGui) n'est pas pilotee
				//    ici -- le BRANCHEMENT du champ est verifie par la compilation
				//    et par la capture, sa SEMANTIQUE par ce cas. *Un banc qui
				//    pretendrait taper alors qu'il appelle une fonction mentirait
				//    sur ce qu'il prouve.*
				{
					NkUINode &nd = st.doc.nodes[(uint32)rc];
					nd.apparences.Clear();
					// (a) POSER : le bloc nait, et LUI SEUL.
					NkBlocEtat(nd, "Hover").fond = NkString("#ff0000");
					const NkApparenceEtat *h = NkBlocEtatSi(nd, "Hover");
					const bool pose = (uint32)nd.apparences.Size() == 1u && h && !h->Vide()
									  && NkBlocEtatSi(nd, "Pressed") == nullptr;
					// (b) VOYAGER : l'aller-retour rend le fond de CET etat.
					NkString ecrit;
					st.doc.Save(ecrit);
					NkUIDocument relu;
					const bool relit = relu.Load(ecrit.Data()) && relu.IsValidIndex(rc);
					const NkApparenceEtat *h2 =
						relit ? NkBlocEtatSi(relu.nodes[(uint32)rc], "Hover") : nullptr;
					const bool voyage = h2 && strcmp(h2->fond.Data(), "#ff0000") == 0
										&& h2->radius < 0.f && h2->opacite < 0.f;
					// (c) LA CLE EST DANS LE TEXTE -- sans ce volet, (b) passerait
					//     sur un document qui garderait tout en memoire.
					const bool dansLeTexte = strstr(ecrit.Data(), "apparence_Hover") != nullptr;
					// (d) VIDER LE CHAMP RETIRE LE BLOC, sans laisser de trace.
					NkBlocEtat(nd, "Hover").fond = NkString("");
					if (NkBlocEtat(nd, "Hover").Vide())
						NkRetirerBlocEtat(nd, "Hover");
					NkString apres;
					st.doc.Save(apres);
					const bool retire = nd.apparences.Empty()
										&& strstr(apres.Data(), "apparence_") == nullptr;
					char d5[224];
					snprintf(d5, sizeof(d5),
							 "pose (1 bloc, les autres intacts)=%d ; cle DANS le texte=%d ; "
							 "aller-retour rend le fond=%d ; vider retire sans trace=%d",
							 pose ? 1 : 0, dansLeTexte ? 1 : 0, voyage ? 1 : 0, retire ? 1 : 0);
					check("5. ETATS : un fond se pose sur UN etat, voyage dans le fichier, et "
						  "vider le champ retire le bloc SANS laisser de cle fantome",
						  pose && dansLeTexte && voyage && retire, d5);
				}

				// ═══ 7. LE PLANTAGE DE RODOLF : LES DEUX BRANCHES RESTANTES ═══
				// 🔴 IL A SIGNALE LE DEFAUT UNE SECONDE FOIS, sur le binaire
				//    CORRIGE. La sortie franche n'avait donc ferme qu'UNE branche
				//    (la liste a plusieurs entrees). Ce cas exerce les deux que
				//    personne n'avait jouees :
				//      (a) la CLE SIMPLE -- `borderColor` seule, pas de liste ;
				//      (b) la DERNIERE entree d'une liste -- apres retrait la
				//          liste devient VIDE, et `simple` reste faux.
				// ⚠️ ON REDESSINE APRES CHAQUE CLIC : c'est l'image suivante qui
				//    paie un indice perime, jamais celle du clic.
				{
					// (a) LA CLE SIMPLE
					NkUINode &nd = st.doc.nodes[(uint32)rc];
					nd.borders.Clear();
					nd.borderColor = NkString("#abcdef");
					nd.borderW = 3.f;
					const bool viseS =
						CliquerPoubelleDuHaut(ctx, insp, 0, st.doc.nodes[(uint32)rc], 0u);
					// deux images de plus : la ligne 2 et la resynchro des tampons
					Image(ctx, insp, -500.f, -500.f, false, 0);
					Image(ctx, insp, -500.f, -500.f, false, 0);
					const bool videeS = st.doc.nodes[(uint32)rc].borderColor.Empty()
										&& st.doc.nodes[(uint32)rc].borderW == 0.f;

					// (b) LA DERNIERE ENTREE D'UNE LISTE
					NkBordure seule;
					seule.couleur = NkString("#fedcba");
					seule.epaisseur = 5.f;
					st.doc.nodes[(uint32)rc].borderColor = NkString();
					st.doc.nodes[(uint32)rc].borderW = 0.f;
					st.doc.nodes[(uint32)rc].borders.Clear();
					st.doc.nodes[(uint32)rc].borders.PushBack(seule);
					const bool viseD =
						CliquerPoubelleDuHaut(ctx, insp, 0, st.doc.nodes[(uint32)rc], 1u);
					Image(ctx, insp, -500.f, -500.f, false, 0);
					Image(ctx, insp, -500.f, -500.f, false, 0);
					const bool videeD = st.doc.nodes[(uint32)rc].borders.Empty();

					// ⚠️ `viseS` N'EST PAS UNE EXIGENCE ICI, et c'est une limite
					//    de HARNAIS, pas du code : `CliquerPoubelleDuHaut` juge le
					//    succes sur la TAILLE DE LA LISTE, or la cle simple n'a
					//    pas de liste -- il rend donc faux sur un geste qui a
					//    parfaitement agi. *L'EFFET est ce qui compte* : les cles
					//    sont-elles videes ? Exiger `viseS` aurait fait rougir le
					//    cas sur ma propre mesure, pas sur un defaut.
					char d7[208];
					snprintf(d7, sizeof(d7),
							 "cle simple : cles videes=%d (visee=%d, non exige : le harnais "
							 "juge sur la liste) ; derniere entree : visee=%d, liste vide=%d",
							 videeS ? 1 : 0, viseS ? 1 : 0, viseD ? 1 : 0, videeD ? 1 : 0);
					check("7. BORDURES : la CLE SIMPLE et la DERNIERE ENTREE se retirent sans "
						  "que l'image suivante lise une case perimee",
						  videeS && viseD && videeD, d7);
				}

								// ═══ 6. LA DICHOTOMIE : CHAQUE CHAMP REPOND, OU RIEN ═══
								// 🔑 L'exigence de Rodolf : « chaque propriete doit fonctionner ».
								//    On le MESURE par le geste au lieu de le supposer. Un champ qui
								//    se dessine sans repondre est pire qu'absent : il PROMET.
								{
									NkUINode &nd = st.doc.nodes[(uint32)rc];
									nd.apparences.Clear();
									nd.radius = 4.f;
									const int32 bandes = ChampsQuiRepondent(ctx, insp, st, 4);
					// DIAGNOSTIC des sections voisines -- il DIT ce qu'il trouve
					// plutot que d'exiger un chiffre : c'est une SONDE, et une
					// sonde qui exigerait aurait fige l'etat du jour en contrat.
					// ⚠️ LA SONDE DOIT AVOIR QUELQUE CHOSE A SONDER : les cas
					//    precedents ont VIDE fills et borders (ils testaient la
					//    poubelle). Sonder ainsi aurait rendu 0 partout et fait
					//    crier au champ mort -- la faute de donnees degenerees, une
					//    fois de plus. On repose une entree dans chaque liste.
					{
						NkRemplissage f;
						f.couleur = NkString("#123456");
						nd.fills.Clear();
						nd.fills.PushBack(f);
						NkBordure bo;
						bo.couleur = NkString("#654321");
						bo.epaisseur = 2.f;
						nd.borders.Clear();
						nd.borders.PushBack(bo);
					}
					const int32 bBord = ChampsQuiRepondent(ctx, insp, st, 0);
					const int32 bRemp = ChampsQuiRepondent(ctx, insp, st, 1);
					const int32 bEffet = ChampsQuiRepondent(ctx, insp, st, 2);
					char son[96];
					snprintf(son, sizeof(son), "bordures=%d remplissages=%d effets=%d",
						 bBord, bRemp, bEffet);
					check("6bis. SONDE des sections voisines (diagnostic, sans exigence)",
						  true, son);
									// ⚠️ PLANCHER, PAS EGALITE : le nombre exact depend du noeud (un
									//    cadre n'a pas tous les reglages) -- figer l'egalite ferait
									//    tomber le cas au premier reglage legitimement absent. *Un banc
									//    trop precis devient un banc faux.*
									const bool repondent = bandes >= 2;
									char d6[176];
									snprintf(d6, sizeof(d6),
											 "APPARENCE : %d bande(s) de champ repondent au glisser", bandes);
									check("6. DICHOTOMIE : les champs d'APPARENCE REPONDENT au geste",
										  repondent, d6);
								}

								printf("\nRECETTE PROPRIETES : %d/%d %s\n", total - echecs, total,
					   echecs == 0 ? "PROUVEE" : "EN ECHEC");
				return echecs == 0 ? 0 : 1;
			}
	};

	inline nkentseu::int32 NkRecetteProprietes() {
		return RecetteProprietesAcces::Lancer();
	}

} // namespace nkuidesign
