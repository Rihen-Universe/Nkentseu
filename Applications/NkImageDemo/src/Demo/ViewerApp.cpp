// =============================================================================
// ViewerApp.cpp
// -----------------------------------------------------------------------------
// Implementation : scan d'un dossier d'images au demarrage, chargement via
// NkImage::Load (qui dispatche automatiquement vers le bon codec selon
// l'extension : NkPNGCodec, NkJPEGCodec, NkSVGCodec, NkGIFCodec, etc.),
// upload GL via Texture2D, render plein ecran avec ratio preserve, HUD
// bas affichant les metadonnees.
// =============================================================================

#include "ViewerApp.h"
#include "Demo/UI/UIScale.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKLogger/NkLog.h"
#include "NKImage/Core/NkImage.h"
#include "NKImage/Codecs/GIF/NkGIFCodec.h"
#include "NKImage/Codecs/HDR/NkHDRCodec.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKMath/NkColor.h"
#include <cstdio>
#include <cstring>

namespace nkentseu
{
    namespace demo
    {

        // Extensions reconnues par les codecs NKImage.
        static const char* kSupportedExts[] = {
            ".png", ".PNG",
            ".jpg", ".JPG", ".jpeg", ".JPEG",
            ".bmp", ".BMP",
            ".tga", ".TGA",
            ".gif", ".GIF",
            ".hdr", ".HDR",
            ".qoi", ".QOI",
            ".webp", ".WEBP",
            ".svg", ".SVG",
            ".ppm", ".PPM",
            ".ico", ".ICO",
        };

        static bool HasSupportedExt(const NkString& path)
        {
            for (const char* ext : kSupportedExts)
            {
                if (path.EndsWith(ext)) return true;
            }
            return false;
        }

        static NkString ExtractFormatFromPath(const NkString& path)
        {
            const char* p = path.CStr();
            const char* lastDot = std::strrchr(p, '.');
            if (lastDot == nullptr) return NkString("?");
            NkString upper(lastDot + 1);
            // Format en majuscules pour HUD.
            for (uint32 i = 0; i < upper.Size(); ++i)
            {
                char c = upper[i];
                if (c >= 'a' && c <= 'z') upper[i] = (char)(c - 32);
            }
            return upper;
        }

        bool ViewerApp::Init()
        {
            mViewportW = mWindow.GetSize().x;
            mViewportH = mWindow.GetSize().y;
            if (mViewportW == 0) mViewportW = 1280;
            if (mViewportH == 0) mViewportH = 720;

            if (!mGL.Init(mWindow))
            {
                logger.Error("[Viewer] GLContext init failed");
                return false;
            }
            mRenderer.Init();
            mFont.Init();
            logger.Info("[Viewer] Init OK ({0}x{1})", mViewportW, mViewportH);

            // Scan : on cherche dans Resources/Pong/Textures (qui contient
            // PNG + SVG + le dossier socials/ + animrihen/). Le user peut
            // pointer ailleurs en modifiant cette ligne ou via env var.
            ScanFolder(NkString("Resources/Pong/Textures"));
            ScanFolder(NkString("Resources/Pong/Textures/socials"));
            ScanFolder(NkString("Resources/Pong/Textures/iconexe"));
            // Le dossier Gif contient les .gif animes a tester (cf Phase 2 codecs).
            ScanFolder(NkString("Resources/Pong/Gif"));
            // Les HDR sont eparpilles : Resources/HDRI (studio.hdr) +
            // Resources/Textures/HDR (~6 fichiers). On scanne les deux.
            ScanFolder(NkString("Resources/HDRI"));
            ScanFolder(NkString("Resources/Textures/HDR"));

            if (mFiles.Size() == 0)
            {
                logger.Warn("[Viewer] Aucun fichier image trouve. Lance depuis la racine du repo Nkentseu.");
                return true;   // on continue quand meme, ecran vide
            }
            logger.Info("[Viewer] {0} fichier(s) detecte(s)", mFiles.Size());

            mCurrentIdx = 0;
            LoadCurrent();
            return true;
        }

        void ViewerApp::Shutdown()
        {
            ClearAnimation();
            if (mHdrSource != nullptr)
            {
                mHdrSource->Free();
                mHdrSource = nullptr;
            }
            mTexture.Shutdown();
            mFont.Shutdown();
            mRenderer.Shutdown();
            mGL.Shutdown();
        }

        void ViewerApp::RebuildHdrTexture()
        {
            // Re-tonemap le source HDR cache avec l'exposure/gamma courants.
            // Utilise pour repondre aux ajustements clavier sans relire le
            // fichier (le decode RGBE est cher sur grosse map 8k).
            if (mHdrSource == nullptr || !mHdrSource->IsValid()) return;
            NkImage* tonemapped =
                NkHDRCodec::ConvertToTexture(*mHdrSource, mHdrExposure, mHdrGamma);
            if (tonemapped == nullptr) return;
            mTexture.Shutdown();
            // UploadFromImage libere l'image apres upload.
            mTexture.UploadFromImage(tonemapped);
        }

        void ViewerApp::OnResize(uint32 w, uint32 h)
        {
            mViewportW = w;
            mViewportH = h;
            mGL.OnResize(w, h);
        }

        void ViewerApp::Update(float dt)
        {
            mTime += dt;

            // ── Avance de l'animation GIF si en cours ─────────────────────
            // On accumule dt jusqu'a depasser le delay de la frame courante,
            // puis on passe a la suivante. Le delayMs est en millisecondes
            // (cf NkGIFCodec::DecodeAnimation). loopCount=0 => boucle infinie.
            if (mAnimTextures.Size() > 1)
            {
                mAnimAccum += dt;
                const float frameDur = (float)mAnimDelaysMs[(uint32)mAnimFrame] / 1000.0f;
                if (mAnimAccum >= frameDur)
                {
                    mAnimAccum -= frameDur;
                    mAnimFrame  = (mAnimFrame + 1) % (int)mAnimTextures.Size();
                }
            }
        }

        void ViewerApp::Render()
        {
            if (!mGL.BeginFrame()) return;
            mRenderer.Begin((int)mViewportW, (int)mViewportH);
            // Fond gris fonce pour faire ressortir les images sur fond
            // transparent.
            mRenderer.Clear(0.12f, 0.12f, 0.14f, 1.0f);

            // ── Image plein ecran (ratio preserve) ─────────────────────────
            // Pour les GIF animes, mAnimTextures contient une texture par
            // frame. Sinon on utilise mTexture (image statique).
            Texture2D* cur = nullptr;
            if (mAnimTextures.Size() > 0 && mAnimFrame >= 0
                && (uint32)mAnimFrame < mAnimTextures.Size())
            {
                cur = mAnimTextures[(uint32)mAnimFrame];
            }
            else if (mLoaded && mTexture.IsValid())
            {
                cur = &mTexture;
            }

            if (cur != nullptr && cur->IsValid())
            {
                const float W = (float)mViewportW;
                const float H = (float)mViewportH;
                const float hudH = 70.0f;            // marge bas pour HUD
                const float availH = H - hudH;
                const float aspect = cur->AspectRatio();
                float drawW = W * 0.92f;
                float drawH = (aspect > 0.0001f) ? (drawW / aspect) : availH;
                if (drawH > availH * 0.95f)
                {
                    drawH = availH * 0.95f;
                    drawW = drawH * aspect;
                }
                const float drawX = (W - drawW) * 0.5f;
                const float drawY = (availH - drawH) * 0.5f;
                mRenderer.BindTexture(cur->Id());
                mRenderer.DrawTexturedQuadRGBA(drawX, drawY, drawW, drawH,
                                               0.0f, 0.0f, 1.0f, 1.0f,
                                               { 255, 255, 255, 255 });
            }

            // ── HUD bas : nom + format + dimensions ────────────────────────
            const float W = (float)mViewportW;
            const float H = (float)mViewportH;
            const float scale = GetUIScale((int)mViewportW, (int)mViewportH);
            const float hudY = H - 60.0f;
            mRenderer.DrawQuad(0.0f, hudY, W, 60.0f, { 0, 0, 0, 180 });

            if (mLoaded)
            {
                char buf[256];
                std::snprintf(buf, sizeof(buf), "[%d/%d]  %s",
                              mCurrentIdx + 1, (int)mFiles.Size(),
                              mMeta.fileName.CStr());
                mFont.DrawStringScaled(mRenderer, FontAtlas::BodySlot, scale,
                                       12.0f, hudY + 12.0f,
                                       buf, { 255, 255, 255, 230 });

                if (mMeta.animated && mAnimTextures.Size() > 1)
                {
                    std::snprintf(buf, sizeof(buf), "%s  %dx%d  frame %d/%d  (%u ms)",
                                  mMeta.format.CStr(),
                                  mMeta.width, mMeta.height,
                                  mAnimFrame + 1, (int)mAnimTextures.Size(),
                                  (unsigned)mAnimDelaysMs[(uint32)mAnimFrame]);
                }
                else if (mIsHdr)
                {
                    std::snprintf(buf, sizeof(buf),
                                  "%s  %dx%d  RGB96F  exposure=%.2f  gamma=%.2f",
                                  mMeta.format.CStr(),
                                  mMeta.width, mMeta.height,
                                  mHdrExposure, mHdrGamma);
                }
                else
                {
                    std::snprintf(buf, sizeof(buf), "%s  %dx%d  %d-channel",
                                  mMeta.format.CStr(),
                                  mMeta.width, mMeta.height,
                                  mMeta.channels);
                }
                mFont.DrawStringScaled(mRenderer, FontAtlas::SmallSlot, scale,
                                       12.0f, hudY + 36.0f,
                                       buf, { 0, 245, 255, 220 });
            }
            else
            {
                mFont.DrawStringCenteredScaled(mRenderer, FontAtlas::BodySlot, scale,
                                               W * 0.5f, hudY + 18.0f,
                                               "AUCUN FICHIER IMAGE TROUVE",
                                               { 255, 100, 100, 220 });
            }

            // Hints clavier coin droit. Sur HDR on rajoute le tip exposure.
            if (mIsHdr)
            {
                mFont.DrawStringScaled(mRenderer, FontAtlas::SmallSlot, scale,
                                       W - 380.0f, hudY + 36.0f,
                                       "<- ->: NAV  +/-: EXPOSURE  ECHAP: QUIT",
                                       { 255, 255, 255, 140 });
            }
            else
            {
                mFont.DrawStringScaled(mRenderer, FontAtlas::SmallSlot, scale,
                                       W - 240.0f, hudY + 36.0f,
                                       "<- ->: NAV     ECHAP: QUIT",
                                       { 255, 255, 255, 140 });
            }

            mRenderer.End();
            mGL.Present();
        }

        void ViewerApp::OnEvent(NkEvent& ev)
        {
            if (auto* k = ev.As<NkKeyPressEvent>())
            {
                switch (k->GetKey())
                {
                case NkKey::NK_ESCAPE:
                    mQuit = true;
                    return;
                case NkKey::NK_LEFT:
                    NavigateNext(-1);
                    return;
                case NkKey::NK_RIGHT:
                    NavigateNext(+1);
                    return;
                case NkKey::NK_HOME:
                    mCurrentIdx = 0;
                    LoadCurrent();
                    return;
                case NkKey::NK_END:
                    if (mFiles.Size() > 0)
                    {
                        mCurrentIdx = (int)mFiles.Size() - 1;
                        LoadCurrent();
                    }
                    return;
                case NkKey::NK_UP:
                case NkKey::NK_EQUALS:    // '+' / '=' US layout
                    if (mIsHdr)
                    {
                        mHdrExposure *= 1.25f;
                        if (mHdrExposure > 16.0f) mHdrExposure = 16.0f;
                        RebuildHdrTexture();
                    }
                    return;
                case NkKey::NK_DOWN:
                case NkKey::NK_MINUS:
                    if (mIsHdr)
                    {
                        mHdrExposure *= 0.8f;
                        if (mHdrExposure < 0.05f) mHdrExposure = 0.05f;
                        RebuildHdrTexture();
                    }
                    return;
                default: break;
                }
            }
        }

        void ViewerApp::OnPause()  {}
        void ViewerApp::OnResume() {}

        bool ViewerApp::RecreateSurface()
        {
            return mGL.RecreateSurface(mWindow);
        }

        // ─────────────────────────────────────────────────────────────────────
        void ViewerApp::ScanFolder(const NkString& folder)
        {
            // NkDirectory::GetFiles enumere via glob (non recursif par defaut).
            // Le pattern "*" recupere tout, on filtre ensuite par extension.
            NkVector<NkString> entries =
                NkDirectory::GetFiles(folder.CStr(), "*");
            uint32 added = 0;
            for (uint32 i = 0; i < entries.Size(); ++i)
            {
                if (HasSupportedExt(entries[i]))
                {
                    mFiles.PushBack(entries[i]);
                    ++added;
                }
            }
            logger.Info("[Viewer] Scan {0} -> {1} fichier(s) image",
                        folder.CStr(), added);
        }

        void ViewerApp::ClearAnimation()
        {
            // Libere toutes les textures GIF allouees + reset l'etat.
            for (uint32 i = 0; i < mAnimTextures.Size(); ++i)
            {
                if (mAnimTextures[i] != nullptr)
                {
                    mAnimTextures[i]->Shutdown();
                    delete mAnimTextures[i];
                }
            }
            mAnimTextures.Clear();
            mAnimDelaysMs.Clear();
            mAnimFrame     = 0;
            mAnimAccum     = 0.0f;
            mAnimLoopCount = 0;
        }

        // Lit un fichier entier en memoire (utilise pour decoder un GIF
        // anime via NkGIFCodec::DecodeAnimation, qui prend data+size). On
        // alloue avec malloc pour pouvoir liberer avec free apres.
        static uint8* ReadFileBytes(const char* path, usize& outSize) noexcept
        {
            FILE* f = std::fopen(path, "rb");
            if (f == nullptr) { outSize = 0; return nullptr; }
            std::fseek(f, 0, SEEK_END);
            const long sz = std::ftell(f);
            std::fseek(f, 0, SEEK_SET);
            if (sz <= 0) { std::fclose(f); outSize = 0; return nullptr; }
            uint8* buf = (uint8*)std::malloc((usize)sz);
            if (buf == nullptr) { std::fclose(f); outSize = 0; return nullptr; }
            const usize rd = std::fread(buf, 1, (usize)sz, f);
            std::fclose(f);
            if (rd != (usize)sz) { std::free(buf); outSize = 0; return nullptr; }
            outSize = rd;
            return buf;
        }

        void ViewerApp::LoadCurrent()
        {
            mTexture.Shutdown();
            ClearAnimation();
            // Libere le source HDR cache de l'image precedente.
            if (mHdrSource != nullptr)
            {
                mHdrSource->Free();
                mHdrSource = nullptr;
            }
            mIsHdr = false;
            mLoaded = false;
            if (mCurrentIdx < 0 || mCurrentIdx >= (int)mFiles.Size()) return;

            const NkString& path = mFiles[(uint32)mCurrentIdx];
            mMeta.fullPath = path;
            // Extrait le nom de fichier (apres le dernier /).
            const char* p = path.CStr();
            const char* slash = std::strrchr(p, '/');
            mMeta.fileName = (slash != nullptr) ? NkString(slash + 1) : path;
            mMeta.format   = ExtractFormatFromPath(path);

            // ── Cas special HDR : Decode RGB96F puis tone map Reinhard ────
            // NkImage::Load(.hdr, 4) ferait un Convert lineaire-clamp qui
            // crame toutes les valeurs > 1.0. On passe par DecodeAnimation
            // pour preserver le float, puis ConvertToTexture(exposure,gamma).
            const bool isHdr = path.EndsWith(".hdr") || path.EndsWith(".HDR");
            if (isHdr)
            {
                usize bytes = 0;
                uint8* data = ReadFileBytes(path.CStr(), bytes);
                if (data != nullptr && bytes > 0)
                {
                    NkImage* hdr = NkHDRCodec::Decode(data, bytes);
                    std::free(data);
                    if (hdr != nullptr && hdr->IsValid())
                    {
                        mHdrSource = hdr;   // owned, libere au prochain LoadCurrent
                        mIsHdr     = true;
                        mMeta.width      = hdr->Width();
                        mMeta.height     = hdr->Height();
                        mMeta.channels   = 3;   // RGB96F = 3 canaux float
                        mMeta.frameCount = 1;
                        mMeta.animated   = false;
                        RebuildHdrTexture();
                        if (mTexture.IsValid())
                        {
                            mLoaded = true;
                            logger.Info("[Viewer] HDR {0}  ({1}x{2}, RGB96F, exposure={3})",
                                        mMeta.fileName.CStr(), mMeta.width, mMeta.height,
                                        mHdrExposure);
                            return;
                        }
                    }
                    else if (hdr != nullptr)
                    {
                        hdr->Free();
                    }
                }
                logger.Warn("[Viewer] HDR Decode FAIL : {0}", path.CStr());
                // Fallback : on continue sur NkImage::Load classique
            }

            // ── Cas special GIF anime : DecodeAnimation → N textures ──────
            // On detecte par extension. Si DecodeAnimation echoue, on
            // fallback sur Decode (premiere frame seulement) via NkImage.
            const bool isGif = path.EndsWith(".gif") || path.EndsWith(".GIF");
            if (isGif)
            {
                usize bytes = 0;
                uint8* data = ReadFileBytes(path.CStr(), bytes);
                if (data != nullptr && bytes > 0)
                {
                    NkGIFAnimation* anim = NkGIFCodec::DecodeAnimation(data, bytes);
                    std::free(data);
                    if (anim != nullptr && anim->frameCount > 0)
                    {
                        // Upload chaque frame en texture GL separee.
                        mAnimTextures.Reserve(anim->frameCount);
                        mAnimDelaysMs.Reserve(anim->frameCount);
                        for (uint32 i = 0; i < anim->frameCount; ++i)
                        {
                            Texture2D* t = new Texture2D();
                            // UploadFromImage libere le NkImage*, on nullify
                            // dans la struct pour eviter le double-free dans
                            // FreeAnimation.
                            NkImage* frameImg = anim->frames[i].image;
                            anim->frames[i].image = nullptr;
                            if (t->UploadFromImage(frameImg))
                            {
                                mAnimTextures.PushBack(t);
                                mAnimDelaysMs.PushBack(anim->frames[i].delayMs);
                            }
                            else
                            {
                                delete t;
                            }
                        }
                        mAnimLoopCount   = anim->loopCount;
                        mMeta.width      = (int)anim->width;
                        mMeta.height     = (int)anim->height;
                        mMeta.channels   = 4;
                        mMeta.frameCount = (int)mAnimTextures.Size();
                        mMeta.animated   = (mAnimTextures.Size() > 1);
                        NkGIFCodec::FreeAnimation(anim);
                        if (mAnimTextures.Size() > 0)
                        {
                            mLoaded = true;
                            logger.Info("[Viewer] GIF anim {0}  ({1}x{2}, {3} frames, loop={4})",
                                        mMeta.fileName.CStr(), mMeta.width, mMeta.height,
                                        mMeta.frameCount, mAnimLoopCount);
                            return;
                        }
                    }
                    // Si DecodeAnimation rate, on tombe dans le fallback Load() en-dessous.
                }
            }

            // ── Cas general : NkImage::Load (statique, 1 frame) ───────────
            NkImage* img = NkImage::Load(path.CStr(), 4);
            if (img == nullptr || !img->IsValid())
            {
                logger.Warn("[Viewer] Decode FAIL : {0}", path.CStr());
                if (img != nullptr) img->Free();
                mMeta.width = mMeta.height = mMeta.channels = 0;
                return;
            }
            mMeta.width      = img->Width();
            mMeta.height     = img->Height();
            mMeta.channels   = img->Channels();
            mMeta.frameCount = 1;
            mMeta.animated   = false;
            // Upload sur le main thread (libere l'image apres).
            if (!mTexture.UploadFromImage(img))
            {
                logger.Warn("[Viewer] Upload GL FAIL : {0}", path.CStr());
                return;
            }
            mLoaded = true;
            logger.Info("[Viewer] Loaded {0}  ({1}x{2}, {3}ch, {4})",
                        mMeta.fileName.CStr(), mMeta.width, mMeta.height,
                        mMeta.channels, mMeta.format.CStr());
        }

        void ViewerApp::NavigateNext(int delta)
        {
            if (mFiles.Size() == 0) return;
            const int n = (int)mFiles.Size();
            mCurrentIdx = ((mCurrentIdx + delta) % n + n) % n;
            LoadCurrent();
        }

    } // namespace demo
} // namespace nkentseu
