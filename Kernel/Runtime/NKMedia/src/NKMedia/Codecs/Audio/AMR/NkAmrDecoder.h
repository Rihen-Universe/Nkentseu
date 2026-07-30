// =============================================================================
// NKMedia/Codecs/Audio/AMR/NkAmrDecoder.h
// -----------------------------------------------------------------------------
// Décodeur AMR-NB (Adaptive Multi-Rate narrowband, 8 kHz) FROM SCRATCH.
//   - Algorithme écrit À LA MAIN depuis le TEXTE de 3GPP TS 26.090 v18.0.0
//     (description fonctionnelle du décodeur, §5 pour les structures de
//     quantification, §6 pour la synthèse, §7 tables 9a-9h pour l'ordre des
//     paramètres) et 3GPP TS 26.101 v18.0.0 (format de trame, ordonnancement
//     des bits d(j) = s(table_m(j)+1)).
//   - Tables numériques normatives extraites MÉCANIQUEMENT (scripts, voir
//     NkAmrTables.inc) des DONNÉES de la livraison officielle du standard :
//     fichiers *.tab de 3GPP TS 26.073 + Annexe B de TS 26.101. AUCUN fichier
//     .c d'algorithme (3GPP/opencore/ffmpeg) n'a été lu ni porté.
//   - Implémentation FLOTTANTE (pas bit-exacte vis-à-vis de la référence
//     fixed-point) ; objectif : corrélation élevée avec le décodeur normatif.
// Conteneur : format de stockage RFC 4867 (« #!AMR\n », 1 octet d'en-tête de
// trame [P|FT(4)|Q|PP] puis les bits d(0..K-1) MSB-first).
// Les 8 modes 4.75…12.2 kbit/s sont décodés ; SID (FT=8) / NO_DATA (FT=15)
// sont parsés et rendus en silence (pas de génération de confort TS 26.092).
// Zero-STL. Namespace nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		// Modes AMR-NB (index = Frame Type 0..7 du conteneur, TS 26.101 table 1a).
		enum NkAmrMode : int32 {
			NK_AMR_MR475 = 0, // 4.75 kbit/s
			NK_AMR_MR515 = 1, // 5.15 kbit/s
			NK_AMR_MR59 = 2,  // 5.90 kbit/s
			NK_AMR_MR67 = 3,  // 6.70 kbit/s
			NK_AMR_MR74 = 4,  // 7.40 kbit/s
			NK_AMR_MR795 = 5, // 7.95 kbit/s
			NK_AMR_MR102 = 6, // 10.2 kbit/s
			NK_AMR_MR122 = 7, // 12.2 kbit/s
			NK_AMR_SID = 8,	  // trame de confort (DTX)
			NK_AMR_NO_DATA = 15
		};

		struct NkAmrDecoder {
				static const int32 kFrameSamples = 160; // 20 ms @ 8 kHz
				static const int32 kSubframeSamples = 40;
				static const int32 kLpcOrder = 10;
				static const int32 kPitMax = 143;
				static const int32 kExcHistory = 160; // 143 + 10 (interp) + marge

				// --- État persistant inter-trames ---
				float64 mExc[kExcHistory + kFrameSamples]; // u(n) : passé + trame courante
				float64 mLspOld[kLpcOrder];		 // q̂4(n-1) (domaine cosinus)
				float64 mPastR[kLpcOrder];		 // r̂(n-1) résiduel LSF quantifié (modes SVQ)
				float64 mPastR2[kLpcOrder];		 // r̂(2)(n-1) (mode 12.2, SMQ)
				float64 mEnerHist[4];			 // R̂(n-1..n-4) : erreurs d'énergie (dB) prédicteur MA
				float64 mPrevGp;				 // ĝp du sous-trame précédent (sharpening)
				float64 mPrevOddGp;				 // ĝp du sous-trame impair précédent (MR475)
				float64 mGpHist[5];				 // ĝp courant + 4 passés (anti-sparseness, médiane)
				float64 mGcHist[5];				 // ĝc courant + 4 passés (lissage du gain)
				float64 mPrevGc;				 // ĝc du sous-trame précédent (détection d'attaque)
				int32 mPrevImpNr;				 // anti-sparseness : impNr précédent
				float64 mLspAvg[kLpcOrder];		 // q̄(n) moyenne LSP (lissage gain, eq. 70)
				int32 mDiffCount;				 // trames consécutives avec diff > 0.65
				int32 mNoSmoothHangover;		 // hangover 40 sous-trames (km = 1)
				int32 mPrevT1;					 // lag entier du sous-trame (impair) précédent
				float64 mSynMem[kLpcOrder];		 // mémoire du filtre de synthèse 1/Â(z)
				// Post-filtre adaptatif
				float64 mPfResMem[kLpcOrder]; // mémoire Â(z/γn) (entrée ŝ passée)
				float64 mPfSynMem[kLpcOrder]; // mémoire 1/Â(z/γd)
				float64 mPfTiltMem;			  // mémoire Ht(z)
				float64 mPfAgcGain;			  // βsc (AGC échantillon par échantillon)
				// Filtre passe-haut de sortie (eq. 87)
				float64 mHpX[2];
				float64 mHpY[2];

				void Init();
				// Décode une trame : frameType = FT (0..8, 15), payload = bits d(j)
				// MSB-first (taille selon le mode). Sort 160 échantillons s16.
				// Retourne false si le type de trame est inconnu/réservé.
				bool DecodeFrame(int32 frameType, const nk_uint8 *payload, nk_int16 *out);

				// Taille en octets du payload (SANS l'octet d'en-tête) pour un FT
				// donné dans le format de stockage RFC 4867 ; -1 si FT invalide.
				static int32 PayloadBytes(int32 frameType);

			private:
				struct Subframe; // paramètres décodés d'un sous-trame (interne .cpp)
				void DecodeLsf(int32 mode, const int32 *idx, int32 thirdSign, float64 lspSub[4][kLpcOrder]);
				void LspToLpc(const float64 *lsp, float64 *a);
				void DecodeAdaptive(int32 mode, int32 sf, int32 index, float64 &lagOut, int32 &t0Out);
				void BuildAdaptiveVector(float64 lag, float64 *v);
				void DecodeAlgebraic(int32 mode, int32 sf, const int32 *pos, const int32 *sign, int32 subset,
									 float64 *c);
				void Postfilter(int32 mode, const float64 *a, const float64 *syn, float64 *out);
		};

		// ---------------------------------------------------------------------
		// Lecteur du format de stockage « #!AMR\n » (RFC 4867, mono).
		// Itère les trames (FT + pointeur payload) sans copie.
		// ---------------------------------------------------------------------
		struct NkAmrFileReader {
				const nk_uint8 *mData = nullptr;
				uint64 mSize = 0;
				uint64 mPos = 0;

				// Attache un tampon complet ; vérifie le magic « #!AMR\n ».
				bool Attach(const nk_uint8 *data, uint64 size);
				// Trame suivante : ft + payload (octets après l'en-tête). false = fin.
				bool NextFrame(int32 &ft, const nk_uint8 *&payload);
		};

	} // namespace media
} // namespace nkentseu
