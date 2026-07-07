// =============================================================================
// NKMeshAITest — Étape 1 de la modélisation par IA : IMITATION d'un expert.
//
// Pont ÉDITEUR (NkEditMesh + couche de commandes, NKRenderer) <-> ML NKAI.
//   Observation : NkEditMesh -> vecteur de features.
//   Action      : type de commande de modélisation (Subdiv/Extrude/Mirror/Array).
//   Expert      : petite policy heuristique (décide l'action selon l'état).
//   Apprentissage : un MLP (NKNN) apprend à IMITER l'expert -> la précision monte.
//
// Prouve la chaîne obs -> policy -> action de bout en bout, from-scratch, petite
// échelle. Fondation pour : apprendre depuis de vraies sessions .nkmec, puis RL
// vers une cible, puis conditionnement image/texte (image-to-3D). Réutilisable
// pour NkAnima (obs = squelette, actions = poses).
// =============================================================================
#include "NKRenderer/Mesh/NkEditMesh.h"
#include "NKTensor/NkTensor.h"
#include "NKNN/NkNN.h"
#include "NKOptim/NkOptim.h"
#include "NKData/NkData.h"
#include "NKTrain/NkTrain.h"
#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::ai;
using namespace nkentseu::math;   // NkVec3f
using renderer::NkEditMesh;

// ── Espace d'actions (classes que la policy prédit) ──────────────────────────
enum ModelAction { ACT_SUBDIV = 0, ACT_EXTRUDE = 1, ACT_MIRROR = 2, ACT_ARRAY = 3, ACT_COUNT = 4 };
static const int FEAT_DIM = 8;

// PRNG déterministe (zéro-STL).
struct Rng {
    uint32 s;
    uint32 nextu() { s = s * 1664525u + 1013904223u; return s; }
    float  f()     { return (float)(nextu() >> 8) * (1.0f / 16777216.0f); }
    int    i(int n){ return (int)(nextu() % (uint32)n); }
};

static float fmin3(float a, float b, float c){ float m=a<b?a:b; return m<c?m:c; }
static float fmax3(float a, float b, float c){ float m=a>b?a:b; return m>c?m:c; }
static float fabsf_(float x){ return x<0.f?-x:x; }

// Cube NkEditMesh (échelles sx/sy/sz ; ox = décalage X pour créer de l'asymétrie).
static void BuildCube(NkEditMesh& m, float sx, float sy, float sz, float ox) {
    renderer::NkVertex3D v[8];
    const float px[8]={-1,1,1,-1,-1,1,1,-1};
    const float py[8]={-1,-1,1,1,-1,-1,1,1};
    const float pz[8]={-1,-1,-1,-1,1,1,1,1};
    for (int k=0;k<8;k++){ v[k]=renderer::NkVertex3D{};
        v[k].pos = { px[k]*sx*0.5f + ox, py[k]*sy*0.5f, pz[k]*sz*0.5f };
        v[k].normal = {0.f,1.f,0.f}; v[k].tangent = {1.f,0.f,0.f}; v[k].color = 0xFFFFFFFFu;
    }
    const uint32 idx[36] = {
        0,1,2, 0,2,3,   4,6,5, 4,7,6,
        0,4,5, 0,5,1,   1,5,6, 1,6,2,
        2,6,7, 2,7,3,   3,7,4, 3,4,0 };
    m.BuildFromIndexed(v, 8, idx, 36, /*quadify*/true);
}

// OBSERVATION : NkEditMesh -> vecteur de features [FEAT_DIM].
static void Encode(const NkEditMesh& m, float* f) {
    const uint32 vc = m.VertCount();
    uint32 fc = 0; for (uint32 i=0;i<m.FaceCount();i++) if (m.faces[i].alive) fc++;  // faces VIVANTES
    NkVec3f mn{1e30f,1e30f,1e30f}, mx{-1e30f,-1e30f,-1e30f};
    for (uint32 k=0;k<vc;k++){ NkVec3f p=m.verts[k].pos;
        if(p.x<mn.x)mn.x=p.x; if(p.y<mn.y)mn.y=p.y; if(p.z<mn.z)mn.z=p.z;
        if(p.x>mx.x)mx.x=p.x; if(p.y>mx.y)mx.y=p.y; if(p.z>mx.z)mx.z=p.z; }
    const float ex=mx.x-mn.x, ey=mx.y-mn.y, ez=mx.z-mn.z;
    const float emax=fmax3(ex,ey,ez)+1e-6f, emin=fmin3(ex,ey,ez);
    const float cx=0.5f*(mn.x+mx.x);
    f[0]=(float)vc/64.f;  f[1]=(float)fc/64.f;
    f[2]=ex; f[3]=ey; f[4]=ez;
    f[5]=emin/emax;                        // aplatissement (1=cube, ~0=plat)
    f[6]=fabsf_(cx);                       // asymétrie X (0=centré)
    f[7]=(float)fc/(float)(vc>0?vc:1u);    // ratio faces/sommets
}

// EXPERT heuristique : features -> action (le "professeur" à imiter).
static int Expert(const float* f) {
    const float fc = f[1]*64.f;
    if (fc < 8.f)     return ACT_SUBDIV;     // peu de faces -> subdiviser
    if (f[5] < 0.35f) return ACT_EXTRUDE;    // très plat -> extruder (donner du volume)
    if (f[6] > 0.20f) return ACT_MIRROR;     // asymétrique -> miroir
    return ACT_ARRAY;                         // sinon -> array
}

// Diversifie l'état (varie le nb de faces via subdivisions).
static void Diversify(NkEditMesh& m, Rng& rng) {
    if (rng.i(3) != 0) { m.SelectAll(); renderer::NkSubdivideParams p; p.cuts=1; m.SubdivideSelectedFaces(p); }
}

// Génère `n` échantillons (features + labels) via l'expert.
static void GenDataset(uint32 n, Rng& rng, NkVector<float>& feats, NkVector<int32>& labels) {
    feats.Clear(); labels.Clear();
    for (uint32 s=0;s<n;s++){
        NkEditMesh m;
        float sx=0.3f+rng.f()*1.7f, sy=0.3f+rng.f()*1.7f, sz=0.3f+rng.f()*1.7f;
        if (rng.i(3)==0){ int ax=rng.i(3); if(ax==0)sx*=0.15f; else if(ax==1)sy*=0.15f; else sz*=0.15f; }
        const float ox = (rng.i(2)==0) ? 0.f : (rng.f()*1.2f);   // parfois décalé -> asymétrie
        BuildCube(m, sx, sy, sz, ox);
        const int nd = rng.i(3);
        for (int j=0;j<nd;j++) Diversify(m, rng);
        float f[FEAT_DIM]; Encode(m, f);
        const int a = Expert(f);
        for (int d=0;d<FEAT_DIM;d++) feats.PushBack(f[d]);
        labels.PushBack((int32)a);
    }
}

int main() {
    printf("=== NKMeshAITest — Etape 1 : imitation d'un expert de modelisation ===\n");
    printf("(from-scratch, petite echelle, pedagogique)\n\n");
    Rng rng{ 20260707u };

    NkVector<float> trF, teF; NkVector<int32> trL, teL;
    GenDataset(3000u, rng, trF, trL);
    GenDataset(600u,  rng, teF, teL);

    int cnt[ACT_COUNT] = {0,0,0,0};
    for (uint32 i=0;i<(uint32)trL.Size();i++) cnt[trL[i]]++;
    const char* an[ACT_COUNT] = {"Subdiv","Extrude","Mirror","Array"};
    printf("train=%u  test=%u  | classes train:", (uint32)trL.Size(), (uint32)teL.Size());
    for (int c=0;c<ACT_COUNT;c++) printf(" %s=%d", an[c], cnt[c]);
    printf("\n");

    NkTensor Xtr = NkTensor::FromData(NkShape{ (int64)trL.Size(), (int64)FEAT_DIM }, trF.Data(), NkDType::NK_F32);
    NkTensor Xte = NkTensor::FromData(NkShape{ (int64)teL.Size(), (int64)FEAT_DIM }, teF.Data(), NkDType::NK_F32);
    data::NkDataset   dsTr(Xtr, trL, ACT_COUNT);
    data::NkDataset   dsTe(Xte, teL, ACT_COUNT);
    data::NkDataLoader trainLoader(dsTr, 64u, /*shuffle*/true,  7u);
    data::NkDataLoader testLoader (dsTe, 64u, /*shuffle*/false, 1u);

    // MLP : FEAT_DIM -> 32 -> ACT_COUNT (logits).
    nn::NkDense l1(FEAT_DIM, 32u, 1234u);
    nn::NkDense l2(32u, ACT_COUNT, 5678u);
    auto forward = [&](const NkVar& x){ return l2.Forward(nn::Relu(l1.Forward(x))); };
    NkVector<NkVar> params; l1.Parameters(params); l2.Parameters(params);
    optim::NkAdam adam(params, /*lr*/0.02f);

    printf("\n--- entrainement (imitation de l'expert) ---\n");
    for (int e=1;e<=60;e++){
        train::EpochStats st = train::TrainEpoch(forward, adam, trainLoader);
        if (e==1 || e%10==0) printf("  epoch %2d  loss=%.4f  acc_train=%.1f%%\n", e, st.loss, st.acc*100.0);
    }
    const double accTe = train::Accuracy(forward, testLoader);
    printf("\n--- RESULTAT : precision sur donnees JAMAIS VUES = %.1f%% ---\n", accTe*100.0);
    printf("La policy (reseau) a appris a imiter les decisions de modelisation de l'expert\n");
    printf("depuis le seul etat du maillage. Prochaine etape : apprendre depuis de vraies\n");
    printf("sessions .nkmec, puis conditionner par une cible / image / texte.\n");
    return 0;
}
