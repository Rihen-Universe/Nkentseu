// =============================================================================
// NkTensorGpu.h — contexte GPU de NKTensor (Jalon 3).
//
// Encapsule le chemin compute PROUVÉ : kernel écrit en NkSL -> GLSL-Vulkan ->
// (glslang -> SPIR-V -> SPIRV-Cross) -> HLSL/SPIRV/MSL -> pipeline compute NKRHI.
// Le tenseur ne connaît pas NKRHI : il ne manipule que des handles opaques (uint64)
// de buffers GPU gérés ici. Device headless (compute-only, sans fenêtre).
//
// Tout est derrière un pimpl : NkTensor.h/.cpp restent CPU-only et sans dépendance
// NKRHI ; seul NkTensorGpu.cpp tire NKRHI + NKSL.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
    namespace ai {

        class NkTensor;   // forward (défini dans NkTensor.h)

        class NkTensorGpu {
        public:
            // Singleton paresseux. Le device GPU n'est créé qu'au premier usage.
            static NkTensorGpu& Get();

            // Un device compute headless est-il disponible sur cette machine ?
            bool        IsAvailable();
            const char* BackendName();   // "DirectX 11" / "Vulkan" / … / "none"

            // ---- Buffers GPU (stockage des tenseurs) ---------------------------
            // Retourne un id opaque (>0) ou 0 en échec. Le buffer est un storage
            // buffer (SSBO / UAV) utilisable en compute et relisible par le CPU.
            uint64 CreateBuffer(nk_size bytes);
            void   DestroyBuffer(uint64 id);
            bool   Upload  (uint64 id, const void* data, nk_size bytes);
            bool   Download(uint64 id, void* out,        nk_size bytes);

            // ---- Kernels ------------------------------------------------------
            // Élémentaire binaire : C = f(A, B) sur `count` éléments f32.
            // Le kernel NkSL doit déclarer buffers 0,1,2 (A,B,C) + UBO binding 3
            // { uint count }, workgroup local_size_x=64. Compilé et mis en cache par nom.
            bool RunBinary(const char* name, const NkString& nkslSrc,
                           uint64 a, uint64 b, uint64 c, uint32 count);

            // Élémentaire unaire : B = f(A) sur `count` éléments f32.
            // Kernel : buffers 0,1 (A,B) + UBO binding 2 { uint count }.
            bool RunUnary(const char* name, const NkString& nkslSrc,
                          uint64 a, uint64 b, uint32 count);

            // MatMul C[M,N] = A[M,K] · B[K,N] (row-major, f32).
            // Kernel : buffers 0,1,2 (A,B,C) + UBO binding 3 { uint M,N,K },
            // workgroup 16x16.
            bool RunMatMul(uint64 a, uint64 b, uint64 c, uint32 M, uint32 N, uint32 K);

            void Shutdown();  // libère device + pipelines (appelé à l'arrêt)

        private:
            NkTensorGpu() = default;
            ~NkTensorGpu();
            NkTensorGpu(const NkTensorGpu&) = delete;
            NkTensorGpu& operator=(const NkTensorGpu&) = delete;

            struct Impl;
            Impl* mImpl = nullptr;
            bool  EnsureInit();
        };

        // Ops GPU (appelées par ops::Add / ops::Matmul quand un opérande est sur GPU).
        // Déplacent au besoin les opérandes sur GPU ; renvoient un tenseur device=GPU.
        // Renvoient un tenseur invalide si le GPU est indisponible.
        NkTensor NkGpuAdd   (const NkTensor& a, const NkTensor& b);   // élémentaire (mêmes formes)
        NkTensor NkGpuMatmul(const NkTensor& a, const NkTensor& b);   // a[M,K] · b[K,N] -> [M,N]

    } // namespace ai
} // namespace nkentseu
