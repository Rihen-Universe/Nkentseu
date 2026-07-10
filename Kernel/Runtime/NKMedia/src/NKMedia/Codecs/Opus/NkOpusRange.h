// =============================================================================
// NKMedia/Codecs/Opus/NkOpusRange.h
// -----------------------------------------------------------------------------
// Décodeur Opus (RFC 6716) — ÉTAPE 2 : CODEUR ENTROPIQUE (range coder, §4.1).
// Port fidèle de la référence (libopus entdec/entenc). Fournit le DÉCODEUR (requis
// pour décoder Opus) et l'ENCODEUR (pour prouver la correction par aller-retour).
// Fonctions : decode/update (cdf), bit_logp, icdf, bits bruts (depuis la fin), uint.
// Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		// Décodeur entropique (range decoder).
		struct NkOpusRangeDecoder {
				const uint8 *buf = nullptr;
				uint32 storage = 0;
				uint32 offs = 0;
				uint32 end_offs = 0;
				uint32 end_window = 0;
				int32 nend_bits = 0;
				int32 nbits_total = 0;
				uint32 val = 0;
				uint32 rng = 0;
				uint32 ext = 0;
				int32 rem = 0;
				int32 error = 0;

				void Init(const uint8 *buffer, uint32 storageBytes);
				uint32 Decode(uint32 ft);				  // renvoie fs (cf. RFC), puis appeler Update
				uint32 DecodeBin(uint32 bits);			  // ft = 1<<bits
				void Update(uint32 fl, uint32 fh, uint32 ft);
				int32 DecodeBitLogp(uint32 logp);		  // 1 bit avec proba 2^-logp
				int32 DecodeIcdf(const uint8 *icdf, uint32 ftb);
				uint32 DecodeBits(uint32 bits);			  // bits bruts (depuis la fin du buffer)
				uint32 DecodeUint(uint32 ft);			  // entier uniforme [0, ft)
				int32 Tell() const;						  // bits consommés (ec_tell) = nbits_total - ilog(rng)
				uint32 TellFrac() const;				  // bits consommés en 1/8 de bit (ec_tell_frac, BITRES=3)
		};

		// Encodeur entropique (range encoder) — pour les tests d'aller-retour.
		struct NkOpusRangeEncoder {
				uint8 *buf = nullptr;
				uint32 storage = 0;
				uint32 offs = 0;
				uint32 end_offs = 0;
				uint32 end_window = 0;
				int32 nend_bits = 0;
				int32 nbits_total = 0;
				uint32 val = 0;
				uint32 rng = 0;
				uint32 ext = 0;
				int32 rem = 0;
				int32 error = 0;

				void Init(uint8 *buffer, uint32 sizeBytes);
				void Encode(uint32 fl, uint32 fh, uint32 ft);
				void EncodeBin(uint32 fl, uint32 fh, uint32 bits);
				void EncodeBitLogp(int32 v, uint32 logp);
				void EncodeIcdf(int32 s, const uint8 *icdf, uint32 ftb);
				void EncodeBits(uint32 fl, uint32 bits);
				void EncodeUint(uint32 fl, uint32 ft);
				void Done();
				uint32 RangeBytes() const {
					return offs + end_offs;
				}
		};

		struct NkOpusRange {
				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
