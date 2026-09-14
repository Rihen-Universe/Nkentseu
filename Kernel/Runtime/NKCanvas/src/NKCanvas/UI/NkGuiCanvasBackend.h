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
						bool fin = false; ///< les chronos PAR COMMANDE (NK_PHASES=2)
						bool decide = false;
						nkentseu::float64 boucle = 0.0; ///< UN seul chrono autour de toute la boucle
						nkentseu::float64 conversion = 0.0, rebasage = 0.0, ressources = 0.0, pilote = 0.0;
						nkentseu::float64 total = 0.0;
						nkentseu::int32 appels = 0;
						// ⚠️ DES COMPTES, PAS DES DUREES. Deduire une allocation d'un
						//    temps long, c'est confirmer ce qu'on croyait deja. On
						//    compte donc un CHANGEMENT DE CAPACITE -- le seul fait qui
						//    dise qu'une reallocation a eu lieu.
						nkentseu::int64 reallocIdx = 0, reallocVtx = 0;
						nkentseu::int64 derniereRealloc = -1; ///< a quel appel, la derniere
						nkentseu::int64 zerosPerdus = 0;	  ///< elements remplis a zero par Resize
						nkentseu::int64 indicesTraites = 0;	  ///< les deux passes utiles
						nkentseu::int64 commandes = 0;		  ///< combien de commandes de dessin
						nkentseu::float64 balayage = 0.0;	  ///< la passe lo/hi
						nkentseu::float64 recopie = 0.0;	  ///< la passe de soustraction
						nkentseu::float64 popclip = 0.0;	  ///< PopClip, un appel au dorsal
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
						// ⚠️ DEUX MODES, ET C'EST LE BANC DE L'INSTRUMENT LUI-MEME.
						//    `=2` pose un chronometre PAR COMMANDE ; `=3` n'en pose
						//    qu'UN, autour de toute la boucle. Si le total de `=2` est
						//    tres superieur a celui de `=3`, **c'est l'instrument qui
						//    coute**, et aucun de ses pourcentages ne vaut.
						rel.actif = v && (v[0] == '2' || v[0] == '3');
						rel.fin = v && v[0] == '2';
					}
					NkChrono hTotal, hPoste;
					if (rel.actif)
						++rel.appels;

					if (rel.actif) {
						const uint32 capAvant = mScratch.Capacity();
						mScratch.Resize(dl.vtx.Size());
						if (mScratch.Capacity() != capAvant) {
							++rel.reallocVtx;
							rel.derniereRealloc = rel.appels;
						}
					} else
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
					NkChrono hBoucle;

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
							if (rel.fin) {
								rel.rebasage += hPoste.Elapsed().ToSeconds() * 1000.0;
								hPoste = NkChrono();
							}
							mRenderer->SetClip(math::NkRect2i{static_cast<int32>(x0), static_cast<int32>(y0),
															  static_cast<int32>(x1 - x0),
															  static_cast<int32>(y1 - y0)});
							if (rel.fin) {
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

						if (rel.fin) {
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
						if (rel.fin) {
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
						if (rel.fin) {
							rel.balayage += hPoste.Elapsed().ToSeconds() * 1000.0;
							++rel.commandes;
							hPoste = NkChrono();
						}
						if (rel.actif) {
							const uint32 capAvant = mIdxTmp.Capacity();
							const uint32 tailleAvant = mIdxTmp.Size();
							// la PASSE PERDUE : `Resize` initialise a zero tout ce qu'il
							// ajoute, et on recrase ces zeros trois lignes plus bas.
							if (dc.idxCount > tailleAvant)
								rel.zerosPerdus += (int64)(dc.idxCount - tailleAvant);
							mIdxTmp.Resize(dc.idxCount);
							if (mIdxTmp.Capacity() != capAvant) {
								++rel.reallocIdx;
								rel.derniereRealloc = rel.appels;
							}
							rel.indicesTraites += (int64)dc.idxCount * 2; // balayage + recopie
						} else
							mIdxTmp.Resize(dc.idxCount);
						// ⚠️ UNE PASSE PERDUE EXISTE ICI, ET ELLE N'EST PAS CORRIGEE.
						//    Ce bloc de commentaire garde la mesure parce qu'elle vaut
						//    plus que le correctif qu'elle a fait abandonner.
						// ⚠️ MESURE D'ABORD, ET ELLE A TUE MON PROPRE SOUPCON. J'avais
						//    accuse la REALLOCATION. Les COMPTES disent : 5 reallocations
						//    de `mIdxTmp` et 3 de `mScratch` sur 600 appels, la derniere
						//    a l'appel n°5. `NkVector` ne rend jamais sa capacite
						//    (`ShrinkToFit` n'est pas appele), donc apres cinq appels il
						//    n'alloue plus rien. **Ce n'etait pas l'allocation.**
						//
						// ⚠️ CE QUE LES COMPTES ONT TROUVE A LA PLACE : `Resize(n)` avec
						//    `n > mSize` fait `ConstructAt` sur chaque element ajoute --
						//    pour un `uint32`, il ECRIT UN ZERO. Comme `mIdxTmp`
						//    retrecissait a chaque commande (`Resize` vers une taille plus
						//    petite detruit, donc la suivante re-grossit), on ecrivait des
						//    zeros qu'on ecrasait trois lignes plus bas :
						//        8 023 344 zeros ecrits puis ecrases
						//        contre 19 830 096 indices traites utilement
						//        -> **40 % de travail perdu**, a chaque image.
						//
						// 🔴 J'AI ECRIT LE CORRECTIF (ne plus retrecir : `if (Size() <
						//    idxCount) Resize(...)`), MESURE, ET JE L'AI RETIRE.
						//    Les zeros sont bien passes de 8 023 344 a 14 652 -- le geste
						//    faisait ce qu'il annoncait -- et le temps N'A PAS BOUGE :
						//        AVANT  1853,17 | 1802,18 | 1808,77 ms
						//        APRES  1798,81 | 1801,68 | 1807,16 ms
						//    (courses ENTRELACEES, meme machine, meme minute -- cette
						//     machine rend 62 a 140 images/s pour la meme mesure, deux
						//     courses eloignees ne se comparent pas.)
						//    Supprimer huit millions d'ecritures inutiles n'a rien change :
						//    **ce n'etait pas la le cout.** Un changement sans gain mesure
						//    dans du code de noyau PARTAGE ne se garde pas -- il ajoute un
						//    risque a quatre autres applications contre rien. La MESURE
						//    reste ; le correctif part.
						//
						// ⚠️ ET LE VRAI COUT EST AILLEURS : `PopClip`, 1 827 ms sur
						//    1 940 -- 94 %. Voir le releve.
						for (uint32 k = 0; k < dc.idxCount; ++k)
							mIdxTmp[k] = dl.idx[dc.idxOffset + k] - lo;
						if (rel.fin) {
							rel.recopie += hPoste.Elapsed().ToSeconds() * 1000.0;
							hPoste = NkChrono();
						}
						mRenderer->DrawVertices(mScratch.Data() + lo, hi - lo + 1u, mIdxTmp.Data(), dc.idxCount, tex);
						if (rel.fin) {
							rel.pilote += hPoste.Elapsed().ToSeconds() * 1000.0;
							hPoste = NkChrono();
						}

						// 🔴 `PopClip` N'ETAIT MESURE PAR RIEN, et il tombait dans le
						//    seau du COUP D'APRES. C'est un appel au DORSAL, au meme
						//    titre que `SetClip` -- le compter avec le rebasage etait une
						//    erreur d'etiquette, pas de chronometre.
						if (hasClip) {
							mRenderer->PopClip();
							if (rel.fin) {
								rel.popclip += hPoste.Elapsed().ToSeconds() * 1000.0;
								hPoste = NkChrono();
							}
						}
					}
					mRenderer->SetBlendMode(renderer::NkBlendMode::NK_ALPHA); // ce qui suit repart en alpha
					if (rel.actif) {
						if (rel.fin)
							rel.pilote += hPoste.Elapsed().ToSeconds() * 1000.0;
						rel.boucle += hBoucle.Elapsed().ToSeconds() * 1000.0;
						rel.total += hTotal.Elapsed().ToSeconds() * 1000.0;
						// Deux appels par image (la liste normale et l'incrustation) :
						// 600 appels = 300 images, la meme cadence que la coquille.
						if ((rel.appels % 600) == 0) {
							const float64 somme = rel.conversion + rel.rebasage + rel.balayage + rel.recopie + rel.popclip + rel.ressources + rel.pilote;
							printf("[submit] %s -- %d appels (= %d images) ; TOTAL %.2f ms ; "
								   "la BOUCLE DE COMMANDES a elle seule %.2f ms\n"
								   "[submit]   conversion des sommets %9.2f ms  (%5.1f %%)\n"
								   "[submit]   rebasage des indices  %9.2f ms  (%5.1f %%)\n"
								   "[submit]   recherche de texture  %9.2f ms  (%5.1f %%)\n"
								   "[submit]   appels au PILOTE      %9.2f ms  (%5.1f %%)\n"
								   "[submit]   FERMETURE : somme des quatre %.2f ms contre %.2f ms "
								   "de total -- il manque %.1f %%\n",
								   rel.fin ? "chronos PAR COMMANDE" : "UN SEUL chrono (grossier)",
								   rel.appels, rel.appels / 2, rel.total, rel.boucle,
								   rel.conversion, 100.0 * rel.conversion / (rel.total > 0.0 ? rel.total : 1.0),
								   rel.rebasage + rel.balayage + rel.recopie,
								   100.0 * (rel.rebasage + rel.balayage + rel.recopie) / (rel.total > 0.0 ? rel.total : 1.0),
								   rel.ressources, 100.0 * rel.ressources / (rel.total > 0.0 ? rel.total : 1.0),
								   rel.pilote + rel.popclip,
								   100.0 * (rel.pilote + rel.popclip) / (rel.total > 0.0 ? rel.total : 1.0),
								   somme, rel.total,
								   rel.total > 0.0 ? 100.0 * (rel.total - somme) / rel.total : 0.0);
							printf("[submit]   COMPTES (pas des durees) :\n"
								   "[submit]     reallocations mIdxTmp : %lld   mScratch : %lld\n"
								   "[submit]     derniere realloc a l'appel n°%lld sur %d\n"
								   "[submit]     zeros ECRITS PUIS ECRASES par Resize : %lld\n"
								   "[submit]     indices traites par les deux passes utiles : %lld\n"
								   "[submit]     -> la passe PERDUE pese %.0f %% des passes utiles\n"
								   "[submit]     commandes de dessin : %lld (= %.0f par image)\n"
								   "[submit]     DANS le rebasage : balayage lo/hi %.2f ms | "
								   "recopie %.2f ms | PopClip %.2f ms | reste %.2f ms\n",
								   (long long)rel.reallocIdx, (long long)rel.reallocVtx,
								   (long long)rel.derniereRealloc, rel.appels,
								   (long long)rel.zerosPerdus, (long long)rel.indicesTraites,
								   rel.indicesTraites > 0
									   ? 100.0 * (float64)rel.zerosPerdus / (float64)rel.indicesTraites
									   : 0.0,
								   (long long)rel.commandes,
								   rel.appels > 0 ? (float64)rel.commandes / ((float64)rel.appels / 2.0) : 0.0,
								   rel.balayage, rel.recopie, rel.popclip,
								   rel.rebasage);
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
