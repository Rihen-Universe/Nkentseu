// =============================================================================
// NkRenderWindow.cpp — Implementation NkRenderTarget pour une NkWindow.
//
// Cree NkIGraphicsContext + NkIRenderer2D via les factories existantes et
// delegue les operations virtuelles au renderer interne. Le submit raw
// vertices (Draw(NkVertex*, count, primitive, states)) applique le transform
// states.transform cote CPU avant DrawVertices car NkIRenderer2D ne dispose
// pas encore d'uniform `uModel` (a ajouter dans une passe ulterieure).
//
// SUPPORT PRIMITIVES (premiere passe)
//   - NK_TRIANGLES         : pris en charge (identity indices).
//   - NK_LINES / NK_POINTS : delegues a DrawLine / DrawPoint en boucle.
//   - NK_TRIANGLE_STRIP / NK_TRIANGLE_FAN / NK_LINE_STRIP : TODO — l'appel
//     est ignore et un warning est emis (a implementer si refonte Pong en a
//     besoin, sinon dans A.6 quand on aura NkRenderer2D facade).
// =============================================================================

#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCanvas/Renderer/Core/NkIRenderer2D.h"
#include "NKCanvas/Renderer/Core/NkRenderer2DFactory.h"
#include "NKCanvas/Core/NkIGraphicsContext.h"
#include "NKCanvas/Factory/NkContextFactory.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKMemory/NkAllocator.h"

namespace nkentseu {
    namespace renderer {

        // -------------------------------------------------------------------------
        // Helpers : conversion NkVertex (struct interne) ↔ float (x,y) avec
        // application d'un NkTransform. Place en TU-local pour eviter d'exposer.
        // -------------------------------------------------------------------------
        namespace {
            inline NkVertex TransformVertex(const NkVertex& v, const NkTransform& t) noexcept {
                NkVertex out = v;
                const NkVec2f p = t.TransformPoint({v.x, v.y});
                out.x = p.x; out.y = p.y;
                return out;
            }

            // Buffer temporaire : stack si petit, heap sinon. Libere a la destruction.
            struct ScratchVertices {
                NkVertex  stack[256];
                NkVertex* heap{nullptr};
                uint32    count{0};

                NkVertex* Acquire(uint32 n) {
                    count = n;
                    if (n <= 256) return stack;
                    heap = static_cast<NkVertex*>(nkentseu::memory::NkAlloc(sizeof(NkVertex) * n));
                    return heap;
                }
                ~ScratchVertices() {
                    if (heap) nkentseu::memory::NkFree(heap);
                }
            };

            struct ScratchIndices {
                uint32  stack[256];
                uint32* heap{nullptr};

                uint32* Acquire(uint32 n) {
                    if (n <= 256) return stack;
                    heap = static_cast<uint32*>(nkentseu::memory::NkAlloc(sizeof(uint32) * n));
                    return heap;
                }
                ~ScratchIndices() {
                    if (heap) nkentseu::memory::NkFree(heap);
                }
            };
        } // anon

        // =========================================================================
        // Ctor / Dtor
        // =========================================================================

        NkRenderWindow::NkRenderWindow(NkWindow& window, const NkContextDesc& desc) noexcept
            : mWindow(&window)
        {
            mContext = NkContextFactory::Create(window, desc);
            if (!mContext) {
                // Echec creation contexte — IsValid() retournera false, l'appelant
                // doit verifier. (NkLogger pris en charge dans une iteration ulterieure
                // avec un logger global ; pour l'instant on reste minimal.)
                return;
            }
            mRenderer = NkRenderer2DFactory::CreateUnique(mContext);
            if (!mRenderer) return;
            // NB : NE PAS appeler mRenderer->Initialize(mContext) ici !
            // NkRenderer2DFactory::Create l'a deja fait (cf. NkRenderer2DFactory.cpp:83).
            // Double-init -> NkOpenGLRenderer2D::Initialize log "Already initialized"
            // ERR et corrompt l'etat (rectangles creux observes en runtime).

            // Wire la facade user-facing sur le backend + ce target.
            mFacade.Bind(mRenderer.Get(), this);
        }

        // Variante a contexte fourni par l'utilisateur : on ne cree PAS le contexte,
        // on cree juste le Renderer2D par-dessus. mOwnsContext=false => le destructeur
        // ne detruira PAS le contexte (l'appelant en reste proprietaire).
        NkRenderWindow::NkRenderWindow(NkWindow& window, NkIGraphicsContext* externalContext) noexcept
            : mWindow(&window)
        {
            mOwnsContext = false;
            mContext     = externalContext;
            if (!mContext) return;                 // IsValid() == false
            mRenderer = NkRenderer2DFactory::CreateUnique(mContext);
            if (!mRenderer) return;
            mFacade.Bind(mRenderer.Get(), this);
        }

        NkRenderWindow::~NkRenderWindow() noexcept {
            // Renderer detruit en premier (via NkUniquePtr) — puis contexte.
            if (mRenderer) {
                mRenderer->Shutdown();
                mRenderer.Reset();
            }
            if (mContext) {
                // On ne detruit le contexte que si on le POSSEDE (ctor a contexte
                // fourni => mOwnsContext=false, l'appelant en reste responsable).
                // NkContextFactory ne fournit pas de Destroy explicite : delete sur
                // la base virtuelle (NkIGraphicsContext) suffit (destructeur virtuel).
                if (mOwnsContext) delete mContext;
                mContext = nullptr;
            }
        }

        bool NkRenderWindow::IsValid() const noexcept {
            return mContext != nullptr && mRenderer && mRenderer->IsValid();
        }

        // =========================================================================
        // Clear / Display
        // =========================================================================

        void NkRenderWindow::Clear(const NkColor2D& color) {
            if (!mRenderer) return;
            // Pousser la couleur de clear au contexte AVANT Begin() : sur Vulkan/
            // Software, BeginFrame() ouvre le render pass / clear le back-buffer avec
            // cette couleur (sinon ils utilisent un gris en dur -> fond incorrect).
            // No-op sur OpenGL/DX (ils clearent via le renderer->Clear ci-dessous).
            if (mContext) {
                mContext->SetClearColor(color.r / 255.f, color.g / 255.f,
                                        color.b / 255.f, color.a / 255.f);
            }
            // Begin() ouvre la frame si pas deja ouverte (idempotent par convention
            // du backend) ; Clear() est appelable en milieu de frame.
            if (!mFrameOpen) {
                mRenderer->Begin();
                mFrameOpen = true;
            }
            mRenderer->Clear(color);
        }

        void NkRenderWindow::Display() {
            if (mRenderer && mFrameOpen) {
                mRenderer->End();
                mFrameOpen = false;
            }
            if (mContext) mContext->Present();
        }

        // =========================================================================
        // View / Viewport / Size — delegues au renderer.
        // =========================================================================

        void     NkRenderWindow::SetView(const NkView2D& view) { if (mRenderer) mRenderer->SetView(view); }
        NkView2D NkRenderWindow::GetView()        const        { return mRenderer ? mRenderer->GetView()         : NkView2D{}; }
        NkView2D NkRenderWindow::GetDefaultView() const        { return mRenderer ? mRenderer->GetDefaultView()  : NkView2D{}; }

        void     NkRenderWindow::SetViewport(NkRect2i viewport) { if (mRenderer) mRenderer->SetViewport(viewport); }
        NkRect2i NkRenderWindow::GetViewport() const            { return mRenderer ? mRenderer->GetViewport() : NkRect2i{}; }

        math::NkVec2u NkRenderWindow::GetSize() const {
            return mWindow ? mWindow->GetSize() : math::NkVec2u{0, 0};
        }

        // =========================================================================
        // Submit raw vertices : applique transform + blend + texture, dispatch
        // selon le primitive type.
        // =========================================================================

        void NkRenderWindow::Draw(const NkVertex* vertices, uint32 count,
                                  NkPrimitiveType primitive,
                                  const NkRenderStates& states) {
            if (!mRenderer || !vertices || count == 0) return;
            if (!mFrameOpen) { mRenderer->Begin(); mFrameOpen = true; }

            mRenderer->SetBlendMode(states.blendMode);

            // Applique le transform cote CPU (un uniform `uModel` cote backend
            // sera plus efficace ; a faire dans une passe ulterieure).
            ScratchVertices sv;
            NkVertex* tv = sv.Acquire(count);
            const bool needsTransform = !(states.transform == NkTransform::Identity());
            if (needsTransform) {
                for (uint32 i = 0; i < count; ++i) tv[i] = TransformVertex(vertices[i], states.transform);
            } else {
                for (uint32 i = 0; i < count; ++i) tv[i] = vertices[i];
            }

            switch (primitive) {
                case NkPrimitiveType::NK_TRIANGLES: {
                    // Identity indices : 0, 1, 2, …
                    ScratchIndices si;
                    uint32* idx = si.Acquire(count);
                    for (uint32 i = 0; i < count; ++i) idx[i] = i;
                    mRenderer->DrawVertices(tv, count, idx, count, states.texture);
                    break;
                }
                case NkPrimitiveType::NK_POINTS: {
                    for (uint32 i = 0; i < count; ++i) {
                        const NkVertex& v = tv[i];
                        const NkColor2D c{v.r, v.g, v.b, v.a};
                        mRenderer->DrawPoint({v.x, v.y}, c, 1.f);
                    }
                    break;
                }
                case NkPrimitiveType::NK_LINES: {
                    // Paires consecutives : (v[0],v[1]), (v[2],v[3]), …
                    const uint32 pairs = count / 2;
                    for (uint32 i = 0; i < pairs; ++i) {
                        const NkVertex& a = tv[2 * i + 0];
                        const NkVertex& b = tv[2 * i + 1];
                        const NkColor2D c{a.r, a.g, a.b, a.a}; // couleur du 1er vertex
                        mRenderer->DrawLine({a.x, a.y}, {b.x, b.y}, c, 1.f);
                    }
                    break;
                }
                case NkPrimitiveType::NK_LINE_STRIP: {
                    // Polyline continue : (v[0],v[1]), (v[1],v[2]), … (v[n-2],v[n-1]).
                    // count vertices -> count-1 segments.
                    for (uint32 i = 0; i + 1 < count; ++i) {
                        const NkVertex& a = tv[i];
                        const NkVertex& b = tv[i + 1];
                        const NkColor2D c{a.r, a.g, a.b, a.a};
                        mRenderer->DrawLine({a.x, a.y}, {b.x, b.y}, c, 1.f);
                    }
                    break;
                }
                case NkPrimitiveType::NK_TRIANGLE_STRIP: {
                    // Triangle strip : (v[0],v[1],v[2]), (v[2],v[1],v[3]),
                    // (v[2],v[3],v[4]), … Winding alterne pour orientation
                    // coherente. count vertices -> count-2 triangles, soit
                    // 3*(count-2) sommets pour le list TRIANGLES.
                    if (count < 3) break;
                    const uint32 triCount = count - 2;
                    ScratchIndices si;
                    ScratchVertices sv2;
                    NkVertex* expanded = sv2.Acquire(triCount * 3);
                    for (uint32 i = 0; i < triCount; ++i) {
                        if (i % 2 == 0) {
                            expanded[i*3 + 0] = tv[i + 0];
                            expanded[i*3 + 1] = tv[i + 1];
                            expanded[i*3 + 2] = tv[i + 2];
                        } else {
                            // Swap pour conserver le winding.
                            expanded[i*3 + 0] = tv[i + 1];
                            expanded[i*3 + 1] = tv[i + 0];
                            expanded[i*3 + 2] = tv[i + 2];
                        }
                    }
                    uint32* idx = si.Acquire(triCount * 3);
                    for (uint32 i = 0; i < triCount * 3; ++i) idx[i] = i;
                    mRenderer->DrawVertices(expanded, triCount * 3, idx, triCount * 3, states.texture);
                    break;
                }
                case NkPrimitiveType::NK_TRIANGLE_FAN: {
                    // Triangle fan : (v[0],v[1],v[2]), (v[0],v[2],v[3]), …
                    // count vertices -> count-2 triangles depuis v[0] central.
                    if (count < 3) break;
                    const uint32 triCount = count - 2;
                    ScratchIndices si;
                    ScratchVertices sv2;
                    NkVertex* expanded = sv2.Acquire(triCount * 3);
                    for (uint32 i = 0; i < triCount; ++i) {
                        expanded[i*3 + 0] = tv[0];
                        expanded[i*3 + 1] = tv[i + 1];
                        expanded[i*3 + 2] = tv[i + 2];
                    }
                    uint32* idx = si.Acquire(triCount * 3);
                    for (uint32 i = 0; i < triCount * 3; ++i) idx[i] = i;
                    mRenderer->DrawVertices(expanded, triCount * 3, idx, triCount * 3, states.texture);
                    break;
                }
                default:
                    // NkPrimitiveType inconnu — ignorer.
                    break;
            }
        }

        // =========================================================================
        // Coord mapping — delegue.
        // =========================================================================

        NkVec2f NkRenderWindow::MapPixelToCoords(NkVec2i pixel) const {
            return mRenderer ? mRenderer->MapPixelToCoords(pixel) : NkVec2f{0.f, 0.f};
        }
        NkVec2i NkRenderWindow::MapCoordsToPixel(NkVec2f point) const {
            return mRenderer ? mRenderer->MapCoordsToPixel(point) : NkVec2i{0, 0};
        }

        // =========================================================================
        // Swapchain recreation (resize + DPI change).
        // Les 5 backends (OpenGL/Vulkan/DX11/DX12/Metal/Software) implementent
        // NkIGraphicsContext::OnResize en faisant la recreation appropriee :
        //   - Vulkan : vkDeviceWaitIdle + destroy/create VkSwapchainKHR
        //   - OpenGL : glViewport(0,0,w,h) + relink des buffers FBO si offscreen
        //   - DX11   : IDXGISwapChain::ResizeBuffers(w, h, …)
        //   - DX12   : IDXGISwapChain3::ResizeBuffers + recreate RTVs
        //   - Metal  : CAMetalLayer.drawableSize = (w, h)
        // =========================================================================

        bool NkRenderWindow::OnResize(uint32 width, uint32 height) noexcept {
            if (!mContext) return false;
            // Si une frame est en cours, on la termine avant la recreation
            // (sinon submit sur swapchain detruite -> UB sur Vulkan/DX12).
            if (mFrameOpen && mRenderer) {
                mRenderer->End();
                mFrameOpen = false;
            }
            return mContext->OnResize(width, height);
        }

        bool NkRenderWindow::OnDpiChange() noexcept {
            if (!mWindow) return false;
            const math::NkVec2u sz = mWindow->GetSize();  // taille physique post-DPI
            return OnResize(sz.x, sz.y);
        }

    } // namespace renderer
} // namespace nkentseu
