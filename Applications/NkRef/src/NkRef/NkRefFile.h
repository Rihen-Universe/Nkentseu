#pragma once
// ============================================================================
// NkRefFile.h — le fichier .nkref : la planche ENTIÈRE dans un seul fichier.
// ----------------------------------------------------------------------------
// Règle de la mission : les images sont EMBARQUÉES (les octets du fichier
// source, tels quels — PNG/JPEG intacts), jamais des chemins qui meurent au
// premier déplacement de dossier. Une planche doit se rouvrir sur une autre
// machine. Corollaire heureux : le plafond d'affichage 4096 px ne perd RIEN,
// les octets d'origine restent dans le fichier.
//
// Format binaire NKRF v1, little-endian, ordre d'écriture FIXE : deux
// enregistrements successifs de la même planche produisent des OCTETS
// IDENTIQUES (le test de round-trip qui a validé les scènes du modeleur).
//
//   'N''K''R''F'  u32 version=1
//   f32 view.cx, view.cy, view.zoom   u8 themeSombre  u8 pad[3]
//   u32 nItems
//     par item : f32 posX posY scale rotationDeg opacity
//                u8 mirrorX mirrorY pad pad
//                u32 texW texH        (taille AFFICHÉE, post-réduction)
//                u32 srcLen + octets  (le fichier image D'ORIGINE, intact ;
//                                      collage presse-papiers = PNG encodé)
//   u32 nStrokes
//     par trait : u8 r g b a   f32 widthWorld   u32 nPoints + f32 x,y…
// ============================================================================

#include "NKContainers/Sequential/NkVector.h"
#include "NkRef/NkRefBoard.h"
#include "NkRef/NkRefView.h"

namespace nkref {

	using nkentseu::uint8;
	using nkentseu::uint32;

	// Les octets source de chaque image vivent CÔTÉ GLUE (parallèles aux
	// textures GPU) : les mettre dans NkRefItem ferait copier des mégaoctets à
	// chaque réordonnancement du modèle pur.
	struct NkRefFileImage {
			const uint8 *data = nullptr;
			usize size = 0;
	};

	namespace detail {
		inline void PutBytes(NkVector<uint8> &out, const void *p, usize n) {
			const uint8 *b = static_cast<const uint8 *>(p);
			for (usize i = 0; i < n; ++i)
				out.PushBack(b[i]);
		}
		inline void PutU32(NkVector<uint8> &out, uint32 v) {
			PutBytes(out, &v, 4);
		}
		inline void PutF32(NkVector<uint8> &out, float32 v) {
			PutBytes(out, &v, 4);
		}
		inline void PutU8(NkVector<uint8> &out, uint8 v) {
			out.PushBack(v);
		}

		struct Reader {
				const uint8 *p = nullptr;
				usize n = 0, at = 0;
				bool ok = true;

				bool Get(void *dst, usize len) {
					if (!ok || at + len > n) {
						ok = false;
						return false;
					}
					uint8 *d = static_cast<uint8 *>(dst);
					for (usize i = 0; i < len; ++i)
						d[i] = p[at + i];
					at += len;
					return true;
				}
				uint32 U32() {
					uint32 v = 0;
					Get(&v, 4);
					return v;
				}
				float32 F32() {
					float32 v = 0;
					Get(&v, 4);
					return v;
				}
				uint8 U8() {
					uint8 v = 0;
					Get(&v, 1);
					return v;
				}
		};
	} // namespace detail

	/// Sérialise la planche complète. `images[i]` = octets source de
	/// `board.items[i]` (MÊME index, la glue les tient alignés).
	inline void NkRefSerialize(NkVector<uint8> &out, const NkRefBoard &board, const NkRefFileImage *images,
							   const NkRefView &view, bool darkTheme) {
		using namespace detail;
		out.Clear();
		PutU8(out, 'N');
		PutU8(out, 'K');
		PutU8(out, 'R');
		PutU8(out, 'F');
		PutU32(out, 1u); // version
		PutF32(out, view.center.x);
		PutF32(out, view.center.y);
		PutF32(out, view.zoom);
		PutU8(out, darkTheme ? 1 : 0);
		PutU8(out, 0);
		PutU8(out, 0);
		PutU8(out, 0);
		PutU32(out, (uint32)board.items.Size());
		for (usize i = 0; i < board.items.Size(); ++i) {
			const NkRefItem &it = board.items[i];
			PutF32(out, it.pos.x);
			PutF32(out, it.pos.y);
			PutF32(out, it.scale);
			PutF32(out, it.rotationDeg);
			PutF32(out, it.opacity);
			PutU8(out, it.mirrorX ? 1 : 0);
			PutU8(out, it.mirrorY ? 1 : 0);
			PutU8(out, 0);
			PutU8(out, 0);
			PutU32(out, it.texW);
			PutU32(out, it.texH);
			PutU32(out, (uint32)images[i].size);
			PutBytes(out, images[i].data, images[i].size);
		}
		PutU32(out, (uint32)board.strokes.Size());
		for (usize s = 0; s < board.strokes.Size(); ++s) {
			const NkRefStroke &st = board.strokes[s];
			PutU8(out, st.r);
			PutU8(out, st.g);
			PutU8(out, st.b);
			PutU8(out, st.a);
			PutF32(out, st.widthWorld);
			PutU32(out, (uint32)st.points.Size());
			for (usize p = 0; p < st.points.Size(); ++p) {
				PutF32(out, st.points[p].x);
				PutF32(out, st.points[p].y);
			}
		}
	}

	/// Item désérialisé : le modèle + ses octets source (à re-décoder en
	/// texture par la glue).
	struct NkRefLoadedItem {
			NkRefItem item;
			NkVector<uint8> src;
	};

	/// Désérialise. Retourne false si le fichier n'est pas un NKRF lisible —
	/// dans ce cas, ne rien toucher à la planche courante.
	inline bool NkRefDeserialize(const uint8 *data, usize size, NkVector<NkRefLoadedItem> &outItems,
								 NkVector<NkRefStroke> &outStrokes, NkRefView &outView, bool &outDark) {
		using namespace detail;
		Reader r{data, size, 0, true};
		uint8 m0 = r.U8(), m1 = r.U8(), m2 = r.U8(), m3 = r.U8();
		if (!r.ok || m0 != 'N' || m1 != 'K' || m2 != 'R' || m3 != 'F')
			return false;
		const uint32 version = r.U32();
		if (!r.ok || version != 1u)
			return false;
		outView.center.x = r.F32();
		outView.center.y = r.F32();
		outView.zoom = r.F32();
		outDark = r.U8() != 0;
		r.U8();
		r.U8();
		r.U8();
		const uint32 nItems = r.U32();
		if (!r.ok || nItems > 100000u)
			return false;
		outItems.Clear();
		for (uint32 i = 0; i < nItems && r.ok; ++i) {
			NkRefLoadedItem li;
			li.item.pos.x = r.F32();
			li.item.pos.y = r.F32();
			li.item.scale = r.F32();
			li.item.rotationDeg = r.F32();
			li.item.opacity = r.F32();
			li.item.mirrorX = r.U8() != 0;
			li.item.mirrorY = r.U8() != 0;
			r.U8();
			r.U8();
			li.item.texW = r.U32();
			li.item.texH = r.U32();
			const uint32 len = r.U32();
			if (!r.ok || r.at + len > r.n)
				return false;
			li.src.Resize(len);
			r.Get(li.src.Data(), len);
			outItems.PushBack(nkentseu::traits::NkMove(li));
		}
		const uint32 nStrokes = r.U32();
		if (!r.ok || nStrokes > 1000000u)
			return false;
		outStrokes.Clear();
		for (uint32 s = 0; s < nStrokes && r.ok; ++s) {
			NkRefStroke st;
			st.r = r.U8();
			st.g = r.U8();
			st.b = r.U8();
			st.a = r.U8();
			st.widthWorld = r.F32();
			const uint32 np = r.U32();
			if (!r.ok || r.at + (usize)np * 8u > r.n)
				return false;
			for (uint32 p = 0; p < np; ++p) {
				NkVec2f pt;
				pt.x = r.F32();
				pt.y = r.F32();
				st.points.PushBack(pt);
			}
			outStrokes.PushBack(nkentseu::traits::NkMove(st));
		}
		return r.ok;
	}

} // namespace nkref
