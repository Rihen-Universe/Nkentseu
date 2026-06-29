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

    logger.Info("=== NKCollision : {0} passes, {1} echecs ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
