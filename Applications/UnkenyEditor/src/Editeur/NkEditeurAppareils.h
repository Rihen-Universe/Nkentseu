// =============================================================================
// NkEditeurAppareils.h — simuler la ZONE SURE sans avoir l'appareil
//
// A QUOI SERT CE FICHIER
//   Montrer, sur un ecran de bureau, ce que la mise en page donnera sur un
//   telephone a encoche, sur une tablette, dans un navigateur.
//
// ⚠️ POURQUOI C'EST LA PREMIERE FONCTION DE L'EDITEUR, ET PAS LA DERNIERE
//   Regle du depot (Rodolf, 2026-08-18) : « L'editeur doit pouvoir SIMULER des
//   zones sures pour qu'on voie le debordement AVANT de deployer. Sans ca, le
//   defaut se decouvre sur le telephone, quand il coute le plus cher. »
//   Un bouton sous l'indicateur de geste n'est pas mal place : il est
//   INATTEIGNABLE. Et on ne le voit jamais depuis sa machine de developpement —
//   c'est le defaut structurellement invisible par excellence.
//
// ⚠️ CES CHIFFRES SONT DES ORDRES DE GRANDEUR, PAS DES MESURES
//   Ils viennent des documentations publiques d'Apple et de Google, pas d'un
//   appareil pose sur la table. Ils servent a VOIR un debordement, pas a
//   certifier une mise en page. Un profil qui pretendrait a l'exactitude sans
//   l'avoir mesuree serait pire qu'utile : on lui ferait confiance.
//   Chaque profil porte sa PROVENANCE (`provenance`), lisible dans les Details.
//
// ⚠️ LA ZONE SURE N'EST PAS DECRITE ICI, ELLE EST DEDUITE (document 03, 01/10)
//   Un profil decrit ce qui MANGE l'ecran (barre d'etat, decoupe de camera,
//   indicateur de geste, barre de navigation) et ou cela se trouve sur
//   l'appareil. NkMargesSysteme en tire les marges dans chaque orientation,
//   selon les regles du SYSTEME (iOS et Android ne tournent pas pareil). Une
//   table de marges par orientation, ecrite a la main, se contredirait au
//   premier appareil ajoute.
//
// ⚠️ LA MEME STRUCTURE QUE SUR L'APPAREIL
//   NkLayoutSimule rend un `renderer::NkLayoutInfo` : ce que NKCanvas lit dans
//   NKWindow (GetSafeAreaInsets, en pixels physiques) sur le vrai telephone.
//   Le jeu lit donc la MEME chose dans l'editeur et sur l'appareil.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - un appareil de plus -> le catalogue de NkEditeurAppareils.cpp, avec sa
//                            PROVENANCE
//   - une orientation     -> deja gere : NkOrienter tourne la geometrie et
//                            redemande les marges a NkMargesSysteme
// =============================================================================
#pragma once

#include "NKCanvas/App/NkCanvasApp.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKEvent/NkSafeArea.h"
#include "Unkeny/Scene/NkUnkenyEcran.h"
#include "NKECS/World/NkWorld.h"

namespace nkentseu {
	namespace editeur {

		/// Les quatre orientations, numerotees par QUARTS DE TOUR de l'appareil
		/// dans le sens INVERSE des aiguilles d'une montre. L'interface, elle,
		/// reste droite : c'est ce que fait le systeme.
		///   paysage GAUCHE = le haut de l'appareil (la camera) passe a GAUCHE
		///                    (UIDeviceOrientation.landscapeLeft, Surface.ROTATION_90)
		///   paysage DROITE = le haut de l'appareil passe a DROITE
		///                    (UIDeviceOrientation.landscapeRight, Surface.ROTATION_270)
		enum class NkOrientation : uint8 {
			NK_PORTRAIT = 0,
			NK_PAYSAGE_GAUCHE,
			NK_PORTRAIT_INVERSE,
			NK_PAYSAGE_DROITE,
			NK_COUNT
		};

		/// Le systeme decide des regles de la zone sure.
		enum class NkSystemeAppareil : uint8 { NK_AUCUN = 0, NK_IOS, NK_ANDROID, NK_WEB, NK_COUNT };

		/// La famille decide du CADRE dessine autour de l'ecran.
		enum class NkFamilleAppareil : uint8 {
			NK_BUREAU = 0,
			NK_TELEPHONE,
			NK_TABLETTE,
			NK_PLIABLE,
			NK_NAVIGATEUR,
			NK_TV,
			NK_CONSOLE,
			NK_MONTRE,
			NK_COUNT
		};

		enum class NkTypeDecoupe : uint8 {
			NK_AUCUNE = 0,
			NK_ENCOCHE, ///< accrochee au bord haut, coins bas arrondis
			NK_ILOT,	///< pilule detachee du bord (Dynamic Island)
			NK_POINCON, ///< un trou rond
			NK_GOUTTE,	///< petite encoche en U
			NK_COUNT
		};

		/// Un rectangle de l'ECRAN de l'appareil, en points, Y vers le bas.
		struct NkRectAppareil {
				float32 x = 0.f;
				float32 y = 0.f;
				float32 w = 0.f;
				float32 h = 0.f;
		};

		/// Un bouton physique, pour le cadre. Decrit en PORTRAIT : `cote` 0 haut,
		/// 1 droite, 2 bas, 3 gauche ; `debut` et `longueur` en fraction du cote.
		struct NkBoutonAppareil {
				uint8 cote = 1;
				float32 debut = 0.f;
				float32 longueur = 0.f;
		};

		constexpr int32 NK_APPAREIL_BOUTONS_MAX = 4;

		struct NkProfilAppareil {
				const char *nom = "Bureau";
				/// En POINTS (pixels logiques) ; les pixels = points x densite.
				uint32 largeur = 1280;
				uint32 hauteur = 720;
				float32 densite = 1.f;
				/// La zone sure, en POINTS, dans l'orientation du profil. Calculee
				/// par NkMargesSysteme a partir des champs ci-dessous : ne pas
				/// l'ecrire a la main dans le catalogue.
				NkSafeAreaInsets zoneSure;

				// --- Depuis le 2026-10-01 (document 03) ---------------------------
				const char *provenance = "";
				NkFamilleAppareil famille = NkFamilleAppareil::NK_BUREAU;
				NkSystemeAppareil systeme = NkSystemeAppareil::NK_AUCUN;
				/// Rayon des coins de l'ECRAN, en points. Un ecran rond : `rond`.
				float32 rayonCoins = 0.f;
				bool rond = false;
				/// La decoupe de camera et son rectangle, dans l'orientation du
				/// profil (en portrait dans le catalogue).
				NkTypeDecoupe decoupe = NkTypeDecoupe::NK_AUCUNE;
				NkRectAppareil rectDecoupe;

				// Ce qui mange l'ecran, en POINTS, decrit en PORTRAIT -- les
				// regles de NkMargesSysteme.
				float32 barreEtat = 0.f;		 ///< en haut de l'INTERFACE
				bool barreEtatPaysage = false;	 ///< reste-t-elle en paysage ?
				float32 margeDecoupe = 0.f;		 ///< marge que la decoupe impose, sur son bord PHYSIQUE
				bool decoupeSymetrique = false;	 ///< iOS : en paysage, la meme marge des DEUX cotes
				float32 indicateur = 0.f;		 ///< indicateur de geste, en bas de l'INTERFACE, portrait
				float32 indicateurPaysage = 0.f; ///< le meme, en paysage
				float32 navBoutons = 0.f;		 ///< Android, trois boutons : sur le bord PHYSIQUE bas
				/// Marge CONSEILLEE (fraction de chaque cote) que le systeme NE rend
				/// PAS : marge des titres d'une TV, carre inscrit d'une montre ronde.
				/// Dessinee a part, jamais versee dans `zoneSure`.
				float32 margeConseillee = 0.f;

				// Le cadre (dessin) : bordures en points, boutons, bouton d'accueil.
				float32 bordure = 0.f;		  ///< bordure laterale, en portrait
				float32 bordureHautBas = 0.f; ///< bordure haute et basse, en portrait
				bool boutonAccueil = false;
				NkBoutonAppareil boutons[NK_APPAREIL_BOUTONS_MAX];
				uint8 nbBoutons = 0;

				NkOrientation orientation = NkOrientation::NK_PORTRAIT;
				/// Le systeme ne tourne PAS l'interface dans cette orientation
				/// (iPhone et telephone Android en portrait inverse) : les marges
				/// montrees sont celles de la rotation, a titre indicatif.
				bool orientationNonProposee = false;
		};

		/// Les profils du catalogue. Ordre volontaire : les six d'origine d'abord
		/// (`--profil=N` et les captures les nomment par leur indice), puis le
		/// reste du catalogue.
		int32 NkNbProfils() noexcept;
		/// Le profil `i` du catalogue, EN PORTRAIT, zone sure calculee. Un indice
		/// hors du catalogue rend le bureau.
		NkProfilAppareil NkProfil(int32 i) noexcept;

		/// Le bord de l'ECRAN (0 haut, 1 droite, 2 bas, 3 gauche) ou se trouve,
		/// dans l'orientation `o`, le bord PHYSIQUE `bord` de l'appareil.
		inline int32 NkBordTourne(NkOrientation o, int32 bord) noexcept {
			return (bord + 4 - static_cast<int32>(o)) % 4;
		}

		inline bool NkEstPaysage(NkOrientation o) noexcept {
			return o == NkOrientation::NK_PAYSAGE_GAUCHE || o == NkOrientation::NK_PAYSAGE_DROITE;
		}

		/// Un point de l'ecran en PORTRAIT (l x h points) vers l'ecran tourne.
		void NkTournerPoint(NkOrientation o, float32 l, float32 h, float32 x, float32 y, float32 &rx,
							float32 &ry) noexcept;
		NkRectAppareil NkTournerRect(NkOrientation o, float32 l, float32 h, const NkRectAppareil &r) noexcept;

		/// Les marges de la zone sure d'un profil decrit en PORTRAIT, dans
		/// l'orientation `o`, selon les regles de son systeme. En points.
		NkSafeAreaInsets NkMargesSysteme(const NkProfilAppareil &portrait, NkOrientation o) noexcept;

		/// L'orientation est-elle refusee par le systeme de ce profil ?
		bool NkOrientationNonProposee(const NkProfilAppareil &portrait, NkOrientation o) noexcept;

		/// LA ROTATION EXACTE : dimensions echangees, decoupe tournee
		/// geometriquement, marges redemandees au systeme. `portrait` doit etre
		/// un profil en portrait (ceux du catalogue le sont).
		NkProfilAppareil NkOrienter(const NkProfilAppareil &portrait, NkOrientation o) noexcept;

		/// Passe un profil en paysage.
		///
		/// ⚠️ GARDE POUR COMPATIBILITE : c'est le PAYSAGE GAUCHE de NkOrienter.
		/// L'ancienne version passait l'encoche a gauche, mettait la marge droite
		/// a zero et reduisait l'indicateur a 60 % : faux pour iOS, qui reserve
		/// la marge des DEUX cotes (document 03, §1).
		inline NkProfilAppareil NkTourner(const NkProfilAppareil &p) noexcept {
			return NkOrienter(p, NkOrientation::NK_PAYSAGE_GAUCHE);
		}

		/// Ce que NKCanvas donnerait au jeu sur cet appareil : taille et zone
		/// sure en PIXELS PHYSIQUES (points x densite), densite. La MEME
		/// structure que `NkCanvasApp::Layout()` sur l'appareil reel.
		renderer::NkLayoutInfo NkLayoutSimule(const NkProfilAppareil &p) noexcept;

		/// Le nom de l'orientation pour ce profil : un bureau ou une TV sont
		/// naturellement en paysage, leur « portrait » n'en est pas un.
		const char *NkNomOrientation(NkOrientation o, bool naturelPaysage) noexcept;
		const char *NkNomSysteme(NkSystemeAppareil s) noexcept;
		const char *NkNomDecoupe(NkTypeDecoupe d) noexcept;

		// =====================================================================
		// L'APPAREIL PERSONNALISE et les REGLAGES du viseur (document 03, §2.2
		// et §2.4) -- enregistres avec la scene, dans `<scene>.nkappareil`.
		// =====================================================================

		/// Les interrupteurs du menu Appareil (NK_A_OPTION_APPAREIL + valeur).
		enum class NkOptionAppareil : uint8 {
			NK_CADRE = 0,	  ///< le cadre de l'appareil (bordure, boutons)
			NK_ZONE_SURE,	  ///< les bandes de la zone sure
			NK_DECOUPE,		  ///< la decoupe de camera
			NK_CADRE_CLAIR,	  ///< cadre clair (sinon sombre)
			NK_APERCU_JEU,	  ///< la camera du JEU dans l'ecran de l'appareil
			NK_PERSONNALISER, ///< copie l'appareil courant dans l'appareil personnalise
			// L'interface ancree a l'ecran (§2.5) : des gestes de l'ecran du jeu.
			NK_AJOUTER_HUD,		///< « + Ajouter » : un element d'interface ancre en haut a droite
			NK_ANCRER_SELECTION, ///< clic droit : la selection devient un element d'interface
			NK_COUNT
		};

		struct NkReglagesAppareil {
				bool voirCadre = true;
				bool voirZoneSure = true;
				bool voirDecoupe = true;
				bool cadreClair = false;
				bool apercuJeu = false;
				/// Le type de base de l'appareil personnalise (indice du
				/// catalogue) : il donne les boutons du cadre.
				int32 persoBase = 2;
				/// L'appareil personnalise, EN PORTRAIT.
				NkProfilAppareil perso;
				/// La camera du JEU selon l'ecran (§2.6) : regle du projet, cuite
				/// avec le jeu (Construire) et montree par l'apercu.
				unkeny::NkRegleCamera regleCamera = unkeny::NkRegleCamera::NK_TOUT_MONTRER;

				NkReglagesAppareil() noexcept;
		};

		/// Une copie modifiable de `base` (ramenee en portrait), nommee
		/// « Personnalisé ».
		NkProfilAppareil NkPersonnaliser(const NkProfilAppareil &base) noexcept;

		/// Le fichier des reglages d'appareil d'une scene : `x.nkscene` ->
		/// `x.nkappareil`.
		NkString NkFichierAppareil(const char *cheminScene);

		/// Le texte du fichier : `cle = valeur` par ligne, `#` commente.
		NkString NkEcrireAppareil(int32 profil, NkOrientation o, const NkReglagesAppareil &r);

		/// Relit ce texte. ⚠️ RETRO-COMPATIBLE DANS LES DEUX SENS : une cle
		/// absente garde sa valeur, une cle inconnue est ignoree. false si le
		/// texte ne porte pas l'entete `format = unkeny.appareil` (rien n'est
		/// alors change).
		bool NkLireAppareil(const NkString &texte, int32 &profil, NkOrientation &o, NkReglagesAppareil &r);

		/// La section « Appareil simulé » de Details > Monde
		/// (NkEditeurAppareilsUi.cpp).
		struct NkEditeurCadre;
		void NkEditeurSectionAppareil(NkEditeurCadre &c);

		/// L'INTERFACE ANCREE A L'ECRAN (NkAncrageEcran2D, Unkeny) : un element
		/// d'interface (sprite + ancrage), la selection ancree, l'exemple
		/// `--exemple=hud`, et le bloc « Ancrage a l'ecran » des Details.
		struct NkEditeurModele;
		ecs::NkEntityId NkEditeurAjouterHud(NkEditeurModele &m, unkeny::NkAncre ancre, const char *nom, uint32 couleur,
											const math::NkVec2f &taille);
		bool NkEditeurAncrerSelection(NkEditeurModele &m);
		void NkEditeurExempleHud(NkEditeurModele &m);
		void NkEditeurBlocAncrage(NkEditeurCadre &c, ecs::NkEntityId id);

		/// `--selftest` : le banc des appareils (NkEditeurBancAppareils.cpp),
		/// compte a part. 0 quand tout tient.
		int32 NkEditeurLancerBancAppareils();

	} // namespace editeur
} // namespace nkentseu
