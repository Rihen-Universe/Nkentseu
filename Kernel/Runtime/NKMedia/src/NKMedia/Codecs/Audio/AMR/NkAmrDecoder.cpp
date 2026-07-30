// =============================================================================
// NKMedia/Codecs/Audio/AMR/NkAmrDecoder.cpp
// -----------------------------------------------------------------------------
// Décodeur AMR-NB from-scratch — implémentation flottante écrite depuis le
// TEXTE de 3GPP TS 26.090 v18.0.0 (équations citées en commentaire) + format
// de trame 3GPP TS 26.101 v18.0.0. Tables normatives : NkAmrTables.inc
// (extraction mécanique des DONNÉES du standard — voir l'en-tête du .inc).
// Aucun fichier .c d'algorithme (3GPP TS 26.073 / opencore / ffmpeg) n'a été
// lu : seuls les *.tab (données) et les spécifications textuelles.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#include "NkAmrDecoder.h"

#include "NKMath/NkFunctions.h"

namespace nkentseu {
	namespace media {

		namespace {
#include "NkAmrTables.inc"

			// Nombre de bits par mode (TS 26.090 table 1 / TS 26.101 table 2).
			const int32 kBitsPerMode[8] = {95, 103, 118, 134, 148, 159, 204, 244};
			// Tables d'ordonnancement d(j) -> s (TS 26.101 Annexe B, 0-based).
			const nk_int16 *kOrderTabs[8] = {kOrder475, kOrder515, kOrder59, kOrder67,
											 kOrder74, kOrder795, kOrder102, kOrder122};
			// Énergie moyenne d'innovation Ē (dB) par mode (TS 26.090 §5.8.2).
			const float64 kMeanEnergy[8] = {33.0, 33.0, 33.0, 28.75, 30.0, 36.0, 33.0, 36.0};
			// Coefficients MA du prédicteur d'énergie (TS 26.090 eq. 54).
			const float64 kPredB[4] = {0.68, 0.58, 0.34, 0.19};

			// Offsets (en tiers d'échantillon) du lag relatif 4 bits autour de T1
			// (TS 26.090 §5.6.1 modes 6.70/5.90/5.15/4.75 : entiers [T1-5, T1+4]
			// + fractions 1/3 dans [T1-1 2/3, T1+2/3]), ordre croissant.
			const int32 kRel4Thirds[16] = {-15, -12, -9, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 6, 9, 12};

			// Filtres d'interpolation de l'excitation passée (TS 26.090 §5.6.1 /
			// §6.1) : sinc fenêtré Hamming, coupure ~3600 Hz dans le domaine
			// suréchantillonné. b60 (résolution 1/6, ±59) pour le mode 12.2,
			// b30 (résolution 1/3, ±29) pour les autres. Les valeurs numériques
			// exactes de la référence fixed-point ne figurant pas dans les
			// données du standard, elles sont RECALCULÉES ici depuis leur
			// définition textuelle (écart non bit-exact assumé, voir rapport).
			float64 gB60[61];
			float64 gB30[31];
			bool gInterpInit = false;

			float64 Sinc(float64 x) {
				if (x > -1e-9 && x < 1e-9)
					return 1.0;
				const float64 px = 3.14159265358979323846 * x;
				return math::NkSin(px) / px;
			}
			void InitInterpFilters() {
				if (gInterpInit)
					return;
				for (int32 j = 0; j <= 60; ++j)
					gB60[j] = 0.9 * Sinc(0.9 * (float64)j / 6.0) *
							  (0.54 + 0.46 * math::NkCos(3.14159265358979323846 * (float64)j / 60.0));
				gB60[60] = 0.0;
				for (int32 j = 0; j <= 30; ++j)
					gB30[j] = 0.9 * Sinc(0.9 * (float64)j / 3.0) *
							  (0.54 + 0.46 * math::NkCos(3.14159265358979323846 * (float64)j / 30.0));
				gB30[30] = 0.0;
				gInterpInit = true;
			}

			// Lecteur séquentiel MSB-first sur le tableau de bits s(i) dé-ordonné.
			struct NkSBits {
					const nk_uint8 *s;
					int32 pos;
					int32 Get(int32 n) {
						int32 v = 0;
						for (int32 i = 0; i < n; ++i)
							v = (v << 1) | s[pos++];
						return v;
					}
			};

			// Paramètres décodés d'un sous-trame.
			struct NkSfPrm {
					int32 acb = 0;		 // index du codebook adaptatif
					int32 subset = 0;	 // MR475/515 : bit de sous-ensemble
					int32 pos[10] = {0}; // index de position bruts par impulsion
					int32 sign[5] = {0}; // bits de signe
					int32 gains = 0;	 // index VQ des gains (modes vectoriels)
					int32 gpIdx = 0;	 // gain adaptatif scalaire (12.2/7.95)
					int32 gcIdx = 0;	 // correction gain fixe scalaire (12.2/7.95)
			};


			float64 Median5(const float64 *v) {
				float64 t[5];
				for (int32 i = 0; i < 5; ++i)
					t[i] = v[i];
				for (int32 i = 0; i < 4; ++i)
					for (int32 j = i + 1; j < 5; ++j)
						if (t[j] < t[i]) {
							const float64 x = t[i];
							t[i] = t[j];
							t[j] = x;
						}
				return t[2];
			}

		} // namespace

		// =====================================================================
		// Init
		// =====================================================================
		void NkAmrDecoder::Init() {
			InitInterpFilters();
			for (int32 i = 0; i < kExcHistory + kFrameSamples; ++i)
				mExc[i] = 0.0;
			for (int32 i = 0; i < kLpcOrder; ++i) {
				mLspOld[i] = (float64)kLspInit[i] / 32768.0;
				mPastR[i] = 0.0;
				mPastR2[i] = 0.0;
				mLspAvg[i] = (float64)kLspInit[i] / 32768.0;
				mSynMem[i] = 0.0;
				mPfResMem[i] = 0.0;
				mPfSynMem[i] = 0.0;
			}
			for (int32 i = 0; i < 4; ++i)
				mEnerHist[i] = -14.0; // état de repos du prédicteur (silence)
			mPrevGp = 0.0;
			mPrevOddGp = 0.0;
			mPrevGc = 0.0;
			for (int32 i = 0; i < 5; ++i) {
				mGpHist[i] = 0.0;
				mGcHist[i] = 0.0;
			}
			mPrevImpNr = 2;
			mDiffCount = 0;
			mNoSmoothHangover = 0;
			mPrevT1 = 40;
			mPfTiltMem = 0.0;
			mPfAgcGain = 1.0;
			mHpX[0] = mHpX[1] = 0.0;
			mHpY[0] = mHpY[1] = 0.0;
		}

		int32 NkAmrDecoder::PayloadBytes(int32 ft) {
			// RFC 4867 §3.6 (tailles totales moins l'octet d'en-tête).
			static const int32 kSizes[16] = {12, 13, 15, 17, 19, 20, 26, 31, 5, 6, 5, 5, 0, 0, 0, 0};
			if (ft < 0 || ft > 15)
				return -1;
			return kSizes[ft];
		}

		// =====================================================================
		// LSF -> LSP -> LPC
		// =====================================================================
		namespace {
			// Ordonne les LSF (unités normalisées : 32768 = 8000 Hz) avec un
			// écart minimal (stabilité du filtre — exigence implicite §5.2.4).
			void ReorderLsf(float64 *f) {
				const float64 kGap = 20.0; // ~5 Hz : n'intervient qu'en cas de
										   // violation d'ordre (stabilité)
				float64 prev = kGap;
				for (int32 i = 0; i < 10; ++i) {
					if (f[i] < prev)
						f[i] = prev;
					prev = f[i] + kGap;
				}
			}
			void LsfToLsp(const float64 *f, float64 *q) {
				for (int32 i = 0; i < 10; ++i)
					q[i] = math::NkCos(3.14159265358979323846 * f[i] / 16384.0);
			}
		} // namespace

		void NkAmrDecoder::DecodeLsf(int32 mode, const int32 *idx, int32 thirdSign, float64 lspSub[4][kLpcOrder]) {
			float64 lspNew[kLpcOrder];
			float64 lspMid[kLpcOrder]; // 12.2 : 2e jeu (sous-trame 2)
			if (mode == NK_AMR_MR122) {
				// SMQ 5 sous-matrices 2x2 (TS 26.090 §5.2.5 12.2) ;
				// prédiction MA p(n) = 0.65 r̂2(n-1) (eq. 21-22).
				const nk_int16 *dico[5] = {kDico1Lsf5, kDico2Lsf5, kDico3Lsf5, kDico4Lsf5, kDico5Lsf5};
				float64 f1[10], f2[10];
				for (int32 sm = 0; sm < 5; ++sm) {
					const nk_int16 *e = dico[sm] + idx[sm] * 4;
					// Ordre de stockage (etabli par recoupement sur flux reel :
					// desordre nul + paire a 440 Hz) :
					// r1(2i), r1(2i+1), r2(2i), r2(2i+1).
					float64 r1a = e[0], r1b = e[1], r2a = e[2], r2b = e[3];
					if (sm == 2 && thirdSign) { // codebook signé (8 bits + signe)
						r1a = -r1a;
						r2a = -r2a;
						r1b = -r1b;
						r2b = -r2b;
					}
					const int32 j = sm * 2;
					const float64 pa = 0.65 * mPastR2[j];
					const float64 pb = 0.65 * mPastR2[j + 1];
					f1[j] = (float64)kMeanLsf5[j] + pa + r1a;
					f1[j + 1] = (float64)kMeanLsf5[j + 1] + pb + r1b;
					f2[j] = (float64)kMeanLsf5[j] + pa + r2a;
					f2[j + 1] = (float64)kMeanLsf5[j + 1] + pb + r2b;
					mPastR2[j] = r2a;
					mPastR2[j + 1] = r2b;
				}
				ReorderLsf(f1);
				ReorderLsf(f2);
				LsfToLsp(f1, lspMid);
				LsfToLsp(f2, lspNew);
				// Interpolation eq. (27) : sous-trames 2 et 4 = jeux décodés,
				// 1 et 3 = moyennes.
				for (int32 i = 0; i < kLpcOrder; ++i) {
					lspSub[0][i] = 0.5 * (mLspOld[i] + lspMid[i]);
					lspSub[1][i] = lspMid[i];
					lspSub[2][i] = 0.5 * (lspMid[i] + lspNew[i]);
					lspSub[3][i] = lspNew[i];
					mLspOld[i] = lspNew[i];
				}
			} else {
				// Split-VQ 3+3+4 (TS 26.090 §5.2.5 autres modes) ;
				// p_j = pred_fac_j * r̂_j(n-1) (eq. 25-26).
				float64 r[10];
				const bool lowLsf = (mode == NK_AMR_MR475 || mode == NK_AMR_MR515);
				const nk_int16 *d1 = (mode == NK_AMR_MR795) ? (kMr795Dico1 + idx[0] * 3) : (kDico1Lsf3 + idx[0] * 3);
				// Sous-vecteur 2 : 9 bits (512 entrées) sauf 4.75/5.15 : 8 bits
				// -> une entrée sur deux du même dictionnaire.
				const nk_int16 *d2 = kDico2Lsf3 + (lowLsf ? (idx[1] * 2) : idx[1]) * 3;
				const nk_int16 *d3 = lowLsf ? (kMr515Dico3 + idx[2] * 4) : (kDico3Lsf3 + idx[2] * 4);
				r[0] = d1[0];
				r[1] = d1[1];
				r[2] = d1[2];
				r[3] = d2[0];
				r[4] = d2[1];
				r[5] = d2[2];
				r[6] = d3[0];
				r[7] = d3[1];
				r[8] = d3[2];
				r[9] = d3[3];
				float64 f[10];
				for (int32 j = 0; j < 10; ++j) {
					const float64 p = ((float64)kPredFac3[j] / 32768.0) * mPastR[j];
					f[j] = (float64)kMeanLsf3[j] + p + r[j];
					mPastR[j] = r[j];
				}
				ReorderLsf(f);
				LsfToLsp(f, lspNew);
				// Interpolation eq. (28).
				for (int32 i = 0; i < kLpcOrder; ++i) {
					lspSub[0][i] = 0.75 * mLspOld[i] + 0.25 * lspNew[i];
					lspSub[1][i] = 0.5 * mLspOld[i] + 0.5 * lspNew[i];
					lspSub[2][i] = 0.25 * mLspOld[i] + 0.75 * lspNew[i];
					lspSub[3][i] = lspNew[i];
					mLspOld[i] = lspNew[i];
				}
			}
		}

		// LSP (domaine cosinus, ordonnés) -> coefficients LP (TS 26.090 §5.2.4,
		// eq. 14-19 ; convention A(z) = 1 + somme a_i z^-i).
		void NkAmrDecoder::LspToLpc(const float64 *lsp, float64 *a) {
			float64 f1[6], f2[6];
			f1[0] = 1.0;
			f2[0] = 1.0;
			f1[1] = 0.0; // f(i-2) virtuel géré par indices décalés
			f2[1] = 0.0;
			// Récurrence du §5.2.4 avec f(0)=1, f(-1)=0.
			float64 w1[7], w2[7]; // w[k] = f(k-1) : w[0] = f(-1)
			w1[0] = 0.0;
			w1[1] = 1.0;
			w2[0] = 0.0;
			w2[1] = 1.0;
			int32 n1 = 2, n2 = 2;
			for (int32 i = 1; i <= 5; ++i) {
				const float64 q1 = lsp[2 * i - 2]; // q(2i-1) : LSP impairs (0-based 0,2,4,6,8)
				const float64 q2 = lsp[2 * i - 1]; // q(2i)   : LSP pairs
				// f1(i) = -2 q1 f1(i-1) + 2 f1(i-2)
				w1[n1] = -2.0 * q1 * w1[n1 - 1] + 2.0 * w1[n1 - 2];
				w2[n2] = -2.0 * q2 * w2[n2 - 1] + 2.0 * w2[n2 - 2];
				for (int32 j = n1 - 1; j >= 2; --j) {
					w1[j] += -2.0 * q1 * w1[j - 1] + w1[j - 2];
					w2[j] += -2.0 * q2 * w2[j - 1] + w2[j - 2];
				}
				++n1;
				++n2;
			}
			// w[k] = f(k-1) : f(i) = w[i+1], i = 0..5
			float64 F1[6], F2[6];
			for (int32 i = 0; i <= 5; ++i) {
				F1[i] = w1[i + 1];
				F2[i] = w2[i + 1];
			}
			// eq. (18) : multiplication par (1 + z^-1) et (1 - z^-1).
			float64 F1p[6], F2p[6];
			F1p[0] = 1.0;
			F2p[0] = 1.0;
			for (int32 i = 1; i <= 5; ++i) {
				F1p[i] = F1[i] + F1[i - 1];
				F2p[i] = F2[i] - F2[i - 1];
			}
			// eq. (19).
			a[0] = 1.0;
			for (int32 i = 1; i <= 5; ++i) {
				a[i] = 0.5 * (F1p[i] + F2p[i]);
				a[11 - i] = 0.5 * (F1p[i] - F2p[i]);
			}
		}

		// =====================================================================
		// Codebook adaptatif : décodage du lag (TS 26.090 §5.6.1 par mode).
		// =====================================================================
		void NkAmrDecoder::DecodeAdaptive(int32 mode, int32 sf, int32 index, float64 &lagOut, int32 &t0Out) {
			const bool odd = (sf == 0 || sf == 2);
			if (mode == NK_AMR_MR122) {
				if (odd) {
					// 9 bits : fractionnaire 1/6 sur [17 3/6, 94 3/6] puis
					// entiers [95, 143].
					if (index < 463) {
						const int32 sixths = index + 105; // 6*17+3 = 105
						lagOut = (float64)sixths / 6.0;
					} else {
						lagOut = (float64)(index - 368);
					}
				} else {
					// 6 bits relatifs : fenêtre [T1-5 3/6, T1+4 3/6] pas 1/6,
					// recalée dans [18, 143].
					int32 tmin = mPrevT1 - 5;
					if (tmin < 18)
						tmin = 18;
					if (tmin > 134)
						tmin = 134;
					lagOut = (float64)tmin - 0.5 + (float64)index / 6.0;
				}
			} else if (mode == NK_AMR_MR475 || mode == NK_AMR_MR515) {
				if (sf == 0) {
					if (index < 197)
						lagOut = (float64)(index + 58) / 3.0; // [19 1/3, 84 2/3]
					else
						lagOut = (float64)(index - 112); // [85, 143]
				} else {
					// 4 bits relatifs au sous-trame précédent.
					int32 t1 = mPrevT1;
					if (t1 < 25)
						t1 = 25; // fenêtre entière [T1-5, T1+4] dans [20, 143]
					if (t1 > 139)
						t1 = 139;
					lagOut = (float64)t1 + (float64)kRel4Thirds[index] / 3.0;
				}
			} else if (odd) {
				// 8 bits absolus (tous les autres modes).
				if (index < 197)
					lagOut = (float64)(index + 58) / 3.0;
				else
					lagOut = (float64)(index - 112);
			} else if (mode == NK_AMR_MR795) {
				// 6 bits relatifs : [T1-10 2/3, T1+9 2/3] pas 1/3.
				int32 tmin = mPrevT1 - 10;
				if (tmin < 20)
					tmin = 20;
				if (tmin > 124)
					tmin = 124;
				lagOut = (float64)tmin - 2.0 / 3.0 + (float64)index / 3.0;
			} else if (mode == NK_AMR_MR74 || mode == NK_AMR_MR102) {
				// 5 bits relatifs : [T1-5 2/3, T1+4 2/3] pas 1/3.
				int32 tmin = mPrevT1 - 5;
				if (tmin < 20)
					tmin = 20;
				if (tmin > 134)
					tmin = 134;
				lagOut = (float64)tmin - 2.0 / 3.0 + (float64)index / 3.0;
			} else {
				// MR67 / MR59 : 4 bits relatifs (entiers + fractions).
				int32 t1 = mPrevT1;
				if (t1 < 25)
					t1 = 25;
				if (t1 > 139)
					t1 = 139;
				lagOut = (float64)t1 + (float64)kRel4Thirds[index] / 3.0;
			}
			if (lagOut < 17.5)
				lagOut = 17.5;
			if (lagOut > 143.0)
				lagOut = 143.0;
			t0Out = (int32)math::NkFloor(lagOut + 0.5); // T entier le plus proche
			// Mise à jour de T1 : sous-trames impairs (1er/3e) pour la plupart
			// des modes ; chaque sous-trame pour 4.75/5.15.
			if (mode == NK_AMR_MR475 || mode == NK_AMR_MR515 || odd)
				mPrevT1 = t0Out;
		}

		// v(n) par interpolation de l'excitation passée (TS 26.090 eq. 40),
		// écrit EN PLACE dans mExc (répétition de pitch pour lag < 40).
		void NkAmrDecoder::BuildAdaptiveVector(float64 lag, float64 *v) {
			float64 *cur = mExc + kExcHistory; // début du sous-trame courant (déjà décalé)
			// L'appelant fournit un lag quantifié en sixièmes (12.2) ou en
			// tiers (autres modes). L'eq. (40) échantillonne u au retard
			// k - t/6 (noyau symétrique autour de k - t/6) : on convertit
			// lag = k0 + f/6 en (k = k0 + 1, t = 6 - f) pour f > 0.
			int32 k = (int32)math::NkFloor(lag);
			int32 t6 = (int32)math::NkFloor((lag - (float64)k) * 6.0 + 0.5);
			if (t6 == 6) {
				++k;
				t6 = 0;
			}
			if (t6 > 0) {
				++k;
				t6 = 6 - t6;
			}
			// Utilise b60 si la phase est impaire en sixièmes (résolution 1/6),
			// sinon b30 avec la phase en tiers. (b60 aux phases paires ==
			// définition 1/3 : les deux formulations sont équivalentes ici.)
			if (t6 % 2 == 0) {
				const int32 t3 = t6 / 2;
				for (int32 n = 0; n < kSubframeSamples; ++n) {
					float64 s = 0.0;
					for (int32 i = 0; i < 10; ++i) {
						s += cur[n - k - i] * gB30[t3 + i * 3];
						s += cur[n - k + 1 + i] * gB30[3 - t3 + i * 3];
					}
					v[n] = s;
					cur[n] = s;
				}
			} else {
				for (int32 n = 0; n < kSubframeSamples; ++n) {
					float64 s = 0.0;
					for (int32 i = 0; i < 10; ++i) {
						s += cur[n - k - i] * gB60[t6 + i * 6];
						s += cur[n - k + 1 + i] * gB60[6 - t6 + i * 6];
					}
					v[n] = s;
					cur[n] = s;
				}
			}
		}

		// =====================================================================
		// Codebook algébrique (TS 26.090 §5.7.1 tables 3-8, §7 tables 9a-9h).
		// =====================================================================
		void NkAmrDecoder::DecodeAlgebraic(int32 mode, int32 sf, const int32 *pos, const int32 *sign, int32 subset,
										   float64 *c) {
			for (int32 n = 0; n < kSubframeSamples; ++n)
				c[n] = 0.0;
			// Convention de signe (calibrée contre le décodeur de référence en
			// boîte noire) : 12.2 et 10.2 -> bit 0 = +1 ; autres -> bit 1 = +1.
			const float64 kSignMap122[2] = {1.0, -1.0};
			const float64 kSignMapStd[2] = {-1.0, 1.0};
			const float64 *kSignMap =
				(mode == NK_AMR_MR122 || mode == NK_AMR_MR102) ? kSignMap122 : kSignMapStd;
			switch (mode) {
				case NK_AMR_MR122: {
					// 10 impulsions, 5 pistes x 2, positions Gray-codées 3 bits,
					// signe du 1er pulse par piste ; le 2e prend le signe opposé
					// si sa position est plus petite (table 3 + §5.7.1).
					for (int32 t = 0; t < 5; ++t) {
						const int32 p1 = kDGray[pos[t]];
						const int32 p2 = kDGray[pos[t + 5]];
						const float64 s1 = kSignMap[sign[t]];
						const float64 s2 = (p2 < p1) ? -s1 : s1;
						c[p1 * 5 + t] += s1;
						c[p2 * 5 + t] += s2;
					}
					break;
				}
				case NK_AMR_MR102: {
					// 8 impulsions, 4 pistes x 2 (table 4), positions 0..9 dans
					// la piste (pos = piste + 4*k). Mots 10 bits pour les
					// groupes {i0,i1,i4} et {i2,i5,i6} : (base5 << 3) | parités,
					// k = 2*q + h ; chiffres base 5 dans l'ordre (pulse 1 de la
					// piste double, pulse 2 de la piste double, pulse isolé),
					// parités idem — établi par sondes boîte noire du décodeur
					// de référence (TS 26.090 ne donne que les largeurs).
					const int32 wA = pos[0], wB = pos[1], wC = pos[2];
					int32 p[8];
					const int32 bA = wA >> 3, hA = wA & 7;
					const int32 bB = wB >> 3, hB = wB & 7;
					p[0] = 2 * (bA % 5) + (hA & 1);		   // i0 (piste 0)
					p[4] = 2 * ((bA / 5) % 5) + ((hA >> 1) & 1); // i4 (piste 0)
					p[1] = 2 * ((bA / 25) % 5) + ((hA >> 2) & 1); // i1 (piste 1)
					p[2] = 2 * (bB % 5) + (hB & 1);		   // i2 (piste 2)
					p[6] = 2 * ((bB / 5) % 5) + ((hB >> 1) & 1); // i6 (piste 2)
					p[5] = 2 * ((bB / 25) % 5) + ((hB >> 2) & 1); // i5 (piste 1)
					// Mot 7 bits {i3,i7} : division arrondie 25/32 puis
					// parcours en serpentin de la grille 5x5 des demi-positions
					// (identifié en boîte noire, cohérent sur les 128 sondes).
					const int32 bC = wC >> 2, hC = wC & 3;
					const int32 pC = (25 * bC + 12) >> 5;
					const int32 kh7 = pC / 5;
					const int32 kh3 = (kh7 & 1) ? (4 - pC % 5) : (pC % 5);
					p[3] = 2 * kh3 + (hC & 1);			   // i3 (piste 3)
					p[7] = 2 * kh7 + ((hC >> 1) & 1);	   // i7 (piste 3)
					static const int32 kTrack[8] = {0, 1, 2, 3, 0, 1, 2, 3};
					for (int32 t = 0; t < 4; ++t) {
						const int32 first = t, second = t + 4;
						const float64 s1 = kSignMap[sign[t]];
						const float64 s2 = (p[second] < p[first]) ? -s1 : s1;
						c[p[first] * 4 + kTrack[first]] += s1;
						c[p[second] * 4 + kTrack[second]] += s2;
					}
					break;
				}
				case NK_AMR_MR795:
				case NK_AMR_MR74: {
					// 4 impulsions (table 5) : pistes 0-2 (3 bits, positions
					// GRAY-codées) + piste 3/4 combinée (4 bits : LSB =
					// sous-piste, pas de piste GRAY-codé) — sondes boîte noire.
					for (int32 t = 0; t < 3; ++t)
						c[kDGray[pos[t]] * 5 + t] += kSignMap[sign[t]];
					const int32 b = pos[3];
					c[kDGray[b >> 1] * 5 + 3 + (b & 1)] += kSignMap[sign[3]];
					break;
				}
				case NK_AMR_MR67: {
					// 3 impulsions (table 6), positions en binaire naturel :
					// i0 piste 0 (3 bits) ; i1 pistes {1,3} (4 bits, LSB =
					// piste) ; i2 pistes {2,4} (idem) — sondes boîte noire.
					c[pos[0] * 5] += kSignMap[sign[0]];
					const int32 b1 = pos[1];
					c[(b1 >> 1) * 5 + ((b1 & 1) ? 3 : 1)] += kSignMap[sign[1]];
					const int32 b2 = pos[2];
					c[(b2 >> 1) * 5 + ((b2 & 1) ? 4 : 2)] += kSignMap[sign[2]];
					break;
				}
				case NK_AMR_MR59: {
					// 2 impulsions (table 7) : i0 pistes {1,3} (4 bits, LSB =
					// piste via startPos1) ; i1 pistes {0,1,2,4} (5 bits, 2 LSB
					// = piste via startPos2) — sondes boîte noire.
					const int32 b0 = pos[0];
					c[(b0 >> 1) * 5 + kStartPos59A[b0 & 1]] += kSignMap[sign[0]];
					const int32 b1 = pos[1];
					c[(b1 >> 2) * 5 + kStartPos59B[b1 & 3]] += kSignMap[sign[1]];
					break;
				}
				default: {
					// MR475 / MR515 : 2 impulsions, sous-ensembles de pistes
					// par sous-trame (table 8, données c2_9pf.tab).
					const int32 tr0 = kStartPos475515[subset * 8 + sf * 2];
					const int32 tr1 = kStartPos475515[subset * 8 + sf * 2 + 1];
					c[pos[0] * 5 + tr0] += kSignMap[sign[0]];
					c[pos[1] * 5 + tr1] += kSignMap[sign[1]];
					break;
				}
			}
		}

		// =====================================================================
		// Post-filtre adaptatif (TS 26.090 §6.2.1) + AGC.
		// =====================================================================
		void NkAmrDecoder::Postfilter(int32 mode, const float64 *a, const float64 *syn, float64 *out) {
			const bool hi = (mode == NK_AMR_MR122 || mode == NK_AMR_MR102);
			const float64 gn = hi ? 0.7 : 0.55;
			const float64 gd = hi ? 0.75 : 0.7;
			float64 an[11], ad[11];
			float64 pn = 1.0, pd = 1.0;
			an[0] = 1.0;
			ad[0] = 1.0;
			for (int32 i = 1; i <= 10; ++i) {
				pn *= gn;
				pd *= gd;
				an[i] = a[i] * pn;
				ad[i] = a[i] * pd;
			}
			// Réponse impulsionnelle tronquée (Lh = 22) de Â(z/γn)/Â(z/γd)
			// pour le facteur de tilt k1' (eq. 82).
			float64 hf[22];
			for (int32 n = 0; n < 22; ++n) {
				float64 s = (n == 0) ? 1.0 : 0.0;
				if (n >= 1 && n <= 10)
					s += an[n];
				for (int32 i = 1; i <= 10 && i <= n; ++i)
					s -= ad[i] * hf[n - i];
				hf[n] = s;
			}
			float64 rh0 = 0.0, rh1 = 0.0;
			for (int32 j = 0; j < 22; ++j)
				rh0 += hf[j] * hf[j];
			for (int32 j = 0; j < 21; ++j)
				rh1 += hf[j] * hf[j + 1];
			const float64 k1 = (rh0 > 0.0) ? (rh1 / rh0) : 0.0;
			float64 mu;
			if (hi)
				mu = (k1 > 0.0) ? 0.8 * k1 : 0.0; // eq. (86)
			else
				mu = 0.8 * k1;
			// Filtrage : r̂ = Â(z/γn){ŝ} puis 1/Â(z/γd) puis tilt 1 - µz^-1.
			float64 x[kSubframeSamples];
			for (int32 n = 0; n < kSubframeSamples; ++n) {
				float64 r = syn[n];
				for (int32 i = 1; i <= 10; ++i) {
					const float64 past = (n - i >= 0) ? syn[n - i] : mPfResMem[10 + (n - i)];
					r += an[i] * past;
				}
				float64 v = r;
				for (int32 i = 1; i <= 10; ++i) {
					const float64 past = (n - i >= 0) ? x[n - i] : mPfSynMem[10 + (n - i)];
					v -= ad[i] * past;
				}
				x[n] = v;
			}
			float64 y[kSubframeSamples];
			for (int32 n = 0; n < kSubframeSamples; ++n) {
				const float64 prev = (n == 0) ? mPfTiltMem : x[n - 1];
				y[n] = x[n] - mu * prev;
			}
			// Mémoires.
			for (int32 i = 0; i < 10; ++i) {
				mPfResMem[i] = syn[kSubframeSamples - 10 + i];
				mPfSynMem[i] = x[kSubframeSamples - 10 + i];
			}
			mPfTiltMem = x[kSubframeSamples - 1];
			// AGC (eq. 83-85) : facteur α = 0.9, échantillon par échantillon.
			float64 eIn = 0.0, eOut = 0.0;
			for (int32 n = 0; n < kSubframeSamples; ++n) {
				eIn += syn[n] * syn[n];
				eOut += y[n] * y[n];
			}
			const float64 gsc = (eOut > 0.0) ? math::NkSqrt(eIn / eOut) : 1.0;
			for (int32 n = 0; n < kSubframeSamples; ++n) {
				mPfAgcGain = 0.9 * mPfAgcGain + 0.1 * gsc;
				out[n] = y[n] * mPfAgcGain;
			}
		}

		// =====================================================================
		// Décodage d'une trame complète.
		// =====================================================================
		bool NkAmrDecoder::DecodeFrame(int32 frameType, const nk_uint8 *payload, nk_int16 *out) {
			if (frameType == NK_AMR_SID || frameType == NK_AMR_NO_DATA || (frameType > 8 && frameType < 15)) {
				// DTX : pas de génération de bruit de confort (TS 26.092 non
				// implémentée) -> silence.
				for (int32 i = 0; i < kFrameSamples; ++i)
					out[i] = 0;
				return true;
			}
			if (frameType < 0 || frameType > 7)
				return false;
			const int32 mode = frameType;
			const int32 K = kBitsPerMode[mode];

			// --- Dé-ordonnancement TS 26.101 §4.2.1 : s(table(j)) = d(j). ---
			nk_uint8 s[244];
			const nk_int16 *ord = kOrderTabs[mode];
			for (int32 j = 0; j < K; ++j) {
				const int32 bit = (payload[j >> 3] >> (7 - (j & 7))) & 1;
				s[ord[j]] = (nk_uint8)bit;
			}
			NkSBits br;
			br.s = s;
			br.pos = 0;

			// --- Analyse des paramètres (TS 26.090 §7, tables 9a-9h). ---
			int32 lsfIdx[5] = {0};
			int32 lsfSign = 0;
			NkSfPrm sf[4];
			switch (mode) {
				case NK_AMR_MR122: {
					lsfIdx[0] = br.Get(7);
					lsfIdx[1] = br.Get(8);
					lsfIdx[2] = br.Get(8);
					lsfSign = br.Get(1);
					lsfIdx[3] = br.Get(8);
					lsfIdx[4] = br.Get(6);
					for (int32 k = 0; k < 4; ++k) {
						sf[k].acb = br.Get((k == 0 || k == 2) ? 9 : 6);
						sf[k].gpIdx = br.Get(4);
						for (int32 t = 0; t < 5; ++t) {
							sf[k].sign[t] = br.Get(1);
							sf[k].pos[t] = br.Get(3);
						}
						for (int32 t = 5; t < 10; ++t)
							sf[k].pos[t] = br.Get(3);
						sf[k].gcIdx = br.Get(5);
					}
					break;
				}
				case NK_AMR_MR102: {
					lsfIdx[0] = br.Get(8);
					lsfIdx[1] = br.Get(9);
					lsfIdx[2] = br.Get(9);
					for (int32 k = 0; k < 4; ++k) {
						sf[k].acb = br.Get((k == 0 || k == 2) ? 8 : 5);
						for (int32 t = 0; t < 4; ++t)
							sf[k].sign[t] = br.Get(1);
						sf[k].pos[0] = br.Get(10);
						sf[k].pos[1] = br.Get(10);
						sf[k].pos[2] = br.Get(7);
						sf[k].gains = br.Get(7);
					}
					break;
				}
				case NK_AMR_MR795: {
					lsfIdx[0] = br.Get(9);
					lsfIdx[1] = br.Get(9);
					lsfIdx[2] = br.Get(9);
					for (int32 k = 0; k < 4; ++k) {
						sf[k].acb = br.Get((k == 0 || k == 2) ? 8 : 6);
						sf[k].pos[3] = br.Get(4);
						sf[k].pos[2] = br.Get(3);
						sf[k].pos[1] = br.Get(3);
						sf[k].pos[0] = br.Get(3);
						sf[k].sign[3] = br.Get(1);
						sf[k].sign[2] = br.Get(1);
						sf[k].sign[1] = br.Get(1);
						sf[k].sign[0] = br.Get(1);
						sf[k].gpIdx = br.Get(4);
						sf[k].gcIdx = br.Get(5);
					}
					break;
				}
				case NK_AMR_MR74: {
					lsfIdx[0] = br.Get(8);
					lsfIdx[1] = br.Get(9);
					lsfIdx[2] = br.Get(9);
					for (int32 k = 0; k < 4; ++k) {
						sf[k].acb = br.Get((k == 0 || k == 2) ? 8 : 5);
						sf[k].pos[3] = br.Get(4);
						sf[k].pos[2] = br.Get(3);
						sf[k].pos[1] = br.Get(3);
						sf[k].pos[0] = br.Get(3);
						sf[k].sign[3] = br.Get(1);
						sf[k].sign[2] = br.Get(1);
						sf[k].sign[1] = br.Get(1);
						sf[k].sign[0] = br.Get(1);
						sf[k].gains = br.Get(7);
					}
					break;
				}
				case NK_AMR_MR67: {
					lsfIdx[0] = br.Get(8);
					lsfIdx[1] = br.Get(9);
					lsfIdx[2] = br.Get(9);
					for (int32 k = 0; k < 4; ++k) {
						sf[k].acb = br.Get((k == 0 || k == 2) ? 8 : 4);
						sf[k].pos[2] = br.Get(4);
						sf[k].pos[1] = br.Get(4);
						sf[k].pos[0] = br.Get(3);
						sf[k].sign[2] = br.Get(1);
						sf[k].sign[1] = br.Get(1);
						sf[k].sign[0] = br.Get(1);
						sf[k].gains = br.Get(7);
					}
					break;
				}
				case NK_AMR_MR59: {
					lsfIdx[0] = br.Get(8);
					lsfIdx[1] = br.Get(9);
					lsfIdx[2] = br.Get(9);
					for (int32 k = 0; k < 4; ++k) {
						sf[k].acb = br.Get((k == 0 || k == 2) ? 8 : 4);
						sf[k].pos[1] = br.Get(5);
						sf[k].pos[0] = br.Get(4);
						sf[k].sign[1] = br.Get(1);
						sf[k].sign[0] = br.Get(1);
						sf[k].gains = br.Get(6);
					}
					break;
				}
				case NK_AMR_MR515: {
					lsfIdx[0] = br.Get(8);
					lsfIdx[1] = br.Get(8);
					lsfIdx[2] = br.Get(7);
					for (int32 k = 0; k < 4; ++k) {
						sf[k].acb = br.Get((k == 0) ? 8 : 4);
						sf[k].subset = br.Get(1);
						sf[k].pos[1] = br.Get(3);
						sf[k].pos[0] = br.Get(3);
						sf[k].sign[1] = br.Get(1);
						sf[k].sign[0] = br.Get(1);
						sf[k].gains = br.Get(6);
					}
					break;
				}
				default: { // MR475
					lsfIdx[0] = br.Get(8);
					lsfIdx[1] = br.Get(8);
					lsfIdx[2] = br.Get(7);
					for (int32 k = 0; k < 4; ++k) {
						sf[k].acb = br.Get((k == 0) ? 8 : 4);
						sf[k].subset = br.Get(1);
						sf[k].pos[1] = br.Get(3);
						sf[k].pos[0] = br.Get(3);
						sf[k].sign[1] = br.Get(1);
						sf[k].sign[0] = br.Get(1);
						if (k == 0 || k == 2)
							sf[k].gains = br.Get(8); // partagé par paire de sous-trames
					}
					sf[1].gains = sf[0].gains;
					sf[3].gains = sf[2].gains;
					break;
				}
			}

			// --- LSF -> LSP (par sous-trame) -> LPC. ---
			float64 lspSub[4][kLpcOrder];
			DecodeLsf(mode, lsfIdx, lsfSign, lspSub);
			// Moyenne LSP pour le lissage du gain fixe (eq. 70).
			const bool smoothMode = (mode == NK_AMR_MR102 || mode == NK_AMR_MR67 || mode == NK_AMR_MR59 ||
									 mode == NK_AMR_MR515 || mode == NK_AMR_MR475);
			if (smoothMode)
				for (int32 i = 0; i < kLpcOrder; ++i)
					mLspAvg[i] = 0.84 * mLspAvg[i] + 0.16 * mLspOld[i];

			// --- Boucle sous-trames. ---
			float64 speech[kFrameSamples];
			for (int32 k = 0; k < 4; ++k) {
				float64 a[11];
				LspToLpc(lspSub[k], a);
				// Décalage : le sous-trame courant commence à mExc[kExcHistory].
				// (mExc a été décalé en fin de sous-trame précédent.)
				float64 lag;
				int32 t0;
				DecodeAdaptive(mode, k, sf[k].acb, lag, t0);
				float64 v[kSubframeSamples];
				BuildAdaptiveVector(lag, v);
				// Codebook algébrique.
				float64 c[kSubframeSamples];
				DecodeAlgebraic(mode, k, sf[k].pos, sf[k].sign, sf[k].subset, c);
				// Renforcement du pitch (§6.1-2) : c(n) += β c(n-T).
				float64 beta;
				if (mode == NK_AMR_MR122)
					beta = mPrevGp; // remplacé ci-dessous par ĝp courant
				else if (mode == NK_AMR_MR475)
					beta = mPrevOddGp;
				else
					beta = mPrevGp;
				// --- Gains. ---
				// (Pour 12.2 : ĝp courant est disponible avant le codebook fixe
				// — table 9a — et sert au sharpening, borné [0,1].)
				float64 gp = 0.0, gamma = 1.0;
				if (mode == NK_AMR_MR122 || mode == NK_AMR_MR795) {
					gp = (float64)kQuaGainPitch[sf[k].gpIdx] / 16384.0;
					gamma = (float64)kQuaGainCode[sf[k].gcIdx * 3] / 2048.0;
				} else if (mode == NK_AMR_MR102 || mode == NK_AMR_MR74 || mode == NK_AMR_MR67) {
					const nk_int16 *e = kGainHighRates + sf[k].gains * 4;
					gp = (float64)e[0] / 16384.0;
					gamma = (float64)e[1] / 4096.0;
				} else if (mode == NK_AMR_MR59 || mode == NK_AMR_MR515) {
					const nk_int16 *e = kGainLowRates + sf[k].gains * 4;
					gp = (float64)e[0] / 16384.0;
					gamma = (float64)e[1] / 4096.0;
				} else { // MR475
					const nk_int16 *e = kGainMr475 + sf[k].gains * 4;
					gp = (float64)e[(k & 1) * 2] / 16384.0;
					gamma = (float64)e[(k & 1) * 2 + 1] / 4096.0;
				}
				if (mode == NK_AMR_MR122)
					beta = gp;
				const float64 betaMax = (mode == NK_AMR_MR122) ? 1.0 : 0.8;
				if (beta > betaMax)
					beta = betaMax;
				if (beta < 0.0)
					beta = 0.0;
				// Prédiction du gain fixe (eq. 66-69) : E_I est calculée sur le
				// vecteur d'impulsions AVANT renforcement de pitch.
				float64 ec = 0.0;
				for (int32 n = 0; n < kSubframeSamples; ++n)
					ec += c[n] * c[n];
				if (t0 < kSubframeSamples)
					for (int32 n = t0; n < kSubframeSamples; ++n)
						c[n] += beta * c[n - t0];
				const float64 eI = 10.0 * math::NkLog10(ec / 40.0 + 1e-12);
				const float64 eTil = kPredB[0] * mEnerHist[0] + kPredB[1] * mEnerHist[1] + kPredB[2] * mEnerHist[2] +
									 kPredB[3] * mEnerHist[3];
				const float64 gc0 = math::NkPow(10.0, 0.05 * (eTil + kMeanEnergy[mode] - eI));
				float64 gc = gamma * gc0;
				// Mise à jour du prédicteur : R(n) = 20 log10(γgc) (eq. 58).
				mEnerHist[3] = mEnerHist[2];
				mEnerHist[2] = mEnerHist[1];
				mEnerHist[1] = mEnerHist[0];
				mEnerHist[0] = 20.0 * math::NkLog10(gamma + 1e-12);
				// Historique des gains.
				for (int32 i = 4; i > 0; --i) {
					mGpHist[i] = mGpHist[i - 1];
					mGcHist[i] = mGcHist[i - 1];
				}
				mGpHist[0] = gp;
				mGcHist[0] = gc;
				// --- Lissage du gain fixe (eq. 70-74, modes 10.2/6.7/5.9/5.15/4.75). ---
				float64 gcUsed = gc;
				if (smoothMode) {
					float64 diff = 0.0;
					for (int32 i = 0; i < kLpcOrder; ++i) {
						const float64 d = lspSub[k][i] - mLspAvg[i];
						const float64 den = (mLspAvg[i] > 0.0) ? mLspAvg[i] : -mLspAvg[i];
						diff += ((d > 0.0) ? d : -d) / (den + 1e-9);
					}
					if (diff > 0.65)
						++mDiffCount;
					else
						mDiffCount = 0;
					if (mDiffCount >= 40) // 10 trames consécutives
						mNoSmoothHangover = 40;
					float64 km = (diff - 0.4) / 0.25;
					if (km < 0.0)
						km = 0.0;
					if (km > 1.0)
						km = 1.0;
					if (mNoSmoothHangover > 0) {
						km = 1.0;
						--mNoSmoothHangover;
					}
					float64 gAvg = 0.0;
					for (int32 i = 0; i < 5; ++i)
						gAvg += mGcHist[i];
					gAvg *= 0.2;
					gcUsed = gc * km + gAvg * (1.0 - km);
				}
				// --- Anti-éparpillement (§6.1-5, modes 7.95/6.7/5.9/5.15/4.75). ---
				const bool dispMode = (mode == NK_AMR_MR795 || mode == NK_AMR_MR67 || mode == NK_AMR_MR59 ||
									   mode == NK_AMR_MR515 || mode == NK_AMR_MR475);
				if (dispMode) {
					int32 impNr = (gp < 0.6) ? 0 : (gp < 0.9) ? 1 : 2;
					const bool onset = (gc > 2.0 * mPrevGc);
					if (!onset && impNr == 0) {
						if (Median5(mGpHist) >= 0.6)
							impNr = 1;
					}
					if (!onset && impNr > mPrevImpNr + 1)
						impNr = mPrevImpNr + 1;
					if (onset && impNr < 2)
						++impNr;
					mPrevImpNr = impNr;
					if (impNr < 2) {
						const nk_int16 *imp;
						if (mode == NK_AMR_MR795)
							imp = (impNr == 0) ? kPhImpLow795 : kPhImpMid795;
						else
							imp = (impNr == 0) ? kPhImpLow : kPhImpMid;
						float64 cd[kSubframeSamples];
						for (int32 n = 0; n < kSubframeSamples; ++n) {
							float64 sAcc = 0.0;
							for (int32 i = 0; i < kSubframeSamples; ++i) {
								int32 j = n - i;
								if (j < 0)
									j += kSubframeSamples;
								sAcc += c[i] * (float64)imp[j] / 32768.0;
							}
							cd[n] = sAcc;
						}
						for (int32 n = 0; n < kSubframeSamples; ++n)
							c[n] = cd[n];
					}
				}
				mPrevGc = gc;
				// --- Excitation u(n) = ĝp v(n) + ĝc c(n) (eq. 75). ---
				float64 *cur = mExc + kExcHistory;
				float64 u[kSubframeSamples];
				for (int32 n = 0; n < kSubframeSamples; ++n) {
					u[n] = gp * v[n] + gcUsed * c[n];
					cur[n] = u[n]; // mémoire du codebook adaptatif
				}
				// Emphase de l'excitation + AGC (eq. 76-78).
				float64 ue[kSubframeSamples];
				if (gp > 0.5) {
					const float64 f = (mode == NK_AMR_MR122) ? 0.25 : 0.5;
					float64 gpB = gp;
					if (gpB > betaMax)
						gpB = betaMax;
					float64 e0 = 0.0, e1 = 0.0;
					for (int32 n = 0; n < kSubframeSamples; ++n) {
						ue[n] = u[n] + f * gpB * gp * v[n];
						e0 += u[n] * u[n];
						e1 += ue[n] * ue[n];
					}
					const float64 eta = (e1 > 0.0) ? math::NkSqrt(e0 / e1) : 1.0;
					for (int32 n = 0; n < kSubframeSamples; ++n)
						ue[n] *= eta;
				} else {
					for (int32 n = 0; n < kSubframeSamples; ++n)
						ue[n] = u[n];
				}
				// --- Synthèse LP (eq. 79). ---
				float64 syn[kSubframeSamples];
				for (int32 n = 0; n < kSubframeSamples; ++n) {
					float64 sAcc = ue[n];
					for (int32 i = 1; i <= 10; ++i) {
						const float64 past = (n - i >= 0) ? syn[n - i] : mSynMem[10 + (n - i)];
						sAcc -= a[i] * past;
					}
					syn[n] = sAcc;
				}
				for (int32 i = 0; i < 10; ++i)
					mSynMem[i] = syn[kSubframeSamples - 10 + i];
				// --- Post-filtre. ---
				Postfilter(mode, a, syn, speech + k * kSubframeSamples);
				// États inter-sous-trames.
				if (k == 0 || k == 2)
					mPrevOddGp = gp;
				mPrevGp = gp;
				// Décale l'excitation : le sous-trame décodé rejoint l'historique.
				for (int32 i = 0; i < kExcHistory; ++i)
					mExc[i] = mExc[i + kSubframeSamples];
			}

			// --- Passe-haut 60 Hz (eq. 87). ---
			// NB : la remise à l'échelle x2 du §6.2.2 est déjà implicite dans
			// notre chaîne flottante (les constantes Ē produisent des gains en
			// pleine échelle) — vérifié par rapport RMS 2.00 contre référence.
			for (int32 n = 0; n < kFrameSamples; ++n) {
				const float64 xn = speech[n];
				const float64 yn = 0.939819335 * xn - 1.879638672 * mHpX[0] + 0.939819335 * mHpX[1] +
								   1.933105469 * mHpY[0] - 0.935913085 * mHpY[1];
				mHpX[1] = mHpX[0];
				mHpX[0] = xn;
				mHpY[1] = mHpY[0];
				mHpY[0] = yn;
				float64 o = yn;
				if (o > 32767.0)
					o = 32767.0;
				if (o < -32768.0)
					o = -32768.0;
				out[n] = (nk_int16)math::NkFloor(o + 0.5);
			}
			return true;
		}

		// =====================================================================
		// Lecteur de fichier « #!AMR\n » (RFC 4867).
		// =====================================================================
		bool NkAmrFileReader::Attach(const nk_uint8 *data, uint64 size) {
			static const nk_uint8 kMagic[6] = {'#', '!', 'A', 'M', 'R', '\n'};
			if (size < 6)
				return false;
			for (int32 i = 0; i < 6; ++i)
				if (data[i] != kMagic[i])
					return false;
			mData = data;
			mSize = size;
			mPos = 6;
			return true;
		}

		bool NkAmrFileReader::NextFrame(int32 &ft, const nk_uint8 *&payload) {
			if (mPos >= mSize)
				return false;
			const nk_uint8 header = mData[mPos];
			ft = (header >> 3) & 0xF;
			const int32 bytes = NkAmrDecoder::PayloadBytes(ft);
			if (bytes < 0 || mPos + 1 + (uint64)bytes > mSize)
				return false;
			payload = mData + mPos + 1;
			mPos += 1 + (uint64)bytes;
			return true;
		}

	} // namespace media
} // namespace nkentseu
