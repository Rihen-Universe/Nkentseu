// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKSerialization/Asset/NkTextureAssetFormat.h
// =============================================================================
// Le PAYLOAD d'un actif texture — ce que le four produit et que le moteur
// televerse SANS DECODER.
//
// Ce fichier ne definit PAS un nouveau format de fichier. Le conteneur existe
// deja et il est bon : `NkAssetFileHeader` (40 o, magie + versions + CRC) suivi
// des metadonnees NkNative, puis d'un payload d'octets libres — cf.
// `NkAssetMetadata.h` et `CONVENTIONS_FICHIERS.md`. Un `.nktex` est donc un
// fichier d'actif ordinaire dont le payload a la forme decrite ici.
//
//   .nktex = NkAssetFileHeader(40) + NkAssetMetadata(NkNative) + CE PAYLOAD
//
// Ce que le payload porte :
//   - un en-tete : magie, version, code de format de pixel, dimensions,
//     nombre de niveaux de mip, espace colorimetrique, mode d'adressage,
//     alignement de televersement ;
//   - une table de niveaux (un par mip x couche) : dimensions, pas de ligne,
//     position et taille dans les donnees ;
//   - les octets de pixels, DEJA dans la disposition attendue par le GPU.
//
// ── DEUX REGLES QUI ONT DICTE CETTE FORME ────────────────────────────────────
//
// 1. **Le code de format est STABLE, ce n'est PAS l'ordinal d'un enum C++.**
//    `NkTextureAsset` (NKRenderer) ecrit aujourd'hui `static_cast<nk_uint32>`
//    d'un `NkGPUFormat` : inserer une valeur dans `NKRHI/Core/NkTypes.h`
//    changerait en silence la signification de tous les fichiers deja ecrits.
//    Ici les codes sont des constantes numerotees a la main, jamais reordonnees,
//    et une nouvelle valeur s'AJOUTE a la fin de sa famille.
//
// 2. **ADDITIF : un champ inconnu se relit et se reemet intact.** L'en-tete
//    porte sa propre taille (`headerSize`) et la table porte la taille d'une
//    entree (`levelEntrySize`). Un lecteur ancien saute ce qu'il ne connait pas
//    au lieu de se desynchroniser, `Decode` lui rend ces octets, et `Encode`
//    les remet — donc un aller-retour par une version ancienne ne DETRUIT pas
//    ce qu'une version recente avait ecrit.
//
// Tout est encode en petit-boutiste explicite : un actif cuit sur un poste
// Windows doit se lire sur Android, iOS et Web sans dependre de la machine qui
// l'a produit.
// =============================================================================
#pragma once

#ifndef NKENTSEU_SERIALIZATION_ASSET_NKTEXTUREASSETFORMAT_H
#define NKENTSEU_SERIALIZATION_ASSET_NKTEXTUREASSETFORMAT_H

#include "NKSerialization/Asset/NkAssetMetadata.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

#include <cstring>

namespace nkentseu {

	// =========================================================================
	// Codes de format de pixel — STABLES SUR DISQUE
	//
	// Ces nombres sont ecrits dans les fichiers. Ils ne se reordonnent JAMAIS et
	// ne se reutilisent jamais ; une valeur nouvelle s'ajoute a la fin de sa
	// famille. Les familles sont espacees pour que l'ajout d'un format non
	// compresse n'oblige pas a decaler les codes des formats compresses.
	// =========================================================================
	enum NkTexFormatCode : nk_uint32 {
		NKTEXFMT_INCONNU = 0u,

		// Valeur de DEMANDE, jamais ecrite dans un fichier : « choisis pour moi ».
		NKTEXFMT_AUTO = 0xFFFFFFFFu,

		// ── 1..99 : non compresses, un octet par canal ──
		NKTEXFMT_R8_UNORM = 1u,
		NKTEXFMT_RG8_UNORM = 2u,
		NKTEXFMT_RGB8_UNORM = 3u,
		NKTEXFMT_RGBA8_UNORM = 4u,
		NKTEXFMT_RGB8_SRGB = 5u,
		NKTEXFMT_RGBA8_SRGB = 6u,

		// ── 100..199 : non compresses, flottants ──
		NKTEXFMT_RGB32_FLOAT = 100u,
		NKTEXFMT_RGBA32_FLOAT = 101u,

		// ── 1000..1999 : compresses par blocs — PALIER SUIVANT, NON LIVRE.
		//    Les codes sont reserves ici pour qu'un fichier ecrit aujourd'hui
		//    n'ait pas a changer de version quand la compression arrivera ; le
		//    four ne les produit pas encore, et le RHI ne sait pas les
		//    televerser (`NkFormatBytesPerPixel` rend 0 pour tous les formats
		//    par blocs — mesure du 2026-09-05).
		NKTEXFMT_BC1_RGB_UNORM = 1000u,
		NKTEXFMT_BC1_RGB_SRGB = 1001u,
		NKTEXFMT_BC3_UNORM = 1002u,
		NKTEXFMT_BC3_SRGB = 1003u,
		NKTEXFMT_BC5_UNORM = 1004u,
		NKTEXFMT_BC7_UNORM = 1005u,
		NKTEXFMT_BC7_SRGB = 1006u,
		NKTEXFMT_ETC2_RGB_UNORM = 1100u,
		NKTEXFMT_ETC2_RGBA_UNORM = 1101u,
		NKTEXFMT_ASTC_4X4_UNORM = 1200u,
		NKTEXFMT_ASTC_4X4_SRGB = 1201u,
	};

	// Octets par pixel d'un format NON compresse. Rend 0 pour un format
	// compresse par blocs ET pour un code inconnu — l'appelant DOIT distinguer
	// les deux par `NkTexFormatEstCompresse`, sinon il calculera une taille
	// nulle sans s'en apercevoir (c'est exactement le defaut mesure dans
	// `NkFormatBytesPerPixel` cote RHI).
	[[nodiscard]] inline nk_uint32 NkTexFormatOctetsParPixel(nk_uint32 code) noexcept {
		switch (code) {
			case NKTEXFMT_R8_UNORM:
				return 1u;
			case NKTEXFMT_RG8_UNORM:
				return 2u;
			case NKTEXFMT_RGB8_UNORM:
			case NKTEXFMT_RGB8_SRGB:
				return 3u;
			case NKTEXFMT_RGBA8_UNORM:
			case NKTEXFMT_RGBA8_SRGB:
				return 4u;
			case NKTEXFMT_RGB32_FLOAT:
				return 12u;
			case NKTEXFMT_RGBA32_FLOAT:
				return 16u;
			default:
				return 0u;
		}
	}

	[[nodiscard]] inline bool NkTexFormatEstCompresse(nk_uint32 code) noexcept {
		return code >= 1000u && code < 2000u;
	}

	[[nodiscard]] inline bool NkTexFormatEstSrgb(nk_uint32 code) noexcept {
		return code == NKTEXFMT_RGB8_SRGB || code == NKTEXFMT_RGBA8_SRGB || code == NKTEXFMT_BC1_RGB_SRGB ||
			   code == NKTEXFMT_BC3_SRGB || code == NKTEXFMT_BC7_SRGB || code == NKTEXFMT_ASTC_4X4_SRGB;
	}

	[[nodiscard]] inline const char *NkTexFormatNom(nk_uint32 code) noexcept {
		switch (code) {
			case NKTEXFMT_R8_UNORM:
				return "R8_UNORM";
			case NKTEXFMT_RG8_UNORM:
				return "RG8_UNORM";
			case NKTEXFMT_RGB8_UNORM:
				return "RGB8_UNORM";
			case NKTEXFMT_RGBA8_UNORM:
				return "RGBA8_UNORM";
			case NKTEXFMT_RGB8_SRGB:
				return "RGB8_SRGB";
			case NKTEXFMT_RGBA8_SRGB:
				return "RGBA8_SRGB";
			case NKTEXFMT_RGB32_FLOAT:
				return "RGB32_FLOAT";
			case NKTEXFMT_RGBA32_FLOAT:
				return "RGBA32_FLOAT";
			case NKTEXFMT_BC1_RGB_UNORM:
				return "BC1_UNORM";
			case NKTEXFMT_BC1_RGB_SRGB:
				return "BC1_SRGB";
			case NKTEXFMT_BC3_UNORM:
				return "BC3_UNORM";
			case NKTEXFMT_BC3_SRGB:
				return "BC3_SRGB";
			case NKTEXFMT_BC5_UNORM:
				return "BC5_UNORM";
			case NKTEXFMT_BC7_UNORM:
				return "BC7_UNORM";
			case NKTEXFMT_BC7_SRGB:
				return "BC7_SRGB";
			case NKTEXFMT_ETC2_RGB_UNORM:
				return "ETC2_RGB";
			case NKTEXFMT_ETC2_RGBA_UNORM:
				return "ETC2_RGBA";
			case NKTEXFMT_ASTC_4X4_UNORM:
				return "ASTC_4x4_UNORM";
			case NKTEXFMT_ASTC_4X4_SRGB:
				return "ASTC_4x4_SRGB";
			default:
				return "inconnu";
		}
	}

	// =========================================================================
	// Drapeaux et modes
	// =========================================================================
	enum NkTexFlag : nk_uint32 {
		NKTEXFLAG_AUCUN = 0u,
		NKTEXFLAG_SRGB = 1u << 0,			 // l'echantillonnage doit deliner
		NKTEXFLAG_CUBEMAP = 1u << 1,		 // arrayLayers == 6, ordre +X -X +Y -Y +Z -Z
		NKTEXFLAG_ALPHA_PREMULT = 1u << 2,	 // couleur deja multipliee par alpha
		NKTEXFLAG_MIPS_PRECALCULES = 1u << 3 // les niveaux > 0 sont dans le fichier
	};

	enum NkTexAddressMode : nk_uint32 {
		NKTEXADDR_REPEAT = 0u,
		NKTEXADDR_CLAMP = 1u,
		NKTEXADDR_MIRROR = 2u,
	};

	enum NkTexFilterMode : nk_uint32 {
		NKTEXFILTER_LINEAR = 0u,
		NKTEXFILTER_NEAREST = 1u,
		NKTEXFILTER_ANISO = 2u,
	};

	// =========================================================================
	// Constantes de disposition — ce qui est ECRIT dans le fichier
	// =========================================================================
	inline constexpr nk_uint32 kNkTexPayloadVersion = 1u;
	// 8 (magie) + 2 (version) + 2 (headerSize) + 15 x 4 = 72 octets, multiple de 8.
	// ⚠️ Ce nombre et la suite d'ecritures d'`Encode` DOIVENT concorder : la
	// premiere version de ce fichier annoncait 64 en en ecrivant 68, et le
	// decodeur lisait alors la table des niveaux quatre octets trop tot. Le
	// defaut est desormais impossible a laisser passer — `Encode` compte ce
	// qu'il a ecrit et REFUSE si le compte ne tombe pas juste.
	inline constexpr nk_uint32 kNkTexHeaderSizeV1 = 72u;
	inline constexpr nk_uint32 kNkTexLevelEntrySizeV1 = 32u;
	inline constexpr char kNkTexMagic[8] = {'N', 'K', 'T', 'E', 'X', 'P', 'L', '1'};

	// =========================================================================
	// Petit-boutiste explicite
	// =========================================================================
	namespace nktexdetail {

		inline void EcrireU16(NkVector<nk_uint8> &b, nk_uint16 v) noexcept {
			b.PushBack(nk_uint8(v & 0xFFu));
			b.PushBack(nk_uint8((v >> 8) & 0xFFu));
		}

		inline void EcrireU32(NkVector<nk_uint8> &b, nk_uint32 v) noexcept {
			b.PushBack(nk_uint8(v & 0xFFu));
			b.PushBack(nk_uint8((v >> 8) & 0xFFu));
			b.PushBack(nk_uint8((v >> 16) & 0xFFu));
			b.PushBack(nk_uint8((v >> 24) & 0xFFu));
		}

		inline nk_uint16 LireU16(const nk_uint8 *p) noexcept {
			return nk_uint16(nk_uint16(p[0]) | (nk_uint16(p[1]) << 8));
		}

		inline nk_uint32 LireU32(const nk_uint8 *p) noexcept {
			return nk_uint32(p[0]) | (nk_uint32(p[1]) << 8) | (nk_uint32(p[2]) << 16) | (nk_uint32(p[3]) << 24);
		}

	} // namespace nktexdetail

	// =========================================================================
	// Un niveau (mip x couche) — description en ENTREE du four
	// =========================================================================
	struct NkTexNiveauSource {
			nk_uint32 width = 0u;
			nk_uint32 height = 0u;
			nk_uint32 depth = 1u;
			nk_uint32 rowPitch = 0u;   // 0 = serre (width * octetsParPixel)
			nk_uint32 slicePitch = 0u; // 0 = rowPitch * height
			const nk_uint8 *data = nullptr;
			nk_uint32 size = 0u;
	};

	// Ce que le four assemble.
	struct NkTexCuisson {
			nk_uint32 formatCode = NKTEXFMT_INCONNU;
			nk_uint32 width = 0u;
			nk_uint32 height = 0u;
			nk_uint32 depth = 1u;
			nk_uint32 arrayLayers = 1u;
			nk_uint32 flags = NKTEXFLAG_AUCUN;
			nk_uint32 addressMode = NKTEXADDR_REPEAT;
			nk_uint32 filterMode = NKTEXFILTER_LINEAR;
			nk_uint32 rowAlignment = 1u;
			// Niveaux, dans l'ordre mip 0..N-1 pour la couche 0, puis couche 1, etc.
			NkVector<NkTexNiveauSource> levels;
			// Octets d'en-tete qu'une version PLUS RECENTE avait ecrits et que
			// cette version ne comprend pas. Relus tels quels par `Decode`, remis
			// tels quels par `Encode` — c'est la regle additive du depot.
			NkVector<nk_uint8> enTeteInconnu;
	};

	// =========================================================================
	// Un niveau, vu EN LECTURE : une fenetre sur le tampon, sans copie
	// =========================================================================
	struct NkTexNiveauVue {
			nk_uint32 width = 0u;
			nk_uint32 height = 0u;
			nk_uint32 depth = 1u;
			nk_uint32 rowPitch = 0u;
			nk_uint32 slicePitch = 0u;
			const nk_uint8 *data = nullptr;
			nk_uint32 size = 0u;
	};

	struct NkTexVue {
			nk_uint32 version = 0u;
			nk_uint32 formatCode = NKTEXFMT_INCONNU;
			nk_uint32 width = 0u;
			nk_uint32 height = 0u;
			nk_uint32 depth = 1u;
			nk_uint32 arrayLayers = 1u;
			nk_uint32 mipCount = 0u;
			nk_uint32 flags = NKTEXFLAG_AUCUN;
			nk_uint32 addressMode = NKTEXADDR_REPEAT;
			nk_uint32 filterMode = NKTEXFILTER_LINEAR;
			nk_uint32 rowAlignment = 1u;
			NkVector<NkTexNiveauVue> levels;
			NkVector<nk_uint8> enTeteInconnu; // copie des octets d'en-tete non compris

			[[nodiscard]] bool EstSrgb() const noexcept {
				return (flags & NKTEXFLAG_SRGB) != 0u;
			}

			[[nodiscard]] bool EstCubemap() const noexcept {
				return (flags & NKTEXFLAG_CUBEMAP) != 0u;
			}
	};

	// =========================================================================
	// NkTexturePayload — encodage / decodage du payload
	// =========================================================================
	class NkTexturePayload {
		public:
			// Nombre de niveaux d'une chaine COMPLETE pour (w, h).
			[[nodiscard]] static nk_uint32 CompteMipsComplet(nk_uint32 w, nk_uint32 h) noexcept {
				nk_uint32 n = 1u;
				while (w > 1u || h > 1u) {
					if (w > 1u)
						w >>= 1;
					if (h > 1u)
						h >>= 1;
					++n;
				}
				return n;
			}

			// ── Encodage ─────────────────────────────────────────────────────
			[[nodiscard]] static nk_bool Encode(const NkTexCuisson &src, NkVector<nk_uint8> &out,
												NkString *err = nullptr) noexcept {
				out.Clear();

				if (src.formatCode == NKTEXFMT_INCONNU)
					return Refus(err, "format de pixel inconnu");
				if (src.width == 0u || src.height == 0u)
					return Refus(err, "dimensions nulles");
				if (src.levels.Size() == 0u)
					return Refus(err, "aucun niveau a ecrire");
				if (src.arrayLayers == 0u)
					return Refus(err, "arrayLayers nul");

				const nk_uint32 bpp = NkTexFormatOctetsParPixel(src.formatCode);
				if (bpp == 0u && !NkTexFormatEstCompresse(src.formatCode))
					return Refus(err, "format non compresse sans taille de pixel connue");
				// Les formats par blocs sont ACCEPTES depuis le 2026-09-05 : le
				// RHI sait les televerser (arithmetique de blocs) et le four sait
				// produire du BC1. Ceux qui ne sont pas encore encodables sont
				// refuses par le FOUR, qui seul sait ce qu'il sait faire — pas ici,
				// ou le refus porterait sur un fichier qu'on nous demande d'ecrire
				// et qui pourrait venir d'ailleurs.
				if (NkTexFormatEstCompresse(src.formatCode) && bpp != 0u)
					return Refus(err, "format par blocs annonce avec un octet-par-pixel : incoherent");

				const nk_size nNiveaux = src.levels.Size();
				if (nNiveaux % nk_size(src.arrayLayers) != 0u)
					return Refus(err, "le nombre de niveaux n'est pas un multiple du nombre de couches");
				const nk_uint32 mipCount = nk_uint32(nNiveaux / nk_size(src.arrayLayers));

				// ── Disposition ──
				const nk_uint32 headerSize = kNkTexHeaderSizeV1 + nk_uint32(src.enTeteInconnu.Size());
				const nk_uint32 levelTableOffset = headerSize;
				const nk_uint32 tableSize = kNkTexLevelEntrySizeV1 * nk_uint32(nNiveaux);
				const nk_uint32 dataOffset = levelTableOffset + tableSize;

				// ── Verification et positions des niveaux ──
				NkVector<nk_uint32> offsets;
				NkVector<nk_uint32> tailles;
				NkVector<nk_uint32> pas;
				nk_uint32 curseur = 0u;
				for (nk_size i = 0; i < nNiveaux; ++i) {
					const NkTexNiveauSource &n = src.levels[i];
					if (n.width == 0u || n.height == 0u || !n.data)
						return Refus(err, "niveau vide ou sans donnees");
					const nk_uint32 rowPitch = n.rowPitch ? n.rowPitch : (n.width * bpp);
					// Sur un format par blocs, le nombre de RANGEES est celui des
					// blocs : une texture de 4 pixels de haut fait UNE rangee.
					const nk_uint32 rangees =
						NkTexFormatEstCompresse(src.formatCode) ? ((n.height + 3u) / 4u) : n.height;
					const nk_uint32 taille = n.size ? n.size : (rowPitch * rangees * (n.depth ? n.depth : 1u));
					if (taille == 0u)
						return Refus(err, "taille de niveau nulle");
					pas.PushBack(rowPitch);
					offsets.PushBack(curseur);
					tailles.PushBack(taille);
					curseur += taille;
				}

				out.Reserve(nk_size(dataOffset) + nk_size(curseur));

				// ── En-tete (64 octets + queue inconnue) ──
				for (int i = 0; i < 8; ++i)
					out.PushBack(nk_uint8(kNkTexMagic[i]));
				nktexdetail::EcrireU16(out, nk_uint16(kNkTexPayloadVersion));
				nktexdetail::EcrireU16(out, nk_uint16(headerSize));
				nktexdetail::EcrireU32(out, src.formatCode);
				nktexdetail::EcrireU32(out, src.width);
				nktexdetail::EcrireU32(out, src.height);
				nktexdetail::EcrireU32(out, src.depth ? src.depth : 1u);
				nktexdetail::EcrireU32(out, src.arrayLayers);
				nktexdetail::EcrireU32(out, mipCount);
				nktexdetail::EcrireU32(out, src.flags);
				nktexdetail::EcrireU32(out, src.addressMode);
				nktexdetail::EcrireU32(out, src.filterMode);
				nktexdetail::EcrireU32(out, src.rowAlignment ? src.rowAlignment : 1u);
				nktexdetail::EcrireU32(out, kNkTexLevelEntrySizeV1);
				nktexdetail::EcrireU32(out, levelTableOffset);
				nktexdetail::EcrireU32(out, dataOffset);
				nktexdetail::EcrireU32(out, curseur); // octets de pixels
				nktexdetail::EcrireU32(out, 0u);	  // reserve v1

				// Le compte, verifie et non suppose : si la suite d'ecritures
				// ci-dessus change sans que `kNkTexHeaderSizeV1` suive, on refuse
				// ici plutot que de produire un fichier que le decodeur lira de
				// travers (defaut paye une fois, le 2026-09-05).
				if (out.Size() != nk_size(kNkTexHeaderSizeV1))
					return Refus(err, "en-tete ecrit d'une taille differente de kNkTexHeaderSizeV1");

				for (nk_size i = 0; i < src.enTeteInconnu.Size(); ++i)
					out.PushBack(src.enTeteInconnu[i]);

				// ── Table des niveaux ──
				for (nk_size i = 0; i < nNiveaux; ++i) {
					const NkTexNiveauSource &n = src.levels[i];
					const nk_uint32 d = n.depth ? n.depth : 1u;
					const nk_uint32 slice = n.slicePitch ? n.slicePitch : (pas[i] * n.height);
					nktexdetail::EcrireU32(out, n.width);
					nktexdetail::EcrireU32(out, n.height);
					nktexdetail::EcrireU32(out, d);
					nktexdetail::EcrireU32(out, pas[i]);
					nktexdetail::EcrireU32(out, slice);
					nktexdetail::EcrireU32(out, offsets[i]);
					nktexdetail::EcrireU32(out, tailles[i]);
					nktexdetail::EcrireU32(out, 0u); // reserve
					// 8 * 4 = 32
				}

				// ── Pixels ──
				for (nk_size i = 0; i < nNiveaux; ++i) {
					const nk_uint8 *p = src.levels[i].data;
					for (nk_uint32 k = 0; k < tailles[i]; ++k)
						out.PushBack(p[k]);
				}
				return true;
			}

			// ── Decodage ─────────────────────────────────────────────────────
			// `data` doit rester vivant tant que `out` est utilise : les niveaux
			// sont des FENETRES, il n'y a aucune copie de pixels.
			[[nodiscard]] static nk_bool Decode(const nk_uint8 *data, nk_size size, NkTexVue &out,
												NkString *err = nullptr) noexcept {
				out.levels.Clear();
				out.enTeteInconnu.Clear();

				if (!data)
					return Refus(err, "payload nul");
				if (size < nk_size(kNkTexHeaderSizeV1))
					return Refus(err, "payload tronque : en-tete incomplet");
				if (memcmp(data, kNkTexMagic, 8u) != 0)
					return Refus(err, "magie absente : ce payload n'est pas une texture cuite");

				const nk_uint32 version = nktexdetail::LireU16(data + 8);
				const nk_uint32 headerSize = nktexdetail::LireU16(data + 10);
				if (version == 0u)
					return Refus(err, "version nulle");
				if (headerSize < kNkTexHeaderSizeV1)
					return Refus(err, "en-tete plus court que la version 1 : illisible");
				if (nk_size(headerSize) > size)
					return Refus(err, "payload tronque : en-tete annonce plus long que le fichier");

				out.version = version;
				out.formatCode = nktexdetail::LireU32(data + 12);
				out.width = nktexdetail::LireU32(data + 16);
				out.height = nktexdetail::LireU32(data + 20);
				out.depth = nktexdetail::LireU32(data + 24);
				out.arrayLayers = nktexdetail::LireU32(data + 28);
				out.mipCount = nktexdetail::LireU32(data + 32);
				out.flags = nktexdetail::LireU32(data + 36);
				out.addressMode = nktexdetail::LireU32(data + 40);
				out.filterMode = nktexdetail::LireU32(data + 44);
				out.rowAlignment = nktexdetail::LireU32(data + 48);
				const nk_uint32 levelEntrySize = nktexdetail::LireU32(data + 52);
				const nk_uint32 levelTableOffset = nktexdetail::LireU32(data + 56);
				const nk_uint32 dataOffset = nktexdetail::LireU32(data + 60);

				// La queue d'en-tete que CETTE version ne comprend pas est
				// conservee telle quelle : `Encode` la remettra.
				for (nk_uint32 i = kNkTexHeaderSizeV1; i < headerSize; ++i)
					out.enTeteInconnu.PushBack(data[i]);

				if (out.width == 0u || out.height == 0u)
					return Refus(err, "dimensions nulles");
				if (out.mipCount == 0u || out.arrayLayers == 0u)
					return Refus(err, "aucun niveau annonce");
				if (levelEntrySize < kNkTexLevelEntrySizeV1)
					return Refus(err, "entree de table plus courte que la version 1 : illisible");

				const nk_uint64 nNiveaux = nk_uint64(out.mipCount) * nk_uint64(out.arrayLayers);
				const nk_uint64 finTable = nk_uint64(levelTableOffset) + nNiveaux * nk_uint64(levelEntrySize);
				if (levelTableOffset < headerSize || finTable > nk_uint64(size))
					return Refus(err, "payload tronque : table des niveaux hors du fichier");
				if (nk_size(dataOffset) > size)
					return Refus(err, "payload tronque : les pixels commencent hors du fichier");

				for (nk_uint64 i = 0; i < nNiveaux; ++i) {
					const nk_uint8 *e = data + levelTableOffset + i * levelEntrySize;
					NkTexNiveauVue n;
					n.width = nktexdetail::LireU32(e + 0);
					n.height = nktexdetail::LireU32(e + 4);
					n.depth = nktexdetail::LireU32(e + 8);
					n.rowPitch = nktexdetail::LireU32(e + 12);
					n.slicePitch = nktexdetail::LireU32(e + 16);
					const nk_uint32 off = nktexdetail::LireU32(e + 20);
					n.size = nktexdetail::LireU32(e + 24);
					// e + 28 : reserve dans la v1, ignore ici, mais l'entree
					// entiere est sautee par `levelEntrySize` — c'est ce qui rend
					// la table additive.

					const nk_uint64 fin = nk_uint64(dataOffset) + nk_uint64(off) + nk_uint64(n.size);
					if (n.size == 0u || fin > nk_uint64(size))
						return Refus(err, "payload tronque : un niveau deborde du fichier");
					n.data = data + dataOffset + off;
					out.levels.PushBack(n);
				}
				return true;
			}

			// Somme des octets de pixels d'une vue — le « combien de Mo » sans
			// avoir a reparcourir le fichier.
			[[nodiscard]] static nk_uint64 OctetsPixels(const NkTexVue &v) noexcept {
				nk_uint64 s = 0u;
				for (nk_size i = 0; i < v.levels.Size(); ++i)
					s += v.levels[i].size;
				return s;
			}

		private:
			static nk_bool Refus(NkString *err, const char *raison) noexcept {
				if (err)
					*err = NkString(raison);
				return false;
			}
	};

	// =========================================================================
	// Ecrire un actif texture CUIT — conteneur compris
	//
	// Vit ICI et pas dans NKRenderer parce que rien de ce geste n'a besoin d'un
	// GPU : c'est un payload d'octets et un `NkAssetFileHeader`. Le four (un
	// outil de ligne de commande) et le moteur appellent donc la MEME fonction,
	// et `NkTextureAssetIO::SaveBaked` n'est plus qu'un renvoi.
	// =========================================================================
	[[nodiscard]] inline nk_bool NkEcrireActifTexture(const nk_uint8 *payload, nk_size payloadSize,
													  const char *cheminDisque, NkStringView cheminLogique,
													  NkStringView cheminSource = NkStringView(),
													  NkAssetId *outId = nullptr, NkString *err = nullptr) noexcept {
		if (!payload || payloadSize == 0u) {
			if (err)
				*err = NkString("payload vide");
			return false;
		}
		if (!cheminDisque) {
			if (err)
				*err = NkString("chemin de sortie nul");
			return false;
		}

		// Relire son propre payload AVANT d'ecrire : refuser ici vaut mieux que
		// produire un fichier que le chargeur refusera loin d'ici.
		NkTexVue controle;
		if (!NkTexturePayload::Decode(payload, payloadSize, controle, err))
			return false;

		NkAssetMetadata meta;
		meta.id = NkAssetId::FromName(cheminLogique);
		meta.type = controle.EstCubemap() ? NkAssetType::TextureCube : NkAssetType::Texture2D;
		meta.typeName = NkString(NkAssetTypeName(meta.type));
		meta.assetPath = NkAssetPath(cheminLogique);
		meta.assetVersion = 1u;
		meta.AddTag(NkAssetTypeName(meta.type));
		meta.AddTag("cuit");
		// L'original n'est garde que pour proposer une RECUISSON s'il change.
		// Le chargement ne l'ouvre jamais.
		meta.sourceFilePath = NkString(cheminSource);

		if (!NkAssetIO::Write(cheminDisque, meta, payload, payloadSize, err))
			return false;

		NkAssetRecord rec;
		rec.id = meta.id;
		rec.assetPath = meta.assetPath;
		rec.type = meta.type;
		rec.typeName = meta.typeName;
		rec.diskPath = NkString(cheminDisque);
		NkAssetRegistry::Global().Register(rec);
		if (outId)
			*outId = meta.id;
		return true;
	}

} // namespace nkentseu

#endif // NKENTSEU_SERIALIZATION_ASSET_NKTEXTUREASSETFORMAT_H
