#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkOpenglCompat41.h — DOUBLURES 4.2-4.5 POUR UN CONTEXTE OPENGL 4.1 CORE
//
// POURQUOI (2026-10-01) : macOS plafonne a OpenGL 4.1 core, et NkOpenGLDevice
// est ecrit en Direct State Access (GL 4.5) avec quelques appels 4.2/4.3
// (vertex_attrib_binding, base instance, copies d'images...). Sur un contexte
// 4.1, glad laisse ces pointeurs NULS et le premier appel tuait le processus.
//
// NkGLCompat41Installer() pose, A LA PLACE DES SEULS POINTEURS RESTES NULS, une
// doublure ecrite avec les fonctions 4.1 (lier, modifier, relier ce qui etait
// lie). Le device ne change pas d'une ligne : il appelle glCreateBuffers, la
// doublure repond. Ce qui n'a pas de doublure credible en 4.1 (calcul, images
// load/store, base instance non nulle) est REFUSE PAR SON NOM, une fois, au
// journal -- jamais un plantage, jamais un silence.
//
// Hors macOS : n'est jamais appele (le pilote fournit tout) ; le .cpp ne
// compile son contenu que sous NK_GL_41.
//
// NK_GL_SIMULER_41 (outil de mise au point, JAMAIS defini par defaut) : sur un
// PC, le device garde son contexte 4.6 mais VIDE les pointeurs que macOS
// laisserait nuls, pose les doublures et adapte les shaders comme sur Mac. Il
// sert a mettre au point le chemin macOS sans Mac (une CI macOS coute 20 min).
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

// NK_GL_41 : le chemin « OpenGL 4.1 core » est compile (macOS, ou simulation).
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NK_GL_SIMULER_41)
#define NK_GL_41 1
#endif

namespace nkentseu {

	// Pose les doublures (contexte GL courant, glad deja charge). Rend leur nombre.
	int NkGLCompat41Installer();

	// Ce que ce contexte refuse, en une ligne lisible (pour le journal).
	const char *NkGLCompat41Refus();

#if defined(NK_GL_SIMULER_41)
	// Simulation : vide les pointeurs qu'un contexte 4.1 d'Apple laisserait nuls.
	void NkGLCompat41Simuler();
#endif

} // namespace nkentseu
