#pragma once
// -----------------------------------------------------------------------------
// @File    Engine/NKEditorKit/src/NKEditorKit/NkVignetteImage.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   LA VIGNETTE D'UNE IMAGE, SANS TEXTURE : l'image reduite a une grille
//          de cellules de couleur moyenne, avec son cache, son plafond et son
//          compteur de decodages REELS.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE FICHIER EXISTE (Q11, 22/09) — ET POURQUOI IL N'EST PAS UN SECOND
//  CHEMIN
// =============================================================================
//  Rodolf, capture 2026-09-22 051749 : « + -> Joindre une image » ouvre un
//  selecteur de 98 fichiers qui montrent tous la MEME icone generique. On
//  choisit une image a l'aveugle, au nom du fichier.
//
//  Le kit savait deja reduire une image : `NkAiVignetteDe`, ecrite en Q8 pour la
//  vignette du composeur, vivait DANS `NkAiThreadPaint.h` -- c'est-a-dire dans
//  la transcription du panneau IA, qui n'a rien a voir avec un selecteur de
//  fichiers. Ecrire un second reducteur pour le selecteur aurait fait deux
//  verites sur le meme fait, qui se decorrelent au premier changement.
//  ⚠️ CE FICHIER NE CONTIENT AUCUNE LIGNE NEUVE DE REDUCTION : le corps de la
//     boucle de moyennage est CELUI de Q8, deplace. Ce qui est ajoute autour,
//     c'est ce qui manquait pour tenir 98 fichiers au lieu d'un seul :
//       - la CLE porte la date de modification ET la taille de grille demandee
//         (un fichier reecrit doit changer de vignette ; le composeur et le
//         selecteur ne demandent pas la meme finesse) ;
//       - un PLAFOND D'OCTETS : au-dela, on ne decode pas du tout ;
//       - un plafond d'ENTREES, avec remplacement circulaire : un dossier de
//         10 000 images ne doit pas faire enfler le processus sans fin ;
//       - `tente` : un fichier casse est tente UNE fois, pas a chaque image ;
//       - `NkVignetteDecodages()` COMPTE les decodages reels. Sans ce compteur,
//         « c'est en cache » serait une affirmation, pas une mesure -- meme
//         lecon que `NkCacheDossiers::accesDisque`.
//
//  ⚠️ CE FICHIER NE CONNAIT NI NKFileSystem NI NKGui. Il ne va pas chercher la
//     date ni la taille d'un fichier : l'appelant les lui DONNE (le selecteur
//     les a deja lues pour son tri). C'est ce qui lui permet de vivre sous la
//     couche des composants sans y faire entrer le systeme de fichiers.
//
//  ⚠️ IL NE FABRIQUE AUCUNE TEXTURE, et c'est assume (memoire « Icon peint un
//     carre ») : le kit n'a pas d'atlas. La vignette est l'image A BASSE
//     DEFINITION, tracee en rectangles -- pas un pictogramme a sa place.
// -----------------------------------------------------------------------------

#include "NKImage/NKImage.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace editorkit {

		/// La grille par defaut du composeur du panneau IA (Q8) : 32 de cote.
		static const int32 kVignetteGrilleDefaut = 32;
		/// Au-dela, on ne decode pas : un PSD de 400 Mo ne doit pas figer la grille.
		static const nk_uint64 kVignetteOctetsMax = 48ull * 1024ull * 1024ull;
		/// Le nombre de vignettes retenues. 32 x 32 x 4 octets = 4 Ko au plus par
		/// entree, donc 2 Mo au plafond -- et il est atteint par remplacement
		/// circulaire, jamais par croissance.
		static const usize kVignetteEntreesMax = 512u;

		struct NkVignetteImage {
				NkString chemin;
				nk_int64 dateModif = 0; ///< partie de la CLE : un fichier reecrit change de vignette
				int32 grille = 0;		///< partie de la CLE : le cote maximal demande
				bool tente = false;		///< decodage deja tente (reussi ou non)
				bool ok = false;		///< faux = illisible, trop gros, ou pas une image
				int32 w = 0, h = 0;		///< les pixels de l'image d'origine
				int32 cw = 0, ch = 0;	///< la grille reellement produite
				NkVector<uint32> cellules; ///< RGBA 0xRRGGBBAA, ligne par ligne
		};

		/// Le nombre de DECODAGES reels depuis le lancement. C'est le chiffre que la
		/// sonde interroge : il dit si le cache travaille, la ou « c'est en cache »
		/// ne dirait rien.
		inline uint32 &NkVignetteDecodages() {
			static uint32 n = 0;
			return n;
		}

		inline NkVector<NkVignetteImage> &NkVignetteCache() {
			static NkVector<NkVignetteImage> v;
			return v;
		}

		/// L'entree vide rendue quand il n'y a rien a montrer. Une reference doit
		/// pointer quelque part, et ce quelque part ne doit jamais bouger.
		inline const NkVignetteImage &NkVignetteVide() {
			static NkVignetteImage v;
			return v;
		}

		namespace detail {
			/// ⚠️ PAS `strcmp` : les deux chemins viennent du meme producteur ici,
			///    mais l'egalite de `NkString` est deja celle du kit et elle est
			///    testee. Une comparaison maison de plus serait une divergence de plus.
			inline bool VignetteMemeCle(const NkVignetteImage &v, const char *chemin, nk_int64 date,
										int32 grille) {
				return v.grille == grille && v.dateModif == date
					   && v.chemin == NkString(chemin ? chemin : "");
			}
		} // namespace detail

		/// Cherche SANS decoder. Rend nullptr quand rien n'est connu de ce fichier a
		/// cette date et a cette finesse. C'est la moitie qui permet a l'appelant de
		/// tenir un BUDGET : il sait ce qui coutera un decodage avant de le demander.
		inline const NkVignetteImage *NkVignetteConnue(const char *chemin, nk_int64 dateModif,
													   int32 grille) {
			if (!chemin || !chemin[0])
				return nullptr;
			NkVector<NkVignetteImage> &c = NkVignetteCache();
			for (usize i = 0; i < c.Size(); ++i)
				if (detail::VignetteMemeCle(c[i], chemin, dateModif, grille))
					return &c[i];
			return nullptr;
		}

		/// L'ENTREE UNIQUE du cache : range `v` et rend sa place definitive.
		inline const NkVignetteImage &NkVignetteRanger(const NkVignetteImage &v) {
			NkVector<NkVignetteImage> &c = NkVignetteCache();
			if (c.Size() < kVignetteEntreesMax) {
				c.PushBack(v);
				return c[c.Size() - 1u];
			}
			// PLEIN : remplacement circulaire. Le plus ancien rang cede sa place --
			// ce n'est pas un LRU, et c'est dit : un vrai LRU demanderait un compteur
			// d'usage par entree pour un gain que 512 vignettes ne justifient pas.
			static usize tour = 0u;
			if (tour >= c.Size())
				tour = 0u;
			c[tour] = v;
			const usize ou = tour;
			++tour;
			return c[ou];
		}

		/// LA VIGNETTE, decodee au plus une fois par (chemin, date, grille).
		///
		/// `octets` : la taille du fichier telle que l'appelant l'a deja lue (0 =
		/// inconnue, on tente). Au-dela de `octetsMax`, on ne decode pas et on rend
		/// une entree `ok = false` -- l'appelant retombe sur son icone generique.
		///
		/// ⚠️ `ok = false` N'EST PAS UNE ERREUR A SIGNALER : un `.txt` passe ici
		///    rend faux, exactement comme un PNG tronque. L'appelant a UN seul
		///    comportement pour les deux -- l'icone de nature -- et c'est ce qui
		///    garantit qu'un fichier casse ne laisse ni case vide ni plantage.
		inline const NkVignetteImage &NkVignetteDe(const char *chemin, nk_int64 dateModif, int32 grille,
												   nk_uint64 octets = 0ull,
												   nk_uint64 octetsMax = kVignetteOctetsMax) {
			if (!chemin || !chemin[0])
				return NkVignetteVide();
			if (grille < 4)
				grille = 4;
			if (grille > 64)
				grille = 64;
			if (const NkVignetteImage *deja = NkVignetteConnue(chemin, dateModif, grille))
				return *deja;

			NkVignetteImage v;
			v.chemin = NkString(chemin);
			v.dateModif = dateModif;
			v.grille = grille;
			v.tente = true;

			if (octets != 0ull && octets > octetsMax)
				return NkVignetteRanger(v); // trop gros : PAS de decodage du tout

			NkImage img;
			++NkVignetteDecodages();
			if (img.Load(chemin, 4) && img.Width() > 0 && img.Height() > 0 && img.Pixels()) {
				v.w = (int32)img.Width();
				v.h = (int32)img.Height();
				// La grille garde le rapport de l'image : le cote long vaut `grille`,
				// le cote court suit. Une vignette etiree montrerait moins, pas plus.
				const float32 k = (float32)v.w / (float32)v.h;
				const float32 g = (float32)grille;
				v.cw = k >= 1.f ? grille : (int32)(g * k + 0.5f);
				v.ch = k >= 1.f ? (int32)(g / k + 0.5f) : grille;
				if (v.cw < 1)
					v.cw = 1;
				if (v.ch < 1)
					v.ch = 1;
				const uint8 *px = img.Pixels();
				const int32 st = (int32)img.Stride();
				// ── LE CORPS DE Q8, MOT POUR MOT ─────────────────────────────────
				// Moyenne des pixels de chaque case. `pas` saute des pixels quand la
				// case est grande : sur une photo de 12 Mpx, moyenner tout coute 12 M
				// lectures pour un resultat que 1 sur 16 donne a l'oeil identique.
				for (int32 cy = 0; cy < v.ch; ++cy)
					for (int32 cx = 0; cx < v.cw; ++cx) {
						const int32 x0 = cx * v.w / v.cw, x1 = (cx + 1) * v.w / v.cw;
						const int32 y0 = cy * v.h / v.ch, y1 = (cy + 1) * v.h / v.ch;
						uint64 r = 0, g2 = 0, b = 0, n = 0;
						const int32 pas = ((x1 - x0) * (y1 - y0) > 256) ? 4 : 1;
						for (int32 yy = y0; yy < (y1 > y0 ? y1 : y0 + 1); yy += pas)
							for (int32 xx = x0; xx < (x1 > x0 ? x1 : x0 + 1); xx += pas) {
								const uint8 *p = px + (usize)yy * (usize)st + (usize)xx * 4u;
								r += p[0];
								g2 += p[1];
								b += p[2];
								++n;
							}
						if (n == 0)
							n = 1;
						v.cellules.PushBack(((uint32)(r / n) << 24) | ((uint32)(g2 / n) << 16)
											| ((uint32)(b / n) << 8) | 0xFFu);
					}
				v.ok = true;
			}
			return NkVignetteRanger(v);
		}

		/// ── LA COMPATIBILITE DU COMPOSEUR (Q8) ───────────────────────────────────
		/// `NkAiThreadPaint.h` et `NkAiPanneau.h` appellent `NkAiVignetteDe(chemin)`
		/// depuis le 21/09. Le nom reste, et il rend exactement ce qu'il rendait :
		/// la grille de 32, sans clause de date (le composeur affiche une image que
		/// l'utilisateur vient de joindre, elle ne change pas sous lui).
		using NkAiVignetteImage = NkVignetteImage;
		inline const NkVignetteImage &NkAiVignetteDe(const char *chemin) {
			return NkVignetteDe(chemin, 0, kVignetteGrilleDefaut);
		}
		inline NkVector<NkVignetteImage> &NkAiCacheVignettes() {
			return NkVignetteCache();
		}

	} // namespace editorkit
} // namespace nkentseu
