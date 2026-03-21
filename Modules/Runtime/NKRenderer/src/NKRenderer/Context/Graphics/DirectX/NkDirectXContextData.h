#pragma once
// =============================================================================
// NkDirectXContextData.h — Données internes DX11 et DX12
// =============================================================================
#if defined(NKENTSEU_PLATFORM_WINDOWS)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

// --- DX11 ---
#include <d3d11_1.h>
#include <dxgi1_2.h>

// --- DX12 ---
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>

#include "NKCore/NkTypes.h"

namespace nkentseu {

    static constexpr uint32 kNkDX12MaxFrames = 3;

    // =========================================================================
    // DX11
    // =========================================================================
    struct NkDX11ContextData {
        ComPtr<ID3D11Device1>           device;
        ComPtr<ID3D11DeviceContext1>     context;
        ComPtr<IDXGISwapChain1>         swapchain;
        ComPtr<ID3D11RenderTargetView>  rtv;
        ComPtr<ID3D11DepthStencilView>  dsv;
        ComPtr<ID3D11Texture2D>         depthTex;
        HWND                            hwnd    = nullptr;
        uint32                          width   = 0;
        uint32                          height  = 0;
        const char*                     renderer= "DirectX 11";
        const char*                     vendor  = "";
        uint32                          vramMB  = 0;

        // Compute : même device, même context
        // Utiliser context->Dispatch() après avoir lié les ressources CS
    };

    // =========================================================================
    // DX12
    // =========================================================================
    struct NkDX12ContextData {
        ComPtr<IDXGIFactory6>           factory;
        ComPtr<IDXGIAdapter4>           adapter;
        ComPtr<ID3D12Device5>           device;

        // Queues
        ComPtr<ID3D12CommandQueue>      commandQueue;          // DIRECT (graphics+copy)
        ComPtr<ID3D12CommandQueue>      computeCommandQueue;   // COMPUTE dédiée

        // Swapchain
        ComPtr<IDXGISwapChain4>         swapchain;
        uint32                          backBufferCount = kNkDX12MaxFrames;
        uint32                          currentBackBuffer = 0;

        // RTV heap
        ComPtr<ID3D12DescriptorHeap>    rtvHeap;
        uint32                          rtvDescSize = 0;
        D3D12_CPU_DESCRIPTOR_HANDLE     rtvHandles[kNkDX12MaxFrames] = {};
        ComPtr<ID3D12Resource>          backBuffers[kNkDX12MaxFrames];

        // DSV heap
        ComPtr<ID3D12DescriptorHeap>    dsvHeap;
        ComPtr<ID3D12Resource>          depthBuffer;
        D3D12_CPU_DESCRIPTOR_HANDLE     dsvHandle = {};

        // SRV heap (CBV/SRV/UAV — pour les ressources shader)
        ComPtr<ID3D12DescriptorHeap>    srvHeap;
        uint32                          srvDescSize = 0;

        // Sampler heap
        ComPtr<ID3D12DescriptorHeap>    samplerHeap;

        // Command allocators & list
        ComPtr<ID3D12CommandAllocator>  cmdAllocators[kNkDX12MaxFrames];
        ComPtr<ID3D12GraphicsCommandList4> cmdList;

        // Compute command list
        ComPtr<ID3D12CommandAllocator>  computeAllocator;
        ComPtr<ID3D12GraphicsCommandList4> computeCmdList;

        // Synchronisation
        ComPtr<ID3D12Fence>             fence;
        uint64                          fenceValues[kNkDX12MaxFrames] = {};
        HANDLE                          fenceEvent = nullptr;

        // Infos
        HWND        hwnd      = nullptr;
        uint32      width     = 0;
        uint32      height    = 0;
        const char* renderer  = "DirectX 12";
        const char* vendor    = "";
        uint32      vramMB    = 0;
        bool        vsync     = true;
        bool        tearingSupported = false;

        uint32      lastWidth        = 0;
        uint32      lastHeight       = 0;
        bool        viewportDirty    = true;

        // Compteur de fence global strictement monotone
        // Toujours utiliser ++globalFenceValue avant chaque Signal()
        uint64      globalFenceValue = 0;
    };

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_WINDOWS
