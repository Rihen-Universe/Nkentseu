#include "NkMesh.h"

#include "NKRenderer/Loader/NkModelLoader.h"
#include "NKLogger/NkLog.h"
#include <cmath>
#include <cstdlib>
#include <unordered_map>
#include <unordered_set>

namespace nkentseu {
    namespace renderer {

        namespace {

            static float Vec3Length(const NkVec3f& v) {
                return NkSqrt(v.x * v.x + v.y * v.y + v.z * v.z);
            }

            static NkVec3f Vec3Normalize(const NkVec3f& v, const NkVec3f& fallback = {0.f, 1.f, 0.f}) {
                const float len = Vec3Length(v);
                if (len <= 1e-6f) return fallback;
                const float inv = 1.0f / len;
                return {v.x * inv, v.y * inv, v.z * inv};
            }

            static NkVec3f Vec3Cross(const NkVec3f& a, const NkVec3f& b) {
                return {
                    a.y * b.z - a.z * b.y,
                    a.z * b.x - a.x * b.z,
                    a.x * b.y - a.y * b.x
                };
            }

            static float Vec3Distance(const NkVec3f& a, const NkVec3f& b) {
                const float dx = a.x - b.x;
                const float dy = a.y - b.y;
                const float dz = a.z - b.z;
                return NkSqrt(dx * dx + dy * dy + dz * dz);
            }

            static float Clamp01(float v) {
                return NkClamp(v, 0.0f, 1.0f);
            }

            static float ClampSignedUnit(float v) {
                return NkClamp(v, -1.0f, 1.0f);
            }

            static bool FileExists(const char* path) {
                if (!path || !path[0]) return false;
                FILE* f = fopen(path, "rb");
                if (!f) return false;
                fclose(f);
                return true;
            }

            static uint32 FindEditVertexIndexById(const NkVector<NkEditVertex>& vertices, uint32 id) {
                for (uint32 i = 0; i < (uint32)vertices.Size(); ++i) {
                    if (vertices[i].id == id) return i;
                }
                return UINT32_MAX;
            }

            static uint32 FindEditEdgeIndexById(const NkVector<NkEditEdge>& edges, uint32 id) {
                for (uint32 i = 0; i < (uint32)edges.Size(); ++i) {
                    if (edges[i].id == id) return i;
                }
                return UINT32_MAX;
            }

            static uint32 FindEditFaceIndexById(const NkVector<NkEditFace>& faces, uint32 id) {
                for (uint32 i = 0; i < (uint32)faces.Size(); ++i) {
                    if (faces[i].id == id) return i;
                }
                return UINT32_MAX;
            }

            static NkVec3f MulPoint(const NkMat4f& m, const NkVec3f& p) {
                return {
                    m.data[0] * p.x + m.data[4] * p.y + m.data[8] * p.z + m.data[12],
                    m.data[1] * p.x + m.data[5] * p.y + m.data[9] * p.z + m.data[13],
                    m.data[2] * p.x + m.data[6] * p.y + m.data[10] * p.z + m.data[14]
                };
            }

            struct NkStaticMeshCPUCache {
                NkVector<NkVertex3D> vertices;
                NkVector<uint32> indices;
            };

            static std::unordered_map<const NkStaticMesh*, NkStaticMeshCPUCache> gStaticMeshCPUCache;
            static std::unordered_map<const NkSkinnedMesh*, NkIDevice*> gSkinnedMeshDevices;

        } // namespace

        uint64 NkStaticMesh::sIDCounter = 1;

        // =============================================================================
        // NkMeshData helpers not implemented in NkModelLoader.cpp
        // =============================================================================

        void NkMeshData::GenerateSphericalUV() {
            if (vertices.Empty()) return;

            RecalculateAABB();
            const NkVec3f c = aabb.Center();

            for (auto& v : vertices) {
                NkVec3f p = {v.position.x - c.x, v.position.y - c.y, v.position.z - c.z};
                p = Vec3Normalize(p, {0.0f, 1.0f, 0.0f});

                const float u = 0.5f + (float)std::atan2(p.z, p.x) / (2.0f * (float)NkPi);
                const float vv = 0.5f - (float)std::asin(ClampSignedUnit(p.y)) / (float)NkPi;

                v.uv0 = {u, vv};
            }
        }

        void NkMeshData::GeneratePlanarUV(int axis) {
            if (vertices.Empty()) return;

            RecalculateAABB();
            const NkVec3f minV = aabb.min;
            const NkVec3f maxV = aabb.max;
            const float sx = NkMax(1e-6f, maxV.x - minV.x);
            const float sy = NkMax(1e-6f, maxV.y - minV.y);
            const float sz = NkMax(1e-6f, maxV.z - minV.z);

            for (auto& v : vertices) {
                if (axis == 0) { // project on YZ
                    v.uv0.x = (v.position.y - minV.y) / sy;
                    v.uv0.y = (v.position.z - minV.z) / sz;
                } else if (axis == 1) { // project on XZ
                    v.uv0.x = (v.position.x - minV.x) / sx;
                    v.uv0.y = (v.position.z - minV.z) / sz;
                } else { // project on XY
                    v.uv0.x = (v.position.x - minV.x) / sx;
                    v.uv0.y = (v.position.y - minV.y) / sy;
                }
            }
        }

        // =============================================================================
        // NkStaticMesh
        // =============================================================================

        bool NkStaticMesh::Create(NkIDevice* device, const NkMeshData& data) {
            Destroy();

            if (!device || !device->IsValid() || data.vertices.Empty()) {
                return false;
            }

            mDevice = device;
            mID.id = sIDCounter++;
            mVertexCount = (uint32)data.vertices.Size();
            mIndexCount = (uint32)data.indices.Size();
            mAABB = data.aabb;
            mTopology = data.topology;
            mSubMeshes = data.subMeshes;
            mMaterials = data.materials;

            if (mSubMeshes.Empty()) {
                NkSubMesh sm;
                sm.firstVertex = 0;
                sm.vertexCount = mVertexCount;
                sm.firstIndex = 0;
                sm.indexCount = mIndexCount;
                sm.materialIdx = 0;
                sm.name = "Default";
                sm.aabbMin = mAABB.min;
                sm.aabbMax = mAABB.max;
                mSubMeshes.PushBack(sm);
            }

            const uint64 vboSize = (uint64)(sizeof(NkVertex3D) * data.vertices.Size());
            mVBO = device->CreateBuffer(NkBufferDesc::Vertex(vboSize, data.vertices.Data()));
            if (!mVBO.IsValid()) {
                Destroy();
                return false;
            }

            if (!data.indices.Empty()) {
                const uint64 iboSize = (uint64)(sizeof(uint32) * data.indices.Size());
                mIBO = device->CreateBuffer(NkBufferDesc::Index(iboSize, data.indices.Data()));
                if (!mIBO.IsValid()) {
                    Destroy();
                    return false;
                }
            }

            NkStaticMeshCPUCache& cpuCache = gStaticMeshCPUCache[this];
            cpuCache.vertices = data.vertices;
            cpuCache.indices = data.indices;

            return true;
        }

        void NkStaticMesh::Destroy() {
            gStaticMeshCPUCache.erase(this);

            if (!mDevice) {
                mSubMeshes.Clear();
                mMaterials.Clear();
                mLODs.Clear();
                mVertexCount = 0;
                mIndexCount = 0;
                return;
            }

            if (mPointVBO.IsValid()) mDevice->DestroyBuffer(mPointVBO);
            if (mWireframeIBO.IsValid()) mDevice->DestroyBuffer(mWireframeIBO);
            if (mIBO.IsValid()) mDevice->DestroyBuffer(mIBO);
            if (mVBO.IsValid()) mDevice->DestroyBuffer(mVBO);

            for (auto& lod : mLODs) {
                if (lod.ibo.IsValid()) mDevice->DestroyBuffer(lod.ibo);
                if (lod.vbo.IsValid()) mDevice->DestroyBuffer(lod.vbo);
                lod.ibo = {};
                lod.vbo = {};
            }

            mPointVBO = {};
            mWireframeIBO = {};
            mIBO = {};
            mVBO = {};
            mSubMeshes.Clear();
            mMaterials.Clear();
            mLODs.Clear();
            mVertexCount = 0;
            mIndexCount = 0;
            mDevice = nullptr;
        }

        NkMaterial* NkStaticMesh::GetMaterial(uint32 idx) const {
            if (idx < (uint32)mMaterials.Size()) {
                return mMaterials[idx];
            }
            return nullptr;
        }

        void NkStaticMesh::SetMaterial(uint32 idx, NkMaterial* mat) {
            if (idx >= (uint32)mMaterials.Size()) {
                mMaterials.Resize((usize)idx + 1);
            }
            mMaterials[idx] = mat;
        }

        void NkStaticMesh::AddLOD(NkIDevice* device, const NkMeshData& data, float screenSizeThreshold) {
            if (!device || !device->IsValid() || data.vertices.Empty()) {
                return;
            }

            NkLODLevel lod;
            lod.screenSizeThreshold = Clamp01(screenSizeThreshold);
            lod.subMeshes = data.subMeshes;
            lod.vertexCount = (uint32)data.vertices.Size();
            lod.indexCount = (uint32)data.indices.Size();

            const uint64 vboSize = (uint64)(sizeof(NkVertex3D) * data.vertices.Size());
            lod.vbo = device->CreateBuffer(NkBufferDesc::Vertex(vboSize, data.vertices.Data()));
            if (!lod.vbo.IsValid()) return;

            if (!data.indices.Empty()) {
                const uint64 iboSize = (uint64)(sizeof(uint32) * data.indices.Size());
                lod.ibo = device->CreateBuffer(NkBufferDesc::Index(iboSize, data.indices.Data()));
                if (!lod.ibo.IsValid()) {
                    device->DestroyBuffer(lod.vbo);
                    return;
                }
            }

            mLODs.PushBack(lod);
        }

        NkLODLevel* NkStaticMesh::GetLOD(float screenSize) {
            if (mLODs.Empty()) return nullptr;

            const float clamped = Clamp01(screenSize);
            NkLODLevel* best = &mLODs[0];
            float bestThreshold = -1.0f;

            for (auto& lod : mLODs) {
                if (clamped >= lod.screenSizeThreshold && lod.screenSizeThreshold > bestThreshold) {
                    best = &lod;
                    bestThreshold = lod.screenSizeThreshold;
                }
            }
            return best;
        }

        void NkStaticMesh::BuildWireframe(NkIDevice* device) {
            if (!device || !mVBO.IsValid()) return;
            if (mWireframeIBO.IsValid()) return;

            NkVector<uint32> edges;
            const auto cacheIt = gStaticMeshCPUCache.find(this);
            const NkVector<uint32>* srcIndices = nullptr;
            if (cacheIt != gStaticMeshCPUCache.end() && !cacheIt->second.indices.Empty()) {
                srcIndices = &cacheIt->second.indices;
            }

            const uint32 triIndexCount = srcIndices
                ? (uint32)srcIndices->Size()
                : (mIndexCount > 0 ? mIndexCount : mVertexCount);
            if (triIndexCount < 3) return;

            edges.Reserve((usize)triIndexCount * 2);
            for (uint32 i = 0; i + 2 < triIndexCount; i += 3) {
                const uint32 a = srcIndices ? (*srcIndices)[i + 0] : (i + 0);
                const uint32 b = srcIndices ? (*srcIndices)[i + 1] : (i + 1);
                const uint32 c = srcIndices ? (*srcIndices)[i + 2] : (i + 2);
                if (a >= mVertexCount || b >= mVertexCount || c >= mVertexCount) continue;

                edges.PushBack(a); edges.PushBack(b);
                edges.PushBack(b); edges.PushBack(c);
                edges.PushBack(c); edges.PushBack(a);
            }

            if (edges.Empty()) return;

            mWireframeIBO = device->CreateBuffer(
                NkBufferDesc::Index((uint64)(edges.Size() * sizeof(uint32)), edges.Data()));
        }

        void NkStaticMesh::BuildPointVBO(NkIDevice* device) {
            if (!device || !mVBO.IsValid()) return;
            if (mPointVBO.IsValid()) return;
            if (mVertexCount == 0) return;

            const auto cacheIt = gStaticMeshCPUCache.find(this);
            if (cacheIt == gStaticMeshCPUCache.end() || cacheIt->second.vertices.Empty()) {
                mPointVBO = device->CreateBuffer(
                    NkBufferDesc::VertexDynamic((uint64)mVertexCount * sizeof(NkVec3f)));
                return;
            }

            NkVector<NkVec3f> positions;
            positions.Resize(cacheIt->second.vertices.Size());
            for (usize i = 0; i < cacheIt->second.vertices.Size(); ++i) {
                positions[i] = cacheIt->second.vertices[i].position;
            }

            mPointVBO = device->CreateBuffer(
                NkBufferDesc::Vertex((uint64)positions.Size() * sizeof(NkVec3f), positions.Data()));
        }

        // =============================================================================
        // NkDynamicMesh
        // =============================================================================

        bool NkDynamicMesh::Create(NkIDevice* device, uint32 maxVertices, uint32 maxIndices) {
            Destroy();

            if (!device || !device->IsValid() || maxVertices == 0) return false;

            mDevice = device;
            mMaxVertices = maxVertices;
            mMaxIndices = maxIndices;
            mCPUVerts.Reserve(maxVertices);
            if (maxIndices > 0) mCPUIndices.Reserve(maxIndices);

            mVBO = device->CreateBuffer(NkBufferDesc::VertexDynamic((uint64)maxVertices * sizeof(NkVertex3D)));
            if (!mVBO.IsValid()) {
                Destroy();
                return false;
            }

            if (maxIndices > 0) {
                mIBO = device->CreateBuffer(NkBufferDesc::IndexDynamic((uint64)maxIndices * sizeof(uint32)));
                if (!mIBO.IsValid()) {
                    Destroy();
                    return false;
                }
            }

            return true;
        }

        void NkDynamicMesh::Destroy() {
            if (!mDevice) {
                mCPUVerts.Clear();
                mCPUIndices.Clear();
                mCurrentVertex = 0;
                mCurrentIndex = 0;
                return;
            }

            if (mIBO.IsValid()) mDevice->DestroyBuffer(mIBO);
            if (mVBO.IsValid()) mDevice->DestroyBuffer(mVBO);

            mIBO = {};
            mVBO = {};
            mCPUVerts.Clear();
            mCPUIndices.Clear();
            mCurrentVertex = 0;
            mCurrentIndex = 0;
            mMaxVertices = 0;
            mMaxIndices = 0;
            mDevice = nullptr;
        }

        void NkDynamicMesh::Begin() {
            mCPUVerts.Clear();
            mCPUIndices.Clear();
            mCurrentVertex = 0;
            mCurrentIndex = 0;
        }

        void NkDynamicMesh::AddVertex(const NkVertex3D& v) {
            if (mCurrentVertex >= mMaxVertices) return;
            mCPUVerts.PushBack(v);
            ++mCurrentVertex;
        }

        void NkDynamicMesh::AddIndex(uint32 i) {
            if (!mIBO.IsValid()) return;
            if (mCurrentIndex >= mMaxIndices) return;
            mCPUIndices.PushBack(i);
            ++mCurrentIndex;
        }

        void NkDynamicMesh::AddTriangle(const NkVertex3D& a, const NkVertex3D& b, const NkVertex3D& c) {
            const uint32 base = mCurrentVertex;
            AddVertex(a);
            AddVertex(b);
            AddVertex(c);
            if (mIBO.IsValid()) {
                AddIndex(base + 0);
                AddIndex(base + 1);
                AddIndex(base + 2);
            }
        }

        void NkDynamicMesh::AddQuad(const NkVertex3D& a, const NkVertex3D& b,
                                    const NkVertex3D& c, const NkVertex3D& d) {
            const uint32 base = mCurrentVertex;
            AddVertex(a);
            AddVertex(b);
            AddVertex(c);
            AddVertex(d);
            if (mIBO.IsValid()) {
                AddIndex(base + 0);
                AddIndex(base + 1);
                AddIndex(base + 2);
                AddIndex(base + 0);
                AddIndex(base + 2);
                AddIndex(base + 3);
            }
        }

        void NkDynamicMesh::End() {
            if (!mDevice || !mVBO.IsValid()) return;

            if (!mCPUVerts.Empty()) {
                mDevice->WriteBuffer(mVBO, mCPUVerts.Data(),
                    (uint64)(mCPUVerts.Size() * sizeof(NkVertex3D)), 0);
            }

            if (mIBO.IsValid() && !mCPUIndices.Empty()) {
                mDevice->WriteBuffer(mIBO, mCPUIndices.Data(),
                    (uint64)(mCPUIndices.Size() * sizeof(uint32)), 0);
            }
        }

        // =============================================================================
        // NkEditableMesh
        // =============================================================================

        uint32 NkEditableMesh::AddVertex(const NkVec3f& pos, const NkVec2f& uv) {
            NkEditVertex v;
            v.position = pos;
            v.normal = {0.0f, 1.0f, 0.0f};
            v.uv = uv;
            v.id = ++mNextID;
            mVertices.PushBack(v);
            return v.id;
        }

        uint32 NkEditableMesh::FindOrAddEdge(uint32 v0, uint32 v1) {
            for (auto& e : mEdges) {
                if ((e.v0 == v0 && e.v1 == v1) || (e.v0 == v1 && e.v1 == v0)) {
                    return e.id;
                }
            }

            NkEditEdge edge;
            edge.v0 = v0;
            edge.v1 = v1;
            edge.id = ++mNextID;
            mEdges.PushBack(edge);
            return edge.id;
        }

        uint32 NkEditableMesh::AddEdge(uint32 v0, uint32 v1) {
            return FindOrAddEdge(v0, v1);
        }

        uint32 NkEditableMesh::AddFace(const NkVector<uint32>& verts, uint32 materialIdx) {
            if (verts.Size() < 3) return 0;

            NkEditFace face;
            face.vertexIndices = verts;
            face.materialIdx = materialIdx;
            face.id = ++mNextID;
            face.normal = {0.0f, 1.0f, 0.0f};

            for (uint32 i = 0; i < (uint32)verts.Size(); ++i) {
                const uint32 a = verts[i];
                const uint32 b = verts[(i + 1) % (uint32)verts.Size()];
                FindOrAddEdge(a, b);
            }

            mFaces.PushBack(face);
            return face.id;
        }

        uint32 NkEditableMesh::AddTriangle(uint32 a, uint32 b, uint32 c, uint32 mat) {
            NkVector<uint32> verts;
            verts.PushBack(a);
            verts.PushBack(b);
            verts.PushBack(c);
            return AddFace(verts, mat);
        }

        uint32 NkEditableMesh::AddQuadFace(uint32 a, uint32 b, uint32 c, uint32 d, uint32 mat) {
            NkVector<uint32> verts;
            verts.PushBack(a);
            verts.PushBack(b);
            verts.PushBack(c);
            verts.PushBack(d);
            return AddFace(verts, mat);
        }

        void NkEditableMesh::MoveVertex(uint32 id, const NkVec3f& newPos) {
            const uint32 idx = FindEditVertexIndexById(mVertices, id);
            if (idx == UINT32_MAX) return;
            mVertices[idx].position = newPos;
        }

        void NkEditableMesh::DeleteVertex(uint32 id) {
            const uint32 idx = FindEditVertexIndexById(mVertices, id);
            if (idx == UINT32_MAX) return;

            mVertices.Erase(mVertices.Begin() + idx);

            // Remove edges referencing this vertex index and remap others.
            for (uint32 i = 0; i < (uint32)mEdges.Size();) {
                auto& e = mEdges[i];
                if (e.v0 == idx || e.v1 == idx) {
                    mEdges.Erase(mEdges.Begin() + i);
                    continue;
                }
                if (e.v0 > idx) --e.v0;
                if (e.v1 > idx) --e.v1;
                ++i;
            }

            for (uint32 i = 0; i < (uint32)mFaces.Size();) {
                auto& f = mFaces[i];
                bool removed = false;
                for (uint32 j = 0; j < (uint32)f.vertexIndices.Size();) {
                    if (f.vertexIndices[j] == idx) {
                        f.vertexIndices.Erase(f.vertexIndices.Begin() + j);
                        removed = true;
                    } else {
                        if (f.vertexIndices[j] > idx) --f.vertexIndices[j];
                        ++j;
                    }
                }
                if (removed && f.vertexIndices.Size() < 3) {
                    mFaces.Erase(mFaces.Begin() + i);
                    continue;
                }
                ++i;
            }
        }

        void NkEditableMesh::DeleteEdge(uint32 id) {
            const uint32 idx = FindEditEdgeIndexById(mEdges, id);
            if (idx == UINT32_MAX) return;
            mEdges.Erase(mEdges.Begin() + idx);
        }

        void NkEditableMesh::DeleteFace(uint32 id) {
            const uint32 idx = FindEditFaceIndexById(mFaces, id);
            if (idx == UINT32_MAX) return;
            mFaces.Erase(mFaces.Begin() + idx);
        }

        void NkEditableMesh::SelectVertex(uint32 id, bool s) {
            const uint32 idx = FindEditVertexIndexById(mVertices, id);
            if (idx == UINT32_MAX) return;
            mVertices[idx].selected = s;
        }

        void NkEditableMesh::SelectEdge(uint32 id, bool s) {
            const uint32 idx = FindEditEdgeIndexById(mEdges, id);
            if (idx == UINT32_MAX) return;
            mEdges[idx].selected = s;
        }

        void NkEditableMesh::SelectFace(uint32 id, bool s) {
            const uint32 idx = FindEditFaceIndexById(mFaces, id);
            if (idx == UINT32_MAX) return;
            mFaces[idx].selected = s;
        }

        void NkEditableMesh::SelectAll(bool s) {
            for (auto& v : mVertices) v.selected = s;
            for (auto& e : mEdges) e.selected = s;
            for (auto& f : mFaces) f.selected = s;
        }

        void NkEditableMesh::DeselectAll() {
            SelectAll(false);
        }

        void NkEditableMesh::ExtrudeFace(uint32 faceId, const NkVec3f& offset) {
            const uint32 faceIdx = FindEditFaceIndexById(mFaces, faceId);
            if (faceIdx == UINT32_MAX) return;

            const NkEditFace src = mFaces[faceIdx];
            if (src.vertexIndices.Size() < 3) return;

            NkVector<uint32> extruded;
            extruded.Reserve(src.vertexIndices.Size());
            for (uint32 idx : src.vertexIndices) {
                if (idx >= (uint32)mVertices.Size()) continue;
                const NkEditVertex& oldV = mVertices[idx];
                const uint32 newId = AddVertex(
                    {oldV.position.x + offset.x, oldV.position.y + offset.y, oldV.position.z + offset.z},
                    oldV.uv);
                const uint32 newIdx = FindEditVertexIndexById(mVertices, newId);
                if (newIdx != UINT32_MAX) extruded.PushBack(newIdx);
            }
            if (extruded.Size() != src.vertexIndices.Size()) return;

            AddFace(extruded, src.materialIdx);

            const uint32 count = (uint32)src.vertexIndices.Size();
            for (uint32 i = 0; i < count; ++i) {
                const uint32 a0 = src.vertexIndices[i];
                const uint32 a1 = src.vertexIndices[(i + 1) % count];
                const uint32 b1 = extruded[(i + 1) % count];
                const uint32 b0 = extruded[i];
                AddQuadFace(a0, a1, b1, b0, src.materialIdx);
            }
        }

        void NkEditableMesh::Subdivide(uint32 iterations) {
            for (uint32 it = 0; it < iterations; ++it) {
                NkVector<NkEditFace> original = mFaces;
                mFaces.Clear();

                for (auto& f : original) {
                    if (f.vertexIndices.Size() < 3) continue;

                    NkVec3f center = {0, 0, 0};
                    for (uint32 vi : f.vertexIndices) {
                        if (vi >= (uint32)mVertices.Size()) continue;
                        center.x += mVertices[vi].position.x;
                        center.y += mVertices[vi].position.y;
                        center.z += mVertices[vi].position.z;
                    }
                    const float inv = 1.0f / (float)f.vertexIndices.Size();
                    center.x *= inv;
                    center.y *= inv;
                    center.z *= inv;

                    const uint32 centerId = AddVertex(center, {0, 0});
                    const uint32 centerIdx = FindEditVertexIndexById(mVertices, centerId);
                    if (centerIdx == UINT32_MAX) continue;

                    for (uint32 i = 0; i < (uint32)f.vertexIndices.Size(); ++i) {
                        const uint32 a = f.vertexIndices[i];
                        const uint32 b = f.vertexIndices[(i + 1) % (uint32)f.vertexIndices.Size()];
                        AddTriangle(a, b, centerIdx, f.materialIdx);
                    }
                }
            }
        }

        void NkEditableMesh::InsetFace(uint32 faceId, float amount) {
            const uint32 faceIdx = FindEditFaceIndexById(mFaces, faceId);
            if (faceIdx == UINT32_MAX) return;
            if (amount <= 0.0f) return;

            NkEditFace& f = mFaces[faceIdx];
            if (f.vertexIndices.Size() < 3) return;

            NkVec3f c = {0, 0, 0};
            for (uint32 vi : f.vertexIndices) {
                if (vi >= (uint32)mVertices.Size()) continue;
                c.x += mVertices[vi].position.x;
                c.y += mVertices[vi].position.y;
                c.z += mVertices[vi].position.z;
            }
            const float inv = 1.0f / (float)f.vertexIndices.Size();
            c.x *= inv;
            c.y *= inv;
            c.z *= inv;

            for (uint32 vi : f.vertexIndices) {
                if (vi >= (uint32)mVertices.Size()) continue;
                NkVec3f p = mVertices[vi].position;
                NkVec3f d = {c.x - p.x, c.y - p.y, c.z - p.z};
                mVertices[vi].position = {
                    p.x + d.x * amount,
                    p.y + d.y * amount,
                    p.z + d.z * amount
                };
            }
        }

        void NkEditableMesh::BevelEdge(uint32 edgeId, float amount, int segments) {
            (void)segments;
            const uint32 edgeIdx = FindEditEdgeIndexById(mEdges, edgeId);
            if (edgeIdx == UINT32_MAX || amount <= 0.0f) return;

            NkEditEdge& e = mEdges[edgeIdx];
            if (e.v0 >= (uint32)mVertices.Size() || e.v1 >= (uint32)mVertices.Size()) return;

            NkVec3f p0 = mVertices[e.v0].position;
            NkVec3f p1 = mVertices[e.v1].position;
            NkVec3f dir = Vec3Normalize({p1.x - p0.x, p1.y - p0.y, p1.z - p0.z}, {1, 0, 0});
            NkVec3f shift = {dir.x * amount * 0.5f, dir.y * amount * 0.5f, dir.z * amount * 0.5f};

            mVertices[e.v0].position = {p0.x + shift.x, p0.y + shift.y, p0.z + shift.z};
            mVertices[e.v1].position = {p1.x - shift.x, p1.y - shift.y, p1.z - shift.z};
        }

        void NkEditableMesh::MergeByDistance(float threshold) {
            if (mVertices.Size() < 2 || threshold <= 0.0f) return;

            for (uint32 i = 0; i < (uint32)mVertices.Size(); ++i) {
                for (uint32 j = i + 1; j < (uint32)mVertices.Size();) {
                    if (Vec3Distance(mVertices[i].position, mVertices[j].position) <= threshold) {
                        for (auto& e : mEdges) {
                            if (e.v0 == j) e.v0 = i;
                            else if (e.v0 > j) --e.v0;
                            if (e.v1 == j) e.v1 = i;
                            else if (e.v1 > j) --e.v1;
                        }
                        for (auto& f : mFaces) {
                            for (auto& vi : f.vertexIndices) {
                                if (vi == j) vi = i;
                                else if (vi > j) --vi;
                            }
                        }
                        mVertices.Erase(mVertices.Begin() + j);
                        continue;
                    }
                    ++j;
                }
            }
        }

        void NkEditableMesh::FlipNormals() {
            for (auto& f : mFaces) {
                for (uint32 i = 0, j = (uint32)f.vertexIndices.Size() - 1; i < j; ++i, --j) {
                    traits::NkSwap(f.vertexIndices[i], f.vertexIndices[j]);
                }
                f.normal = {-f.normal.x, -f.normal.y, -f.normal.z};
            }
            for (auto& v : mVertices) {
                v.normal = {-v.normal.x, -v.normal.y, -v.normal.z};
            }
        }

        void NkEditableMesh::RecalculateNormals() {
            for (auto& v : mVertices) {
                v.normal = {0.0f, 0.0f, 0.0f};
            }

            for (auto& f : mFaces) {
                if (f.vertexIndices.Size() < 3) {
                    f.normal = {0.0f, 1.0f, 0.0f};
                    continue;
                }
                const uint32 i0 = f.vertexIndices[0];
                const uint32 i1 = f.vertexIndices[1];
                const uint32 i2 = f.vertexIndices[2];
                if (i0 >= (uint32)mVertices.Size() || i1 >= (uint32)mVertices.Size() || i2 >= (uint32)mVertices.Size()) {
                    continue;
                }

                const NkVec3f p0 = mVertices[i0].position;
                const NkVec3f p1 = mVertices[i1].position;
                const NkVec3f p2 = mVertices[i2].position;
                const NkVec3f e1 = {p1.x - p0.x, p1.y - p0.y, p1.z - p0.z};
                const NkVec3f e2 = {p2.x - p0.x, p2.y - p0.y, p2.z - p0.z};
                const NkVec3f n = Vec3Normalize(Vec3Cross(e1, e2));
                f.normal = n;

                for (uint32 vi : f.vertexIndices) {
                    if (vi >= (uint32)mVertices.Size()) continue;
                    mVertices[vi].normal = {
                        mVertices[vi].normal.x + n.x,
                        mVertices[vi].normal.y + n.y,
                        mVertices[vi].normal.z + n.z
                    };
                }
            }

            for (auto& v : mVertices) {
                v.normal = Vec3Normalize(v.normal);
            }
        }

        void NkEditableMesh::TriangulateFace(const NkEditFace& f, NkVector<uint32>& outIndices) const {
            if (f.vertexIndices.Size() < 3) return;
            for (uint32 i = 1; i + 1 < (uint32)f.vertexIndices.Size(); ++i) {
                outIndices.PushBack(f.vertexIndices[0]);
                outIndices.PushBack(f.vertexIndices[i]);
                outIndices.PushBack(f.vertexIndices[i + 1]);
            }
        }

        NkMeshData NkEditableMesh::ToMeshData() const {
            NkMeshData out;
            out.topology = NkMeshTopology::NK_TRIANGLES;
            out.vertices.Resize(mVertices.Size());

            for (uint32 i = 0; i < (uint32)mVertices.Size(); ++i) {
                NkVertex3D v{};
                v.position = mVertices[i].position;
                v.normal = mVertices[i].normal;
                v.uv0 = mVertices[i].uv;
                v.color = mVertices[i].color;
                out.vertices[i] = v;
            }

            for (const auto& face : mFaces) {
                TriangulateFace(face, out.indices);
            }

            NkSubMesh sm;
            sm.firstIndex = 0;
            sm.indexCount = (uint32)out.indices.Size();
            sm.firstVertex = 0;
            sm.vertexCount = (uint32)out.vertices.Size();
            sm.materialIdx = 0;
            sm.name = "Editable";
            out.subMeshes.PushBack(sm);

            out.RecalculateAABB();
            return out;
        }

        uint32 NkEditableMesh::SelectedVertexCount() const {
            uint32 count = 0;
            for (const auto& v : mVertices) if (v.selected) ++count;
            return count;
        }

        uint32 NkEditableMesh::SelectedEdgeCount() const {
            uint32 count = 0;
            for (const auto& e : mEdges) if (e.selected) ++count;
            return count;
        }

        uint32 NkEditableMesh::SelectedFaceCount() const {
            uint32 count = 0;
            for (const auto& f : mFaces) if (f.selected) ++count;
            return count;
        }

        // =============================================================================
        // NkSkinnedMesh
        // =============================================================================

        bool NkSkinnedMesh::CreateSkinned(NkIDevice* device, const NkMeshData& data,
                                          const NkVector<NkBone>& skeleton) {
            if (!Create(device, data)) return false;
            gSkinnedMeshDevices[this] = device;
            mSkeleton = skeleton;

            if (mSkeleton.Empty()) return true;

            const uint64 bytes = (uint64)mSkeleton.Size() * sizeof(NkMat4f);
            mSkinSSBO = device->CreateBuffer(NkBufferDesc::Storage(bytes));
            if (!mSkinSSBO.IsValid()) {
                mSkeleton.Clear();
                gSkinnedMeshDevices.erase(this);
                Destroy();
                return false;
            }
            return true;
        }

        void NkSkinnedMesh::DestroySkinnedData() {
            auto it = gSkinnedMeshDevices.find(this);
            if (mSkinSSBO.IsValid() && it != gSkinnedMeshDevices.end() && it->second) {
                it->second->DestroyBuffer(mSkinSSBO);
            }
            mSkeleton.Clear();
            mSkinSSBO = {};
            gSkinnedMeshDevices.erase(this);
        }

        void NkSkinnedMesh::ComputeSkinMatrices(const NkPose& pose, NkVector<NkMat4f>& outMats) const {
            outMats.Clear();
            if (mSkeleton.Empty()) return;

            outMats.Resize(mSkeleton.Size(), NkMat4f::Identity());
            NkVector<NkMat4f> global;
            global.Resize(mSkeleton.Size(), NkMat4f::Identity());

            for (uint32 i = 0; i < (uint32)mSkeleton.Size(); ++i) {
                NkMat4f local = NkMat4f::Identity();
                if (i < (uint32)pose.localTransforms.Size()) {
                    local = pose.localTransforms[i];
                }
                const int32 parent = mSkeleton[i].parent;
                if (parent >= 0 && parent < (int32)global.Size()) {
                    global[i] = global[(uint32)parent] * local;
                } else {
                    global[i] = local;
                }
                outMats[i] = global[i] * mSkeleton[i].inverseBindPose;
            }
        }

        void NkSkinnedMesh::UploadSkinMatrices(NkIDevice* device, const NkVector<NkMat4f>& mats) {
            if (!device || mats.Empty()) return;
            gSkinnedMeshDevices[this] = device;

            const uint64 bytes = (uint64)mats.Size() * sizeof(NkMat4f);
            if (!mSkinSSBO.IsValid()) {
                mSkinSSBO = device->CreateBuffer(NkBufferDesc::Storage(bytes));
            }
            if (!mSkinSSBO.IsValid()) return;
            device->WriteBuffer(mSkinSSBO, mats.Data(), bytes, 0);
        }

        // =============================================================================
        // Procedural mesh generators
        // =============================================================================

        namespace NkProceduralMesh {

            NkMeshData Plane(float width, float depth, uint32 res) {
                NkMeshData m;
                m.topology = NkMeshTopology::NK_TRIANGLES;
                const uint32 seg = NkMax(1u, res);
                const uint32 vx = seg + 1;
                const uint32 vz = seg + 1;
                m.vertices.Reserve((usize)vx * (usize)vz);
                m.indices.Reserve((usize)seg * (usize)seg * 6);

                for (uint32 z = 0; z < vz; ++z) {
                    for (uint32 x = 0; x < vx; ++x) {
                        const float tx = (float)x / (float)seg;
                        const float tz = (float)z / (float)seg;
                        NkVertex3D v{};
                        v.position = {(tx - 0.5f) * width, 0.0f, (tz - 0.5f) * depth};
                        v.normal = {0, 1, 0};
                        v.tangent = {1, 0, 0};
                        v.bitangent = {0, 0, 1};
                        v.uv0 = {tx, tz};
                        v.color = {1, 1, 1, 1};
                        m.vertices.PushBack(v);
                    }
                }

                for (uint32 z = 0; z < seg; ++z) {
                    for (uint32 x = 0; x < seg; ++x) {
                        const uint32 i0 = z * vx + x;
                        const uint32 i1 = i0 + 1;
                        const uint32 i2 = i0 + vx;
                        const uint32 i3 = i2 + 1;
                        m.indices.PushBack(i0); m.indices.PushBack(i2); m.indices.PushBack(i1);
                        m.indices.PushBack(i1); m.indices.PushBack(i2); m.indices.PushBack(i3);
                    }
                }

                NkSubMesh sm;
                sm.firstIndex = 0;
                sm.indexCount = (uint32)m.indices.Size();
                sm.firstVertex = 0;
                sm.vertexCount = (uint32)m.vertices.Size();
                sm.name = "Plane";
                m.subMeshes.PushBack(sm);

                m.RecalculateAABB();
                return m;
            }

            NkMeshData Cube(float size) {
                const float h = size * 0.5f;
                NkMeshData m;
                m.topology = NkMeshTopology::NK_TRIANGLES;
                m.vertices.Reserve(24);
                m.indices.Reserve(36);

                const NkVec3f pos[8] = {
                    {-h,-h,-h}, { h,-h,-h}, { h, h,-h}, {-h, h,-h},
                    {-h,-h, h}, { h,-h, h}, { h, h, h}, {-h, h, h}
                };

                struct FaceDef { uint32 i0, i1, i2, i3; NkVec3f n; NkVec3f t; };
                const FaceDef faces[6] = {
                    {4,5,6,7, {0,0,1},  {1,0,0}},   // front
                    {1,0,3,2, {0,0,-1}, {-1,0,0}},  // back
                    {0,4,7,3, {-1,0,0}, {0,0,1}},   // left
                    {5,1,2,6, {1,0,0},  {0,0,-1}},  // right
                    {3,7,6,2, {0,1,0},  {1,0,0}},   // top
                    {0,1,5,4, {0,-1,0}, {1,0,0}}    // bottom
                };

                for (const auto& f : faces) {
                    const uint32 base = (uint32)m.vertices.Size();
                    NkVertex3D v0{}, v1{}, v2{}, v3{};
                    v0.position = pos[f.i0]; v1.position = pos[f.i1];
                    v2.position = pos[f.i2]; v3.position = pos[f.i3];
                    v0.normal = v1.normal = v2.normal = v3.normal = f.n;
                    v0.tangent = v1.tangent = v2.tangent = v3.tangent = f.t;
                    v0.uv0 = {0,1}; v1.uv0 = {1,1}; v2.uv0 = {1,0}; v3.uv0 = {0,0};
                    v0.color = v1.color = v2.color = v3.color = {1,1,1,1};
                    m.vertices.PushBack(v0);
                    m.vertices.PushBack(v1);
                    m.vertices.PushBack(v2);
                    m.vertices.PushBack(v3);

                    m.indices.PushBack(base + 0); m.indices.PushBack(base + 1); m.indices.PushBack(base + 2);
                    m.indices.PushBack(base + 0); m.indices.PushBack(base + 2); m.indices.PushBack(base + 3);
                }

                NkSubMesh sm;
                sm.firstIndex = 0;
                sm.indexCount = (uint32)m.indices.Size();
                sm.vertexCount = (uint32)m.vertices.Size();
                sm.name = "Cube";
                m.subMeshes.PushBack(sm);
                m.RecalculateAABB();
                return m;
            }

            NkMeshData Sphere(float radius, uint32 stacks, uint32 slices) {
                const uint32 st = NkMax(3u, stacks);
                const uint32 sl = NkMax(3u, slices);

                NkMeshData m;
                m.topology = NkMeshTopology::NK_TRIANGLES;
                m.vertices.Reserve((usize)(st + 1) * (usize)(sl + 1));
                m.indices.Reserve((usize)st * (usize)sl * 6);

                for (uint32 y = 0; y <= st; ++y) {
                    const float v = (float)y / (float)st;
                    const float phi = v * (float)NkPi;
                    const float sp = NkSin(phi);
                    const float cp = NkCos(phi);

                    for (uint32 x = 0; x <= sl; ++x) {
                        const float u = (float)x / (float)sl;
                        const float theta = u * 2.0f * (float)NkPi;
                        const float stheta = NkSin(theta);
                        const float ctheta = NkCos(theta);

                        NkVec3f n = {ctheta * sp, cp, stheta * sp};
                        NkVertex3D vert{};
                        vert.normal = n;
                        vert.position = {n.x * radius, n.y * radius, n.z * radius};
                        vert.tangent = {-stheta, 0, ctheta};
                        vert.bitangent = Vec3Cross(vert.normal, vert.tangent);
                        vert.uv0 = {u, v};
                        vert.color = {1,1,1,1};
                        m.vertices.PushBack(vert);
                    }
                }

                const uint32 stride = sl + 1;
                for (uint32 y = 0; y < st; ++y) {
                    for (uint32 x = 0; x < sl; ++x) {
                        const uint32 i0 = y * stride + x;
                        const uint32 i1 = i0 + 1;
                        const uint32 i2 = i0 + stride;
                        const uint32 i3 = i2 + 1;
                        m.indices.PushBack(i0); m.indices.PushBack(i2); m.indices.PushBack(i1);
                        m.indices.PushBack(i1); m.indices.PushBack(i2); m.indices.PushBack(i3);
                    }
                }

                NkSubMesh sm;
                sm.firstIndex = 0;
                sm.indexCount = (uint32)m.indices.Size();
                sm.vertexCount = (uint32)m.vertices.Size();
                sm.name = "Sphere";
                m.subMeshes.PushBack(sm);
                m.RecalculateAABB();
                return m;
            }

            NkMeshData Cylinder(float radius, float height, uint32 segments) {
                const uint32 seg = NkMax(3u, segments);
                const float half = height * 0.5f;

                NkMeshData m;
                m.topology = NkMeshTopology::NK_TRIANGLES;

                for (uint32 i = 0; i <= seg; ++i) {
                    const float t = (float)i / (float)seg;
                    const float a = t * 2.0f * (float)NkPi;
                    const float c = NkCos(a);
                    const float s = NkSin(a);

                    NkVertex3D v0{}, v1{};
                    v0.position = {c * radius, -half, s * radius};
                    v1.position = {c * radius,  half, s * radius};
                    v0.normal = v1.normal = {c, 0, s};
                    v0.uv0 = {t, 1}; v1.uv0 = {t, 0};
                    v0.color = v1.color = {1,1,1,1};
                    m.vertices.PushBack(v0);
                    m.vertices.PushBack(v1);
                }

                for (uint32 i = 0; i < seg; ++i) {
                    const uint32 i0 = i * 2;
                    const uint32 i1 = i0 + 1;
                    const uint32 i2 = i0 + 2;
                    const uint32 i3 = i0 + 3;
                    m.indices.PushBack(i0); m.indices.PushBack(i1); m.indices.PushBack(i2);
                    m.indices.PushBack(i2); m.indices.PushBack(i1); m.indices.PushBack(i3);
                }

                NkSubMesh sm;
                sm.firstIndex = 0;
                sm.indexCount = (uint32)m.indices.Size();
                sm.vertexCount = (uint32)m.vertices.Size();
                sm.name = "Cylinder";
                m.subMeshes.PushBack(sm);
                m.RecalculateAABB();
                return m;
            }

            NkMeshData Cone(float radius, float height, uint32 segments) {
                const uint32 seg = NkMax(3u, segments);
                const float half = height * 0.5f;

                NkMeshData m;
                m.topology = NkMeshTopology::NK_TRIANGLES;

                NkVertex3D apex{};
                apex.position = {0, half, 0};
                apex.normal = {0, 1, 0};
                apex.uv0 = {0.5f, 0.0f};
                apex.color = {1,1,1,1};
                const uint32 apexIndex = 0;
                m.vertices.PushBack(apex);

                for (uint32 i = 0; i <= seg; ++i) {
                    const float t = (float)i / (float)seg;
                    const float a = t * 2.0f * (float)NkPi;
                    const float c = NkCos(a);
                    const float s = NkSin(a);
                    NkVertex3D v{};
                    v.position = {c * radius, -half, s * radius};
                    v.normal = Vec3Normalize({c, radius / NkMax(1e-6f, height), s});
                    v.uv0 = {t, 1.0f};
                    v.color = {1,1,1,1};
                    m.vertices.PushBack(v);
                }

                for (uint32 i = 0; i < seg; ++i) {
                    const uint32 b0 = 1 + i;
                    const uint32 b1 = 1 + i + 1;
                    m.indices.PushBack(apexIndex);
                    m.indices.PushBack(b0);
                    m.indices.PushBack(b1);
                }

                NkSubMesh sm;
                sm.firstIndex = 0;
                sm.indexCount = (uint32)m.indices.Size();
                sm.vertexCount = (uint32)m.vertices.Size();
                sm.name = "Cone";
                m.subMeshes.PushBack(sm);
                m.RecalculateAABB();
                return m;
            }

            NkMeshData Torus(float major, float minor, uint32 majorSeg, uint32 minorSeg) {
                const uint32 ms = NkMax(3u, majorSeg);
                const uint32 ns = NkMax(3u, minorSeg);

                NkMeshData m;
                m.topology = NkMeshTopology::NK_TRIANGLES;
                m.vertices.Reserve((usize)(ms + 1) * (usize)(ns + 1));
                m.indices.Reserve((usize)ms * (usize)ns * 6);

                for (uint32 i = 0; i <= ms; ++i) {
                    const float u = (float)i / (float)ms;
                    const float a = u * 2.0f * (float)NkPi;
                    const float ca = NkCos(a);
                    const float sa = NkSin(a);

                    for (uint32 j = 0; j <= ns; ++j) {
                        const float v = (float)j / (float)ns;
                        const float b = v * 2.0f * (float)NkPi;
                        const float cb = NkCos(b);
                        const float sb = NkSin(b);

                        const float r = major + minor * cb;
                        NkVec3f pos = {r * ca, minor * sb, r * sa};
                        NkVec3f n = Vec3Normalize({cb * ca, sb, cb * sa});

                        NkVertex3D vert{};
                        vert.position = pos;
                        vert.normal = n;
                        vert.uv0 = {u, v};
                        vert.color = {1,1,1,1};
                        m.vertices.PushBack(vert);
                    }
                }

                const uint32 row = ns + 1;
                for (uint32 i = 0; i < ms; ++i) {
                    for (uint32 j = 0; j < ns; ++j) {
                        const uint32 i0 = i * row + j;
                        const uint32 i1 = i0 + 1;
                        const uint32 i2 = i0 + row;
                        const uint32 i3 = i2 + 1;
                        m.indices.PushBack(i0); m.indices.PushBack(i2); m.indices.PushBack(i1);
                        m.indices.PushBack(i1); m.indices.PushBack(i2); m.indices.PushBack(i3);
                    }
                }

                NkSubMesh sm;
                sm.firstIndex = 0;
                sm.indexCount = (uint32)m.indices.Size();
                sm.vertexCount = (uint32)m.vertices.Size();
                sm.name = "Torus";
                m.subMeshes.PushBack(sm);
                m.RecalculateAABB();
                return m;
            }

            NkMeshData Capsule(float radius, float height, uint32 segments) {
                // Fallback production-safe: cylinder approximates the capsule body.
                return Cylinder(radius, NkMax(height, radius * 2.0f), NkMax(3u, segments));
            }

            NkMeshData Arrow(float length, float radius) {
                // Fallback production-safe: return a slender cone-like arrow.
                return Cone(NkMax(0.001f, radius * 2.0f), NkMax(0.05f, length), 24);
            }

            NkMeshData Grid(float width, float depth, uint32 divisions) {
                // Use a tessellated plane; callers can render wireframe mode for grid visuals.
                return Plane(width, depth, NkMax(1u, divisions));
            }

            NkMeshData Terrain(const float* heightmap, uint32 w, uint32 d, float scale, float heightScale) {
                NkMeshData m;
                if (!heightmap || w < 2 || d < 2) return m;

                m.topology = NkMeshTopology::NK_TRIANGLES;
                m.vertices.Reserve((usize)w * (usize)d);
                m.indices.Reserve((usize)(w - 1) * (usize)(d - 1) * 6);

                const float halfW = (float)(w - 1) * 0.5f;
                const float halfD = (float)(d - 1) * 0.5f;
                for (uint32 z = 0; z < d; ++z) {
                    for (uint32 x = 0; x < w; ++x) {
                        const uint32 idx = z * w + x;
                        NkVertex3D v{};
                        v.position = {
                            ((float)x - halfW) * scale,
                            heightmap[idx] * heightScale,
                            ((float)z - halfD) * scale
                        };
                        v.normal = {0, 1, 0};
                        v.uv0 = {
                            (float)x / (float)(w - 1),
                            (float)z / (float)(d - 1)
                        };
                        v.color = {1, 1, 1, 1};
                        m.vertices.PushBack(v);
                    }
                }

                for (uint32 z = 0; z + 1 < d; ++z) {
                    for (uint32 x = 0; x + 1 < w; ++x) {
                        const uint32 i0 = z * w + x;
                        const uint32 i1 = i0 + 1;
                        const uint32 i2 = i0 + w;
                        const uint32 i3 = i2 + 1;
                        m.indices.PushBack(i0); m.indices.PushBack(i2); m.indices.PushBack(i1);
                        m.indices.PushBack(i1); m.indices.PushBack(i2); m.indices.PushBack(i3);
                    }
                }

                m.RecalculateNormals();
                m.RecalculateTangents();
                m.RecalculateAABB();
                NkSubMesh sm;
                sm.indexCount = (uint32)m.indices.Size();
                sm.vertexCount = (uint32)m.vertices.Size();
                sm.name = "Terrain";
                m.subMeshes.PushBack(sm);
                return m;
            }

            NkMeshData Billboard(float width, float height) {
                return Plane(width, height, 1);
            }

            NkMeshData Skybox(float size) {
                return Cube(size);
            }

        } // namespace NkProceduralMesh

        // =============================================================================
        // NkMeshLibrary
        // =============================================================================

        NkMeshLibrary& NkMeshLibrary::Get() {
            static NkMeshLibrary sLib;
            return sLib;
        }

        void NkMeshLibrary::Init(NkIDevice* device) {
            mDevice = device;
            mByPath.Clear();
            for (auto& p : mBuiltins) p = nullptr;
        }

        void NkMeshLibrary::Shutdown() {
            std::unordered_set<NkStaticMesh*> uniqueMeshes;
            for (auto& kv : mByPath) {
                if (kv.Second) uniqueMeshes.insert(kv.Second);
            }
            for (NkStaticMesh* mesh : uniqueMeshes) {
                delete mesh;
            }
            mByPath.Clear();
            for (auto& p : mBuiltins) p = nullptr;
            mDevice = nullptr;
        }

        NkStaticMesh* NkMeshLibrary::Load(const char* path) {
            if (!mDevice || !path || !path[0]) return nullptr;

            NkString key(path);
            if (auto* found = mByPath.Find(key)) return *found;

            if (!FileExists(path)) {
                logger_src.Infof("[NkMeshLibrary] Fichier mesh introuvable: %s\n", path);
                return nullptr;
            }

            NkStaticMesh* mesh = NkModelImporter::ImportMesh(mDevice, path, nullptr, NkModelImportOptions::Default());
            if (!mesh) return nullptr;

            mesh->SetName(path);
            mByPath[key] = mesh;
            return mesh;
        }

        NkStaticMesh* NkMeshLibrary::Find(const NkString& name) const {
            auto* p = mByPath.Find(name);
            return p ? *p : nullptr;
        }

        void NkMeshLibrary::Register(NkStaticMesh* mesh) {
            if (!mesh) return;

            NkString key = mesh->GetName();
            if (key.Empty()) {
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "Mesh_%llu", (unsigned long long)mesh->GetID().id);
                key = tmp;
                mesh->SetName(key.CStr());
            }

            mByPath[key] = mesh;
        }

        NkStaticMesh* NkMeshLibrary::GetCube()     { return CreateBuiltin(NK_CUBE); }
        NkStaticMesh* NkMeshLibrary::GetSphere()   { return CreateBuiltin(NK_SPHERE); }
        NkStaticMesh* NkMeshLibrary::GetPlane()    { return CreateBuiltin(NK_PLANE); }
        NkStaticMesh* NkMeshLibrary::GetCapsule()  { return CreateBuiltin(NK_CAPSULE); }
        NkStaticMesh* NkMeshLibrary::GetCylinder() { return CreateBuiltin(NK_CYLINDER); }
        NkStaticMesh* NkMeshLibrary::GetCone()     { return CreateBuiltin(NK_CONE); }
        NkStaticMesh* NkMeshLibrary::GetTorus()    { return CreateBuiltin(NK_TORUS); }
        NkStaticMesh* NkMeshLibrary::GetArrow()    { return CreateBuiltin(NK_ARROW); }
        NkStaticMesh* NkMeshLibrary::GetSkybox()   { return CreateBuiltin(NK_SKYBOX); }

        NkStaticMesh* NkMeshLibrary::CreateBuiltin(uint32 type) {
            if (!mDevice) return nullptr;
            if (type >= (uint32)(sizeof(mBuiltins) / sizeof(mBuiltins[0]))) return nullptr;
            if (mBuiltins[type]) return mBuiltins[type];

            NkMeshData data;
            const char* name = "Builtin";

            switch (type) {
                case NK_CUBE:     data = NkProceduralMesh::Cube(); break;
                case NK_SPHERE:   data = NkProceduralMesh::Sphere(); break;
                case NK_PLANE:    data = NkProceduralMesh::Plane(); break;
                case NK_CAPSULE:  data = NkProceduralMesh::Capsule(); break;
                case NK_CYLINDER: data = NkProceduralMesh::Cylinder(); break;
                case NK_CONE:     data = NkProceduralMesh::Cone(); break;
                case NK_TORUS:    data = NkProceduralMesh::Torus(); break;
                case NK_ARROW:    data = NkProceduralMesh::Arrow(); break;
                case NK_SKYBOX:   data = NkProceduralMesh::Skybox(); break;
                default: return nullptr;
            }

            switch (type) {
                case NK_CUBE: name = "Cube"; break;
                case NK_SPHERE: name = "Sphere"; break;
                case NK_PLANE: name = "Plane"; break;
                case NK_CAPSULE: name = "Capsule"; break;
                case NK_CYLINDER: name = "Cylinder"; break;
                case NK_CONE: name = "Cone"; break;
                case NK_TORUS: name = "Torus"; break;
                case NK_ARROW: name = "Arrow"; break;
                case NK_SKYBOX: name = "Skybox"; break;
                default: break;
            }

            NkStaticMesh* mesh = new NkStaticMesh();
            if (!mesh->Create(mDevice, data)) {
                delete mesh;
                return nullptr;
            }
            mesh->SetName(name);
            mBuiltins[type] = mesh;
            mByPath[NkString(name)] = mesh;
            return mesh;
        }

    } // namespace renderer
} // namespace nkentseu

