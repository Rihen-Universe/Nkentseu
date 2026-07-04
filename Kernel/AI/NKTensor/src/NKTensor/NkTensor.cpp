// =============================================================================
// NkTensor.cpp — implémentation CPU du tenseur (NKAI).
// =============================================================================
#include "NKTensor/NkTensor.h"
#include "NKTensor/NkTensorGpu.h"   // léger (pimpl) : libération des buffers GPU dans Release
#include "NKMemory/NkAllocator.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
    namespace ai {

        static memory::NkIAllocator& Alloc() {
            return memory::NkGetDefaultAllocator();
        }

        // =====================================================================
        // Stockage refcompté
        // =====================================================================
        NkTensorStorage* NkTensorStorage::Allocate(nk_size bytes) {
            NkTensorStorage* s = Alloc().New<NkTensorStorage>();
            s->data     = (bytes > 0) ? Alloc().Allocate(bytes, 64) : nullptr; // 64 = SIMD-friendly
            s->bytes    = bytes;
            s->refcount = 1;
            return s;
        }
        void NkTensorStorage::Retain(NkTensorStorage* s) {
            if (s) ++s->refcount;
        }
        void NkTensorStorage::Release(NkTensorStorage* s) {
            if (!s) return;
            if (--s->refcount == 0) {
                if (s->data) Alloc().Deallocate(s->data);
                if (s->gpuBuffer) NkTensorGpu::Get().DestroyBuffer(s->gpuBuffer);
                Alloc().Delete(s);
            }
        }

        // =====================================================================
        // Helpers de forme
        // =====================================================================
        int64 NkShapeNumel(const NkShape& shape) {
            int64 n = 1;
            for (uint32 i = 0; i < shape.Size(); i++) n *= shape[i];
            return shape.Size() == 0 ? 0 : n;
        }

        NkShape NkContiguousStrides(const NkShape& shape) {
            NkShape strides;
            const uint32 r = (uint32)shape.Size();
            strides.Resize(r);
            int64 acc = 1;
            for (int32 i = (int32)r - 1; i >= 0; --i) {
                strides[(uint32)i] = acc;
                acc *= shape[(uint32)i];
            }
            return strides;
        }

        // =====================================================================
        // Cycle de vie
        // =====================================================================
        NkTensor::~NkTensor() { ReleaseStorage(); }

        void NkTensor::ReleaseStorage() {
            NkTensorStorage::Release(mStorage);
            mStorage = nullptr;
        }

        void NkTensor::ShareFrom(const NkTensor& o) {
            mStorage = o.mStorage;
            NkTensorStorage::Retain(mStorage);
            mOffset  = o.mOffset;
            mShape   = o.mShape;
            mStrides = o.mStrides;
            mDType   = o.mDType;
            mDevice  = o.mDevice;
        }

        NkTensor::NkTensor(const NkTensor& other) { ShareFrom(other); }

        NkTensor& NkTensor::operator=(const NkTensor& other) {
            if (this != &other) {
                ReleaseStorage();
                ShareFrom(other);
            }
            return *this;
        }

        NkTensor::NkTensor(NkTensor&& o) noexcept
            : mStorage(o.mStorage), mOffset(o.mOffset), mShape(o.mShape),
              mStrides(o.mStrides), mDType(o.mDType), mDevice(o.mDevice) {
            o.mStorage = nullptr;
        }

        NkTensor& NkTensor::operator=(NkTensor&& o) noexcept {
            if (this != &o) {
                ReleaseStorage();
                mStorage = o.mStorage; mOffset = o.mOffset; mShape = o.mShape;
                mStrides = o.mStrides; mDType = o.mDType; mDevice = o.mDevice;
                o.mStorage = nullptr;
            }
            return *this;
        }

        // =====================================================================
        // Fabriques
        // =====================================================================
        NkTensor NkTensor::Empty(const NkShape& shape, NkDType dtype, NkDevice device) {
            NkTensor t;
            t.mShape   = shape;
            t.mStrides = NkContiguousStrides(shape);
            t.mDType   = dtype;
            t.mDevice  = device;
            t.mOffset  = 0;
            const int64 n = NkShapeNumel(shape);
            const nk_size bytes = (nk_size)(n < 0 ? 0 : n) * NkDTypeSize(dtype);
            t.mStorage = NkTensorStorage::Allocate(bytes);
            return t;
        }

        NkTensor NkTensor::Zeros(const NkShape& shape, NkDType dtype, NkDevice device) {
            NkTensor t = Empty(shape, dtype, device);
            if (t.mStorage && t.mStorage->data)
                memset(t.mStorage->data, 0, t.mStorage->bytes);
            return t;
        }

        NkTensor NkTensor::Full(const NkShape& shape, double value, NkDType dtype, NkDevice device) {
            NkTensor t = Empty(shape, dtype, device);
            t.Fill(value);
            return t;
        }

        NkTensor NkTensor::Ones(const NkShape& shape, NkDType dtype, NkDevice device) {
            return Full(shape, 1.0, dtype, device);
        }

        NkTensor NkTensor::FromData(const NkShape& shape, const void* data, NkDType dtype, NkDevice device) {
            NkTensor t = Empty(shape, dtype, device);
            if (t.mStorage && t.mStorage->data && data)
                memcpy(t.mStorage->data, data, t.mStorage->bytes);
            return t;
        }

        NkTensor NkTensor::Arange(double start, double stop, double step, NkDType dtype, NkDevice device) {
            int64 n = 0;
            if (step != 0.0) {
                const double span = (stop - start) / step;
                n = (int64)(span > 0.0 ? span : 0.0);
                // Inclut le dernier pas si la division tombe pile en dessous (arrondi).
                if (start + (double)n * step < stop) ++n;
            }
            NkTensor t = Empty(NkShape{ n }, dtype, device);
            for (int64 i = 0; i < n; i++) t.SetItem(NkShape{ i }, start + (double)i * step);
            return t;
        }

        NkTensor NkTensor::Eye(int64 n, NkDType dtype, NkDevice device) {
            NkTensor t = Zeros(NkShape{ n, n }, dtype, device);
            for (int64 i = 0; i < n; i++) t.SetItem(NkShape{ i, i }, 1.0);
            return t;
        }

        // =====================================================================
        // Infos
        // =====================================================================
        int64 NkTensor::Numel() const { return NkShapeNumel(mShape); }

        bool NkTensor::IsContiguous() const {
            NkShape c = NkContiguousStrides(mShape);
            if (c.Size() != mStrides.Size()) return false;
            for (uint32 i = 0; i < c.Size(); i++)
                if (c[i] != mStrides[i]) return false;
            return true;
        }

        char* NkTensor::BytePtr() const {
            if (!mStorage || !mStorage->data) return nullptr;
            return (char*)mStorage->data + (nk_size)mOffset * NkDTypeSize(mDType);
        }
        void*       NkTensor::RawData()       { return BytePtr(); }
        const void* NkTensor::RawData() const { return BytePtr(); }

        // =====================================================================
        // Itération générique (odomètre sur la forme logique)
        // =====================================================================
        // Retourne l'offset (en éléments) d'un multi-index via les strides.
        static int64 FlatOffset(const NkShape& index, const NkShape& strides, int64 base) {
            int64 off = base;
            for (uint32 i = 0; i < index.Size(); i++) off += index[i] * strides[i];
            return off;
        }

        // Incrémente un compteur multi-dim ; retourne false quand on a bouclé.
        static bool NextIndex(NkShape& idx, const NkShape& shape) {
            for (int32 d = (int32)shape.Size() - 1; d >= 0; --d) {
                if (++idx[(uint32)d] < shape[(uint32)d]) return true;
                idx[(uint32)d] = 0;
            }
            return false;
        }

        // =====================================================================
        // Accès scalaire
        // =====================================================================
        double NkTensor::GetItem(const NkShape& index) const {
            const int64 off = FlatOffset(index, mStrides, mOffset);
            const char* p = (const char*)mStorage->data + (nk_size)off * NkDTypeSize(mDType);
            double out = 0.0;
            NK_DTYPE_DISPATCH(mDType, T, { out = (double)(*reinterpret_cast<const T*>(p)); });
            return out;
        }

        void NkTensor::SetItem(const NkShape& index, double v) {
            const int64 off = FlatOffset(index, mStrides, mOffset);
            char* p = (char*)mStorage->data + (nk_size)off * NkDTypeSize(mDType);
            NK_DTYPE_DISPATCH(mDType, T, { *reinterpret_cast<T*>(p) = (T)v; });
        }

        void NkTensor::Fill(double value) {
            if (!mStorage || !mStorage->data) return;
            const int64 n = Numel();
            if (n <= 0) return;
            // Chemin rapide : contigu -> écriture linéaire.
            if (IsContiguous()) {
                char* base = BytePtr();
                NK_DTYPE_DISPATCH(mDType, T, {
                    T* d = reinterpret_cast<T*>(base);
                    const T val = (T)value;
                    for (int64 i = 0; i < n; i++) d[i] = val;
                });
                return;
            }
            // Chemin générique (vue à strides).
            NkShape idx; idx.Resize(mShape.Size());
            for (uint32 i = 0; i < idx.Size(); i++) idx[i] = 0;
            do { SetItem(idx, value); } while (NextIndex(idx, mShape));
        }

        // =====================================================================
        // Vues / remodelage
        // =====================================================================
        NkTensor NkTensor::Reshape(const NkShape& newShape) const {
            if (!IsContiguous()) {
                // Non contigu : matérialiser puis remodeler.
                return Contiguous().Reshape(newShape);
            }
            // Résout une éventuelle dimension -1 (inférée) + valide le nombre d'éléments.
            NkShape resolved = newShape;
            int32 inferIdx = -1;
            int64 known = 1;
            for (uint32 i = 0; i < resolved.Size(); i++) {
                if (resolved[i] == -1) {
                    if (inferIdx != -1) {
                        fprintf(stderr, "[NkTensor] Reshape : une seule dimension -1 autorisée\n");
                        return NkTensor();
                    }
                    inferIdx = (int32)i;
                } else {
                    known *= resolved[i];
                }
            }
            const int64 total = Numel();
            if (inferIdx != -1) {
                if (known <= 0 || total % known != 0) {
                    fprintf(stderr, "[NkTensor] Reshape : dimension -1 non résoluble\n");
                    return NkTensor();
                }
                resolved[(uint32)inferIdx] = total / known;
            } else if (known != total) {
                fprintf(stderr, "[NkTensor] Reshape : %lld éléments incompatibles avec la forme demandée (%lld)\n",
                        (long long)total, (long long)known);
                return NkTensor();
            }
            NkTensor v;
            v.mStorage = mStorage; NkTensorStorage::Retain(v.mStorage);
            v.mOffset  = mOffset;
            v.mShape   = resolved;
            v.mStrides = NkContiguousStrides(resolved);
            v.mDType   = mDType;
            v.mDevice  = mDevice;
            return v;
        }

        NkTensor NkTensor::Permute(const NkShape& order) const {
            const uint32 r = Rank();
            if (order.Size() != r) {
                fprintf(stderr, "[NkTensor] Permute : ordre de taille %u attendu, reçu %u\n",
                        r, (uint32)order.Size());
                return NkTensor();
            }
            NkTensor v;
            v.mStorage = mStorage; NkTensorStorage::Retain(v.mStorage);
            v.mOffset  = mOffset;
            v.mDType   = mDType;
            v.mDevice  = mDevice;
            v.mShape.Resize(r);
            v.mStrides.Resize(r);
            for (uint32 i = 0; i < r; i++) {
                const int64 src = order[i];
                if (src < 0 || src >= (int64)r) {
                    fprintf(stderr, "[NkTensor] Permute : axe %lld hors bornes\n", (long long)src);
                    return NkTensor();
                }
                v.mShape[i]   = mShape[(uint32)src];
                v.mStrides[i] = mStrides[(uint32)src];
            }
            return v;
        }

        NkTensor NkTensor::Slice(uint32 dim, int64 start, int64 stop, int64 step) const {
            if (dim >= Rank()) {
                fprintf(stderr, "[NkTensor] Slice : dim %u hors rang %u\n", dim, Rank());
                return NkTensor();
            }
            if (step < 1) step = 1;
            const int64 size = mShape[dim];
            if (start < 0) start = 0;
            if (stop > size) stop = size;
            if (stop < start) stop = start;
            const int64 count = (stop - start + step - 1) / step; // ceil

            NkTensor v;
            v.mStorage = mStorage; NkTensorStorage::Retain(v.mStorage);
            v.mDType   = mDType;
            v.mDevice  = mDevice;
            v.mShape   = mShape;
            v.mStrides = mStrides;
            v.mOffset  = mOffset + start * mStrides[dim];
            v.mShape[dim]   = count;
            v.mStrides[dim] = mStrides[dim] * step;
            return v;
        }

        NkTensor NkTensor::Transpose(uint32 dim0, uint32 dim1) const {
            NkTensor v;
            v.mStorage = mStorage; NkTensorStorage::Retain(v.mStorage);
            v.mOffset  = mOffset;
            v.mShape   = mShape;
            v.mStrides = mStrides;
            v.mDType   = mDType;
            v.mDevice  = mDevice;
            if (dim0 < v.mShape.Size() && dim1 < v.mShape.Size()) {
                int64 s = v.mShape[dim0]; v.mShape[dim0] = v.mShape[dim1]; v.mShape[dim1] = s;
                int64 t = v.mStrides[dim0]; v.mStrides[dim0] = v.mStrides[dim1]; v.mStrides[dim1] = t;
            }
            return v;
        }

        NkTensor NkTensor::Contiguous() const {
            if (IsContiguous()) return *this; // vue partagée déjà contiguë
            NkTensor out = Empty(mShape, mDType, mDevice);
            const int64 n = Numel();
            if (n <= 0) return out;
            NkShape idx; idx.Resize(mShape.Size());
            for (uint32 i = 0; i < idx.Size(); i++) idx[i] = 0;
            const nk_size isz = NkDTypeSize(mDType);
            char* dst = out.BytePtr();
            int64 k = 0;
            do {
                const int64 off = FlatOffset(idx, mStrides, mOffset);
                const char* src = (const char*)mStorage->data + (nk_size)off * isz;
                memcpy(dst + (nk_size)k * isz, src, isz);
                ++k;
            } while (NextIndex(idx, mShape));
            return out;
        }

        NkTensor NkTensor::Clone() const {
            NkTensor c = Contiguous();
            if (c.mStorage == mStorage) {
                // Contiguous a renvoyé une vue partagée : forcer une vraie copie.
                NkTensor out = Empty(mShape, mDType, mDevice);
                if (out.mStorage->data && mStorage->data)
                    memcpy(out.mStorage->data, BytePtr(), out.mStorage->bytes);
                return out;
            }
            return c;
        }

        // =====================================================================
        // Debug
        // =====================================================================
        void NkTensor::Print(const char* name) const {
            printf("NkTensor%s%s [", name ? " " : "", name ? name : "");
            for (uint32 i = 0; i < mShape.Size(); i++)
                printf("%lld%s", (long long)mShape[i], i + 1 < mShape.Size() ? "x" : "");
            printf("] %s (%s, %s)\n", NkDTypeName(mDType),
                   mDevice == NkDevice::NK_CPU ? "cpu" : "gpu",
                   IsContiguous() ? "contig" : "strided");
        }

    } // namespace ai
} // namespace nkentseu
