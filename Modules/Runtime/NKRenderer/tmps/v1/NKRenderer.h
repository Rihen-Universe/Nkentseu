#pragma once
// =============================================================================
// NKRenderer.h
// En-tête public unique du module NKRenderer.
// L'application inclut uniquement ce fichier.
// =============================================================================

// Types fondamentaux et handles
#include "NKRenderer/Core/NkRendererTypes.h"

// Interface principale
#include "NKRenderer/Core/NkIRenderer.h"

// Fabrique
#include "NKRenderer/Core/NkRendererFactory.h"

// Accumulateur de draw calls
#include "NKRenderer/Command/NkRendererCommand.h"

// Helpers 2D/3D
#include "NKRenderer/2D/NkRenderer2D.h"

// Mesh types (Static, Dynamic, Editor, Model)
#include "NKRenderer/3D/NkMeshTypes.h"

// Système de matériaux
#include "NKRenderer/Material/NkMaterialSystem.h"

// Rendu de texte
#include "NKRenderer/Font/NkFontRenderer.h"

// Shaders built-in (accès optionnel)
#include "NKRenderer/Shaders/OpenGL/NkShaders_OpenGL.h"
#include "NKRenderer/Shaders/Vulkan/NkShaders_Vulkan.h"
#include "NKRenderer/Shaders/DX11/NkShaders_DX11.h"
#include "NKRenderer/Shaders/DX12/NkShaders_DX12.h"

// =============================================================================
// Usage typique (depuis l'application) :
//
//   #include "NKRenderer/NKRenderer.h"
//   using namespace nkentseu::renderer;
//
//   NkIRenderer* r = NkRendererFactory::Create(device);
//
//   // Ressources
//   NkShaderDesc sd;
//   sd.AddGL(NkShaderStageType::Vertex,   shaders::opengl::kPBR_Vert);
//   sd.AddGL(NkShaderStageType::Fragment, shaders::opengl::kPBR_Frag);
//   sd.AddVK(NkShaderStageType::Vertex,   shaders::vulkan::kPBR_Vert);
//   sd.AddVK(NkShaderStageType::Fragment, shaders::vulkan::kPBR_Frag);
//   sd.AddDX11(NkShaderStageType::Vertex,   shaders::dx11::kPBR_VS, "VSMain");
//   sd.AddDX11(NkShaderStageType::Fragment, shaders::dx11::kPBR_PS, "PSMain");
//   sd.AddDX12(NkShaderStageType::Vertex,   shaders::dx12::kPBR_VS, "VSMain");
//   sd.AddDX12(NkShaderStageType::Fragment, shaders::dx12::kPBR_PS, "PSMain");
//   NkShaderHandle hShader = r->CreateShader(sd);
//
//   NkTextureHandle hTex = r->CreateTexture(NkTextureDesc::Tex2D(512, 512));
//   NkMeshHandle    hMesh = r->CreateMesh(meshDesc);
//
//   NkMaterialHandle hMat = NkMaterialBuilder("PBR_Metal")
//       .WithShader(hShader).WithShading(NkShadingModel::PBR)
//       .Param("useAlbedoMap", true).Param("metallicFactor", 1.0f)
//       .Build(r);
//
//   NkMaterialInstHandle hInst = NkMaterialInstanceBuilder(hMat)
//       .Set("uAlbedoMap", hTex)
//       .Build(r);
//
//   NkCameraHandle hCam = r->CreateCamera3D({
//       .position={0,2,5}, .target={0,0,0}, .fovY=60.f, .aspect=16.f/9.f
//   });
//
//   // Boucle de frame :
//   r->BeginFrame();
//   r->BeginRenderPass({.camera=hCam});
//     NkRendererCommand& cmd = r->GetCommand();
//     cmd.Draw3D(hMesh, hInst, NkMat4f::Identity());
//     cmd.DrawText(hFont, "Hello NKRenderer!", {10,10}, 24.f);
//     cmd.AddDirectionalLight({0,-1,0}, {1,1,1}, 1.f, true);
//     cmd.DrawDebugGrid();
//   r->EndRenderPass();
//   r->EndFrame();
// =============================================================================