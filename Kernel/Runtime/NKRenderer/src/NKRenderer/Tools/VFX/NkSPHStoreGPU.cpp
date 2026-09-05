// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkSPHStoreGPU.cpp — DFSPH sur GPU (2026-09-05), voir le .h. Les formules sont celles de
// NkSPHSolver.cpp (noyau cubique, alpha, kappa, Morris, XSPH, filets) : les cinq témoins
// du repos et le front de rupture de barrage doivent donner les mêmes chiffres qu'en CPU.
// Deux différences DITES : XSPH est Jacobi ici (Gauss-Seidel sur CPU), et Morris lit la
// vitesse brute (sur CPU, déjà lissée par XSPH).
// =============================================================================
#include "NkSPHStoreGPU.h"
#include "NkSPHSolver.h"
#include "NkVFXSystem.h"
#include "NkForceField.h"
#include "NKRHI/Core/NkGraphicsApi.h"
#include "NKSL/Compiler/NkSLCompiler.h"
#include "NKSL/ShaderConvert/NkShaderConvert.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "NKTime/NkChrono.h" // en dernier (namespace `time`)

namespace nkentseu {
	namespace renderer {

		// ── Préambule commun des noyaux ────────────────────────────────────────────
		// Dialecte NkSL : boucles `i = i + 1u`, pas de bool, un seul return par fonction,
		// écritures de vec4 entiers. Les tampons sont ceux du .h, dans le même ordre.
		static const char *kSPHCommon = R"NKSL(
@binding(set=0, binding=0) buffer PosBuf { vec4 p[]; } X;
@binding(set=0, binding=1) buffer VelBuf { vec4 v[]; } V;
@binding(set=0, binding=2) buffer KeyIdxBuf { uint a[]; } KV;   // fluide, fpad x (cle, indice)
@binding(set=0, binding=3) buffer GKeyIdxBuf { uint a[]; } GKV; // fantomes, gpad x (cle, indice)
@binding(set=0, binding=4) buffer FCellBuf { uint a[]; } FSE;   // numCells x (debut, fin) fluide
@binding(set=0, binding=5) buffer GCellBuf { uint a[]; } GSE;   // numCells x (debut, fin) fantomes
@binding(set=0, binding=6) buffer FieldBuf { float f[]; } FL;   // M x 8 : rho, alpha, kappa, kappa chaud, kappa total, erreur, voisines fluides, 0
@binding(set=0, binding=7) buffer TmpBuf { vec4 t[]; } T;
@binding(set=0, binding=8) buffer RedBuf { float r[]; } R;
@binding(set=0, binding=9) buffer InstF { float f[]; } IF;
@binding(set=0, binding=10) buffer InstU { uint u[]; } IU;
@binding(set=0, binding=11) buffer BirthBuf { vec4 b[]; } B;
@binding(set=0, binding=14) buffer NeighBuf { uint n[]; } NB;   // cap x 64 : indices des voisines (fluide < cap, fantome >= cap)
@binding(set=0, binding=12) uniform Params {
    vec4 hm;    // h, m, rho0, dt
    vec4 grav;  // gravite xyz, invDt
    vec4 gmin;  // origine de la grille xyz, inv = 1/h
    vec4 bmin;  // boite min xyz, restitution
    vec4 bmax;  // boite max xyz, maxSpeed
    vec4 visc;  // xsph, wallFriction, nu, warmScale
    uint nx; uint ny; uint nz; uint numCells;
    uint cap; uint nb; uint fpad; uint gpad;
    uint surfaceMode; uint mode; uint accum; uint nbirths;
    float size; uint color; uint pad0; uint pad1;
} p;
@binding(set=0, binding=13) uniform Sort { uint j; uint k; uint which; uint n; } q;
@binding(set=0, binding=15) uniform Field { vec4 f0; vec4 f1; vec4 f2; } wf;
layout(local_size_x = 256) in;

float kW(float r) {
    float H = p.hm.x;
    float qq = r / H;
    float kk = 8.0 / (3.14159265 * H * H * H);
    float w = 0.0;
    if (qq < 1.0) {
        if (qq <= 0.5) {
            w = kk * (6.0 * (qq * qq * qq - qq * qq) + 1.0);
        } else {
            float t = 1.0 - qq;
            w = kk * 2.0 * t * t * t;
        }
    }
    return w;
}
float kDW(float r) {
    float H = p.hm.x;
    float qq = r / H;
    float l = 48.0 / (3.14159265 * H * H * H * H);
    float w = 0.0;
    if (qq < 1.0 && r > 0.000000001) {
        if (qq <= 0.5) {
            w = l * qq * (3.0 * qq - 2.0);
        } else {
            float t = 1.0 - qq;
            w = -l * t * t;
        }
    }
    return w;
}
int cellCoord(float v, float g, uint n) {
    int c = int((v - g) * p.gmin.w);
    if (c < 0) { c = 0; }
    if (c >= int(n)) { c = int(n) - 1; }
    return c;
}
uint cellKey(vec3 pos) {
    int cx = cellCoord(pos.x, p.gmin.x, p.nx);
    int cy = cellCoord(pos.y, p.gmin.y, p.ny);
    int cz = cellCoord(pos.z, p.gmin.z, p.nz);
    return uint(cx) + p.nx * (uint(cy) + p.ny * uint(cz));
}
uint cellAt(vec3 pos, int dx, int dy, int dz) {
    int x = cellCoord(pos.x, p.gmin.x, p.nx) + dx;
    int y = cellCoord(pos.y, p.gmin.y, p.ny) + dy;
    int z = cellCoord(pos.z, p.gmin.z, p.nz) + dz;
    uint c = p.numCells;
    if (x >= 0 && y >= 0 && z >= 0 && x < int(p.nx) && y < int(p.ny) && z < int(p.nz)) {
        c = uint(x) + p.nx * (uint(y) + p.ny * uint(z));
    }
    return c;
}
)NKSL";

		// naissances / morts vers leurs emplacements (compte = nbirths)
		static const char *kBirthBody = R"NKSL(
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < p.nbirths) {
        uint k3 = i * 3u;
        vec4 pl = B.b[k3];
        vec4 vr = B.b[k3 + 1u];
        vec4 sf = B.b[k3 + 2u];
        uint slot = uint(sf.x);
        if (sf.z > 0.5) {
            X.p[slot] = vec4(pl.x, pl.y, pl.z, 1.0);
            V.v[slot] = vec4(vr.x, vr.y, vr.z, 0.0);
        } else {
            X.p[slot] = vec4(pl.x, pl.y, pl.z, 0.0);
            V.v[slot] = vec4(0.0, 0.0, 0.0, 0.0);
        }
        FL.f[8u * (slot) + 3u] = 0.0;
        FL.f[8u * (slot) + 4u] = 0.0;
    }
}
)NKSL";

		// cles de cellule (which 0 : fluide sur fpad ; 1 : fantomes sur gpad)
		static const char *kKeyBody = R"NKSL(
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (q.which == 0u) {
        if (i < p.fpad) {
            uint key = (0u - 1u);
            if (i < p.cap) {
                vec4 pp = X.p[i];
                if (pp.w > 0.5 && pp.w < 1.5) {
                    key = cellKey(vec3(pp.x, pp.y, pp.z));
                }
            }
            KV.a[2u * (i)] = key;
            KV.a[2u * (i) + 1u] = i;
        }
    } else {
        if (i < p.gpad) {
            uint key = (0u - 1u);
            if (i < p.nb) {
                vec4 pp = X.p[p.cap + i];
                key = cellKey(vec3(pp.x, pp.y, pp.z));
            }
            GKV.a[2u * (i)] = key;
            GKV.a[2u * (i) + 1u] = p.cap + i;
        }
    }
}
)NKSL";

		// un pas du tri bitonique (q.j, q.k) sur (cle, indice)
		static const char *kSortBody = R"NKSL(
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < q.n) {
        uint ixj = i ^ q.j;
        if (ixj > i) {
            uint asc = 0u;
            if ((i & q.k) == 0u) { asc = 1u; }
            if (q.which == 0u) {
                uint a = KV.a[2u * (i)];
                uint b = KV.a[2u * (ixj)];
                uint gt = 0u;
                if (a > b) { gt = 1u; }
                if (gt == asc) {
                    KV.a[2u * (i)] = b;
                    KV.a[2u * (ixj)] = a;
                    uint va = KV.a[2u * (i) + 1u];
                    KV.a[2u * (i) + 1u] = KV.a[2u * (ixj) + 1u];
                    KV.a[2u * (ixj) + 1u] = va;
                }
            } else {
                uint a = GKV.a[2u * (i)];
                uint b = GKV.a[2u * (ixj)];
                uint gt = 0u;
                if (a > b) { gt = 1u; }
                if (gt == asc) {
                    GKV.a[2u * (i)] = b;
                    GKV.a[2u * (ixj)] = a;
                    uint va = GKV.a[2u * (i) + 1u];
                    GKV.a[2u * (i) + 1u] = GKV.a[2u * (ixj) + 1u];
                    GKV.a[2u * (ixj) + 1u] = va;
                }
            }
        }
    }
}
)NKSL";

		static const char *kClearBody = R"NKSL(
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < p.numCells) {
        if (q.which == 0u) {
            FSE.a[2u * (i)] = 0u;
            FSE.a[2u * (i) + 1u] = 0u;
        } else {
            GSE.a[2u * (i)] = 0u;
            GSE.a[2u * (i) + 1u] = 0u;
        }
    }
}
)NKSL";

		// debut / fin de cellule la ou la cle triee change
		static const char *kCellBody = R"NKSL(
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < q.n) {
        uint key = (0u - 1u);
        uint prev = (0u - 1u);
        uint next = (0u - 1u);
        if (q.which == 0u) {
            key = KV.a[2u * (i)];
            if (i > 0u) { prev = KV.a[2u * (i - 1u)]; }
            if (i + 1u < q.n) { next = KV.a[2u * (i + 1u)]; }
            if (key != (0u - 1u)) {
                if (prev != key) { FSE.a[2u * (key)] = i; }
                if (next != key) { FSE.a[2u * (key) + 1u] = i + 1u; }
            }
        } else {
            key = GKV.a[2u * (i)];
            if (i > 0u) { prev = GKV.a[2u * (i - 1u)]; }
            if (i + 1u < q.n) { next = GKV.a[2u * (i + 1u)]; }
            if (key != (0u - 1u)) {
                if (prev != key) { GSE.a[2u * (key)] = i; }
                if (next != key) { GSE.a[2u * (key) + 1u] = i + 1u; }
            }
        }
    }
}
)NKSL";

		// listes de voisines (une fois par sous-pas, 64 max, compte brut releve) -- les quatre passes de
		// voisinage lisent la liste au lieu de retraverser 27 cellules (mesure du 05/09 : c'etait le cout)
		static const char *kNeighBody = R"NKSL(
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < p.cap) {
        vec4 pi4 = X.p[i];
        uint nn = 0u;
        if (pi4.w > 0.5 && pi4.w < 1.5) {
            vec3 xi = vec3(pi4.x, pi4.y, pi4.z);
            float h2 = p.hm.x * p.hm.x;
            uint base = i * 64u;
            for (int dz = -1; dz <= 1; dz = dz + 1) {
                for (int dy = -1; dy <= 1; dy = dy + 1) {
                    for (int dx = -1; dx <= 1; dx = dx + 1) {
                        uint c = cellAt(xi, dx, dy, dz);
                        if (c < p.numCells) {
                            for (uint qq = FSE.a[2u * (c)]; qq < FSE.a[2u * (c) + 1u]; qq = qq + 1u) {
                                uint j = KV.a[2u * (qq) + 1u];
                                if (j != i) {
                                    vec4 pj = X.p[j];
                                    vec3 dd = xi - vec3(pj.x, pj.y, pj.z);
                                    if (dot(dd, dd) < h2) {
                                        if (nn < 64u) { NB.n[base + nn] = j; }
                                        nn = nn + 1u;
                                    }
                                }
                            }
                            for (uint qg = GSE.a[2u * (c)]; qg < GSE.a[2u * (c) + 1u]; qg = qg + 1u) {
                                uint j = GKV.a[2u * (qg) + 1u];
                                vec4 pj = X.p[j];
                                vec3 dd = xi - vec3(pj.x, pj.y, pj.z);
                                if (dot(dd, dd) < h2) {
                                    if (nn < 64u) { NB.n[base + nn] = j; }
                                    nn = nn + 1u;
                                }
                            }
                        }
                    }
                }
            }
        }
        FL.f[8u * (i) + 7u] = float(nn);
    }
}
)NKSL";

		// densite, alpha, voisines fluides (compte = cap)
		static const char *kDensBody = R"NKSL(
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < p.cap) {
        vec4 pi4 = X.p[i];
        if (pi4.w > 0.5 && pi4.w < 1.5) {
            vec3 xi = vec3(pi4.x, pi4.y, pi4.z);
            float m = p.hm.y;
            float rho = m * kW(0.0);
            vec3 sg = vec3(0.0, 0.0, 0.0);
            float sg2 = 0.0;
            uint cnt = 0u;
            uint nn = uint(FL.f[8u * (i) + 7u]);
            if (nn > 64u) { nn = 64u; }
            uint base = i * 64u;
            for (uint qn = 0u; qn < nn; qn = qn + 1u) {
                uint j = NB.n[base + qn];
                vec4 pj = X.p[j];
                vec3 dd = xi - vec3(pj.x, pj.y, pj.z);
                float r = sqrt(dot(dd, dd));
                rho = rho + m * kW(r);
                float s = 0.0;
                if (r > 0.000000001) { s = kDW(r) / r; }
                vec3 g = dd * (s * m);
                sg = sg + g;
                if (j < p.cap) {
                    sg2 = sg2 + dot(g, g);
                    cnt = cnt + 1u;
                }
            }
            FL.f[8u * (i)] = rho;
            float den = dot(sg, sg) + sg2;
            float al = 0.0;
            if (den > 0.000001) { al = rho / den; }
            FL.f[8u * (i) + 1u] = al;
            FL.f[8u * (i) + 6u] = float(cnt);
        } else {
            FL.f[8u * (i)] = p.hm.z;
            FL.f[8u * (i) + 1u] = 0.0;
            FL.f[8u * (i) + 6u] = 0.0;
        }
    }
}
)NKSL";

		// kappa d'une iteration : mode 0 = divergence nulle, 1 = densite constante (compte = cap)
		static const char *kKappaBody = R"NKSL(
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < p.cap) {
        vec4 pi4 = X.p[i];
        float kap = 0.0;
        float err = 0.0;
        if (pi4.w > 0.5 && pi4.w < 1.5) {
            vec3 xi = vec3(pi4.x, pi4.y, pi4.z);
            vec4 vi4 = V.v[i];
            vec3 vi = vec3(vi4.x, vi4.y, vi4.z);
            float m = p.hm.y;
            float rho0 = p.hm.z;
            float dt = p.hm.w;
            float div = 0.0;
            uint nn = uint(FL.f[8u * (i) + 7u]);
            if (nn > 64u) { nn = 64u; }
            uint base = i * 64u;
            for (uint qn = 0u; qn < nn; qn = qn + 1u) {
                uint j = NB.n[base + qn];
                vec4 pj = X.p[j];
                vec3 dd = xi - vec3(pj.x, pj.y, pj.z);
                float r = sqrt(dot(dd, dd));
                float s = 0.0;
                if (r > 0.000000001) { s = kDW(r) / r; }
                vec3 dv = vi;
                if (j < p.cap) {
                    vec4 vj4 = V.v[j];
                    dv = vi - vec3(vj4.x, vj4.y, vj4.z);
                }
                div = div + m * dot(dv, dd * s);
            }
            float al = FL.f[8u * (i) + 1u];
            if (p.mode == 0u) {
                float ddv = 0.0;
                if (FL.f[8u * (i) + 6u] >= 20.0) {
                    ddv = div;
                    if (ddv < 0.0) { ddv = 0.0; }
                }
                kap = ddv * al * p.grav.w;
                err = ddv * dt / rho0;
            } else {
                float ra = FL.f[8u * (i)] + dt * div;
                if (ra < rho0) {
                    if (p.surfaceMode == 1u) { ra = rho0; }
                    if (p.surfaceMode == 2u) { ra = rho0 + 0.5 * (ra - rho0); }
                }
                kap = (ra - rho0) * al * p.grav.w * p.grav.w;
                err = abs(ra - rho0) / rho0;
            }
        }
        FL.f[8u * (i) + 2u] = kap;
        FL.f[8u * (i) + 5u] = err;
    }
}
)NKSL";

		// correction de vitesse par les kappa (compte = cap) ; accum : KS += KP (demarrage a chaud)
		static const char *kCorrectBody = R"NKSL(
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < p.cap) {
        vec4 pi4 = X.p[i];
        if (pi4.w > 0.5 && pi4.w < 1.5) {
            vec3 xi = vec3(pi4.x, pi4.y, pi4.z);
            float m = p.hm.y;
            float dt = p.hm.w;
            float ki = FL.f[8u * (i) + 2u] / FL.f[8u * (i)];
            vec3 acc = vec3(0.0, 0.0, 0.0);
            uint nn = uint(FL.f[8u * (i) + 7u]);
            if (nn > 64u) { nn = 64u; }
            uint base = i * 64u;
            for (uint qn = 0u; qn < nn; qn = qn + 1u) {
                uint j = NB.n[base + qn];
                vec4 pj = X.p[j];
                vec3 dd = xi - vec3(pj.x, pj.y, pj.z);
                float r = sqrt(dot(dd, dd));
                float s = 0.0;
                if (r > 0.000000001) { s = kDW(r) / r; }
                float kj = 0.0;
                if (j < p.cap) { kj = FL.f[8u * (j) + 2u] / FL.f[8u * (j)]; }
                acc = acc + dd * (s * m * (ki + kj));
            }
            vec4 vi4 = V.v[i];
            V.v[i] = vec4(vi4.x - dt * acc.x, vi4.y - dt * acc.y, vi4.z - dt * acc.z, 0.0);
            if (p.accum == 1u) { FL.f[8u * (i) + 4u] = FL.f[8u * (i) + 4u] + FL.f[8u * (i) + 2u]; }
        }
    }
}
)NKSL";

		// demarrage a chaud : kappa = s x kappa total du pas precedent (compte = cap)
		static const char *kWarmBody = R"NKSL(
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < p.cap) {
        vec4 pi4 = X.p[i];
        float kap = 0.0;
        if (pi4.w > 0.5 && pi4.w < 1.5) {
            kap = p.visc.w * FL.f[8u * (i) + 3u];
        } else {
            FL.f[8u * (i) + 3u] = 0.0;
        }
        FL.f[8u * (i) + 2u] = kap;
        FL.f[8u * (i) + 4u] = kap;
    }
}
)NKSL";

		static const char *kStoreWarmBody = R"NKSL(
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < p.cap) {
        FL.f[8u * (i) + 3u] = FL.f[8u * (i) + 4u];
    }
}
)NKSL";

		// forces non-pression sur tampon : XSPH (Jacobi) + Morris, lues sur V, ecrites dans T
		static const char *kNonPBody = R"NKSL(
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < p.cap) {
        vec4 pi4 = X.p[i];
        if (pi4.w > 0.5 && pi4.w < 1.5) {
            vec3 xi = vec3(pi4.x, pi4.y, pi4.z);
            vec4 vi4 = V.v[i];
            vec3 vi = vec3(vi4.x, vi4.y, vi4.z);
            float h = p.hm.x;
            float m = p.hm.y;
            float rho0 = p.hm.z;
            float dt = p.hm.w;
            float xs = p.visc.x;
            float wf = p.visc.y;
            float nu = p.visc.z;
            float eps = 0.01 * h * h;
            float rhoi = FL.f[8u * (i)];
            float mui = rhoi * nu;
            vec3 accX = vec3(0.0, 0.0, 0.0);
            vec3 accM = vec3(0.0, 0.0, 0.0);
            uint nn = uint(FL.f[8u * (i) + 7u]);
            if (nn > 64u) { nn = 64u; }
            uint base = i * 64u;
            for (uint qn = 0u; qn < nn; qn = qn + 1u) {
                uint j = NB.n[base + qn];
                vec4 pj = X.p[j];
                vec3 dd = xi - vec3(pj.x, pj.y, pj.z);
                float r2 = dot(dd, dd);
                float r = sqrt(r2);
                float s = 0.0;
                if (r > 0.000000001) { s = kDW(r) / r; }
                float xg = dot(dd, dd * s);
                if (j < p.cap) {
                    vec4 vj4 = V.v[j];
                    vec3 vj = vec3(vj4.x, vj4.y, vj4.z);
                    float rhoj = FL.f[8u * (j)];
                    accX = accX + (vj - vi) * ((m / rhoj) * kW(r));
                    if (nu > 0.0) {
                        float cij = m * (mui + rhoj * nu) / (rhoi * rhoj) * xg / (r2 + eps);
                        accM = accM + (vi - vj) * cij;
                    }
                } else {
                    accX = accX - vi * (wf * (m / rho0) * kW(r));
                    if (nu > 0.0) {
                        float cij = m * (mui + rho0 * nu) / (rhoi * rho0) * xg / (r2 + eps);
                        accM = accM + vi * cij;
                    }
                }
            }
            vec3 vo = vi + accX * xs + accM * dt;
            T.t[i] = vec4(vo.x, vo.y, vo.z, 0.0);
        }
    }
}
)NKSL";

		static const char *kApplyBody = R"NKSL(
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < p.cap) {
        vec4 pi4 = X.p[i];
        if (pi4.w > 0.5 && pi4.w < 1.5) {
            vec4 t4 = T.t[i];
            float dt = p.hm.w;
            vec3 a = vec3(p.grav.x, p.grav.y, p.grav.z) + nkForceField(vec3(pi4.x, pi4.y, pi4.z));
            V.v[i] = vec4(t4.x + a.x * dt, t4.y + a.y * dt, t4.z + a.z * dt, 0.0);
        }
    }
}
)NKSL";

		// 256 partiels du residu (somme de ER sur les vivantes)
		static const char *kReduceBody = R"NKSL(
@stage(compute)
@entry
void main() {
    uint t = gl_GlobalInvocationID.x;
    if (t < 256u) {
        float s = 0.0;
        for (uint i = t; i < p.cap; i = i + 256u) {
            s = s + FL.f[8u * (i) + 5u];
        }
        R.r[t] = s;
    }
}
)NKSL";

		// integration, filets (vitesse, boite), instances (compte = cap)
		static const char *kIntegBody = R"NKSL(
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < p.cap) {
        vec4 pi4 = X.p[i];
        uint o = i * 6u;
        if (pi4.w > 0.5 && pi4.w < 1.5) {
            vec4 vi4 = V.v[i];
            float vx = vi4.x;
            float vy = vi4.y;
            float vz = vi4.z;
            float dt = p.hm.w;
            float vcap = p.bmax.w;
            float clamped = 0.0;
            float v2 = vx * vx + vy * vy + vz * vz;
            if (v2 > vcap * vcap) {
                float sc = vcap / sqrt(v2);
                vx = vx * sc;
                vy = vy * sc;
                vz = vz * sc;
                clamped = 1.0;
            }
            float xx = pi4.x + vx * dt;
            float xy = pi4.y + vy * dt;
            float xz = pi4.z + vz * dt;
            float e = 0.0 - p.bmin.w;
            if (xx < p.bmin.x) { xx = p.bmin.x; vx = vx * e; }
            if (xx > p.bmax.x) { xx = p.bmax.x; vx = vx * e; }
            if (xy < p.bmin.y) { xy = p.bmin.y; vy = vy * e; }
            if (xy > p.bmax.y) { xy = p.bmax.y; vy = vy * e; }
            if (xz < p.bmin.z) { xz = p.bmin.z; vz = vz * e; }
            if (xz > p.bmax.z) { xz = p.bmax.z; vz = vz * e; }
            X.p[i] = vec4(xx, xy, xz, 1.0);
            V.v[i] = vec4(vx, vy, vz, clamped);
            IF.f[o] = xx;
            IF.f[o + 1u] = xy;
            IF.f[o + 2u] = xz;
            IF.f[o + 3u] = p.size;
            IU.u[o + 4u] = p.color;
            IF.f[o + 5u] = 0.0;
        } else {
            IF.f[o + 3u] = 0.0;
        }
    }
}
)NKSL";

		// 256 partiels de statistiques : 16 valeurs par fil, a partir de R[256]
		static const char *kStatsBody = R"NKSL(
@stage(compute)
@entry
void main() {
    uint t = gl_GlobalInvocationID.x;
    if (t < 256u) {
        float rho0 = p.hm.z;
        float h = p.hm.x;
        float sumRho = 0.0;
        float minRho = 1000000000.0;
        float maxRho = 0.0;
        float vmax = 0.0;
        float xmax = -1000000000.0;
        float ymax = -1000000000.0;
        float ymin = 1000000000.0;
        float xdense = -1000000000.0;
        float floorSum = 0.0;
        float floorN = 0.0;
        float clumped = 0.0;
        float clamped = 0.0;
        float n = 0.0;
        float nmax = 0.0;
        for (uint i = t; i < p.cap; i = i + 256u) {
            vec4 pi4 = X.p[i];
            if (pi4.w > 0.5 && pi4.w < 1.5) {
                float rho = FL.f[8u * (i)];
                if (FL.f[8u * (i) + 7u] > nmax) { nmax = FL.f[8u * (i) + 7u]; }
                vec4 vi4 = V.v[i];
                float sp = sqrt(vi4.x * vi4.x + vi4.y * vi4.y + vi4.z * vi4.z);
                sumRho = sumRho + rho;
                if (rho < minRho) { minRho = rho; }
                if (rho > maxRho) { maxRho = rho; }
                if (sp > vmax) { vmax = sp; }
                if (pi4.x > xmax) { xmax = pi4.x; }
                if (pi4.y > ymax) { ymax = pi4.y; }
                if (pi4.y < ymin) { ymin = pi4.y; }
                if (rho >= 0.5 * rho0 && pi4.x > xdense) { xdense = pi4.x; }
                if (pi4.y < p.bmin.y + h) { floorSum = floorSum + rho; floorN = floorN + 1.0; }
                if (rho > 1.1 * rho0) { clumped = clumped + 1.0; }
                clamped = clamped + vi4.w;
                n = n + 1.0;
            }
        }
        uint o = 256u + t * 16u;
        R.r[o] = sumRho;
        R.r[o + 1u] = minRho;
        R.r[o + 2u] = maxRho;
        R.r[o + 3u] = vmax;
        R.r[o + 4u] = xmax;
        R.r[o + 5u] = ymax;
        R.r[o + 6u] = ymin;
        R.r[o + 7u] = xdense;
        R.r[o + 8u] = floorSum;
        R.r[o + 9u] = floorN;
        R.r[o + 10u] = clumped;
        R.r[o + 11u] = clamped;
        R.r[o + 12u] = n;
        R.r[o + 13u] = nmax;
    }
}
)NKSL";

		static uint32 NextPow2(uint32 v) {
			uint32 r = 1;
			while (r < v)
				r <<= 1;
			return r;
		}

		bool NkSPHStoreGPU::CompileKernel(int which, const char *name, const char *body) {
			NkString src(kSPHCommon);
			src.Append(NkForceFieldNkSL());
			src.Append(body);
			NkSLCompiler slc;
			NkSLCompileResult gl = slc.Compile(src, NkSLStage::NK_COMPUTE, NkSLTarget::NK_GLSL_VULKAN);
			if (!gl.success) {
				mFail = "NkSL -> GLSL refuse";
				std::fprintf(stderr, "[NkSPHStoreGPU] NkSL refuse le noyau %s\n", name);
				return false;
			}
			NkShaderConvertResult hl, sp, ms;
			NkSLCompileResult glo;
			NkShaderDesc sd;
			sd.debugName = name;
			const NkGraphicsApi api = mDevice->GetApi();
			if (api == NkGraphicsApi::NK_GFX_API_DX11 || api == NkGraphicsApi::NK_GFX_API_DX12) {
				hl = NkShaderConverter::GlslToHlsl(gl.source, NkSLStage::NK_COMPUTE, 50u, name);
				if (!hl.success) {
					mFail = "GLSL -> HLSL refuse";
					return false;
				}
				sd.AddHLSL(NkShaderStage::NK_COMPUTE, hl.source.CStr(), "main");
			} else if (api == NkGraphicsApi::NK_GFX_API_VULKAN) {
				sp = NkShaderConverter::GlslToSpirv(gl.source, NkSLStage::NK_COMPUTE, name);
				if (!sp.success) {
					mFail = "GLSL -> SPIR-V refuse";
					return false;
				}
				sd.AddSPIRV(NkShaderStage::NK_COMPUTE, sp.binary.Data(), (uint64)sp.binary.Size());
			} else if (api == NkGraphicsApi::NK_GFX_API_METAL) {
				ms = NkShaderConverter::GlslToMsl(gl.source, NkSLStage::NK_COMPUTE, name);
				if (!ms.success) {
					mFail = "GLSL -> MSL refuse";
					return false;
				}
				sd.AddMSL(NkShaderStage::NK_COMPUTE, ms.source.CStr(), "main");
			} else if (api == NkGraphicsApi::NK_GFX_API_OPENGL) {
				glo = slc.Compile(src, NkSLStage::NK_COMPUTE, NkSLTarget::NK_GLSL);
				if (!glo.success) {
					mFail = "NkSL -> GLSL(GL) refuse";
					return false;
				}
				if (const char *dump = std::getenv("NK_SPH_DUMP"); dump && dump[0] == '1') {
					std::fprintf(stderr, "[NkSPHStoreGPU] GLSL genere pour %s :\n", name);
					std::fwrite(glo.source.CStr(), 1, glo.source.Size(), stderr);
					std::fputs("\n[NkSPHStoreGPU] fin du GLSL\n", stderr);
				}
				sd.AddGLSL(NkShaderStage::NK_COMPUTE, glo.source.CStr(), "main");
			} else {
				mFail = "API sans compute";
				std::fprintf(stderr, "[NkSPHStoreGPU] refuse : %s\n", mFail);
				return false;
			}
			Kernel &k = mKernels[which];
			k.shader = mDevice->CreateShader(sd);
			if (!k.shader.IsValid()) {
				mFail = "CreateShader refuse (le journal du device dit lequel)";
				std::fprintf(stderr, "[NkSPHStoreGPU] CreateShader refuse le noyau %s\n", name);
				return false;
			}
			NkComputePipelineDesc cpd;
			cpd.shader = k.shader;
			cpd.debugName = name;
			cpd.descriptorSetLayouts.PushBack(mLayout);
			k.pipe = mDevice->CreateComputePipeline(cpd);
			if (!k.pipe.IsValid()) {
				mFail = "CreateComputePipeline refuse";
				std::fprintf(stderr, "[NkSPHStoreGPU] pipeline refuse pour le noyau %s\n", name);
				return false;
			}
			return true;
		}

		bool NkSPHStoreGPU::Init(NkIDevice *device, const NkEmitterDesc &desc) {
			mDevice = device;
			mCapacity = desc.maxParticles;
			mFail = "";
			if (!device || !mOwner) {
				mFail = "pas de device ou pas de solveur";
				return false;
			}
			if (!device->GetCaps().computeShaders) {
				mFail = "pas de compute sur ce device";
				std::fprintf(stderr, "[NkSPHStoreGPU] refuse : %s\n", mFail);
				return false;
			}
			if (mCapacity == 0) {
				mFail = "capacite nulle";
				return false;
			}
			const NkSPHParams &pr = mOwner->params;
			// Fantômes : les mêmes que le CPU (une seule recette, NkSPHSolver::BuildBoundaryPositions).
			NkVector<NkVec3f> ghosts;
			NkSPHSolver::BuildBoundaryPositions(pr, ghosts);
			mBoundary = (uint32)ghosts.Size();
			mFpad = NextPow2(mCapacity);
			mGpad = NextPow2(mBoundary > 0 ? mBoundary : 1u);
			// Grille : la même que BuildNeighbors (origine, taille, cellules).
			const float32 h = pr.h, d = h * 0.5f, inv = 1.f / h;
			const NkVec3f gmin = {pr.boundsMin.x - 3.f * d, pr.boundsMin.y - 3.f * d, pr.boundsMin.z - 3.f * d};
			const uint32 nx = (uint32)((pr.boundsMax.x - gmin.x + 3.f * d) * inv) + 1u;
			const uint32 ny = (uint32)((pr.boundsMax.y - gmin.y + 3.f * d) * inv) + 1u;
			const uint32 nz = (uint32)((pr.boundsMax.z - gmin.z + 3.f * d) * inv) + 1u;
			mNumCells = nx * ny * nz;
			mParams = Params{};
			mParams.hm = {h, pr.Mass(), pr.restDensity, 1.f / 60.f};
			mParams.grav = {pr.gravity.x, pr.gravity.y, pr.gravity.z, 60.f};
			mParams.gmin = {gmin.x, gmin.y, gmin.z, inv};
			mParams.bmin = {pr.boundsMin.x, pr.boundsMin.y, pr.boundsMin.z, pr.restitution};
			mParams.bmax = {pr.boundsMax.x, pr.boundsMax.y, pr.boundsMax.z, pr.maxSpeed};
			mParams.visc = {pr.viscosity, pr.wallFriction, pr.kinematicViscosity, pr.warmStartScale};
			mParams.nx = nx;
			mParams.ny = ny;
			mParams.nz = nz;
			mParams.numCells = mNumCells;
			mParams.cap = mCapacity;
			mParams.nb = mBoundary;
			mParams.fpad = mFpad;
			mParams.gpad = mGpad;
			mParams.surfaceMode = pr.surfaceMode;
			mParams.size = desc.sizeStart;
			mParams.color = ((uint32)(desc.colorStart.w * 255) << 24) | ((uint32)(desc.colorStart.z * 255) << 16) |
							((uint32)(desc.colorStart.y * 255) << 8) | (uint32)(desc.colorStart.x * 255);
			// L'horloge des vies.
			mLife.Resize(mCapacity);
			mAlive.Resize(mCapacity);
			for (uint32 i = 0; i < mCapacity; ++i) {
				mLife[i] = 0.f;
				mAlive[i] = 0;
			}
			mFree.Clear();
			mFree.Reserve(mCapacity);
			for (uint32 i = mCapacity; i > 0; --i)
				mFree.PushBack(i - 1);
			mPending.Clear();
			mAliveCount = 0;
			mLastVmax = 0.f;
			// Tampons. X part à zéro (mortes) puis reçoit les fantômes (w = 2) à [cap, cap + nb).
			const uint32 M = mCapacity + mBoundary;
			{
				NkVector<NkVec4f> x0;
				x0.Resize(M);
				for (uint32 i = 0; i < mCapacity; ++i)
					x0[i] = {0.f, 0.f, 0.f, 0.f};
				for (uint32 i = 0; i < mBoundary; ++i)
					x0[mCapacity + i] = {ghosts[i].x, ghosts[i].y, ghosts[i].z, 2.f};
				NkBufferDesc bx = NkBufferDesc::Storage((uint64)M * 16u);
				bx.initialData = x0.Data();
				bx.debugName = "sph_pos";
				mX = device->CreateBuffer(bx);
				NkVector<uint8> zeros;
				uint32 zbytes = M * 32u;
				if (mFpad * 8u > zbytes) zbytes = mFpad * 8u;
				if (mGpad * 8u > zbytes) zbytes = mGpad * 8u;
				if (mNumCells * 8u > zbytes) zbytes = mNumCells * 8u;
				zeros.Resize(zbytes);
				for (uint32 i = 0; i < zbytes; ++i)
					zeros[i] = 0;
				auto mk = [&](NkBufferHandle &out, uint64 bytes, const char *name) {
					NkBufferDesc bd = NkBufferDesc::Storage(bytes);
					if (bytes <= zbytes)
						bd.initialData = zeros.Data();
					bd.debugName = name;
					out = device->CreateBuffer(bd);
				};
				mk(mV, (uint64)M * 16u, "sph_vel");
				mk(mKV, (uint64)mFpad * 8u, "sph_keyidx");
				mk(mGKV, (uint64)mGpad * 8u, "sph_gkeyidx");
				mk(mFSE, (uint64)mNumCells * 8u, "sph_fcells");
				mk(mGSE, (uint64)mNumCells * 8u, "sph_gcells");
				mk(mFL, (uint64)M * 32u, "sph_fields");
				mk(mT, (uint64)mCapacity * 16u, "sph_tmp");
				mk(mNB, (uint64)mCapacity * 64u * 4u, "sph_neigh");
				mk(mRed, (uint64)(256u + 256u * 16u) * 4u, "sph_red");
				NkBufferDesc id = NkBufferDesc::Storage((uint64)mCapacity * (uint64)sizeof(NkParticleInstance));
				id.bindFlags = id.bindFlags | NkBindFlags::NK_VERTEX_BUFFER;
				if (mCapacity * 24u <= zbytes)
					id.initialData = zeros.Data();
				id.debugName = "sph_instances";
				mInstances = device->CreateBuffer(id);
				NkBufferDesc bb = NkBufferDesc::Storage((uint64)mCapacity * (uint64)sizeof(GpuBirth));
				bb.debugName = "sph_births";
				mBirths = device->CreateBuffer(bb);
				mUbo = device->CreateBuffer(NkBufferDesc::Uniform(sizeof(Params)));
				mSortUbo = device->CreateBuffer(NkBufferDesc::Uniform(sizeof(SortParams)));
				mFieldUbo = device->CreateBuffer(NkBufferDesc::Uniform(48));
				{
					NkVec4f fw[3] = {{0.f, 0.f, 0.f, 0.f}, {0.f, 0.f, 0.f, 0.f}, {0.f, 0.f, 0.f, 0.f}};
					device->WriteBuffer(mFieldUbo, fw, 48);
				}
			}
			NkBufferHandle *all[] = {&mX, &mV, &mKV, &mGKV, &mFSE, &mGSE, &mFL, &mT, &mRed, &mInstances, &mBirths, &mUbo, &mSortUbo, &mNB, &mFieldUbo};
			for (NkBufferHandle *b : all)
				if (!b->IsValid()) {
					mFail = "un tampon de stockage n'a pas pu etre cree";
					return false;
				}
			mDevice->WriteBuffer(mUbo, &mParams, sizeof(Params));
			// Layout : 12 tampons de stockage + 2 uniformes (NVIDIA : 16 blocs de stockage par etage, mesure le 05/09).
			NkDescriptorSetLayoutDesc ld;
			for (uint32 b = 0; b < 12u; ++b)
				ld.Add(b, NkDescriptorType::NK_STORAGE_BUFFER, NkShaderStage::NK_COMPUTE);
			ld.Add(12, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_COMPUTE);
			ld.Add(13, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_COMPUTE);
			ld.Add(14, NkDescriptorType::NK_STORAGE_BUFFER, NkShaderStage::NK_COMPUTE);
			ld.Add(15, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_COMPUTE);
			mLayout = mDevice->CreateDescriptorSetLayout(ld);
			static const char *names[K_COUNT] = {"sph_birth", "sph_key", "sph_sort", "sph_clear", "sph_cell", "sph_dens", "sph_kappa",
												 "sph_correct", "sph_warm", "sph_storewarm", "sph_nonp", "sph_apply", "sph_reduce",
												 "sph_integ", "sph_stats", "sph_neigh"};
			static const char *bodies[K_COUNT] = {kBirthBody, kKeyBody, kSortBody, kClearBody, kCellBody, kDensBody, kKappaBody,
												  kCorrectBody, kWarmBody, kStoreWarmBody, kNonPBody, kApplyBody, kReduceBody,
												  kIntegBody, kStatsBody, kNeighBody};
			for (int k = 0; k < K_COUNT; ++k)
				if (!CompileKernel(k, names[k], bodies[k]))
					return false;
			mSet = mDevice->AllocateDescriptorSet(mLayout);
			if (!mSet.IsValid()) {
				mFail = "AllocateDescriptorSet refuse";
				return false;
			}
			NkBufferHandle *ssbo[12] = {&mX, &mV, &mKV, &mGKV, &mFSE, &mGSE, &mFL, &mT, &mRed, &mInstances, &mInstances, &mBirths};
			for (uint32 b = 0; b < 12u; ++b) {
				NkDescriptorWrite w{};
				w.set = mSet;
				w.binding = b;
				w.type = NkDescriptorType::NK_STORAGE_BUFFER;
				w.buffer = *ssbo[b];
				mDevice->UpdateDescriptorSets(&w, 1);
			}
			mDevice->BindUniformBuffer(mSet, 12, mUbo);
			mDevice->BindUniformBuffer(mSet, 13, mSortUbo);
			mDevice->BindUniformBuffer(mSet, 15, mFieldUbo);
			{
				NkDescriptorWrite w{};
				w.set = mSet;
				w.binding = 14;
				w.type = NkDescriptorType::NK_STORAGE_BUFFER;
				w.buffer = mNB;
				mDevice->UpdateDescriptorSets(&w, 1);
			}
			mCmd = mDevice->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
			if (!mCmd) {
				mFail = "CreateCommandBuffer(compute) refuse";
				return false;
			}
			// Les fantômes : triés UNE fois dans leur grille.
			mCmd->Reset();
			mCmd->Begin();
			mRecording = true;
			if (mBoundary > 0)
				SortGrid(1u, mGpad);
			Flush();
			mCmd->End();
			mRecording = false;
			mOwner->mStats = NkSPHStats{};
			mOwner->mStats.boundary = mBoundary;
			std::fprintf(stderr,
						 "[NkSPHStoreGPU] pret : %u emplacements, %u fantomes, grille %u x %u x %u (%u cellules), tri sur %u / %u\n",
						 mCapacity, mBoundary, nx, ny, nz, mNumCells, mFpad, mGpad);
			return true;
		}

		void NkSPHStoreGPU::Shutdown(NkIDevice *device) {
			if (device) {
				if (mCmd)
					device->DestroyCommandBuffer(mCmd);
				if (mSet.IsValid())
					device->FreeDescriptorSet(mSet);
				NkBufferHandle *all[] = {&mX, &mV, &mKV, &mGKV, &mFSE, &mGSE, &mFL, &mT, &mRed, &mInstances, &mBirths, &mUbo, &mSortUbo, &mNB, &mFieldUbo};
				for (NkBufferHandle *b : all)
					if (b->IsValid()) {
						device->DestroyBuffer(*b);
						*b = {};
					}
			}
			mCmd = nullptr;
			mSet = {};
			mDevice = nullptr;
		}

		void NkSPHStoreGPU::Spawn(const NkParticleBirth *births, uint32 n) {
			for (uint32 k = 0; k < n; ++k) {
				if (mFree.Empty())
					return;
				const uint32 i = mFree.Back();
				mFree.PopBack();
				mAlive[i] = 1;
				mLife[i] = births[k].life;
				GpuBirth gb;
				gb.posLife = {births[k].pos.x, births[k].pos.y, births[k].pos.z, births[k].life};
				gb.velRot = {births[k].vel.x, births[k].vel.y, births[k].vel.z, 0.f};
				gb.slotFlag = {(float32)i, 0.f, 1.f, 0.f};
				mPending.PushBack(gb);
			}
		}

		void NkSPHStoreGPU::Dispatch(int which, uint32 count) {
			if (count == 0)
				return;
			mCmd->BindComputePipeline(mKernels[which].pipe);
			mCmd->BindDescriptorSet(mSet, 0);
			mCmd->Dispatch((count + 255u) / 256u, 1, 1);
			mCmd->UAVBarrier(mX); // sur GL : glMemoryBarrier(SHADER_STORAGE | UNIFORM), global
		}

		// Tri bitonique de n = 2^k cles (which 0 : fluide, 1 : fantomes), puis debut/fin de cellule.
		void NkSPHStoreGPU::SortGrid(uint32 which, uint32 n) {
			SortParams sp;
			sp.which = which;
			sp.n = n;
			mCmd->UpdateBuffer(mSortUbo, 0, sizeof(SortParams), &sp);
			Dispatch(K_KEY, n);
			for (uint32 k = 2; k <= n; k <<= 1) {
				for (uint32 j = k >> 1; j > 0; j >>= 1) {
					sp.j = j;
					sp.k = k;
					mCmd->UpdateBuffer(mSortUbo, 0, sizeof(SortParams), &sp);
					Dispatch(K_SORT, n);
				}
			}
			Dispatch(K_CLEAR, mNumCells);
			Dispatch(K_CELL, n);
		}

		bool NkSPHStoreGPU::Flush() {
			if (!mRecording)
				return false;
			mCmd->End();
			mDevice->Submit(&mCmd, 1);
			mCmd->Reset();
			mCmd->Begin();
			++mSyncs;
			return true;
		}

		float32 NkSPHStoreGPU::ReadResidual(uint32 n) {
			Dispatch(K_REDUCE, 256u);
			Flush();
			mScratch.Resize(256);
			mDevice->ReadBuffer(mRed, mScratch.Data(), 256u * 4u, 0);
			float32 s = 0.f;
			for (uint32 t = 0; t < 256u; ++t)
				s += mScratch[t];
			return n > 0 ? s / (float32)n : 0.f;
		}

		void NkSPHStoreGPU::StepOnce(float32 dt) {
			const NkSPHParams &pr = mOwner->params;
			const uint32 n = mAliveCount;
			mParams.hm.w = dt;
			mParams.grav.w = 1.f / dt;
			mParams.mode = 0;
			mParams.accum = 0;
			mCmd->UpdateBuffer(mUbo, 0, sizeof(Params), &mParams);
			// 1) grille du fluide
			SortGrid(0u, mFpad);
			// 2) listes de voisines (une fois par sous-pas), densite, alpha
			Dispatch(K_NEIGH, mCapacity);
			Dispatch(K_DENS, mCapacity);
			// 3) divergence nulle
			uint32 it = 0;
			float32 err = 0.f;
			if (mOwner->pressureEnabled) {
				// Le residu est RELU une iteration sur deux (une synchronisation chacune) : aux iterations
				// impaires ici, paires pour la densite -- au plus une iteration de plus qu'en CPU, dit.
				for (;;) {
					Dispatch(K_KAPPA, mCapacity);
					if (it >= pr.maxIterDivergence)
						break;
					if (it >= 1 && (it & 1u) == 1u) {
						err = ReadResidual(n);
						if (err < pr.tolDivergence)
							break;
					}
					Dispatch(K_CORRECT, mCapacity);
					++it;
				}
				if (it >= pr.maxIterDivergence)
					++mCaps;
			}
			mIterV += it;
			mResV += err;
			// 4) forces non-pression puis gravite
			Dispatch(K_NONP, mCapacity);
			Dispatch(K_APPLY, mCapacity);
			// 5) densite constante (demarrage a chaud d'abord)
			it = 0;
			err = 0.f;
			if (mOwner->pressureEnabled) {
				mParams.mode = 1;
				mParams.accum = pr.warmStart ? 1u : 0u;
				mCmd->UpdateBuffer(mUbo, 0, sizeof(Params), &mParams);
				if (pr.warmStart) {
					Dispatch(K_WARM, mCapacity);
					mParams.accum = 0;
					mCmd->UpdateBuffer(mUbo, 0, sizeof(Params), &mParams);
					Dispatch(K_CORRECT, mCapacity);
					mParams.accum = 1;
					mCmd->UpdateBuffer(mUbo, 0, sizeof(Params), &mParams);
					++mWarm;
				}
				for (;;) {
					Dispatch(K_KAPPA, mCapacity);
					if (it >= pr.maxIterDensity)
						break;
					if (it >= 2 && (it & 1u) == 0u) {
						err = ReadResidual(n);
						if (err < pr.tolDensity)
							break;
					}
					Dispatch(K_CORRECT, mCapacity);
					++it;
				}
				if (pr.warmStart)
					Dispatch(K_STOREWARM, mCapacity);
				if (it >= pr.maxIterDensity)
					++mCaps;
				mParams.mode = 0;
				mParams.accum = 0;
				mCmd->UpdateBuffer(mUbo, 0, sizeof(Params), &mParams);
			}
			mIterD += it;
			mResD += err;
			// 6) integration + instances
			Dispatch(K_INTEG, mCapacity);
		}

		void NkSPHStoreGPU::ReadStats(uint32 sub, uint32 subVisc, float32 ms) {
			Dispatch(K_STATS, 256u);
			Flush();
			mScratch.Resize(256u * 16u);
			mDevice->ReadBuffer(mRed, mScratch.Data(), 256u * 16u * 4u, 256u * 4u);
			float32 sumRho = 0.f, minRho = 1e30f, maxRho = 0.f, vmax = 0.f, xmax = -1e30f, ymax = -1e30f, ymin = 1e30f,
					xdense = -1e30f, floorSum = 0.f, floorN = 0.f, clumped = 0.f, clamped = 0.f, cnt = 0.f, nmax = 0.f;
			for (uint32 t = 0; t < 256u; ++t) {
				const float32 *r = &mScratch[t * 16u];
				if (r[12] <= 0.f)
					continue;
				sumRho += r[0];
				if (r[1] < minRho) minRho = r[1];
				if (r[2] > maxRho) maxRho = r[2];
				if (r[3] > vmax) vmax = r[3];
				if (r[4] > xmax) xmax = r[4];
				if (r[5] > ymax) ymax = r[5];
				if (r[6] < ymin) ymin = r[6];
				if (r[7] > xdense) xdense = r[7];
				floorSum += r[8];
				floorN += r[9];
				clumped += r[10];
				clamped += r[11];
				cnt += r[12];
				if (r[13] > nmax) nmax = r[13];
			}
			if (nmax > 64.f && !mNeighOverflowSaid) {
				mNeighOverflowSaid = true;
				std::fprintf(stderr, "[NkSPHStoreGPU] ATTENTION : %g voisines pour une particule, la liste en garde 64 -- densite fausse la\n", nmax);
			}
			mNeighMax = nmax;
			NkSPHStats &s = mOwner->mStats;
			s = NkSPHStats{};
			s.alive = (uint32)cnt;
			s.subSteps = sub;
			s.subStepsViscous = subVisc;
			s.speedClamped = (uint32)clamped;
			s.densityMean = cnt > 0.f ? sumRho / cnt : 0.f;
			s.densityMin = cnt > 0.f ? minRho : 0.f;
			s.densityMax = maxRho;
			s.maxSpeed = vmax;
			s.maxX = xmax;
			s.maxY = ymax;
			s.minY = ymin;
			s.frontDenseX = xdense > -1e29f ? xdense : xmax;
			s.boundary = mBoundary;
			s.densityFloorMean = floorN > 0.f ? floorSum / floorN : 0.f;
			s.iterDensity = sub ? (float32)mIterD / (float32)sub : 0.f;
			s.iterDivergence = sub ? (float32)mIterV / (float32)sub : 0.f;
			s.residualDensity = sub ? mResD / (float32)sub : 0.f;
			s.residualDivergence = sub ? mResV / (float32)sub : 0.f;
			s.iterCapHits = mCaps;
			s.warmStarts = mWarm;
			s.clumped = (uint32)clumped;
			s.ms = ms;
			mLastVmax = vmax;
			mOwner->mLastVmax = vmax;
		}

		void NkSPHStoreGPU::Step(NkICommandBuffer *cmd, const NkEmitterDesc &desc, float32 dt,
								 NkParticleStepStats &stats) {
			(void)cmd;
			const int64 t0 = ::nkentseu::NkChrono::Now().nanoseconds;
			const NkSPHParams &pr = mOwner->params;
			mTime += dt;
			{
				NkVec4f fw[3];
				NkPackForceField(desc.field, mTime, fw[0], fw[1], fw[2]);
				mDevice->WriteBuffer(mFieldUbo, fw, 48);
			}
			// horloge des vies : les mortes rendent leur emplacement et sont dites au GPU
			for (uint32 i = 0; i < mCapacity; ++i) {
				if (!mAlive[i])
					continue;
				mLife[i] -= dt;
				if (mLife[i] <= 0.f) {
					mAlive[i] = 0;
					mFree.PushBack(i);
					GpuBirth gb;
					gb.posLife = {0.f, 0.f, 0.f, 0.f};
					gb.velRot = {0.f, 0.f, 0.f, 0.f};
					gb.slotFlag = {(float32)i, 0.f, 0.f, 0.f};
					mPending.PushBack(gb);
				}
			}
			mAliveCount = 0;
			for (uint32 i = 0; i < mCapacity; ++i)
				mAliveCount += mAlive[i] ? 1u : 0u;
			// sous-pas : la meme CFL que NkSPHSolver::Apply (vitesse mesuree au pas precedent, CFL visqueuse)
			const float32 h = pr.h > 1e-5f ? pr.h : 1e-5f;
			const float32 vref = mLastVmax > 1.f ? mLastVmax : 1.f;
			float32 dtMax = pr.cfl * h / vref;
			uint32 subVisc = 0;
			if (pr.kinematicViscosity > 0.f) {
				const float32 hs = 0.5f * h;
				const float32 dtVisc = 0.125f * hs * hs / pr.kinematicViscosity;
				if (dtVisc < dtMax) {
					dtMax = dtVisc;
					subVisc = (uint32)ceilf(dt / dtVisc);
				}
			}
			uint32 sub = (uint32)ceilf(dt / dtMax);
			if (sub < 1u)
				sub = 1u;
			if (sub > pr.maxSubSteps)
				sub = pr.maxSubSteps;
			const float32 dts = dt / (float32)sub;
			mIterD = mIterV = mCaps = mWarm = 0;
			mResD = mResV = 0.f;
			mSyncs = 0;
			// parametres qui peuvent changer par la sonde entre deux images (boutons)
			mParams.surfaceMode = pr.surfaceMode;
			mParams.visc = {pr.viscosity, pr.wallFriction, pr.kinematicViscosity, pr.warmStartScale};
			mParams.bmax.w = pr.maxSpeed;
			mCmd->Reset();
			mCmd->Begin();
			mRecording = true;
			const uint32 nbirth = (uint32)mPending.Size();
			if (nbirth > 0) {
				mDevice->WriteBuffer(mBirths, mPending.Data(), (uint64)nbirth * sizeof(GpuBirth));
				mParams.nbirths = nbirth;
				mCmd->UpdateBuffer(mUbo, 0, sizeof(Params), &mParams);
				Dispatch(K_BIRTH, nbirth);
				mPending.Clear();
				stats.uploadBytes += nbirth * (uint32)sizeof(GpuBirth);
			}
			for (uint32 s = 0; s < sub; ++s)
				StepOnce(dts);
			{
				NkBufferBarrier b{mInstances, NkResourceState::NK_UNORDERED_ACCESS, NkResourceState::NK_VERTEX_BUFFER};
				mCmd->Barrier(&b, 1, nullptr, 0);
			}
			const float32 ms = (float32)((::nkentseu::NkChrono::Now().nanoseconds - t0) / 1.0e6);
			ReadStats(sub, subVisc, ms);
			mCmd->End();
			mRecording = false;
			const float32 msAll = (float32)((::nkentseu::NkChrono::Now().nanoseconds - t0) / 1.0e6);
			mOwner->mStats.ms = msAll; // temps CPU de l'image, relectures (synchronisations GPU) comprises
			mOwner->mStats.syncs = mSyncs;
			stats.simMs += msAll;
			stats.alive += mAliveCount;
		}

	} // namespace renderer
} // namespace nkentseu
