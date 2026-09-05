// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKImage/Core/NkTextureOven.h — LE FOUR
// =============================================================================
// Une image decodee (PNG, JPEG, TGA, HDR…) -> le payload d'un actif `.nktex` :
// pixels DEJA dans la disposition que le GPU accepte, mipmaps precalculees.
//
// ── POURQUOI CE FICHIER EST UN EN-TETE SEUL, ET OU IL VIT ────────────────────
// Le four est un PONT entre deux modules : `NKImage` sait decoder et reduire,
// `NKSerialization` sait ecrire un actif. Aucun des deux ne doit dependre de
// l'autre pour ça — NKSerialization tirerait douze codecs d'image, NKImage
// tirerait la reflexion. Le pont est donc un en-tete seul, range du cote des
// pixels, et c'est SON UTILISATEUR (l'outil de cuisson, le banc, l'editeur) qui
// met les deux modules sur son chemin d'inclusion. Rien n'est compile dans
// `NKImage.lib` : la dependance n'existe que chez celui qui inclut ce fichier.
//
// ⚠️ Une seule copie de cette logique. Le banc et l'outil de production doivent
// eprouver le MEME chemin — un four ecrit deux fois diverge au premier correctif.
//
// ── CE QUE LE FOUR CHANGE, ET POURQUOI ───────────────────────────────────────
// Une image a 3 canaux (RGB24) N'EST PAS televersable : `NkGPUFormat` n'a aucun
// format RGB8, et `NkFormatBytesPerPixel` rendrait 0. C'est d'ailleurs ce que
// fait deja `NkTextureLibrary::LoadWithNKImage` a chaque chargement — il
// convertit en RGBA8 dense. Le four fait donc la meme conversion UNE FOIS, a la
// cuisson, au lieu de la refaire a chaque lancement. C'est exactement la raison
// d'etre d'un format d'actif : le travail se paie au four, pas au demarrage.
// =============================================================================
#pragma once

#ifndef NKENTSEU_IMAGE_CORE_NKTEXTUREOVEN_H
#define NKENTSEU_IMAGE_CORE_NKTEXTUREOVEN_H

#include "NKImage/Core/NkImage.h"
#include "NKSerialization/Asset/NkTextureAssetFormat.h"

#include <cstring>
#include <utility>

namespace nkentseu {

	// =========================================================================
	// Reglages de cuisson
	// =========================================================================
	struct NkTexOvenReglages {
			// Vrai pour une carte de COULEUR (albedo, emission) — l'echantillonnage
			// devra deliner. Faux pour une donnee (normale, rugosite, metallique,
			// occlusion) : la deliner l'abimerait.
			bool sRGB = true;
			bool genererMips = true;
			nk_uint32 addressMode = NKTEXADDR_REPEAT;
			nk_uint32 filterMode = NKTEXFILTER_LINEAR;
	};

	// =========================================================================
	// NkTextureOven
	// =========================================================================
	class NkTextureOven {
		public:
			// Le format de pixel que le GPU acceptera, pour une image donnee.
			// Rend NKTEXFMT_INCONNU si l'image doit d'abord etre convertie.
			[[nodiscard]] static nk_uint32 CodeFormat(NkImagePixelFormat f, bool srgb) noexcept {
				switch (f) {
					case NkImagePixelFormat::NK_GRAY8:
						return NKTEXFMT_R8_UNORM;
					case NkImagePixelFormat::NK_GRAY_A16:
						return NKTEXFMT_RG8_UNORM;
					case NkImagePixelFormat::NK_RGBA32:
						return srgb ? nk_uint32(NKTEXFMT_RGBA8_SRGB) : nk_uint32(NKTEXFMT_RGBA8_UNORM);
					case NkImagePixelFormat::NK_RGBA128F:
						return NKTEXFMT_RGBA32_FLOAT;
					case NkImagePixelFormat::NK_RGB96F:
						return NKTEXFMT_RGB32_FLOAT;
					default:
						// NK_RGB24 tombe ici EXPRES : il faut le convertir en
						// RGBA32 avant, le GPU n'a pas de format a 3 octets.
						return NKTEXFMT_INCONNU;
				}
			}

			// Le format vers lequel convertir une image que le GPU refuserait.
			// Rend le format d'entree si aucune conversion n'est necessaire.
			[[nodiscard]] static NkImagePixelFormat FormatTeleversable(NkImagePixelFormat f) noexcept {
				switch (f) {
					case NkImagePixelFormat::NK_RGB24:
						return NkImagePixelFormat::NK_RGBA32; // pas de RGB8 cote GPU
					case NkImagePixelFormat::NK_RGB96F:
						return NkImagePixelFormat::NK_RGB96F; // NK_RGB32_FLOAT existe
					default:
						return f;
				}
			}

			// Recopie les lignes d'une image SERREES : `NkImage` aligne son pas de
			// ligne sur 4 octets, le fichier porte un pas explicite et sans trou.
			static void SerrerLignes(const NkImage &img, NkVector<nk_uint8> &out) noexcept {
				const nk_size pas = nk_size(img.Width()) * nk_size(BytesPerPixelOf(img.Format()));
				out.Clear();
				out.Resize(pas * nk_size(img.Height()));
				for (int32 y = 0; y < img.Height(); ++y)
					memcpy(out.Data() + nk_size(y) * pas, img.RowPtr(y), pas);
			}

			// ── LE FOUR ──────────────────────────────────────────────────────
			// `source` : une image decodee. `out` : le payload d'un `.nktex`.
			// L'appelant l'ecrit ensuite dans un actif par `NkAssetIO::Write`.
			[[nodiscard]] static nk_bool Cuire(const NkImage &source, const NkTexOvenReglages &reglages,
											   NkVector<nk_uint8> &out, NkString *err = nullptr) noexcept {
				out.Clear();
				if (!source.IsValid())
					return Refus(err, "image source invalide");

				// 1. Amener l'image dans un format que le GPU accepte.
				const NkImagePixelFormat cible = FormatTeleversable(source.Format());
				NkImage travail = (cible == source.Format()) ? source.Copy() : source.Convert(cible);
				if (!travail.IsValid())
					return Refus(err, "conversion vers un format televersable impossible");

				const nk_uint32 code = CodeFormat(travail.Format(), reglages.sRGB);
				if (code == NKTEXFMT_INCONNU)
					return Refus(err, "format de pixel non cuisinable");
				const nk_uint32 bpp = nk_uint32(BytesPerPixelOf(travail.Format()));

				// 2. La chaine de niveaux. Les octets sont ranges d'abord, en
				//    entier, parce que les descripteurs pointeront dedans : le
				//    tampon ne doit plus bouger ensuite.
				const nk_uint32 nMax = reglages.genererMips
										   ? NkTexturePayload::CompteMipsComplet(nk_uint32(travail.Width()),
																				 nk_uint32(travail.Height()))
										   : 1u;
				NkVector<NkVector<nk_uint8>> octets;
				octets.Resize(nk_size(nMax));
				NkVector<nk_uint32> largeurs, hauteurs;

				{
					NkImage courant = std::move(travail);
					for (nk_uint32 i = 0; i < nMax; ++i) {
						SerrerLignes(courant, octets[i]);
						largeurs.PushBack(nk_uint32(courant.Width()));
						hauteurs.PushBack(nk_uint32(courant.Height()));
						if (i + 1u == nMax)
							break;
						NkImage suivant = courant.ReduceHalf();
						if (!suivant.IsValid())
							break;
						courant = std::move(suivant);
					}
				}

				// 3. Assembler et encoder.
				const nk_size nNiveaux = largeurs.Size();
				NkTexCuisson cuisson;
				cuisson.formatCode = code;
				cuisson.width = largeurs[0];
				cuisson.height = hauteurs[0];
				cuisson.depth = 1u;
				cuisson.arrayLayers = 1u;
				cuisson.flags = (reglages.sRGB && NkTexFormatEstSrgb(code) ? nk_uint32(NKTEXFLAG_SRGB) : 0u) |
								(nNiveaux > 1u ? nk_uint32(NKTEXFLAG_MIPS_PRECALCULES) : 0u);
				cuisson.addressMode = reglages.addressMode;
				cuisson.filterMode = reglages.filterMode;
				cuisson.rowAlignment = 1u;
				for (nk_size i = 0; i < nNiveaux; ++i) {
					NkTexNiveauSource n;
					n.width = largeurs[i];
					n.height = hauteurs[i];
					n.depth = 1u;
					n.rowPitch = largeurs[i] * bpp;
					n.data = octets[i].Data();
					n.size = nk_uint32(octets[i].Size());
					cuisson.levels.PushBack(n);
				}
				return NkTexturePayload::Encode(cuisson, out, err);
			}

			// Commodite : depuis un fichier image sur disque.
			[[nodiscard]] static nk_bool CuireFichier(const char *cheminImage, const NkTexOvenReglages &reglages,
													  NkVector<nk_uint8> &out, NkString *err = nullptr) noexcept {
				NkImage img;
				if (!cheminImage || !img.Load(cheminImage, 0))
					return Refus(err, "image source illisible");
				return Cuire(img, reglages, out, err);
			}

		private:
			static nk_bool Refus(NkString *err, const char *raison) noexcept {
				if (err)
					*err = NkString(raison);
				return false;
			}
	};

} // namespace nkentseu

#endif // NKENTSEU_IMAGE_CORE_NKTEXTUREOVEN_H
