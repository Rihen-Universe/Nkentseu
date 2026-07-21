// =============================================================================
// NKMedia/Codecs/Video/VP8/NkVp8BoolDecoder.h
// -----------------------------------------------------------------------------
// Décodeur booléen (arithmétique binaire) VP8 — RFC 6386 §7.3 "Boolean Entropy
// Decoder". C'est l'épine dorsale de TOUT le bitstream VP8 (en-têtes ET résidus) :
// contrairement à la CABAC H.264 (machine à états + tables de transition par
// contexte), le décodeur booléen VP8 est un simple codeur arithmétique binaire à
// probabilité EXPLICITE (0..255) fournie par l'appelant à chaque appel — pas de
// table d'état à transcrire, donc bien moins de surface pour un bug de table
// (leçon des ×5 bugs de table du décodeur H.264, voir ROADMAP).
//
// Utilisé pour : tous les indicateurs d'en-tête (key_frame, dimensions,
// segmentation, filtre de boucle, quantification…), les modes de prédiction
// (arbres de probabilités, §8.2 GetTree), les vecteurs de mouvement, et les
// coefficients résiduels (tokens DCT, contexte par catégorie).
//
// Header-only, zero-STL, namespace nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		// Nœud d'arbre de probabilités (§8.2) : un indice négatif = feuille (valeur =
		// -indice), un indice positif = nœud interne (indice dans le même tableau).
		// Convention RFC : les tables sont des tableaux de int8, deux entrées par nœud
		// (branche 0 / branche 1).
		struct NkVp8BoolDecoder {
				const uint8 *buf = nullptr;
				usize size = 0;
				usize pos = 0;	   // prochain octet à consommer
				uint32 value = 0; // accumulateur (16 bits utiles, marge pour le décalage)
				uint32 range = 255;
				int32 bitCount = 0; // nb de décalages avant de recharger un octet

				NkVp8BoolDecoder() = default;
				NkVp8BoolDecoder(const uint8 *d, usize n) {
					Init(d, n);
				}

				uint8 NextByte() {
					return (pos < size) ? buf[pos++] : 0; // 0 au-delà de la fin (comportement standard)
				}

				void Init(const uint8 *d, usize n) {
					buf = d;
					size = n;
					pos = 0;
					value = ((uint32)NextByte() << 8);
					value |= NextByte();
					range = 255;
					bitCount = 0;
				}

				// §7.3 bool_get : décode UN bit sachant P(bit=0) = probability/256.
				int32 GetBool(int32 probability) {
					const uint32 split = 1u + (((range - 1u) * (uint32)probability) >> 8);
					const uint32 SPLIT = split << 8;
					int32 bit;
					if (value >= SPLIT) {
						bit = 1;
						range -= split;
						value -= SPLIT;
					} else {
						bit = 0;
						range = split;
					}
					while (range < 128u) {
						value <<= 1;
						range <<= 1;
						if (++bitCount == 8) {
							bitCount = 0;
							value |= NextByte();
						}
					}
					return bit;
				}

				// Bit non biaisé (probabilité 1/2) : flags simples de l'en-tête.
				int32 GetFlag() {
					return GetBool(128);
				}

				// L(n) : n bits non biaisés, MSB en premier (§9.1 littéraux d'en-tête).
				uint32 GetLiteral(int32 n) {
					uint32 v = 0;
					for (int32 i = 0; i < n; ++i)
						v = (v << 1) | (uint32)GetFlag();
					return v;
				}

				// Littéral signé : magnitude sur n bits PUIS un bit de signe (1 = négatif) —
				// convention utilisée pour delta_q, les ajustements de segmentation, etc.
				int32 GetSignedLiteral(int32 n) {
					const int32 mag = (int32)GetLiteral(n);
					return GetFlag() ? -mag : mag;
				}

				// Valeur optionnelle : un flag de présence PUIS la valeur si présente (sinon
				// `defaultValue`). Motif très fréquent dans l'en-tête (probabilités mises à
				// jour "si et seulement si" un flag précède).
				bool GetFlagged(int32 n, int32 &outValue) {
					if (GetFlag()) {
						outValue = (int32)GetLiteral(n);
						return true;
					}
					return false;
				}

				// §8.2 : décode un symbole via un arbre binaire de probabilités. `tree` = paires
				// (branche0,branche1), valeur >0 = index du nœud suivant, <=0 = feuille (-valeur).
				// `probs` = une probabilité par nœud INTERNE (même indexation /2 que `tree`).
				int32 GetTree(const int8 *tree, const uint8 *probs, int32 start = 0) {
					int32 i = start;
					while ((i = tree[i + GetBool(probs[i >> 1])]) > 0) {
					}
					return -i;
				}

				// Motif "presence puis valeur signee" utilisé partout dans l'en-tête compressé
				// (deltas de quantification §9.6, données de segmentation §9.3, deltas de
				// filtre de boucle §9.4) : 1 bit de présence, si vrai magnitude sur n bits PUIS
				// signe. Renvoie 0 si absent.
				int32 GetOptionalSignedLiteral(int32 n) {
					if (!GetFlag())
						return 0;
					const int32 mag = (int32)GetLiteral(n);
					return GetFlag() ? -mag : mag;
				}

				bool Eof() const {
					return pos >= size && bitCount == 0; // heuristique : plus d'octets neufs à charger
				}
		};

	} // namespace media
} // namespace nkentseu
