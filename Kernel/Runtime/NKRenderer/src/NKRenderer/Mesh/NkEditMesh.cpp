// =============================================================================
// NkEditMesh.cpp — NKRenderer — maillage éditable demi-arête (n-gon)
// =============================================================================
#include "NkEditMesh.h"
#include "NKContainers/Associative/NkHashMap.h"

namespace nkentseu {
    namespace renderer {

        void NkEditMesh::BuildFromIndexed(const NkVertex3D* v, uint32 vc,
                                          const uint32* idx, uint32 ic, bool quadify) {
            Clear();
            verts.Resize(vc);
            for (uint32 i=0;i<vc;i++){
                verts[i].pos=v[i].pos; verts[i].normal=v[i].normal; verts[i].uv=v[i].uv;
                verts[i].hedge=NK_EM_INVALID; verts[i].sel=0;
            }
            const uint32 triCount = ic/3;
            faces.Reserve(triCount);
            hedges.Reserve(ic);
            for (uint32 t=0;t<triCount;t++){
                const uint32 a=idx[t*3], b=idx[t*3+1], c=idx[t*3+2];
                const NkEmId f  = (NkEmId)faces.Size();
                const NkEmId h0 = (NkEmId)hedges.Size(), h1=h0+1, h2=h0+2;
                Hedge e0,e1,e2;
                e0.origin=a; e0.next=h1; e0.face=f;
                e1.origin=b; e1.next=h2; e1.face=f;
                e2.origin=c; e2.next=h0; e2.face=f;
                hedges.PushBack(e0); hedges.PushBack(e1); hedges.PushBack(e2);
                Face fc; fc.hedge=h0; fc.alive=1; faces.PushBack(fc);
                if (verts[a].hedge==NK_EM_INVALID) verts[a].hedge=h0;
                if (verts[b].hedge==NK_EM_INVALID) verts[b].hedge=h1;
                if (verts[c].hedge==NK_EM_INVALID) verts[c].hedge=h2;
            }
            LinkTwins();
            RecomputeNormals();
            if (quadify) Quadify();
        }

        uint32 NkEditMesh::FaceSize(NkEmId f) const {
            if (f>=(NkEmId)faces.Size() || !faces[f].alive) return 0;
            const NkEmId start=faces[f].hedge; if (start==NK_EM_INVALID) return 0;
            NkEmId h=start; uint32 n=0, guard=0;
            do { ++n; h=hedges[h].next; if(++guard>100000u) break; } while(h!=start && h!=NK_EM_INVALID);
            return n;
        }

        void NkEditMesh::Quadify(float32 coplanarDot) {
            // Paires de triangles CONSÉCUTIFS (issus de la triangulation quad-par-quad).
            for (uint32 f1=0; f1+1<(uint32)faces.Size(); f1+=2){
                const uint32 f2=f1+1;
                if (!faces[f1].alive || !faces[f2].alive) continue;
                if (FaceSize(f1)!=3 || FaceSize(f2)!=3) continue;
                if (faces[f1].normal.Dot(faces[f2].normal) < coplanarDot) continue;
                // Demi-arête partagée h (dans f1) dont le twin est dans f2.
                NkEmId h=NK_EM_INVALID, start=faces[f1].hedge, hh=start; uint32 guard=0;
                do { const NkEmId tw=hedges[hh].twin;
                     if (tw!=NK_EM_INVALID && hedges[tw].alive && hedges[tw].face==f2){ h=hh; break; }
                     hh=hedges[hh].next; } while(hh!=start && ++guard<100000u);
                if (h==NK_EM_INVALID) continue;            // triangles non adjacents
                const NkEmId tw=hedges[h].twin;
                const NkEmId hA=hedges[h].next,  hB=hedges[hA].next;   // f1 : b->c, c->a
                const NkEmId hC=hedges[tw].next, hD=hedges[hC].next;   // f2 : a->d, d->b
                hedges[hB].next=hC; hedges[hD].next=hA;                // recoud la boucle quad
                hedges[hA].face=f1; hedges[hB].face=f1; hedges[hC].face=f1; hedges[hD].face=f1;
                faces[f1].hedge=hA; faces[f2].alive=0;
                const uint32 a=hedges[h].origin, b=hedges[tw].origin;
                hedges[h].alive=0; hedges[tw].alive=0;
                hedges[h].face=NK_EM_INVALID; hedges[tw].face=NK_EM_INVALID;
                verts[a].hedge=hC; verts[b].hedge=hA;                  // repointe (h/tw morts)
            }
            RecomputeNormals();
        }

        void NkEditMesh::LinkTwins() {
            NkHashMap<uint64, NkEmId> map; map.Reserve((uint32)hedges.Size());
            for (uint32 h=0; h<(uint32)hedges.Size(); ++h){
                const uint32 o = hedges[h].origin;
                const uint32 d = hedges[hedges[h].next].origin;
                const uint64 opp = ((uint64)d<<32) | (uint64)o;   // demi-arête opposée (d->o)
                NkEmId* found = map.Find(opp);
                if (found){ hedges[h].twin = *found; hedges[*found].twin = h; }
                else       { map.InsertOrAssign(((uint64)o<<32)|(uint64)d, h); }
            }
        }

        void NkEditMesh::GetFaceVerts(NkEmId f, NkVector<NkEmId>& out) const {
            out.Clear();
            if (f>=(NkEmId)faces.Size()) return;
            const NkEmId start = faces[f].hedge;
            if (start==NK_EM_INVALID) return;
            NkEmId h=start; uint32 guard=0;
            do {
                out.PushBack(hedges[h].origin);
                h = hedges[h].next;
                if (++guard > 100000u) break;      // garde-fou (topologie cassée)
            } while (h!=start && h!=NK_EM_INVALID);
        }

        void NkEditMesh::RecomputeNormals() {
            for (uint32 i=0;i<(uint32)verts.Size();++i) verts[i].normal = {0.f,0.f,0.f};
            NkVector<NkEmId> loop;
            for (uint32 f=0; f<(uint32)faces.Size(); ++f){
                if (!faces[f].alive) continue;
                loop.Clear(); GetFaceVerts(f, loop);
                if (loop.Size()<3) continue;
                const NkVec3f p0=verts[loop[0]].pos, p1=verts[loop[1]].pos, p2=verts[loop[2]].pos;
                NkVec3f n = (p1-p0).Cross(p2-p0);          // pondéré par l'aire (non normalisé)
                float32 l=n.Len(); faces[f].normal = (l>1e-8f)? n*(1.f/l) : NkVec3f{0.f,1.f,0.f};
                for (uint32 k=0;k<(uint32)loop.Size();++k) verts[loop[k]].normal = verts[loop[k]].normal + n;
            }
            for (uint32 i=0;i<(uint32)verts.Size();++i){
                float32 l=verts[i].normal.Len();
                verts[i].normal = (l>1e-8f)? verts[i].normal*(1.f/l) : NkVec3f{0.f,1.f,0.f};
            }
        }

        void NkEditMesh::GetUniqueEdges(NkVector<uint32>& outPairs) const {
            outPairs.Clear();
            for (uint32 h=0; h<(uint32)hedges.Size(); ++h){
                if (!hedges[h].alive) continue;         // arête interne dissoute (quadify)
                const NkEmId tw = hedges[h].twin;
                if (tw==NK_EM_INVALID || h < tw){       // une seule des deux demi-arêtes
                    const uint32 o = hedges[h].origin;
                    const uint32 d = hedges[hedges[h].next].origin;
                    outPairs.PushBack(o); outPairs.PushBack(d);
                }
            }
        }

        void NkEditMesh::Triangulate(NkVector<NkVertex3D>& outV, NkVector<uint32>& outIdx,
                                     NkVector<NkEmId>& outTriFace) const {
            outV.Clear(); outIdx.Clear(); outTriFace.Clear();
            outV.Resize((uint32)verts.Size());
            for (uint32 i=0;i<(uint32)verts.Size();++i){
                NkVertex3D nv{};
                nv.pos=verts[i].pos; nv.normal=verts[i].normal; nv.tangent={1.f,0.f,0.f};
                nv.uv=verts[i].uv; nv.uv2={0.f,0.f}; nv.color=0xFFFFFFFFu;
                outV[i]=nv;
            }
            NkVector<NkEmId> loop;
            for (uint32 f=0; f<(uint32)faces.Size(); ++f){
                if (!faces[f].alive) continue;
                loop.Clear(); GetFaceVerts(f, loop);
                if (loop.Size()<3) continue;
                for (uint32 i=1; i+1<(uint32)loop.Size(); ++i){   // éventail
                    outIdx.PushBack(loop[0]); outIdx.PushBack(loop[i]); outIdx.PushBack(loop[i+1]);
                    outTriFace.PushBack((NkEmId)f);
                }
            }
        }

    } // namespace renderer
} // namespace nkentseu
