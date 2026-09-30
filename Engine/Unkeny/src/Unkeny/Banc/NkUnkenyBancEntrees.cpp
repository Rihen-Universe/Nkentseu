//
// NkUnkenyBancEntrees.cpp
// =============================================================================
// Description :
//   Le banc des entrees du jeu. Voir NkUnkenyBancEntrees.h.
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   (k1)  une action posee par programme passe par ses trois etats ; une action
//         qu'aucune liaison ne vise n'est PAS remise a zero par la trame
//   (k2)  clavier et manette SIMULES donnent la meme action : Espace -> Sauter,
//         puis le bouton Sud de la manette 0 -> Sauter
//   (k3)  diagonale au clavier (D + Z) : Direction normalisee a 1 (brute 1,414)
//   (k4)  Gauche et Droite ensemble : Avancer = 0 ; Gauche seule : -1
//   (k5)  zone morte RADIALE (0,2) : stick (0,1 ; 0,1) -> 0 ; (0,6 ; 0) -> 0,5 ;
//         (0,25 ; 0,05) garde un Y NON NUL (une zone morte axiale l'annulerait)
//   (k6)  deux manettes, deux joueurs (liaison j=*) : Sud de la manette 1 ->
//         Sauter du joueur 1 seulement ; le joueur 1 reassigne a la manette 0
//         ne le voit plus
//   (k7)  zone d'ecran (bas-droite) : un doigt dedans -> Sauter ; ailleurs, rien
//   (k8)  joystick virtuel FLOTTANT (rayon 0,1 de la hauteur = 60 px) : +60 px a
//         droite -> X = 1 ; 30 px vers le haut -> Y = +0,5 ; leve -> 0
//   (k9)  aller-retour texte : Ecrire puis Lire dans une autre table rend les
//         memes liaisons (champ par champ) et les memes actions ; (k9n) trois
//         lignes fautives refusees AVEC leur numero, les bonnes appliquees
//   (k10) reconfiguration : la liaison d'Espace capture K au clavier ; puis la
//         liaison du bouton Sud capture Est a la manette
//   (k11) ToutRelacher : D tenu puis relache par le jeu qui perd la main ->
//         Avancer = 0, et la trame suivante ne la ressuscite pas
//   (k12) champ NKGui focalise + composition IME « abc » : la zone IME est
//         publiee, des pixels de plus sont peints, et le tampon N'EST PAS
//         touche ; (k12n) en mot de passe, rien n'est affiche en clair
//   (k13) navigation au focus NKGui, ALLUMEE : Bas, Bas, Sud sur trois boutons
//         -> « Deux » active, lui seul ; (k13n) ETEINTE (le defaut) : la meme
//         suite ne clique rien et ne touche pas au pointeur
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Unkeny/Banc/NkUnkenyBancEntrees.h"

#include "NKEvent/NkGamepadSystem.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkTouchEvent.h"
#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiDrawListRaster.h"
#include "NKGui/Core/NkGuiFont.h"
#include "NKGui/Core/NkGuiNavigation.h"
#include "NKGui/Widgets/NkGuiWidgets.h"
#include "NKImage/Codecs/PNG/NkPNGCodec.h"
#include "NKImage/Core/NkImage.h"
#include "NKMemory/NkAllocator.h"
#include "Unkeny/Entree/NkUnkenyEntreesJeu.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace unkeny {

		namespace {

			int32 gEchecs = 0;
			int32 gReussis = 0;

			void Temoin(bool ok, const char *quoi, float32 valeur) {
				std::printf("  [%s] %-66s %10.4f\n", ok ? " OK " : "ECHEC", quoi, static_cast<double>(valeur));
				(ok ? gReussis : gEchecs)++;
			}

			bool Pres(float32 a, float32 b, float32 tol = 1.0e-4f) {
				return math::NkFabs(a - b) <= tol;
			}

			enum NkActionBanc : int32 { A_AVANCER = 0, A_MONTER, A_SAUTER, A_TIRER, A_SCRIPT };

			void Nommer(NkLiaisons &l) {
				l.NommerAction(A_AVANCER, "Avancer");
				l.NommerAction(A_MONTER, "Monter");
				l.NommerAction(A_SAUTER, "Sauter");
				l.NommerAction(A_TIRER, "Tirer");
				l.NommerAction(A_SCRIPT, "Script");
			}

			// ── Une manette de banc : un vrai backend, sans materiel ──
			class NkManetteDeBanc final : public NkIGamepad {
				public:
					NkGamepadSnapshot etat[NK_MAX_GAMEPADS];

					bool Init() override {
						return true;
					}
					void Shutdown() override {
					}
					void Poll() override {
					}
					uint32 GetConnectedCount() const override {
						uint32 n = 0;
						for (uint32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
							n += etat[i].connected ? 1u : 0u;
						}
						return n;
					}
					const NkGamepadSnapshot &GetSnapshot(uint32 i) const override {
						return etat[i < NK_MAX_GAMEPADS ? i : 0];
					}
					void Rumble(uint32, float32, float32, float32, float32, uint32) override {
					}
					const char *GetName() const noexcept override {
						return "ManetteDeBanc";
					}

					void Brancher(uint32 i) {
						etat[i].Clear();
						etat[i].connected = true;
						etat[i].info.index = i;
					}
					void Bouton(uint32 i, NkGamepadButton b, bool bas) {
						etat[i].buttons[static_cast<uint32>(b)] = bas;
					}
					void Axe(uint32 i, NkGamepadAxis a, float32 v) {
						etat[i].axes[static_cast<uint32>(a)] = v;
					}
			};

			/// Un NkGamepadSystem REEL devant la manette de banc : le chemin lu par
			/// le jeu est celui d'une vraie manette (remappage compris).
			struct NkManettes {
					NkGamepadSystem sys;
					NkManetteDeBanc *banc = nullptr;

					NkManettes() {
						memory::NkAllocator &a = memory::NkGetDefaultAllocator();
						banc = a.New<NkManetteDeBanc>();
						sys.Init(memory::NkUniquePtr<NkIGamepad>(banc, memory::NkDefaultDelete<NkIGamepad>(&a)));
					}
					void Sonder() {
						sys.PollGamepads();
					}
			};

			void Touche(NkEntreesJeu &e, NkKey k, bool bas) {
				if (bas) {
					NkKeyPressEvent ev(k);
					e.Lire(ev);
				} else {
					NkKeyReleaseEvent ev(k);
					e.Lire(ev);
				}
			}

			NkTouchPoint Point(uint64 id, float32 x, float32 y) {
				NkTouchPoint p;
				p.id = id;
				p.clientX = x;
				p.clientY = y;
				return p;
			}

			void Doigt(NkEntreesJeu &e, NkEventType::Value type, uint64 id, float32 x, float32 y) {
				const NkTouchPoint p = Point(id, x, y);
				if (type == NkEventType::NK_TOUCH_BEGIN) {
					NkTouchBeginEvent ev(&p, 1);
					e.Lire(ev);
				} else if (type == NkEventType::NK_TOUCH_MOVE) {
					NkTouchMoveEvent ev(&p, 1);
					e.Lire(ev);
				} else {
					NkTouchEndEvent ev(&p, 1);
					e.Lire(ev);
				}
			}

			/// Les liaisons de reference, communes a plusieurs temoins.
			void LiaisonsDeBase(NkLiaisons &l) {
				Nommer(l);
				l.LierTouche(A_AVANCER, NkKey::NK_D, 1.f);
				l.LierTouche(A_AVANCER, NkKey::NK_A, -1.f);
				l.LierTouche(A_MONTER, NkKey::NK_W, 1.f);
				l.LierTouche(A_MONTER, NkKey::NK_S, -1.f);
				l.LierTouche(A_SAUTER, NkKey::NK_SPACE);
				l.LierBouton(A_SAUTER, NkGamepadButton::NK_GP_SOUTH);
				l.LierAxe(A_AVANCER, NkGamepadAxis::NK_GP_AXIS_LX, 1.f, 0.2f);
				l.LierAxe(A_MONTER, NkGamepadAxis::NK_GP_AXIS_LY, 1.f, 0.2f);
			}

			// =================================================================
			// (k12) le champ NKGui en composition
			// =================================================================
			struct NkChampDeBanc {
					nkgui::NkGuiFont police;
					bool policeOk = false;
					nkgui::NkGuiContext ctx;
					nkgui::NkGuiDrawListRaster ras;
					char tampon[64] = {};

					NkChampDeBanc() {
						policeOk = police.LoadEmbedded(NkEmbeddedFontId::DroidSans, 15.f, false);
						ctx.Init(240, 40);
						ctx.font = policeOk ? &police : nullptr;
						ras.Init(240, 40);
						std::snprintf(tampon, sizeof(tampon), "x");
					}

					/// Une image ; `clic` focalise le champ (appui pose, transition vue).
					void Image(bool clic, nkgui::NkGuiInputFlags drapeaux) {
						ctx.input.mousePos = nkgui::NkVec2{60.f, 24.f}; // AU MILIEU du champ (marge de mise en page : 10 px)
						ctx.input.mouseDown[0] = clic;
						// dt NUL : le temps ne bouge pas, le curseur clignotant est donc
						// dans la MEME phase a chaque image -- sans cela deux images
						// identiques differeraient d'un trait de curseur.
						ctx.BeginFrame(0.f);
						ctx.BeginLayout(nkgui::NkRect{4.f, 4.f, 232.f, 32.f});
						ctx.DL().Reset();
						ctx.dlOverlay.Reset();
						nkgui::InputTextEx(ctx, "##champ", tampon, static_cast<int32>(sizeof(tampon)), drapeaux, -1);
						ctx.EndFrame();
						ras.Effacer(0x000000FFu);
						if (policeOk && police.pixels) {
							ras.PoserTexture(police.TexId(), police.pixels, police.atlasW, police.atlasH, 1);
						}
						(void)ras.Rasteriser(ctx.dl);
					}

					uint32 PixelsClairs() const {
						uint32 n = 0;
						for (int32 y = 0; y < 40; ++y) {
							for (int32 x = 0; x < 240; ++x) {
								const uint32 p = ras.Pixel(x, y);
								const uint32 r = (p >> 24) & 0xFFu;
								const uint32 g = (p >> 16) & 0xFFu;
								const uint32 b = (p >> 8) & 0xFFu;
								if (r + g + b > 360u) {
									++n;
								}
							}
						}
						return n;
					}

					void Png(const char *dossier, const char *nom) const {
						if (dossier == nullptr || dossier[0] == '\0') {
							return;
						}
						NkImage img = NkImage::Alloc(240, 40, NkImagePixelFormat::NK_RGBA32);
						if (!img.Pixels()) {
							return;
						}
						std::memcpy(img.Pixels(), ras.Pixels(), 240u * 40u * 4u);
						uint8 *sortie = nullptr;
						usize taille = 0;
						if (!NkPNGCodec::Encode(img, sortie, taille) || sortie == nullptr) {
							return;
						}
						char chemin[512];
						std::snprintf(chemin, sizeof(chemin), "%s/%s", dossier, nom);
						FILE *f = std::fopen(chemin, "wb");
						if (f != nullptr) {
							std::fwrite(sortie, 1, static_cast<size_t>(taille), f);
							std::fclose(f);
							std::printf("  (image ecrite : %s)\n", chemin);
						}
						memory::NkFree(sortie);
					}
			};

		} // namespace

		int32 NkUnkenyLancerBancEntrees() {
			gEchecs = 0;
			gReussis = 0;
			std::printf("\nUnkeny — banc des entrees du jeu\n\n");
			NkEntreesJeu *pe = memory::NkGetDefaultAllocator().New<NkEntreesJeu>();
			NkEntreesJeu &e = *pe;
			LiaisonsDeBase(e.Liaisons());
			NkManettes pads;
			pads.banc->Brancher(0);
			pads.banc->Brancher(1);
			pads.Sonder();

			// ── (k1) les trois etats, et l'action du programme ─────────────────
			{
				NkActions &a = e.Actions(0);
				e.Trame(&pads.sys);
				a.Poser(A_SCRIPT, 1.f);
				const bool presse = a.VientDEtrePressee(A_SCRIPT);
				e.Trame(&pads.sys);
				const bool tenue = a.Enfoncee(A_SCRIPT) && !a.VientDEtrePressee(A_SCRIPT);
				a.Poser(A_SCRIPT, 0.f);
				const bool relachee = a.VientDEtreRelachee(A_SCRIPT);
				e.Trame(&pads.sys);
				Temoin(presse && tenue && relachee && !a.Enfoncee(A_SCRIPT),
					   "(k1) action posee : pressee, tenue (la trame la garde), relachee", a.Valeur(A_SCRIPT));
			}

			// ── (k2) clavier et manette : la meme action ──────────────────────
			{
				Touche(e, NkKey::NK_SPACE, true);
				e.Trame(&pads.sys);
				const bool clavier = e.Actions(0).VientDEtrePressee(A_SAUTER);
				Touche(e, NkKey::NK_SPACE, false);
				e.Trame(&pads.sys);
				const bool lache = !e.Actions(0).Enfoncee(A_SAUTER);
				pads.banc->Bouton(0, NkGamepadButton::NK_GP_SOUTH, true);
				pads.Sonder();
				e.Trame(&pads.sys);
				const bool manette = e.Actions(0).VientDEtrePressee(A_SAUTER);
				pads.banc->Bouton(0, NkGamepadButton::NK_GP_SOUTH, false);
				pads.Sonder();
				e.Trame(&pads.sys);
				Temoin(clavier && lache && manette, "(k2) Espace et le bouton Sud donnent le meme Sauter", 0.f);
			}

			// ── (k3) diagonale normalisee ─────────────────────────────────────
			{
				Touche(e, NkKey::NK_D, true);
				Touche(e, NkKey::NK_W, true);
				e.Trame(&pads.sys);
				const NkVec2f d = e.Actions(0).Direction(A_AVANCER, A_MONTER);
				const float32 l = math::NkSqrt(d.x * d.x + d.y * d.y);
				Touche(e, NkKey::NK_D, false);
				Touche(e, NkKey::NK_W, false);
				e.Trame(&pads.sys);
				Temoin(Pres(l, 1.f, 1.0e-5f) && Pres(d.x, d.y), "(k3) diagonale au clavier : longueur 1, pas 1,414", l);
			}

			// ── (k4) touches opposees ─────────────────────────────────────────
			{
				Touche(e, NkKey::NK_A, true);
				Touche(e, NkKey::NK_D, true);
				e.Trame(&pads.sys);
				const float32 ensemble = e.Actions(0).Valeur(A_AVANCER);
				Touche(e, NkKey::NK_D, false);
				e.Trame(&pads.sys);
				const float32 seule = e.Actions(0).Valeur(A_AVANCER);
				Touche(e, NkKey::NK_A, false);
				e.Trame(&pads.sys);
				Temoin(Pres(ensemble, 0.f) && Pres(seule, -1.f), "(k4) Gauche + Droite = 0 ; Gauche seule = -1", seule);
			}

			// ── (k5) zone morte radiale ───────────────────────────────────────
			{
				auto stick = [&](float32 x, float32 y) {
					pads.banc->Axe(0, NkGamepadAxis::NK_GP_AXIS_LX, x);
					pads.banc->Axe(0, NkGamepadAxis::NK_GP_AXIS_LY, y);
					pads.Sonder();
					e.Trame(&pads.sys);
				};
				stick(0.1f, 0.1f);
				const bool mort = Pres(e.Actions(0).Valeur(A_AVANCER), 0.f) && Pres(e.Actions(0).Valeur(A_MONTER), 0.f);
				stick(0.6f, 0.f);
				const float32 moitie = e.Actions(0).Valeur(A_AVANCER);
				stick(0.25f, 0.05f);
				const float32 yDoux = e.Actions(0).Valeur(A_MONTER);
				stick(0.f, 0.f);
				Temoin(mort && Pres(moitie, 0.5f) && yDoux > 1.0e-3f,
					   "(k5) zone morte radiale : 0 dedans, 0,5 a 0,6, Y doux garde", yDoux);
			}

			// ── (k6) deux joueurs, deux manettes ──────────────────────────────
			{
				pads.banc->Bouton(1, NkGamepadButton::NK_GP_SOUTH, true);
				pads.Sonder();
				e.Trame(&pads.sys);
				const bool j1 = e.Actions(1).Enfoncee(A_SAUTER);
				const bool j0 = e.Actions(0).Enfoncee(A_SAUTER);
				e.AssignerManette(1, 0);
				e.Trame(&pads.sys);
				const bool j1Apres = e.Actions(1).Enfoncee(A_SAUTER);
				e.AssignerManette(1, 1);
				pads.banc->Bouton(1, NkGamepadButton::NK_GP_SOUTH, false);
				pads.Sonder();
				e.Trame(&pads.sys);
				Temoin(j1 && !j0 && !j1Apres, "(k6) manette 1 -> joueur 1 seul ; reassigne a la 0, plus rien", 0.f);
			}

			// ── (k7) et (k8) le doigt ─────────────────────────────────────────
			{
				NkEntreesJeu *pt = memory::NkGetDefaultAllocator().New<NkEntreesJeu>();
				NkEntreesJeu &t = *pt;
				Nommer(t.Liaisons());
				t.PoserSurface(0.f, 0.f, 800.f, 600.f);
				t.Liaisons().LierZone(A_SAUTER, NkZoneEcran{0.75f, 0.5f, 0.25f, 0.5f});
				t.Liaisons().LierStick(A_AVANCER, A_MONTER, NkZoneEcran{0.f, 0.f, 0.5f, 1.f}, 0.1f, 0.f);
				Doigt(t, NkEventType::NK_TOUCH_BEGIN, 7, 700.f, 500.f);
				t.Trame(nullptr);
				const bool dedans = t.Actions(0).Enfoncee(A_SAUTER);
				Doigt(t, NkEventType::NK_TOUCH_END, 7, 700.f, 500.f);
				Doigt(t, NkEventType::NK_TOUCH_BEGIN, 9, 600.f, 100.f); // hors de tout
				t.Trame(nullptr);
				const bool dehors = !t.Actions(0).Enfoncee(A_SAUTER);
				Doigt(t, NkEventType::NK_TOUCH_END, 9, 600.f, 100.f);
				t.Trame(nullptr);
				Temoin(dedans && dehors, "(k7) doigt dans la zone bas-droite : Sauter ; ailleurs, rien", 0.f);

				Doigt(t, NkEventType::NK_TOUCH_BEGIN, 3, 200.f, 300.f);
				Doigt(t, NkEventType::NK_TOUCH_MOVE, 3, 260.f, 300.f);
				t.Trame(nullptr);
				const float32 x = t.Actions(0).Valeur(A_AVANCER);
				const float32 y0 = t.Actions(0).Valeur(A_MONTER);
				Doigt(t, NkEventType::NK_TOUCH_MOVE, 3, 200.f, 270.f);
				t.Trame(nullptr);
				const float32 y = t.Actions(0).Valeur(A_MONTER);
				Doigt(t, NkEventType::NK_TOUCH_END, 3, 200.f, 270.f);
				t.Trame(nullptr);
				const bool zero = Pres(t.Actions(0).Valeur(A_MONTER), 0.f) && t.DoigtsSuivis() == 0u;
				Temoin(Pres(x, 1.f) && Pres(y0, 0.f) && Pres(y, 0.5f) && zero,
					   "(k8) joystick flottant : +60 px -> X 1 ; 30 px haut -> Y +0,5 ; leve -> 0", y);
				memory::NkGetDefaultAllocator().Delete(pt);
			}

			// ── (k9) aller-retour texte ───────────────────────────────────────
			{
				NkEntreesJeu *pa = memory::NkGetDefaultAllocator().New<NkEntreesJeu>();
				NkEntreesJeu *pb = memory::NkGetDefaultAllocator().New<NkEntreesJeu>();
				NkLiaisons &la = pa->Liaisons();
				LiaisonsDeBase(la);
				la.LierBouton(A_TIRER, NkGamepadButton::NK_GP_RB, 1.f, 1);
				la.LierZone(A_TIRER, NkZoneEcran{0.8f, 0.1f, 0.2f, 0.2f}, 1.f, 0);
				la.LierStick(A_AVANCER, A_MONTER, NkZoneEcran{0.f, 0.5f, 0.4f, 0.5f}, 0.08f, 0.15f, 0);
				la.LierSouris(A_TIRER, NkMouseButton::NK_MB_LEFT, 1.f, 0);
				const NkString texte = la.Ecrire();
				NkLiaisons &lb = pb->Liaisons();
				Nommer(lb);
				const NkRapportLiaisons r = lb.Lire(texte);
				bool memes = r.Ok() && lb.Nombre() == la.Nombre();
				for (int32 i = 0; memes && i < la.Nombre(); ++i) {
					const NkLiaison &a = la[i];
					const NkLiaison &b = lb[i];
					memes = a.action == b.action && a.joueur == b.joueur && a.nature == b.nature && a.code == b.code &&
							Pres(a.echelle, b.echelle) && Pres(a.zoneMorte, b.zoneMorte) && Pres(a.rayon, b.rayon) &&
							Pres(a.zone.x, b.zone.x) && Pres(a.zone.y, b.zone.y) && Pres(a.zone.l, b.zone.l) &&
							Pres(a.zone.h, b.zone.h);
				}
				// Les MEMES entrees donnent les MEMES actions.
				Touche(*pa, NkKey::NK_A, true);
				Touche(*pb, NkKey::NK_A, true);
				pa->Trame(&pads.sys);
				pb->Trame(&pads.sys);
				memes = memes && Pres(pa->Actions(0).Valeur(A_AVANCER), pb->Actions(0).Valeur(A_AVANCER)) &&
						Pres(pb->Actions(0).Valeur(A_AVANCER), -1.f);
				Temoin(memes, "(k9) Ecrire puis Lire : memes liaisons, memes actions", static_cast<float32>(lb.Nombre()));

				const NkString fautif = "# trois lignes fautives, deux bonnes\n"
										"lier Voler Key:SPACE\n"
										"lier Sauter Key:BLURB\n"
										"lier Sauter Key:SPACE 1 j=*\n"
										"zone Sauter 0.1 x 0.2 0.3\n"
										"stickx Avancer 0 0 0.5 1 1 r=0.1\n";
				const NkRapportLiaisons rf = lb.Lire(fautif);
				const bool refus = rf.erreurs.Size() == 3u && rf.appliquees == 2u && lb.Nombre() == 2 &&
								   rf.erreurs[0].Find("ligne 2") != NkString::npos &&
								   rf.erreurs[2].Find("ligne 5") != NkString::npos;
				Temoin(refus, "(k9n) trois lignes refusees avec leur numero, deux appliquees",
					   static_cast<float32>(rf.erreurs.Size()));
				memory::NkGetDefaultAllocator().Delete(pa);
				memory::NkGetDefaultAllocator().Delete(pb);
			}

			// ── (k10) reconfiguration ─────────────────────────────────────────
			{
				const int32 iEspace = e.Liaisons().Trouver(A_SAUTER, NkInputCode::Key(NkKey::NK_SPACE));
				const bool arme = e.CapturerProchaineEntree(iEspace);
				Touche(e, NkKey::NK_K, true); // capturee : ne presse rien
				Touche(e, NkKey::NK_K, false);
				Touche(e, NkKey::NK_K, true);
				e.Trame(&pads.sys);
				const bool parK = e.Actions(0).Enfoncee(A_SAUTER);
				Touche(e, NkKey::NK_K, false);
				Touche(e, NkKey::NK_SPACE, true);
				e.Trame(&pads.sys);
				const bool plusEspace = !e.Actions(0).Enfoncee(A_SAUTER);
				Touche(e, NkKey::NK_SPACE, false);

				const int32 iSud = e.Liaisons().Trouver(A_SAUTER, NkInputCode::Gamepad(NkGamepadButton::NK_GP_SOUTH));
				e.CapturerProchaineEntree(iSud);
				pads.banc->Bouton(0, NkGamepadButton::NK_GP_EAST, true);
				pads.Sonder();
				e.Trame(&pads.sys);
				pads.banc->Bouton(0, NkGamepadButton::NK_GP_EAST, false);
				pads.Sonder();
				e.Trame(&pads.sys);
				const bool est = e.Liaisons()[iSud].code == NkInputCode::Gamepad(NkGamepadButton::NK_GP_EAST);
				Temoin(arme && parK && plusEspace && est && !e.CaptureEnCours(),
					   "(k10) capture : Espace devient K ; le bouton Sud devient Est", 0.f);
			}

			// ── (k11) ToutRelacher ─────────────────────────────────────────────
			{
				Touche(e, NkKey::NK_D, true);
				e.Trame(&pads.sys);
				const bool tenue = e.Actions(0).Enfoncee(A_AVANCER);
				e.ToutRelacher(); // le jeu perd la main : le relachement de D ne viendra jamais
				const bool zero = Pres(e.Actions(0).Valeur(A_AVANCER), 0.f);
				e.Trame(&pads.sys);
				Temoin(tenue && zero && !e.Actions(0).Enfoncee(A_AVANCER) && !e.ToucheTenue(NkKey::NK_D),
					   "(k11) ToutRelacher : plus de touche tenue, la trame ne la ressuscite pas", 0.f);
			}

			// ── (k12) la composition IME dans un champ NKGui ──────────────────
			{
				NkChampDeBanc *pc = memory::NkGetDefaultAllocator().New<NkChampDeBanc>();
				NkChampDeBanc &c = *pc;
				c.Image(true, nkgui::NkGuiInputFlags::None); // le clic donne le focus
				c.Image(false, nkgui::NkGuiInputFlags::None);
				const uint32 sans = c.PixelsClairs();
				c.ctx.input.SetComposition("abc", 3);
				c.Image(false, nkgui::NkGuiInputFlags::None);
				const uint32 avec = c.PixelsClairs();
				const bool zone = c.ctx.imeZoneValid && c.ctx.imeZone.x > 4.f && c.ctx.imeZone.w > 1.5f;
				c.Png(std::getenv("NK_UNKENY_BANC_CAPTURES"), "entrees_composition.png");
				const bool intact = std::strcmp(c.tampon, "x") == 0;
				Temoin(c.policeOk && zone && avec > sans + 20u && intact,
					   "(k12) composition : zone IME publiee, soulignee au curseur, tampon intact",
					   static_cast<float32>(avec - sans));

				c.ctx.input.ClearComposition();
				c.Image(false, nkgui::NkGuiInputFlags::Password);
				const uint32 mdpSans = c.PixelsClairs();
				c.ctx.input.SetComposition("abc", 3);
				c.Image(false, nkgui::NkGuiInputFlags::Password);
				const uint32 mdpAvec = c.PixelsClairs();
				Temoin(mdpAvec == mdpSans, "(k12n) mot de passe : la composition n'est pas montree en clair",
					   static_cast<float32>(mdpAvec) - static_cast<float32>(mdpSans));
				memory::NkGetDefaultAllocator().Delete(pc);
			}

			// ── (k13) la navigation au focus dans NKGui (manette) ─────────────
			{
				struct NkMenuDeBanc {
						nkgui::NkGuiContext ctx;
						nkgui::NkGuiNavigation nav;
						int32 clics[3] = {};

						void Image(nkgui::NkGuiNavDirection d, bool activer) {
							nkgui::NkGuiNavAvancer(ctx, nav, d, activer, false);
							ctx.BeginFrame(1.f / 60.f);
							ctx.BeginLayout(nkgui::NkRect{0.f, 0.f, 200.f, 200.f});
							static const char *kNoms[3] = {"Un", "Deux", "Trois"};
							for (int32 i = 0; i < 3; ++i) {
								if (nkgui::Button(ctx, kNoms[i])) {
									++clics[i];
								}
							}
							nkgui::NkGuiNavDessiner(ctx, nav);
							ctx.EndFrame();
						}

						/// Bas, Bas, puis Sud : quatre images pour que le clic complet
						/// (viser, appuyer, relacher) ait lieu.
						void Suite() {
							Image(nkgui::NkGuiNavDirection::Aucune, false);
							Image(nkgui::NkGuiNavDirection::Bas, false);
							Image(nkgui::NkGuiNavDirection::Bas, false);
							for (int32 k = 0; k < 4; ++k) {
								Image(nkgui::NkGuiNavDirection::Aucune, k == 0);
							}
						}
				};
				NkMenuDeBanc *pm = memory::NkGetDefaultAllocator().New<NkMenuDeBanc>();
				NkMenuDeBanc &m = *pm;
				m.ctx.Init(200, 200);
				m.ctx.input.mousePos = nkgui::NkVec2{-1000.f, -1000.f};
				// ETEINTE (le defaut) : la meme suite de gestes ne fait RIEN.
				m.Suite();
				const bool inerte = m.clics[0] + m.clics[1] + m.clics[2] == 0 && m.ctx.input.mousePos.x == -1000.f;
				// ALLUMEE : premier geste = montrer « Un », second = descendre sur
				// « Deux », puis activer.
				m.nav.actif = true;
				m.Image(nkgui::NkGuiNavDirection::Aucune, false); // le releve commence
				m.Suite();
				Temoin(inerte, "(k13n) navigation eteinte (defaut) : aucun clic, pointeur intact", 0.f);
				Temoin(m.clics[1] == 1 && m.clics[0] == 0 && m.clics[2] == 0,
					   "(k13) manette dans NKGui : Bas, Bas, Sud -> « Deux » active, lui seul",
					   static_cast<float32>(m.clics[1]));
				memory::NkGetDefaultAllocator().Delete(pm);
			}

			memory::NkGetDefaultAllocator().Delete(pe);
			std::printf("\n%s : %d reussis, %d echec%s\n", gEchecs == 0 ? "BANC ENTREES REUSSI" : "BANC ENTREES EN ECHEC",
						gReussis, gEchecs, gEchecs > 1 ? "s" : "");
			return gEchecs == 0 ? 0 : 1;
		}

	} // namespace unkeny
} // namespace nkentseu
