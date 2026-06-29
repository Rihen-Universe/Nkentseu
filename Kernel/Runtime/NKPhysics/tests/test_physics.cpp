// =============================================================================
// test_physics.cpp — Self-test NKPhysics (NKLogger, pas printf). [M0]
// =============================================================================
#include "NKPhysics/NKPhysics.h"
#include "NKLogger/NkLog.h"

using namespace nkentseu;
using namespace nkentseu::physics;

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { ++g_pass; } else { ++g_fail; \
    logger.Errorf("  [FAIL] %s\n", msg); } } while (0)
static bool Near(float32 a, float32 b, float32 eps = 1e-3f) { float32 d = a - b; return (d < 0 ? -d : d) <= eps; }

int main() {
    // ── M0 : chute libre (Euler semi-implicite) ─────────────────────────────
    {
        NkPhysicsWorld world(NkPhysicsConfig{ /*gravity*/ {0.f, -9.81f, 0.f} });
        NkBodyDef def; def.type = NkBodyType::DYNAMIC; def.position = { 0.f, 10.f, 0.f };
        const NkBodyId id = world.CreateBody(def, collision::NkShape::Sphere({0,10,0}, 0.5f));

        const float32 dt = 1.f / 60.f;
        for (int i = 0; i < 60; ++i) world.Step(dt);  // 1 s

        const NkRigidBody* b = world.GetBody(id);
        // Euler symplectique : chute = g*dt²*n(n+1)/2 = 9.81*(1/3600)*1830 ≈ 4.987
        CHECK(b && Near(b->position.y, 10.f - 4.9867f, 0.05f), "M0 chute libre : y ~ 5.013 apres 1s");
        CHECK(b && Near(b->linearVelocity.y, -9.81f, 0.05f), "M0 vitesse : vy ~ -9.81 apres 1s");
        // la shape de collision a suivi le corps
        const collision::NkBody* cb = nullptr; // accès indirect via world non exposé -> on valide la pose du body
        (void)cb;
    }

    // ── Corps statique : ne bouge pas ; gravityScale=0 : ne tombe pas ───────
    {
        NkPhysicsWorld world(NkPhysicsConfig{ {0.f, -9.81f, 0.f} });
        NkBodyDef st; st.type = NkBodyType::STATIC; st.position = { 0.f, 0.f, 0.f };
        const NkBodyId sid = world.CreateBody(st, collision::NkShape::Box3D({0,0,0}, {5,1,5}));
        NkBodyDef fl; fl.type = NkBodyType::DYNAMIC; fl.position = { 3.f, 2.f, 0.f }; fl.gravityScale = 0.f;
        const NkBodyId fid = world.CreateBody(fl, collision::NkShape::Sphere({3,2,0}, 0.5f));
        for (int i = 0; i < 30; ++i) world.Step(1.f / 60.f);
        CHECK(world.GetBody(sid)->position.y == 0.f, "statique immobile");
        CHECK(Near(world.GetBody(fid)->position.y, 2.f), "gravityScale=0 : pas de chute");
    }

    logger.Info("=== NKPhysics : {0} passes, {1} echecs ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
