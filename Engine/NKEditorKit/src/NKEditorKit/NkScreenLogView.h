#pragma once
// -----------------------------------------------------------------------------
// @File    Engine/NKEditorKit/src/NKEditorKit/NkScreenLogView.h
// @Brief   L'AFFICHAGE du neuvieme puits : ce que le puits a depose devient une
//          pile de bandeaux en bas de la vue, a la maniere d'Unreal.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// POURQUOI ICI ET PAS DANS UNE APPLICATION
//   NK3DModeler, Nogee, NkAnimaEditor et NKScena ont le meme besoin. Ce qui sert
//   plusieurs hotes descend sous eux, une fois, au debut. Ce fichier est ecrit
//   contre `NkComponentPaint` -- l'abstraction de peinture du kit, deja
//   implementee trois fois (`NkGuiComponentPaint` pour NKGui,
//   `NkModelerComponentPaint` dans NK3DModeler, `NkRecordingPaint` pour les
//   bancs). Il ne connait donc ni NKGui, ni NKCanvas, ni NKRHI.
//
// ⚠️ C'EST `NkRecordingPaint` QUI REND CE FICHIER PROUVABLE SANS ECRAN
//   Le banc peint dans l'enregistreur et RELIT les commandes : le texte, le
//   role de couleur, le rectangle. Aucune capture, aucune fenetre, aucun GPU.
//
// LES TROIS CHOSES SONT SEPAREES, ET CE FICHIER N'EN FAIT QU'UNE
//   (A) les compteurs (Draw/Tris/FPS) : continus, sous interrupteur -- PAS ICI.
//   (B) les messages : transitoires, declenches par un evenement -- ICI.
//   (C) le journal : persistant, ouvert par l'utilisateur -- PAS ICI
//       (NK3DModeler a deja le sien, `NkModelerJournal.h`).
//   Les confondre a deja coute une fois dans ce depot : une seule garde
//   couvrait le labo, l'aide produit ET le rectangle de selection.
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkScreenLogSink.h"
#include "NKEditorKit/NkTheme.h"
#include "NKEditorKit/Components/NkComponentPaint.h"

namespace nkentseu {
	namespace editorkit {

		/// Nombre de bandeaux VISIBLES a la fois. Au-dela, le plus ancien cede :
		/// c'est le dernier message qui repond au dernier geste.
		inline constexpr uint32 kEcranLogVisibles = 6;

		/// LA DUREE DEPEND DU NIVEAU, et c'est la regle demandee : une erreur
		/// reste plus longtemps qu'une information. Les chiffres sont ici, en un
		/// seul endroit, et nulle part ailleurs.
		inline float32 NkEcranLogDuree(NkLogLevel n) {
			switch (n) {
				case NkLogLevel::NK_TRACE:
				case NkLogLevel::NK_DEBUG:
					return 4.f;
				case NkLogLevel::NK_INFO:
					return 6.f;
				case NkLogLevel::NK_WARN:
					return 12.f;
				case NkLogLevel::NK_ERROR:
					return 20.f;
				default:
					return 30.f; // critical, fatal
			}
		}

		/// LE MOT double la couleur. Un daltonien lit le mot, et une capture en
		/// niveaux de gris reste lisible.
		inline const char *NkEcranLogMot(NkLogLevel n) {
			switch (n) {
				case NkLogLevel::NK_TRACE:
					return "TRACE";
				case NkLogLevel::NK_DEBUG:
					return "DEBUG";
				case NkLogLevel::NK_INFO:
					return "INFO";
				case NkLogLevel::NK_WARN:
					return "AVERTISSEMENT";
				case NkLogLevel::NK_ERROR:
					return "ERREUR";
				case NkLogLevel::NK_CRITICAL:
					return "CRITIQUE";
				default:
					return "FATAL";
			}
		}

		/// LE ROLE, jamais une couleur ecrite en dur : une couleur en dur devient
		/// illisible dans l'autre theme, et ce depot a deja paye ce prix.
		inline uint16 NkEcranLogRole(NkLogLevel n) {
			if (n >= NkLogLevel::NK_ERROR)
				return (uint16)NkRole::StatusErr;
			if (n == NkLogLevel::NK_WARN)
				return (uint16)NkRole::StatusWarn;
			return (uint16)NkRole::AccentUi;
		}

		struct NkEcranLogMsg {
				char texte[kEcranLogTexte] = {};
				NkLogLevel niveau = NkLogLevel::NK_INFO;
				uint32 repetitions = 1;
				float32 restant = 0.f;
		};

		// ── LA VUE ──────────────────────────────────────────────────────────────
		class NkEcranLogVue {
			public:
				/// UNE FOIS PAR IMAGE, avec le VRAI `dt` de la boucle -- jamais un
				/// compteur d'images : a 20 images/s, un message de « six secondes »
				/// compte en images en durerait dix-huit.
				void Tick(float32 dt) {
					Vieillir(dt);
					NkEcranLogSink *puits = NkEcranLogPuits();
					if (!puits)
						return;
					// LE ZERO SE DIT : ce que la rafale a fait tomber du tampon est
					// annonce, sinon l'ecran affirme une completude qu'il n'a pas.
					const uint32 perdus = puits->ReprendrePerdus();
					NkEcranLogEntree lot[kEcranLogMax];
					const uint32 n = puits->Drain(lot, kEcranLogMax);
					for (uint32 i = 0; i < n; ++i)
						Poser(lot[i].niveau, lot[i].texte, lot[i].repetitions);
					if (perdus > 0) {
						char b[64];
						Formater(b, sizeof(b), "messages perdus (rafale)", perdus);
						Poser(NkLogLevel::NK_WARN, b, 1);
					}
				}

				/// Depose un message SANS passer par le journal. Reserve aux hotes
				/// qui ont deja un message tout fait ; le chemin normal est
				/// `logger.Error(...)`, et c'est celui que le critere eprouve.
				void Poser(NkLogLevel niveau, const char *texte, uint32 repetitions = 1) {
					if (!texte)
						texte = "";
					// Fusion avec un bandeau DEJA VISIBLE : le compteur monte et la
					// duree repart. Sans cela, un message repete une fois par image
					// remplirait la pile de copies de lui-meme.
					for (uint32 i = 0; i < mCount; ++i) {
						if (mItems[i].niveau == niveau && Egal(mItems[i].texte, texte)) {
							mItems[i].repetitions += repetitions;
							mItems[i].restant = NkEcranLogDuree(niveau);
							return;
						}
					}
					if (mCount >= kEcranLogVisibles) {
						for (uint32 i = 1; i < kEcranLogVisibles; ++i)
							mItems[i - 1] = mItems[i];
						--mCount;
					}
					NkEcranLogMsg &m = mItems[mCount++];
					uint32 k = 0;
					for (; texte[k] && k + 1 < kEcranLogTexte; ++k)
						m.texte[k] = texte[k];
					m.texte[k] = '\0';
					m.niveau = niveau;
					m.repetitions = repetitions;
					m.restant = NkEcranLogDuree(niveau);
				}

				void Vider() {
					mCount = 0;
				}
				uint32 Count() const {
					return mCount;
				}
				const NkEcranLogMsg &Item(uint32 i) const {
					return mItems[i];
				}

				/// Peint la pile EN BAS AU CENTRE, dans le rectangle `zone`.
				/// Rend le rectangle occupe (vide si rien), pour que l'appelant en
				/// interdise l'entree a ce qui est dessous.
				///
				/// ⚠️ L'APPELANT DOIT PEINDRE EN DERNIER (couche d'incrustation).
				///    Un panneau peint apres recouvrirait le message : il
				///    existerait sans se voir, c'est-a-dire le defaut qu'on repare.
				NkPaintRect Peindre(NkComponentPaint &p, const NkPaintRect &zone) {
					if (mCount == 0)
						return {0.f, 0.f, 0.f, 0.f};
					const float32 lh = p.LineHeight();
					const float32 pad = 8.f, gap = 6.f, bande = 4.f, ecart = 10.f;
					const float32 rowH = lh + pad * 2.f;

					float32 largeur = zone.w * 0.60f;
					const float32 mini = 380.f, maxi = zone.w - 24.f;
					if (largeur < mini)
						largeur = mini;
					if (largeur > maxi)
						largeur = maxi;
					if (largeur < 1.f)
						return {0.f, 0.f, 0.f, 0.f};

					const float32 x = zone.x + (zone.w - largeur) * 0.5f;
					const float32 total = (float32)mCount * (rowH + gap) - gap;
					float32 y = zone.y + zone.h - ecart - total;
					if (y < zone.y + 4.f)
						y = zone.y + 4.f;
					const NkPaintRect emprise{x, y, largeur, total};

					for (uint32 i = 0; i < mCount; ++i) {
						const NkEcranLogMsg &m = mItems[i];
						const uint16 accent = NkEcranLogRole(m.niveau);
						const NkPaintRect r{x, y, largeur, rowH};
						p.Fill(r, (uint16)NkRole::PanelHeader, 6.f);
						p.OutlineSharp(r, accent);
						// La bande de gauche redit la severite SANS dependre de la
						// lecture.
						p.Fill({r.x, r.y, bande, r.h}, accent, 0.f);

						const char *mot = NkEcranLogMot(m.niveau);
						const float32 motW = p.TextWidth(mot) + 12.f;
						const float32 tx = r.x + bande + pad;
						p.Text({tx, r.y + pad, motW, lh}, mot, accent, NkTextAlign::Left);

						// Le compteur de repetitions, a DROITE : « x 12 » plutot que
						// douze lignes. Il n'apparait qu'a partir de deux -- « x 1 »
						// serait du bruit sur chaque message.
						float32 finTexte = r.x + r.w - pad;
						if (m.repetitions > 1) {
							char c[24];
							Formater(c, sizeof(c), "x", m.repetitions);
							const float32 cw = p.TextWidth(c) + 8.f;
							p.Text({r.x + r.w - pad - cw, r.y + pad, cw, lh}, c, accent,
								   NkTextAlign::Right);
							finTexte -= cw;
						}
						const float32 dispo = finTexte - (tx + motW);
						if (dispo > 8.f) {
							p.PushClip({tx + motW, r.y, dispo, r.h});
							p.Text({tx + motW, r.y + pad, dispo, lh}, m.texte,
								   (uint16)NkRole::Text, NkTextAlign::Left);
							p.PopClip();
						}
						y += rowH + gap;
					}
					return emprise;
				}

			private:
				void Vieillir(float32 dt) {
					uint32 k = 0;
					for (uint32 i = 0; i < mCount; ++i) {
						mItems[i].restant -= dt;
						if (mItems[i].restant <= 0.f)
							continue;
						if (k != i)
							mItems[k] = mItems[i];
						++k;
					}
					mCount = k;
				}

				static bool Egal(const char *a, const char *b) {
					for (; *a && *b; ++a, ++b)
						if (*a != *b)
							return false;
					return *a == *b;
				}

				/// Un « %s %u » sans <cstdio> : le depot ecrit son propre formatage
				/// (cf. NkFormat), et une dependance a la libc ici serait gratuite.
				static void Formater(char *dst, uint32 cap, const char *mot, uint32 n) {
					uint32 k = 0;
					if (mot) {
						// L'ordre du depot : « x 12 » pour le compteur, « 12 messages
						// perdus » pour la perte. Le mot decide ou va le nombre : un
						// mot d'UNE lettre est un prefixe, un mot long un suffixe.
						const bool prefixe = (mot[0] != '\0' && mot[1] == '\0');
						if (prefixe) {
							for (uint32 i = 0; mot[i] && k + 1 < cap; ++i)
								dst[k++] = mot[i];
							if (k + 1 < cap)
								dst[k++] = ' ';
							k += Chiffres(dst + k, cap - k, n);
						} else {
							k += Chiffres(dst + k, cap - k, n);
							if (k + 1 < cap)
								dst[k++] = ' ';
							for (uint32 i = 0; mot[i] && k + 1 < cap; ++i)
								dst[k++] = mot[i];
						}
					}
					dst[k < cap ? k : cap - 1] = '\0';
				}

				static uint32 Chiffres(char *dst, uint32 cap, uint32 n) {
					char tmp[12];
					uint32 t = 0;
					if (n == 0)
						tmp[t++] = '0';
					while (n > 0 && t < 12) {
						tmp[t++] = (char)('0' + (n % 10u));
						n /= 10u;
					}
					uint32 k = 0;
					while (t > 0 && k + 1 < cap)
						dst[k++] = tmp[--t];
					return k;
				}

				NkEcranLogMsg mItems[kEcranLogVisibles];
				uint32 mCount = 0;
		};

		/// LA vue de l'application. Un hote qui en veut une a lui peut construire
		/// son propre `NkEcranLogVue` ; celle-ci evite d'avoir a la faire voyager
		/// jusqu'au fond de la peinture.
		inline NkEcranLogVue &NkEcranLog() {
			static NkEcranLogVue vue;
			return vue;
		}

	} // namespace editorkit
} // namespace nkentseu
