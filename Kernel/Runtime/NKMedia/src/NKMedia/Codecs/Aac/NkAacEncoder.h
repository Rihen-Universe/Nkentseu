// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKMedia/Codecs/Aac/NkAacEncoder.h
// -----------------------------------------------------------------------------
// (23/10) UN CODEUR AAC-LC MINIMAL (ISO/IEC 14496-3), pour le son du Direct de NKCode (le micro).
// Fenetre longue seulement (ONLY_LONG_SEQUENCE, sinus), MDCT 2048 par repliement TDAC + DCT-IV par FFT de
// 512 points, un element CPE stereo en M/S sur toutes les bandes (un micro mono donne S nul : presque rien),
// livre de code 11 (et ZERO_HCB pour une bande nulle), UN facteur d'echelle par trame, cherche par dichotomie
// sous le budget du debit. Les tables Huffman sont CELLES DU DECODEUR maison (NkAacHuffmanTables.cpp, valide
// bit-proche contre ffmpeg).
// PROUVE (23/10) par ffmpeg (decodeur independant) : une sinusoide de 440 Hz revient a 440,0 Hz, a son niveau,
// rapport signal/bruit 77,7 dB, sans decalage (prototype tools/prouver_aac_codeur.py, puis ce code : banc du Direct).
// La trame k code la fenetre [bloc k, bloc k+1] : le decodeur en rend EXACTEMENT le bloc k (pas de retard d'un
// bloc), au prix d'un bloc d'avance a l'encodage.
// Zero-STL, nkentseu::media.
// =============================================================================
#pragma once
#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace media {
		class NkAacEncoder {
			public:
				/// 44 100 Hz seulement pour l'instant (le Direct) ; `debit` en bit/s (128 000 conseille).
				bool Open(int32 sampleRate, int32 debit);
				/// Code la fenetre de 2048 echantillons par canal (echelle int16 : +-32767), [bloc k, bloc k+1], en une
				/// trame brute (raw_data_block). Rend faux si le codeur n'est pas ouvert.
				bool EncodeFrame(const float32 *gauche, const float32 *droite, NkVector<uint8> &trame);
				/// L'AudioSpecificConfig (2 octets) : AAC LC, la frequence, 2 canaux. Rend 0 si la frequence est inconnue.
				static int32 AudioSpecificConfig(int32 sampleRate, uint8 out[2]);
				/// La MDCT rapide contre la formule directe (donnees pseudo-aleatoires) : l'ecart relatif.
				static float64 SelfTestMdct();
				int32 DernierFacteur() const {
					return mDernierFacteur;
				}
				/// (bancs) contre-epreuve : la MDCT sans le facteur 2 de la norme (le son sort a moitie).
				bool mutationSansFacteurDeux = false;

			private:
				bool mOuvert = false;
				int32 mSfIndex = 4;
				int32 mBudget = 0; ///< bits par trame
				int32 mMaxSfb = 0;
				const uint16 *mSwb = nullptr;
				int32 mDernierFacteur = 0;
				float32 mM[1024], mS[1024];
				int32 mQ[1024];
		};
	} // namespace media
} // namespace nkentseu
