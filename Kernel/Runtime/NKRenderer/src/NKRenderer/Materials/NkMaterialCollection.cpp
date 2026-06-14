// =============================================================================
// NkMaterialCollection.cpp  — NKRenderer Phase M.2
// =============================================================================
#include "NkMaterialCollection.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
    namespace renderer {

        bool NkMaterialCollection::Init(NkIDevice* device) {
            if (!device) return false;
            mDevice = device;
            mUBO = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(mData)));
            mDirty = true;

            // Pre-reserve les slots conventionnels (stables = utilisables par
            // index dans les shaders). L'ordre de SetVec4/SetFloat ci-dessous
            // determine l'index : globalTint=0, gameTime=1, ...
            SetVec4("globalTint", {1.f, 1.f, 1.f, 1.f});  // slot 0 = no-op multiplicative
            SetFloat("gameTime",  0.f);                    // slot 1 = animation generique
            SetVec3("windDirection", {0.f, 0.f, 0.f});     // slot 2 = vent (foliage)
            SetFloat("windStrength", 0.f);                 // slot 3 = vent strength

            logger.Info("[NkMaterialCollection] Initialized ({0} slots max, {1} bytes UBO, "
                        "{2} preset slots)\n",
                        kMaxParams, (uint32)sizeof(mData), mUsedSlots);
            return mUBO.IsValid();
        }

        void NkMaterialCollection::Shutdown() {
            if (mDevice && mUBO.IsValid()) {
                mDevice->DestroyBuffer(mUBO);
                mUBO = {};
            }
            mNameToSlot.Clear();
            mUsedSlots = 0;
            mDevice = nullptr;
        }

        int32 NkMaterialCollection::ReserveSlot(const NkString& name) {
            auto* existing = mNameToSlot.Find(name);
            if (existing) return *existing;
            if (mUsedSlots >= kMaxParams) {
                logger.Warnf("[NkMaterialCollection] full (max %u slots), drop '%s'\n",
                             kMaxParams, name.CStr());
                return -1;
            }
            const int32 slot = (int32)mUsedSlots++;
            mNameToSlot.Insert(name, slot);
            return slot;
        }

        int32 NkMaterialCollection::GetSlot(const NkString& name) const {
            auto* p = mNameToSlot.Find(name);
            return p ? *p : -1;
        }

        NkVec4f NkMaterialCollection::Get(const NkString& name) const {
            const int32 slot = GetSlot(name);
            if (slot < 0) return {0.f, 0.f, 0.f, 0.f};
            return mData[slot];
        }

        void NkMaterialCollection::SetFloat(const NkString& name, float32 v) {
            const int32 slot = ReserveSlot(name);
            if (slot < 0) return;
            mData[slot].x = v;
            mDirty = true;
        }

        void NkMaterialCollection::SetVec2(const NkString& name, NkVec2f v) {
            const int32 slot = ReserveSlot(name);
            if (slot < 0) return;
            mData[slot].x = v.x; mData[slot].y = v.y;
            mDirty = true;
        }

        void NkMaterialCollection::SetVec3(const NkString& name, NkVec3f v) {
            const int32 slot = ReserveSlot(name);
            if (slot < 0) return;
            mData[slot].x = v.x; mData[slot].y = v.y; mData[slot].z = v.z;
            mDirty = true;
        }

        void NkMaterialCollection::SetVec4(const NkString& name, NkVec4f v) {
            const int32 slot = ReserveSlot(name);
            if (slot < 0) return;
            mData[slot] = v;
            mDirty = true;
        }

        void NkMaterialCollection::SetInt(const NkString& name, int32 v) {
            const int32 slot = ReserveSlot(name);
            if (slot < 0) return;
            // std140 : int loge dans le canal x (4 bytes) avec padding implicite.
            // On reinterpret (bit cast) pour preserver le pattern dans le shader.
            union { int32 i; float32 f; } u;
            u.i = v;
            mData[slot].x = u.f;
            mDirty = true;
        }

        void NkMaterialCollection::Upload() {
            if (!mDirty || !mDevice || !mUBO.IsValid()) return;
            mDevice->WriteBuffer(mUBO, mData, sizeof(mData));
            mDirty = false;
        }

    } // namespace renderer
} // namespace nkentseu
