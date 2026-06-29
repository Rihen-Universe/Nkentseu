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

    // ── M1 : la bille tombe et REPOSE sur le sol (ne le traverse pas) ────────
    {
        NkPhysicsWorld world(NkPhysicsConfig{ {0.f, -9.81f, 0.f} });
        // sol statique : sommet à y=1
        NkBodyDef gd; gd.type = NkBodyType::STATIC;
        world.CreateBody(gd, collision::NkShape::Box3D({0,0,0}, {10,1,10}));
        // bille dynamique r=0.5 lâchée de y=5 -> repos attendu centre y ≈ 1.5
        NkBodyDef bd; bd.type = NkBodyType::DYNAMIC; bd.position = { 0.f, 5.f, 0.f };
        const NkBodyId id = world.CreateBody(bd, collision::NkShape::Sphere({0,5,0}, 0.5f));

        for (int i = 0; i < 240; ++i) world.Step(1.f / 60.f);  // 4 s

        const NkRigidBody* b = world.GetBody(id);
        CHECK(b && b->position.y > 1.0f, "M1 : la bille ne traverse PAS le sol (y > 1)");
        CHECK(b && Near(b->position.y, 1.5f, 0.06f), "M1 : la bille REPOSE (centre y ~ 1.5)");
        CHECK(b && Near(b->linearVelocity.y, 0.f, 0.2f), "M1 : au repos (vy ~ 0)");
    }

    // ── M1 : caisse dynamique posée sur le sol ───────────────────────────────
    {
        NkPhysicsWorld world(NkPhysicsConfig{ {0.f, -9.81f, 0.f} });
        NkBodyDef gd; gd.type = NkBodyType::STATIC;
        world.CreateBody(gd, collision::NkShape::Box3D({0,0,0}, {10,1,10}));   // sommet y=1
        NkBodyDef bd; bd.type = NkBodyType::DYNAMIC; bd.position = { 0.f, 4.f, 0.f };
        const NkBodyId id = world.CreateBody(bd, collision::NkShape::Box3D({0,4,0}, {0.5f,0.5f,0.5f}));
        for (int i = 0; i < 240; ++i) world.Step(1.f / 60.f);
        const NkRigidBody* b = world.GetBody(id);
        CHECK(b && Near(b->position.y, 1.5f, 0.08f), "M1 : la caisse REPOSE (centre y ~ 1.5)");
    }

    // ── M2 : frottement (le glissement s'arrête) ────────────────────────────
    {
        NkPhysicsWorld world(NkPhysicsConfig{ {0.f, -9.81f, 0.f} });
        NkPhysicsMaterial grippy; grippy.dynamicFriction = 0.9f; grippy.staticFriction = 0.9f;
        NkBodyDef gd; gd.type = NkBodyType::STATIC; gd.material = grippy;
        world.CreateBody(gd, collision::NkShape::Box3D({0,0,0}, {20,1,20}));   // sommet y=1
        // caisse posée (y=1.5) lancée horizontalement -> le frottement la freine.
        NkBodyDef bd; bd.type = NkBodyType::DYNAMIC; bd.position = { 0.f, 1.5f, 0.f };
        bd.linearVelocity = { 3.f, 0.f, 0.f }; bd.material = grippy;
        const NkBodyId id = world.CreateBody(bd, collision::NkShape::Box3D({0,1.5f,0}, {0.5f,0.5f,0.5f}));
        for (int i = 0; i < 180; ++i) world.Step(1.f / 60.f);
        const NkRigidBody* b = world.GetBody(id);
        CHECK(b && Near(b->linearVelocity.x, 0.f, 0.2f), "M2 frottement : la caisse s'arrete (vx ~ 0)");
        CHECK(b && b->position.x < 2.0f, "M2 frottement : distance de glissement bornee");
    }

    // ── M2 : restitution (la bille rebondit) ────────────────────────────────
    {
        NkPhysicsWorld world(NkPhysicsConfig{ {0.f, -9.81f, 0.f} });
        NkBodyDef gd; gd.type = NkBodyType::STATIC;
        world.CreateBody(gd, collision::NkShape::Box3D({0,0,0}, {20,1,20}));   // sommet y=1
        NkBodyDef bd; bd.type = NkBodyType::DYNAMIC; bd.position = { 0.f, 3.f, 0.f };
        bd.material.restitution = 0.8f;
        const NkBodyId id = world.CreateBody(bd, collision::NkShape::Sphere({0,3,0}, 0.5f));
        float32 maxYAfterImpact = 0.f; bool impacted = false;
        for (int i = 0; i < 240; ++i) {
            world.Step(1.f / 60.f);
            const NkRigidBody* b = world.GetBody(id);
            if (b->position.y < 1.7f) impacted = true;            // proche du sol -> a touché
            if (impacted && b->position.y > maxYAfterImpact) maxYAfterImpact = b->position.y;
        }
        CHECK(impacted && maxYAfterImpact > 2.2f, "M2 restitution : la bille rebondit (pic > 2.2)");
    }

    // ── M3 : warm-starting -> pile de 3 caisses STABLE ──────────────────────
    {
        NkPhysicsWorld world(NkPhysicsConfig{ {0.f, -9.81f, 0.f} });
        NkBodyDef gd; gd.type = NkBodyType::STATIC;
        world.CreateBody(gd, collision::NkShape::Box3D({0,0,0}, {20,1,20}));   // sommet y=1
        // 3 caisses empilées (demi 0.5) -> repos 1.5 / 2.5 / 3.5
        NkBodyId ids[3];
        for (int k = 0; k < 3; ++k) {
            NkBodyDef bd; bd.type = NkBodyType::DYNAMIC;
            const float32 y = 1.5f + k * 1.0f;
            bd.position = { 0.f, y, 0.f };
            ids[k] = world.CreateBody(bd, collision::NkShape::Box3D({0,y,0}, {0.5f,0.5f,0.5f}));
        }
        for (int i = 0; i < 300; ++i) world.Step(1.f / 60.f);   // 5 s
        const NkRigidBody* bottom = world.GetBody(ids[0]);
        const NkRigidBody* top    = world.GetBody(ids[2]);
        CHECK(bottom && Near(bottom->position.y, 1.5f, 0.1f), "M3 pile : caisse du bas tenue (~1.5)");
        CHECK(top && top->position.y > 3.2f && top->position.y < 3.7f, "M3 pile : caisse du haut tenue (~3.5, pas effondree)");
        CHECK(top && Near(top->linearVelocity.y, 0.f, 0.3f), "M3 pile : au repos (vy ~ 0)");
    }

    // ── M4 : split-impulse -> repos PRÉCIS (pénétration ~ slop) et PROPRE ────
    {
        NkPhysicsWorld world(NkPhysicsConfig{ {0.f, -9.81f, 0.f} });
        NkBodyDef gd; gd.type = NkBodyType::STATIC;
        world.CreateBody(gd, collision::NkShape::Box3D({0,0,0}, {20,1,20}));   // sommet y=1
        NkBodyDef bd; bd.type = NkBodyType::DYNAMIC; bd.position = { 0.f, 3.f, 0.f };
        const NkBodyId id = world.CreateBody(bd, collision::NkShape::Box3D({0,3,0}, {0.5f,0.5f,0.5f}));
        for (int i = 0; i < 300; ++i) world.Step(1.f / 60.f);
        const NkRigidBody* b = world.GetBody(id);
        // centre au repos ≈ 1 + 0.5 - slop ≈ 1.495 -> tolérance serrée
        CHECK(b && Near(b->position.y, 1.5f, 0.03f), "M4 : repos precis (penetration ~ slop)");
        CHECK(b && Near(b->linearVelocity.y, 0.f, 0.05f), "M4 : repos propre (vy ~ 0, pas d'energie injectee)");
    }

    // ── M5 : plateforme KINEMATIC montante porte une caisse dynamique ───────
    {
        NkPhysicsWorld world(NkPhysicsConfig{ {0.f, -9.81f, 0.f} });
        // plateforme kinematic (demi 0.5 en Y) montant à 1 m/s, sommet initial y=0.5
        NkBodyDef pd; pd.type = NkBodyType::KINEMATIC; pd.position = { 0.f, 0.f, 0.f };
        pd.linearVelocity = { 0.f, 1.f, 0.f };
        const NkBodyId plat = world.CreateBody(pd, collision::NkShape::Box3D({0,0,0}, {5,0.5f,5}));
        // caisse dynamique posée dessus (centre y=1.0)
        NkBodyDef bd; bd.type = NkBodyType::DYNAMIC; bd.position = { 0.f, 1.f, 0.f };
        const NkBodyId box = world.CreateBody(bd, collision::NkShape::Box3D({0,1,0}, {0.5f,0.5f,0.5f}));

        for (int i = 0; i < 60; ++i) world.Step(1.f / 60.f);   // 1 s

        const NkRigidBody* p = world.GetBody(plat);
        const NkRigidBody* b = world.GetBody(box);
        CHECK(p && Near(p->position.y, 1.0f, 0.05f), "M5 kinematic : la plateforme suit sa vitesse (y ~ 1.0)");
        CHECK(b && b->position.y > 1.7f, "M5 kinematic : la caisse est PORTEE vers le haut (y > 1.7)");
        CHECK(b && b->position.y < 2.2f, "M5 kinematic : la caisse reste posee (pas ejectee)");
    }

    // ── M6 : sommeil (la caisse immobile s'endort) + réveil au choc ─────────
    {
        NkPhysicsWorld world(NkPhysicsConfig{ {0.f, -9.81f, 0.f} });
        NkBodyDef gd; gd.type = NkBodyType::STATIC;
        world.CreateBody(gd, collision::NkShape::Box3D({0,0,0}, {20,1,20}));   // sommet y=1
        NkBodyDef bd; bd.type = NkBodyType::DYNAMIC; bd.position = { 0.f, 1.5f, 0.f };
        const NkBodyId id = world.CreateBody(bd, collision::NkShape::Box3D({0,1.5f,0}, {0.5f,0.5f,0.5f}));
        for (int i = 0; i < 240; ++i) world.Step(1.f / 60.f);  // 4 s -> repos puis sommeil
        CHECK(world.GetBody(id) && !world.GetBody(id)->IsAwake(), "M6 sommeil : la caisse immobile s'endort");

        // réveil : on lâche une 2e caisse dessus.
        NkBodyDef d2; d2.type = NkBodyType::DYNAMIC; d2.position = { 0.f, 4.f, 0.f };
        world.CreateBody(d2, collision::NkShape::Box3D({0,4,0}, {0.5f,0.5f,0.5f}));
        bool woke = false;
        for (int i = 0; i < 120; ++i) { world.Step(1.f / 60.f); if (world.GetBody(id)->IsAwake()) woke = true; }
        CHECK(woke, "M6 reveil : la caisse se reveille au choc de la 2e");
    }

    // ── M7 : DISTANCE joint -> pendule (la longueur est maintenue) ──────────
    {
        NkPhysicsWorld world(NkPhysicsConfig{ {0.f, -9.81f, 0.f} });
        NkBodyDef ad; ad.type = NkBodyType::STATIC; ad.position = { 0.f, 5.f, 0.f };
        const NkBodyId anchor = world.CreateBody(ad, collision::NkShape::Sphere({0,5,0}, 0.1f));
        NkBodyDef bd; bd.type = NkBodyType::DYNAMIC; bd.position = { 2.f, 5.f, 0.f };
        bd.linearDamping = 0.6f; bd.angularDamping = 0.6f;
        const NkBodyId ball = world.CreateBody(bd, collision::NkShape::Sphere({2,5,0}, 0.3f));
        world.CreateDistanceJoint(anchor, ball, {0,5,0}, {2,5,0});   // longueur 2
        for (int i = 0; i < 360; ++i) world.Step(1.f / 60.f);        // 6 s
        const NkRigidBody* b = world.GetBody(ball);
        const NkVec3f d = b->position - NkVec3f{0,5,0};
        const float32 L = math::NkSqrt(d.Dot(d));
        CHECK(Near(L, 2.f, 0.1f), "M7 distance : la longueur du pendule est maintenue (~2)");
        CHECK(b->position.y < 4.f, "M7 distance : le pendule est descendu (swing)");
        CHECK(b->position.x < 1.5f && b->position.y < 3.6f, "M7 distance : descend vers l'equilibre (sous l'ancre)");
    }

    // ── M7 : BALL joint -> le corps est SUSPENDU au pivot (ne tombe pas) ────
    {
        NkPhysicsWorld world(NkPhysicsConfig{ {0.f, -9.81f, 0.f} });
        NkBodyDef ad; ad.type = NkBodyType::STATIC; ad.position = { 0.f, 5.f, 0.f };
        const NkBodyId anchor = world.CreateBody(ad, collision::NkShape::Sphere({0,5,0}, 0.1f));
        NkBodyDef bd; bd.type = NkBodyType::DYNAMIC; bd.position = { 0.f, 4.f, 0.f };
        bd.linearDamping = 0.6f; bd.angularDamping = 0.6f;
        const NkBodyId body = world.CreateBody(bd, collision::NkShape::Box3D({0,4,0}, {0.3f,0.3f,0.3f}));
        world.CreateBallJoint(anchor, body, {0,5,0});               // pivot au-dessus du corps
        for (int i = 0; i < 240; ++i) world.Step(1.f / 60.f);
        const NkRigidBody* b = world.GetBody(body);
        CHECK(b->position.y > 3.5f, "M7 ball : le corps reste SUSPENDU (ne tombe pas)");
        const NkVec3f d = b->position - NkVec3f{0,5,0};
        CHECK(Near(math::NkSqrt(d.Dot(d)), 1.f, 0.15f), "M7 ball : distance au pivot ~ 1 (tenu)");
    }

    // ── M7 cut2 : REVOLUTE (charnière) -> trappe qui bascule dans son plan ──
    {
        NkPhysicsWorld world(NkPhysicsConfig{ {0.f, -9.81f, 0.f} });
        NkBodyDef ad; ad.type = NkBodyType::STATIC; ad.position = { 0.f, 5.f, 0.f };
        const NkBodyId anchor = world.CreateBody(ad, collision::NkShape::Sphere({0,5,0}, 0.1f));
        // panneau : centre en (1,5,0), bord gauche au pivot (0,5,0). Charnière axe Z.
        NkBodyDef pd; pd.type = NkBodyType::DYNAMIC; pd.position = { 1.f, 5.f, 0.f };
        pd.linearDamping = 0.6f; pd.angularDamping = 0.6f;
        const NkBodyId panel = world.CreateBody(pd, collision::NkShape::Box3D({1,5,0}, {1.f,0.1f,0.5f}));
        world.CreateRevoluteJoint(anchor, panel, {0,5,0}, {0,0,1});   // axe Z
        for (int i = 0; i < 360; ++i) world.Step(1.f / 60.f);         // 6 s
        const NkRigidBody* b = world.GetBody(panel);
        const NkVec3f d = b->position - NkVec3f{0,5,0};
        CHECK(Near(math::NkSqrt(d.Dot(d)), 1.f, 0.12f), "M7 revolute : pivot tenu (centre a ~1 du pivot)");
        CHECK(b->position.y < 4.5f, "M7 revolute : la trappe a bascule sous la gravite");
        CHECK(Near(b->position.z, 0.f, 0.08f), "M7 revolute : reste dans le plan (axe Z aligne)");
        CHECK(b->position.x < 0.8f, "M7 revolute : bascule vers l'equilibre (rentre depuis x=1)");
    }

    // ── M7 cut3 : WELD (soudure rigide) -> corps fige en pose ───────────────
    {
        NkPhysicsWorld world(NkPhysicsConfig{ {0.f, -9.81f, 0.f} });
        NkBodyDef ad; ad.type = NkBodyType::STATIC; ad.position = { 0.f, 0.f, 0.f };
        const NkBodyId anchor = world.CreateBody(ad, collision::NkShape::Sphere({0,0,0}, 0.1f));
        NkBodyDef bd; bd.type = NkBodyType::DYNAMIC; bd.position = { 2.f, 0.f, 0.f };
        const NkBodyId box = world.CreateBody(bd, collision::NkShape::Box3D({2,0,0}, {0.5f,0.5f,0.5f}));
        world.CreateWeldJoint(anchor, box, {1.f, 0.f, 0.f});       // soudé (offset 1 du pivot)
        for (int i = 0; i < 180; ++i) world.Step(1.f / 60.f);      // 3 s
        const NkRigidBody* b = world.GetBody(box);
        CHECK(Near(b->position.x, 2.f, 0.15f) && Near(b->position.y, 0.f, 0.15f) && Near(b->position.z, 0.f, 0.1f),
              "M7 weld : le corps reste fige en position (pas de chute)");
        CHECK(Near(b->orientation.w, 1.f, 0.1f), "M7 weld : orientation verrouillee (pas de rotation)");
    }

    // ── M8 : MOTEUR (drive PD) -> un bras se TIENT a l'horizontale (ragdoll actif) ──
    {
        NkPhysicsWorld world(NkPhysicsConfig{ {0.f, -9.81f, 0.f} });
        NkBodyDef ad; ad.type = NkBodyType::STATIC; ad.position = { 0.f, 5.f, 0.f };
        const NkBodyId anchor = world.CreateBody(ad, collision::NkShape::Sphere({0,5,0}, 0.1f));
        NkBodyDef pd; pd.type = NkBodyType::DYNAMIC; pd.position = { 1.f, 5.f, 0.f };
        const NkBodyId arm = world.CreateBody(pd, collision::NkShape::Box3D({1,5,0}, {1.f,0.1f,0.3f}));
        const NkJointId jt = world.CreateRevoluteJoint(anchor, arm, {0,5,0}, {0,0,1});
        world.SetRevoluteMotor(jt, /*angle cible*/ 0.f, /*kp*/ 25.f, /*couple max*/ 500.f);
        for (int i = 0; i < 180; ++i) world.Step(1.f / 60.f);
        const NkRigidBody* b = world.GetBody(arm);
        // sans moteur le bras tomberait à (0,4,0) ; avec moteur il TIENT l'horizontale (~1,5,0).
        CHECK(Near(b->position.x, 1.f, 0.3f) && Near(b->position.y, 5.f, 0.3f), "M8 moteur : le bras TIENT la pose horizontale");
    }

    // ── M8 : LIMITE d'angle -> la trappe s'arrête à la borne (anti-hyperextension) ──
    {
        NkPhysicsWorld world(NkPhysicsConfig{ {0.f, -9.81f, 0.f} });
        NkBodyDef ad; ad.type = NkBodyType::STATIC; ad.position = { 0.f, 5.f, 0.f };
        const NkBodyId anchor = world.CreateBody(ad, collision::NkShape::Sphere({0,5,0}, 0.1f));
        NkBodyDef pd; pd.type = NkBodyType::DYNAMIC; pd.position = { 1.f, 5.f, 0.f };
        pd.angularDamping = 0.6f;
        const NkBodyId panel = world.CreateBody(pd, collision::NkShape::Box3D({1,5,0}, {1.f,0.1f,0.3f}));
        const NkJointId jt = world.CreateRevoluteJoint(anchor, panel, {0,5,0}, {0,0,1});
        world.SetRevoluteLimit(jt, /*lower*/ -0.5f, /*upper*/ 1.0f);   // ne descend pas sous -0.5 rad
        for (int i = 0; i < 240; ++i) world.Step(1.f / 60.f);
        const NkRigidBody* b = world.GetBody(panel);
        // arrêt à ~-0.5 rad -> centre ~ (0.88, 4.52) ; PAS de pleine pente (0,4).
        CHECK(b->position.y > 4.3f, "M8 limite : la trappe s'ARRETE a la borne (ne pend pas a fond)");
        CHECK(b->position.y < 4.85f, "M8 limite : elle a bien bascule jusqu'a la limite");
    }

    logger.Info("=== NKPhysics : {0} passes, {1} echecs ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
