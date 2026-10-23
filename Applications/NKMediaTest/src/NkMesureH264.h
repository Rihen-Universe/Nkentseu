// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#pragma once
// =============================================================================
// NkMesureH264.h — LE COUT PAR IMAGE DE L'ENCODEUR H.264, SUR UNE SOURCE QUI CHANGE EN ENTIER (23/10).
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
//
// Le coordinateur, avant les cameras du direct : « Une camera change TOUTE l'image a chaque trame. Mesure
// d'abord le cout par image de l'encodeur, sur une source de synthese qui change en entier a 15 i/s en 720p.
// Trouve ou part le temps, puis fais-lui tenir le temps reel, sans perdre en qualite de facon mesurable. »
//
//   NKMediaTest --mesure-h264 <dossier> [images=45] [qp=26] [sorties=0|1]
//
// La source imite une camera : une scene texturee (degrades, motifs fins, aretes nettes) qui defile de
// 3 px x 2 px par image, un disque qui traverse plus vite, un scintillement de luminosite et un bruit de
// capteur de +-3 sur chaque pixel : AUCUN macrobloc n'est identique d'une image a l'autre. 1280x720, RGBA
// (ce que le direct pousse), 15 i/s, QP 26 et une IDR toutes les 30 images (les reglages du direct).
// Elle est encodee DEUX fois avec la meme source : la recherche de mouvement du direct (rapide), puis la
// recherche EXHAUSTIVE +-16 d'avant le 23/10, gardee comme reference de qualite. Le script
// (tools/banc_h264_vitesse.sh) juge la qualite par un decodeur qui n'est pas le notre (ffmpeg).
// sorties=1 : <dossier>/source.rgba, <dossier>/rapide.h264 + rapide_recon.yuv, <dossier>/exhaustive.h264.
// Contre-epreuves (NK_NKMEDIA_MUTATION) : h264-recherche-exhaustive (le direct reprend la recherche d'avant, en
// tranches : la taille et le temps montent), h264-une-tranche (une seule tranche, pas de parallele : le temps reel
// tombe), h264-recherche-zero (la recherche rapide n'essaie que le vecteur nul : la taille explose), h264-plans-arrondi
// (un plan demi-pel faux : la reconstruction de l'encodeur quitte celle du decodeur).
// =============================================================================
#include "NKMedia/Codecs/Video/H264/NkH264Encoder.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/Text/NkSortie.h"
#include "NKFileSystem/NkFile.h"
#include "NKMath/NkFunctions.h"
#include "NKPlatform/NkEnv.h"
#include "NKTime/NkChrono.h"

namespace nkentseu {

	inline int32 NkMesureEntier(const char *s, int32 defaut) {
		if (!s || !*s)
			return defaut;
		int32 n = 0;
		for (const char *q = s; *q >= '0' && *q <= '9'; ++q)
			n = n * 10 + (*q - '0');
		return n;
	}

	/// La scene, plus grande que l'image (la camera y defile). Calculee une fois.
	struct NkMesureScene {
			static constexpr int32 W = 1280, H = 720, M = 160; ///< marge de defilement de chaque cote
			int32 sw = W + 2 * M, sh = H + 2 * M;
			NkVector<uint8> rgb;

			void Construire() {
				rgb.Resize((usize)sw * sh * 3);
				// des rectangles a aretes nettes (des objets), places par un generateur fixe
				uint32 g = 12345u;
				auto alea = [&g]() {
					g = g * 1664525u + 1013904223u;
					return g >> 8;
				};

				struct Rect {
						int32 x, y, w, h;
						uint8 c[3];
				} rects[48];

				for (Rect &r : rects) {
					r.x = (int32)(alea() % (uint32)sw);
					r.y = (int32)(alea() % (uint32)sh);
					r.w = 20 + (int32)(alea() % 140u);
					r.h = 20 + (int32)(alea() % 100u);
					for (int32 c = 0; c < 3; ++c)
						r.c[c] = (uint8)(alea() & 255u);
				}
				for (int32 y = 0; y < sh; ++y)
					for (int32 x = 0; x < sw; ++x) {
						float32 v[3];
						for (int32 c = 0; c < 3; ++c) {
							const float32 fx = (float32)x, fy = (float32)y, fc = (float32)c;
							v[c] = 120.f + 45.f * math::NkSin(0.013f * fx + 0.009f * fy + fc) +
								   25.f * math::NkSin(0.071f * fx - 0.043f * fy + 2.f * fc) +
								   10.f * math::NkSin(0.61f * fx) * math::NkSin(0.47f * fy + fc); // detail fin
						}
						for (const Rect &r : rects)
							if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h)
								for (int32 c = 0; c < 3; ++c)
									v[c] = 0.35f * v[c] + 0.65f * (float32)r.c[c];
						uint8 *p = &rgb[((usize)y * sw + x) * 3];
						for (int32 c = 0; c < 3; ++c)
							p[c] = (uint8)(v[c] < 0.f ? 0.f : (v[c] > 255.f ? 255.f : v[c]));
					}
			}

			/// L'image k, en RGBA : le cadre defile, un disque traverse, la luminosite scintille, le capteur bruite.
			void Image(int32 k, uint8 *rgba, uint32 &bruit) const {
				const int32 ox = 8 + 3 * k, oy = 8 + 2 * k; // 45 images : 143 et 98 < 2*M
				const int32 cx = 100 + 19 * k, cy = 360 + (int32)(120.f * math::NkSin(0.2f * (float32)k)), r2 = 70 * 70;
				const int32 lum = ((k * 7) % 5) - 2;
				for (int32 y = 0; y < H; ++y) {
					const uint8 *s = &rgb[((usize)(y + oy) * sw + ox) * 3];
					uint8 *d = &rgba[(usize)y * W * 4];
					for (int32 x = 0; x < W; ++x, s += 3, d += 4) {
						const bool disque = (x - cx) * (x - cx) + (y - cy) * (y - cy) < r2;
						for (int32 c = 0; c < 3; ++c) {
							bruit = bruit * 1664525u + 1013904223u;
							int32 v = disque ? (c == 0 ? 230 - ((x + y) & 31) : 40 + (x & 15)) : (int32)s[c];
							v += lum + (int32)((bruit >> 24) % 7u) - 3;
							d[c] = (uint8)(v < 0 ? 0 : (v > 255 ? 255 : v));
						}
						d[3] = 255;
					}
				}
			}
	};

	struct NkMesureResultat {
			float64 moyI = 0, moyP = 0, maxP = 0, moyTout = 0, p95 = 0;
			int64 octets = 0;
			media::NkH264Encoder::Profil profil;
			bool ok = false;
	};

	inline NkMesureResultat NkMesureUnePasse(const NkMesureScene &scene, const char *chemin, const char *recon,
											 NkFile *source, int32 images, int32 qp, bool exhaustive,
											 bool rechercheZero, bool plansArrondi, int32 tranches) {
		NkMesureResultat R;
		media::NkH264Encoder enc;
		if (!enc.Open(chemin, NkMesureScene::W, NkMesureScene::H, 15, 1, qp, 30))
			return R;
		enc.rechercheExhaustive = exhaustive;
		enc.mutationRechercheZero = rechercheZero;
		enc.mutationPlansArrondi = plansArrondi;
		enc.tranches = tranches;
		enc.ActiverProfil(true);
		if (recon)
			(void)enc.EnableReconDump(recon);
		NkVector<uint8> rgba;
		rgba.Resize((usize)NkMesureScene::W * NkMesureScene::H * 4);
		NkVector<float64> ms;
		uint32 bruit = 777u;
		for (int32 k = 0; k < images; ++k) {
			scene.Image(k, rgba.Data(), bruit);
			if (source)
				(void)source->Write(rgba.Data(), rgba.Size());
			const float64 t0 = NkChrono::Now().milliseconds;
			if (!enc.WriteFrame(rgba.Data(), media::NkVideoInputFormat::RGBA32))
				return R;
			ms.PushBack(NkChrono::Now().milliseconds - t0);
		}
		R.profil = enc.LireProfil();
		enc.Close();
		float64 sI = 0, sP = 0, s = 0;
		int32 nI = 0, nP = 0;
		for (int32 k = 0; k < images; ++k) {
			const bool idr = (k % 30) == 0;
			(idr ? sI : sP) += ms[(usize)k];
			++(idr ? nI : nP);
			s += ms[(usize)k];
			if (!idr && ms[(usize)k] > R.maxP)
				R.maxP = ms[(usize)k];
		}
		// le 95e centile (tri par insertion : 45 valeurs)
		NkVector<float64> t = ms;
		for (usize i = 1; i < t.Size(); ++i)
			for (usize j = i; j > 0 && t[j - 1] > t[j]; --j) {
				const float64 x = t[j];
				t[j] = t[j - 1];
				t[j - 1] = x;
			}
		R.p95 = t[(usize)((t.Size() - 1) * 95 / 100)];
		R.moyI = nI ? sI / nI : 0.0;
		R.moyP = nP ? sP / nP : 0.0;
		R.moyTout = images ? s / images : 0.0;
		NkFile f;
		if (f.Open(chemin, NkFileMode::NK_READ_BINARY)) {
			R.octets = (int64)f.GetSize();
			f.Close();
		}
		R.ok = true;
		return R;
	}

	inline void NkMesureDire(const char *nom, const NkMesureResultat &R, int32 images) {
		const media::NkH264Encoder::Profil &p = R.profil;
		const float64 nP = p.imagesP > 0 ? (float64)p.imagesP : 1.0;
		NkPrintfSortie(
			"[mesure-h264] %s : par image I %.1f ms, P %.1f ms (max %.1f), toutes %.1f ms, 95e centile %.1f ms\n", nom,
			R.moyI, R.moyP, R.maxP, R.moyTout, R.p95);
		NkPrintfSortie(
			"[mesure-h264] %s : par image P -> conversion %.1f, plans %.1f, essais P_Skip %.1f, mouvement %.1f, "
			"residu %.1f, deblocage %.1f ms\n",
			nom, p.conversionMs / (float64)images, p.plansMs / nP, p.sautMs / nP, p.mouvementMs / nP, p.residuMs / nP,
			p.deblocageMs / (float64)images);
		const int64 mb = p.mbCodes + p.mbSautes;
		NkPrintfSortie(
			"[mesure-h264] %s : macroblocs P codes %lld / %lld, positions entieres par macrobloc code %.0f ; "
			"%lld octets = %.0f kbit/s a 15 i/s\n",
			nom, (long long)p.mbCodes, (long long)mb,
			p.mbCodes ? (float64)p.positionsEntieres / (float64)p.mbCodes : 0.0, (long long)R.octets,
			(float64)R.octets * 8.0 * 15.0 / (float64)images / 1000.0);
	}

	inline int NkMesureH264(int argc, char **argv) {
		const NkString dossier(argc >= 3 ? argv[2] : ".");
		const int32 images = NkMesureEntier(argc >= 4 ? argv[3] : nullptr, 45);
		const int32 qp = NkMesureEntier(argc >= 5 ? argv[4] : nullptr, 26);
		const bool sorties = argc >= 6 && argv[5][0] == '1';
		if (images < 2 || images > 45) {
			NkPrintfSortie("[mesure-h264] ECHEC D'INSTRUMENT : 2 a 45 images (la scene a sa marge)\n");
			return 2;
		}
		const char *mut = env::GetEnvVar("NK_NKMEDIA_MUTATION");
		const NkString mutation(mut ? mut : "");
		NkMesureScene scene;
		const float64 t0 = NkChrono::Now().milliseconds;
		scene.Construire();
		NkPrintfSortie(
			"[mesure-h264] source : 1280x720 RGBA, 15 i/s, %d images, QP %d, IDR toutes les 30 ; chaque image "
			"change en entier (defilement 3x2 px, disque, scintillement, bruit +-3) ; scene en %.0f ms\n",
			images, qp, NkChrono::Now().milliseconds - t0);
		NkFile src;
		const bool ecrireSource = sorties && src.Open((dossier + "/source.rgba").CStr(), NkFileMode::NK_WRITE_BINARY);
		const NkString recon = dossier + "/rapide_recon.yuv";
		// 1) le direct : la recherche rapide (ou, en contre-epreuve, celle d'avant)
		const NkMesureResultat rapide = NkMesureUnePasse(
			scene, (dossier + "/rapide.h264").CStr(), sorties ? recon.CStr() : nullptr, ecrireSource ? &src : nullptr,
			images, qp, mutation == NkString("h264-recherche-exhaustive"), mutation == NkString("h264-recherche-zero"),
			mutation == NkString("h264-plans-arrondi"), mutation == NkString("h264-une-tranche") ? 1 : 0);
		if (ecrireSource)
			src.Close();
		// 2) la reference de qualite : la recherche exhaustive +-16 d'avant le 23/10
		const NkMesureResultat exh = NkMesureUnePasse(scene, (dossier + "/exhaustive.h264").CStr(), nullptr, nullptr,
													  images, qp, true, false, false, 1);
		if (!rapide.ok || !exh.ok) {
			NkPrintfSortie("[mesure-h264] ECHEC D'INSTRUMENT : l'encodeur n'a pas ouvert ou refuse une image\n");
			return 2;
		}
		NkMesureDire("direct", rapide, images);
		NkMesureDire("exhaustive", exh, images);
		const float64 budget = 1000.0 / 15.0;
		NkPrintfSortie("[mesure-h264] budget du temps reel : %.1f ms par image ; direct %.1f ms en moyenne (%.0f %%), "
					   "95e centile %.1f ms ; exhaustive %.1f ms (%.0f %%)\n",
					   budget, rapide.moyTout, 100.0 * rapide.moyTout / budget, rapide.p95, exh.moyTout,
					   100.0 * exh.moyTout / budget);
		NkPrintfSortie("[mesure-h264] taille : direct %lld octets, exhaustive %lld octets (rapport %.4f)\n",
					   (long long)rapide.octets, (long long)exh.octets,
					   exh.octets ? (float64)rapide.octets / (float64)exh.octets : 0.0);
		NkFflush(NkFlux::Sortie);
		return 0;
	}

} // namespace nkentseu
