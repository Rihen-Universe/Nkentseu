// =============================================================================
// tests/test_collision.cpp — Self-test NKCollision (standalone, ZÉRO stdlib).
// Journalisation via NKLogger (pas de printf/<cstdio>). Couvre narrowphase
// 2D/3D, world (broadphase+narrow), raycast, filtrage de layers.
// =============================================================================
#include "NKCollision/NKCollision.h"
#include "NKLogger/NkLog.h"

using namespace nkentseu;
using namespace nkentseu::collision;

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { ++g_pass; } else { ++g_fail; \
    logger.Errorf("  [FAIL] %s\n", msg); } } while(0)

static bool Near(float32 a, float32 b, float32 e = 1e-3f) {
    float32 d = a - b; return (d < 0 ? -d : d) <= e;
}

int main() {
    logger.Info("=== NKCollision self-test ===\n");

    // ── 3D narrowphase ───────────────────────────────────────────────────────
    {
        NkManifold3D m;
        CHECK(NkSphereSphere({0,0,0}, 1.f, {1.5f,0,0}, 1.f, m), "sphere-sphere overlap");
        CHECK(m.count == 1 && Near(m.points[0].depth, 0.5f), "sphere-sphere depth=0.5");
        CHECK(Near(m.normal.x, 1.f), "sphere-sphere normal +X");
        NkManifold3D m2;
        CHECK(!NkSphereSphere({0,0,0}, 1.f, {3,0,0}, 1.f, m2), "sphere-sphere separated");
    }
    {
        NkManifold3D m;
        CHECK(NkSphereBox({0,2.4f,0}, 1.f, {0,0,0}, {1,2,1}, m), "sphere-box overlap");
        CHECK(Near(m.points[0].depth, 0.6f), "sphere-box depth");
        NkManifold3D m2;
        CHECK(!NkSphereBox({0,5,0}, 1.f, {0,0,0}, {1,2,1}, m2), "sphere-box separated");
    }
    {
        NkManifold3D m;
        CHECK(NkBoxBox3D({0,0,0}, {1,1,1}, {1.5f,0,0}, {1,1,1}, m), "box-box overlap");
        CHECK(Near(m.points[0].depth, 0.5f) && Near(m.normal.x, 1.f), "box-box axis X depth 0.5");
        NkManifold3D m2;
        CHECK(!NkBoxBox3D({0,0,0}, {1,1,1}, {3,0,0}, {1,1,1}, m2), "box-box separated");
    }
    {
        NkManifold3D m;  // capsules verticales proches
        CHECK(NkCapsuleCapsule({0,-1,0}, {0,1,0}, 0.5f, {0.8f,-1,0}, {0.8f,1,0}, 0.5f, m), "capsule-capsule overlap");
        NkManifold3D m2;
        CHECK(!NkCapsuleCapsule({0,-1,0}, {0,1,0}, 0.5f, {3,-1,0}, {3,1,0}, 0.5f, m2), "capsule-capsule separated");
    }

    // ── 2D narrowphase ───────────────────────────────────────────────────────
    {
        NkManifold2D m;
        CHECK(NkCircleCircle({0,0}, 1.f, {1.5f,0}, 1.f, m), "circle-circle overlap");
        CHECK(Near(m.points[0].depth, 0.5f), "circle-circle depth");
        NkManifold2D m2;
        CHECK(NkBoxBox2D({0,0}, {1,1}, {1.5f,0}, {1,1}, m2), "box2D-box2D overlap");
        CHECK(!NkBoxBox2D({0,0}, {1,1}, {3,0}, {1,1}, m), "box2D-box2D separated");
    }

    // ── 2D OBB (boîtes orientées, SAT) ───────────────────────────────────────
    {
        const float32 PI4 = 0.785398f;  // 45°
        NkManifold2D m;
        // Deux boîtes unité, l'une tournée à 45°, centres à 1.3 : se touchent en OBB.
        CHECK(NkOBB2DvsOBB2D({0,0}, {1,1}, 0.f, {1.3f,0}, {1,1}, PI4, m), "OBB-OBB overlap (45deg)");
        NkManifold2D m2;
        // Séparées nettement.
        CHECK(!NkOBB2DvsOBB2D({0,0}, {1,1}, 0.f, {5,0}, {1,1}, PI4, m2), "OBB-OBB separated");
        // OBB tournée 45° : son coin pointe vers +X (portée ~sqrt(2)). À x=2.2 un cercle
        // r=0.3 touche le coin, alors qu'une AABB (portée 1) ne toucherait pas.
        NkManifold2D mc;
        CHECK(NkOBB2DvsCircle({0,0}, {1,1}, PI4, {1.6f,0}, 0.3f, mc), "OBB-cercle touche le coin (45deg)");
        NkManifold2D mc2;
        CHECK(!NkOBB2DvsCircle({0,0}, {1,1}, 0.f, {1.6f,0}, 0.3f, mc2), "AABB-cercle (0deg) ne touche pas a 1.6");
    }

    // ── Raycast ──────────────────────────────────────────────────────────────
    {
        NkRay3D ray; ray.origin = {-5,0,0}; ray.dir = {1,0,0}; ray.maxT = 100.f;
        NkRayHit3D h;
        CHECK(NkRaySphere(ray, {0,0,0}, 1.f, h) && Near(h.t, 4.f), "ray-sphere t=4");
        NkRayHit3D hb;
        CHECK(NkRayAABB3D(ray, {-1,-1,-1}, {1,1,1}, hb) && Near(hb.t, 4.f), "ray-aabb t=4");
        NkRay3D miss; miss.origin={-5,5,0}; miss.dir={1,0,0}; miss.maxT=100.f; NkRayHit3D hm;
        CHECK(!NkRaySphere(miss, {0,0,0}, 1.f, hm), "ray-sphere miss");
    }

    // ── World : broadphase + narrowphase + filtrage layers ───────────────────
    {
        NkWorld world;
        world.AddBody(NkShape::Sphere({0,0,0}, 1.f));
        world.AddBody(NkShape::Sphere({1.5f,0,0}, 1.f));
        world.AddBody(NkShape::Sphere({100,0,0}, 1.f));   // loin
        world.Step();
        CHECK(world.Pairs().Size() == 1, "world: 1 paire (a-b, c isole)");

        NkWorld w2;
        w2.AddBody(NkShape::Sphere({0,0,0}, 1.f), 0x1, 0x1);     // layer 1 seulement
        w2.AddBody(NkShape::Sphere({1.5f,0,0}, 1.f), 0x2, 0x2);  // layer 2 seulement
        w2.Step();
        CHECK(w2.Pairs().Size() == 0, "world: filtrage layers (pas de paire)");

        NkRay3D ray; ray.origin={-5,0,0}; ray.dir={1,0,0}; ray.maxT=100.f;
        NkRayHit3D h;
        CHECK(world.Raycast3D(ray, h) && Near(h.t, 4.f), "world raycast hit sphere");
    }

    // ── GJK/EPA 3D : cohérence vs analytique + nouveaux types convexes ────────
    {
        // sphere-sphere via GJK/EPA == analytique (depth 0.5, normale +X)
        NkShape sa = NkShape::Sphere({0,0,0}, 1.f), sb = NkShape::Sphere({1.5f,0,0}, 1.f);
        NkManifold3D m;
        CHECK(NkGJKEPA3D(sa, sb, m), "gjk3d sphere-sphere overlap");
        CHECK(Near(m.points[0].depth, 0.5f, 2e-2f) && m.normal.x > 0.9f, "gjk3d sphere-sphere depth/normal ~ analytique");
        NkShape sc = NkShape::Sphere({3,0,0}, 1.f); NkManifold3D m2;
        CHECK(!NkGJKEPA3D(sa, sc, m2), "gjk3d sphere-sphere separated");

        // box-box via GJK/EPA == analytique
        NkShape ba = NkShape::Box3D({0,0,0}, {1,1,1}), bb = NkShape::Box3D({1.5f,0,0}, {1,1,1});
        NkManifold3D mb;
        CHECK(NkGJKEPA3D(ba, bb, mb) && Near(mb.points[0].depth, 0.5f, 3e-2f) && mb.normal.x > 0.9f, "gjk3d box-box depth/normal");

        // box-capsule (paire NON gérée analytiquement -> fallback GJK)
        NkShape box = NkShape::Box3D({0,0,0}, {1,1,1});
        NkShape cap = NkShape::Capsule3D({1.2f,-0.5f,0}, {1.2f,0.5f,0}, 0.5f);  // surface x=0.7 vs face x=1 -> depth 0.3
        NkManifold3D mc;
        CHECK(NkGJKEPA3D(box, cap, mc) && mc.normal.x > 0.8f, "gjk3d box-capsule overlap");
        CHECK(Near(mc.points[0].depth, 0.3f, 6e-2f), "gjk3d box-capsule depth ~0.3");

        // cylindre-sphère
        NkShape cyl = NkShape::Cylinder3D({0,0,0}, {0,1,0}, 1.f, 1.f);  // axe Y, rayon 1
        NkShape cs  = NkShape::Sphere({1.5f,0,0}, 1.f);                 // côté x=1 vs surface x=0.5 -> depth 0.5
        NkManifold3D mcy;
        CHECK(NkGJKEPA3D(cyl, cs, mcy) && mcy.normal.x > 0.8f, "gjk3d cylinder-sphere overlap");
        CHECK(Near(mcy.points[0].depth, 0.5f, 6e-2f), "gjk3d cylinder-sphere depth ~0.5");
        NkShape csf = NkShape::Sphere({3,0,0}, 1.f); NkManifold3D mcy2;
        CHECK(!NkGJKEPA3D(cyl, csf, mcy2), "gjk3d cylinder-sphere separated");

        // cône-sphère (booléens)
        NkShape cone = NkShape::Cone3D({0,0,0}, {0,1,0}, 2.f, 1.f);
        NkShape cos1 = NkShape::Sphere({1.1f,0,0}, 0.5f); NkManifold3D mco;
        CHECK(NkGJKEPA3D(cone, cos1, mco), "gjk3d cone-sphere overlap");
        NkShape cosf = NkShape::Sphere({3,0,0}, 0.5f); NkManifold3D mco2;
        CHECK(!NkGJKEPA3D(cone, cosf, mco2), "gjk3d cone-sphere separated");

        // convexe (tétraèdre) - sphère
        static const NkVec3f tet[4] = { {0,0,0}, {2,0,0}, {0,2,0}, {0,0,2} };
        NkShape cv = NkShape::Convex3D(tet, 4);
        NkShape cvs = NkShape::Sphere({-0.3f,0.3f,0.3f}, 0.5f); NkManifold3D mcv;
        CHECK(NkGJKEPA3D(cv, cvs, mcv), "gjk3d convex(tetra)-sphere overlap");
        NkShape cvsf = NkShape::Sphere({5,5,5}, 0.5f); NkManifold3D mcv2;
        CHECK(!NkGJKEPA3D(cv, cvsf, mcv2), "gjk3d convex-sphere separated");

        // triangle 3D - sphère
        static const NkVec3f tri[3] = { {-1,0,-1}, {1,0,-1}, {0,0,1} };
        NkShape t3 = NkShape::Triangle3D(tri);
        NkShape t3s = NkShape::Sphere({0,0.3f,0}, 0.5f); NkManifold3D mt3;
        CHECK(NkGJKEPA3D(t3, t3s, mt3), "gjk3d triangle-sphere overlap");
    }

    // ── Plan / half-space (analytique) via le world ──────────────────────────
    {
        NkWorld pw;
        pw.AddBody(NkShape::Plane3D({0,0,0}, {0,1,0}));     // sol y=0, solide en dessous
        pw.AddBody(NkShape::Sphere({0,0.5f,0}, 1.f));        // s'enfonce de 0.5
        pw.Step();
        CHECK(pw.Pairs().Size() == 1, "world plane-sphere : 1 paire");
        if (pw.Pairs().Size() == 1) {
            const NkManifold3D& pm = pw.Pairs()[0].manifold;
            CHECK(pm.normal.y > 0.8f && Near(pm.points[0].depth, 0.5f, 6e-2f), "world plane-sphere normale +Y depth 0.5");
        }
        NkWorld pw2;
        pw2.AddBody(NkShape::Plane3D({0,0,0}, {0,1,0}));
        pw2.AddBody(NkShape::Sphere({0,5,0}, 1.f));          // au-dessus, pas de contact
        pw2.Step();
        CHECK(pw2.Pairs().Size() == 0, "world plane-sphere separes");
    }

    // ── GJK/EPA 2D : polygone, triangle, fallback world ──────────────────────
    {
        static const NkVec3f sq1[4] = { {-1,-1,0}, {1,-1,0}, {1,1,0}, {-1,1,0} };
        static const NkVec3f sq2[4] = { {0.5f,-1,0}, {2.5f,-1,0}, {2.5f,1,0}, {0.5f,1,0} };
        NkShape p1 = NkShape::Polygon2D(sq1, 4), p2 = NkShape::Polygon2D(sq2, 4);
        NkManifold2D m2d;
        CHECK(NkGJKEPA2D(p1, p2, m2d) && Near(m2d.points[0].depth, 0.5f, 6e-2f) && m2d.normal.x > 0.8f, "gjk2d polygone-polygone depth 0.5");
        static const NkVec3f sq3[4] = { {5,-1,0}, {7,-1,0}, {7,1,0}, {5,1,0} };
        NkShape p3 = NkShape::Polygon2D(sq3, 4); NkManifold2D m2s;
        CHECK(!NkGJKEPA2D(p1, p3, m2s), "gjk2d polygone-polygone separes");

        static const NkVec3f tr2[3] = { {0,0,0}, {2,0,0}, {0,2,0} };
        NkShape tt = NkShape::Triangle2D(tr2);
        NkShape circ = NkShape::Circle2D({-0.3f,0.3f}, 0.5f); NkManifold2D mt2;
        CHECK(NkGJKEPA2D(tt, circ, mt2), "gjk2d triangle-cercle overlap");
    }

    // ── World : nouveaux types convexes (dispatch -> GJK/EPA) ────────────────
    {
        NkWorld cw;
        cw.AddBody(NkShape::Cylinder3D({0,0,0}, {0,1,0}, 1.f, 1.f));
        cw.AddBody(NkShape::Sphere({1.4f,0,0}, 0.5f));       // côté x=1 vs surface x=0.9 -> overlap
        cw.Step();
        CHECK(cw.Pairs().Size() == 1, "world cylindre-sphere : 1 paire (dispatch GJK)");
    }

    // ── VAGUE 3 : concaves par décomposition + compound + événements ─────────
    {
        // Triangle mesh (quad XZ à y=0) vs sphère
        static const NkVec3f quad[4] = { {-1,0,-1}, {1,0,-1}, {1,0,1}, {-1,0,1} };
        static const uint32  idx[6]  = { 0,1,2, 0,2,3 };
        NkShape mesh = NkShape::Trimesh3D(quad, 4, idx, 6);
        NkManifold3D m;
        CHECK(NkConvexVsTrimesh3D(mesh, NkShape::Sphere({0,0.3f,0}, 0.5f), m), "trimesh-sphere overlap");
        NkManifold3D m2;
        CHECK(!NkConvexVsTrimesh3D(mesh, NkShape::Sphere({0,5,0}, 0.5f), m2), "trimesh-sphere separated");

        // Heightfield plat (2x2, y=0) vs sphère
        static const float32 hh[4] = { 0,0, 0,0 };
        NkShape hf = NkShape::Heightfield3D({-1,0,-1}, 2.f, 2.f, hh, 2, 2);
        NkManifold3D mh;
        CHECK(NkConvexVsHeightfield3D(hf, NkShape::Sphere({0,0.3f,0}, 0.5f), mh), "heightfield-sphere overlap");
        NkManifold3D mh2;
        CHECK(!NkConvexVsHeightfield3D(hf, NkShape::Sphere({0,5,0}, 0.5f), mh2), "heightfield-sphere separated");

        // Chain 2D (polyligne y=0) vs cercle
        static const NkVec3f chainV[3] = { {-2,0,0}, {0,0,0}, {2,0,0} };
        NkShape chain = NkShape::Chain2D(chainV, 3);
        NkManifold2D mc;
        CHECK(NkConvexVsChain2D(chain, NkShape::Circle2D({0,0.3f}, 0.5f), mc), "chain2D-cercle overlap");
        NkManifold2D mc2;
        CHECK(!NkConvexVsChain2D(chain, NkShape::Circle2D({0,5}, 0.5f), mc2), "chain2D-cercle separated");
    }

    // ── World : trimesh + compound (dispatch) ────────────────────────────────
    {
        static const NkVec3f quad[4] = { {-1,0,-1}, {1,0,-1}, {1,0,1}, {-1,0,1} };
        static const uint32  idx[6]  = { 0,1,2, 0,2,3 };
        NkWorld w;
        w.AddBody(NkShape::Trimesh3D(quad, 4, idx, 6));
        w.AddBody(NkShape::Sphere({0,0.3f,0}, 0.5f));
        w.Step();
        CHECK(w.Pairs().Size() == 1, "world trimesh-sphere : 1 paire");

        // Compound (2 sphères) vs sphère touchant l'une d'elles
        static const NkShape kids[2] = { NkShape::Sphere({-1,0,0}, 0.5f), NkShape::Sphere({1,0,0}, 0.5f) };
        NkWorld wc;
        wc.AddBody(NkShape::Compound(kids, 2));
        wc.AddBody(NkShape::Sphere({1.f,0.6f,0}, 0.5f));   // proche du child (1,0,0)
        wc.Step();
        CHECK(wc.Pairs().Size() == 1, "world compound-sphere : 1 paire (sous-forme)");
    }

    // ── Événements : enter / stay / exit ─────────────────────────────────────
    {
        NkWorld w;
        w.AddBody(NkShape::Sphere({0,0,0}, 1.f));
        uint32 b = w.AddBody(NkShape::Sphere({1.5f,0,0}, 1.f));
        w.Step();   // frame 1 : entrée
        CHECK(w.EnterEvents().Size() == 1 && w.StayEvents().Size() == 0 && w.ExitEvents().Size() == 0, "events: ENTER frame 1");
        w.Step();   // frame 2 : maintien
        CHECK(w.EnterEvents().Size() == 0 && w.StayEvents().Size() == 1 && w.ExitEvents().Size() == 0, "events: STAY frame 2");
        w.SetShape(b, NkShape::Sphere({5,0,0}, 1.f));     // on écarte b
        w.Step();   // frame 3 : sortie
        CHECK(w.EnterEvents().Size() == 0 && w.StayEvents().Size() == 0 && w.ExitEvents().Size() == 1, "events: EXIT frame 3");
    }

    // ── VAGUE 4 : OBB 3D orientée + broadphase Sweep-and-Prune ───────────────
    {
        // OBB tournée 45° autour de Z : son coin porte à ~1.41 sur +X ; une AABB ne
        // porte qu'à 1.0. Une sphère à x=1.3 (surface 1.1) touche l'OBB, pas l'AABB.
        const float32 s22 = 0.38268343f, c22 = 0.92387953f;   // quat 45° autour de Z
        NkShape obb  = NkShape::OBB3D({0,0,0}, {1,1,1}, math::NkQuatf(0.f, 0.f, s22, c22));
        NkShape aabb = NkShape::Box3D({0,0,0}, {1,1,1});
        NkShape sp   = NkShape::Sphere({1.3f,0,0}, 0.2f);
        NkManifold3D mo, ma;
        CHECK(NkGJKEPA3D(obb, sp, mo), "OBB3D 45deg vs sphere : overlap (coin ~1.41)");
        CHECK(!NkGJKEPA3D(aabb, sp, ma), "AABB vs sphere : pas de contact a 1.3 (portee 1.0)");

        // Dispatch world : l'OBB orientée doit être routée vers GJK (pas l'analytique AABB).
        NkWorld wo;
        wo.AddBody(NkShape::OBB3D({0,0,0}, {1,1,1}, math::NkQuatf(0.f, 0.f, s22, c22)));
        wo.AddBody(NkShape::Sphere({1.3f,0,0}, 0.2f));
        wo.Step();
        CHECK(wo.Pairs().Size() == 1, "world OBB3D-sphere : 1 paire (dispatch GJK)");
    }
    {
        // Sweep-and-Prune : chaîne de 5 sphères, seules les adjacentes se touchent.
        NkWorld sw;
        for (int32 i = 0; i < 5; ++i) sw.AddBody(NkShape::Sphere({ (float32)i, 0.f, 0.f }, 0.6f));
        sw.Step();
        CHECK(sw.Pairs().Size() == 4, "SAP : 4 paires adjacentes (chaine de 5 spheres)");

        // Sphères toutes éloignées -> 0 paire (le balayage prune tout).
        NkWorld sw2;
        for (int32 i = 0; i < 6; ++i) sw2.AddBody(NkShape::Sphere({ (float32)i * 10.f, 0.f, 0.f }, 0.6f));
        sw2.Step();
        CHECK(sw2.Pairs().Size() == 0, "SAP : 0 paire (spheres eloignees)");
    }

    // ── VAGUE 5 : requêtes de scène (raycast exact + overlap) ────────────────
    {
        const float32 s22 = 0.38268343f, c22 = 0.92387953f;   // 45° autour de Z
        // OBB identité == AABB : rayon +X vers boîte à l'origine -> t=4
        NkRay3D rx; rx.origin = {-5,0,0}; rx.dir = {1,0,0}; rx.maxT = 100.f;
        NkRayHit3D h;
        CHECK(NkRayOBB3D(rx, {0,0,0}, {1,1,1}, math::NkQuatf(), h) && Near(h.t, 4.f), "ray-OBB(identite)==AABB t=4");
        // OBB tournée 45° : portée en Y = ~1.414 -> rayon +Y depuis y=-5 entre à t~3.586
        NkRay3D ry; ry.origin = {0,-5,0}; ry.dir = {0,1,0}; ry.maxT = 100.f;
        NkRayHit3D h2;
        CHECK(NkRayOBB3D(ry, {0,0,0}, {1,1,1}, math::NkQuatf(0.f,0.f,s22,c22), h2) && Near(h2.t, 3.5858f, 1e-2f), "ray-OBB 45deg t~3.586");
        // ray-plane
        NkRayHit3D hp;
        CHECK(NkRayPlane3D(ry, {0,0,0}, {0,1,0}, hp) && Near(hp.t, 5.f), "ray-plane t=5");
        // ray-triangle (Möller-Trumbore)
        NkRayHit3D ht;
        CHECK(NkRayTriangle3D(ry, {-1,0,-1}, {1,0,-1}, {0,0,1}, ht) && Near(ht.t, 5.f), "ray-triangle t=5");
    }

    // ── World : raycast exact (OBB/plan/trimesh) + Overlap ───────────────────
    {
        NkWorld w;
        w.AddBody(NkShape::Plane3D({0,0,0}, {0,1,0}));
        NkRay3D ray; ray.origin = {0,5,0}; ray.dir = {0,-1,0}; ray.maxT = 100.f;
        NkRayHit3D h;
        CHECK(w.Raycast3D(ray, h) && Near(h.t, 5.f) && h.normal.y > 0.8f, "world raycast plane t=5 normale+Y");

        static const NkVec3f quad[4] = { {-1,0,-1}, {1,0,-1}, {1,0,1}, {-1,0,1} };
        static const uint32  idx[6]  = { 0,1,2, 0,2,3 };
        NkWorld wm;
        wm.AddBody(NkShape::Trimesh3D(quad, 4, idx, 6));
        NkRayHit3D hm;
        CHECK(wm.Raycast3D(ray, hm) && Near(hm.t, 5.f), "world raycast trimesh t=5");

        // Overlap : forme arbitraire vs monde
        NkWorld wo;
        wo.AddBody(NkShape::Sphere({0,0,0}, 1.f));
        wo.AddBody(NkShape::Sphere({1.5f,0,0}, 1.f));
        wo.AddBody(NkShape::Sphere({100,0,0}, 1.f));
        NkVector<uint32> ids;
        const uint32 cnt = wo.Overlap(NkShape::Sphere({0.7f,0,0}, 1.f), ids);
        CHECK(cnt == 2, "Overlap sphere : 2 corps touches (pas le lointain)");
    }

    // ── VAGUE 6 : ray-cast convexe générique (GJK/CA) + shape cast ───────────
    {
        NkRay3D ray; ray.origin = {-5,0,0}; ray.dir = {1,0,0}; ray.maxT = 100.f;
        // ray vs sphère via CA == analytique (t=4)
        NkRayHit3D h;
        CHECK(NkRayConvex3D(ray, NkShape::Sphere({0,0,0}, 1.f), h) && Near(h.t, 4.f, 1e-2f), "ray-convexe sphere t=4 (CA)");
        // ray vs capsule (côté à x=-0.5) -> t=4.5
        NkRayHit3D hc;
        CHECK(NkRayConvex3D(ray, NkShape::Capsule3D({0,-1,0}, {0,1,0}, 0.5f), hc) && Near(hc.t, 4.5f, 2e-2f), "ray-convexe capsule t=4.5 (CA)");
        // ray vs cône (booléen : touche)
        NkRayHit3D hco;
        CHECK(NkRayConvex3D(ray, NkShape::Cone3D({0,-1,0}, {0,1,0}, 2.f, 1.f), hco), "ray-convexe cone (CA) touche");

        // World : raycast exact sur un corps capsule
        NkWorld w;
        w.AddBody(NkShape::Capsule3D({0,-1,0}, {0,1,0}, 0.5f));
        NkRayHit3D hw;
        CHECK(w.Raycast3D(ray, hw) && Near(hw.t, 4.5f, 2e-2f), "world raycast capsule t=4.5 (exact)");
    }
    {
        // Shape cast : sphère r=0.5 partant de x=-5 vers +X, cible sphère r=1 à l'origine.
        // Contact quand |centre| = 1.5 -> centre à x=-1.5 -> distance parcourue 3.5.
        NkWorld w;
        w.AddBody(NkShape::Sphere({0,0,0}, 1.f));
        NkRayHit3D hit;
        CHECK(w.ShapeCast(NkShape::Sphere({-5,0,0}, 0.5f), {1,0,0}, 100.f, hit) && Near(hit.t, 3.5f, 2e-2f), "shapecast sphere TOI=3.5");
        // Passe au-dessus -> rate
        NkRayHit3D hit2;
        CHECK(!w.ShapeCast(NkShape::Sphere({-5,5,0}, 0.5f), {1,0,0}, 100.f, hit2), "shapecast sphere rate (passe au-dessus)");
    }

    logger.Info("=== NKCollision : {0} passes, {1} echecs ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
