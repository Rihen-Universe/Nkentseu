#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkOpenglContexteApple.h — CONTEXTE NSOPENGL 4.1 CORE POUR LE DEVICE GL
//
// Le device GL est du C++ pur ; NSOpenGLContext est de l'Objective-C. Ces
// fonctions (NkOpenglContexteApple.mm, macOS seulement) font le pont avec des
// handles opaques : le contexte et son format de pixel, RETENUS (CFBridgingRetain)
// jusqu'a NkGLAppleDetruire. Meme modele que NKCanvas (NkOpenGLContextMacOS.mm).
// =============================================================================

namespace nkentseu {

	// Cree un contexte 4.1 core, l'attache a `vue` (NSView*, peut etre nul : rendu
	// hors ecran seulement) et le rend courant. Faux si aucun format n'est accorde.
	bool NkGLAppleCreerContexte(void *vue, bool vsync, void *&contexte, void *&formatPixel);

	// [contexte flushBuffer] : presente le tampon arriere.
	void NkGLApplePresenter(void *contexte);

	// [contexte update] : apres un redimensionnement de la vue.
	void NkGLAppleMettreAJour(void *contexte);

	// Detache, libere, remet les handles a nul.
	void NkGLAppleDetruire(void *&contexte, void *&formatPixel);

} // namespace nkentseu
