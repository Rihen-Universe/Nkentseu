// -----------------------------------------------------------------------------
// FICHIER: Dames/NkDamesEcran.cpp
// DESCRIPTION: Geometrie et dessin. Ne decide rien, ne modifie rien.
//
// LA REGLE DE CE FICHIER : il ne prend AUCUNE decision de jeu. S'il a besoin de
// savoir quelque chose, ce quelque chose arrive par NkDamesVue. Le jour ou une
// fonction d'ici veut appeler CoupsLegaux, c'est qu'elle est au mauvais endroit.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Dames/NkDamesEcran.h"
#include "Dames/NkDamesTheme.h"
#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace jeux {
		namespace dames {

			namespace {
				// --- Aides de texte -------------------------------------------
				// Elles delegent aux memes conventions que NkCanvasGuiApp : `topY`
				// est le HAUT du texte, pas sa ligne de base.
				// ⚠️ RELAIS, plus des implementations : le corps vit dans
				// NKCanvas/App/NkCanvasTexte.h, en fonctions LIBRES. Ces trois
				// copies existaient parce que les aides avaient d'abord ete
				// rangees en methodes protegees de NkCanvasGuiApp — invisibles
				// depuis un fichier de dessin. Corrige le 2026-09-01.
				float32 MesurerW(NkGuiFont *f, const char *s) noexcept {
					return renderer::NkTexteLargeur(f, s);
				}
				void Texte(NkGuiDrawList &dl, NkGuiFont *f, float32 x, float32 topY, const char *s, const NkColor &c,
						   float32 maxWidth = -1.f) {
					renderer::NkTexte(dl, f, x, topY, s, c, maxWidth);
				}
				void TexteDansBoite(NkGuiDrawList &dl, NkGuiFont *f, const NkRect &box, const char *s,
									const NkColor &c) {
					renderer::NkTexteDansBoite(dl, f, box, s, c);
				}
				void TexteADroite(NkGuiDrawList &dl, NkGuiFont *f, float32 droite, float32 topY, const char *s,
								  const NkColor &c) {
					renderer::NkTexteADroite(dl, f, droite, topY, s, c);
				}
			} // namespace

			// =====================================================================
			// NkDamesAnim
			// =====================================================================
			void NkDamesAnim::CaseFinale(int32 &r, int32 &c) const noexcept {
				if (nbEtapes == 0) {
					r = depR;
					c = depC;
					return;
				}
				r = etapeR[nbEtapes - 1];
				c = etapeC[nbEtapes - 1];
			}

			void NkDamesAnim::Position(float32 &r, float32 &c) const noexcept {
				if (nbEtapes == 0) {
					r = static_cast<float32>(depR);
					c = static_cast<float32>(depC);
					return;
				}
				// On repartit le temps EGALEMENT entre les segments. Le repartir
				// selon la longueur donnerait une vitesse constante — plus juste
				// physiquement, mais une rafle de huit prises deviendrait alors
				// interminable sur ses grands sauts.
				const float32 tt = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
				const float32 pos = tt * static_cast<float32>(nbEtapes);
				int32 seg = static_cast<int32>(pos);
				if (seg >= nbEtapes) {
					seg = nbEtapes - 1;
				}
				const float32 u = pos - static_cast<float32>(seg);

				const float32 r0 = (seg == 0) ? static_cast<float32>(depR) : static_cast<float32>(etapeR[seg - 1]);
				const float32 c0 = (seg == 0) ? static_cast<float32>(depC) : static_cast<float32>(etapeC[seg - 1]);
				const float32 r1 = static_cast<float32>(etapeR[seg]);
				const float32 c1 = static_cast<float32>(etapeC[seg]);

				// Adoucissement aux deux bouts : un deplacement lineaire demarre
				// et s'arrete sec, ce qui se lit comme un saut.
				const float32 e = u * u * (3.f - 2.f * u);
				r = r0 + (r1 - r0) * e;
				c = c0 + (c1 - c0) * e;
			}

			// =====================================================================
			bool NkDansRect(const NkRect &r, const NkVec2f &p) noexcept {
				return p.x >= r.x && p.y >= r.y && p.x < r.x + r.w && p.y < r.y + r.h;
			}

			bool NkDamesGeometrie::CaseSous(const NkVec2f &p, int32 &r, int32 &c) const noexcept {
				if (cellule <= 0.f || p.x < plateau.x || p.y < plateau.y || p.x >= plateau.x + plateau.w ||
					p.y >= plateau.y + plateau.h) {
					return false;
				}
				c = static_cast<int32>((p.x - plateau.x) / cellule);
				r = static_cast<int32>((p.y - plateau.y) / cellule);
				return NkDamesPartie::DansDamier(r, c);
			}

			// =====================================================================
			void NkDamesGeometrie::Calculer(const renderer::NkLayoutInfo &info) noexcept {
				const float32 W = static_cast<float32>(info.width);
				const float32 H = static_cast<float32>(info.height);

				// ⚠️ La zone sure n'est PAS une marge decorative : sous l'encoche
				// ou l'indicateur de geste, un bouton devient INATTEIGNABLE. On
				// s'y ancre pour tout ce qui se touche ou se lit.
				const float32 hautSur = info.safeArea.top;
				const float32 basSur = info.safeArea.bottom;
				const float32 gaucheSur = info.safeArea.left;
				const float32 droiteSur = info.safeArea.right;

				// ── LA MARGE SE MESURE SUR LE PLUS PETIT COTE ───────────────
				// ⚠️ `W * 0.03f` donnait, en paysage, une marge deux fois plus
				// large que haute. Une proportion se prend sur la dimension
				// CONTRAINTE, jamais sur la plus grande.
				const float32 petitEcran = W < H ? W : H;
				const float32 marge = petitEcran * 0.03f;

				const float32 zoneX = gaucheSur + marge;
				const float32 zoneY = hautSur + marge;
				const float32 zoneW = W - gaucheSur - droiteSur - marge * 2.f;
				const float32 zoneH = H - hautSur - basSur - marge * 2.f;

				// ── QUAND BASCULER EN DEUX COLONNES ─────────────────────────
				// ⚠️ PAS SUR `IsPortrait()`, qui ne dit que `H >= W`. Sur un ecran
				// PRESQUE carre, deux colonnes donneraient au damier moins de la
				// moitie de la place qu'une colonne unique lui donne. Le critere
				// est la PROPORTION, pas l'orientation.
				//
				// ⚠️ ET LE COMMENTAIRE PRECEDENT MENTAIT : « il tient en portrait
				// comme en paysage sans deux mises en page separees ». Il tenait,
				// en effet -- minuscule, avec les deux tiers de la largeur vides.
				// Tenir n'est pas occuper.
				const bool colonneUnique = zoneW < zoneH * 1.30f;

				const float32 hBandeau = zoneH * (colonneUnique ? 0.11f : 0.14f);

				if (colonneUnique) {
					// ── UNE COLONNE : bandeau, damier, pied de page ───────────
					bandeau = NkRect{zoneX, zoneY, zoneW, hBandeau};

					const float32 dispoH = zoneH - hBandeau - marge * 2.f - hBandeau * 0.75f;
					const float32 cote = zoneW < dispoH ? zoneW : dispoH;
					const float32 xPlateau = zoneX + (zoneW - cote) * 0.5f;

					// ── EN PORTRAIT AUSSI, L'ACTION S'ANCRE EN BAS ────────────
					// Sur un telephone 1080x2400 le damier est bride par la
					// LARGEUR : plus de 600 px de hauteur restent, et un centrage
					// en bloc en faisait deux bandes vides, en haut et en bas.
					// La disposition d'un ecran de telephone est plutot : le
					// bandeau en haut, les boutons en bas SOUS LE POUCE, et le jeu
					// centre entre les deux. Meme regle qu'en paysage -- une seule
					// facon de faire, pas une par orientation.
					const float32 hBas = hBandeau * 0.72f;
					const float32 yBas = zoneY + zoneH - hBas;

					// Le damier se centre entre le bandeau et le pied de page.
					// Quand la hauteur est juste (`cote == dispoH`), il tombe dans
					// le flot d'origine : le centrage repartit, il n'invente rien.
					const float32 yDebut = bandeau.y + bandeau.h + marge;
					const float32 yFin = yBas - marge;
					float32 yPlateau = yDebut + ((yFin - yDebut) - cote) * 0.5f;
					if (yPlateau < yDebut) {
						yPlateau = yDebut;
					}
					plateau = NkRect{xPlateau, yPlateau, cote, cote};

					// Le pied de page porte TROIS boutons : les deux sieges, puis
					// « nouvelle partie ». Ils partagent la largeur du damier.
					const float32 ecart = cote * 0.02f;
					const float32 lSiege = (cote - ecart * 2.f) * 0.27f;
					siege[0] = NkRect{plateau.x, yBas, lSiege, hBas};
					siege[1] = NkRect{plateau.x + lSiege + ecart, yBas, lSiege, hBas};
					rejouer = NkRect{plateau.x + (lSiege + ecart) * 2.f, yBas, cote - (lSiege + ecart) * 2.f, hBas};
				} else {
					// ── DEUX COLONNES : le damier a gauche, le reste a droite ──
					//
					// En paysage c'est la HAUTEUR qui contraint le damier. On lui
					// donne toute la hauteur utile ; la colonne prend le reste,
					// bornee pour ne pas devenir absurde sur un ecran tres large.
					float32 cote = zoneH;
					float32 lCol = zoneW - cote - marge;
					const float32 lColMin = hBandeau * 4.2f;

					// ── LA COLONNE SE MESURE SUR SON CONTENU, PAS SUR L'ECRAN ─
					// ⚠️ C'ETAIT `zoneW * 0.42f`. Une fraction de la LARGEUR
					// grandit avec l'ecran, alors que le contenu de la colonne --
					// un bandeau et trois boutons -- ne grandit pas : sur un
					// telephone en paysage la colonne devenait presque aussi large
					// que le damier. Signale par Rodolf le 03/09 (« une fois
					// roter ce n'est pas proportionnel »), mesure faite sur
					// NkLudo : 918 px de colonne contre 1015 de plateau.
					//
					// L'unite juste est `hBandeau`, la hauteur de ligne : elle
					// suit la taille du texte, donc le contenu. Le minimum
					// ci-dessus vaut 4,2 -- les deux bornes parlent enfin la meme
					// langue.
					const float32 lColMax = hBandeau * 4.6f;
					if (lCol > lColMax) {
						lCol = lColMax;
					}
					// ⚠️ SI LA COLONNE NE TIENT PAS, C'EST LE DAMIER QUI CEDE --
					// sinon elle sortirait de l'ecran par la droite, et aucun
					// calcul ne l'aurait signale.
					if (lCol < lColMin) {
						lCol = lColMin;
						cote = zoneW - lCol - marge;
						if (cote > zoneH) {
							cote = zoneH;
						}
					}

					plateau = NkRect{zoneX, zoneY + (zoneH - cote) * 0.5f, cote, cote};

					const float32 xCol = zoneX + cote + marge;
					bandeau = NkRect{xCol, zoneY, lCol, hBandeau};

					// Les trois boutons s'empilent : en colonne etroite, trois
					// libelles cote a cote seraient tronques.
					const float32 hBas = hBandeau * 0.85f;
					const float32 ecart = hBandeau * 0.20f;
					float32 y = bandeau.y + bandeau.h + marge;
					siege[0] = NkRect{xCol, y, lCol, hBas};
					y += hBas + ecart;
					siege[1] = NkRect{xCol, y, lCol, hBas};
					y += hBas + ecart;

					// ── L'ACTION S'ANCRE EN BAS, A HAUTEUR DU damier ------
					// ⚠️ VU SUR UNE CAPTURE, PAS AU CALCUL. Empile a la suite des
					// deux sieges, le bouton laissait sous lui une bande vide en
					// bas a droite : le damier descendait jusqu'en bas, la colonne
					// s'arretait plus haut. Le banc ne le voyait pas -- il mesure
					// la boite ENGLOBANTE, que le damier remplissait deja.
					//
					// 📌 C'est la disposition habituelle d'un panneau : titre en
					// haut, liste au milieu, ACTION en bas -- et le bouton se
					// retrouve la ou le pouce l'atteint.
					// Le bas de la ZONE, pas du damier : quand le damier a cede de
					// la place a la colonne (tablette 2560x1600 : 1452 px pour 1504
					// de hauteur utile), il est centre et son bas flotte a 26 px du
					// bord. La colonne, elle, va toujours jusqu'en bas -- c'est le
					// banc qui l'a vu, pas l'oeil.
					const float32 basPlateau = zoneY + zoneH;
					float32 yAction = y;
					// Si la pile depasse le damier, on garde le flot naturel
					// plutot que de faire remonter le bouton SUR les sieges.
					if (yAction + hBas < basPlateau) {
						yAction = basPlateau - hBas;
					}
					rejouer = NkRect{xCol, yAction, lCol, hBas};
				}

				// ── CE QUI RESTE DEVIENT UNE MARGE, JAMAIS UN VIDE D'UN COTE ──
				//
				// Rodolf, 03/09 : « ca laisse du vide ». Mesure faite sur NkLudo,
				// qui portait le meme calcul : vide de 252 px A DROITE sur un
				// telephone en paysage (11,2 % de la largeur), 1020 px (29,7 %)
				// sur un ecran tres large, et 548 px EN BAS (24,3 %) en portrait.
				//
				// ⚠️ AUCUN RECTANGLE N'ETAIT FAUX -- chacun tenait dans la zone
				// sure, le damier restait carre, rien ne se recouvrait, et le banc
				// etait vert. Ce qui manquait n'est pas DANS les rectangles, c'est
				// ENTRE eux : la mise en page etait plaquee en haut a gauche et le
				// surplus s'accumulait du seul cote oppose.
				//
				// 📌 `NkCentrerDans` vit dans NKMath, pas ici : les trois jeux
				// avaient le meme defaut, et trois copies du meme correctif
				// divergent au premier ajustement.
				{
					NkRect *tous[5];
					tous[0] = &plateau;
					tous[1] = &bandeau;
					tous[2] = &siege[0];
					tous[3] = &siege[1];
					tous[4] = &rejouer;
					const NkRect dispo{gaucheSur, hautSur, W - gaucheSur - droiteSur, H - hautSur - basSur};
					math::NkCentrerDans(tous, static_cast<uint32>(sizeof(tous) / sizeof(tous[0])), dispo);
				}

				cellule = plateau.w / static_cast<float32>(NkDamesPartie::NK_TAILLE);

				// Le retour au menu vit dans le bandeau, a droite : c'est la
				// seule place qui ne bouge pas d'un ecran a l'autre.
				const float32 cRetour = bandeau.h * 0.56f;
				retour = NkRect{bandeau.x + bandeau.w - cRetour - bandeau.h * 0.22f,
								bandeau.y + (bandeau.h - cRetour) * 0.5f, cRetour, cRetour};

				// Les trois choix du menu : empiles, larges. Une cible de menu se
				// touche au pouce, elle n'a aucune raison d'etre etroite.
				//
				// ⚠️ MESURE DU 2026-09-01 : la premiere version faisait demarrer les
				// boutons a 0,42 de la hauteur MOINS un pas — ils RECOUVRAIENT le
				// titre, et le sous-titre disparaissait entierement. Le defaut ne se
				// voyait pas au calcul, il s'est vu sur la CAPTURE.
				// On ancre desormais le bloc de boutons SOUS le titre au lieu de le
				// centrer a l'aveugle : titreH + sousTitreH est une hauteur connue.
				menuTitre = NkRect{plateau.x, plateau.y + plateau.h * 0.06f, plateau.w, hBandeau * 1.1f};
				menuSousTitre = NkRect{plateau.x, menuTitre.y + menuTitre.h, plateau.w, hBandeau * 0.66f};

				const float32 lChoix = plateau.w * 0.86f;
				const float32 hChoix = hBandeau * 0.92f;
				const float32 pas = hChoix * 1.3f;
				const float32 y0 = menuSousTitre.y + menuSousTitre.h + hBandeau * 0.5f;
				for (int32 i = 0; i < 3; ++i) {
					choix[i] = NkRect{plateau.x + (plateau.w - lChoix) * 0.5f, y0 + static_cast<float32>(i) * pas, lChoix,
									  hChoix};
				}
			}

			// =====================================================================
			void DessinerFond(NkGuiDrawList &dl, const renderer::NkLayoutInfo &info) {
				dl.AddRectFilled(NkRect{0.f, 0.f, static_cast<float32>(info.width), static_cast<float32>(info.height)},
								 kFond);
			}

			// =====================================================================
			// LE MENU — on choisit le mode avant de jouer
			// =====================================================================
			void DessinerMenu(NkGuiDrawList &dl, const renderer::NkLayoutInfo &info, const NkDamesGeometrie &geo,
							  const NkDamesPolices &f) {
				(void)info;

				TexteDansBoite(dl, f.titre, geo.menuTitre, "Dames", kTexte);
				TexteDansBoite(dl, f.petite, geo.menuSousTitre, "Regles internationales, 10x10", kTexteFaible);

				const char *libelles[3] = {"Contre l'ordinateur", "A deux, meme ecran", "IA contre IA"};
				const NkColor teintes[3] = {kDestination, kSelection, kOr};
				for (int32 i = 0; i < 3; ++i) {
					dl.AddRectFilled(geo.choix[i], kPanneauActif, geo.choix[i].h * 0.26f);
					dl.AddRect(geo.choix[i], teintes[i], 2.f, geo.choix[i].h * 0.26f);
					// Une pastille de couleur a gauche : elle distingue les trois
					// entrees sans dependre de la lecture du libelle.
					dl.AddCircleFilled(
						NkVec2f(geo.choix[i].x + geo.choix[i].h * 0.5f, geo.choix[i].y + geo.choix[i].h * 0.5f),
						geo.choix[i].h * 0.16f, teintes[i]);
					TexteDansBoite(dl, f.corps,
								   NkRect{geo.choix[i].x + geo.choix[i].h * 0.9f, geo.choix[i].y,
										  geo.choix[i].w - geo.choix[i].h * 1.2f, geo.choix[i].h},
								   libelles[i], kTexte);
				}
			}

			// =====================================================================
			void DessinerBandeau(NkGuiDrawList &dl, const NkDamesGeometrie &geo, const NkDamesPolices &f,
								 const NkDamesVue &vue) {
				dl.AddRectFilled(geo.bandeau, kPanneau, geo.bandeau.h * 0.22f);
				const float32 pad = geo.bandeau.h * 0.22f;

				TexteDansBoite(dl, f.titre,
							   NkRect{geo.bandeau.x + pad, geo.bandeau.y, geo.bandeau.w * 0.5f, geo.bandeau.h * 0.55f},
							   "Dames", kTexte);

				const int32 trait = (vue.partie->Trait() == NkDamesCamp::NK_BLANC) ? 0 : 1;
				const bool humainAuTrait = vue.controleur[trait] == NkControleur::NK_HUMAIN;
				const bool deuxHumains =
					vue.controleur[0] == NkControleur::NK_HUMAIN && vue.controleur[1] == NkControleur::NK_HUMAIN;

				const char *ligne = "A vous de jouer";
				if (vue.finie) {
					ligne = "Partie terminee";
				} else if (!humainAuTrait) {
					ligne = "L'ordinateur reflechit";
				} else if (deuxHumains) {
					// A deux sur le meme ecran, "a vous" ne veut rien dire : il
					// faut nommer le camp.
					ligne = (trait == 0) ? "Aux blancs de jouer" : "Aux noirs de jouer";
				}
				Texte(dl, f.petite, geo.bandeau.x + pad, geo.bandeau.y + geo.bandeau.h * 0.56f, ligne, kTexteFaible,
					  geo.bandeau.w * 0.55f);

				const NkString compte =
					NkString::Format("%d  -  %d", vue.partie->CompterPieces(NkDamesCamp::NK_BLANC),
									 vue.partie->CompterPieces(NkDamesCamp::NK_NOIR));
				TexteADroite(dl, f.corps, geo.retour.x - pad * 0.6f,
							 geo.bandeau.y + (geo.bandeau.h - (f.corps ? f.corps->LineHeight() : 16.f)) * 0.5f,
							 compte.Data(), kTexte);

				// Retour au menu : trois barres, dessinees plutot qu'ecrites —
				// un glyphe demanderait un atlas d'icones que ce jeu n'a pas.
				dl.AddRectFilled(geo.retour, kPanneauActif, geo.retour.h * 0.26f);
				dl.AddRect(geo.retour, kBord, 1.5f, geo.retour.h * 0.26f);
				for (int32 i = 0; i < 3; ++i) {
					const float32 y = geo.retour.y + geo.retour.h * (0.32f + static_cast<float32>(i) * 0.18f);
					dl.AddRectFilled(NkRect{geo.retour.x + geo.retour.w * 0.26f, y, geo.retour.w * 0.48f,
											geo.retour.h * 0.07f},
									 kTexte, geo.retour.h * 0.035f);
				}
			}

			// =====================================================================
			void DessinerDamier(NkGuiDrawList &dl, const NkDamesGeometrie &geo, const NkDamesVue &vue) {
				const float32 bord = geo.cellule * 0.16f;
				dl.AddRectFilled(NkRect{geo.plateau.x - bord, geo.plateau.y - bord, geo.plateau.w + bord * 2.f,
										geo.plateau.h + bord * 2.f},
								 kCadre, bord);

				for (int32 r = 0; r < NkDamesPartie::NK_TAILLE; ++r) {
					for (int32 c = 0; c < NkDamesPartie::NK_TAILLE; ++c) {
						dl.AddRectFilled(geo.CaseRect(r, c),
										 NkDamesPartie::CaseJouable(r, c) ? kCaseSombre : kCaseClaire);
					}
				}

				// Les destinations AVANT les pieces : une pastille ne doit jamais
				// recouvrir un pion qu'on s'apprete a prendre.
				if (vue.coupsProposes != nullptr) {
					for (uint32 i = 0; i < vue.coupsProposes->Size(); ++i) {
						const NkDamesCoup &coup = (*vue.coupsProposes)[i];
						const NkVec2f centre = geo.CentreCase(coup.arrR, coup.arrC);
						dl.AddCircleFilled(centre, geo.cellule * (coup.EstPrise() ? 0.30f : 0.18f),
										   NkColor(kDestination.r, kDestination.g, kDestination.b, 190));
						// Une PRISE se distingue d'un simple deplacement : sans
						// cela on ne voit pas qu'on est en train de rafler.
						if (coup.EstPrise()) {
							dl.AddCircle(centre, geo.cellule * 0.38f, kDestination, geo.cellule * kTraitFin);
						}
					}
				}

				if (vue.selR >= 0) {
					dl.AddRect(geo.CaseRect(vue.selR, vue.selC), kSelection, geo.cellule * kTraitEpais);
				}
			}

			// =====================================================================
			namespace {
				/// Un pion, dessine a un centre quelconque. Extraite pour que la
				/// piece EN MOUVEMENT et les pieces posees empruntent exactement
				/// le meme dessin — deux dessins qui doivent se ressembler
				/// finissent toujours par diverger.
				void DessinerUnPion(NkGuiDrawList &dl, const NkVec2f &centre, float32 rayon, NkDamesPiece piece,
									float32 elevation) {
					const bool blanc = NkDamesEstBlanc(piece);
					// Une piece en mouvement porte une ombre plus basse et plus
					// large : c'est ce qui la fait lire comme SOULEVEE plutot que
					// glissee, et ca vaut mieux qu'une trainee.
					dl.AddCircleFilled(NkVec2f(centre.x, centre.y + rayon * (0.16f + elevation * 0.5f)),
									   rayon * (1.f + elevation * 0.25f), NkColor(0, 0, 0, 90));
					dl.AddCircleFilled(NkVec2f(centre.x, centre.y + rayon * 0.10f), rayon,
									   blanc ? kPionBlancOmbre : kPionNoirOmbre);
					dl.AddCircleFilled(centre, rayon * 0.94f, blanc ? kPionBlanc : kPionNoir);
					dl.AddCircle(centre, rayon * 0.68f, blanc ? kPionBlancTrait : kPionNoirTrait, rayon * 0.10f);
					if (NkDamesEstDame(piece)) {
						dl.AddCircle(centre, rayon * 0.44f, kOr, rayon * 0.20f);
						dl.AddCircleFilled(centre, rayon * 0.16f, kOr);
					}
				}
			} // namespace

			void DessinerPieces(NkGuiDrawList &dl, const NkDamesGeometrie &geo, const NkDamesVue &vue) {
				const float32 rayon = geo.cellule * kRayonPion;

				// La case ou la piece animee est DEJA arrivee (dans les regles) :
				// on ne l'y dessine pas, sinon elle apparait a deux endroits.
				int32 sauterR = -1, sauterC = -1;
				const bool anime = (vue.anim != nullptr && vue.anim->actif);
				if (anime) {
					vue.anim->CaseFinale(sauterR, sauterC);
				}

				for (int32 r = 0; r < NkDamesPartie::NK_TAILLE; ++r) {
					for (int32 c = 0; c < NkDamesPartie::NK_TAILLE; ++c) {
						if (anime && r == sauterR && c == sauterC) {
							continue;
						}
						const NkDamesPiece piece = vue.partie->Case(r, c);
						if (piece == NkDamesPiece::NK_VIDE) {
							continue;
						}
						const NkVec2f centre = geo.CentreCase(r, c);
						DessinerUnPion(dl, centre, rayon, piece, 0.f);
					}
				}

				// Les pieces PRISES, tant que l'attaquant ne les a pas franchies.
				// Elles palissent en s'effacant : une disparition sèche se lit
				// comme un defaut d'affichage, un fondu se lit comme une capture.
				if (anime) {
					for (uint8 i = 0; i < vue.anim->nbPrises; ++i) {
						if (!vue.anim->PriseVisible(i)) {
							continue;
						}
						const float32 avance = vue.anim->t * static_cast<float32>(vue.anim->nbEtapes);
						const float32 reste = static_cast<float32>(i) + 0.55f - avance;
						const float32 opacite = reste > 0.30f ? 1.f : (reste / 0.30f);
						const NkVec2f centre = geo.CentreCase(vue.anim->prisR[i], vue.anim->prisC[i]);
						const NkDamesPiece pp = vue.anim->prisPiece[i];
						const bool b = NkDamesEstBlanc(pp);
						const uint8 a = static_cast<uint8>(opacite * 255.f);
						dl.AddCircleFilled(NkVec2f(centre.x, centre.y + rayon * 0.10f), rayon * (0.6f + 0.4f * opacite),
										   NkColor(b ? kPionBlancOmbre.r : kPionNoirOmbre.r,
												   b ? kPionBlancOmbre.g : kPionNoirOmbre.g,
												   b ? kPionBlancOmbre.b : kPionNoirOmbre.b, a));
						dl.AddCircleFilled(centre, rayon * 0.94f * (0.6f + 0.4f * opacite),
										   NkColor(b ? kPionBlanc.r : kPionNoir.r, b ? kPionBlanc.g : kPionNoir.g,
												   b ? kPionBlanc.b : kPionNoir.b, a));
					}
				}

				// La piece en mouvement passe EN DERNIER : elle doit survoler les
				// autres, jamais passer dessous.
				if (anime) {
					float32 fr = 0.f, fc = 0.f;
					vue.anim->Position(fr, fc);
					const NkVec2f centre(geo.plateau.x + (fc + 0.5f) * geo.cellule,
										 geo.plateau.y + (fr + 0.5f) * geo.cellule);
					// Elle se souleve puis se repose : une cloche sur la duree.
					const float32 t = vue.anim->t;
					const float32 cloche = 4.f * t * (1.f - t);
					DessinerUnPion(dl, NkVec2f(centre.x, centre.y - geo.cellule * 0.10f * cloche), rayon * (1.f + 0.06f * cloche),
								   vue.anim->piece, cloche);
				}
			}

			// =====================================================================
			namespace {
				/// Un bouton de siege dit DEUX choses d'un coup d'oeil : de quel
				/// camp il s'agit, et qui le tient. Un bouton qui n'afficherait
				/// que "IA" obligerait a se souvenir de quel camp il parle.
				void DessinerSiege(NkGuiDrawList &dl, const NkRect &box, NkGuiFont *petite, const char *nom,
								   NkControleur qui, bool auTrait, bool seulHumain, const NkColor &teinte) {
					const bool ia = (qui == NkControleur::NK_IA);
					dl.AddRectFilled(box, ia ? kPanneau : kPanneauActif, box.h * 0.28f);
					// Le siege AU TRAIT se souligne : c'est l'information qu'on
					// cherche en permanence quand deux humains jouent.
					dl.AddRect(box, auTrait ? kSelection : kBord, auTrait ? 2.5f : 1.5f, box.h * 0.28f);
					dl.AddCircleFilled(NkVec2f(box.x + box.h * 0.40f, box.y + box.h * 0.5f), box.h * 0.19f, teinte);

					// ── « VOUS » N'A DE SENS QUE S'IL N'Y A QU'UN SEUL HUMAIN ─
					// Vu sur une capture : « Blancs vous » et « Noirs vous », deux
					// personnes autour du meme ecran designees toutes deux par le
					// meme mot. A deux, c'est le camp qui nomme le siege ; face a
					// l'IA, « vous » dit enfin quelque chose -- et le tiret rend
					// lisible ce que l'espace seule ne separait pas.
					const NkString libelle = ia ? NkString::Format("%s — IA", nom)
											 : (seulHumain ? NkString::Format("%s — vous", nom) : NkString(nom));
					TexteDansBoite(dl, petite, NkRect{box.x + box.h * 0.66f, box.y, box.w - box.h * 0.72f, box.h},
								   libelle.Data(), ia ? kTexteFaible : kTexte);
				}
			} // namespace

			void DessinerPiedDePage(NkGuiDrawList &dl, const NkDamesGeometrie &geo, const NkDamesPolices &f,
									const NkDamesVue &vue) {
				const int32 trait = (vue.partie->Trait() == NkDamesCamp::NK_BLANC) ? 0 : 1;
				// « seul humain » = l'autre siege est tenu par l'IA.
				const bool ia0 = vue.controleur[0] == NkControleur::NK_IA;
				const bool ia1 = vue.controleur[1] == NkControleur::NK_IA;
				DessinerSiege(dl, geo.siege[0], f.petite, "Blancs", vue.controleur[0], !vue.finie && trait == 0, ia1,
							  kPionBlanc);
				DessinerSiege(dl, geo.siege[1], f.petite, "Noirs", vue.controleur[1], !vue.finie && trait == 1, ia0,
							  NkColor(120, 124, 140));

				dl.AddRectFilled(geo.rejouer, kPanneau, geo.rejouer.h * 0.28f);
				dl.AddRect(geo.rejouer, kBord, 1.5f, geo.rejouer.h * 0.28f);
				TexteDansBoite(dl, f.corps, geo.rejouer, "Nouvelle partie", kTexte);
			}

			// =====================================================================
			void DessinerFin(NkGuiDrawList &dl, const renderer::NkLayoutInfo &info, const NkDamesPolices &f,
							 const NkDamesVue &vue) {
				const float32 W = static_cast<float32>(info.width);
				const float32 H = static_cast<float32>(info.height);
				dl.AddRectFilled(NkRect{0.f, 0.f, W, H}, kVoile);

				const NkRect panneau{W * 0.10f, H * 0.36f, W * 0.80f, H * 0.24f};
				dl.AddRectFilled(panneau, kPanneau, panneau.h * 0.14f);
				dl.AddRect(panneau, kOr, 2.f, panneau.h * 0.14f);

				// Le libelle depend de QUI tenait le camp gagnant : "vous avez
				// gagne" n'a aucun sens dans une simulation IA contre IA.
				const int32 idxGagnant = (vue.gagnant == NkDamesCamp::NK_BLANC) ? 0 : 1;
				const char *nom = (idxGagnant == 0) ? "Les blancs gagnent" : "Les noirs gagnent";
				if (vue.controleur[idxGagnant] == NkControleur::NK_HUMAIN &&
					vue.controleur[1 - idxGagnant] == NkControleur::NK_IA) {
					nom = "Vous avez gagne";
				} else if (vue.controleur[idxGagnant] == NkControleur::NK_IA &&
						   vue.controleur[1 - idxGagnant] == NkControleur::NK_HUMAIN) {
					nom = "L'ordinateur gagne";
				}

				TexteDansBoite(dl, f.titre, NkRect{panneau.x, panneau.y, panneau.w, panneau.h * 0.62f}, nom, kTexte);
				TexteDansBoite(dl, f.petite,
							   NkRect{panneau.x, panneau.y + panneau.h * 0.58f, panneau.w, panneau.h * 0.4f},
							   "Touchez pour rejouer", kTexteFaible);
			}

		} // namespace dames
	} // namespace jeux
} // namespace nkentseu
