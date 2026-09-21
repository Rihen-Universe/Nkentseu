#pragma once
// -----------------------------------------------------------------------------
// @File    Engine/NKEditorKit/src/NKEditorKit/NkAiThreadLayout.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   LE PLAN DU PANNEAU IA : tous les rectangles que le peintre va
//          peindre, et RIEN D'AUTRE. Publie par la mesure, lu par la sonde.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// POURQUOI LE PEINTRE EST COUPE EN DEUX, ET C'EST LA DECISION DE CE FICHIER
//   Un peintre ordinaire calcule une position puis dessine, dans le meme geste.
//   Il est alors INVERIFIABLE sans fenetre. Ici la geometrie est un PRODUIT :
//   `NkAiFilMesurer`, `NkAiEnteteMesurer` et `NkAiComposeurMesurer` rendent un
//   plan -- la liste des rectangles, chacun nomme, chacun rattache au bloc qui
//   le porte. Le peintre (`NkAiThreadPaint.h`) ne fait que TRANSCRIRE ce plan.
//
//   ⚠️ *La sonde compare des rectangles PUBLIES, jamais recalcules.* Une sonde
//      qui refait le calcul mesure sa propre copie du calcul.
//
// ═══════════════════════════════════════════════════════════════════════════
//  21/09/2026 — LE PLAN SUIT LA CAPTURE, PLUS UNE APPROXIMATION D'ELLE
// ═══════════════════════════════════════════════════════════════════════════
//  Rodolf, 21/09 a 04h, captures a l'appui : « ce qui est observe n'est pas ce
//  qui est attendu ». Le plan d'avant etait JUSTE sur ses essais et FAUX sur
//  l'ecran, pour trois raisons que la capture montre et que les essais ne
//  regardaient pas :
//    1. les compartiments IN / OUT etaient deux aplats SEPARES ; la capture
//       les met dans UNE boite bordee, coupee d'un filet, la sortie sur un fond
//       plus clair RENTRE dans la boite ;
//    2. le code s'y REPLIAIT ; la capture ne replie JAMAIS une ligne de code :
//       elle la COUPE NET au bord, et montre trois lignes puis une estompe ;
//    3. la prose ne connaissait ni le gras ni le `code en ligne`, et le fil
//       n'avait pas de RAIL : la capture relie ses puces par un filet vertical.
//  Et le composeur ne publiait que son cadre : la capture en fait une surface
//  arrondie a plusieurs lignes, avec sa barre du bas (+, /, activite, duree,
//  lieu, modele, mode, envoi).
//
//  ⚠️ LE TEXTE EST PUBLIE EN FRAGMENTS, PAR DEBUT ET LONGUEUR.
//     Replier une phrase qui porte du gras et du code en ligne demande de
//     mesurer chaque mot dans SA police. Si le peintre le faisait, il
//     calculerait des positions -- ce qu'il n'a pas le droit de faire. Le plan
//     publie donc chaque morceau de ligne : ou il va, dans quelle police, et
//     QUELLE TRANCHE de la chaine du bloc il porte. Le peintre retrouve la
//     chaine par l'identifiant du bloc (pas de pointeur qui survivrait a son
//     proprietaire -- « le registre garde un pointeur », deja paye).
//
// LES MESURES VIENNENT DE LA CAPTURE, PAS D'UN GOUT
//   `Screenshot 2026-09-20 111040.png`, 695 x 1292, relevee au pixel :
//     - la bulle de la demande : x 24 .. 654 -- retraits 24 a gauche, 41 a droite ;
//     - les blocs du fil : x 55 ; le rail : x 36 ; les puces dessus ;
//     - la boite d'outil : x 55 .. 654 ; gouttiere `IN` a +8, code a +38 ;
//       lignes de code a 17 px ; trois lignes puis l'estompe ;
//     - la sortie : fond clair rentre de +36 a gauche et de 8 a droite ;
//     - l'en-tete : 43 px, titre gras a x 16, deux icones de 16 a droite ;
//     - le composeur : marges 21 / 21 / 22, coins de 10, texte a +13, barre du
//       bas de 34 px, bouton d'envoi orange de 26 px a 6 px du bord droit.
//   Elles sont exprimees en RETRAITS depuis les bords, jamais en positions
//   absolues : le panneau du modeleur fait 276 px, celui de la capture 695.
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkAiThread.h"
#include "NKEditorKit/NkTheme.h"

namespace nkentseu {
	namespace editorkit {

		// ── LES PIECES ──────────────────────────────────────────────────────────
		// Le NOM d'un rectangle. C'est par lui que la sonde interroge le plan.
		// APPEND-ONLY : un fil enregistre, une trace, une sonde lisent ces valeurs.
		enum class NkAiPiece : uint8 {
			/// La bulle bordee de la demande.
			Cadre = 0,
			/// La puce ronde du rail. Verte pour un outil, grise sinon.
			Puce,
			/// Le nom en gras (`Bash`).
			Titre,
			/// LA ZONE du texte d'un bloc (toutes ses lignes). ⚠️ Depuis le 21/09
			/// elle ne se PEINT plus : ce sont ses `Fragment` qui portent les
			/// glyphes. Elle reste publiee parce que c'est elle qu'on clique et
			/// qu'une sonde interroge (« ou est la ligne de ce bloc ? »).
			Texte,
			/// La gouttiere `IN`.
			GouttiereIn,
			/// Le fond du compartiment d'entree (code_bg).
			FondIn,
			/// UNE ligne de l'entree, coupee net au bord (tranche de `entree`).
			TexteIn,
			/// La gouttiere `OUT`.
			GouttiereOut,
			/// Le fond clair, RENTRE, du compartiment de sortie (code_out_bg).
			FondOut,
			/// UNE ligne de la sortie (tranche de `sortie`).
			TexteOut,
			/// L'effet mesure (`faces 6 -> 384`), a droite de la ligne.
			Effet,
			/// L'estompe d'un compartiment tronque : « il y a la suite ».
			Estompe,
			// ── le chrome (20/09) ──
			TitreConversation,
			IconeHistorique,
			IconeNouvelle,
			Filet,
			ComposeurCadre,
			/// UNE ligne du composeur (tranche de la saisie, ou de l'invite).
			ComposeurTexte,
			// ── 21/09 : ce que la capture montre et que le plan ignorait ──
			/// Le filet vertical qui relie deux puces consecutives.
			Rail,
			/// La boite bordee qui contient IN et OUT.
			BoiteOutil,
			/// Le filet horizontal entre IN et OUT.
			Separateur,
			/// Un morceau de ligne dans UNE police (tranche du texte du bloc).
			Fragment,
			/// Le fond d'un `code en ligne`, sous son fragment.
			FondCode,
			/// Un bouton porte par un bloc (« Annuler cette action »). `debut` =
			/// l'indice de l'action (0 ou 1).
			Action,
			/// Le curseur du composeur.
			Caret,
			/// Le « + » de la barre du composeur.
			BoutonPlus,
			/// Le « / » encadre : les commandes.
			BoutonCommandes,
			/// L'indicateur d'activite (arc qui tourne), SEULEMENT quand un tour
			/// est en cours -- un indicateur permanent ne dirait rien.
			Activite,
			/// L'horloge de la duree de la conversation.
			Horloge,
			/// La pastille « lieu » : le point vert LOCAL, orange DISTANT.
			PastilleLieu,
			/// La pastille du fournisseur et de son modele (menu a deux niveaux).
			PastilleModele,
			/// La pastille du mode (le geste, l'autorisation).
			PastilleMode,
			/// Le bouton d'envoi orange (ou d'arret pendant un tour).
			Envoi,
			/// Le fond d'un menu deroule.
			MenuFond,
			/// Une ligne de menu (fond de survol, coche).
			MenuLigne,
			/// Un texte du chrome, retrouve par sa SOURCE (voir `NkAiSource`).
			ChromeTexte,
			/// L'indication d'un fil vide (« ce que l'assistant sait faire »).
			Indication,
			// ── 21/09, Q5 : les proprietes d'un modele et ses fenetres ──
			/// Un interrupteur a droite d'une ligne de menu (Thinking). kAiActif = allume.
			Interrupteur,
			/// Un curseur a crans (Effort). `debut` = cran courant, `longueur` = crans.
			Curseur,
			/// Le champ « Filtrer les actions… » en tete du menu « / ».
			MenuFiltre,
			/// Une fenetre (Utilisation, Carte des agents) posee sur le fil.
			Fenetre,
			/// La croix qui la ferme.
			FenetreFermer,
			/// Une barre de proportion dans une fenetre. `longueur` = part en millimes.
			Barre,
			// ── 21/09, Q8 : selectionner et copier ──
			/// Le surlignage d'une tranche SELECTIONNEE (fil ou composeur).
			Surlignage,
			/// Le bouton « copier » d'un bloc, pose au SURVOL de ce bloc.
			BoutonCopier,
			/// La vignette d'un resultat de design : son cadre, puis un rectangle par
			/// element pose (`debut` = genre : 0 cadre, 1 texte, 2 composant, 3 bouton).
			VignetteFond,
			VignetteRect,
			/// (Q8) Une image jointe, en vignette : dans le composeur (bloc 0) ou
			/// dans la demande encadree. `debut` = son rang.
			ImageJointe,
			/// La croix qui retire une image jointe du composeur.
			RetirerImage,

			Count
		};

		inline const char *NkAiPieceNom(NkAiPiece p) {
			switch (p) {
				case NkAiPiece::Cadre:			   return "cadre";
				case NkAiPiece::Puce:			   return "puce";
				case NkAiPiece::Titre:			   return "titre";
				case NkAiPiece::Texte:			   return "texte";
				case NkAiPiece::GouttiereIn:	   return "gouttiere_in";
				case NkAiPiece::FondIn:			   return "fond_in";
				case NkAiPiece::TexteIn:		   return "texte_in";
				case NkAiPiece::GouttiereOut:	   return "gouttiere_out";
				case NkAiPiece::FondOut:		   return "fond_out";
				case NkAiPiece::TexteOut:		   return "texte_out";
				case NkAiPiece::Effet:			   return "effet";
				case NkAiPiece::Estompe:		   return "estompe";
				case NkAiPiece::TitreConversation: return "titre_conversation";
				case NkAiPiece::IconeHistorique:   return "icone_historique";
				case NkAiPiece::IconeNouvelle:	   return "icone_nouvelle";
				case NkAiPiece::Filet:			   return "filet";
				case NkAiPiece::ComposeurCadre:	   return "composeur_cadre";
				case NkAiPiece::ComposeurTexte:	   return "composeur_texte";
				case NkAiPiece::Rail:			   return "rail";
				case NkAiPiece::BoiteOutil:		   return "boite_outil";
				case NkAiPiece::Separateur:		   return "separateur";
				case NkAiPiece::Fragment:		   return "fragment";
				case NkAiPiece::FondCode:		   return "fond_code";
				case NkAiPiece::Action:			   return "action";
				case NkAiPiece::Caret:			   return "caret";
				case NkAiPiece::BoutonPlus:		   return "bouton_plus";
				case NkAiPiece::BoutonCommandes:   return "bouton_commandes";
				case NkAiPiece::Activite:		   return "activite";
				case NkAiPiece::Horloge:		   return "horloge";
				case NkAiPiece::PastilleLieu:	   return "pastille_lieu";
				case NkAiPiece::PastilleModele:	   return "pastille_modele";
				case NkAiPiece::PastilleMode:	   return "pastille_mode";
				case NkAiPiece::Envoi:			   return "envoi";
				case NkAiPiece::MenuFond:		   return "menu_fond";
				case NkAiPiece::MenuLigne:		   return "menu_ligne";
				case NkAiPiece::ChromeTexte:	   return "chrome_texte";
				case NkAiPiece::Indication:		   return "indication";
				case NkAiPiece::Interrupteur:	   return "interrupteur";
				case NkAiPiece::Curseur:		   return "curseur";
				case NkAiPiece::MenuFiltre:		   return "menu_filtre";
				case NkAiPiece::Fenetre:		   return "fenetre";
				case NkAiPiece::FenetreFermer:	   return "fenetre_fermer";
				case NkAiPiece::Barre:			   return "barre";
				case NkAiPiece::Surlignage:		   return "surlignage";
				case NkAiPiece::BoutonCopier:	   return "bouton_copier";
				case NkAiPiece::VignetteFond:	   return "vignette_fond";
				case NkAiPiece::VignetteRect:	   return "vignette_rect";
				case NkAiPiece::ImageJointe:	   return "image_jointe";
				case NkAiPiece::RetirerImage:	   return "retirer_image";
				default:						   return "";
			}
		}

		/// La police d'une piece de texte. Les valeurs sont celles que
		/// `NkComponentPaint::TextePolice` recoit (0, 1, 2).
		enum class NkAiPolice : uint8 { Normale = 0, Grasse, ChasseFixe };

		/// D'ou vient le texte d'une piece de CHROME (elle n'appartient a aucun
		/// bloc, donc l'identifiant ne la retrouve pas). APPEND-ONLY.
		enum class NkAiSource : uint8 {
			Aucune = 0,
			Titre,		  ///< le sujet de la conversation
			Saisie,		  ///< ce qui est tape
			Invite,		  ///< l'invite d'un composeur vide
			Duree,		  ///< « 57m »
			Lieu,		  ///< « local » / « distant »
			Modele,		  ///< le fournisseur et le modele
			ModeleDetail, ///< le detail du modele, attenue (« High »)
			Mode,		  ///< le mode courant
			MenuTexte,	  ///< la ligne `debut` du menu ouvert
			MenuDetail,	  ///< le detail attenue de la ligne `debut`
			Indication,	  ///< l'indication d'un fil vide
			Action,		  ///< le libelle de l'action `debut` (le bloc est dans blocId)
			EffetBloc,	  ///< une tranche de l'EFFET du bloc (l'effet deplie en entier)
			MenuDroite,	  ///< le texte attenue aligne a droite de la ligne `debut` du menu
			Filtre,		  ///< ce qui est tape dans le filtre du menu « / » (ou son invite)
			FenetreTexte  ///< ligne `debut` de la fenetre ; `longueur` 0 = libelle, 1 = valeur
		};

		/// Drapeaux d'une piece. Ils disent un ETAT que la geometrie ne dit pas.
		enum : uint8 {
			kAiEllipse = 1u,  ///< le texte a ete coupe : le peintre pose « ... » apres
			kAiSurvol = 2u,	  ///< la piece est sous la souris (publie par le panneau)
			kAiEteint = 4u,	  ///< le geste est indisponible : il se dessine ATTENUE
			kAiActif = 8u,	  ///< la ligne de menu est la valeur courante (coche)
			kAiDistant = 16u, ///< la pastille dit un service DISTANT (point orange)
			kAiArret = 32u,	  ///< le bouton d'envoi est un bouton d'ARRET (tour en cours)
			kAiSection = 64u  ///< la ligne de menu est un TITRE de section : ni survol, ni clic
		};

		/// UN RECTANGLE PUBLIE. Ce que le peintre peint, et rien d'autre.
		struct NkAiRectPublie {
				/// Le bloc qui porte cette piece. 0 = le chrome.
				uint32 blocId = 0;
				NkAiPiece piece = NkAiPiece::Cadre;
				/// Le ROLE du theme, jamais une couleur : une valeur en dur est
				/// inecrivable ici, et c'est ce qui fait suivre le theme clair.
				NkRole role = NkRole::Text;
				NkAiPolice police = NkAiPolice::Normale;
				float32 x = 0.f, y = 0.f, w = 0.f, h = 0.f;
				/// Pour une piece de TEXTE : la tranche [debut, debut+longueur) de
				/// la chaine source. Pour une `Action`, une `MenuLigne` ou une source
				/// de menu : l'INDICE de l'element. ⚠️ Une tranche, jamais un
				/// pointeur : la chaine appartient au bloc.
				uint32 debut = 0, longueur = 0;
				uint8 drapeaux = 0;
				NkAiSource source = NkAiSource::Aucune;
		};

		// ── LES METRIQUES : la capture, en retraits ─────────────────────────────
		struct NkAiMetriques {
				// -- le fil --
				float32 retraitDemande = 24.f; ///< la bulle de la demande commence la
				float32 margeDroite = 41.f;	   ///< tout le fil s'arrete la
				float32 retraitFil = 55.f;	   ///< les blocs commencent la
				float32 railX = 36.f;		   ///< le rail et ses puces
				float32 puceR = 3.5f;
				float32 gouttiere = 42.f;  ///< du bord de la boite au debut du code (38 a la capture ; +4 : notre « OUT » est plus large)
				float32 ligne = 20.f;	   ///< une ligne de texte courant
				float32 ligneCode = 17.f;  ///< une ligne de code (IN / OUT)
				float32 entreBlocs = 18.f; ///< l'air entre deux blocs
				float32 padding = 8.f;	   ///< l'interieur d'une bulle, d'un compartiment
				/// Le PLAFOND de lignes d'un compartiment IN / OUT. ⚠️ Un COMPTE, pas
				/// une longueur : `Echelle` ne le touche pas (essai 22x).
				uint32 lignesMax = 3;
				float32 estompeH = 10.f;
				float32 codeRetrait = 4.f; ///< l'air autour d'un `code en ligne`
				// -- le chrome --
				float32 entete = 43.f;
				float32 icone = 16.f;
				float32 margeTitre = 16.f;
				/// ⚠️ GARDEE POUR LA TRACE, PLUS POUR LA MISE EN PAGE : le composeur
				///    n'a plus de hauteur fixe depuis le 21/09 -- il grandit avec ce
				///    qu'on tape, comme celui de la capture.
				float32 composeur = 118.f;
				float32 barreEtat = 34.f;  ///< la barre du bas du composeur
				float32 margeBas = 22.f;
				float32 margeComposeur = 21.f;
				float32 hautComposeur = 14.f;  ///< du bord du cadre a la premiere ligne
				float32 ecartSousTexte = 26.f; ///< de la derniere ligne a la barre
				float32 texteComposeur = 13.f; ///< retrait du texte dans le cadre
				uint32 lignesComposeurMin = 2;
				uint32 lignesComposeurMax = 8;
				float32 envoi = 26.f;
				float32 pastilleH = 20.f;
				float32 menuLigne = 26.f;

				/// (Q8, Rodolf 21/09 : « le texte est trop centre, il doit prendre bien
				/// de l'espace et laisser juste une petite marge »). Dans un panneau
				/// ETROIT (celui des applications : 276 a 520 px), les retraits de la
				/// capture (24 / 55 / 41 px, pris sur 695) mangeaient un tiers de la
				/// largeur. Le fil, la demande et le composeur y gardent 8 a 12 px.
				void Compacter(float32 k) {
					retraitDemande = 10.f * k;
					margeDroite = 10.f * k;
					railX = 16.f * k;
					retraitFil = 30.f * k;
					margeComposeur = 8.f * k;
					margeBas = 10.f * k;
					margeTitre = 12.f * k;
				}
				void Echelle(float32 k) {
					retraitDemande *= k; margeDroite *= k; retraitFil *= k;
					railX *= k; puceR *= k; gouttiere *= k; ligne *= k; ligneCode *= k;
					entreBlocs *= k; padding *= k; estompeH *= k; codeRetrait *= k;
					entete *= k; icone *= k; margeTitre *= k; composeur *= k; barreEtat *= k;
					margeBas *= k; margeComposeur *= k; hautComposeur *= k; ecartSousTexte *= k;
					texteComposeur *= k; envoi *= k; pastilleH *= k; menuLigne *= k;
				}
		};

		/// La mesure de texte que le plan demande a l'appelant. Dans l'application
		/// c'est la vraie fonte ; au banc, une regle deterministe.
		typedef float32 (*NkAiMesureTexte)(void *ctx, NkAiPolice police, const char *texte);

		// ── LE PLAN ─────────────────────────────────────────────────────────────
		class NkAiPlan {
			public:
				void Vider() {
					mPieces.Clear();
					mHauteur = 0.f;
				}
				void Ajouter(const NkAiRectPublie &r) {
					mPieces.PushBack(r);
				}
				uint32 Pieces() const {
					return (uint32)mPieces.Size();
				}
				const NkAiRectPublie &Piece(uint32 i) const {
					return mPieces[i];
				}
				NkAiRectPublie &PieceModifiable(uint32 i) {
					return mPieces[i];
				}
				float32 Hauteur() const {
					return mHauteur;
				}
				void PoserHauteur(float32 h) {
					mHauteur = h;
				}
				/// La PREMIERE piece de ce nom pour ce bloc.
				bool Trouver(uint32 blocId, NkAiPiece p, NkAiRectPublie &out) const {
					for (usize i = 0; i < mPieces.Size(); ++i)
						if (mPieces[i].blocId == blocId && mPieces[i].piece == p) {
							out = mPieces[i];
							return true;
						}
					return false;
				}
				/// La piece de ce nom pour ce bloc ET cet indice (`debut`) -- une
				/// action, une ligne de menu.
				bool TrouverIndice(uint32 blocId, NkAiPiece p, uint32 indice, NkAiRectPublie &out) const {
					for (usize i = 0; i < mPieces.Size(); ++i)
						if (mPieces[i].blocId == blocId && mPieces[i].piece == p &&
							mPieces[i].debut == indice) {
							out = mPieces[i];
							return true;
						}
					return false;
				}
				bool Possede(uint32 blocId, NkAiPiece p) const {
					NkAiRectPublie tmp;
					return Trouver(blocId, p, tmp);
				}
				uint32 Compter(NkAiPiece p) const {
					uint32 n = 0;
					for (usize i = 0; i < mPieces.Size(); ++i)
						if (mPieces[i].piece == p)
							++n;
					return n;
				}

			private:
				NkVector<NkAiRectPublie> mPieces;
				float32 mHauteur = 0.f;
		};

		/// LES BOUTONS D'UN BLOC. Un seul bloc a la fois peut en porter, et c'est
		/// l'HOTE qui dit lequel : il est seul a savoir ce que sa pile d'annulation
		/// sait defaire. NK3DModeler : « Annuler cette action » sur la DERNIERE
		/// operation (sa pile a un cran). NKCode : « Autoriser » / « Refuser » sur
		/// la demande de permission du CLI.
		struct NkAiActionsFil {
				uint32 blocId = 0;
				const char *libelle[2] = {nullptr, nullptr};
				bool actif[2] = {true, true};
		};

		namespace aidetail {

			/// Longueur d'un octet de tete UTF-8.
			inline uint32 LongueurCp(unsigned char c) {
				if (c < 0x80u)
					return 1u;
				if ((c & 0xE0u) == 0xC0u)
					return 2u;
				if ((c & 0xF0u) == 0xE0u)
					return 3u;
				if ((c & 0xF8u) == 0xF0u)
					return 4u;
				return 1u; // octet de continuation isole : on avance d'un, sans boucler
			}

			/// Largeur de la tranche [a, b) du texte `t`, mesuree par l'appelant.
			/// ⚠️ PAR MORCEAUX DE 480 OCTETS au-dela : la mesure recoit une chaine
			///    terminee, et une ligne de code peut depasser le tampon. Couper a
			///    une frontiere de caractere, jamais au milieu d'un octet UTF-8.
			inline float32 Largeur(const char *t, uint32 a, uint32 b, NkAiPolice pol, NkAiMesureTexte mes,
								   void *ctx) {
				if (!t || b <= a || !mes)
					return 0.f;
				float32 w = 0.f;
				char tmp[512];
				uint32 i = a;
				while (i < b) {
					uint32 n = 0;
					while (i < b && n < 480u) {
						const uint32 lc = LongueurCp((unsigned char)t[i]);
						for (uint32 k = 0; k < lc && i < b; ++k)
							tmp[n++] = t[i++];
					}
					tmp[n] = 0;
					w += mes(ctx, pol, tmp);
				}
				return w;
			}

			struct Tranche {
					uint32 debut = 0, fin = 0;
			};

			/// Replie la tranche [a, b) (sans saut de ligne) en lignes de largeur
			/// <= L, aux espaces ; un mot plus long que la ligne est coupe au
			/// caractere. Rend `true` si le plafond `max` a coupe la suite.
			inline bool ReplierSegment(const char *t, uint32 a, uint32 b, float32 L, NkAiPolice pol,
									   NkAiMesureTexte mes, void *ctx, uint32 max,
									   NkVector<Tranche> &out) {
				if (b <= a) {
					if (out.Size() >= max)
						return true;
					Tranche v;
					v.debut = a;
					v.fin = a;
					out.PushBack(v); // une ligne VIDE compte : elle prend sa place
					return false;
				}
				uint32 pos = a;
				while (pos < b) {
					if (out.Size() >= max)
						return true;
					uint32 dernier = pos; // la derniere coupure a un espace qui tient
					uint32 cur = pos;
					while (cur < b) {
						uint32 fm = cur;
						while (fm < b && t[fm] != ' ')
							++fm;
						if (Largeur(t, pos, fm, pol, mes, ctx) <= L) {
							dernier = fm;
							cur = fm + 1u; // l'espace
						} else
							break;
					}
					uint32 e = dernier;
					if (e == pos) {
						// UN MOT TROP LONG : coupe au caractere, au moins un caractere
						// par ligne -- sinon la boucle ne progresserait jamais.
						uint32 c = pos;
						while (c < b) {
							const uint32 nc = c + LongueurCp((unsigned char)t[c]);
							if (Largeur(t, pos, nc > b ? b : nc, pol, mes, ctx) > L && c > pos)
								break;
							c = nc > b ? b : nc;
						}
						e = c;
					}
					Tranche v;
					v.debut = pos;
					v.fin = e;
					out.PushBack(v);
					pos = e;
					while (pos < b && t[pos] == ' ')
						++pos;
				}
				return false;
			}

			/// Replie tout un texte (les '\n' coupent).
			inline bool Replier(const char *t, float32 L, NkAiPolice pol, NkAiMesureTexte mes, void *ctx,
								uint32 max, NkVector<Tranche> &out) {
				out.Clear();
				if (!t || !t[0] || L <= 1.f)
					return false;
				uint32 a = 0, i = 0;
				for (;; ++i) {
					if (t[i] == '\n' || t[i] == 0) {
						if (ReplierSegment(t, a, i, L, pol, mes, ctx, max, out))
							return true;
						if (t[i] == 0)
							break;
						a = i + 1u;
					}
				}
				return false;
			}

			/// Les lignes d'un compartiment de CODE : coupees aux '\n', JAMAIS
			/// repliees (la capture coupe net au bord). Rend `true` si tronque.
			inline bool LignesCode(const char *t, uint32 max, NkVector<Tranche> &out) {
				out.Clear();
				if (!t || !t[0])
					return false;
				uint32 a = 0;
				for (uint32 i = 0;; ++i) {
					if (t[i] == '\n' || t[i] == 0) {
						if (out.Size() >= max)
							return true;
						Tranche v;
						v.debut = a;
						v.fin = i;
						out.PushBack(v);
						if (t[i] == 0)
							break;
						a = i + 1u;
						if (t[a] == 0)
							break; // un saut final ne fabrique pas une ligne vide
					}
				}
				return false;
			}

			/// UN MORCEAU DE TEXTE RICHE : une tranche dans une police, avec ou
			/// sans fond de code. Les marques (`**`, `` ` ``) n'y sont PAS : elles
			/// structurent, elles ne s'affichent pas.
			struct Jeton {
					uint32 debut = 0, fin = 0;
					NkAiPolice police = NkAiPolice::Normale;
					bool code = false;
					bool espace = false;
					bool saut = false; ///< un '\n' force la ligne suivante
			};

			/// Decoupe un texte portant du **gras** et du `code` en jetons.
			/// ⚠️ UNE MARQUE ORPHELINE (un seul `) RESTE DU TEXTE : on ne cache
			///    pas la moitie d'une reponse parce que le modele a oublie de
			///    fermer un accent grave. Rien ne deborde d'un paragraphe.
			inline void Jetons(const char *t, bool riche, NkVector<Jeton> &out) {
				out.Clear();
				if (!t)
					return;
				bool gras = false, code = false;
				uint32 i = 0;
				auto pousser = [&](uint32 a, uint32 b, bool esp) {
					if (b <= a)
						return;
					Jeton j;
					j.debut = a;
					j.fin = b;
					j.police = code ? NkAiPolice::ChasseFixe : (gras ? NkAiPolice::Grasse : NkAiPolice::Normale);
					j.code = code;
					j.espace = esp;
					out.PushBack(j);
				};
				while (t[i]) {
					const char c = t[i];
					if (c == '\n') {
						Jeton j;
						j.debut = i;
						j.fin = i;
						j.saut = true;
						out.PushBack(j);
						gras = false;
						code = false;
						++i;
						continue;
					}
					if (riche && c == '`') {
						// on ne bascule que si une fermeture existe dans le paragraphe
						bool ferme = code;
						if (!ferme)
							for (uint32 k = i + 1u; t[k] && t[k] != '\n'; ++k)
								if (t[k] == '`') {
									ferme = true;
									break;
								}
						if (ferme) {
							code = !code;
							++i;
							continue;
						}
					}
					if (riche && !code && c == '*' && t[i + 1] == '*') {
						bool ferme = gras;
						if (!ferme)
							for (uint32 k = i + 2u; t[k] && t[k] != '\n'; ++k)
								if (t[k] == '*' && t[k + 1] == '*') {
									ferme = true;
									break;
								}
						if (ferme) {
							gras = !gras;
							i += 2u;
							continue;
						}
					}
					if (c == ' ') {
						uint32 e = i;
						while (t[e] == ' ')
							++e;
						pousser(i, e, true);
						i = e;
						continue;
					}
					uint32 e = i;
					while (t[e] && t[e] != ' ' && t[e] != '\n' &&
						   (!riche || (t[e] != '`' && !(t[e] == '*' && t[e + 1] == '*'))))
						e += LongueurCp((unsigned char)t[e]);
					if (e == i)
						e = i + 1u;
					pousser(i, e, false);
					i = e;
				}
			}

		} // namespace aidetail

		/// Publie les lignes d'un texte a partir de (x0, y0), largeur L, dans le
		/// plan, en fragments. Rend le nombre de lignes. `riche` : gras et code en
		/// ligne reconnus. `maxLignes` atteint : le dernier fragment porte « ... ».
		inline uint32 NkAiPublierTexte(NkAiPlan &plan, uint32 blocId, const char *t, bool riche, float32 x0,
									   float32 y0, float32 L, float32 ligne, NkRole role, uint32 maxLignes,
									   const NkAiMetriques &m, NkAiMesureTexte mes, void *ctx,
									   NkAiSource source = NkAiSource::Aucune) {
			if (!t || !t[0] || L <= 4.f || maxLignes == 0)
				return 0;
			NkVector<aidetail::Jeton> jt;
			aidetail::Jetons(t, riche, jt);

			struct Place {
					uint32 ligne, debut, fin;
					float32 x, w;
					NkAiPolice police;
					bool code;
			};
			NkVector<Place> places;
			uint32 nl = 0;
			float32 x = 0.f;
			bool tronque = false;
			const float32 esp = m.codeRetrait;
			for (usize i = 0; i < jt.Size() && !tronque; ++i) {
				const aidetail::Jeton &j = jt[i];
				if (j.saut) {
					if (nl + 1u >= maxLignes) {
						tronque = (i + 1 < jt.Size());
						break;
					}
					++nl;
					x = 0.f;
					continue;
				}
				if (j.espace) {
					if (x > 0.f) {
						const float32 w = aidetail::Largeur(t, j.debut, j.fin, j.police, mes, ctx);
						Place p{nl, j.debut, j.fin, x, w, j.police, j.code};
						places.PushBack(p);
						x += w;
					}
					continue;
				}
				const float32 bord = j.code ? esp : 0.f;
				float32 w = aidetail::Largeur(t, j.debut, j.fin, j.police, mes, ctx);
				if (x > 0.f && x + bord * 2.f + w > L) {
					// les espaces de fin de ligne ne se dessinent pas
					while (places.Size() > 0 && places[places.Size() - 1].ligne == nl &&
						   t[places[places.Size() - 1].debut] == ' ')
						places.PopBack();
					if (nl + 1u >= maxLignes) {
						tronque = true;
						break;
					}
					++nl;
					x = 0.f;
				}
				// un mot plus long que la ligne entiere : coupe au caractere
				uint32 a = j.debut;
				while (a < j.fin) {
					uint32 b = j.fin;
					w = aidetail::Largeur(t, a, b, j.police, mes, ctx);
					if (x + bord * 2.f + w > L) {
						b = a;
						while (b < j.fin) {
							const uint32 nb = b + aidetail::LongueurCp((unsigned char)t[b]);
							if (x + bord * 2.f + aidetail::Largeur(t, a, nb, j.police, mes, ctx) > L && b > a)
								break;
							b = nb > j.fin ? j.fin : nb;
						}
						w = aidetail::Largeur(t, a, b, j.police, mes, ctx);
					}
					Place p{nl, a, b, x + bord, w, j.police, j.code};
					places.PushBack(p);
					x += w + bord * 2.f;
					a = b;
					if (a < j.fin) {
						if (nl + 1u >= maxLignes) {
							tronque = true;
							break;
						}
						++nl;
						x = 0.f;
					}
				}
			}
			const uint32 lignes = nl + 1u;

			// ── LA FUSION : un fragment par suite de meme police sur une ligne ──
			// ⚠️ SEULEMENT SI LES OCTETS SE SUIVENT dans la source : entre deux
			//    morceaux separes par une marque (`**`), la fusion ferait peindre
			//    la marque elle-meme.
			usize i = 0;
			uint32 dernierFragment = 0xFFFFFFFFu;
			while (i < places.Size()) {
				Place a = places[i];
				usize k = i + 1;
				while (k < places.Size() && places[k].ligne == a.ligne && places[k].police == a.police &&
					   places[k].code == a.code && places[k].debut == a.fin) {
					a.fin = places[k].fin;
					a.w = places[k].x + places[k].w - a.x;
					++k;
				}
				// un fragment fait seulement d'espaces ne se publie pas
				bool vide = true;
				for (uint32 q = a.debut; q < a.fin; ++q)
					if (t[q] != ' ') {
						vide = false;
						break;
					}
				if (!vide) {
					const float32 yy = y0 + (float32)a.ligne * ligne;
					if (a.code) {
						NkAiRectPublie f;
						f.blocId = blocId;
						f.piece = NkAiPiece::FondCode;
						f.role = NkRole::InlineCodeBg;
						f.x = x0 + a.x - esp;
						f.y = yy + 2.f;
						f.w = a.w + esp * 2.f;
						f.h = ligne - 4.f;
						plan.Ajouter(f);
					}
					NkAiRectPublie r;
					r.blocId = blocId;
					r.piece = NkAiPiece::Fragment;
					r.source = source;
					r.role = role;
					r.police = a.police;
					r.x = x0 + a.x;
					r.y = yy;
					r.w = a.w;
					r.h = ligne;
					r.debut = a.debut;
					r.longueur = a.fin - a.debut;
					dernierFragment = plan.Pieces();
					plan.Ajouter(r);
				}
				i = k;
			}
			if (tronque && dernierFragment != 0xFFFFFFFFu)
				plan.PieceModifiable(dernierFragment).drapeaux |= kAiEllipse;
			return lignes;
		}

		/// Une ligne unique, coupee avec « ... » si elle deborde. Rend la largeur
		/// publiee. C'est le cas d'un bloc REPLIE et du titre de la conversation.
		inline float32 NkAiPublierLigne(NkAiPlan &plan, uint32 blocId, NkAiPiece piece, NkAiSource source,
										const char *t, float32 x, float32 y, float32 L, float32 h, NkRole role,
										NkAiPolice pol, NkAiMesureTexte mes, void *ctx, uint8 drapeaux = 0) {
			if (!t || !t[0] || L <= 4.f)
				return 0.f;
			uint32 fin = 0;
			while (t[fin] && t[fin] != '\n')
				++fin;
			const bool coupeSaut = t[fin] == '\n';
			float32 w = aidetail::Largeur(t, 0, fin, pol, mes, ctx);
			bool ellipse = coupeSaut;
			const float32 suite = mes ? mes(ctx, pol, "...") : 0.f;
			if (w > L || (ellipse && w + suite > L)) {
				ellipse = true;
				uint32 b = 0;
				while (b < fin) {
					const uint32 nb = b + aidetail::LongueurCp((unsigned char)t[b]);
					if (aidetail::Largeur(t, 0, nb, pol, mes, ctx) + suite > L)
						break;
					b = nb;
				}
				fin = b;
				w = aidetail::Largeur(t, 0, fin, pol, mes, ctx);
			}
			NkAiRectPublie r;
			r.blocId = blocId;
			r.piece = piece;
			r.source = source;
			r.role = role;
			r.police = pol;
			r.x = x;
			r.y = y;
			r.w = w + (ellipse ? suite : 0.f);
			r.h = h;
			r.debut = 0;
			r.longueur = fin;
			r.drapeaux = (uint8)(drapeaux | (ellipse ? kAiEllipse : 0u));
			plan.Ajouter(r);
			return r.w;
		}

		/// LE FIL. `actions` : les boutons d'UN bloc (ou nullptr).
		inline void NkAiFilMesurer(const NkAiFil &fil, float32 largeurPanneau, const NkAiMetriques &m,
								   NkAiMesureTexte mes, void *ctx, NkAiPlan &plan,
								   const NkAiActionsFil *actions = nullptr) {
			plan.Vider();
			if (largeurPanneau <= m.retraitFil + m.margeDroite + 4.f)
				return; // panneau trop etroit : un plan VIDE, pas un plan faux

			float32 y = m.entreBlocs;
			const float32 droite = largeurPanneau - m.margeDroite;
			auto Pousser = [&](uint32 id, NkAiPiece p, NkRole role, NkAiPolice pol, float32 x, float32 yy,
							   float32 w, float32 h) {
				NkAiRectPublie r;
				r.blocId = id;
				r.piece = p;
				r.role = role;
				r.police = pol;
				r.x = x;
				r.y = yy;
				r.w = w;
				r.h = h;
				plan.Ajouter(r);
			};
			// Les centres des puces, pour le rail. `-1` = une demande s'intercale :
			// le rail ne traverse pas la bulle de l'utilisateur (la capture non plus).
			NkVector<float32> centres;

			for (uint32 i = 0; i < fil.Taille(); ++i) {
				const NkAiBlocDonnees &b = fil.At(i);

				if (b.type == NkAiBloc::Demande) {
					centres.PushBack(-1.f);
					const float32 x = m.retraitDemande;
					const float32 w = droite - x;
					const uint32 iCadre = plan.Pieces();
					Pousser(b.id, NkAiPiece::Cadre, NkRole::PanelHeader, NkAiPolice::Normale, x, y, w, 0.f);
					const uint32 n = NkAiPublierTexte(plan, b.id, b.texte.CStr(), false, x + m.padding, y + m.padding,
													  w - 2.f * m.padding, m.ligne, NkRole::Text,
													  64u /* la question ne se tronque pas */, m, mes, ctx);
					const float32 hT = (float32)(n == 0 ? 1 : n) * m.ligne;
					float32 h = hT + 2.f * m.padding;
					// (Q8) LES IMAGES JOINTES, en tuiles sous la question
					if (b.images.Size() > 0) {
						const float32 t = 56.f;
						float32 tx = x + m.padding;
						for (usize k = 0; k < b.images.Size(); ++k) {
							if (tx + t > x + w - m.padding)
								break;
							NkAiRectPublie im;
							im.blocId = b.id;
							im.piece = NkAiPiece::ImageJointe;
							im.x = tx;
							im.y = y + h - m.padding + 4.f;
							im.w = t;
							im.h = t;
							im.debut = (uint32)k;
							plan.Ajouter(im);
							tx += t + 6.f;
						}
						h += t + 4.f;
					}
					plan.PieceModifiable(iCadre).h = h;
					Pousser(b.id, NkAiPiece::Texte, NkRole::Text, NkAiPolice::Normale, x + m.padding, y + m.padding,
							w - 2.f * m.padding, hT);
					y += h + m.entreBlocs;
					continue;
				}

				const float32 x = m.retraitFil;
				const float32 cy = y + m.ligne * 0.5f;
				centres.PushBack(cy);
				const NkRole rolePuce = (b.type == NkAiBloc::Outil)	  ? NkRole::StatusOk
										: (b.type == NkAiBloc::Refus) ? NkRole::StatusWarn
										: (b.type == NkAiBloc::Echec) ? NkRole::StatusErr
																	  : NkRole::TextMuted;
				Pousser(b.id, NkAiPiece::Puce, rolePuce, NkAiPolice::Normale, m.railX - m.puceR, cy - m.puceR,
						m.puceR * 2.f, m.puceR * 2.f);

				// ── LA LIGNE DU BLOC ──
				float32 xTexte = x;
				const bool titreGras = b.type != NkAiBloc::Reflexion;
				if (b.titre.Length() > 0) {
					const NkAiPolice pt = titreGras ? NkAiPolice::Grasse : NkAiPolice::Normale;
					const NkRole rt = titreGras ? NkRole::Text : NkRole::TextMuted;
					float32 wT = mes ? mes(ctx, pt, b.titre.CStr()) : 0.f;
					if (wT > droite - x)
						wT = droite - x;
					NkAiRectPublie r;
					r.blocId = b.id;
					r.piece = NkAiPiece::Titre;
					r.role = rt;
					r.police = pt;
					r.x = x;
					r.y = y;
					r.w = wT;
					r.h = m.ligne;
					r.debut = 0;
					r.longueur = (uint32)b.titre.Length();
					plan.Ajouter(r);
					xTexte = x + wT + 8.f;
				}
				const char *surLaLigne = b.texte.CStr();
				NkRole roleTexte = NkRole::Text;
				bool riche = true;
				// UN SEUL VISAGE REPLIE : l'outil et la reflexion se resument a UNE
				// ligne ; la prose, le refus, l'echec et l'effet se lisent en entier.
				bool uneLigne = false;
				if (b.type == NkAiBloc::Refus) {
					surLaLigne = b.motif.CStr();
					roleTexte = NkRole::StatusWarn; // un refus n'est PAS une erreur
					riche = false;
				} else if (b.type == NkAiBloc::Echec) {
					surLaLigne = b.motif.CStr();
					roleTexte = NkRole::StatusErr;
					riche = false;
				} else if (b.type == NkAiBloc::Effet) {
					surLaLigne = b.effet.CStr();
					roleTexte = NkRole::TextMuted;
					riche = false;
				} else if (b.type == NkAiBloc::Outil) {
					roleTexte = NkRole::TextMuted; // la phrase courte d'un outil
					riche = false;
					uneLigne = true;
				} else if (b.type == NkAiBloc::Reflexion) {
					// replie : RIEN de plus que le titre (« Thinking ») ; deplie, le
					// contenu vient SOUS la ligne, attenue.
					surLaLigne = "";
					uneLigne = true;
				}
				float32 wEffet = 0.f;
				const bool aEffet = (b.type != NkAiBloc::Effet) && b.effet.Length() > 0;
				bool effetCoupe = false;
				if (aEffet) {
					wEffet = (mes ? mes(ctx, NkAiPolice::Normale, b.effet.CStr()) : 0.f) + 12.f;
					// ⚠️ UN EFFET LONG NE MANGE PAS LA LIGNE. La voie de creation du
					//    modeleur ecrit une phrase entiere (« 12 parties posees au sol,
					//    boite 0.45 x 0.90 x 0.45 m… ») : sur la ligne, il garde au plus
					//    60 % et se coupe avec « ... » ; deplie, il se lit en entier.
					const float32 plafond = (droite - xTexte) * 0.6f;
					if (b.texte.Length() > 0 && wEffet > plafond) {
						wEffet = plafond;
						effetCoupe = true;
					}
				}
				float32 wTexte = droite - xTexte - wEffet;
				// ⚠️ QUAND LA PLACE MANQUE, C'EST LA PROSE QUI CEDE (22p) : l'effet
				//    est le FAIT, la phrase est le commentaire.
				if (wTexte < 24.f) {
					wTexte = 0.f;
					wEffet = droite - xTexte;
					// coupe SEULEMENT si le fait ne tient pas dans toute la ligne restante
					effetCoupe = aEffet && (mes ? mes(ctx, NkAiPolice::Normale, b.effet.CStr()) : 0.f) > wEffet;
				}
				uint32 nTexte = 0;
				if (wTexte > 0.f && surLaLigne && surLaLigne[0]) {
					if (uneLigne) {
						nTexte = NkAiPublierLigne(plan, b.id, NkAiPiece::Fragment, NkAiSource::Aucune, surLaLigne,
												  xTexte, y, wTexte, m.ligne, roleTexte, NkAiPolice::Normale, mes,
												  ctx) > 0.f
									 ? 1u
									 : 0u;
					} else {
						nTexte = NkAiPublierTexte(plan, b.id, surLaLigne, riche, xTexte, y, wTexte, m.ligne, roleTexte,
												  400u, m, mes, ctx);
					}
					if (nTexte > 0)
						Pousser(b.id, NkAiPiece::Texte, roleTexte, NkAiPolice::Normale, xTexte, y, wTexte,
								(float32)nTexte * m.ligne);
				}
				if (aEffet) {
					const float32 wNat = mes ? mes(ctx, NkAiPolice::Normale, b.effet.CStr()) : 0.f;
					if (!effetCoupe || wNat <= wEffet - 12.f) {
						NkAiRectPublie r;
						r.blocId = b.id;
						r.piece = NkAiPiece::Effet;
						r.role = NkRole::TextMuted;
						r.x = droite - wEffet;
						r.y = y;
						r.w = wEffet;
						r.h = m.ligne;
						r.debut = 0;
						r.longueur = (uint32)b.effet.Length();
						plan.Ajouter(r);
					} else
						(void)NkAiPublierLigne(plan, b.id, NkAiPiece::Effet, NkAiSource::Aucune, b.effet.CStr(),
											   droite - wEffet + 12.f, y, wEffet - 12.f, m.ligne, NkRole::TextMuted,
											   NkAiPolice::Normale, mes, ctx);
				}
				float32 yb = y + (float32)(nTexte == 0 ? 1 : nTexte) * m.ligne;
				// L'EFFET COUPE SE LIT EN ENTIER QUAND LE BLOC EST DEPLIE.
				if (aEffet && effetCoupe && !b.replie) {
					const uint32 n = NkAiPublierTexte(plan, b.id, b.effet.CStr(), false, x, yb + 2.f, droite - x, m.ligne,
													  NkRole::TextMuted, 12u, m, mes, ctx, NkAiSource::EffetBloc);
					yb += 2.f + (float32)n * m.ligne;
				}

				// ── LA REFLEXION DEPLIEE : son contenu, attenue, sous la ligne ──
				if (!b.replie && b.type == NkAiBloc::Reflexion && b.texte.Length() > 0) {
					const uint32 n = NkAiPublierTexte(plan, b.id, b.texte.CStr(), true, x, yb + 2.f, droite - x,
													  m.ligne, NkRole::TextMuted, 400u, m, mes, ctx);
					yb += 2.f + (float32)n * m.ligne;
				}

				// ── LA BOITE D'OUTIL : IN puis OUT, dans UN cadre ──
				if (!b.replie && b.type == NkAiBloc::Outil && (b.entree.Length() > 0 || b.sortie.Length() > 0)) {
					yb += 4.f;
					const float32 bx = x, bw = droite - x;
					const uint32 iBoite = plan.Pieces();
					Pousser(b.id, NkAiPiece::BoiteOutil, NkRole::CodeBg, NkAiPolice::Normale, bx, yb, bw, 0.f);
					const float32 y0 = yb;
					// LA GOUTTIERE SE MESURE sur les etiquettes de L'HOTE (« Demande »,
					// « Pose ») : 42 px tenaient « OUT », pas un mot.
					float32 gout = m.gouttiere;
					if (mes) {
						const float32 le = b.etiquetteEntree.Length() ? mes(ctx, NkAiPolice::Normale, b.etiquetteEntree.CStr()) : 0.f;
						const float32 ls = b.etiquetteSortie.Length() ? mes(ctx, NkAiPolice::Normale, b.etiquetteSortie.CStr()) : 0.f;
						const float32 lm = (le > ls ? le : ls) + 18.f;
						if (lm > gout)
							gout = lm;
					}
					const float32 xc = bx + gout;
					const float32 wc = droite - 8.f - xc;
					NkVector<aidetail::Tranche> lg;
					bool premier = true;
					for (int32 comp = 0; comp < 2; ++comp) {
						const NkString &src = comp == 0 ? b.entree : b.sortie;
						if (src.Length() == 0)
							continue;
						// un RESULTAT EN CLAIR se lit en entier (12 lignes) : c'est la liste de
						// ce qui a ete fait, pas un journal a survoler
						const bool tr =
							aidetail::LignesCode(src.CStr(), (comp == 1 && b.sortieEnClair) ? 12u : m.lignesMax, lg);
						const uint32 n = (uint32)lg.Size();
						const float32 h = (float32)n * m.ligneCode + 2.f * m.padding;
						if (!premier)
							Pousser(b.id, NkAiPiece::Separateur, NkRole::Border, NkAiPolice::Normale, bx, yb, bw, 1.f);
						premier = false;
						if (comp == 0)
							Pousser(b.id, NkAiPiece::FondIn, NkRole::CodeBg, NkAiPolice::Normale, bx + 1.f, yb + 1.f,
									bw - 2.f, h - 1.f);
						else
							Pousser(b.id, NkAiPiece::FondOut, NkRole::CodeOutBg, NkAiPolice::Normale, xc - 2.f,
									yb + m.padding * 0.5f, droite - 8.f - (xc - 2.f), h - m.padding);
						Pousser(b.id, comp == 0 ? NkAiPiece::GouttiereIn : NkAiPiece::GouttiereOut, NkRole::TextMuted,
								NkAiPolice::Normale, bx + 8.f, yb + m.padding, gout - 10.f, m.ligneCode);
						for (uint32 k = 0; k < n; ++k) {
							NkAiRectPublie r;
							r.blocId = b.id;
							r.piece = comp == 0 ? NkAiPiece::TexteIn : NkAiPiece::TexteOut;
							r.role = NkRole::Text;
							r.police = (comp == 1 && b.sortieEnClair) ? NkAiPolice::Normale : NkAiPolice::ChasseFixe;
							r.x = xc;
							r.y = yb + m.padding + (float32)k * m.ligneCode;
							r.w = wc;
							r.h = m.ligneCode;
							r.debut = lg[k].debut;
							r.longueur = lg[k].fin - lg[k].debut;
							plan.Ajouter(r);
						}
						if (tr)
							Pousser(b.id, NkAiPiece::Estompe, comp == 0 ? NkRole::CodeBg : NkRole::CodeOutBg,
									NkAiPolice::Normale, comp == 0 ? bx + 1.f : xc - 2.f,
									yb + h - m.padding - m.estompeH * 0.6f,
									comp == 0 ? bw - 2.f : droite - 8.f - (xc - 2.f), m.estompeH);
						yb += h;
					}
					// LA VIGNETTE : ce qui a ete pose, trace a l'echelle dans la boite.
					if (b.vignette.Size() > 0) {
						float32 vw = wc;
						if (vw > 240.f)
							vw = 240.f;
						float32 vh = vw * (b.vignetteRapport > 0.05f ? b.vignetteRapport : 0.75f);
						if (vh > 150.f) {
							vw *= 150.f / vh;
							vh = 150.f;
						}
						const float32 vx = xc, vy = yb + 6.f;
						Pousser(b.id, NkAiPiece::VignetteFond, NkRole::PanelBg, NkAiPolice::Normale, vx, vy, vw, vh);
						for (usize k = 0; k < b.vignette.Size(); ++k) {
							const NkAiBlocDonnees::Vignette &v = b.vignette[k];
							NkAiRectPublie r;
							r.blocId = b.id;
							r.piece = NkAiPiece::VignetteRect;
							r.role = NkRole::Border;
							r.x = vx + v.x * vw;
							r.y = vy + v.y * vh;
							r.w = v.w * vw > 1.f ? v.w * vw : 1.f;
							r.h = v.h * vh > 1.f ? v.h * vh : 1.f;
							r.debut = v.genre;
							plan.Ajouter(r);
						}
						yb = vy + vh + 8.f;
					}
					plan.PieceModifiable(iBoite).h = yb - y0;
				}

				// ── LES BOUTONS DU BLOC (« Annuler cette action ») ──
				if (actions && actions->blocId == b.id) {
					float32 ax = x;
					bool pose = false;
					for (uint32 a = 0; a < 2; ++a) {
						const char *lib = actions->libelle[a];
						if (!lib || !lib[0])
							continue;
						const float32 wl = (mes ? mes(ctx, NkAiPolice::Normale, lib) : 0.f) + 20.f;
						if (ax + wl > droite)
							break;
						NkAiRectPublie r;
						r.blocId = b.id;
						r.piece = NkAiPiece::Action;
						r.source = NkAiSource::Action;
						r.role = NkRole::Text;
						r.x = ax;
						r.y = yb + 6.f;
						r.w = wl;
						r.h = m.ligne + 4.f;
						r.debut = a;
						r.drapeaux = actions->actif[a] ? 0u : kAiEteint;
						plan.Ajouter(r);
						ax += wl + 8.f;
						pose = true;
					}
					if (pose)
						yb += 6.f + m.ligne + 4.f;
				}
				y = yb + m.entreBlocs;
			}
			// ── LE RAIL : un filet entre deux puces CONSECUTIVES du fil ──
			for (usize k = 1; k < centres.Size(); ++k) {
				if (centres[k - 1] < 0.f || centres[k] < 0.f)
					continue;
				Pousser(0u, NkAiPiece::Rail, NkRole::Border, NkAiPolice::Normale, m.railX - 0.5f, centres[k - 1], 1.f,
						centres[k] - centres[k - 1]);
			}
			plan.PoserHauteur(y);
		}

		// ── L'EN-TETE : le sujet tronque, l'historique, la nouvelle conversation ─
		struct NkAiEnteteDecl {
				bool porteHistorique = false;
				bool porteNouvelle = false;
		};

		/// Rend la hauteur consommee (0 si trop etroit). Le titre n'est coupe en
		/// fragment que si une mesure est fournie ; sans elle, seule sa ZONE est
		/// publiee (c'est ce que l'essai 22s interroge).
		inline float32 NkAiEnteteMesurer(const char *titre, float32 largeur, float32 y0, const NkAiEnteteDecl &decl,
										 const NkAiMetriques &m, NkAiPlan &plan, NkAiMesureTexte mes = nullptr,
										 void *ctx = nullptr) {
			if (largeur <= m.retraitDemande * 2.f)
				return 0.f; // trop etroit : rien plutot qu un titre ecrase
			float32 xDroite = largeur - (m.margeTitre + 1.f);
			NkAiRectPublie r;
			r.blocId = 0u;
			if (decl.porteNouvelle) {
				xDroite -= m.icone;
				r.piece = NkAiPiece::IconeNouvelle;
				r.role = NkRole::TextMuted;
				r.x = xDroite;
				r.y = y0 + (m.entete - m.icone) * 0.5f;
				r.w = m.icone;
				r.h = m.icone;
				plan.Ajouter(r);
				xDroite -= m.icone + 1.f;
			}
			if (decl.porteHistorique) {
				xDroite -= m.icone;
				r.piece = NkAiPiece::IconeHistorique;
				r.role = NkRole::TextMuted;
				r.x = xDroite;
				r.y = y0 + (m.entete - m.icone) * 0.5f;
				r.w = m.icone;
				r.h = m.icone;
				plan.Ajouter(r);
				xDroite -= 10.f;
			}
			const float32 wTitre = (xDroite - m.margeTitre) > 0.f ? (xDroite - m.margeTitre) : 0.f;
			if (wTitre > 0.f && titre && titre[0]) {
				// LA ZONE DU TITRE, a largeur fixe (22s : les icones ne bougent pas
				// avec lui), puis son texte coupe.
				NkAiRectPublie z;
				z.blocId = 0u;
				z.piece = NkAiPiece::TitreConversation;
				z.role = NkRole::Text;
				z.police = NkAiPolice::Grasse;
				z.x = m.margeTitre;
				z.y = y0 + (m.entete - m.ligne) * 0.5f;
				z.w = wTitre;
				z.h = m.ligne;
				plan.Ajouter(z);
				if (mes)
					(void)NkAiPublierLigne(plan, 0u, NkAiPiece::ChromeTexte, NkAiSource::Titre, titre, z.x, z.y, wTitre,
										   m.ligne, NkRole::Text, NkAiPolice::Grasse, mes, ctx);
			}
			r.piece = NkAiPiece::Filet;
			r.role = NkRole::Border;
			r.police = NkAiPolice::Normale;
			r.x = 0.f;
			r.y = y0 + m.entete - 1.f;
			r.w = largeur;
			r.h = 1.f;
			plan.Ajouter(r);
			return m.entete;
		}

		// ── LE COMPOSEUR ET SA BARRE DU BAS ─────────────────────────────────────
		/// Ce que le porteur declare. ⚠️ TOUT CE QUI N'A PAS DE SOURCE EST ABSENT,
		/// et c'est la meme regle que les blocs : un « + » qui n'ouvre rien, une
		/// duree qui ne mesure rien ne se publient pas.
		struct NkAiComposeurDecl {
				const char *texte = nullptr;  ///< la saisie
				const char *invite = nullptr; ///< l'invite quand la saisie est vide
				int32 caret = -1;			  ///< octet du curseur, -1 = pas de focus
				bool portePlus = false;
				bool porteCommandes = false;
				bool occupe = false;		 ///< un tour est en cours : l'indicateur d'activite
				bool arretSeul = false;		 ///< l'envoi est un bouton d'ARRET (rien a mettre en file)
				const char *duree = nullptr; ///< « 57m », ou nullptr
				const char *lieu = nullptr;	 ///< « local » / « distant », ou nullptr
				bool distant = false;
				const char *modele = nullptr; ///< « Ollama · qwen2.5 7B »
				const char *modeleDetail = nullptr;
				/// (Q8) La hauteur RESERVEE en tete du cadre (les vignettes des images
				/// jointes) : le texte descend d'autant, le cadre grandit d'autant.
				float32 reserveHaut = 0.f;
				const char *mode = nullptr; ///< « Auto », « Générer », ou nullptr
				bool envoiActif = false;	///< quelque chose a envoyer
				/// Le geste d'envoi PRODUIT-il un document ? (§7 de la spec) : orange
				/// s'il produit, neutre s'il discute. `true` par defaut : la capture.
				bool envoiProduit = true;
		};

		/// Publie le composeur au BAS du panneau (largeur x hauteur). Rend le haut
		/// du cadre -- le fil s'arrete au-dessus.
		inline float32 NkAiComposeurMesurer(const NkAiComposeurDecl &d, float32 largeur, float32 hauteur,
											const NkAiMetriques &m, NkAiMesureTexte mes, void *ctx, NkAiPlan &plan) {
			const float32 mc = m.margeComposeur;
			if (largeur <= mc * 2.f + 80.f || hauteur <= m.barreEtat + m.margeBas + m.entete + 40.f)
				return hauteur; // trop petit : aucun composeur plutot qu un composeur faux
			const float32 fx = mc, fw = largeur - mc * 2.f;
			const float32 xt = fx + m.texteComposeur;
			const float32 wt = fw - m.texteComposeur * 2.f;
			const bool vide = !d.texte || !d.texte[0];
			const char *quoi = vide ? d.invite : d.texte;
			// Les lignes : la saisie REPLIEE. Au-dela du plafond, le cadre ne grandit
			// plus et c'est la FIN qu'on voit -- la ou l'on tape.
			NkVector<aidetail::Tranche> lg;
			if (quoi && quoi[0])
				(void)aidetail::Replier(quoi, wt, NkAiPolice::Normale, mes, ctx, 4096u, lg);
			// un '\n' final ouvre une ligne de plus : le curseur y est
			if (!vide) {
				uint32 n = 0;
				while (d.texte[n])
					++n;
				if (n > 0 && d.texte[n - 1] == '\n') {
					aidetail::Tranche t;
					t.debut = n;
					t.fin = n;
					lg.PushBack(t);
				}
			}
			const uint32 n = (uint32)lg.Size();
			uint32 vis = n < m.lignesComposeurMin ? m.lignesComposeurMin : n;
			if (vis > m.lignesComposeurMax)
				vis = m.lignesComposeurMax;
			const uint32 premiere = n > vis ? n - vis : 0u;
			const float32 fh = d.reserveHaut + m.hautComposeur + (float32)vis * m.ligne + m.ecartSousTexte + m.barreEtat;
			const float32 fy = hauteur - m.margeBas - fh;

			NkAiRectPublie r;
			r.blocId = 0u;
			r.piece = NkAiPiece::ComposeurCadre;
			r.role = NkRole::PanelHeader;
			r.x = fx;
			r.y = fy;
			r.w = fw;
			r.h = fh;
			plan.Ajouter(r);

			for (uint32 k = premiere; k < n; ++k) {
				if (lg[k].fin <= lg[k].debut)
					continue; // une ligne vide ne porte aucun glyphe
				NkAiRectPublie t;
				t.blocId = 0u;
				t.piece = NkAiPiece::ComposeurTexte;
				t.source = vide ? NkAiSource::Invite : NkAiSource::Saisie;
				t.role = vide ? NkRole::TextMuted : NkRole::Text;
				t.x = xt;
				t.y = fy + d.reserveHaut + m.hautComposeur + (float32)(k - premiere) * m.ligne;
				t.w = wt;
				t.h = m.ligne;
				t.debut = lg[k].debut;
				t.longueur = lg[k].fin - lg[k].debut;
				plan.Ajouter(t);
			}
			// ── LE CURSEUR : dans sa ligne, apres la tranche qui le precede ──
			if (d.caret >= 0) {
				uint32 ligneC = 0;
				for (uint32 k = 0; k < n; ++k)
					if ((uint32)d.caret >= lg[k].debut)
						ligneC = k;
				float32 cx = xt, cyy = fy + d.reserveHaut + m.hautComposeur;
				if (!vide && n > 0 && ligneC >= premiere) {
					cx = xt + aidetail::Largeur(d.texte, lg[ligneC].debut, (uint32)d.caret, NkAiPolice::Normale, mes, ctx);
					cyy = fy + d.reserveHaut + m.hautComposeur + (float32)(ligneC - premiere) * m.ligne;
				}
				NkAiRectPublie c;
				c.blocId = 0u;
				c.piece = NkAiPiece::Caret;
				c.role = NkRole::Text;
				c.x = cx;
				c.y = cyy + 2.f;
				c.w = 1.f;
				c.h = m.ligne - 4.f;
				plan.Ajouter(c);
			}

			// ── LA BARRE DU BAS ─────────────────────────────────────────────────
			const float32 by = fy + fh - m.barreEtat;
			const float32 cy = by + m.barreEtat * 0.5f;
			const float32 ic = m.icone;
			auto larg = [&](const char *s) {
				return (s && mes) ? mes(ctx, NkAiPolice::Normale, s) : 0.f;
			};
			// A DROITE, d'abord : l'envoi ne cede jamais sa place.
			const float32 ex = fx + fw - 6.f - m.envoi;
			const float32 droiteLibre = ex - 12.f;
			float32 wMode = (d.mode && d.mode[0]) ? (12.f + 6.f + larg(d.mode) + 8.f) : 0.f;
			// A GAUCHE, dans l'ordre de la capture. Ce qui ne tient pas CEDE, dans
			// l'ordre inverse de son importance : la duree, puis le lieu, puis les
			// commandes -- jamais le modele, qui dit A QUI on parle.
			float32 wDur = (d.duree && d.duree[0]) ? (ic + 3.f + larg(d.duree) + 12.f) : 0.f;
			float32 wLieu = (d.lieu && d.lieu[0]) ? (10.f + 7.f + 6.f + larg(d.lieu) + 10.f + 8.f) : 0.f;
			float32 wCmd = d.porteCommandes ? (ic + 13.f) : 0.f;
			const float32 wPlus = d.portePlus ? (ic + 11.f) : 0.f;
			const float32 wAct = d.occupe ? (ic + 11.f) : 0.f;
			float32 wMod = 0.f;
			if (d.modele && d.modele[0])
				wMod = 10.f + larg(d.modele) +
					   ((d.modeleDetail && d.modeleDetail[0]) ? 5.f + larg(d.modeleDetail) : 0.f) + 10.f;
			const float32 x0 = fx + 11.f;
			auto total = [&]() {
				return wPlus + wCmd + wAct + wDur + wLieu + wMod + (wMode > 0.f ? wMode + 8.f : 0.f);
			};
			const float32 dispo = droiteLibre - x0;
			// L'ORDRE DE CE QUI CEDE, mesure sur le panneau du modeleur (276 px) :
			// d'abord ce qui informe (duree, lieu), puis la LONGUEUR du modele (il
			// s'ellipse, il ne disparait pas), puis le mode, et le « / » en dernier
			// -- c'est un geste, pas une information.
			auto reduire = [&](float32 &w, float32 plancher, float32 extra) {
				if (w <= 0.f || total() <= dispo)
					return;
				const float32 reste = dispo - (total() - w - extra);
				w = (reste - extra > plancher) ? reste - extra : plancher;
			};
			// 21/09 (coordinateur, sur les images des applications) : la DUREE et le
			// MODE sont dans la capture a toute largeur ; le LIEU cede le premier,
			// puis la longueur du modele et du mode, et la duree seulement ensuite.
			if (total() > dispo)
				wLieu = 0.f;
			reduire(wMod, 96.f, 0.f);
			reduire(wMode, 56.f, 8.f);
			if (total() > dispo)
				wDur = 0.f;
			if (total() > dispo)
				wCmd = 0.f;
			reduire(wMod, 48.f, 0.f);
			if (total() > dispo)
				wMode = 0.f;
			if (total() > dispo)
				wMod = 0.f;

			auto piece = [&](NkAiPiece p, NkRole role, float32 x, float32 yy, float32 w, float32 h, uint8 dr) {
				NkAiRectPublie q;
				q.blocId = 0u;
				q.piece = p;
				q.role = role;
				q.x = x;
				q.y = yy;
				q.w = w;
				q.h = h;
				q.drapeaux = dr;
				plan.Ajouter(q);
			};
			float32 x = x0;
			if (wPlus > 0.f) {
				piece(NkAiPiece::BoutonPlus, NkRole::TextMuted, x, cy - ic * 0.5f, ic, ic, 0u);
				x += wPlus;
			}
			if (wCmd > 0.f) {
				piece(NkAiPiece::BoutonCommandes, NkRole::TextMuted, x, cy - ic * 0.5f, ic, ic, 0u);
				x += wCmd;
			}
			if (wAct > 0.f) {
				piece(NkAiPiece::Activite, NkRole::AccentSel, x, cy - ic * 0.5f, ic, ic, 0u);
				x += wAct;
			}
			if (wDur > 0.f) {
				piece(NkAiPiece::Horloge, NkRole::TextMuted, x, cy - (ic - 2.f) * 0.5f, ic - 2.f, ic - 2.f, 0u);
				(void)NkAiPublierLigne(plan, 0u, NkAiPiece::ChromeTexte, NkAiSource::Duree, d.duree, x + ic + 1.f,
									   cy - m.ligne * 0.5f, wDur - ic - 1.f, m.ligne, NkRole::TextMuted,
									   NkAiPolice::Normale, mes, ctx);
				x += wDur;
			}
			if (wLieu > 0.f) {
				const float32 w = wLieu - 8.f;
				piece(NkAiPiece::PastilleLieu, d.distant ? NkRole::StatusWarn : NkRole::StatusOk, x,
					  cy - m.pastilleH * 0.5f, w, m.pastilleH, d.distant ? kAiDistant : 0u);
				(void)NkAiPublierLigne(plan, 0u, NkAiPiece::ChromeTexte, NkAiSource::Lieu, d.lieu, x + 10.f + 7.f + 6.f,
									   cy - m.ligne * 0.5f, w - 23.f, m.ligne, NkRole::Text, NkAiPolice::Normale, mes,
									   ctx);
				x += wLieu;
			}
			if (wMod > 0.f) {
				piece(NkAiPiece::PastilleModele, NkRole::Border, x, cy - m.pastilleH * 0.5f, wMod, m.pastilleH, 0u);
				const float32 wm =
					NkAiPublierLigne(plan, 0u, NkAiPiece::ChromeTexte, NkAiSource::Modele, d.modele, x + 10.f,
									 cy - m.ligne * 0.5f, wMod - 20.f, m.ligne, NkRole::Text, NkAiPolice::Normale, mes, ctx);
				if (d.modeleDetail && d.modeleDetail[0] && wMod - 20.f - wm > 20.f)
					(void)NkAiPublierLigne(plan, 0u, NkAiPiece::ChromeTexte, NkAiSource::ModeleDetail, d.modeleDetail,
										   x + 10.f + wm + 5.f, cy - m.ligne * 0.5f, wMod - 20.f - wm - 5.f, m.ligne,
										   NkRole::TextMuted, NkAiPolice::Normale, mes, ctx);
				x += wMod;
			}
			if (wMode > 0.f) {
				const float32 mx = ex - 12.f - wMode;
				piece(NkAiPiece::PastilleMode, NkRole::TextMuted, mx, cy - m.pastilleH * 0.5f, wMode, m.pastilleH, 0u);
				(void)NkAiPublierLigne(plan, 0u, NkAiPiece::ChromeTexte, NkAiSource::Mode, d.mode, mx + 12.f + 6.f,
									   cy - m.ligne * 0.5f, wMode - 18.f - 4.f, m.ligne, NkRole::Text,
									   NkAiPolice::Normale, mes, ctx);
			}
			// L'ENVOI. Orange s'il produit (la capture) ; bleu s'il discute (§7) ;
			// eteint s'il n'y a rien a envoyer ; carre d'arret pendant un tour.
			{
				uint8 dr = 0;
				if (d.arretSeul)
					dr |= kAiArret;
				else if (!d.envoiActif)
					dr |= kAiEteint;
				piece(NkAiPiece::Envoi, d.envoiProduit ? NkRole::AccentSel : NkRole::AccentUi, ex, cy - m.envoi * 0.5f,
					  m.envoi, m.envoi, dr);
			}
			return fy;
		}

		/// Le bloc sous un point du plan du FIL (coordonnees du plan). Rend
		/// l'IDENTIFIANT, jamais la position -- la fenetre glissante decale les
		/// positions sans que personne n'insere rien.
		inline bool NkAiFilBlocSous(const NkAiPlan &plan, float32 px, float32 py, uint32 &blocIdOut) {
			for (uint32 i = plan.Pieces(); i > 0; --i) {
				const NkAiRectPublie &r = plan.Piece(i - 1);
				if (r.blocId == 0u || r.w <= 0.f || r.h <= 0.f)
					continue;
				if (px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h) {
					blocIdOut = r.blocId;
					return true;
				}
			}
			return false;
		}

	} // namespace editorkit
} // namespace nkentseu
