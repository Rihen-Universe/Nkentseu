#pragma once
#include <cstdio>
#include <cstdlib>
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkGuiCanvasBackend.h — rend un nkgui::NkGuiDrawList via NKCanvas (NkIRenderer2D).
// Backend RÉUTILISABLE (lib) : le cœur NKGui reste render-agnostique ; ce pont
// traduit ses draw-lists en appels NkIRenderer2D. Gère l'atlas de police
// (gray8 -> RGBA blanc+alpha) et les images RGBA pour les commandes texturées.
//
// HEADER-ONLY : NKCanvas ne le compile pas (pas de .cpp) ; seuls les consommateurs
// (qui dépendent déjà de NKGui ET NKCanvas) l'incluent. Modelé sur NkUICanvasBackend.
// =============================================================================
#include "NKCanvas/Renderer/Core/NkIRenderer2D.h"
#include "NKCanvas/Renderer/Core/NkRenderer2DTypes.h" // NkVertex2D
#include "NKCanvas/Renderer/Resources/NkTexture.h"	  // NkTexture / NkTextureFilter
#include "NKContainers/Sequential/NkVector.h"
#include "NKMemory/NKMemory.h"	// NkGetDefaultAllocator
#include "NKMath/NkRectangle.h" // NkRect2i
#include "NKGui/NKGui.h"

namespace nkentseu {
	namespace renderer {

		class NkGuiCanvasBackend {
			public:
				~NkGuiCanvasBackend() {
					auto &alloc = nkentseu::memory::NkGetDefaultAllocator();
					for (nkentseu::uint32 i = 0; i < mFonts.Size(); ++i)
						if (mFonts[i].tex)
							alloc.Delete(mFonts[i].tex);
					for (nkentseu::uint32 i = 0; i < mImages.Size(); ++i)
						if (mImages[i].tex)
							alloc.Delete(mImages[i].tex);
				}

				bool Init(nkentseu::renderer::NkIRenderer2D *renderer) {
					mRenderer = renderer;
					return renderer != nullptr;
				}

				// Upload (ou ré-upload) d'un atlas alpha8 sous l'id `texId` : étend gray ->
				// RGBA (blanc + alpha = couverture du glyphe).
				bool UploadFontGray8(nkentseu::uint32 texId, const nkentseu::uint8 *gray, nkentseu::int32 w,
									 nkentseu::int32 h) {
					using namespace nkentseu;
					if (!mRenderer || !gray || w <= 0 || h <= 0 || texId == 0u)
						return false;

					const usize n = static_cast<usize>(w) * static_cast<usize>(h);
					mExpand.Resize(n * 4u);
					uint8 *d = mExpand.Data();
					for (usize i = 0; i < n; ++i) {
						d[i * 4u + 0u] = 255u;
						d[i * 4u + 1u] = 255u;
						d[i * 4u + 2u] = 255u;
						d[i * 4u + 3u] = gray[i];
					}

					// Cherche l'atlas de police existant pour ce texId (UI, code, ...).
					FontTex *ft = nullptr;
					for (uint32 i = 0; i < mFonts.Size(); ++i)
						if (mFonts[i].id == texId) {
							ft = &mFonts[i];
							break;
						}

					// (Re)créer la texture si elle n'existe pas OU si la taille change (rechargement
					// de police à une autre taille = zoom / DPI). Sinon Update() déborderait l'ancienne taille.
					auto &alloc = memory::NkGetDefaultAllocator();
					if (!ft) {
						mFonts.PushBack(FontTex{texId, nullptr, 0, 0});
						ft = &mFonts[mFonts.Size() - 1u];
					}
					if (!ft->tex || ft->w != w || ft->h != h) {
						if (ft->tex) {
							alloc.Delete(ft->tex);
							ft->tex = nullptr;
						}
						ft->tex = alloc.New<renderer::NkTexture>();
						if (!ft->tex)
							return false;
						if (!ft->tex->Create(*mRenderer, static_cast<uint32>(w), static_cast<uint32>(h))) {
							alloc.Delete(ft->tex);
							ft->tex = nullptr;
							return false;
						}
						ft->tex->SetFilter(renderer::NkTextureFilter::NK_LINEAR);
						ft->w = w;
						ft->h = h;
					}
					return ft->tex->Update(mExpand.Data(), static_cast<uint32>(w), static_cast<uint32>(h), 0, 0);
				}

				// Upload (ou ré-upload) d'une VRAIE image RGBA (4 octets/pixel, tight) sous
				// `texId` → texture résolue par Submit pour les commandes Image().
				bool UploadImageRGBA(nkentseu::uint32 texId, const nkentseu::uint8 *rgba, nkentseu::int32 w,
									 nkentseu::int32 h) {
					using namespace nkentseu;
					if (!mRenderer || !rgba || w <= 0 || h <= 0 || texId == 0u)
						return false;
					renderer::NkTexture *tex = nullptr;
					for (uint32 i = 0; i < mImages.Size(); ++i)
						if (mImages[i].id == texId) {
							tex = mImages[i].tex;
							break;
						}
					if (!tex) {
						auto &alloc = memory::NkGetDefaultAllocator();
						tex = alloc.New<renderer::NkTexture>();
						if (!tex)
							return false;
						if (!tex->Create(*mRenderer, static_cast<uint32>(w), static_cast<uint32>(h))) {
							alloc.Delete(tex);
							return false;
						}
						tex->SetFilter(renderer::NkTextureFilter::NK_LINEAR);
						mImages.PushBack(ImgTex{texId, tex});
					}
					return tex->Update(rgba, static_cast<uint32>(w), static_cast<uint32>(h), 0, 0);
				}

				// ═══════════════════════════════════════════════════════════════
				//  LE RELEVE DE `Submit` (NK_PHASES=2) — les 92 % vus de l'interieur
				// ═══════════════════════════════════════════════════════════════
				//  La coquille a montre que **92 % de l'image** part dans les deux
				//  appels a `SubmitDrawList`, et que la presentation ne pese que
				//  0,84 % -- donc ni attente d'ecran, ni synchronisation : du TRAVAIL.
				//  Mais « SubmitDrawList » nomme l'appel, pas son contenu. Voici la
				//  borne d'un cran plus bas, en TROIS postes, et la decoupe suit la
				//  structure reelle de la fonction -- elle n'est pas plaquee :
				//
				//    1. CONVERSION   les sommets NKGui -> sommets du dorsal (une
				//                    boucle sur tout le tampon, pur calcul)
				//    2. RESSOURCES   la recherche de texture : deux balayages
				//                    LINEAIRES (polices puis images) par commande
				//                    texturee. C'est le poste « creation ou recherche
				//                    de ressources ».
				//    3. PILOTE       `SetClip`, `SetBlendMode` et `DrawVertices` --
				//                    les appels qui traversent vers le dorsal
				//                    graphique. **C'est le seul des trois qui puisse
				//                    porter une attente implicite**, et donc le seul
				//                    qui expliquerait une image geante isolee.
				//    (le rebasage des indices est compte avec 1 : c'est du calcul sur
				//     le tampon, meme nature, meme absence de traversee)
				//
				//  ⚠️ LES TROIS REGLES, appliquees ici comme au-dessus :
				//     le TOTAL est la SOMME des trois (il ne peut pas en diverger) ;
				//     le DENOMINATEUR est imprime ; l'ECART DE FERMETURE est dit.
				//
				//  ⚠️ ET CE QUE CETTE DECOUPE NE PEUT PAS FAIRE, dit avec elle : elle
				//     ne distingue pas, DANS le poste 3, l'enregistrement d'une
				//     commande d'une attente du pilote. Les deux se produisent
				//     derriere le meme appel. Ce qu'elle permet, c'est de savoir s'il
				//     faut descendre la -- ou ailleurs.
				struct NkReleveSubmit {
						bool actif = false;
						bool decide = false;
						nkentseu::float64 conversion = 0.0, rebasage = 0.0, ressources = 0.0, pilote = 0.0;
						nkentseu::float64 total = 0.0;
						nkentseu::int32 appels = 0;
				};
				static NkReleveSubmit &Releve() {
					static NkReleveSubmit r;
					return r;
				}

				void Submit(const nkentseu::nkgui::NkGuiDrawList &dl, nkentseu::uint32 fbW, nkentseu::uint32 fbH) {
					using namespace nkentseu;
					if (!mRenderer || dl.vtx.Size() == 0 || dl.idx.Size() == 0)
						return;

					NkReleveSubmit &rel = Releve();
					if (!rel.decide) {
						rel.decide = true;
						const char *v = getenv("NK_PHASES");
						rel.actif = v && v[0] == '2';
					}
					NkChrono hTotal, hPoste;
					if (rel.actif)
						++rel.appels;

					mScratch.Resize(dl.vtx.Size());
					for (uint32 i = 0; i < dl.vtx.Size(); ++i) {
						const nkgui::NkGuiVertex &s = dl.vtx[i];
						renderer::NkVertex2D &d = mScratch[i];
						d.x = s.pos.x;
						d.y = s.pos.y;
						d.u = s.uv.x;
						d.v = s.uv.y;
						d.r = static_cast<uint8>(s.col & 0xFFu);
						d.g = static_cast<uint8>((s.col >> 8) & 0xFFu);
						d.b = static_cast<uint8>((s.col >> 16) & 0xFFu);
						d.a = static_cast<uint8>((s.col >> 24) & 0xFFu);
					}

					if (rel.actif) {
						rel.conversion += hPoste.Elapsed().ToSeconds() * 1000.0;
						hPoste = NkChrono();
					}

					for (uint32 ci = 0; ci < dl.cmds.Size(); ++ci) {
						const nkgui::NkGuiDrawCmd &dc = dl.cmds[ci];
						if (dc.idxCount == 0u)
							continue;

						const bool hasClip = (dc.clipRect.w < 1.0e8f && dc.clipRect.h < 1.0e8f);
						if (hasClip) {
							float32 x0 = dc.clipRect.x < 0.f ? 0.f : dc.clipRect.x;
							float32 y0 = dc.clipRect.y < 0.f ? 0.f : dc.clipRect.y;
							float32 x1 = dc.clipRect.x + dc.clipRect.w;
							float32 y1 = dc.clipRect.y + dc.clipRect.h;
							if (x1 > static_cast<float32>(fbW))
								x1 = static_cast<float32>(fbW);
							if (y1 > static_cast<float32>(fbH))
								y1 = static_cast<float32>(fbH);
							if (x1 <= x0 || y1 <= y0)
								continue;
							if (rel.actif) {
								rel.rebasage += hPoste.Elapsed().ToSeconds() * 1000.0;
								hPoste = NkChrono();
							}
							mRenderer->SetClip(math::NkRect2i{static_cast<int32>(x0), static_cast<int32>(y0),
															  static_cast<int32>(x1 - x0),
															  static_cast<int32>(y1 - y0)});
							if (rel.actif) {
								rel.pilote += hPoste.Elapsed().ToSeconds() * 1000.0;
								hPoste = NkChrono();
							}
						}

						// 2026-09-04 : le mode de melange de la commande -> l'etat du dorsal
						switch (dc.blend) {
							case nkgui::NkGuiBlend::Multiply:
								mRenderer->SetBlendMode(renderer::NkBlendMode::NK_MULTIPLY);
								break;
							case nkgui::NkGuiBlend::Screen:
								mRenderer->SetBlendMode(renderer::NkBlendMode::NK_SCREEN);
								break;
							case nkgui::NkGuiBlend::Darken:
								mRenderer->SetBlendMode(renderer::NkBlendMode::NK_DARKEN);
								break;
							case nkgui::NkGuiBlend::Lighten:
								mRenderer->SetBlendMode(renderer::NkBlendMode::NK_LIGHTEN);
								break;
							case nkgui::NkGuiBlend::PlusLighter:
								mRenderer->SetBlendMode(renderer::NkBlendMode::NK_PLUS_LIGHTER);
								break;
							default:
								mRenderer->SetBlendMode(renderer::NkBlendMode::NK_ALPHA);
								break;
						}

						if (rel.actif) {
							rel.pilote += hPoste.Elapsed().ToSeconds() * 1000.0; // SetBlendMode
							hPoste = NkChrono();
						}
						renderer::NkTexture *tex = nullptr;
						if (dc.type == nkgui::NkGuiDrawCmdType::TexturedTriangles) {
							for (uint32 fi = 0; fi < mFonts.Size(); ++fi)
								if (mFonts[fi].id == dc.texId) {
									tex = mFonts[fi].tex;
									break;
								}
							if (!tex)
								for (uint32 ti = 0; ti < mImages.Size(); ++ti)
									if (mImages[ti].id == dc.texId) {
										tex = mImages[ti].tex;
										break;
									}
						}

						// Ne soumet que le SOUS-ENSEMBLE de vertices reference par cette
						// commande (indices rebases). Indispensable : passer tout le buffer
						// depasse kMaxVertices (65536) des qu'un draw list est gros -> crash.
						if (rel.actif) {
							rel.ressources += hPoste.Elapsed().ToSeconds() * 1000.0;
							hPoste = NkChrono();
						}
						uint32 lo = 0xFFFFFFFFu, hi = 0u;
						for (uint32 k = 0; k < dc.idxCount; ++k) {
							const uint32 v = dl.idx[dc.idxOffset + k];
							if (v < lo)
								lo = v;
							if (v > hi)
								hi = v;
						}
						mIdxTmp.Resize(dc.idxCount);
						for (uint32 k = 0; k < dc.idxCount; ++k)
							mIdxTmp[k] = dl.idx[dc.idxOffset + k] - lo;
						if (rel.actif) {
							rel.rebasage += hPoste.Elapsed().ToSeconds() * 1000.0;
							hPoste = NkChrono();
						}
						mRenderer->DrawVertices(mScratch.Data() + lo, hi - lo + 1u, mIdxTmp.Data(), dc.idxCount, tex);
						if (rel.actif) {
							rel.pilote += hPoste.Elapsed().ToSeconds() * 1000.0;
							hPoste = NkChrono();
						}

						if (hasClip)
							mRenderer->PopClip();
					}
					mRenderer->SetBlendMode(renderer::NkBlendMode::NK_ALPHA); // ce qui suit repart en alpha
					if (rel.actif) {
						rel.pilote += hPoste.Elapsed().ToSeconds() * 1000.0;
						rel.total += hTotal.Elapsed().ToSeconds() * 1000.0;
						// Deux appels par image (la liste normale et l'incrustation) :
						// 600 appels = 300 images, la meme cadence que la coquille.
						if ((rel.appels % 600) == 0) {
							const float64 somme = rel.conversion + rel.rebasage + rel.ressources + rel.pilote;
							printf("[submit] %d appels (= %d images) ; TOTAL mesure %.2f ms\n"
								   "[submit]   conversion des sommets %9.2f ms  (%5.1f %%)\n"
								   "[submit]   rebasage des indices  %9.2f ms  (%5.1f %%)\n"
								   "[submit]   recherche de texture  %9.2f ms  (%5.1f %%)\n"
								   "[submit]   appels au PILOTE      %9.2f ms  (%5.1f %%)\n"
								   "[submit]   FERMETURE : somme des quatre %.2f ms contre %.2f ms "
								   "de total -- il manque %.1f %%\n",
								   rel.appels, rel.appels / 2, rel.total,
								   rel.conversion, 100.0 * rel.conversion / (rel.total > 0.0 ? rel.total : 1.0),
								   rel.rebasage, 100.0 * rel.rebasage / (rel.total > 0.0 ? rel.total : 1.0),
								   rel.ressources, 100.0 * rel.ressources / (rel.total > 0.0 ? rel.total : 1.0),
								   rel.pilote, 100.0 * rel.pilote / (rel.total > 0.0 ? rel.total : 1.0),
								   somme, rel.total,
								   rel.total > 0.0 ? 100.0 * (rel.total - somme) / rel.total : 0.0);
							fflush(stdout);
						}
					}
				}

			private:
				struct ImgTex {
						nkentseu::uint32 id;
						nkentseu::renderer::NkTexture *tex;
				};

				// Un atlas de police par texId (interface, code, ...). Plusieurs polices
				// = plusieurs textures resolues par Submit selon le texId de la commande.
				struct FontTex {
						nkentseu::uint32 id;
						nkentseu::renderer::NkTexture *tex;
						nkentseu::int32 w;
						nkentseu::int32 h;
				};

				nkentseu::renderer::NkIRenderer2D *mRenderer = nullptr;
				nkentseu::NkVector<FontTex> mFonts;
				nkentseu::NkVector<ImgTex> mImages;
				nkentseu::NkVector<nkentseu::renderer::NkVertex2D> mScratch;
				nkentseu::NkVector<nkentseu::uint32> mIdxTmp; // indices rebases par commande
				nkentseu::NkVector<nkentseu::uint8> mExpand;
		};

	} // namespace renderer
} // namespace nkentseu
