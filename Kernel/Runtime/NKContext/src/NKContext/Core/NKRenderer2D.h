#pragma once
// =============================================================================
// NKRenderer2D.h — Single include for the entire NKRenderer 2D system
//
// Usage in nkmain.cpp:
//   #include "NKRenderer2D.h"
//   using namespace nkentseu::renderer;
//
//   // In nkmain():
//   NkIGraphicsContext* gfx = NkContextFactory::Create(window, desc);
//   auto r2d = NkRenderer2DFactory::CreateUnique(gfx);
//
//   // Load resources:
//   NkTexture bg; bg.LoadFromFile(*r2d, "assets/bg.png");
//   NkFont    font; font.LoadFromFile(*r2d, "assets/Roboto-Regular.ttf");
//   NkSprite  sprite(bg);
//   NkText    label(font, "Hello, NkEngine!", 32);
//   label.SetFillColor(NkColor2D::Yellow());
//   label.SetPosition({50, 50});
//
//   // In the render loop:
//   r2d->Clear(NkColor2D::Black());
//   r2d->Begin();
//     r2d->Draw(sprite);
//     r2d->Draw(label);
//     r2d->DrawFilledRect({100,100,200,50}, NkColor2D::Red());
//     r2d->DrawFilledCircle({400,300}, 80, NkColor2D::Blue());
//   r2d->End();
// =============================================================================

// Core types
#include "NKContext/Renderer/Core/NkRenderer2DTypes.h"
#include "NKContext/Renderer/Core/NkIRenderer2D.h"
#include "NKContext/Renderer/Core/NkRenderer2DFactory.h"

// Resources
#include "NKContext/Renderer/Resources/NkImage.h"
#include "NKContext/Renderer/Resources/NkTexture.h"
#include "NKContext/Renderer/Resources/NkFont.h"
#include "NKContext/Renderer/Resources/NkSprite.h"   // also contains NkText