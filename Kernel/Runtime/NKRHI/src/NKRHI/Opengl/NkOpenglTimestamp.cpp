// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkOpenglTimestamp.cpp — le chrono GPU du backend OpenGL (2026-09-04).
//
// AVANT : NkIDevice::BeginTimestampQuery/EndTimestampQuery/GetTimestampResults
// etaient des corps VIDES qu'aucun backend ne surchargeait, et personne
// n'ecrivait NkRendererStats::gpuTimeMs -- le HUD affichait « GPU: 0.00ms »
// depuis toujours : un instrument qui n'existe pas rend un faux.
//
// ICI : glQueryCounter(GL_TIMESTAMP) au debut et a la fin, dans un anneau de
// kTsRing frames, pour DEUX chronos : index 0 = la frame entiere, index 1 = la
// seule passe VFX. La lecture rend la frame la plus ancienne dont les deux
// resultats sont DISPONIBLES (GL_QUERY_RESULT_AVAILABLE), sans jamais bloquer.
// ATTENTION, lecture honnete : un horodatage GPU mesure le temps ENTRE deux
// marqueurs sur la file GPU -- si le CPU met 600 ms a emettre les commandes
// entre les deux, le GPU attend, et l'attente est comptee. Le chrono 1, pose
// juste autour des appels de dessin des particules, isole le dessin.
// Sans glQueryCounter (WebGL2, GLES 3.0) : dit une fois, rend faux -> « -- ».
// =============================================================================
#include "NkOpenglDevice.h" // inclut <glad/gl.h>
#include <cstdio>

// ── Quels symboles portent le chrono, selon le chargeur glad LIE ─────────────
// Mesure du 2026-09-04 (build Web) : glad_glQueryCounter et
// glad_glGetQueryObjectui64v sont DECLARES par glad/gl.h (bureau) mais seul
// gles2.c est compile sur Web/Android -- wasm-ld : « undefined symbol », le
// build web etait casse depuis le commit du chrono. Le controle d'execution
// (`if (!glad_glQueryCounter)`) ne pouvait rien : le lien echoue avant.
// Sur ES, le chrono est l'extension EXT_disjoint_timer_query : memes
// signatures, suffixe EXT, symboles definis dans gles2.c. glad/gles2.h est
// inconciliable avec glad/gl.h deja inclus (cf. NkOpenglDevice.cpp), donc on
// declare les deux pointeurs localement, comme gladLoadGLES2. Pointeur nul
// (extension absente, WebGL2 sans EXT_disjoint_timer_query_webgl2) -> le
// chemin « chrono GPU absent, dit une fois, HUD -- » ci-dessous, inchange.
#if defined(NK_OPENGL_ES)
extern "C" {
extern void (*glad_glQueryCounterEXT)(GLuint id, GLenum target);
extern void (*glad_glGetQueryObjectui64vEXT)(GLuint id, GLenum pname, GLuint64 *params);
}
#define NK_GL_QUERY_COUNTER glad_glQueryCounterEXT
#define NK_GL_GET_QUERY_UI64 glad_glGetQueryObjectui64vEXT
#define NK_GL_TIMESTAMP_TARGET 0x8E28 /* GL_TIMESTAMP_EXT == GL_TIMESTAMP */
#else
#define NK_GL_QUERY_COUNTER glad_glQueryCounter
#define NK_GL_GET_QUERY_UI64 glad_glGetQueryObjectui64v
#define NK_GL_TIMESTAMP_TARGET GL_TIMESTAMP
#endif

namespace nkentseu {

	void NkOpenGLDevice::BeginTimestampQuery(uint32 index) {
		if (index >= kTsIdx)
			return;
		if (!NK_GL_QUERY_COUNTER || !NK_GL_GET_QUERY_UI64 || !glad_glGenQueries) {
			if (!mTsAbsentDit) {
				mTsAbsentDit = true;
				fprintf(stderr, "[NkGL] chrono GPU absent : glQueryCounter(GL_TIMESTAMP) indisponible sur ce contexte -- le HUD dira « -- »\n");
			}
			return;
		}
		const uint32 s = mTsSlot[index];
		if (mTsQuery[index][s][0] == 0)
			glGenQueries(2, mTsQuery[index][s]);
		NK_GL_QUERY_COUNTER(mTsQuery[index][s][0], NK_GL_TIMESTAMP_TARGET);
	}

	void NkOpenGLDevice::EndTimestampQuery(uint32 index) {
		if (index >= kTsIdx || !NK_GL_QUERY_COUNTER)
			return;
		const uint32 s = mTsSlot[index];
		if (mTsQuery[index][s][0] == 0)
			return;
		NK_GL_QUERY_COUNTER(mTsQuery[index][s][1], NK_GL_TIMESTAMP_TARGET);
		mTsIssued[index][s] = true;
		mTsSlot[index] = (s + 1) % kTsRing;
	}

	// outNs : [debut0, fin0] pour count >= 2, puis [debut1, fin1] pour count >= 4
	// (zeros si le chrono 1 n'a rien de disponible). Rend vrai si le chrono 0 a repondu.
	bool NkOpenGLDevice::GetTimestampResults(uint64 *outNs, uint32 count) {
		if (!outNs || count < 2 || !NK_GL_GET_QUERY_UI64)
			return false;
		bool ok0 = false;
		for (uint32 idx = 0; idx < kTsIdx && (idx == 0 || count >= 4); ++idx) {
			outNs[idx * 2] = outNs[idx * 2 + 1] = 0;
			for (uint32 k = 1; k <= kTsRing; ++k) {
				const uint32 s = (mTsSlot[idx] + k) % kTsRing;
				if (!mTsIssued[idx][s])
					continue;
				GLuint64 avail0 = 0, avail1 = 0;
				NK_GL_GET_QUERY_UI64(mTsQuery[idx][s][0], GL_QUERY_RESULT_AVAILABLE, &avail0);
				NK_GL_GET_QUERY_UI64(mTsQuery[idx][s][1], GL_QUERY_RESULT_AVAILABLE, &avail1);
				if (!avail0 || !avail1)
					continue; // pas encore : on ne bloque pas
				GLuint64 t0 = 0, t1 = 0;
				NK_GL_GET_QUERY_UI64(mTsQuery[idx][s][0], GL_QUERY_RESULT, &t0);
				NK_GL_GET_QUERY_UI64(mTsQuery[idx][s][1], GL_QUERY_RESULT, &t1);
				mTsIssued[idx][s] = false;
				outNs[idx * 2] = (uint64)t0;
				outNs[idx * 2 + 1] = (uint64)t1;
				if (idx == 0)
					ok0 = true;
				break;
			}
		}
		return ok0;
	}

} // namespace nkentseu
