// =============================================================================
// NkCanvasSplash.h — l'ecran d'ouverture, IDENTIQUE d'un jeu a l'autre
//
// ⚠️ LA REGLE QUI COMMANDE CE FICHIER (Rodolf, 2026-09-01)
//   « Je veux que les splash screen soient identiques d'un jeu a l'autre, et que
//     la seule chose qui differe soit celle liee au jeu en question. »
//
//   Donc : DEUX volets, toujours les memes, toujours dans le meme ordre, avec la
//   meme charte, les memes durees et la meme mise en page. Le nom du jeu est la
//   SEULE variable — il n'y a pas d'autre reglage, et c'est deliberé : offrir
//   des couleurs ou des durees par jeu, c'est garantir qu'ils divergeront.
//
// LE MODELE EST CELUI DE GEMCRUSH, ET IL N'EST PAS INVENTE ICI
//   `Applications/Gemcrush/src/Gemcrush/Ui/NkGemSplash.cpp`, lui-meme calque sur
//   `Applications/Pong/src/Pong/UI/Scenes/NogeIntroScene.cpp`. On en reprend la
//   structure exacte :
//     volet 1  fond petrole degrade, mot RIHEN, filet orange qui SE TRACE
//              pendant le fondu, « RIHEN UNIVERSE » dessous ;
//     volet 2  fond sombre degrade, mot NKENTSEU, les modules employes, la
//              signature du moteur — et le nom du jeu.
//
//   La charte vient du `CLAUDE.md` de Rihen : petrole #0A555F, orange #F79A28.
//
//   ⚠️ ET LE LOGO EST LE VRAI. Il n'est pas dessine a la main : c'est le SVG
//   officiel `rihen-mark.svg`, EMBARQUE dans `NkRihenMarque.h` (514 octets) et
//   rasterise au demarrage. On ne fabrique pas une marque, on affiche la sienne
//   — en fabriquer une serait une invention.
//
// ⚠️ POURQUOI IL VIT ICI ET NON DANS CHAQUE JEU
//   Il en existait DEJA DEUX, ecrits separement (Pong, GemCrush). Trois jeux de
//   plus en auraient fait CINQ, et la correction faite sur l'un ne serait jamais
//   parvenue aux autres. Quand la couche du dessous ne porte pas la chose, on la
//   fait grossir LA. Toute application batie sur NkCanvasApp en herite.
//
// ⚠️ EN-TETE SEUL, ET C'EST DELIBERE
//   Un `.cpp` dans un module du noyau cree une dependance DURE pour tous ses
//   dependants — c'est le mecanisme qui a fait lier NKUI, module deprecie, a des
//   applications qui ne l'utilisaient pas. Ce fichier ne fait que dessiner.
//
// ⚠️ AUCUN FICHIER A LIVRER — et c'est different de « aucune texture »
//   Le symbole EST une texture : le SVG embarque est rasterise en RGBA au
//   demarrage, puis televerse par `NkCanvasGuiApp` comme le sont les atlas de
//   police. Mais il n'y a AUCUN fichier a copier : pas de `dependfiles`, pas
//   d'`androidassets`, pas de preload Web, pas de bundle iOS. L'ouverture est
//   donc identique sur les sept plateformes sans chaine d'assets — la propriete
//   que GemCrush a explicitement voulue, conservee.
//
//   Le reste — fonds, filet, jetons, trait de progression — est en primitives.
//
// ⚠️ IL EST TOUJOURS SAUTABLE
//   Un ecran d'ouverture qu'on ne peut pas passer est une taxe payee a chaque
//   lancement, y compris par celui qui teste le jeu vingt fois par heure.
//
// POINT D'EXTENSION
//   Le logo COMPLET (le mot RIHEN dessine, pas seulement le symbole) vit a
//   `Resources/Pong/Textures/logo.svg` — 27,7 Ko, 1920x562. Pour l'employer :
//   soit l'embarquer comme le symbole, en acceptant 27,7 Ko dans chaque binaire,
//   soit ouvrir la chaine d'assets et le charger. Le second choix se paie sur
//   sept plateformes, pas sur une. Le changement se fait ICI, et les cinq
//   applications le recoivent le meme jour.
// =============================================================================
#pragma once

#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKCore/NkTypes.h"
#include "NKGui/Core/NkGuiDrawList.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace renderer {

		/// L'ecran d'ouverture, commun a toutes les applications NkCanvasApp.
		///
		/// Emploi, et il tient en trois lignes :
		/// ```
		/// bool OnGuiInit() override { mSplash.PoserJeu("Ludo"); ... }
		/// void OnTick(float32 dt) override { if (mSplash.Avancer(dt)) return; ... }
		/// void OnDraw(NkGuiDrawList &dl) override {
		///     if (!mSplash.Termine()) { mSplash.Dessiner(dl, titre, corps, petite, ecran); return; }
		///     ...
		/// }
		/// bool OnPointer(const NkPointer &p) override {
		///     if (!mSplash.Termine()) { if (p.phase == UP) mSplash.Sauter(); return true; }
		///     ...
		/// }
		/// ```
		class NkCanvasSplash {
			public:
				// ── Le SEUL reglage ──────────────────────────────────────────
				/// Le nom du jeu, unique element qui differe d'une application a
				/// l'autre.
				///
				/// ⚠️ La chaine n'est PAS copiee : elle doit survivre au splash.
				/// Un litteral convient ; une NkString temporaire, non. Copier
				/// obligerait a allouer dans un en-tete de dessin.
				void PoserJeu(const char *nomDuJeu) noexcept {
					mNom = (nomDuJeu != nullptr) ? nomDuJeu : "";
					Reprendre();
				}

				// ── Deroulement ──────────────────────────────────────────────
				/// Avance d'une trame. Rend `true` tant que l'ouverture tient
				/// l'ecran — l'appelant s'arrete la.
				bool Avancer(float32 deltaTime) noexcept {
					if (mTermine) {
						return false;
					}
					// ⚠️ Le pas de temps est PLAFONNE. Au retour de veille, ou a
					// la premiere image sur le Web, l'horloge rend parfois
					// plusieurs secondes d'un coup : sans plafond, l'ouverture
					// entiere serait sautee sans qu'on l'ait vue, et le defaut
					// se lirait comme « le splash ne s'affiche pas ».
					const float32 dt = (deltaTime > 0.1f) ? 0.1f : ((deltaTime < 0.f) ? 0.f : deltaTime);
					mTemps += dt;
					if (mTemps >= kDureeVolet) {
						mTemps = 0.f;
						++mVolet;
						if (mVolet >= kNbVolets) {
							mTermine = true;
							return false;
						}
					}
					return true;
				}

				void Sauter() noexcept {
					mTermine = true;
					mVolet = kNbVolets;
					mTemps = 0.f;
				}

				void Reprendre() noexcept {
					mTermine = false;
					mVolet = 0;
					mTemps = 0.f;
				}

				bool Termine() const noexcept {
					return mTermine;
				}

				// ── Dessin ───────────────────────────────────────────────────
				/// Peint le volet courant dans `ecran`.
				///
				/// Trois polices, parce que les trois tailles portent trois
				/// niveaux de lecture. Elles peuvent etre identiques : on ne
				/// suppose pas que l'appelant en a trois.
				/// `logoTexId` est le symbole Rihen rasterise -- 0 si absent.
				///
				/// ⚠️ ZERO N'EST PAS UNE PANNE : le volet se dessine alors avec un
				/// anneau trace en primitives. Un splash qui refuserait de
				/// s'afficher parce qu'un decodage SVG a echoue transformerait un
				/// defaut cosmetique en application morte.
				void Dessiner(nkgui::NkGuiDrawList &dl, nkgui::NkGuiFont *titre, nkgui::NkGuiFont *corps,
							  nkgui::NkGuiFont *petite, const nkgui::NkRect &ecran,
							  uint32 logoTexId = 0u) const noexcept {
					if (mTermine) {
						return;
					}
					const float32 a = Opacite();
					if (mVolet == 0) {
						DessinerRihen(dl, titre, petite, ecran, a, logoTexId);
					} else {
						DessinerMoteur(dl, titre, corps, petite, ecran, a);
					}
					Invite(dl, petite, ecran, a);
				}

			private:
				// ── La charte Rihen ──────────────────────────────────────────
				// #0A555F petrole, #F79A28 orange. Elles viennent du CLAUDE.md de
				// Rihen ; on ne les choisit pas ici, on les applique.
				static nkgui::NkColor Petrole(float32 a) noexcept {
					return Alpha(nkgui::NkColor{10, 85, 95, 255}, a);
				}
				static nkgui::NkColor PetroleFonce(float32 a) noexcept {
					return Alpha(nkgui::NkColor{5, 48, 55, 255}, a);
				}
				static nkgui::NkColor Orange(float32 a) noexcept {
					return Alpha(nkgui::NkColor{247, 154, 40, 255}, a);
				}

				static nkgui::NkColor Alpha(const nkgui::NkColor &c, float32 a) noexcept {
					const float32 f = (a < 0.f) ? 0.f : ((a > 1.f) ? 1.f : a);
					return nkgui::NkColor{c.r, c.g, c.b, static_cast<uint8>(static_cast<float32>(c.a) * f + 0.5f)};
				}

				// ── Volet 1 : RIHEN. Rigoureusement identique partout. ───────
				void DessinerRihen(nkgui::NkGuiDrawList &dl, nkgui::NkGuiFont *titre, nkgui::NkGuiFont *petite,
								   const nkgui::NkRect &ecran, float32 a, uint32 logoTexId) const noexcept {
					// Fond petrole PLEIN ECRAN, encoche comprise : un fond de
					// marque qui s'arreterait a la zone sure laisserait des
					// bandes noires sur un telephone a encoche.
					dl.AddRectFilledMultiColor(ecran, Petrole(a), Petrole(a), PetroleFonce(a), PetroleFonce(a));

					const float32 cx = ecran.x + ecran.w * 0.5f;
					const float32 cy = ecran.y + ecran.h * 0.5f;
					const float32 s = Echelle(ecran);

					// ── LE SYMBOLE RIHEN ────────────────────────────────────
					// C'est le VRAI logo, rasterise depuis le SVG officiel
					// embarque (NkRihenMarque.h) : on ne dessine pas une marque,
					// on affiche la sienne.
					//
					// Il RESPIRE legerement : la meme pulsation que la gemme de
					// GemCrush, pour que l'ouverture ne soit pas une image fixe
					// pendant deux secondes.
					{
						const float32 pouls = 0.5f + 0.5f * math::NkSin(mTemps * 2.2f);
						const float32 r = (52.f + pouls * 4.f) * s;
						const math::NkVec2f c(cx, cy - 86.f * s);
						if (logoTexId != 0u) {
							dl.AddImage(logoTexId, nkgui::NkRect{c.x - r, c.y - r, r * 2.f, r * 2.f},
										math::NkVec2f(0.f, 0.f), math::NkVec2f(1.f, 1.f),
										Alpha(nkgui::NkColor{255, 255, 255, 255}, a));
						} else {
							// ⚠️ REPLI ASSUME, ET IL SE VOIT COMME UN REPLI : un
							// anneau, pas une contrefacon du symbole. Dessiner
							// une approximation ferait croire que le logo est la.
							dl.AddCircle(c, r, Orange(a), 3.f * s);
							dl.AddCircleFilled(c, r * 0.30f, Orange(a * 0.85f));
						}
					}

					const float32 hTitre = NkTexteHauteurLigne(titre, 34.f * s);
					const float32 yTitre = cy + 8.f * s;
					NkTexteCentre(dl, titre, cx, yTitre, "RIHEN", Alpha(nkgui::NkColor{255, 255, 255, 255}, a));

					// ⚠️ LE FILET SE TRACE PENDANT LE FONDU, il n'apparait pas
					// d'un coup. C'est ce qui donne a l'ouverture son mouvement
					// sans aucune animation a piloter.
					{
						const float32 pousse = math::NkClamp(mTemps / (kEntree * 1.6f), 0.f, 1.f);
						const float32 lMax = (ecran.w * 0.42f < 240.f * s) ? ecran.w * 0.42f : 240.f * s;
						const float32 l = lMax * pousse;
						dl.AddRectFilled(nkgui::NkRect{cx - l * 0.5f, yTitre + hTitre * 1.02f, l, 3.f * s}, Orange(a),
										 2.f);
					}

					NkTexteCentre(dl, petite, cx, yTitre + hTitre * 1.02f + 14.f * s, "RIHEN UNIVERSE",
								  Alpha(nkgui::NkColor{200, 226, 230, 255}, a * 0.85f));
				}

				// ── Volet 2 : NKENTSEU. Identique, SAUF le nom du jeu. ───────
				void DessinerMoteur(nkgui::NkGuiDrawList &dl, nkgui::NkGuiFont *titre, nkgui::NkGuiFont *corps,
									nkgui::NkGuiFont *petite, const nkgui::NkRect &ecran, float32 a) const noexcept {
					dl.AddRectFilledMultiColor(ecran, Alpha(nkgui::NkColor{22, 27, 38, 255}, a),
											   Alpha(nkgui::NkColor{22, 27, 38, 255}, a),
											   Alpha(nkgui::NkColor{10, 12, 17, 255}, a),
											   Alpha(nkgui::NkColor{10, 12, 17, 255}, a));

					const float32 cx = ecran.x + ecran.w * 0.5f;
					const float32 cy = ecran.y + ecran.h * 0.5f;
					const float32 s = Echelle(ecran);

					// Cinq jetons en arc, qui ARRIVENT L'UN APRES L'AUTRE pendant
					// le fondu. Meme role que les six gemmes de GemCrush : montrer
					// que le moteur dessine, plutot que de l'ecrire.
					for (int32 i = 0; i < 5; ++i) {
						const float32 t = static_cast<float32>(i) / 4.f;
						const float32 venue = math::NkClamp((mTemps - t * 0.10f) / kEntree, 0.f, 1.f);
						if (venue <= 0.f) {
							continue;
						}
						const float32 angle = math::NK_PI_F * (0.18f + 0.64f * t);
						const float32 rayonMax = (ecran.w * 0.30f < 150.f * s) ? ecran.w * 0.30f : 150.f * s;
						const math::NkVec2f c(cx - math::NkCos(angle) * rayonMax,
											  cy - 92.f * s - math::NkSin(angle) * rayonMax * 0.42f);
						const nkgui::NkColor teintes[5] = {
							nkgui::NkColor{247, 154, 40, 255},  nkgui::NkColor{90, 200, 255, 255},
							nkgui::NkColor{90, 220, 140, 255},  nkgui::NkColor{235, 90, 110, 255},
							nkgui::NkColor{200, 160, 255, 255}};
						dl.AddCircleFilled(c, 17.f * s * venue, Alpha(teintes[i], a * venue));
					}

					float32 y = cy - 24.f * s;
					const float32 hTitre = NkTexteHauteurLigne(titre, 34.f * s);
					NkTexteCentre(dl, titre, cx, y, "NKENTSEU", Alpha(nkgui::NkColor{90, 200, 255, 255}, a));
					y += hTitre + 10.f * s;

					// Les modules REELLEMENT employes par ces jeux, pas une liste
					// d'apparat : NKCanvas rend, NKGui dessine, NKEvent ecoute.
					NkTexteCentre(dl, corps, cx, y, "NKCANVAS   NKGUI   NKEVENT",
								  Alpha(nkgui::NkColor{130, 190, 210, 255}, a * 0.95f));
					y += NkTexteHauteurLigne(corps, 18.f * s) + 18.f * s;

					NkTexteCentre(dl, petite, cx, y, "MOTEUR C++ SANS BIBLIOTHEQUE STANDARD",
								  Alpha(nkgui::NkColor{140, 150, 170, 255}, a * 0.8f));
					y += NkTexteHauteurLigne(petite, 14.f * s) + 26.f * s;

					// ── LA SEULE CHOSE QUI DIFFERE D'UN JEU A L'AUTRE ────────
					if (mNom != nullptr && mNom[0] != '\0') {
						const float32 lTrait = (ecran.w * 0.30f < 180.f * s) ? ecran.w * 0.30f : 180.f * s;
						dl.AddRectFilled(nkgui::NkRect{cx - lTrait * 0.5f, y, lTrait, 2.f * s}, Orange(a * 0.7f), 1.f);
						y += 16.f * s;
						NkTexteCentre(dl, corps, cx, y, mNom, Alpha(nkgui::NkColor{255, 255, 255, 255}, a));
					}
				}

				// ── L'invite, commune aux deux volets ────────────────────────
				void Invite(nkgui::NkGuiDrawList &dl, nkgui::NkGuiFont *petite, const nkgui::NkRect &ecran,
							float32 a) const noexcept {
					// Elle n'apparait qu'apres le fondu d'entree, pour ne pas
					// clignoter au demarrage.
					if (mTemps <= kEntree) {
						return;
					}
					const float32 s = Echelle(ecran);
					const float32 h = NkTexteHauteurLigne(petite, 14.f * s);
					NkTexteCentre(dl, petite, ecran.x + ecran.w * 0.5f, ecran.y + ecran.h - h * 2.4f,
								  "toucher pour passer", Alpha(nkgui::NkColor{150, 170, 180, 255}, a * 0.75f));

					// Le trait de progression : il DIT qu'il y a une suite, et
					// combien il en reste. Sans lui, une ouverture de 4 s
					// ressemble a un programme fige.
					const float32 l = ecran.w * 0.22f;
					const nkgui::NkRect fond{ecran.x + ecran.w * 0.5f - l * 0.5f, ecran.y + ecran.h - h * 1.1f, l,
											 3.f * s};
					dl.AddRectFilled(fond, Alpha(nkgui::NkColor{255, 255, 255, 60}, a), 1.5f);
					const float32 avance =
						(static_cast<float32>(mVolet) + mTemps / kDureeVolet) / static_cast<float32>(kNbVolets);
					dl.AddRectFilled(nkgui::NkRect{fond.x, fond.y, fond.w * avance, fond.h}, Orange(a), 1.5f);
				}

				/// L'echelle : tout est dimensionne pour une hauteur de reference
				/// de 720 px, puis mis a l'echelle. Sans cela, le meme splash
				/// serait minuscule sur un ecran de bureau et illisible sur un
				/// telephone — deux defauts qui ne se voient jamais ensemble.
				static float32 Echelle(const nkgui::NkRect &ecran) noexcept {
					const float32 petitCote = (ecran.w < ecran.h) ? ecran.w : ecran.h;
					const float32 s = petitCote / 720.f;
					return (s < 0.55f) ? 0.55f : ((s > 2.2f) ? 2.2f : s);
				}

				/// Fondu d'entree, plateau, fondu de sortie.
				float32 Opacite() const noexcept {
					if (mTemps < kEntree) {
						return mTemps / kEntree;
					}
					if (mTemps < kEntree + kMaintien) {
						return 1.f;
					}
					const float32 t = (mTemps - kEntree - kMaintien) / kSortie;
					return (t >= 1.f) ? 0.f : (1.f - t);
				}

				// Durees d'un volet. Deux volets => ~4,1 s, comme GemCrush.
				static constexpr float32 kEntree = 0.45f;
				static constexpr float32 kMaintien = 1.20f;
				static constexpr float32 kSortie = 0.40f;
				static constexpr float32 kDureeVolet = kEntree + kMaintien + kSortie;
				static constexpr int32 kNbVolets = 2;

				const char *mNom = "";
				int32 mVolet = 0;
				float32 mTemps = 0.f;
				bool mTermine = false;
		};

	} // namespace renderer
} // namespace nkentseu
