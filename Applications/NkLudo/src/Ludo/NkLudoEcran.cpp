// -----------------------------------------------------------------------------
// FICHIER: Ludo/NkLudoEcran.cpp
// DESCRIPTION: Geometrie d'ecran et dessin. Ne decide rien, ne modifie rien.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Ludo/NkLudoEcran.h"
#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace jeux {
		namespace ludo {

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
				void TexteDansBoite(NkGuiDrawList &dl, NkGuiFont *f, const NkRect &box, const char *s,
									const NkColor &c) {
					renderer::NkTexteDansBoite(dl, f, box, s, c);
				}

				/// Un de avec de VRAIS points, pas un chiffre : on lit un de d'un
				/// coup d'oeil, on dechiffre un chiffre.
				void DessinerDe(NkGuiDrawList &dl, const NkRect &box, int32 valeur) {
					dl.AddRectFilled(box, NkColor(240, 238, 232), box.h * 0.18f);
					dl.AddRect(box, NkColor(120, 116, 110), 1.5f, box.h * 0.18f);
					if (valeur < 1 || valeur > 6) {
						return; // pas encore lance : la face reste vide
					}
					const float32 r = box.h * 0.09f;
					const float32 g = box.x + box.w * 0.27f;
					const float32 m = box.x + box.w * 0.5f;
					const float32 d = box.x + box.w * 0.73f;
					const float32 h = box.y + box.h * 0.27f;
					const float32 c = box.y + box.h * 0.5f;
					const float32 b = box.y + box.h * 0.73f;
					const NkColor pt(40, 38, 36);
					auto point = [&](float32 x, float32 y) { dl.AddCircleFilled(NkVec2f(x, y), r, pt); };
					if (valeur == 1 || valeur == 3 || valeur == 5) {
						point(m, c);
					}
					if (valeur >= 2) {
						point(g, h);
						point(d, b);
					}
					if (valeur >= 4) {
						point(d, h);
						point(g, b);
					}
					if (valeur == 6) {
						point(g, c);
						point(d, c);
					}
				}
			} // namespace

			// =====================================================================
			void NkLudoAnim::Position(float32 &l, float32 &c) const noexcept {
				if (nbCases == 0) {
					l = 0.f;
					c = 0.f;
					return;
				}
				if (nbCases == 1) {
					l = static_cast<float32>(ligne[0]);
					c = static_cast<float32>(colonne[0]);
					return;
				}
				const float32 tt = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
				const int32 segments = nbCases - 1;
				const float32 pos = tt * static_cast<float32>(segments);
				int32 seg = static_cast<int32>(pos);
				if (seg >= segments) {
					seg = segments - 1;
				}
				const float32 u = pos - static_cast<float32>(seg);
				// Pas d'adoucissement PAR SEGMENT : le pion doit avancer d'un pas
				// regulier de case en case, comme une main qui le deplace. Un
				// adoucissement a chaque case le ferait hesiter six fois.
				l = static_cast<float32>(ligne[seg]) +
					(static_cast<float32>(ligne[seg + 1]) - static_cast<float32>(ligne[seg])) * u;
				c = static_cast<float32>(colonne[seg]) +
					(static_cast<float32>(colonne[seg + 1]) - static_cast<float32>(colonne[seg])) * u;
			}

			// =====================================================================
			bool NkDansRect(const NkRect &r, const NkVec2f &p) noexcept {
				return p.x >= r.x && p.y >= r.y && p.x < r.x + r.w && p.y < r.y + r.h;
			}

			void NkPositionPion(int32 joueur, int32 pion, int32 avancement, int32 &ligne, int32 &colonne) noexcept {
				if (avancement == NK_LUDO_ECURIE) {
					NkLudoGeometrieEcurie(joueur, pion, ligne, colonne);
				} else if (avancement >= NK_LUDO_ARRIVEE) {
					// Rentre : on le pose sur la derniere case de sa colonne.
					NkLudoGeometrieMaison(joueur, NK_LUDO_MAISON - 1, ligne, colonne);
				} else if (avancement >= 51) {
					NkLudoGeometrieMaison(joueur, avancement - 51, ligne, colonne);
				} else {
					NkLudoGeometriePiste(NkLudoCasePiste(joueur, avancement), ligne, colonne);
				}
			}

			// =====================================================================
			void NkLudoGeometrie::Calculer(const renderer::NkLayoutInfo &info) noexcept {
				const float32 W = static_cast<float32>(info.width);
				const float32 H = static_cast<float32>(info.height);
				const float32 hautSur = info.safeArea.top;
				const float32 basSur = info.safeArea.bottom;
				const float32 gaucheSur = info.safeArea.left;
				const float32 droiteSur = info.safeArea.right;

				const float32 marge = W * 0.03f;
				const float32 hBandeau = (H - hautSur - basSur) * (info.IsPortrait() ? 0.11f : 0.14f);
				bandeau = NkRect{gaucheSur + marge, hautSur + marge, W - gaucheSur - droiteSur - marge * 2.f, hBandeau};

				// Le plateau est CARRE : il prend la plus petite des deux places.
				// On reserve DEUX bandes en bas : les sieges, puis le bouton.
				const float32 dispoW = W - gaucheSur - droiteSur - marge * 2.f;
				const float32 dispoH = H - hautSur - basSur - hBandeau - marge * 4.f - hBandeau * 1.6f;
				const float32 cote = dispoW < dispoH ? dispoW : dispoH;

				plateau = NkRect{gaucheSur + (W - gaucheSur - droiteSur - cote) * 0.5f, bandeau.y + bandeau.h + marge,
								 cote, cote};
				cellule = cote / static_cast<float32>(NK_LUDO_GRILLE);

				// Quatre sieges cote a cote : ils tiennent parce qu'ils affichent
				// une pastille de couleur et un mot court, pas une phrase.
				const float32 hSiege = hBandeau * 0.62f;
				const float32 ySiege = plateau.y + cote + marge * 0.6f;
				const float32 ecart = cote * 0.015f;
				const float32 lSiege = (cote - ecart * 3.f) / 4.f;
				for (int32 i = 0; i < NK_LUDO_JOUEURS; ++i) {
					siege[i] = NkRect{plateau.x + static_cast<float32>(i) * (lSiege + ecart), ySiege, lSiege, hSiege};
				}

				bouton = NkRect{plateau.x, ySiege + hSiege + marge * 0.5f, cote, hBandeau * 0.82f};

				const float32 cRetour = bandeau.h * 0.56f;
				retour = NkRect{bandeau.x + bandeau.w - cRetour - bandeau.h * 0.22f,
								bandeau.y + (bandeau.h - cRetour) * 0.5f, cRetour, cRetour};

				// ⚠️ Les boutons du menu s'ancrent SOUS le titre. La version
				// centree employee d'abord sur NkDames les faisait RECOUVRIR le
				// titre — defaut invisible au calcul, vu sur la capture.
				menuTitre = NkRect{plateau.x, plateau.y + plateau.h * 0.04f, cote, hBandeau * 1.1f};
				menuSousTitre = NkRect{plateau.x, menuTitre.y + menuTitre.h, cote, hBandeau * 0.66f};

				// Cinq entrees au lieu de quatre : elles doivent tenir sous le
				// sous-titre sans deborder du plateau. On resserre le pas plutot
				// que de rapetisser les cibles — une cible de menu se touche.
				const float32 lChoix = cote * 0.86f;
				const float32 hChoix = hBandeau * 0.80f;
				const float32 pas = hChoix * 1.24f;
				const float32 y0 = menuSousTitre.y + menuSousTitre.h + hBandeau * 0.22f;
				for (int32 i = 0; i < NK_LUDO_NB_MODES; ++i) {
					choix[i] =
						NkRect{plateau.x + (cote - lChoix) * 0.5f, y0 + static_cast<float32>(i) * pas, lChoix, hChoix};
				}
			}

			// =====================================================================
			void DessinerFond(NkGuiDrawList &dl, const renderer::NkLayoutInfo &info) {
				dl.AddRectFilled(NkRect{0.f, 0.f, static_cast<float32>(info.width), static_cast<float32>(info.height)},
								 kFond);
			}

			// =====================================================================
			void DessinerMenu(NkGuiDrawList &dl, const NkLudoGeometrie &geo, const NkLudoPolices &f) {
				TexteDansBoite(dl, f.titre, geo.menuTitre, "Ludo", kTexte);
				TexteDansBoite(dl, f.petite, geo.menuSousTitre, "Quatre joueurs, humains ou IA", kTexteFaible);

				const char *libelles[NK_LUDO_NB_MODES] = {"1 joueur, 3 IA", "2 joueurs, 2 IA", "3 joueurs, 1 IA",
														  "4 joueurs", "4 IA (simulation)"};
				for (int32 i = 0; i < NK_LUDO_NB_MODES; ++i) {
					dl.AddRectFilled(geo.choix[i], kPanneauActif, geo.choix[i].h * 0.26f);
					dl.AddRect(geo.choix[i], kJoueur[i % NK_LUDO_JOUEURS], 2.f, geo.choix[i].h * 0.26f);
					// La pastille reprend la couleur d'un joueur : elle distingue
					// les entrees sans dependre de la lecture du libelle.
					dl.AddCircleFilled(
						NkVec2f(geo.choix[i].x + geo.choix[i].h * 0.5f, geo.choix[i].y + geo.choix[i].h * 0.5f),
						geo.choix[i].h * 0.16f, kJoueur[i % NK_LUDO_JOUEURS]);
					TexteDansBoite(dl, f.corps,
								   NkRect{geo.choix[i].x + geo.choix[i].h * 0.9f, geo.choix[i].y,
										  geo.choix[i].w - geo.choix[i].h * 1.2f, geo.choix[i].h},
								   libelles[i], kTexte);
				}
			}

			// =====================================================================
			void DessinerBandeau(NkGuiDrawList &dl, const NkLudoGeometrie &geo, const NkLudoPolices &f,
								 const NkLudoVue &vue) {
				dl.AddRectFilled(geo.bandeau, kPanneau, geo.bandeau.h * 0.22f);
				const float32 pad = geo.bandeau.h * 0.22f;
				TexteDansBoite(dl, f.titre,
							   NkRect{geo.bandeau.x + pad, geo.bandeau.y, geo.bandeau.w * 0.40f,
									  geo.bandeau.h * 0.55f},
							   "Ludo", kTexte);

				const int32 j = vue.partie->Joueur();
				const bool humain = vue.controleur[j] == NkControleur::NK_HUMAIN;
				const NkString ligne = humain ? NkString::Format("A %s de jouer", kNomJoueur[j])
											  : NkString::Format("%s reflechit", kNomJoueur[j]);
				Texte(dl, f.petite, geo.bandeau.x + pad, geo.bandeau.y + geo.bandeau.h * 0.56f, ligne.Data(),
					  kJoueur[j], geo.bandeau.w * 0.45f);

				// Le de a une place FIXE : c'est l'information la plus regardee
				// de la partie, elle ne doit pas se deplacer d'un tour a l'autre.
				const float32 cote = geo.bandeau.h * 0.60f;
				// Pendant le lancer, on montre la face qui DEFILE, pas le resultat :
				// un de qui affiche sa valeur finale des la premiere image ne se
				// lit pas comme un lancer.
				int32 faceAffichee = vue.deLance ? vue.dernierDe : 0;
				if (vue.deAnim != nullptr && vue.deAnim->actif) {
					faceAffichee = vue.deAnim->faceMontree;
				}
				DessinerDe(dl, NkRect{geo.retour.x - cote - pad * 0.5f, geo.bandeau.y + (geo.bandeau.h - cote) * 0.5f,
									  cote, cote},
						   faceAffichee);

				// Retour au menu : trois barres dessinees, pas un glyphe — un
				// glyphe demanderait un atlas d'icones que ce jeu n'a pas.
				dl.AddRectFilled(geo.retour, kPanneauActif, geo.retour.h * 0.26f);
				dl.AddRect(geo.retour, kBord, 1.5f, geo.retour.h * 0.26f);
				for (int32 i = 0; i < 3; ++i) {
					const float32 y = geo.retour.y + geo.retour.h * (0.32f + static_cast<float32>(i) * 0.18f);
					dl.AddRectFilled(
						NkRect{geo.retour.x + geo.retour.w * 0.26f, y, geo.retour.w * 0.48f, geo.retour.h * 0.07f},
						kTexte, geo.retour.h * 0.035f);
				}
			}

			// =====================================================================
			void DessinerPlateau(NkGuiDrawList &dl, const NkLudoGeometrie &geo) {
				dl.AddRectFilled(geo.plateau, kPlateau, geo.cellule * 0.4f);

				// Les quatre ecuries : un bloc 6x6 dans chaque angle.
				const int32 coinL[4] = {0, 0, 9, 9};
				const int32 coinC[4] = {0, 9, 9, 0};
				for (int32 j = 0; j < NK_LUDO_JOUEURS; ++j) {
					const NkRect bloc{geo.plateau.x + static_cast<float32>(coinC[j]) * geo.cellule,
									  geo.plateau.y + static_cast<float32>(coinL[j]) * geo.cellule, geo.cellule * 6.f,
									  geo.cellule * 6.f};
					dl.AddRectFilled(bloc, kJoueur[j], geo.cellule * 0.4f);
					const float32 pad = geo.cellule * 0.7f;
					dl.AddRectFilled(NkRect{bloc.x + pad, bloc.y + pad, bloc.w - pad * 2.f, bloc.h - pad * 2.f},
									 kPlateau, geo.cellule * 0.3f);
					for (int32 p = 0; p < NK_LUDO_PIONS; ++p) {
						int32 l, c;
						NkLudoGeometrieEcurie(j, p, l, c);
						dl.AddCircle(geo.CentreCase(l, c), geo.cellule * 0.42f, kJoueurSombre[j], geo.cellule * 0.07f);
					}
				}

				// La piste. Une entree porte la couleur de son proprietaire ; les
				// autres cases sures sont grises.
				for (int32 i = 0; i < NK_LUDO_PISTE; ++i) {
					int32 l, c;
					NkLudoGeometriePiste(i, l, c);
					const NkRect box = geo.CaseRect(l, c);
					NkColor fond = kPlateau;
					bool estEntree = false;
					for (int32 j = 0; j < NK_LUDO_JOUEURS; ++j) {
						if (NkLudoEntree(j) == i) {
							fond = kJoueur[j];
							estEntree = true;
						}
					}
					if (!estEntree && NkLudoCaseSure(i)) {
						fond = kSure;
					}
					dl.AddRectFilled(box, fond);
					dl.AddRect(box, kTrait, 1.f);
				}

				for (int32 j = 0; j < NK_LUDO_JOUEURS; ++j) {
					for (int32 k = 0; k < NK_LUDO_MAISON; ++k) {
						int32 l, c;
						NkLudoGeometrieMaison(j, k, l, c);
						const NkRect box = geo.CaseRect(l, c);
						dl.AddRectFilled(box, kJoueur[j]);
						dl.AddRect(box, kTrait, 1.f);
					}
				}

				// Le centre : quatre triangles vers le milieu.
				const NkVec2f centre = geo.CentreCase(7, 7);
				const float32 d = geo.cellule * 1.5f;
				const NkVec2f hg(centre.x - d, centre.y - d), hd(centre.x + d, centre.y - d);
				const NkVec2f bd(centre.x + d, centre.y + d), bg(centre.x - d, centre.y + d);
				dl.AddTriangleFilled(hg, bg, centre, kJoueur[0]);
				dl.AddTriangleFilled(hg, hd, centre, kJoueur[1]);
				dl.AddTriangleFilled(hd, bd, centre, kJoueur[2]);
				dl.AddTriangleFilled(bg, bd, centre, kJoueur[3]);
			}

			// =====================================================================
			void DessinerPions(NkGuiDrawList &dl, const NkLudoGeometrie &geo, const NkLudoVue &vue) {
				const float32 r = geo.cellule * 0.36f;
				const int32 joueurCourant = vue.partie->Joueur();
				const bool humainAuTrait = vue.controleur[joueurCourant] == NkControleur::NK_HUMAIN;

				const bool anime = (vue.anim != nullptr && vue.anim->actif);

				for (int32 j = 0; j < NK_LUDO_JOUEURS; ++j) {
					for (int32 p = 0; p < NK_LUDO_PIONS; ++p) {
						// Le pion anime est DEJA arrive dans les regles : on ne le
						// dessine pas a sa case, sinon il apparait deux fois.
						if (anime && vue.anim->joueur == j && vue.anim->pion == p) {
							continue;
						}
						int32 l = 0, c = 0;
						const int32 av = vue.partie->Avancement(j, p);
						NkPositionPion(j, p, av, l, c);
						if (l < 0) {
							continue;
						}
						NkVec2f centre = geo.CentreCase(l, c);
						// Les pions rentres s'empilent legerement : sinon quatre
						// pions sur la meme case se lisent comme un seul.
						if (av >= NK_LUDO_ARRIVEE) {
							centre.x += (static_cast<float32>(p) - 1.5f) * r * 0.42f;
						}

						dl.AddCircleFilled(NkVec2f(centre.x, centre.y + r * 0.22f), r, NkColor(0, 0, 0, 80));
						dl.AddCircleFilled(centre, r, kJoueurSombre[j]);
						dl.AddCircleFilled(NkVec2f(centre.x, centre.y - r * 0.10f), r * 0.78f, kJoueur[j]);
						dl.AddCircleFilled(NkVec2f(centre.x - r * 0.24f, centre.y - r * 0.34f), r * 0.20f,
										   NkColor(255, 255, 255, 150));

						// Un pion JOUABLE se signale — et seulement quand un
						// HUMAIN est au trait : pendant le tour d'une IA, un
						// halo ferait croire qu'on attend un clic.
						if (j == joueurCourant && humainAuTrait && vue.deLance && vue.coups != nullptr) {
							for (uint32 i = 0; i < vue.coups->Size(); ++i) {
								if ((*vue.coups)[i].pion == static_cast<int8>(p)) {
									dl.AddCircle(centre, r * 1.28f, NkColor(255, 255, 255, 220), r * 0.16f);
								}
							}
						}
					}
				}

				// Le pion en mouvement passe EN DERNIER : il survole les autres.
				if (anime) {
					float32 fl = 0.f, fc = 0.f;
					vue.anim->Position(fl, fc);
					const float32 cloche = 4.f * vue.anim->t * (1.f - vue.anim->t);
					const NkVec2f centre(geo.plateau.x + (fc + 0.5f) * geo.cellule,
										 geo.plateau.y + (fl + 0.5f) * geo.cellule - geo.cellule * 0.12f * cloche);
					const int32 j = vue.anim->joueur;
					dl.AddCircleFilled(NkVec2f(centre.x, centre.y + r * (0.22f + cloche * 0.5f)),
									   r * (1.f + cloche * 0.2f), NkColor(0, 0, 0, 80));
					dl.AddCircleFilled(centre, r, kJoueurSombre[j]);
					dl.AddCircleFilled(NkVec2f(centre.x, centre.y - r * 0.10f), r * 0.78f, kJoueur[j]);
					dl.AddCircleFilled(NkVec2f(centre.x - r * 0.24f, centre.y - r * 0.34f), r * 0.20f,
									   NkColor(255, 255, 255, 150));
				}
			}

			// =====================================================================
			void DessinerPiedDePage(NkGuiDrawList &dl, const NkLudoGeometrie &geo, const NkLudoPolices &f,
									const NkLudoVue &vue) {
				const int32 courant = vue.partie->Joueur();
				for (int32 i = 0; i < NK_LUDO_JOUEURS; ++i) {
					const bool ia = vue.controleur[i] == NkControleur::NK_IA;
					const bool auTrait = (!vue.finie && courant == i);
					dl.AddRectFilled(geo.siege[i], ia ? kPanneau : kPanneauActif, geo.siege[i].h * 0.28f);
					dl.AddRect(geo.siege[i], auTrait ? kJoueur[i] : kBord, auTrait ? 2.5f : 1.5f,
							   geo.siege[i].h * 0.28f);
					dl.AddCircleFilled(
						NkVec2f(geo.siege[i].x + geo.siege[i].h * 0.42f, geo.siege[i].y + geo.siege[i].h * 0.5f),
						geo.siege[i].h * 0.19f, kJoueur[i]);
					// Un mot court : quatre sieges tiennent cote a cote parce que
					// le libelle est "IA" ou "vous", jamais une phrase.
					TexteDansBoite(dl, f.petite,
								   NkRect{geo.siege[i].x + geo.siege[i].h * 0.68f, geo.siege[i].y,
										  geo.siege[i].w - geo.siege[i].h * 0.76f, geo.siege[i].h},
								   ia ? "IA" : "vous", ia ? kTexteFaible : kTexte);
				}

				// Le bouton fait DEUX choses selon l'etat : lancer le de, ou
				// passer quand aucun coup n'est possible. Un seul bouton dont le
				// libelle dit lequel des deux — deux boutons dont un est toujours
				// grise coutent plus de place et disent moins.
				const bool aJouer = (!vue.finie && vue.controleur[courant] == NkControleur::NK_HUMAIN);
				const char *libelle = "Au tour de l'ordinateur";
				if (aJouer) {
					libelle = !vue.deLance
								  ? "Lancer le de"
								  : ((vue.coups == nullptr || vue.coups->Size() == 0) ? "Aucun coup — passer"
																					 : "Choisissez un pion");
				}
				dl.AddRectFilled(geo.bouton, aJouer ? kJoueur[courant] : kPanneau, geo.bouton.h * 0.3f);
				dl.AddRect(geo.bouton, kBord, 1.5f, geo.bouton.h * 0.3f);
				TexteDansBoite(dl, f.corps, geo.bouton, libelle, aJouer ? NkColor(255, 250, 246) : kTexteFaible);
			}

			// =====================================================================
			void DessinerFin(NkGuiDrawList &dl, const renderer::NkLayoutInfo &info, const NkLudoPolices &f,
							 const NkLudoVue &vue) {
				const float32 W = static_cast<float32>(info.width);
				const float32 H = static_cast<float32>(info.height);
				dl.AddRectFilled(NkRect{0.f, 0.f, W, H}, kVoile);

				const NkRect panneau{W * 0.10f, H * 0.36f, W * 0.80f, H * 0.24f};
				dl.AddRectFilled(panneau, kPanneau, panneau.h * 0.14f);
				dl.AddRect(panneau, vue.gagnant >= 0 ? kJoueur[vue.gagnant] : kOr, 2.f, panneau.h * 0.14f);

				// "Vous avez gagne" n'a de sens que si le gagnant etait tenu par
				// un humain. En simulation, on nomme la couleur.
				NkString titre = NkString("Partie terminee");
				if (vue.gagnant >= 0) {
					titre = (vue.controleur[vue.gagnant] == NkControleur::NK_HUMAIN)
								? NkString::Format("%s gagne — c'est vous", kNomJoueur[vue.gagnant])
								: NkString::Format("%s gagne", kNomJoueur[vue.gagnant]);
				}
				TexteDansBoite(dl, f.corps, NkRect{panneau.x, panneau.y, panneau.w, panneau.h * 0.62f}, titre.Data(),
							   kTexte);
				TexteDansBoite(dl, f.petite,
							   NkRect{panneau.x, panneau.y + panneau.h * 0.58f, panneau.w, panneau.h * 0.4f},
							   "Touchez pour rejouer", kTexteFaible);
			}

		} // namespace ludo
	} // namespace jeux
} // namespace nkentseu
