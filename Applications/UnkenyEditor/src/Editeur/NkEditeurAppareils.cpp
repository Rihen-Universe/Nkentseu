// -----------------------------------------------------------------------------
// FICHIER: Editeur/NkEditeurAppareils.cpp
// DESCRIPTION: Le catalogue des appareils simules, la rotation exacte et les
//              regles de la zone sure de chaque systeme (document 03).
//
// ⚠️ D'OU VIENNENT LES REGLES (documentation publique, consultee le 2026-10-01)
//   iOS / iPadOS — UIView.safeAreaInsets et Apple HIG « Layout »
//     (developer.apple.com/design/human-interface-guidelines/layout) :
//     - portrait : la marge haute couvre l'encoche ou l'ilot ; l'indicateur
//       d'accueil prend 34 pt en bas ;
//     - paysage, ecran a decoupe : la MEME marge a gauche ET a droite (le
//       systeme ne dit pas de quel cote est la camera), le haut a 0 (la barre
//       d'etat se cache en paysage sur iPhone), l'indicateur 21 pt en bas ;
//     - iPad : barre d'etat et indicateur gardes en paysage ;
//     - portrait inverse : refuse sur iPhone (masque par defaut
//       `UIInterfaceOrientationMaskAllButUpsideDown`).
//   Android — WindowInsets / DisplayCutout
//     (developer.android.com/develop/ui/views/layout/display-cutout) :
//     - la decoupe ne retient que SON bord (getSafeInsetLeft... d'un seul
//       cote), en mode `shortEdges`, celui des jeux plein ecran ;
//     - la barre d'etat reste en haut de l'interface, en paysage aussi ;
//     - navigation par gestes : en bas de l'interface ; a trois boutons : sur
//       le bord PHYSIQUE bas du telephone, donc sur un cote en paysage ;
//     - portrait inverse : non propose par defaut sur un telephone
//       (`config_allowAllRotations` faux).
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Editeur/NkEditeurAppareils.h"

namespace nkentseu {
	namespace editeur {

		namespace {

			// Les boutons d'un iPhone (gauche : action, volume + et - ; droite :
			// marche) et d'un telephone Android (droite : marche puis volume).
			void BoutonsIPhone(NkProfilAppareil &p) {
				p.boutons[0] = NkBoutonAppareil{3, 0.16f, 0.045f};
				p.boutons[1] = NkBoutonAppareil{3, 0.23f, 0.075f};
				p.boutons[2] = NkBoutonAppareil{3, 0.32f, 0.075f};
				p.boutons[3] = NkBoutonAppareil{1, 0.25f, 0.11f};
				p.nbBoutons = 4;
			}

			void BoutonsAndroid(NkProfilAppareil &p) {
				p.boutons[0] = NkBoutonAppareil{1, 0.22f, 0.07f};
				p.boutons[1] = NkBoutonAppareil{1, 0.33f, 0.14f};
				p.nbBoutons = 2;
			}

			void BoutonsIPad(NkProfilAppareil &p) {
				p.boutons[0] = NkBoutonAppareil{0, 0.80f, 0.08f};
				p.boutons[1] = NkBoutonAppareil{1, 0.07f, 0.05f};
				p.boutons[2] = NkBoutonAppareil{1, 0.13f, 0.05f};
				p.nbBoutons = 3;
			}

			/// Une decoupe centree sur le bord haut, en portrait.
			NkRectAppareil Centree(float32 largeurEcran, float32 w, float32 h, float32 y) {
				return NkRectAppareil{(largeurEcran - w) * 0.5f, y, w, h};
			}

			NkProfilAppareil Telephone(const char *nom, NkSystemeAppareil s, uint32 l, uint32 h, float32 d) {
				NkProfilAppareil p;
				p.nom = nom;
				p.famille = NkFamilleAppareil::NK_TELEPHONE;
				p.systeme = s;
				p.largeur = l;
				p.hauteur = h;
				p.densite = d;
				p.bordure = 12.f;
				p.bordureHautBas = 12.f;
				return p;
			}

			NkProfilAppareil Ecran(const char *nom, NkFamilleAppareil f, uint32 l, uint32 h, float32 d) {
				NkProfilAppareil p;
				p.nom = nom;
				p.famille = f;
				p.largeur = l;
				p.hauteur = h;
				p.densite = d;
				return p;
			}

			/// Le catalogue, EN PORTRAIT (orientation naturelle), sans zone sure :
			/// NkProfil la calcule.
			NkProfilAppareil Catalogue(int32 i) {
				NkProfilAppareil p;
				switch (i) {
					case 1: // encoche haute + indicateur de geste : le cas dur
						p = Telephone("Téléphone à encoche", NkSystemeAppareil::NK_IOS, 390, 844, 3.f);
						p.provenance = "Apple, iPhone 14 : 390x844 pt @3x. UIView.safeAreaInsets : portrait haut 47, "
									   "bas 34 ; paysage gauche 47 ET droite 47, bas 21 (Apple HIG « Layout »). "
									   "Rayon et encoche (162x34 pt) : ordres de grandeur releves publiquement.";
						p.rayonCoins = 47.f;
						p.decoupe = NkTypeDecoupe::NK_ENCOCHE;
						p.rectDecoupe = Centree(390.f, 162.f, 34.f, 0.f);
						p.margeDecoupe = 47.f;
						p.decoupeSymetrique = true;
						p.indicateur = 34.f;
						p.indicateurPaysage = 21.f;
						BoutonsIPhone(p);
						break;
					case 2:
						p = Telephone("Téléphone Android", NkSystemeAppareil::NK_ANDROID, 412, 915, 2.625f);
						p.provenance = "Android, telephone 20:9 courant (412x915 dp, densite 2,625). Barre d'etat 24 dp, "
									   "navigation par gestes 24 dp. Poinçon centre : DisplayCutout, marge du SEUL cote "
									   "de la decoupe en paysage (developer.android.com, display-cutout, shortEdges).";
						p.rayonCoins = 28.f;
						p.decoupe = NkTypeDecoupe::NK_POINCON;
						p.rectDecoupe = Centree(412.f, 18.f, 18.f, 6.f);
						p.barreEtat = 24.f;
						p.barreEtatPaysage = true;
						p.margeDecoupe = 24.f;
						p.indicateur = 24.f;
						p.indicateurPaysage = 24.f;
						BoutonsAndroid(p);
						break;
					case 3:
						p = Telephone("Téléphone compact", NkSystemeAppareil::NK_ANDROID, 360, 640, 2.f);
						p.provenance = "Android, ancien telephone 16:9 (360x640 dp, densite 2). Barre d'etat 24 dp ; la "
									   "barre de navigation est HORS de la fenetre (boutons sous l'ecran).";
						p.barreEtat = 24.f;
						p.barreEtatPaysage = true;
						p.bordureHautBas = 64.f;
						BoutonsAndroid(p);
						break;
					case 4:
						p = Ecran("Tablette", NkFamilleAppareil::NK_TABLETTE, 820, 1180, 2.f);
						p.systeme = NkSystemeAppareil::NK_IOS;
						p.provenance = "Apple, iPad Air 10,9 po : 820x1180 pt @2x. UIView.safeAreaInsets : haut 24, "
									   "bas 20, en portrait COMME en paysage (Apple HIG « Layout »).";
						p.rayonCoins = 18.f;
						p.barreEtat = 24.f;
						p.barreEtatPaysage = true;
						p.indicateur = 20.f;
						p.indicateurPaysage = 20.f;
						p.bordure = 22.f;
						p.bordureHautBas = 22.f;
						BoutonsIPad(p);
						break;
					case 5:
						p = Ecran("Navigateur", NkFamilleAppareil::NK_NAVIGATEUR, 960, 600, 1.f);
						p.systeme = NkSystemeAppareil::NK_WEB;
						p.provenance = "Fenetre de navigateur. Sans `viewport-fit=cover`, la page reste hors des "
									   "decoupes : env(safe-area-inset-*) vaut 0 (NKWindow Emscripten rend {0,0,0,0}).";
						break;
					case 6:
						p = Telephone("Téléphone à îlot", NkSystemeAppareil::NK_IOS, 393, 852, 3.f);
						p.provenance = "Apple, iPhone 15 : 393x852 pt @3x. UIView.safeAreaInsets : portrait haut 59, "
									   "bas 34 ; paysage gauche 59 ET droite 59, bas 21 (Apple HIG « Layout »). Ilot "
									   "126x37 pt a 11 pt du bord, rayon 55 pt : ordres de grandeur publics.";
						p.rayonCoins = 55.f;
						p.decoupe = NkTypeDecoupe::NK_ILOT;
						p.rectDecoupe = Centree(393.f, 126.f, 37.f, 11.f);
						p.margeDecoupe = 59.f;
						p.decoupeSymetrique = true;
						p.indicateur = 34.f;
						p.indicateurPaysage = 21.f;
						BoutonsIPhone(p);
						break;
					case 7:
						p = Telephone("Téléphone à poinçon (angle)", NkSystemeAppareil::NK_ANDROID, 384, 854, 2.8125f);
						p.provenance = "Android, telephone 20:9 a poinçon d'angle (1080x2400 px, 384x854 dp). Barre "
									   "d'etat elargie a la decoupe (28 dp), gestes 24 dp ; DisplayCutout : un seul cote.";
						p.rayonCoins = 26.f;
						p.decoupe = NkTypeDecoupe::NK_POINCON;
						p.rectDecoupe = NkRectAppareil{18.f, 7.f, 20.f, 20.f};
						p.barreEtat = 28.f;
						p.barreEtatPaysage = true;
						p.margeDecoupe = 28.f;
						p.indicateur = 24.f;
						p.indicateurPaysage = 24.f;
						BoutonsAndroid(p);
						break;
					case 8:
						p = Telephone("Téléphone à goutte d'eau", NkSystemeAppareil::NK_ANDROID, 360, 760, 2.f);
						p.provenance = "Android, entree de gamme 19:9 (720x1520 px, 360x760 dp). Goutte d'eau ; "
									   "navigation A TROIS BOUTONS, 48 dp, sur le bord PHYSIQUE bas : sur un cote en "
									   "paysage (developer.android.com, WindowInsets).";
						p.rayonCoins = 20.f;
						p.decoupe = NkTypeDecoupe::NK_GOUTTE;
						p.rectDecoupe = Centree(360.f, 28.f, 24.f, 0.f);
						p.barreEtat = 24.f;
						p.barreEtatPaysage = true;
						p.margeDecoupe = 24.f;
						p.navBoutons = 48.f;
						p.bordureHautBas = 16.f;
						BoutonsAndroid(p);
						break;
					case 9:
						p = Telephone("Téléphone sans découpe", NkSystemeAppareil::NK_IOS, 375, 667, 2.f);
						p.provenance = "Apple, iPhone SE (3e gen.) : 375x667 pt @2x. Barre d'etat 20 pt en portrait, "
									   "aucune marge en paysage (UIView.safeAreaInsets) ; bouton d'accueil.";
						p.barreEtat = 20.f;
						p.bordureHautBas = 66.f;
						p.boutonAccueil = true;
						BoutonsIPhone(p);
						break;
					case 10:
						p = Ecran("Tablette 4:3", NkFamilleAppareil::NK_TABLETTE, 810, 1080, 2.f);
						p.systeme = NkSystemeAppareil::NK_IOS;
						p.provenance = "Apple, iPad (9e gen.) 10,2 po : 810x1080 pt @2x. Barre d'etat 20 pt, bouton "
									   "d'accueil, pas d'indicateur de geste.";
						p.barreEtat = 20.f;
						p.barreEtatPaysage = true;
						p.bordure = 28.f;
						p.bordureHautBas = 62.f;
						p.boutonAccueil = true;
						BoutonsIPad(p);
						break;
					case 11:
						p = Ecran("Tablette 16:10", NkFamilleAppareil::NK_TABLETTE, 1280, 800, 2.f);
						p.systeme = NkSystemeAppareil::NK_ANDROID;
						p.provenance = "Android, tablette 16:10 (2560x1600 px, 1280x800 dp), orientation naturelle "
									   "PAYSAGE. Barre d'etat 24 dp, barre des taches / gestes 24 dp en bas de "
									   "l'interface ; camera dans la bordure.";
						p.rayonCoins = 16.f;
						p.barreEtat = 24.f;
						p.barreEtatPaysage = true;
						p.indicateur = 24.f;
						p.indicateurPaysage = 24.f;
						p.bordure = 20.f;
						p.bordureHautBas = 20.f;
						p.boutons[0] = NkBoutonAppareil{0, 0.08f, 0.06f};
						p.boutons[1] = NkBoutonAppareil{0, 0.17f, 0.09f};
						p.nbBoutons = 2;
						break;
					case 12:
						p = Ecran("Pliable, écran intérieur", NkFamilleAppareil::NK_PLIABLE, 690, 829, 2.625f);
						p.systeme = NkSystemeAppareil::NK_ANDROID;
						p.provenance = "Samsung, Galaxy Z Fold5, ecran interieur 2176x1812 px (fiche technique "
									   "Samsung), presque carre. Camera SOUS l'ecran : aucune decoupe. Barre d'etat et "
									   "gestes 24 dp.";
						p.rayonCoins = 18.f;
						p.barreEtat = 24.f;
						p.barreEtatPaysage = true;
						p.indicateur = 24.f;
						p.indicateurPaysage = 24.f;
						p.bordure = 10.f;
						p.bordureHautBas = 10.f;
						BoutonsAndroid(p);
						break;
					case 13:
						p = Telephone("Pliable, écran extérieur", NkSystemeAppareil::NK_ANDROID, 344, 882, 2.625f);
						p.famille = NkFamilleAppareil::NK_PLIABLE;
						p.provenance = "Samsung, Galaxy Z Fold5, ecran exterieur 904x2316 px, 23,1:9 (fiche technique "
									   "Samsung) : etroit. Poinçon centre ; barre d'etat et gestes 24 dp.";
						p.rayonCoins = 22.f;
						p.decoupe = NkTypeDecoupe::NK_POINCON;
						p.rectDecoupe = Centree(344.f, 16.f, 16.f, 7.f);
						p.barreEtat = 24.f;
						p.barreEtatPaysage = true;
						p.margeDecoupe = 24.f;
						p.indicateur = 24.f;
						p.indicateurPaysage = 24.f;
						BoutonsAndroid(p);
						break;
					case 14:
						p = Ecran("Bureau 16:10", NkFamilleAppareil::NK_BUREAU, 1280, 800, 1.f);
						p.provenance = "Ecran 16:10 (1280x800 points). Un bureau ne rend aucune marge.";
						break;
					case 15:
						p = Ecran("Bureau 21:9", NkFamilleAppareil::NK_BUREAU, 2560, 1080, 1.f);
						p.provenance = "Ecran ultra-large 21:9 (2560x1080). Un bureau ne rend aucune marge.";
						break;
					case 16:
						p = Ecran("Bureau 4:3", NkFamilleAppareil::NK_BUREAU, 1024, 768, 1.f);
						p.provenance = "Ecran 4:3 (1024x768). Un bureau ne rend aucune marge.";
						break;
					case 17:
						p = Ecran("TV", NkFamilleAppareil::NK_TV, 960, 540, 2.f);
						p.systeme = NkSystemeAppareil::NK_ANDROID;
						p.provenance = "Android TV : interface 960x540 dp (1920x1080 px). Marge de securite CONSEILLEE "
									   "de 5 % (48 dp / 27 dp, « overscan », developer.android.com/training/tv) : le "
									   "systeme ne la rend PAS, elle est dessinee a part.";
						p.margeConseillee = 0.05f;
						break;
					case 18:
						p = Ecran("Console portable", NkFamilleAppareil::NK_CONSOLE, 1280, 720, 1.f);
						p.provenance = "Nintendo Switch : ecran 1280x720 (fiche technique Nintendo). Aucune marge.";
						break;
					case 19:
						p = Ecran("Console portable 16:10", NkFamilleAppareil::NK_CONSOLE, 1280, 800, 1.f);
						p.provenance = "Valve, Steam Deck : ecran 1280x800 (fiche technique Valve). Aucune marge.";
						break;
					case 20:
						p = Ecran("Montre ronde", NkFamilleAppareil::NK_MONTRE, 225, 225, 2.f);
						p.systeme = NkSystemeAppareil::NK_ANDROID;
						p.provenance = "Wear OS, montre ronde 450x450 px. Ecran ROND : le systeme ne rend aucune marge ; "
									   "carre inscrit CONSEILLE, 14,6 % par cote (BoxInsetLayout, "
									   "developer.android.com/training/wearables).";
						p.rond = true;
						p.rayonCoins = 112.5f;
						p.margeConseillee = 0.146447f;
						p.bordure = 16.f;
						p.bordureHautBas = 16.f;
						p.boutons[0] = NkBoutonAppareil{1, 0.40f, 0.20f};
						p.boutons[1] = NkBoutonAppareil{1, 0.70f, 0.10f};
						p.nbBoutons = 2;
						break;
					default:
						p = Ecran("Bureau", NkFamilleAppareil::NK_BUREAU, 1280, 720, 1.f);
						p.provenance = "Ecran 16:9 (1280x720 points). Un bureau ne rend aucune marge (NKWindow Win32, "
									   "X11, Wayland, Cocoa : {0,0,0,0}).";
						break;
				}
				return p;
			}

			constexpr int32 kNbCatalogue = 21;

		} // namespace

		int32 NkNbProfils() noexcept {
			return kNbCatalogue;
		}

		NkProfilAppareil NkProfil(int32 i) noexcept {
			NkProfilAppareil p = Catalogue(i >= 0 && i < kNbCatalogue ? i : 0);
			p.orientation = NkOrientation::NK_PORTRAIT;
			p.zoneSure = NkMargesSysteme(p, NkOrientation::NK_PORTRAIT);
			return p;
		}

		void NkTournerPoint(NkOrientation o, float32 l, float32 h, float32 x, float32 y, float32 &rx,
							float32 &ry) noexcept {
			switch (o) {
				case NkOrientation::NK_PAYSAGE_GAUCHE: // le haut passe a gauche
					rx = y;
					ry = l - x;
					break;
				case NkOrientation::NK_PORTRAIT_INVERSE:
					rx = l - x;
					ry = h - y;
					break;
				case NkOrientation::NK_PAYSAGE_DROITE: // le haut passe a droite
					rx = h - y;
					ry = x;
					break;
				default:
					rx = x;
					ry = y;
					break;
			}
		}

		NkRectAppareil NkTournerRect(NkOrientation o, float32 l, float32 h, const NkRectAppareil &r) noexcept {
			float32 ax, ay, bx, by;
			NkTournerPoint(o, l, h, r.x, r.y, ax, ay);
			NkTournerPoint(o, l, h, r.x + r.w, r.y + r.h, bx, by);
			NkRectAppareil s;
			s.x = ax < bx ? ax : bx;
			s.y = ay < by ? ay : by;
			s.w = ax < bx ? bx - ax : ax - bx;
			s.h = ay < by ? by - ay : ay - by;
			return s;
		}

		bool NkOrientationNonProposee(const NkProfilAppareil &p, NkOrientation o) noexcept {
			if (o == NkOrientation::NK_PORTRAIT) {
				return false;
			}
			// Une montre, une TV, une console ne tournent pas leur interface.
			if (p.famille == NkFamilleAppareil::NK_MONTRE || p.famille == NkFamilleAppareil::NK_TV ||
				p.famille == NkFamilleAppareil::NK_CONSOLE) {
				return true;
			}
			// iPhone (masque AllButUpsideDown) et telephone Android
			// (config_allowAllRotations faux) : pas de portrait inverse.
			const bool telephone = p.famille == NkFamilleAppareil::NK_TELEPHONE || p.famille == NkFamilleAppareil::NK_PLIABLE;
			const bool mobile = p.systeme == NkSystemeAppareil::NK_IOS || p.systeme == NkSystemeAppareil::NK_ANDROID;
			return telephone && mobile && o == NkOrientation::NK_PORTRAIT_INVERSE;
		}

		NkSafeAreaInsets NkMargesSysteme(const NkProfilAppareil &p, NkOrientation o) noexcept {
			// « Paysage » = un quart de tour depuis l'orientation NATURELLE.
			const bool quart = NkEstPaysage(o);
			float32 bords[4] = {0.f, 0.f, 0.f, 0.f}; // haut, droite, bas, gauche -- de l'ECRAN
			auto poser = [&bords](int32 bord, float32 v) {
				if (v > bords[bord]) {
					bords[bord] = v;
				}
			};
			// 1. La barre d'etat : en haut de l'INTERFACE (elle ne tourne pas
			//    avec l'appareil). iPhone : cachee en paysage.
			if (!quart || p.barreEtatPaysage) {
				poser(0, p.barreEtat);
			}
			// 2. La decoupe : sur le bord PHYSIQUE haut, ou qu'il soit passe.
			//    ⚠️ iOS reserve la meme marge du cote OPPOSE en paysage : c'est
			//    ce que rend UIView.safeAreaInsets (47/47 ou 59/59), et c'est ce
			//    que l'ancienne simulation ignorait.
			if (p.margeDecoupe > 0.f) {
				const int32 b = NkBordTourne(o, 0);
				poser(b, p.margeDecoupe);
				if (quart && p.decoupeSymetrique) {
					poser((b + 2) % 4, p.margeDecoupe);
				}
			}
			// 3. L'indicateur de geste : en bas de l'INTERFACE.
			poser(2, quart ? p.indicateurPaysage : p.indicateur);
			// 4. La navigation a trois boutons d'un telephone Android : sur le
			//    bord PHYSIQUE bas (un cote en paysage).
			if (p.navBoutons > 0.f) {
				poser(NkBordTourne(o, 2), p.navBoutons);
			}
			return NkSafeAreaInsets(bords[0], bords[2], bords[3], bords[1]);
		}

		NkProfilAppareil NkOrienter(const NkProfilAppareil &p, NkOrientation o) noexcept {
			// Un profil deja tourne revient d'abord en portrait : NkOrienter pose
			// une orientation ABSOLUE, elle ne s'ajoute pas a la precedente.
			NkProfilAppareil base = p;
			if (p.orientation != NkOrientation::NK_PORTRAIT) {
				const NkOrientation inverse = static_cast<NkOrientation>((4 - static_cast<int32>(p.orientation)) % 4);
				base.largeur = NkEstPaysage(p.orientation) ? p.hauteur : p.largeur;
				base.hauteur = NkEstPaysage(p.orientation) ? p.largeur : p.hauteur;
				base.rectDecoupe = NkTournerRect(inverse, static_cast<float32>(p.largeur), static_cast<float32>(p.hauteur),
												 p.rectDecoupe);
				base.orientation = NkOrientation::NK_PORTRAIT;
			}
			NkProfilAppareil r = base;
			if (o >= NkOrientation::NK_COUNT) {
				o = NkOrientation::NK_PORTRAIT;
			}
			const float32 l = static_cast<float32>(base.largeur);
			const float32 h = static_cast<float32>(base.hauteur);
			if (NkEstPaysage(o)) {
				r.largeur = base.hauteur;
				r.hauteur = base.largeur;
			}
			r.rectDecoupe = NkTournerRect(o, l, h, base.rectDecoupe);
			r.zoneSure = NkMargesSysteme(base, o);
			r.orientation = o;
			r.orientationNonProposee = NkOrientationNonProposee(base, o);
			return r;
		}

		renderer::NkLayoutInfo NkLayoutSimule(const NkProfilAppareil &p) noexcept {
			renderer::NkLayoutInfo info;
			const float32 d = p.densite > 0.f ? p.densite : 1.f;
			info.width = static_cast<uint32>(static_cast<float32>(p.largeur) * d + 0.5f);
			info.height = static_cast<uint32>(static_cast<float32>(p.hauteur) * d + 0.5f);
			info.density = d;
			info.safeArea = NkSafeAreaInsets(p.zoneSure.top * d, p.zoneSure.bottom * d, p.zoneSure.left * d,
											 p.zoneSure.right * d);
			return info;
		}

		const char *NkNomOrientation(NkOrientation o, bool naturelPaysage) noexcept {
			if (naturelPaysage) {
				static const char *kNoms[4] = {"Paysage", "Portrait (gauche)", "Paysage inversé", "Portrait (droite)"};
				return kNoms[static_cast<int32>(o) & 3];
			}
			static const char *kNoms[4] = {"Portrait", "Paysage gauche", "Portrait inversé", "Paysage droite"};
			return kNoms[static_cast<int32>(o) & 3];
		}

		const char *NkNomSysteme(NkSystemeAppareil s) noexcept {
			static const char *kNoms[4] = {"aucun", "iOS", "Android", "Web"};
			const int32 i = static_cast<int32>(s);
			return i >= 0 && i < 4 ? kNoms[i] : "?";
		}

		const char *NkNomDecoupe(NkTypeDecoupe d) noexcept {
			static const char *kNoms[5] = {"aucune", "encoche", "îlot", "poinçon", "goutte d'eau"};
			const int32 i = static_cast<int32>(d);
			return i >= 0 && i < 5 ? kNoms[i] : "?";
		}

	} // namespace editeur
} // namespace nkentseu
