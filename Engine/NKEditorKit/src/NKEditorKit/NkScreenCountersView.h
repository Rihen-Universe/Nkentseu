#pragma once
// -----------------------------------------------------------------------------
// @File    Engine/NKEditorKit/src/NKEditorKit/NkScreenCountersView.h
// @Brief   (A) LES COMPTEURS — Draw, Tris, Sommets, Lots, GPU, CPU, FPS, dt —
//          rendus dans la vue, en haut a droite, sous leur propre interrupteur.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// POURQUOI CE FICHIER EXISTE
//   Le HUD de laboratoire de `renderdemo` peignait ces chiffres PAR-DESSUS le
//   produit, dans le projet d'un utilisateur, et sous la MEME garde que l'aide
//   produit et que le rectangle de selection. Il a ete eteint le 25/09.
//   Rodolf n'a jamais demande qu'ils disparaissent : il a demande qu'ils aient
//   un domicile digne. Le voici.
//
// ⚠️ CE N'EST PAS (B), ET LES CONFONDRE A DEJA COUTE
//   Un compteur est CONTINU et s'allume par un interrupteur ; un message est
//   TRANSITOIRE et arrive par un evenement. Ils n'ont ni la meme duree de vie,
//   ni le meme declencheur, ni le meme public. Les faire passer par le journal
//   recreerait la garde unique qu'on vient de defaire, et un chiffre qui change
//   a chaque image inonderait le fichier et l'ecran.
//   -> AUCUN lien avec `NkScreenLogSink.h`. Les deux vues cohabitent : les
//      messages en BAS, les compteurs en HAUT A DROITE. Deux piles qui se
//      poussent sont une pile qui cache l'autre.
//
// ⚠️ LES CHIFFRES SONT RELAYES, JAMAIS RECALCULES
//   `NkEcranCompteurs` est un transport : l'hote le remplit depuis
//   `NkRenderer::GetStats()` et rien d'autre. Ce depot a paye *une derivation en
//   double, pas une compensation* : l'un publiait, l'autre lisait, et les deux
//   ont diverge en silence. Un compteur recalcule a cote de sa source derive
//   sans que rien ne le signale.
//
// ⚠️ ET UN ZERO QUI N'EST PAS UN ZERO
//   `NkRendererStats::gpuTimeValid` vaut faux tant qu'aucune requete GPU n'a
//   repondu. Afficher « 0.00 ms » dans ce cas serait un chiffre FAUX, pas une
//   absence : cette vue ecrit « -- », comme le faisait deja le HUD du moteur
//   (note du 2026-09-04). *Un compteur dont le zero n'est pas zero.*
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkTheme.h"
#include "NKEditorKit/Components/NkComponentPaint.h"

namespace nkentseu {
	namespace editorkit {

		/// LE TRANSPORT. Rempli par l'hote depuis `NkRenderer::GetStats()`.
		/// Chaque champ porte, quand c'est necessaire, son drapeau de VALIDITE :
		/// une mesure absente et une mesure a zero ne sont pas la meme chose.
		struct NkEcranCompteurs {
				uint32 draws = 0;
				uint32 triangles = 0;
				uint32 sommets = 0;
				uint32 lots = 0;	  ///< batchCount
				uint32 ecartes = 0;	  ///< culled
				uint32 lumieres = 0;  ///< lightsActive
				uint32 ombreurs = 0;  ///< shadowCasters
				float32 gpuMs = 0.f;
				bool gpuValide = false; ///< faux -> « -- », jamais « 0.00 »
				float32 cpuMs = 0.f;
				bool cpuValide = false;
				float32 fps = 0.f;
				float32 dtMs = 0.f;
				bool horlogeValide = false; ///< faux avant la premiere image mesuree
				const char *api = nullptr;	///< « Vulkan », « OpenGL »... ou nullptr
		};

		/// Le LISSAGE de l'horloge, et il vit ici pour que tous les hotes aient le
		/// meme. Une image dure 16 ms puis 19 puis 15 : affiche brut, le nombre
		/// clignote et devient illisible. Constante de temps ~0,4 s.
		///
		/// ⚠️ CE N'EST PAS UN RECALCUL DE CHIFFRE DU MOTEUR : `dt` appartient a la
		///    BOUCLE, pas au renderer, et le renderer n'en publie aucun. Il n'y a
		///    donc pas deux sources qui pourraient diverger -- il n'y en a qu'une.
		class NkEcranHorloge {
			public:
				void Tick(float32 dt) {
					if (dt <= 0.f)
						return;
					const float32 a = 0.08f; // ~0,4 s a 60 images/s
					mDt = mValide ? (mDt + (dt - mDt) * a) : dt;
					mValide = true;
				}
				bool Valide() const {
					return mValide;
				}
				float32 DtMs() const {
					return mDt * 1000.f;
				}
				float32 Fps() const {
					return mDt > 1e-6f ? 1.f / mDt : 0.f;
				}

			private:
				float32 mDt = 0.f;
				bool mValide = false;
		};

		// ── LA VUE ──────────────────────────────────────────────────────────────
		class NkEcranCompteursVue {
			public:
				/// Peint le panneau en HAUT A DROITE de `zone` et rend le rectangle
				/// occupe (vide si rien). L'appelant en interdit l'entree a ce qui
				/// est dessous.
				///
				/// ⚠️ L'APPELANT DOIT PEINDRE EN DERNIER (couche d'incrustation),
				///    pour la meme raison que les messages : un panneau peint apres
				///    recouvrirait les chiffres, et ils existeraient sans se voir.
				NkPaintRect Peindre(NkComponentPaint &p, const NkPaintRect &zone,
									const NkEcranCompteurs &c) const {
					const float32 lh = p.LineHeight();
					const float32 pad = 8.f, marge = 10.f;

					// Deux colonnes : le LIBELLE et la VALEUR. La valeur est alignee
					// a droite -- des nombres alignes a gauche sautent d'une image a
					// l'autre et on ne peut plus lire une tendance.
					char valeurs[kLignes][24];
					const char *libelles[kLignes];
					uint32 n = 0;
					Entier(libelles, valeurs, n, "Draw", c.draws);
					Entier(libelles, valeurs, n, "Tris", c.triangles);
					Entier(libelles, valeurs, n, "Sommets", c.sommets);
					Entier(libelles, valeurs, n, "Lots", c.lots);
					Entier(libelles, valeurs, n, "Ecartes", c.ecartes);
					Entier(libelles, valeurs, n, "Lumieres", c.lumieres);
					Entier(libelles, valeurs, n, "Ombreurs", c.ombreurs);
					Millis(libelles, valeurs, n, "GPU", c.gpuMs, c.gpuValide);
					Millis(libelles, valeurs, n, "CPU", c.cpuMs, c.cpuValide);
					Millis(libelles, valeurs, n, "dt", c.dtMs, c.horlogeValide);
					Decimal(libelles, valeurs, n, "FPS", c.fps, c.horlogeValide);

					float32 wLib = 0.f, wVal = 0.f;
					for (uint32 i = 0; i < n; ++i) {
						const float32 a = p.TextWidth(libelles[i]);
						const float32 b = p.TextWidth(valeurs[i]);
						if (a > wLib)
							wLib = a;
						if (b > wVal)
							wVal = b;
					}
					const char *titre = c.api && c.api[0] ? c.api : "Rendu";
					const float32 wTitre = p.TextWidth(titre);
					float32 largeur = wLib + wVal + pad * 3.f;
					if (largeur < wTitre + pad * 2.f)
						largeur = wTitre + pad * 2.f;
					const float32 hauteur = (float32)(n + 1u) * lh + pad * 2.f;
					if (largeur > zone.w || hauteur > zone.h)
						return {0.f, 0.f, 0.f, 0.f}; // la vue est trop petite : on se tait

					const NkPaintRect r{zone.x + zone.w - largeur - marge, zone.y + marge, largeur,
										hauteur};
					p.Fill(r, (uint16)NkRole::PanelHeader, 6.f);
					p.OutlineSharp(r, (uint16)NkRole::Border);

					float32 y = r.y + pad;
					p.Text({r.x + pad, y, largeur - pad * 2.f, lh}, titre, (uint16)NkRole::AccentUi,
						   NkTextAlign::Left);
					y += lh;
					for (uint32 i = 0; i < n; ++i) {
						p.Text({r.x + pad, y, wLib, lh}, libelles[i], (uint16)NkRole::TextMuted,
							   NkTextAlign::Left);
						p.Text({r.x + largeur - pad - wVal, y, wVal, lh}, valeurs[i],
							   (uint16)NkRole::Text, NkTextAlign::Right);
						y += lh;
					}
					return r;
				}

				static constexpr uint32 kLignes = 11;

			private:
				static void Entier(const char **lib, char (*val)[24], uint32 &n, const char *nom,
								   uint32 v) {
					if (n >= kLignes)
						return;
					lib[n] = nom;
					Nombre(val[n], 24u, v);
					++n;
				}
				/// ⚠️ `valide == false` ECRIT « -- », JAMAIS « 0.00 ». Une mesure
				///    absente et une mesure nulle ne sont pas la meme chose, et
				///    `gpuTimeValid` existe precisement pour les distinguer.
				static void Millis(const char **lib, char (*val)[24], uint32 &n, const char *nom,
								   float32 v, bool valide) {
					if (n >= kLignes)
						return;
					lib[n] = nom;
					if (!valide) {
						val[n][0] = '-';
						val[n][1] = '-';
						val[n][2] = '\0';
					} else {
						Fixe2(val[n], 24u, v);
						Suffixe(val[n], 24u, " ms");
					}
					++n;
				}
				static void Decimal(const char **lib, char (*val)[24], uint32 &n, const char *nom,
									float32 v, bool valide) {
					if (n >= kLignes)
						return;
					lib[n] = nom;
					if (!valide) {
						val[n][0] = '-';
						val[n][1] = '-';
						val[n][2] = '\0';
					} else {
						Fixe2(val[n], 24u, v);
					}
					++n;
				}

				/// Entier en base 10, sans <cstdio> : le kit ecrit son propre
				/// formatage (cf. NkFormat), et une dependance a la libc serait
				/// gratuite dans un en-tete partage.
				static void Nombre(char *dst, uint32 cap, uint32 v) {
					char tmp[12];
					uint32 t = 0;
					if (v == 0)
						tmp[t++] = '0';
					while (v > 0 && t < 12) {
						tmp[t++] = (char)('0' + (v % 10u));
						v /= 10u;
					}
					uint32 k = 0;
					while (t > 0 && k + 1 < cap)
						dst[k++] = tmp[--t];
					dst[k] = '\0';
				}
				/// Deux decimales, sans `printf`. Le negatif est ecrit meme si
				/// aucun de nos champs ne l'est aujourd'hui : un formateur qui perd
				/// le signe rendrait « 3.00 » pour -3, et ce serait indetectable.
				static void Fixe2(char *dst, uint32 cap, float32 v) {
					uint32 k = 0;
					if (v < 0.f) {
						if (k + 1 < cap)
							dst[k++] = '-';
						v = -v;
					}
					const uint32 ent = (uint32)v;
					const uint32 cent = (uint32)((v - (float32)ent) * 100.f + 0.5f);
					char b[24];
					Nombre(b, 24u, cent >= 100u ? ent + 1u : ent);
					for (uint32 i = 0; b[i] && k + 1 < cap; ++i)
						dst[k++] = b[i];
					if (k + 1 < cap)
						dst[k++] = '.';
					const uint32 c2 = cent >= 100u ? 0u : cent;
					if (k + 1 < cap)
						dst[k++] = (char)('0' + (c2 / 10u));
					if (k + 1 < cap)
						dst[k++] = (char)('0' + (c2 % 10u));
					dst[k] = '\0';
				}
				static void Suffixe(char *dst, uint32 cap, const char *s) {
					uint32 k = 0;
					while (dst[k])
						++k;
					for (uint32 i = 0; s[i] && k + 1 < cap; ++i)
						dst[k++] = s[i];
					dst[k] = '\0';
				}
		};

	} // namespace editorkit
} // namespace nkentseu
