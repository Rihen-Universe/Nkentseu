#pragma once

#include "NkUI/NkUI.h"
#include "NkUI/NkUILayout2.h"
#include "NkUIFontIntegration.h"
#include "NKLogger/NkLog.h"
#include "NKMemory/NkFunction.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace nkentseu {
    namespace nkui {

        class NkUIDemo {
            public:
                NkUIDemo() = default;

                bool Init(NkUIContext& ctx,
                          NkUIWindowManager& wm,
                          NkUIFontLoader& fontLoader);

                void Destroy();

                void Render(NkUIContext& ctx,
                            NkUIWindowManager& wm,
                            NkUIDockManager& dock,
                            NkUILayoutStack& ls,
                            float32 dt);

                NkUIFontLoader& GetFontLoader() {
                    return mFontLoader;
                }

            private:
                static constexpr int32 kTexW = 256;
                static constexpr int32 kTexH = 256;
                static constexpr uint32 kTexId = 100001u;
                static constexpr int32 kLogLines = 64;

                NkUIFontLoader mFontLoader;
                NkUIFont* mFont = nullptr;
                NkUIFont mFallbackFont{};

                bool mInitialized = false;
                bool mResetLayout = true;
                float32 mTime = 0.f;

                bool mShowControl = false;
                bool mShowWidgets = false;
                bool mShowLayout = false;
                bool mShowData = false;
                bool mShowTextures = false;
                bool mShowViewport = false;
                bool mShowInput = false;
                bool mShowUnicode = false;
                bool mShowZOrder = false;
                bool mShowAbout = false;
                bool mShowMenuBar = true;
                bool mShowDocking = false;
                bool mPrevShowDocking = false;
                bool mDockBootstrap = false;
                bool mShowDockInspector = false;
                bool mShowDockConsole = false;
                bool mShowDockAssets = false;

                bool mOverlapOpen[3] = {true, true, true};
                int32 mOverlapClicks[3] = {0, 0, 0};

                bool mCheckA = true;
                bool mCheckB = false;
                bool mToggle = true;
                int32 mRadio = 0;
                float32 mSlider = 0.5f;
                int32 mSliderInt = 8;
                float32 mDrag = 1.0f;
                int32 mDragInt = 42;
                int32 mCombo = 1;
                int32 mList = 0;
                bool mListMultiSelect = false;
                bool mListRequireCtrl = false;
                bool mListToggleSelection = true;
                bool mListMask[10] = {};
                float32 mProgress = 0.3f;
                bool mProgressAnim = true;

                char mInputText[256] = "NkUI test";
                char mInputMulti[1024] = "Line 1\nLine 2\nLine 3";
                int32 mInputInt = 123;
                float32 mInputFloat = 3.14f;
                NkUITextInputOptions mTextInputOpts = {};

                float32 mInnerScrollX = 0.f;
                float32 mInnerScrollY = 0.f;

                uint8 mTexturePixels[kTexW * kTexH * 4] = {};
                bool mTextureUploaded = false;
                bool mTextureDirty = true;
                bool mTextureAnimate = false;
                int32 mTexturePattern = 0;
                float32 mTextureTime = 0.f;

                float32 mViewportYaw = 0.f;
                float32 mViewportPitch = 20.f;
                bool mViewportAutoRotate = true;

                char mUnicodeInput[256] = "Hello UTF-8";

                char mLog[kLogLines][128] = {};
                int32 mLogCount = 0;

                void PushLog(const char* fmt, ...);
                void EnsureFont();
                void SetupFallbackFont();

                void GenerateTexture();
                void GenerateChecker();
                void GenerateGradient();
                void GenerateWave();
                void UploadTextureIfNeeded();

                void WinControl(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls);
                void WinWidgets(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls);
                void WinLayout(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls);
                void WinData(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls);
                void WinTextures(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls);
                void WinViewport(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls);
                void WinInput(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls);
                void WinUnicode(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls);
                void WinZOrder(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls);
                void WinAbout(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls);
                void WinOverlap(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls,
                                int32 idx, const char* name, NkVec2 pos, NkVec2 size);
                void RenderMainMenuBar(NkUIContext& ctx, NkUIDrawList& dl);
                void WinDockInspector(NkUIContext& ctx, NkUIWindowManager& wm, NkUIDockManager& dock, NkUILayoutStack& ls);
                void WinDockConsole(NkUIContext& ctx, NkUIWindowManager& wm, NkUIDockManager& dock, NkUILayoutStack& ls);
                void WinDockAssets(NkUIContext& ctx, NkUIWindowManager& wm, NkUIDockManager& dock, NkUILayoutStack& ls);
                void WinDocking(NkUIContext& ctx, NkUIWindowManager& wm, NkUIDockManager& dock, NkUILayoutStack& ls);

                void DrawViewportPreview(NkUIDrawList& dl, NkRect area, NkUIContext& ctx, float32 dt);
                void DrawGrid(NkUIDrawList& dl, NkRect r, NkColor col, int32 div);
        };

        inline void NkUIDemo::PushLog(const char* fmt, ...) {
            if (mLogCount >= kLogLines) {
                for (int32 i = 1; i < kLogLines; ++i) {
                    memory::NkCopy(mLog[i - 1], mLog[i], sizeof(mLog[i - 1]));
                }
                mLogCount = kLogLines - 1;
            }
            va_list ap;
            va_start(ap, fmt);
            ::vsnprintf(mLog[mLogCount], sizeof(mLog[mLogCount]), fmt, ap);
            va_end(ap);
            ++mLogCount;
        }

        inline void NkUIDemo::SetupFallbackFont() {
            memory::NkSet(&mFallbackFont, 0, sizeof(mFallbackFont));
            memory::NkCopy(mFallbackFont.name, "builtin", sizeof("builtin"));
            mFallbackFont.size = 16.f;
            mFallbackFont.isBuiltin = true;
            mFallbackFont.atlas = nullptr;
            mFallbackFont.metrics.ascender = 12.f;
            mFallbackFont.metrics.descender = 4.f;
            mFallbackFont.metrics.lineHeight = 18.f;
            mFallbackFont.metrics.spaceWidth = 8.f;
            mFallbackFont.metrics.tabWidth = 32.f;
        }

        inline void NkUIDemo::EnsureFont() {
            mFont = mFontLoader.DefaultFont();
            if (mFont) return;

            const char* primaryCandidates[] = {
                "Resources/Fonts/ProggyClean.ttf",
                "Resources/Fonts/ProggyTiny.ttf",
                "Resources/Fonts/Roboto-Medium.ttf",
                "Resources/Fonts/Karla-Regular.ttf",
                "Resources/Fonts/DroidSans.ttf",
                "Resources/Fonts/Cousine-Regular.ttf",
                "C:/Windows/Fonts/segoeui.ttf",
                "C:/Windows/Fonts/arial.ttf",
                "C:/Windows/Fonts/consola.ttf",
                nullptr
            };
            const char* emojiCandidates[] = {
                "C:/Windows/Fonts/seguiemj.ttf",
                "C:/Windows/Fonts/seguisym.ttf",
                "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf",
                "/usr/share/fonts/noto/NotoColorEmoji.ttf",
                nullptr
            };

            auto tryLoad = [&](const char* path, NkUIFont** outLoaded) -> bool {
                const uint32 before = mFontLoader.GetFontCount();
                (void)mFontLoader.LoadFontFromFile(path, 16.f, true);
                const uint32 after = mFontLoader.GetFontCount();
                if (after <= before) {
                    return false;
                }
                if (outLoaded) {
                    *outLoaded = mFontLoader.GetFont(after - 1u);
                }
                return true;
            };

            NkUIFont* primary = nullptr;
            for (int32 i = 0; primaryCandidates[i]; ++i) {
                if (tryLoad(primaryCandidates[i], &primary)) {
                    break;
                }
            }

            // Merge a second font into the same atlas for emoji/symbol fallback glyphs.
            // NkUIFont::RenderChar resolves glyphs from the atlas by codepoint.
            for (int32 i = 0; emojiCandidates[i]; ++i) {
                NkUIFont* ignored = nullptr;
                if (tryLoad(emojiCandidates[i], &ignored)) {
                    break;
                }
            }

            if (primary) {
                mFont = primary;
                mFontLoader.UploadAtlas();
            }

            if (!mFont) {
                SetupFallbackFont();
                mFont = &mFallbackFont;
                PushLog("Using builtin fallback font");
            }
        }

        inline void NkUIDemo::GenerateChecker() {
            for (int32 y = 0; y < kTexH; ++y) {
                for (int32 x = 0; x < kTexW; ++x) {
                    const int32 i = (y * kTexW + x) * 4;
                    const bool dark = ((x / 16 + y / 16) & 1) != 0;
                    const uint8 v = dark ? 45 : 210;
                    mTexturePixels[i + 0] = v;
                    mTexturePixels[i + 1] = v;
                    mTexturePixels[i + 2] = v;
                    mTexturePixels[i + 3] = 255;
                }
            }
        }

        inline void NkUIDemo::GenerateGradient() {
            for (int32 y = 0; y < kTexH; ++y) {
                for (int32 x = 0; x < kTexW; ++x) {
                    const int32 i = (y * kTexW + x) * 4;
                    mTexturePixels[i + 0] = static_cast<uint8>((x * 255) / (kTexW - 1));
                    mTexturePixels[i + 1] = static_cast<uint8>((y * 255) / (kTexH - 1));
                    mTexturePixels[i + 2] = 180;
                    mTexturePixels[i + 3] = 255;
                }
            }
        }

        inline void NkUIDemo::GenerateWave() {
            for (int32 y = 0; y < kTexH; ++y) {
                for (int32 x = 0; x < kTexW; ++x) {
                    const float32 fx = static_cast<float32>(x) / static_cast<float32>(kTexW);
                    const float32 fy = static_cast<float32>(y) / static_cast<float32>(kTexH);
                    const float32 s0 = 0.5f + 0.5f * ::sinf(fx * 20.f + mTextureTime * 2.0f);
                    const float32 s1 = 0.5f + 0.5f * ::cosf(fy * 18.f - mTextureTime * 1.7f);
                    const uint8 r = static_cast<uint8>(50.f + 205.f * s0);
                    const uint8 g = static_cast<uint8>(30.f + 225.f * s1);
                    const uint8 b = static_cast<uint8>(80.f + 120.f * (s0 * s1));
                    const int32 i = (y * kTexW + x) * 4;
                    mTexturePixels[i + 0] = r;
                    mTexturePixels[i + 1] = g;
                    mTexturePixels[i + 2] = b;
                    mTexturePixels[i + 3] = 255;
                }
            }
        }

        inline void NkUIDemo::GenerateTexture() {
            if (mTexturePattern == 0) {
                GenerateChecker();
            } else if (mTexturePattern == 1) {
                GenerateGradient();
            } else {
                GenerateWave();
            }
            mTextureDirty = true;
        }

        inline void NkUIDemo::UploadTextureIfNeeded() {
            if (!mTextureDirty) return;

            NkGPUUploadFunc fn = mFontLoader.GetGPUUploadFunc();
            void* dev = mFontLoader.GetGPUDevice();
            if (!fn || !dev) {
                mTextureUploaded = false;
                return;
            }

            mTextureUploaded = fn(dev, kTexId, mTexturePixels, kTexW, kTexH);
            mTextureDirty = false;
        }

        inline bool NkUIDemo::Init(NkUIContext& ctx,
                                   NkUIWindowManager& wm,
                                   NkUIFontLoader& fontLoader) {
            (void)wm;
            (void)fontLoader;

            ctx.SetTheme(NkUITheme::Dark());
            EnsureFont();
            GenerateTexture();
            UploadTextureIfNeeded();
            mTextInputOpts.repeatBackspace = true;
            mTextInputOpts.repeatDelete = true;
            mTextInputOpts.allowEnterNewLine = true;
            mTextInputOpts.multilineLineSpacing = 1.2f;
            mTextInputOpts.multilineCharSpacing = 0.35f;
            mTextInputOpts.multilineWrap = true;

            PushLog("NkUI Test Lab initialized");
            PushLog("Drag windows, resize borders, test scroll and input");

            mInitialized = true;
            mResetLayout = true;
            return true;
        }

        inline void NkUIDemo::Destroy() {
            mInitialized = false;
            mFont = nullptr;
            mFontLoader.Destroy();
        }

        inline void NkUIDemo::DrawGrid(NkUIDrawList& dl, NkRect r, NkColor col, int32 div) {
            if (div < 2) div = 2;
            for (int32 i = 0; i <= div; ++i) {
                const float32 t = static_cast<float32>(i) / static_cast<float32>(div);
                const float32 x = r.x + t * r.w;
                const float32 y = r.y + t * r.h;
                dl.AddLine({x, r.y}, {x, r.y + r.h}, col, 1.f);
                dl.AddLine({r.x, y}, {r.x + r.w, y}, col, 1.f);
            }
        }

        inline void NkUIDemo::DrawViewportPreview(NkUIDrawList& dl, NkRect area, NkUIContext& ctx, float32 dt) {
            if (mViewportAutoRotate) {
                mViewportYaw += dt * 35.f;
            }
            if (ctx.IsHovered(area) && ctx.input.IsMouseDown(0)) {
                mViewportYaw += ctx.input.mouseDelta.x * 0.3f;
                mViewportPitch += ctx.input.mouseDelta.y * 0.3f;
            }

            dl.AddRectFilled(area, {16, 20, 30, 255}, 4.f);
            DrawGrid(dl, area, {45, 55, 80, 180}, 10);

            const NkVec2 c = {area.x + area.w * 0.5f, area.y + area.h * 0.5f};
            const float32 rr = (area.h < area.w ? area.h : area.w) * 0.25f;
            const float32 ay = mViewportYaw * NKUI_PI / 180.f;
            const float32 ap = mViewportPitch * NKUI_PI / 180.f;

            NkVec2 pts[4] = {};
            for (int32 i = 0; i < 4; ++i) {
                const float32 a = ay + i * NKUI_PI * 0.5f;
                const float32 x = ::cosf(a) * rr;
                const float32 z = ::sinf(a) * rr;
                const float32 y = ::sinf(ap + i * 0.9f) * rr * 0.4f;
                pts[i] = {c.x + x + z * 0.4f, c.y + y - z * 0.2f};
            }

            dl.AddLine(pts[0], pts[1], {240, 120, 80, 255}, 2.f);
            dl.AddLine(pts[1], pts[2], {240, 120, 80, 255}, 2.f);
            dl.AddLine(pts[2], pts[3], {240, 120, 80, 255}, 2.f);
            dl.AddLine(pts[3], pts[0], {240, 120, 80, 255}, 2.f);
            dl.AddLine(pts[0], pts[2], {120, 200, 255, 200}, 1.5f);
            dl.AddLine(pts[1], pts[3], {120, 200, 255, 200}, 1.5f);

            char info[96];
            ::snprintf(info, sizeof(info), "Yaw %.1f Pitch %.1f", mViewportYaw, mViewportPitch);
            if (mFont) {
                mFont->RenderText(dl, {area.x + 8.f, area.y + 16.f}, info, {220, 220, 230, 255});
            }
        }

        inline void NkUIDemo::WinControl(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls) {
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({12.f, 36.f});
                NkUIWindow::SetNextWindowSize({360.f, 300.f});
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "UI Test Lab", &mShowControl)) {
                NkUI::Text(ctx, ls, dl, *mFont, "This window toggles all test panels.");
                NkUI::Separator(ctx, ls, dl);

                NkUI::Checkbox(ctx, ls, dl, *mFont, "Widgets", mShowWidgets);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Layout + Scroll", mShowLayout);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Data (Tree/Table)", mShowData);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Textures + Images", mShowTextures);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Viewport Preview", mShowViewport);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Input Diagnostics", mShowInput);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Unicode + Font", mShowUnicode);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Z-Order Test", mShowZOrder);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "About", mShowAbout);

                if (NkUI::Button(ctx, ls, dl, *mFont, "Reset Window Layout")) {
                    mResetLayout = true;
                    PushLog("Window layout reset requested");
                }

                char ids[160];
                ::snprintf(ids, sizeof(ids), "hot=%u active=%u focus=%u hoveredWnd=%u",
                           ctx.hotId, ctx.activeId, ctx.focusId,
                           ctx.wm ? ctx.wm->hoveredId : 0u);
                NkUI::Text(ctx, ls, dl, *mFont, ids);

                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::WinWidgets(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls) {
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            static const char* comboItems[] = {"One", "Two", "Three", "Four"};
            static const char* listItems[] = {
                "Alpha", "Bravo", "Charlie", "Delta", "Echo", "Foxtrot", "Golf", "Hotel", "India", "Juliet"
            };

            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({390.f, 36.f});
                NkUIWindow::SetNextWindowSize({420.f, 520.f});
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "Widgets", &mShowWidgets)) {
                NkUI::Button(ctx, ls, dl, *mFont, "Simple Button");
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Check A", mCheckA);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Check B", mCheckB);
                NkUI::Toggle(ctx, ls, dl, *mFont, "Toggle", mToggle);
                NkUI::RadioButton(ctx, ls, dl, *mFont, "Radio 0", mRadio, 0);
                NkUI::RadioButton(ctx, ls, dl, *mFont, "Radio 1", mRadio, 1);

                NkUI::SliderFloat(ctx, ls, dl, *mFont, "Slider", mSlider, 0.f, 1.f);
                NkUI::SliderInt(ctx, ls, dl, *mFont, "Slider Int", mSliderInt, 0, 20);
                NkUI::DragFloat(ctx, ls, dl, *mFont, "Drag Float", mDrag, 0.02f, -10.f, 10.f);
                NkUI::DragInt(ctx, ls, dl, *mFont, "Drag Int", mDragInt, 1.f, -100, 100);

                NkUI::Checkbox(ctx, ls, dl, *mFont, "Repeat Backspace", mTextInputOpts.repeatBackspace);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Repeat Delete", mTextInputOpts.repeatDelete);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Multiline Enter Newline", mTextInputOpts.allowEnterNewLine);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Multiline Wrap", mTextInputOpts.multilineWrap);
                NkUI::SliderFloat(ctx, ls, dl, *mFont, "Multiline Spacing",
                                  mTextInputOpts.multilineLineSpacing, 1.0f, 1.8f, "%.2f");
                NkUI::SliderFloat(ctx, ls, dl, *mFont, "Multiline Char Spacing",
                                  mTextInputOpts.multilineCharSpacing, 0.0f, 3.0f, "%.2f");

                NkUI::InputText(ctx, ls, dl, *mFont, "Input", mInputText,
                                static_cast<int32>(sizeof(mInputText)), 0.f, &mTextInputOpts);
                NkUI::InputInt(ctx, ls, dl, *mFont, "Input Int", mInputInt, &mTextInputOpts);
                NkUI::InputFloat(ctx, ls, dl, *mFont, "Input Float", mInputFloat, "%.3f", &mTextInputOpts);
                NkUI::InputMultiline(ctx, ls, dl, *mFont, "Multiline", mInputMulti,
                                     static_cast<int32>(sizeof(mInputMulti)), {0.f, 110.f}, &mTextInputOpts);

                NkUI::Combo(ctx, ls, dl, *mFont, "Combo", mCombo, comboItems, 4);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "List Multi-select", mListMultiSelect);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "List Toggle Selection", mListToggleSelection);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "List Require Ctrl", mListRequireCtrl);
                if (NkUI::Button(ctx, ls, dl, *mFont, "Clear List Selection")) {
                    for (int32 i = 0; i < 10; ++i) mListMask[i] = false;
                }
                NkUIListBoxOptions listOpts{};
                listOpts.multiSelect = mListMultiSelect;
                listOpts.toggleSelection = mListToggleSelection;
                listOpts.requireCtrlForMulti = mListRequireCtrl;
                listOpts.selectedMask = mListMask;
                listOpts.selectedMaskCount = 10;
                NkUI::ListBox(ctx, ls, dl, *mFont, "List", mList, listItems, 10, 6, &listOpts);

                if (mProgressAnim) {
                    mProgress += ctx.dt * 0.15f;
                    if (mProgress > 1.f) mProgress = 0.f;
                }
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Animate Progress", mProgressAnim);
                NkUI::ProgressBar(ctx, ls, dl, mProgress, {0.f, 0.f}, "Progress");
                NkUI::ProgressBarVertical(ctx, ls, dl, mProgress, {30.f, 110.f}, "V");
                char circularLabel[24];
                ::snprintf(circularLabel, sizeof(circularLabel), "%d%%", static_cast<int32>(mProgress * 100.f));
                NkUI::ProgressBarCircular(ctx, ls, dl, *mFont, mProgress, 72.f, circularLabel);

                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::WinLayout(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls) {
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({826.f, 36.f});
                NkUIWindow::SetNextWindowSize({430.f, 520.f});
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "Layout + Scroll", &mShowLayout)) {
                NkUI::Text(ctx, ls, dl, *mFont, "Window scroll should appear when content overflows.");

                for (int32 i = 0; i < 22; ++i) {
                    char line[64];
                    ::snprintf(line, sizeof(line), "Overflow line %d", i);
                    NkUI::Text(ctx, ls, dl, *mFont, line);
                }

                NkUI::Separator(ctx, ls, dl);
                NkUI::Text(ctx, ls, dl, *mFont, "Inner scroll region:");

                NkRect region = NkUILayout::NextItemRect(ctx, ls, 390.f, 190.f);
                if (NkUI::BeginScrollRegion(ctx, ls, "inner_scroll_demo", region, &mInnerScrollX, &mInnerScrollY)) {
                    for (int32 i = 0; i < 60; ++i) {
                        char row[96];
                        ::snprintf(row, sizeof(row), "Row %02d  wheel here to scroll", i);
                        NkUI::Text(ctx, ls, dl, *mFont, row);
                    }
                    NkUI::EndScrollRegion(ctx, ls);
                }
                NkUILayout::AdvanceItem(ctx, ls, region);

                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::WinData(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls) {
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({12.f, 350.f});
                NkUIWindow::SetNextWindowSize({400.f, 340.f});
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "Data (Tree + Table)", &mShowData)) {
                if (NkUI::TreeNode(ctx, ls, dl, *mFont, "Root##tree_root")) {
                    NkUI::Text(ctx, ls, dl, *mFont, "Child A");
                    NkUI::Text(ctx, ls, dl, *mFont, "Child B");
                    if (NkUI::TreeNode(ctx, ls, dl, *mFont, "Subtree##tree_sub")) {
                        NkUI::Text(ctx, ls, dl, *mFont, "Leaf 1");
                        NkUI::Text(ctx, ls, dl, *mFont, "Leaf 2");
                        NkUI::TreePop(ctx, ls);
                    }
                    NkUI::TreePop(ctx, ls);
                }

                NkUI::Separator(ctx, ls, dl);
                if (NkUI::BeginTable(ctx, ls, dl, *mFont, "demo_table", 3, 0.f)) {
                    NkUI::TableHeader(ctx, ls, dl, *mFont, "ID");
                    NkUI::TableHeader(ctx, ls, dl, *mFont, "Name");
                    NkUI::TableHeader(ctx, ls, dl, *mFont, "Value");

                    for (int32 i = 0; i < 8; ++i) {
                        if (NkUI::TableNextRow(ctx, ls, dl, true)) {
                            char idBuf[16];
                            char valueBuf[32];
                            ::snprintf(idBuf, sizeof(idBuf), "%d", i);
                            ::snprintf(valueBuf, sizeof(valueBuf), "%.2f", 1.0f + i * 0.37f);

                            NkUI::Text(ctx, ls, dl, *mFont, idBuf);
                            NkUI::TableNextCell(ctx, ls);
                            NkUI::Text(ctx, ls, dl, *mFont, (i & 1) ? "Item B" : "Item A");
                            NkUI::TableNextCell(ctx, ls);
                            NkUI::Text(ctx, ls, dl, *mFont, valueBuf);
                        }
                    }
                    NkUI::EndTable(ctx, ls, dl);
                }

                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::WinTextures(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls) {
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            static const char* patterns[] = {"Checker", "Gradient", "Wave"};

            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({430.f, 570.f});
                NkUIWindow::SetNextWindowSize({420.f, 300.f});
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "Textures + Images", &mShowTextures)) {
                NkUI::Combo(ctx, ls, dl, *mFont, "Pattern", mTexturePattern, patterns, 3);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Animate", mTextureAnimate);

                if (NkUI::Button(ctx, ls, dl, *mFont, "Regenerate Texture")) {
                    GenerateTexture();
                    UploadTextureIfNeeded();
                    PushLog("Texture regenerated");
                }

                if (mTextureAnimate && mTexturePattern == 2) {
                    mTextureTime += ctx.dt;
                    GenerateTexture();
                    UploadTextureIfNeeded();
                }

                const char* status = mTextureUploaded ? "uploaded" : "not uploaded";
                NkUI::LabelValue(ctx, ls, dl, *mFont, "GPU texture", status);

                NkUI::ButtonImage(ctx, ls, dl, kTexId, {120.f, 90.f});

                NkRect imageRect = NkUILayout::NextItemRect(ctx, ls, 360.f, 150.f);
                dl.AddRectFilled(imageRect, {18, 18, 24, 255}, 4.f);
                dl.AddRect(imageRect, {85, 85, 100, 255}, 1.f, 4.f);
                dl.AddImage(kTexId, {imageRect.x + 4.f, imageRect.y + 4.f, imageRect.w - 8.f, imageRect.h - 8.f});
                NkUILayout::AdvanceItem(ctx, ls, imageRect);

                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::WinViewport(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls) {
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({870.f, 570.f});
                NkUIWindow::SetNextWindowSize({390.f, 300.f});
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "Viewport Preview", &mShowViewport)) {
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Auto rotate", mViewportAutoRotate);

                NkRect area = NkUILayout::NextItemRect(ctx, ls, 340.f, 190.f);
                DrawViewportPreview(dl, area, ctx, ctx.dt);
                NkUILayout::AdvanceItem(ctx, ls, area);

                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::WinInput(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls) {
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({1274.f, 36.f});
                NkUIWindow::SetNextWindowSize({320.f, 400.f});
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "Input Diagnostics", &mShowInput)) {
                char line[160];
                ::snprintf(line, sizeof(line), "Mouse pos: %.1f %.1f", ctx.input.mousePos.x, ctx.input.mousePos.y);
                NkUI::Text(ctx, ls, dl, *mFont, line);
                ::snprintf(line, sizeof(line), "Mouse delta: %.2f %.2f", ctx.input.mouseDelta.x, ctx.input.mouseDelta.y);
                NkUI::Text(ctx, ls, dl, *mFont, line);
                ::snprintf(line, sizeof(line), "Wheel V/H: %.2f %.2f", ctx.input.mouseWheel, ctx.input.mouseWheelH);
                NkUI::Text(ctx, ls, dl, *mFont, line);
                ::snprintf(line, sizeof(line), "Buttons down: L=%d R=%d M=%d",
                           ctx.input.mouseDown[0] ? 1 : 0,
                           ctx.input.mouseDown[1] ? 1 : 0,
                           ctx.input.mouseDown[2] ? 1 : 0);
                NkUI::Text(ctx, ls, dl, *mFont, line);
                ::snprintf(line, sizeof(line), "Clicked: L=%d R=%d M=%d",
                           ctx.input.mouseClicked[0] ? 1 : 0,
                           ctx.input.mouseClicked[1] ? 1 : 0,
                           ctx.input.mouseClicked[2] ? 1 : 0);
                NkUI::Text(ctx, ls, dl, *mFont, line);

                ::snprintf(line, sizeof(line), "Input chars this frame: %d", ctx.input.numInputChars);
                NkUI::Text(ctx, ls, dl, *mFont, line);

                NkUI::Separator(ctx, ls, dl);
                NkUI::Text(ctx, ls, dl, *mFont, "Recent demo log:");
                for (int32 i = (mLogCount > 8 ? mLogCount - 8 : 0); i < mLogCount; ++i) {
                    NkUI::Text(ctx, ls, dl, *mFont, mLog[i]);
                }

                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::WinUnicode(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls) {
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({1274.f, 450.f});
                NkUIWindow::SetNextWindowSize({320.f, 250.f});
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "Unicode + Font", &mShowUnicode)) {
                NkUI::Text(ctx, ls, dl, *mFont, "UTF-8 samples:");
                NkUI::Text(ctx, ls, dl, *mFont, "Fr: Bonjour, ca va?");
                NkUI::Text(ctx, ls, dl, *mFont, "Es: Hola senor, accion");
                NkUI::Text(ctx, ls, dl, *mFont, "Emoji: 😀 🚀 🎮 ✅");

                NkUI::InputText(ctx, ls, dl, *mFont, "Edit UTF-8", mUnicodeInput, static_cast<int32>(sizeof(mUnicodeInput)));
                NkUI::Text(ctx, ls, dl, *mFont, mUnicodeInput);

                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::WinOverlap(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls,
                                          int32 idx, const char* name, NkVec2 pos, NkVec2 size) {
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos(pos);
                NkUIWindow::SetNextWindowSize(size);
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, name, &mOverlapOpen[idx])) {
                char caption[96];
                ::snprintf(caption, sizeof(caption), "Clicks in this window: %d", mOverlapClicks[idx]);
                NkUI::Text(ctx, ls, dl, *mFont, caption);
                if (NkUI::Button(ctx, ls, dl, *mFont, "Click Me")) {
                    ++mOverlapClicks[idx];
                }
                NkUI::Text(ctx, ls, dl, *mFont, "Drag title bar and resize borders.");
                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::WinZOrder(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls) {
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({12.f, 706.f});
                NkUIWindow::SetNextWindowSize({400.f, 160.f});
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "Z-Order Test", &mShowZOrder)) {
                NkUI::Text(ctx, ls, dl, *mFont, "Three overlap windows are opened.");
                NkUI::Text(ctx, ls, dl, *mFont, "Front window should capture click/drag.");
                NkUI::Text(ctx, ls, dl, *mFont, "Click counters must change only on top window.");
                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::WinAbout(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls) {
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({1274.f, 710.f});
                NkUIWindow::SetNextWindowSize({320.f, 160.f});
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "About", &mShowAbout, NkUIWindowFlags::NK_NO_RESIZE)) {
                NkUI::Text(ctx, ls, dl, *mFont, "NkUI Demo Test Lab");
                NkUI::Text(ctx, ls, dl, *mFont, "Goal: validate widgets, scroll, input,");
                NkUI::Text(ctx, ls, dl, *mFont, "textures, unicode and window behavior.");
                NkUI::Text(ctx, ls, dl, *mFont, "If one panel fails, report panel name.");
                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::RenderMainMenuBar(NkUIContext& ctx, NkUIDrawList& dl) {
            if (!mFont) return;
            (void)dl;
            // Menubar sur l'overlay: évite toute interaction avec des clips de fenêtres.
            NkUIDrawList& menuDL = ctx.layers[NkUIContext::LAYER_OVERLAY];
            const NkRect barR = {0.f, 0.f, static_cast<float32>(ctx.viewW), ctx.theme.metrics.titleBarHeight};
            if (!NkUIMenu::BeginMenuBar(ctx, menuDL, *mFont, barR)) return;

            if (NkUIMenu::BeginMenu(ctx, menuDL, *mFont, "File")) {
                if (NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Reset Layout")) {
                    mResetLayout = true;
                    mDockBootstrap = true;
                    PushLog("Layout reset from menu");
                }
                if (NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Regenerate Texture")) {
                    GenerateTexture();
                    UploadTextureIfNeeded();
                    PushLog("Texture regenerated from menu");
                }
                NkUIMenu::EndMenu(ctx);
            }

            if (NkUIMenu::BeginMenu(ctx, menuDL, *mFont, "View")) {
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Docking Demo", nullptr, &mShowDocking);
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "UI Test Lab", nullptr, &mShowControl);
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Widgets", nullptr, &mShowWidgets);
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Layout + Scroll", nullptr, &mShowLayout);
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Data", nullptr, &mShowData);
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Textures", nullptr, &mShowTextures);
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Viewport", nullptr, &mShowViewport);
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Input", nullptr, &mShowInput);
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Unicode", nullptr, &mShowUnicode);
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Z-Order", nullptr, &mShowZOrder);
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "About", nullptr, &mShowAbout);
                NkUIMenu::EndMenu(ctx);
            }

            if (NkUIMenu::BeginMenu(ctx, menuDL, *mFont, "Dock")) {
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Inspector", nullptr, &mShowDockInspector);
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Console", nullptr, &mShowDockConsole);
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Assets", nullptr, &mShowDockAssets);
                if (NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Redock All")) {
                    mDockBootstrap = true;
                }
                NkUIMenu::EndMenu(ctx);
            }

            NkUIMenu::EndMenuBar(ctx);
        }

        inline void NkUIDemo::WinDockInspector(NkUIContext& ctx, NkUIWindowManager& wm, NkUIDockManager& dock, NkUILayoutStack& ls) {
            if (!mShowDocking || !mShowDockInspector || !mFont) return;
            NkUIWindowState* ws = wm.Find("Dock Inspector");
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            if (ws && ws->isDocked) {
                if (!ws->isOpen) {
                    mShowDockInspector = false;
                    return;
                }
                if (dock.BeginDocked(ctx, wm, dl, "Dock Inspector")) {
                    char ids[160];
                    ::snprintf(ids, sizeof(ids), "hover=%u active=%u focus=%u", ctx.hotId, ctx.activeId, ctx.focusId);
                    NkUI::Text(ctx, ls, dl, *mFont, "Dock Inspector");
                    NkUI::Text(ctx, ls, dl, *mFont, ids);
                    NkUI::Checkbox(ctx, ls, dl, *mFont, "Auto Rotate Viewport", mViewportAutoRotate);
                    NkUI::SliderFloat(ctx, ls, dl, *mFont, "Yaw", mViewportYaw, -180.f, 180.f);
                    NkUI::SliderFloat(ctx, ls, dl, *mFont, "Pitch", mViewportPitch, -89.f, 89.f);
                    dock.EndDocked(ctx, dl);
                }
                return;
            }

            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({20.f, 110.f});
                NkUIWindow::SetNextWindowSize({320.f, 260.f});
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "Dock Inspector", &mShowDockInspector)) {
                char ids[160];
                ::snprintf(ids, sizeof(ids), "hover=%u active=%u focus=%u", ctx.hotId, ctx.activeId, ctx.focusId);
                NkUI::Text(ctx, ls, dl, *mFont, "Dock Inspector");
                NkUI::Text(ctx, ls, dl, *mFont, ids);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Auto Rotate Viewport", mViewportAutoRotate);
                NkUI::SliderFloat(ctx, ls, dl, *mFont, "Yaw", mViewportYaw, -180.f, 180.f);
                NkUI::SliderFloat(ctx, ls, dl, *mFont, "Pitch", mViewportPitch, -89.f, 89.f);
                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::WinDockConsole(NkUIContext& ctx, NkUIWindowManager& wm, NkUIDockManager& dock, NkUILayoutStack& ls) {
            if (!mShowDocking || !mShowDockConsole || !mFont) return;
            NkUIWindowState* ws = wm.Find("Dock Console");
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            if (ws && ws->isDocked) {
                if (!ws->isOpen) {
                    mShowDockConsole = false;
                    return;
                }
                if (dock.BeginDocked(ctx, wm, dl, "Dock Console")) {
                    NkUI::Text(ctx, ls, dl, *mFont, "Dock Console");
                    for (int32 i = (mLogCount > 10 ? mLogCount - 10 : 0); i < mLogCount; ++i) {
                        NkUI::Text(ctx, ls, dl, *mFont, mLog[i]);
                    }
                    dock.EndDocked(ctx, dl);
                }
                return;
            }

            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({20.f, 380.f});
                NkUIWindow::SetNextWindowSize({500.f, 220.f});
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "Dock Console", &mShowDockConsole)) {
                NkUI::Text(ctx, ls, dl, *mFont, "Dock Console");
                for (int32 i = (mLogCount > 10 ? mLogCount - 10 : 0); i < mLogCount; ++i) {
                    NkUI::Text(ctx, ls, dl, *mFont, mLog[i]);
                }
                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::WinDockAssets(NkUIContext& ctx, NkUIWindowManager& wm, NkUIDockManager& dock, NkUILayoutStack& ls) {
            if (!mShowDocking || !mShowDockAssets || !mFont) return;
            NkUIWindowState* ws = wm.Find("Dock Assets");
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            if (ws && ws->isDocked) {
                if (!ws->isOpen) {
                    mShowDockAssets = false;
                    return;
                }
                if (dock.BeginDocked(ctx, wm, dl, "Dock Assets")) {
                    static const char* assets[] = {
                        "textures/checker.png",
                        "textures/gradient.png",
                        "models/cube.mesh",
                        "models/sphere.mesh",
                        "scenes/demo_scene.json",
                        "fonts/arial.ttf"
                    };
                    NkUI::Text(ctx, ls, dl, *mFont, "Dock Assets");
                    for (int32 i = 0; i < 6; ++i) {
                        char label[96];
                        ::snprintf(label, sizeof(label), "Open##asset_%d", i);
                        NkUI::LabelValue(ctx, ls, dl, *mFont, assets[i], "");
                        if (NkUI::Button(ctx, ls, dl, *mFont, label, {80.f, 0.f})) {
                            PushLog("Asset action: %s", assets[i]);
                        }
                    }
                    dock.EndDocked(ctx, dl);
                }
                return;
            }

            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({530.f, 380.f});
                NkUIWindow::SetNextWindowSize({360.f, 220.f});
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "Dock Assets", &mShowDockAssets)) {
                static const char* assets[] = {
                    "textures/checker.png",
                    "textures/gradient.png",
                    "models/cube.mesh",
                    "models/sphere.mesh",
                    "scenes/demo_scene.json",
                    "fonts/arial.ttf"
                };
                NkUI::Text(ctx, ls, dl, *mFont, "Dock Assets");
                for (int32 i = 0; i < 6; ++i) {
                    char label[96];
                    ::snprintf(label, sizeof(label), "Open##asset_%d", i);
                    NkUI::LabelValue(ctx, ls, dl, *mFont, assets[i], "");
                    if (NkUI::Button(ctx, ls, dl, *mFont, label, {80.f, 0.f})) {
                        PushLog("Asset action: %s", assets[i]);
                    }
                }
                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::WinDocking(NkUIContext& ctx, NkUIWindowManager& wm, NkUIDockManager& dock, NkUILayoutStack& ls) {
            if (!mShowDocking || !mFont) return;
            (void)ctx;
            (void)ls;

            if (mDockBootstrap && dock.rootIdx >= 0) {
                bool didDock = false;
                if (mShowDockInspector) {
                    NkUIWindowState* wi = wm.Find("Dock Inspector");
                    if (wi && !wi->isDocked) {
                        didDock |= dock.DockWindow(wm, wi->id, dock.rootIdx, NkUIDockDrop::NK_CENTER);
                    }
                }
                if (mShowDockConsole) {
                    NkUIWindowState* wc = wm.Find("Dock Console");
                    if (wc && !wc->isDocked) {
                        didDock |= dock.DockWindow(wm, wc->id, dock.rootIdx, NkUIDockDrop::NK_CENTER);
                    }
                }
                if (mShowDockAssets) {
                    NkUIWindowState* wa = wm.Find("Dock Assets");
                    if (wa && !wa->isDocked) {
                        didDock |= dock.DockWindow(wm, wa->id, dock.rootIdx, NkUIDockDrop::NK_CENTER);
                    }
                }
                mDockBootstrap = false;
                if (didDock) {
                    PushLog("Dock layout initialized");
                }
            }
        }

        inline void NkUIDemo::Render(NkUIContext& ctx,
                                     NkUIWindowManager& wm,
                                     NkUIDockManager& dock,
                                     NkUILayoutStack& ls,
                                     float32 dt) {
            if (!mInitialized) return;
            if (!mFont) {
                EnsureFont();
                if (!mFont) return;
            }
            ctx.SetActiveFont(mFont);

            mTime += dt;

            NkUIDrawList& bg = ctx.layers[NkUIContext::LAYER_BG];
            bg.AddRectFilled({0.f, 0.f, static_cast<float32>(ctx.viewW), static_cast<float32>(ctx.viewH)}, ctx.theme.colors.bgPrimary);
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];

            const float32 menuH = mShowMenuBar ? ctx.theme.metrics.titleBarHeight : 0.f;
            if (mShowMenuBar) {
                RenderMainMenuBar(ctx, dl);
            }

            if (mShowDocking && !mPrevShowDocking) {
                mShowDockInspector = true;
                mShowDockConsole = true;
                mShowDockAssets = true;
                mDockBootstrap = true;
            }
            mPrevShowDocking = mShowDocking;

            if (mResetLayout) {
                mDockBootstrap = true;
            }

            if (mShowDocking) {
                const NkRect dockRect = {0.f, menuH, static_cast<float32>(ctx.viewW), static_cast<float32>(ctx.viewH) - menuH};
                dock.SetViewport(dockRect);
                dock.BeginFrame(ctx, wm, dl, *mFont);
                dock.Render(ctx, dl, *mFont, wm, ls);
            }

            // Root widgets (outside any window) to validate non-window UI path.
            // Keep them on the windows layer so popup layers (menus/combos) stay visible on top.
            {
                NkUIDrawList& root = ctx.layers[NkUIContext::LAYER_WINDOWS];
                NkUIDrawList* savedDl = ctx.dl;
                NkVec2 savedCursor = ctx.cursor;
                NkVec2 savedCursorStart = ctx.cursorStart;
                const NkUIID savedWindowId = ctx.currentWindowId;

                ctx.dl = &root;
                ctx.currentWindowId = NKUI_ID_NONE;
                ctx.SetCursor({8.f, menuH + 6.f});
                ctx.cursorStart = ctx.cursor;

                static bool rootToggle = true;
                static float32 rootOpacity = 0.75f;
                static int32 rootClicks = 0;
                static int32 rootValuePlacement = 0;
                static const char* rootValuePlacementItems[] = {
                    "Above thumb",
                    "Below thumb",
                    "Left of slider",
                    "Right of slider",
                    "In label",
                    "Hidden"
                };

                NkUISliderValueOptions rootValueOpts{};
                switch (rootValuePlacement) {
                    default:
                    case 0: rootValueOpts.placement = NkUISliderValuePlacement::NK_ABOVE_THUMB; break;
                    case 1: rootValueOpts.placement = NkUISliderValuePlacement::NK_BELOW_THUMB; break;
                    case 2: rootValueOpts.placement = NkUISliderValuePlacement::NK_LEFT_OF_SLIDER; break;
                    case 3: rootValueOpts.placement = NkUISliderValuePlacement::NK_RIGHT_OF_SLIDER; break;
                    case 4: rootValueOpts.placement = NkUISliderValuePlacement::NK_IN_LABEL; break;
                    case 5: rootValueOpts.placement = NkUISliderValuePlacement::NK_HIDDEN; break;
                }
                rootValueOpts.followThumb = true;
                rootValueOpts.gap = 4.f;

                NkUI::BeginRow(ctx, ls, 24.f);
                if (NkUI::Button(ctx, ls, root, *mFont, "Root Button##root_btn")) {
                    ++rootClicks;
                }
                NkUI::Checkbox(ctx, ls, root, *mFont, "Root Toggle##root_toggle", rootToggle);
                NkUI::SliderFloat(
                    ctx, ls, root, *mFont,
                    "Root Opacity##root_opacity", rootOpacity, 0.f, 1.f,
                    "%.2f", 260.f, &rootValueOpts);
                NkUI::EndRow(ctx, ls);

                NkUI::Combo(
                    ctx, ls, root, *mFont,
                    "Root Value Pos##root_value_pos",
                    rootValuePlacement,
                    rootValuePlacementItems,
                    6,
                    220.f);

                char rootInfo[96];
                ::snprintf(rootInfo, sizeof(rootInfo), "Root clicks: %d", rootClicks);
                NkUI::Text(ctx, ls, root, *mFont, rootInfo);

                ctx.dl = savedDl;
                ctx.cursor = savedCursor;
                ctx.cursorStart = savedCursorStart;
                ctx.currentWindowId = savedWindowId;
            }

            enum class TaskId : uint8 {
                Control,
                Widgets,
                Layout,
                Data,
                Textures,
                Viewport,
                Input,
                Unicode,
                DockInspector,
                DockConsole,
                DockAssets,
                ZOrder,
                OverlapA,
                OverlapB,
                OverlapC,
                About
            };
            struct Task {
                TaskId id;
                int32 order;
            };

            Task tasks[24] = {};
            int32 taskCount = 0;
            int32 fallbackOrder = 0;

            auto taskOrder = [&](const char* windowName) -> int32 {
                if (NkUIWindowState* ws = wm.Find(windowName)) {
                    return ws->zOrder;
                }
                return fallbackOrder;
            };
            auto pushTask = [&](TaskId id, const char* windowName, bool enabled) {
                if (!enabled) {
                    ++fallbackOrder;
                    return;
                }
                tasks[taskCount++] = {id, taskOrder(windowName)};
                ++fallbackOrder;
            };

            pushTask(TaskId::Control,  "UI Test Lab",        mShowControl);
            pushTask(TaskId::Widgets,  "Widgets",            mShowWidgets);
            pushTask(TaskId::Layout,   "Layout + Scroll",    mShowLayout);
            pushTask(TaskId::Data,     "Data (Tree + Table)",mShowData);
            pushTask(TaskId::Textures, "Textures + Images",  mShowTextures);
            pushTask(TaskId::Viewport, "Viewport Preview",   mShowViewport);
            pushTask(TaskId::Input,    "Input Diagnostics",  mShowInput);
            pushTask(TaskId::Unicode,  "Unicode + Font",     mShowUnicode);
            pushTask(TaskId::DockInspector, "Dock Inspector", mShowDocking && mShowDockInspector);
            pushTask(TaskId::DockConsole,   "Dock Console",   mShowDocking && mShowDockConsole);
            pushTask(TaskId::DockAssets,    "Dock Assets",    mShowDocking && mShowDockAssets);
            pushTask(TaskId::ZOrder,   "Z-Order Test",       mShowZOrder);
            pushTask(TaskId::OverlapA, "Overlap A",          mShowZOrder);
            pushTask(TaskId::OverlapB, "Overlap B",          mShowZOrder);
            pushTask(TaskId::OverlapC, "Overlap C",          mShowZOrder);
            pushTask(TaskId::About,    "About",              mShowAbout);

            std::sort(tasks, tasks + taskCount, [](const Task& a, const Task& b) {
                return a.order < b.order;
            });

            for (int32 i = 0; i < taskCount; ++i) {
                switch (tasks[i].id) {
                    case TaskId::Control: {
                        WinControl(ctx, wm, ls);
                    } break;
                    case TaskId::Widgets: {
                        WinWidgets(ctx, wm, ls);
                    } break;
                    case TaskId::Layout: {
                        WinLayout(ctx, wm, ls);
                    } break;
                    case TaskId::Data: {
                        WinData(ctx, wm, ls);
                    } break;
                    case TaskId::Textures: {
                        WinTextures(ctx, wm, ls);
                    } break;
                    case TaskId::Viewport: {
                        WinViewport(ctx, wm, ls);
                    } break;
                    case TaskId::Input: {
                        WinInput(ctx, wm, ls);
                    } break;
                    case TaskId::Unicode: {
                        WinUnicode(ctx, wm, ls);
                    } break;
                    case TaskId::DockInspector: {
                        WinDockInspector(ctx, wm, dock, ls);
                    } break;
                    case TaskId::DockConsole: {
                        WinDockConsole(ctx, wm, dock, ls);
                    } break;
                    case TaskId::DockAssets: {
                        WinDockAssets(ctx, wm, dock, ls);
                    } break;
                    case TaskId::ZOrder: {
                        WinZOrder(ctx, wm, ls);
                    } break;
                    case TaskId::OverlapA: {
                        WinOverlap(ctx, wm, ls, 0, "Overlap A", {520.f, 360.f}, {300.f, 180.f});
                    } break;
                    case TaskId::OverlapB: {
                        WinOverlap(ctx, wm, ls, 1, "Overlap B", {560.f, 390.f}, {300.f, 180.f});
                    } break;
                    case TaskId::OverlapC: {
                        WinOverlap(ctx, wm, ls, 2, "Overlap C", {600.f, 420.f}, {300.f, 180.f});
                    } break;
                    case TaskId::About: {
                        WinAbout(ctx, wm, ls);
                    } break;
                }
            }

            WinDocking(ctx, wm, dock, ls);

            mResetLayout = false;
        }

    }
}
