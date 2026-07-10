// =============================================================================
// NKVideoTest — création vidéo from-scratch (SANS ffmpeg).
// Génère des trames animées (dégradé + disque qui rebondit) et écrit :
//   - un .avi MJPEG (compressé, universel)
//   - un .avi RAW BGR (non compressé)
// Vérifie ensuite que les fichiers sont bien formés (RIFF/AVI + trames).
// =============================================================================
#include "NKMedia/Video/NkVideoWriter.h"
#include "NKMemory/NKMemory.h"

#include <cstdio>
#include <cmath>

using namespace nkentseu;

namespace {
	// Remplit un buffer RGB24 (top-down) : dégradé animé + disque qui rebondit.
	void RenderFrame(uint8 *rgb, int32 w, int32 h, int32 frame, int32 nframes) {
		const float t = (float)frame / (float)nframes;
		// Disque : position rebondissante.
		const float px = (0.5f + 0.4f * ::sinf(t * 6.2831853f)) * (float)w;
		const float py = (0.5f + 0.4f * ::sinf(t * 6.2831853f * 1.7f)) * (float)h;
		const float rad = 0.12f * (float)(w < h ? w : h);
		const float rad2 = rad * rad;
		for (int32 y = 0; y < h; ++y) {
			for (int32 x = 0; x < w; ++x) {
				uint8 *p = rgb + ((usize)y * w + x) * 3;
				// Fond : dégradé diagonal qui défile avec t.
				float u = (float)x / (float)w, v = (float)y / (float)h;
				int32 r = (int32)(255.0f * (0.5f + 0.5f * ::sinf((u + t) * 6.2831853f)));
				int32 g = (int32)(255.0f * (0.5f + 0.5f * ::sinf((v + t * 0.7f) * 6.2831853f)));
				int32 b = (int32)(255.0f * (0.5f + 0.5f * ::sinf((u + v + t * 1.3f) * 6.2831853f)));
				// Disque blanc-jaune.
				const float dx = (float)x - px, dy = (float)y - py;
				if (dx * dx + dy * dy < rad2) {
					r = 255;
					g = 240;
					b = 60;
				}
				p[0] = (uint8)(r < 0 ? 0 : r > 255 ? 255 : r);
				p[1] = (uint8)(g < 0 ? 0 : g > 255 ? 255 : g);
				p[2] = (uint8)(b < 0 ? 0 : b > 255 ? 255 : b);
			}
		}
	}

	// Vérifie qu'un fichier commence par RIFF....AVI et contient un idx1.
	bool VerifyAvi(const char *path, int32 &outFrames) {
		FILE *f = ::fopen(path, "rb");
		if (!f)
			return false;
		::fseek(f, 0, SEEK_END);
		long sz = ::ftell(f);
		::fseek(f, 0, SEEK_SET);
		if (sz < 64) {
			::fclose(f);
			return false;
		}
		uint8 *buf = (uint8 *)memory::NkAlloc((usize)sz);
		::fread(buf, 1, (usize)sz, f);
		::fclose(f);
		bool ok = buf[0] == 'R' && buf[1] == 'I' && buf[2] == 'F' && buf[3] == 'F' && buf[8] == 'A' && buf[9] == 'V' &&
				  buf[10] == 'I' && buf[11] == ' ';
		// L'index idx1 donne le nombre exact de trames (taille idx1 / 16).
		int32 nchunks = 0;
		bool hasIdx = false;
		for (long i = 0; i + 8 <= sz; ++i) {
			if (buf[i] == 'i' && buf[i + 1] == 'd' && buf[i + 2] == 'x' && buf[i + 3] == '1') {
				hasIdx = true;
				const uint32 idxSize = (uint32)buf[i + 4] | ((uint32)buf[i + 5] << 8) | ((uint32)buf[i + 6] << 16) |
									   ((uint32)buf[i + 7] << 24);
				nchunks = (int32)(idxSize / 16);
				break;
			}
		}
		outFrames = nchunks;
		memory::NkFree(buf);
		return ok && hasIdx;
	}

	bool MakeVideo(const char *path, media::NkVideoCodec codec, media::NkVideoContainer container, int32 w, int32 h,
				   int32 fps, int32 nframes) {
		media::NkVideoConfig cfg;
		cfg.width = w;
		cfg.height = h;
		cfg.fpsNum = fps;
		cfg.fpsDen = 1;
		cfg.codec = codec;
		cfg.container = container;
		cfg.quality = 88;

		media::NkVideoWriter vw;
		if (!vw.Open(path, cfg)) {
			::printf("  [ERREUR] ouverture %s\n", path);
			return false;
		}
		uint8 *rgb = (uint8 *)memory::NkAlloc((usize)w * h * 3);
		for (int32 fr = 0; fr < nframes; ++fr) {
			RenderFrame(rgb, w, h, fr, nframes);
			if (!vw.WriteFrame(rgb, media::NkVideoInputFormat::RGB24)) {
				::printf("  [ERREUR] trame %d\n", fr);
				memory::NkFree(rgb);
				vw.Close();
				return false;
			}
		}
		memory::NkFree(rgb);
		vw.Close();
		return true;
	}
} // namespace

int main(int argc, char **argv) {
	const char *outDir = (argc > 1) ? argv[1] : ".";
	const int32 W = 320, H = 240, FPS = 25, N = 50; // 2 s

	char pathMjpeg[1024], pathRaw[1024], pathMov[1024];
	::snprintf(pathMjpeg, sizeof(pathMjpeg), "%s/nkvideo_mjpeg.avi", outDir);
	::snprintf(pathRaw, sizeof(pathRaw), "%s/nkvideo_raw.avi", outDir);
	::snprintf(pathMov, sizeof(pathMov), "%s/nkvideo_mjpeg.mov", outDir);

	::printf("=== NKVideoTest — creation video from-scratch (sans ffmpeg) ===\n\n");
	::printf("Rendu de %d trames %dx%d @ %d fps...\n\n", N, W, H, FPS);

	int nbOk = 0, nbTotal = 0;

	// --- AVI MJPEG ---
	{
		++nbTotal;
		const bool made = MakeVideo(pathMjpeg, media::NkVideoCodec::MJPEG, media::NkVideoContainer::AVI, W, H, FPS, N);
		int32 frames = 0;
		const bool verified = made && VerifyAvi(pathMjpeg, frames);
		::printf("[ %s ] AVI MJPEG -> %s  (%d trames)\n", verified && frames == N ? "OK " : "FAIL", pathMjpeg, frames);
		if (verified && frames == N)
			++nbOk;
	}
	// --- AVI RAW BGR ---
	{
		++nbTotal;
		const bool made = MakeVideo(pathRaw, media::NkVideoCodec::RAW_BGR, media::NkVideoContainer::AVI, W, H, FPS, N);
		int32 frames = 0;
		const bool verified = made && VerifyAvi(pathRaw, frames);
		::printf("[ %s ] AVI RAW   -> %s  (%d trames)\n", verified && frames == N ? "OK " : "FAIL", pathRaw, frames);
		if (verified && frames == N)
			++nbOk;
	}
	// --- MOV/MP4 MJPEG ---
	{
		++nbTotal;
		const bool made = MakeVideo(pathMov, media::NkVideoCodec::MJPEG, media::NkVideoContainer::MOV, W, H, FPS, N);
		// Vérif conteneur ISOBMFF : contient 'ftyp' + 'moov' + 'mdat'.
		bool ok = made;
		if (ok) {
			FILE *f = ::fopen(pathMov, "rb");
			ok = (f != nullptr);
			if (f) {
				::fseek(f, 0, SEEK_END);
				long s = ::ftell(f);
				::fseek(f, 0, SEEK_SET);
				uint8 *b = (uint8 *)memory::NkAlloc((usize)s);
				::fread(b, 1, (usize)s, f);
				::fclose(f);
				bool hasFtyp = false, hasMoov = false, hasMdat = false;
				for (long i = 0; i + 4 <= s; ++i) {
					if (b[i] == 'f' && b[i + 1] == 't' && b[i + 2] == 'y' && b[i + 3] == 'p')
						hasFtyp = true;
					if (b[i] == 'm' && b[i + 1] == 'o' && b[i + 2] == 'o' && b[i + 3] == 'v')
						hasMoov = true;
					if (b[i] == 'm' && b[i + 1] == 'd' && b[i + 2] == 'a' && b[i + 3] == 't')
						hasMdat = true;
				}
				ok = hasFtyp && hasMoov && hasMdat;
				memory::NkFree(b);
			}
		}
		::printf("[ %s ] MOV MJPEG -> %s\n", ok ? "OK " : "FAIL", pathMov);
		if (ok)
			++nbOk;
	}

	::printf("\n=== Resultat : %d/%d OK ===\n", nbOk, nbTotal);
	::printf("(Ouvre les .avi dans VLC / un navigateur pour verifier le rendu.)\n");
	return (nbOk == nbTotal) ? 0 : 1;
}
