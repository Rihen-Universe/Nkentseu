// -----------------------------------------------------------------------------
// FICHIER: Echecs/NkEchecsEcran.cpp
// DESCRIPTION: Geometrie et dessin. Ne decide rien, ne modifie rien.
//
// LA REGLE DE CE FICHIER : aucune decision de jeu. Le jour ou une fonction d ici
// veut appeler CoupsLegaux, c est qu elle est au mauvais endroit.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Echecs/NkEchecsEcran.h"
#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace jeux {
		namespace echecs {

			namespace {
				// Aides de texte : `topY` est le HAUT du texte, pas sa ligne de base.
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
				void TexteDansBoite(NkGuiDrawList &dl, NkGuiFont *f, const NkRect &box, const char *s, const NkColor &c) {
					renderer::NkTexteDansBoite(dl, f, box, s, c);
				}
			} // namespace

			bool NkDansRect(const NkRect &r, const NkVec2f &p) noexcept {
				return p.x >= r.x && p.y >= r.y && p.x < r.x + r.w && p.y < r.y + r.h;
			}

			bool NkEchecsGeometrie::CaseSous(const NkVec2f &p, int32 &r, int32 &c) const noexcept {
				if (cellule <= 0.f || p.x < plateau.x || p.y < plateau.y || p.x >= plateau.x + plateau.w ||
					p.y >= plateau.y + plateau.h) {
					return false;
				}
				c = static_cast<int32>((p.x - plateau.x) / cellule);
				r = static_cast<int32>((p.y - plateau.y) / cellule);
				return NkEchecsPartie::DansDamier(r, c);
			}

			// =====================================================================
			void NkEchecsGeometrie::Calculer(const renderer::NkLayoutInfo &info) noexcept {
				const float32 W = static_cast<float32>(info.width);
				const float32 H = static_cast<float32>(info.height);

				// ⚠️ La zone sure n'est PAS decorative : sous l'encoche ou
				// l'indicateur de geste, un bouton devient INATTEIGNABLE.
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
					// que l'echiquier. Signale par Rodolf le 03/09 (« une fois
					// roter ce n'est pas proportionnel »), mesure sur NkLudo :
					// 918 px de colonne contre 1015 de plateau.
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

					// ── L'ACTION S'ANCRE EN BAS, A HAUTEUR DU echiquier ------
					// ⚠️ VU SUR UNE CAPTURE, PAS AU CALCUL. Empile a la suite des
					// deux sieges, le bouton laissait sous lui une bande vide en
					// bas a droite : le echiquier descendait jusqu'en bas, la colonne
					// s'arretait plus haut. Le banc ne le voyait pas -- il mesure
					// la boite ENGLOBANTE, que le echiquier remplissait deja.
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
					// Si la pile depasse le echiquier, on garde le flot naturel
					// plutot que de faire remonter le bouton SUR les sieges.
					if (yAction + hBas < basPlateau) {
						yAction = basPlateau - hBas;
					}
					rejouer = NkRect{xCol, yAction, lCol, hBas};
				}

				// ── CE QUI RESTE DEVIENT UNE MARGE, JAMAIS UN VIDE D'UN COTE ──
				//
				// Rodolf, 03/09 : « ca laisse du vide ». Mesure faite sur
				// NkLudo, qui portait le meme calcul : vide de 252 px A DROITE sur
				// un telephone en paysage (11,2 % de la largeur), 1020 px (29,7 %)
				// sur un ecran tres large, et 548 px EN BAS (24,3 %) en portrait.
				//
				// ⚠️ AUCUN RECTANGLE N'ETAIT FAUX -- chacun tenait dans la zone
				// sure, l'echiquier restait carre, rien ne se recouvrait, et le
				// banc etait vert. Ce qui manquait n'est pas DANS les rectangles,
				// c'est ENTRE eux : la mise en page etait plaquee en haut a gauche
				// et le surplus s'accumulait du seul cote oppose.
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

				cellule = plateau.w / static_cast<float32>(NkEchecsPartie::NK_TAILLE);

				// Le retour au menu vit dans le bandeau, a droite : la seule place
				// qui ne bouge pas d'un ecran a l'autre.
				const float32 cRetour = bandeau.h * 0.56f;
				retour = NkRect{bandeau.x + bandeau.w - cRetour - bandeau.h * 0.22f,
				                bandeau.y + (bandeau.h - cRetour) * 0.5f, cRetour, cRetour};

				// ⚠️ Les boutons du menu s'ancrent SOUS le titre, ils ne se centrent
				// pas a l'aveugle : sur NkDames, la version centree les faisait
				// RECOUVRIR le titre — defaut invisible au calcul, vu sur la CAPTURE.
				menuTitre = NkRect{plateau.x, plateau.y + plateau.h * 0.06f, plateau.w, hBandeau * 1.1f};
				menuSousTitre = NkRect{plateau.x, menuTitre.y + menuTitre.h, plateau.w, hBandeau * 0.66f};

				const float32 lChoix = plateau.w * 0.86f;
				const float32 hChoix = hBandeau * 0.92f;
				const float32 pasChoix = hChoix * 1.3f;
				const float32 y0 = menuSousTitre.y + menuSousTitre.h + hBandeau * 0.5f;
				for (int32 i = 0; i < 3; ++i) {
					choix[i] = NkRect{plateau.x + (plateau.w - lChoix) * 0.5f, y0 + static_cast<float32>(i) * pasChoix,
					                  lChoix, hChoix};
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
			void DessinerMenu(NkGuiDrawList &dl, const NkEchecsGeometrie &geo, const NkEchecsPolices &f) {
				TexteDansBoite(dl, f.titre, geo.menuTitre, "Echecs", kTexte);
				TexteDansBoite(dl, f.petite, geo.menuSousTitre, "Roque, prise en passant, promotion", kTexteFaible);

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

			void DessinerBandeau(NkGuiDrawList &dl, const NkEchecsGeometrie &geo, const NkEchecsPolices &f,
									 const NkEchecsVue &vue) {
				dl.AddRectFilled(geo.bandeau, kPanneau, geo.bandeau.h * 0.22f);
				const float32 pad = geo.bandeau.h * 0.22f;
				TexteDansBoite(dl, f.titre,
								   NkRect{geo.bandeau.x + pad, geo.bandeau.y, geo.bandeau.w * 0.5f, geo.bandeau.h * 0.55f},
								   "Echecs", kTexte);

				const int32 trait = (vue.partie->Trait() == NkEchecsCamp::NK_BLANC) ? 0 : 1;
				const bool humainAuTrait = vue.controleur[trait] == NkControleur::NK_HUMAIN;
				const bool deuxHumains =
					vue.controleur[0] == NkControleur::NK_HUMAIN && vue.controleur[1] == NkControleur::NK_HUMAIN;

				const char *ligne = "A vous de jouer";
				if (vue.finie) {
					ligne = "Partie terminee";
				} else if (vue.etat == NkEchecsEtat::NK_ECHEC) {
					ligne = "Echec au roi";
				} else if (!humainAuTrait) {
					ligne = "L'ordinateur reflechit";
				} else if (deuxHumains) {
					// A deux sur le meme ecran, "a vous" ne veut rien dire.
					ligne = (trait == 0) ? "Aux blancs de jouer" : "Aux noirs de jouer";
				}
				Texte(dl, f.petite, geo.bandeau.x + pad, geo.bandeau.y + geo.bandeau.h * 0.56f, ligne,
						  vue.etat == NkEchecsEtat::NK_ECHEC ? kAlerte : kTexteFaible, geo.bandeau.w * 0.62f);

				// Retour au menu : trois barres, dessinees plutot qu'ecrites — un
				// glyphe demanderait un atlas d'icones que ce jeu n'a pas.
				dl.AddRectFilled(geo.retour, kPanneauActif, geo.retour.h * 0.26f);
				dl.AddRect(geo.retour, kBord, 1.5f, geo.retour.h * 0.26f);
				for (int32 i = 0; i < 3; ++i) {
					const float32 yb = geo.retour.y + geo.retour.h * (0.32f + static_cast<float32>(i) * 0.18f);
					dl.AddRectFilled(
						NkRect{geo.retour.x + geo.retour.w * 0.26f, yb, geo.retour.w * 0.48f, geo.retour.h * 0.07f},
						kTexte, geo.retour.h * 0.035f);
				}
			}

			// =====================================================================
			void DessinerDamier(NkGuiDrawList &dl, const NkEchecsGeometrie &geo, const NkEchecsVue &vue) {
				const float32 bord = geo.cellule * 0.16f;
				dl.AddRectFilled(NkRect{geo.plateau.x - bord, geo.plateau.y - bord, geo.plateau.w + bord * 2.f,
												geo.plateau.h + bord * 2.f},
									 kCadre, bord);

				for (int32 r = 0; r < NkEchecsPartie::NK_TAILLE; ++r) {
					for (int32 c = 0; c < NkEchecsPartie::NK_TAILLE; ++c) {
						dl.AddRectFilled(geo.CaseRect(r, c), ((r + c) & 1) ? kCaseSombre : kCaseClaire);
					}
				}

				// Le roi en echec se signale : sans cela, on cherche pourquoi les
				// coups proposes sont si peu nombreux.
				if (vue.etat == NkEchecsEtat::NK_ECHEC || vue.etat == NkEchecsEtat::NK_ECHEC_ET_MAT) {
					const NkEchecsPiece roi = vue.partie->Trait() == NkEchecsCamp::NK_BLANC ? NkEchecsPiece::NK_ROI_B
																								   : NkEchecsPiece::NK_ROI_N;
					for (int32 r = 0; r < 8; ++r) {
						for (int32 c = 0; c < 8; ++c) {
							if (vue.partie->Case(r, c) == roi) {
								dl.AddRectFilled(geo.CaseRect(r, c), NkColor(kAlerte.r, kAlerte.g, kAlerte.b, 120));
							}
						}
					}
				}

				if (vue.coupsProposes != nullptr) {
					for (uint32 i = 0; i < vue.coupsProposes->Size(); ++i) {
						const NkEchecsCoup &coup = (*vue.coupsProposes)[i];
						const NkVec2f centre = geo.CentreCase(coup.arrR, coup.arrC);
						const bool prise =
							vue.partie->Case(coup.arrR, coup.arrC) != NkEchecsPiece::NK_VIDE || coup.enPassant;
						if (prise) {
							// Un ANNEAU autour de la piece a prendre : une pastille
							// pleine la cacherait, et on ne saurait pas quoi on prend.
							dl.AddCircle(centre, geo.cellule * 0.42f, kDestination, geo.cellule * 0.07f);
						} else {
							dl.AddCircleFilled(centre, geo.cellule * 0.16f,
														   NkColor(kDestination.r, kDestination.g, kDestination.b, 190));
						}
					}
				}

				if (vue.selR >= 0) {
					dl.AddRect(geo.CaseRect(vue.selR, vue.selC), kSelection, geo.cellule * 0.07f);
				}
			}

			// =====================================================================
			void DessinerPieces(NkGuiDrawList &dl, const NkEchecsGeometrie &geo, const NkEchecsVue &vue) {
				const bool anime = (vue.anim != nullptr && vue.anim->actif);

				for (int32 r = 0; r < NkEchecsPartie::NK_TAILLE; ++r) {
					for (int32 c = 0; c < NkEchecsPartie::NK_TAILLE; ++c) {
						// La piece animee est DEJA arrivee dans les regles : on ne
						// l'y dessine pas, sinon elle apparait a deux endroits.
						if (anime && vue.anim->EstArrivee(r, c)) {
							continue;
						}
						const NkEchecsPiece p = vue.partie->Case(r, c);
						if (p == NkEchecsPiece::NK_VIDE) {
							continue;
						}
						DessinerPiece(dl, geo.CentreCase(r, c), geo.cellule, NkEchecsType(p), NkEchecsEstBlanc(p));
					}
				}

				// La piece PRISE reste visible jusqu'a l'arrivee de l'attaquant,
				// en palissant : une disparition sèche se lit comme un defaut
				// d'affichage, un fondu se lit comme une prise.
				if (anime && vue.anim->prise != NkEchecsPiece::NK_VIDE && vue.anim->priseR >= 0) {
					const float32 t = vue.anim->t < 0.f ? 0.f : (vue.anim->t > 1.f ? 1.f : vue.anim->t);
					if (t < 0.92f) {
						DessinerPiece(dl, geo.CentreCase(vue.anim->priseR, vue.anim->priseC), geo.cellule,
									  NkEchecsType(vue.anim->prise), NkEchecsEstBlanc(vue.anim->prise));
					}
				}

				// Les pieces en mouvement passent EN DERNIER : elles survolent les
				// autres, jamais l'inverse.
				if (anime) {
					// Adoucissement aux deux bouts : un deplacement lineaire
					// demarre et s'arrete sec, ce qui se lit comme un saut.
					const float32 t = vue.anim->t < 0.f ? 0.f : (vue.anim->t > 1.f ? 1.f : vue.anim->t);
					const float32 e = t * t * (3.f - 2.f * t);
					const float32 cloche = 4.f * t * (1.f - t);
					for (uint8 i = 0; i < vue.anim->nb; ++i) {
						const NkEchecsAnim::Mouvement &m = vue.anim->mvt[i];
						const float32 fr = static_cast<float32>(m.depR) +
										   (static_cast<float32>(m.arrR) - static_cast<float32>(m.depR)) * e;
						const float32 fc = static_cast<float32>(m.depC) +
										   (static_cast<float32>(m.arrC) - static_cast<float32>(m.depC)) * e;
						const NkVec2f centre(geo.plateau.x + (fc + 0.5f) * geo.cellule,
											 geo.plateau.y + (fr + 0.5f) * geo.cellule - geo.cellule * 0.08f * cloche);
						DessinerPiece(dl, centre, geo.cellule, NkEchecsType(m.piece), NkEchecsEstBlanc(m.piece));
					}
				}
			}
		void DessinerPiece(NkGuiDrawList &dl, const NkVec2f &centre, float32 cell, uint8 type, bool blanc) {
			const NkColor corps = blanc ? kBlanc : kNoir;
			const NkColor trait = blanc ? kBlancTrait : kNoirTrait;
			const float32 u = cell * 0.5f; // demi-cellule : l'unite de dessin
			const float32 epais = cell * 0.045f;

			// Ombre portee commune : elle decolle la piece de la case.
			dl.AddCircleFilled(NkVec2f(centre.x, centre.y + u * 0.62f), u * 0.42f, NkColor(0, 0, 0, 70));

			// Socle commun a toutes les pieces.
			// ⚠️ MESURE DU 2026-09-01 : le socle faisait u*0.22 de haut, soit
			// ~5,7 px pour une cellule de 52 px, avec un contour de ~2,3 px de
			// CHAQUE cote. Le contour mangeait donc presque tout le socle et les
			// pieces NOIRES avaient une base GRIS CLAIR — l'inverse de leur
			// couleur. Un contour ne se dimensionne pas independamment de ce
			// qu'il entoure.
			// CORRECTION 2 du meme jour : agrandir le socle n'a pas suffi. La
			// cause n'etait pas la taille mais la COULEUR — `trait` est CLAIR
			// pour les pieces noires (c'est un liseré de contraste, voulu sur le
			// corps). Cerner une bande de 8 px d'un liseré clair fait lire toute
			// la bande comme claire. Un socle porte la MEME matiere que son
			// corps ; son epaisseur se rend par une ombre, pas par un contour.
			const NkColor ombreSocle = blanc ? kBlancTrait : NkColor(22, 20, 26);
			const NkRect socle{centre.x - u * 0.46f, centre.y + u * 0.34f, u * 0.92f, u * 0.32f};
			dl.AddRectFilled(socle, corps, u * 0.08f);
			dl.AddRectFilled(NkRect{socle.x, socle.y + socle.h * 0.58f, socle.w, socle.h * 0.42f}, ombreSocle,
							 u * 0.07f);

			switch (type) {
				case 1: { // PION — une boule sur un col
					dl.AddRectFilled(NkRect{centre.x - u * 0.16f, centre.y - u * 0.05f, u * 0.32f, u * 0.48f},
									 corps, u * 0.08f);
					dl.AddCircleFilled(NkVec2f(centre.x, centre.y - u * 0.22f), u * 0.26f, corps);
					dl.AddCircle(NkVec2f(centre.x, centre.y - u * 0.22f), u * 0.26f, trait, epais);
					break;
				}
				case 2: { // CAVALIER — une tete tournee, silhouette unique
					const NkVec2f pts[6] = {
						NkVec2f(centre.x - u * 0.30f, centre.y + u * 0.40f),
						NkVec2f(centre.x - u * 0.24f, centre.y - u * 0.10f),
						NkVec2f(centre.x - u * 0.42f, centre.y - u * 0.30f),
						NkVec2f(centre.x - u * 0.02f, centre.y - u * 0.62f),
						NkVec2f(centre.x + u * 0.36f, centre.y - u * 0.24f),
						NkVec2f(centre.x + u * 0.28f, centre.y + u * 0.40f)};
					dl.AddConvexPolyFilled(pts, 6, corps);
					dl.AddPolyline(pts, 6, trait, epais, true);
					dl.AddCircleFilled(NkVec2f(centre.x + u * 0.10f, centre.y - u * 0.34f), u * 0.06f, trait);
					break;
				}
				case 3: { // FOU — une mitre pointue, fendue
					const NkVec2f pts[5] = {NkVec2f(centre.x - u * 0.26f, centre.y + u * 0.40f),
											NkVec2f(centre.x - u * 0.22f, centre.y - u * 0.10f),
											NkVec2f(centre.x, centre.y - u * 0.66f),
											NkVec2f(centre.x + u * 0.22f, centre.y - u * 0.10f),
											NkVec2f(centre.x + u * 0.26f, centre.y + u * 0.40f)};
					dl.AddConvexPolyFilled(pts, 5, corps);
					dl.AddPolyline(pts, 5, trait, epais, true);
					dl.AddLine(NkVec2f(centre.x - u * 0.10f, centre.y - u * 0.34f),
							   NkVec2f(centre.x + u * 0.12f, centre.y - u * 0.14f), trait, epais);
					break;
				}
				case 4: { // TOUR — carree, avec ses creneaux
					dl.AddRectFilled(NkRect{centre.x - u * 0.32f, centre.y - u * 0.28f, u * 0.64f, u * 0.70f},
									 corps, u * 0.05f);
					dl.AddRect(NkRect{centre.x - u * 0.32f, centre.y - u * 0.28f, u * 0.64f, u * 0.70f}, trait,
							   epais, u * 0.05f);
					for (int32 i = 0; i < 3; ++i) {
						const float32 x = centre.x - u * 0.34f + static_cast<float32>(i) * u * 0.24f;
						const NkRect cr{x, centre.y - u * 0.52f, u * 0.20f, u * 0.26f};
						dl.AddRectFilled(cr, corps);
						dl.AddRect(cr, trait, epais);
					}
					break;
				}
				case 5: { // DAME — couronne a cinq pointes
					dl.AddRectFilled(NkRect{centre.x - u * 0.28f, centre.y - u * 0.06f, u * 0.56f, u * 0.48f},
									 corps, u * 0.06f);
					const NkVec2f pts[7] = {NkVec2f(centre.x - u * 0.36f, centre.y - u * 0.04f),
											NkVec2f(centre.x - u * 0.30f, centre.y - u * 0.52f),
											NkVec2f(centre.x - u * 0.15f, centre.y - u * 0.22f),
											NkVec2f(centre.x, centre.y - u * 0.62f),
											NkVec2f(centre.x + u * 0.15f, centre.y - u * 0.22f),
											NkVec2f(centre.x + u * 0.30f, centre.y - u * 0.52f),
											NkVec2f(centre.x + u * 0.36f, centre.y - u * 0.04f)};
					// ⚠️ Une couronne est CONCAVE : AddConvexPolyFilled la
					// rendrait fausse. On la remplit en eventail depuis le bas.
					for (int32 i = 0; i < 6; ++i) {
						dl.AddTriangleFilled(NkVec2f(centre.x, centre.y - u * 0.02f), pts[i], pts[i + 1], corps);
					}
					dl.AddPolyline(pts, 7, trait, epais, false);
					break;
				}
				default: { // ROI — la croix, reconnaissable entre toutes
					dl.AddRectFilled(NkRect{centre.x - u * 0.28f, centre.y - u * 0.10f, u * 0.56f, u * 0.52f},
									 corps, u * 0.06f);
					dl.AddRect(NkRect{centre.x - u * 0.28f, centre.y - u * 0.10f, u * 0.56f, u * 0.52f}, trait,
							   epais, u * 0.06f);
					dl.AddRectFilled(NkRect{centre.x - u * 0.07f, centre.y - u * 0.68f, u * 0.14f, u * 0.44f},
									 corps);
					dl.AddRectFilled(NkRect{centre.x - u * 0.22f, centre.y - u * 0.54f, u * 0.44f, u * 0.14f},
									 corps);
					dl.AddRect(NkRect{centre.x - u * 0.07f, centre.y - u * 0.68f, u * 0.14f, u * 0.44f}, trait,
							   epais * 0.8f);
					break;
				}
			}
		}

			// =====================================================================
			namespace {
				/// Un bouton de siege dit DEUX choses : de quel camp il s'agit, et
				/// qui le tient. Un bouton qui n'afficherait que "IA" obligerait a
				/// se souvenir de quel camp il parle.
				void DessinerSiege(NkGuiDrawList &dl, const NkRect &box, NkGuiFont *petite, const char *nom,
								   NkControleur qui, bool auTrait, bool seulHumain, const NkColor &teinte) {
					const bool ia = (qui == NkControleur::NK_IA);
					dl.AddRectFilled(box, ia ? kPanneau : kPanneauActif, box.h * 0.28f);
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

			void DessinerPiedDePage(NkGuiDrawList &dl, const NkEchecsGeometrie &geo, const NkEchecsPolices &f,
										const NkEchecsVue &vue) {
				const int32 trait = (vue.partie->Trait() == NkEchecsCamp::NK_BLANC) ? 0 : 1;
				// « seul humain » = l'autre siege est tenu par l'IA.
				const bool ia0 = vue.controleur[0] == NkControleur::NK_IA;
				const bool ia1 = vue.controleur[1] == NkControleur::NK_IA;
				DessinerSiege(dl, geo.siege[0], f.petite, "Blancs", vue.controleur[0], !vue.finie && trait == 0, ia1, kBlanc);
				DessinerSiege(dl, geo.siege[1], f.petite, "Noirs", vue.controleur[1], !vue.finie && trait == 1, ia0,
								  NkColor(120, 124, 140));

				dl.AddRectFilled(geo.rejouer, kPanneau, geo.rejouer.h * 0.28f);
				dl.AddRect(geo.rejouer, kBord, 1.5f, geo.rejouer.h * 0.28f);
				TexteDansBoite(dl, f.corps, geo.rejouer, "Nouvelle partie", kTexte);
			}

			// =====================================================================
			void DessinerFin(NkGuiDrawList &dl, const renderer::NkLayoutInfo &info, const NkEchecsPolices &f,
									 const NkEchecsVue &vue) {
				const float32 W = static_cast<float32>(info.width);
				const float32 H = static_cast<float32>(info.height);
				dl.AddRectFilled(NkRect{0.f, 0.f, W, H}, kVoile);

				const NkRect panneau{W * 0.07f, H * 0.36f, W * 0.86f, H * 0.24f};
				dl.AddRectFilled(panneau, kPanneau, panneau.h * 0.14f);
				dl.AddRect(panneau, kOr, 2.f, panneau.h * 0.14f);

				// Le libelle depend de QUI tenait le camp gagnant : "vous perdez" n'a
				// aucun sens dans une simulation IA contre IA.
				const char *titre = "Partie nulle";
				if (vue.etat == NkEchecsEtat::NK_ECHEC_ET_MAT) {
					const int32 perdant = (vue.partie->Trait() == NkEchecsCamp::NK_BLANC) ? 0 : 1;
					const int32 gagnant = 1 - perdant;
					if (vue.controleur[gagnant] == NkControleur::NK_HUMAIN &&
						vue.controleur[perdant] == NkControleur::NK_IA) {
						titre = "Echec et mat — vous gagnez";
					} else if (vue.controleur[gagnant] == NkControleur::NK_IA &&
							   vue.controleur[perdant] == NkControleur::NK_HUMAIN) {
						titre = "Echec et mat — vous perdez";
					} else {
						titre = (gagnant == 0) ? "Mat — les blancs gagnent" : "Mat — les noirs gagnent";
					}
				} else if (vue.etat == NkEchecsEtat::NK_PAT) {
					titre = "Pat — partie nulle";
				} else if (vue.etat == NkEchecsEtat::NK_NULLE_MATERIEL) {
					titre = "Nulle — materiel insuffisant";
				} else if (vue.etat == NkEchecsEtat::NK_NULLE_50_COUPS) {
					titre = "Nulle — regle des 50 coups";
				}
				TexteDansBoite(dl, f.corps, NkRect{panneau.x, panneau.y, panneau.w, panneau.h * 0.62f}, titre, kTexte);
				TexteDansBoite(dl, f.petite,
								   NkRect{panneau.x, panneau.y + panneau.h * 0.58f, panneau.w, panneau.h * 0.4f},
								   "Touchez pour rejouer", kTexteFaible);
			}

		} // namespace echecs
	} // namespace jeux
} // namespace nkentseu
