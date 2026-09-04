// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkOpenglTimestamp.cpp — le chrono GPU du backend OpenGL (2026-09-04).
//
// AVANT : NkIDevice::BeginTimestampQuery/EndTimestampQuery/GetTimestampResults
// etaient des corps VIDES qu'aucun backend ne surchargeait, et personne
// n'ecrivait NkRendererStats::gpuTimeMs -- le HUD affichait « GPU: 0.00ms »
// depuis toujours : un instrument qui n'existe pas rend un faux.
//
// ICI : glQueryCounter(GL_TIMESTAMP) au debut et a la fin de la frame, dans un
// anneau de kTsRing frames ; la lecture rend la frame la plus ancienne dont les
// deux resultats sont DISPONIBLES (GL_QUERY_RESULT_AVAILABLE), sans jamais
// bloquer le CPU. Sans glQueryCounter (WebGL2, GLES 3.0), on le dit une fois et
// on rend faux -> le HUD affiche « -- ».
// =============================================================================
#include "NkOpenglDevice.h" // inclut <glad/gl.h>
#include <cstdio>

namespace nkentseu {

	void NkOpenGLDevice::BeginTimestampQuery(uint32 index) {
		(void)index; // un seul chrono : la frame entiere
		if (!glad_glQueryCounter || !glad_glGetQueryObjectui64v || !glad_glGenQueries) {
			if (!mTsAbsentDit) {
				mTsAbsentDit = true;
				fprintf(stderr, "[NkGL] chrono GPU absent : glQueryCounter(GL_TIMESTAMP) indisponible sur ce contexte -- le HUD dira « -- »\n");
			}
			return;
		}
		if (mTsQuery[mTsSlot][0] == 0)
			glGenQueries(2, mTsQuery[mTsSlot]);
		glQueryCounter(mTsQuery[mTsSlot][0], GL_TIMESTAMP);
	}

	void NkOpenGLDevice::EndTimestampQuery(uint32 index) {
		(void)index;
		if (!glad_glQueryCounter || mTsQuery[mTsSlot][0] == 0)
			return;
		glQueryCounter(mTsQuery[mTsSlot][1], GL_TIMESTAMP);
		mTsIssued[mTsSlot] = true;
		mTsSlot = (mTsSlot + 1) % kTsRing;
	}

	bool NkOpenGLDevice::GetTimestampResults(uint64 *outNs, uint32 count) {
		if (!outNs || count < 2 || !glad_glGetQueryObjectui64v)
			return false;
		// La plus ancienne frame emise : celle qui suit le slot courant dans l'anneau.
		for (uint32 k = 1; k <= kTsRing; ++k) {
			const uint32 s = (mTsSlot + k) % kTsRing;
			if (!mTsIssued[s])
				continue;
			GLuint64 avail0 = 0, avail1 = 0;
			glGetQueryObjectui64v(mTsQuery[s][0], GL_QUERY_RESULT_AVAILABLE, &avail0);
			glGetQueryObjectui64v(mTsQuery[s][1], GL_QUERY_RESULT_AVAILABLE, &avail1);
			if (!avail0 || !avail1)
				continue; // pas encore : on ne bloque pas, on reessaiera a la frame suivante
			GLuint64 t0 = 0, t1 = 0;
			glGetQueryObjectui64v(mTsQuery[s][0], GL_QUERY_RESULT, &t0);
			glGetQueryObjectui64v(mTsQuery[s][1], GL_QUERY_RESULT, &t1);
			mTsIssued[s] = false;
			outNs[0] = (uint64)t0;
			outNs[1] = (uint64)t1;
			return true;
		}
		return false;
	}

} // namespace nkentseu
