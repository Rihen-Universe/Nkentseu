// =============================================================================
// NkTensor.h — tenseur n-dimensionnel (NKAI), backend CPU (Jalon 1/2).
//
// Brique de base de tout NKAI. Un tenseur = { stockage refcompté partagé,
// forme (shape), pas (strides), type (dtype), device }. Les vues (reshape,
// transpose, slice) partagent le stockage sans copie (refcount). Le GPU (NKRHI
// compute) arrivera au Jalon 3 derrière la MÊME API — cf ROADMAP.
//
// Convention : row-major (C-order). Indexation générique via strides.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKTensor/NkDType.h"

namespace nkentseu {
    namespace ai {

        // Device d'un tenseur. CPU seul pour l'instant (GPU = Jalon 3).
        enum class NkDevice : uint8 { NK_CPU = 0, NK_GPU = 1 };

        // Forme / pas : petits tableaux d'entiers 64 bits (dimensions).
        using NkShape = NkVector<int64>;

        // -------------------------------------------------------------------------
        // Stockage refcompté (partagé entre un tenseur et ses vues).
        // Alloué via NKMemory ; libéré quand le dernier référent disparaît.
        // -------------------------------------------------------------------------
        struct NkTensorStorage {
            void*     data      = nullptr;
            nk_size   bytes     = 0;
            int32     refcount  = 1;
            uint64    gpuBuffer = 0;   // handle NkTensorGpu (0 = stockage CPU)

            static NkTensorStorage* Allocate(nk_size bytes);
            static void             Retain(NkTensorStorage* s);
            static void             Release(NkTensorStorage* s); // free (CPU ou GPU) si refcount -> 0
        };

        // -------------------------------------------------------------------------
        // NkTensor
        // -------------------------------------------------------------------------
        class NkTensor {
        public:
            NkTensor() = default;
            ~NkTensor();

            // Copie = vue partagée (même stockage, incref) — pas cher. Pour une
            // copie profonde indépendante, utiliser Clone().
            NkTensor(const NkTensor& other);
            NkTensor& operator=(const NkTensor& other);
            NkTensor(NkTensor&& other) noexcept;
            NkTensor& operator=(NkTensor&& other) noexcept;

            // ---- Fabriques -----------------------------------------------------
            static NkTensor Empty(const NkShape& shape, NkDType dtype = NkDType::NK_F32,
                                  NkDevice device = NkDevice::NK_CPU);
            static NkTensor Zeros(const NkShape& shape, NkDType dtype = NkDType::NK_F32,
                                  NkDevice device = NkDevice::NK_CPU);
            static NkTensor Ones (const NkShape& shape, NkDType dtype = NkDType::NK_F32,
                                  NkDevice device = NkDevice::NK_CPU);
            static NkTensor Full (const NkShape& shape, double value,
                                  NkDType dtype = NkDType::NK_F32, NkDevice device = NkDevice::NK_CPU);
            // Copie depuis un buffer contigu (row-major) fourni par l'appelant.
            static NkTensor FromData(const NkShape& shape, const void* data, NkDType dtype,
                                     NkDevice device = NkDevice::NK_CPU);
            // Séquence [start, stop) par pas `step` (1D). Pratique pour tests/démos.
            static NkTensor Arange(double start, double stop, double step = 1.0,
                                   NkDType dtype = NkDType::NK_F32, NkDevice device = NkDevice::NK_CPU);
            // Matrice identité n x n.
            static NkTensor Eye(int64 n, NkDType dtype = NkDType::NK_F32,
                                NkDevice device = NkDevice::NK_CPU);

            // ---- Infos ---------------------------------------------------------
            bool             IsValid()  const { return mStorage != nullptr; }
            uint32           Rank()     const { return (uint32)mShape.Size(); }
            int64            Numel()    const;
            NkDType          DType()    const { return mDType; }
            NkDevice         Device()   const { return mDevice; }
            const NkShape&   Shape()    const { return mShape; }
            const NkShape&   Strides()  const { return mStrides; }
            nk_size          ItemSize() const { return NkDTypeSize(mDType); }
            bool             IsContiguous() const;

            void*            RawData();           // pointeur début (offset appliqué)
            const void*      RawData() const;
            template<typename T> T*       DataAs()       { return reinterpret_cast<T*>(RawData()); }
            template<typename T> const T* DataAs() const { return reinterpret_cast<const T*>(RawData()); }

            // ---- Vues / remodelage --------------------------------------------
            // Vue partagée avec une nouvelle forme. Une dimension peut valoir -1
            // (inférée depuis le nombre total d'éléments). Exige un tenseur contigu
            // (sinon matérialise via Contiguous). Renvoie un tenseur invalide si le
            // nombre d'éléments ne correspond pas.
            NkTensor Reshape(const NkShape& newShape) const;
            // Vue partagée transposant deux dimensions (permute shape + strides).
            NkTensor Transpose(uint32 dim0, uint32 dim1) const;
            // Vue partagée réordonnant toutes les dimensions selon `order`
            // (permutation de [0..rank)).
            NkTensor Permute(const NkShape& order) const;
            // Vue partagée : tranche [start, stop) par pas `step` le long de `dim`.
            // start/stop sont bornés à [0, taille] ; step >= 1.
            NkTensor Slice(uint32 dim, int64 start, int64 stop, int64 step = 1) const;
            // Copie contiguë row-major (matérialise une vue à strides quelconques).
            NkTensor Contiguous() const;
            // Copie profonde indépendante (stockage neuf).
            NkTensor Clone() const;

            // ---- Transferts CPU <-> GPU (backend NkTensorGpu / NKRHI compute) ---
            // ToGPU : matérialise contigu, alloue un buffer GPU, uploade -> tenseur
            // device=GPU. ToCPU : télécharge -> tenseur CPU. Renvoie *this si déjà
            // sur le bon device. Renvoie un tenseur invalide si le GPU est indispo.
            NkTensor ToGPU() const;
            NkTensor ToCPU() const;

            // ---- Accès scalaire (debug / petits cas) --------------------------
            double GetItem(const NkShape& index) const;      // lit -> double
            void   SetItem(const NkShape& index, double v);  // écrit depuis double
            void   Fill(double value);

            // Affichage compact (debug).
            void   Print(const char* name = nullptr) const;

        private:
            NkTensorStorage* mStorage = nullptr; // stockage partagé (refcompté)
            int64            mOffset  = 0;        // offset en ÉLÉMENTS depuis data
            NkShape          mShape;
            NkShape          mStrides;            // en ÉLÉMENTS (pas en octets)
            NkDType          mDType   = NkDType::NK_F32;
            NkDevice         mDevice  = NkDevice::NK_CPU;

            void  ReleaseStorage();
            void  ShareFrom(const NkTensor& o);  // copie métadonnées + incref
            char* BytePtr() const;               // data + offset*itemsize

            friend struct NkTensorInternal;
        };

        // Calcule les strides contigus (row-major) d'une forme.
        NkShape NkContiguousStrides(const NkShape& shape);
        // Produit des dimensions (nombre d'éléments).
        int64   NkShapeNumel(const NkShape& shape);

    } // namespace ai
} // namespace nkentseu
