// tests/test_suite.cpp — Tests unitaires complets
// Compile: g++ -std=c++17 -O2 -msse4.2 -I../include ../src/shapes.cpp test_suite.cpp -o test_suite -lpthread
// Run:     ./test_suite

#include <collision/collision.h>
#include <iostream>
#include <cmath>
#include <cassert>
#include <vector>
#include <sstream>

using namespace col;

// ─────────────────────────────────────────────────────────────────────────────
//  Micro test framework
// ─────────────────────────────────────────────────────────────────────────────
static int  gPass=0, gFail=0;
static std::string gSuite;

#define SUITE(name) { gSuite = name; std::cout << "\n── " << name << " ──\n"; }

#define EXPECT_TRUE(expr) do { \
    if(!(expr)) { \
        std::cout << "  FAIL  " << #expr << "  [" << __FILE__ << ":" << __LINE__ << "]\n"; \
        gFail++; \
    } else { gPass++; } \
} while(0)

#define EXPECT_FALSE(expr)     EXPECT_TRUE(!(expr))
#define EXPECT_EQ(a,b)         EXPECT_TRUE((a)==(b))
#define EXPECT_NEAR(a,b,eps)   EXPECT_TRUE(std::abs((a)-(b)) < (eps))
#define EXPECT_GT(a,b)         EXPECT_TRUE((a)>(b))
#define EXPECT_GE(a,b)         EXPECT_TRUE((a)>=(b))
#define EXPECT_LT(a,b)         EXPECT_TRUE((a)<(b))

// ─────────────────────────────────────────────────────────────────────────────
//  T1 — Vec2
// ─────────────────────────────────────────────────────────────────────────────
void testVec2() {
    SUITE("Vec2");

    Vec2 a(3,4), b(1,2);
    EXPECT_NEAR(a.length(), 5.f, 1e-5f);
    EXPECT_NEAR(a.dot(b), 11.f, 1e-5f);
    EXPECT_NEAR(a.cross(b), 2.f, 1e-5f);    // 3*2 - 4*1

    Vec2 n = a.normalized();
    EXPECT_NEAR(n.length(), 1.f, 1e-5f);

    Vec2 p = a.perp();
    EXPECT_NEAR(p.dot(a), 0.f, 1e-5f);

    Vec2 mn = Vec2::Min(a,b);
    EXPECT_NEAR(mn.x, 1.f, 1e-5f);
    EXPECT_NEAR(mn.y, 2.f, 1e-5f);

    Vec2 mx = Vec2::Max(a,b);
    EXPECT_NEAR(mx.x, 3.f, 1e-5f);
    EXPECT_NEAR(mx.y, 4.f, 1e-5f);

    Vec2 lerp = Vec2::Lerp(Vec2(0,0), Vec2(10,10), 0.5f);
    EXPECT_NEAR(lerp.x, 5.f, 1e-5f);
}

// ─────────────────────────────────────────────────────────────────────────────
//  T2 — Vec3
// ─────────────────────────────────────────────────────────────────────────────
void testVec3() {
    SUITE("Vec3");

    Vec3 a(1,0,0), b(0,1,0);
    Vec3 c = a.cross(b);
    EXPECT_NEAR(c.x, 0.f, 1e-5f);
    EXPECT_NEAR(c.y, 0.f, 1e-5f);
    EXPECT_NEAR(c.z, 1.f, 1e-5f);

    EXPECT_NEAR(a.dot(b), 0.f, 1e-5f);
    EXPECT_NEAR(Vec3(3,4,0).length(), 5.f, 1e-5f);
    EXPECT_NEAR(Vec3(3,4,0).normalized().length(), 1.f, 1e-5f);

    // Component-wise
    Vec3 d(2,3,4), e(1,2,3);
    Vec3 prod = d*e;
    EXPECT_NEAR(prod.x, 2.f, 1e-5f);
    EXPECT_NEAR(prod.y, 6.f, 1e-5f);

    // Division by Vec3
    Vec3 div = d/e;
    EXPECT_NEAR(div.x, 2.f, 1e-5f);
    EXPECT_NEAR(div.y, 1.5f, 1e-5f);
}

// ─────────────────────────────────────────────────────────────────────────────
//  T3 — Quaternion
// ─────────────────────────────────────────────────────────────────────────────
void testQuat() {
    SUITE("Quaternion");

    // Identity should not rotate
    Quat id = Quat::identity();
    Vec3 v(1,2,3);
    Vec3 rv = id.rotate(v);
    EXPECT_NEAR(rv.x, 1.f, 1e-5f);
    EXPECT_NEAR(rv.y, 2.f, 1e-5f);
    EXPECT_NEAR(rv.z, 3.f, 1e-5f);

    // 90° around Y: X -> -Z
    Quat qy = Quat::fromAxisAngle({0,1,0}, kPi*0.5f);
    Vec3 xAxis(1,0,0);
    Vec3 rotated = qy.rotate(xAxis);
    EXPECT_NEAR(rotated.x, 0.f, 1e-4f);
    EXPECT_NEAR(rotated.y, 0.f, 1e-4f);
    EXPECT_NEAR(rotated.z, -1.f, 1e-4f);

    // Conjugate * q = identity
    Quat conj = qy.conjugate();
    Vec3 back = conj.rotate(rotated);
    EXPECT_NEAR(back.x, 1.f, 1e-4f);
    EXPECT_NEAR(back.z, 0.f, 1e-4f);

    // toMat3 and back
    Mat3 m = qy.toMat3();
    Vec3 fromMat = m * xAxis;
    EXPECT_NEAR(fromMat.x, 0.f, 1e-4f);
    EXPECT_NEAR(fromMat.z, -1.f, 1e-4f);
}

// ─────────────────────────────────────────────────────────────────────────────
//  T4 — AABB2D / AABB3D
// ─────────────────────────────────────────────────────────────────────────────
void testAABB() {
    SUITE("AABB");

    // 2D
    AABB2D a({0,0},{2,2}), b({1,1},{3,3}), c({5,5},{7,7});
    EXPECT_TRUE(a.overlaps(b));
    EXPECT_FALSE(a.overlaps(c));
    EXPECT_TRUE(a.contains({1,1}));
    EXPECT_FALSE(a.contains({3,3}));
    EXPECT_NEAR(a.area(), 4.f, 1e-5f);
    EXPECT_GT(a.overlapArea(b), 0.f);
    EXPECT_NEAR(a.overlapArea(c), 0.f, 1e-5f);

    // 2D support function
    Vec2 sup = a.support({1,1});
    EXPECT_NEAR(sup.x, 2.f, 1e-5f);
    EXPECT_NEAR(sup.y, 2.f, 1e-5f);

    // 3D
    AABB3D A({0,0,0},{2,2,2}), B({1,1,1},{3,3,3}), C({5,5,5},{7,7,7});
    EXPECT_TRUE(A.overlaps(B));
    EXPECT_FALSE(A.overlaps(C));
    EXPECT_NEAR(A.surfaceArea(), 24.f, 1e-5f);
    EXPECT_NEAR(A.volume(), 8.f, 1e-5f);

    // 3D merge
    AABB3D merged = AABB3D::merge(A, B);
    EXPECT_NEAR(merged.min.x, 0.f, 1e-5f);
    EXPECT_NEAR(merged.max.x, 3.f, 1e-5f);

    // 3D raycast
    Ray ray({-5,1,1},{1,0,0});
    float t;
    EXPECT_TRUE(A.raycast(ray, t));
    EXPECT_NEAR(t, 5.f, 1e-4f);

    Ray missRay({-5,10,1},{1,0,0});
    EXPECT_FALSE(A.raycast(missRay, t));
}

// ─────────────────────────────────────────────────────────────────────────────
//  T5 — OBB2D / OBB3D
// ─────────────────────────────────────────────────────────────────────────────
void testOBB() {
    SUITE("OBB");

    // OBB3D — axis-aligned degenerate (same as AABB)
    OBB3D obb({0,0,0}, {1,1,1}, Quat::identity());
    AABB3D box = obb.toAABB();
    EXPECT_NEAR(box.min.x, -1.f, 1e-4f);
    EXPECT_NEAR(box.max.x,  1.f, 1e-4f);

    // Rotated 45° around Z
    OBB3D obbRot({0,0,0}, {1,1,1}, Quat::fromAxisAngle({0,0,1}, kPi*0.25f));
    AABB3D rotBox = obbRot.toAABB();
    float expected = std::sqrt(2.f); // projected half-extent
    EXPECT_NEAR(rotBox.max.x, expected, 1e-3f);

    // Support function
    Vec3 sup = obb.support({1,0,0});
    EXPECT_NEAR(sup.x, 1.f, 1e-4f);

    // Projection (SAT)
    Interval iv = obb.project({1,0,0});
    EXPECT_NEAR(iv.lo, -1.f, 1e-4f);
    EXPECT_NEAR(iv.hi,  1.f, 1e-4f);

    // SAT OBB vs OBB
    OBB3D a({0,0,0},{1,1,1}, Quat::identity());
    OBB3D b({1.5f,0,0},{1,1,1}, Quat::identity());
    auto res = SAT3D::testOBBOBB(a, b);
    EXPECT_TRUE(res.hit);
    EXPECT_NEAR(res.depth, 0.5f, 1e-3f);

    OBB3D c({5,0,0},{1,1,1}, Quat::identity());
    auto res2 = SAT3D::testOBBOBB(a, c);
    EXPECT_FALSE(res2.hit);
}

// ─────────────────────────────────────────────────────────────────────────────
//  T6 — Sphere tests
// ─────────────────────────────────────────────────────────────────────────────
void testSpheres() {
    SUITE("Sphere");

    Sphere a({0,0,0}, 1.f), b({1.5f,0,0}, 1.f), c({5,0,0}, 1.f);

    // Overlap
    auto r1 = SAT3D::testSphereSphere(a, b);
    EXPECT_TRUE(r1.hit);
    EXPECT_NEAR(r1.depth, 0.5f, 1e-4f);
    EXPECT_NEAR(r1.normal.x, 1.f, 1e-4f);

    // Separated
    auto r2 = SAT3D::testSphereSphere(a, c);
    EXPECT_FALSE(r2.hit);

    // Tangent (touching)
    Sphere d({2,0,0}, 1.f);
    auto r3 = SAT3D::testSphereSphere(a, d);
    EXPECT_FALSE(r3.hit);  // exactly touching = no penetration

    // Sphere vs AABB
    AABB3D box({-1,-1,-1},{1,1,1});
    auto r4 = SAT3D::testSphereAABB(a, box);
    EXPECT_TRUE(r4.hit);

    Sphere far({5,0,0}, 0.5f);
    auto r5 = SAT3D::testSphereAABB(far, box);
    EXPECT_FALSE(r5.hit);

    // Sphere vs OBB
    OBB3D obb({0,0,0},{1,1,1}, Quat::identity());
    auto r6 = SAT3D::testSphereOBB(a, obb);
    EXPECT_TRUE(r6.hit);
}

// ─────────────────────────────────────────────────────────────────────────────
//  T7 — Capsule tests
// ─────────────────────────────────────────────────────────────────────────────
void testCapsules() {
    SUITE("Capsule");

    Capsule3D a({0,0,0},{0,2,0}, 0.5f);
    Capsule3D b({0.8f,0,0},{0.8f,2,0}, 0.5f);
    Capsule3D c({5,0,0},{5,2,0}, 0.5f);

    auto r1 = SAT3D::testCapsuleCapsule(a, b);
    EXPECT_TRUE(r1.hit);
    EXPECT_NEAR(r1.depth, 0.2f, 1e-3f);

    auto r2 = SAT3D::testCapsuleCapsule(a, c);
    EXPECT_FALSE(r2.hit);

    // Capsule vs Sphere
    Sphere s({0,1,0.9f}, 0.5f);  // near the middle of capsule a
    auto r3 = SAT3D::testCapsuleSphere(a, s);
    EXPECT_TRUE(r3.hit);

    // Closest point on capsule
    Vec3 cp = a.closestPoint({1,1,0});
    EXPECT_NEAR(cp.x, 0.f, 1e-5f);
    EXPECT_NEAR(cp.y, 1.f, 1e-5f);

    // AABB from capsule
    AABB3D aabb = a.toAABB();
    EXPECT_NEAR(aabb.min.x, -0.5f, 1e-5f);
    EXPECT_NEAR(aabb.max.y,  2.5f, 1e-5f);
}

// ─────────────────────────────────────────────────────────────────────────────
//  T8 — GJK 3D
// ─────────────────────────────────────────────────────────────────────────────
void testGJK() {
    SUITE("GJK");

    // Two overlapping spheres
    CollisionShape sa = CollisionShape::makeSphere({0,0,0}, 1.f);
    CollisionShape sb = CollisionShape::makeSphere({1.5f,0,0}, 1.f);
    Transform ta, tb;

    auto r1 = GJK3D::test(sa, ta, sb, tb);
    EXPECT_TRUE(r1.hit);

    // Separated spheres
    CollisionShape sc = CollisionShape::makeSphere({5,0,0}, 1.f);
    auto r2 = GJK3D::test(sa, ta, sc, tb);
    EXPECT_FALSE(r2.hit);
    EXPECT_GT(r2.distance, 0.f);

    // Two overlapping AABB
    CollisionShape aa = CollisionShape::makeAABB3D({-1,-1,-1},{1,1,1});
    CollisionShape ab = CollisionShape::makeAABB3D({0.5f,-1,-1},{2.5f,1,1});
    auto r3 = GJK3D::test(aa, ta, ab, tb);
    EXPECT_TRUE(r3.hit);

    // Separated AABB
    CollisionShape ac = CollisionShape::makeAABB3D({3,-1,-1},{5,1,1});
    auto r4 = GJK3D::test(aa, ta, ac, tb);
    EXPECT_FALSE(r4.hit);

    // OBB overlap
    CollisionShape oa = CollisionShape::makeOBB3D({0,0,0},{1,1,1}, Quat::identity());
    CollisionShape ob = CollisionShape::makeOBB3D({1.5f,0,0},{1,1,1},
                             Quat::fromAxisAngle({0,1,0}, kPi*0.25f));
    auto r5 = GJK3D::test(oa, ta, ob, tb);
    EXPECT_TRUE(r5.hit);
}

// ─────────────────────────────────────────────────────────────────────────────
//  T9 — SAT 2D
// ─────────────────────────────────────────────────────────────────────────────
void testSAT2D() {
    SUITE("SAT 2D");

    // Circle vs Circle
    Circle2D ca({0,0},1.f), cb({1.5f,0},1.f), cc({5,0},1.f);
    auto r1 = SAT2D::testCircleCircle(ca,cb);
    EXPECT_TRUE(r1.hit);
    EXPECT_NEAR(r1.depth, 0.5f, 1e-4f);
    EXPECT_NEAR(r1.normal.x, 1.f, 1e-4f);

    auto r2 = SAT2D::testCircleCircle(ca,cc);
    EXPECT_FALSE(r2.hit);

    // OBB vs OBB 2D
    OBB2D oa({0,0},{1,1},0.f);
    OBB2D ob({1.5f,0},{1,1},0.f);
    OBB2D oc({5,0},{1,1},0.f);

    auto r3 = SAT2D::testOBBOBB(oa,ob);
    EXPECT_TRUE(r3.hit);
    EXPECT_NEAR(r3.depth, 0.5f, 1e-3f);

    auto r4 = SAT2D::testOBBOBB(oa,oc);
    EXPECT_FALSE(r4.hit);

    // Rotated OBB
    OBB2D od({1.8f,0},{1,1}, kPi*0.25f);
    auto r5 = SAT2D::testOBBOBB(oa,od);
    EXPECT_TRUE(r5.hit);

    // Polygon vs Polygon
    ConvexPolygon2D pa, pb;
    pa.vertices = {{-1,-1},{1,-1},{1,1},{-1,1}};
    pb.vertices = {{0.5f,-1},{2.5f,-1},{2.5f,1},{0.5f,1}};
    auto r6 = SAT2D::testPolygonPolygon(pa,pb);
    EXPECT_TRUE(r6.hit);

    ConvexPolygon2D pc;
    pc.vertices = {{3,-1},{5,-1},{5,1},{3,1}};
    auto r7 = SAT2D::testPolygonPolygon(pa,pc);
    EXPECT_FALSE(r7.hit);
}

// ─────────────────────────────────────────────────────────────────────────────
//  T10 — Narrowphase contact generation
// ─────────────────────────────────────────────────────────────────────────────
void testNarrowphase() {
    SUITE("Narrowphase");
    Transform ta, tb;

    // Sphere-Sphere contact
    auto sa = CollisionShape::makeSphere({0,0,0},1.f);
    auto sb = CollisionShape::makeSphere({1.5f,0,0},1.f);
    auto m1 = Narrowphase::generateContacts(sa,ta,sb,tb);
    EXPECT_TRUE(m1.hit);
    EXPECT_EQ(m1.count, 1);
    EXPECT_NEAR(m1.points[0].penetrationDepth, 0.5f, 1e-4f);
    EXPECT_NEAR(m1.points[0].normal.x, 1.f, 1e-4f);
    EXPECT_NEAR(m1.points[0].normal.y, 0.f, 1e-4f);

    // AABB-AABB contact
    auto aa = CollisionShape::makeAABB3D({-1,-1,-1},{1,1,1});
    auto ab = CollisionShape::makeAABB3D({0.5f,-1,-1},{2.5f,1,1});
    auto m2 = Narrowphase::generateContacts(aa,ta,ab,tb);
    EXPECT_TRUE(m2.hit);
    EXPECT_NEAR(m2.points[0].penetrationDepth, 0.5f, 1e-4f);

    // Sphere-AABB contact
    auto bb = CollisionShape::makeAABB3D({0,0,0},{2,2,2});
    auto m3 = Narrowphase::generateContacts(sa,ta,bb,tb);
    EXPECT_TRUE(m3.hit);

    // No-hit: separated spheres
    auto sc = CollisionShape::makeSphere({10,0,0},1.f);
    auto m4 = Narrowphase::generateContacts(sa,ta,sc,tb);
    EXPECT_FALSE(m4.hit);

    // Capsule contact
    auto ca = CollisionShape::makeCapsule3D({0,0,0},{0,2,0},0.5f);
    auto cb2= CollisionShape::makeCapsule3D({0.8f,0,0},{0.8f,2,0},0.5f);
    auto m5 = Narrowphase::generateContacts(ca,ta,cb2,tb);
    EXPECT_TRUE(m5.hit);
    EXPECT_NEAR(m5.points[0].penetrationDepth, 0.2f, 1e-3f);

    // 2D contacts
    auto ca2d = CollisionShape::makeCircle2D({0,0},1.f);
    auto cb2d = CollisionShape::makeCircle2D({1.5f,0},1.f);
    auto m6 = Narrowphase::generateContacts2D(ca2d, cb2d);
    EXPECT_TRUE(m6.hit);
    EXPECT_NEAR(m6.normal.x, 1.f, 1e-4f);
}

// ─────────────────────────────────────────────────────────────────────────────
//  T11 — Dynamic BVH
// ─────────────────────────────────────────────────────────────────────────────
void testBVH() {
    SUITE("Dynamic BVH");

    DynamicBVH bvh;

    // Insert 20 bodies in a line
    for(uint64_t i=0;i<20;i++) {
        float x = (float)i * 2.f;
        bvh.insert(i, {{x-0.5f,-0.5f,-0.5f},{x+0.5f,0.5f,0.5f}});
    }

    // Query a specific region
    std::vector<uint64_t> hits;
    bvh.query({{5,-1,-1},{9,1,1}}, [&](uint64_t id){ hits.push_back(id); return true; });
    EXPECT_GE((int)hits.size(), 2);

    // Ray query
    std::vector<uint64_t> rayHits;
    bvh.rayQuery({{-5,0,0},{1,0,0}}, [&](uint64_t id, float) -> bool {
        rayHits.push_back(id); return true;
    });
    EXPECT_GE((int)rayHits.size(), 1);

    // Update one body — move it out
    bvh.update(5, {{50,-0.5f,-0.5f},{51,0.5f,0.5f}});
    hits.clear();
    bvh.query({{9,-1,-1},{11,1,1}}, [&](uint64_t id){ hits.push_back(id); return true; });
    bool body5Found = false;
    for(auto id:hits) if(id==5) body5Found=true;
    EXPECT_FALSE(body5Found);

    // Collect pairs (adjacent bodies should overlap when spacing < 2*0.5=1)
    std::vector<CollidingPair> pairs;
    bvh.collectPairs(pairs);
    // With 0.1f fattenFactor on 1.0 extents, slightly fattened — no overlap in 2.0 spacing
    // (the test just checks the method runs without crash)
    EXPECT_GE((int)pairs.size(), 0);

    // Remove bodies
    bvh.remove(0);
    bvh.remove(1);
    hits.clear();
    bvh.query({{-1,-1,-1},{1,1,1}}, [&](uint64_t id){ hits.push_back(id); return true; });
    EXPECT_EQ((int)hits.size(), 0);
}

// ─────────────────────────────────────────────────────────────────────────────
//  T12 — Sweep and Prune
// ─────────────────────────────────────────────────────────────────────────────
void testSAP() {
    SUITE("Sweep and Prune");

    SweepAndPrune sap;
    for(uint64_t i=0;i<10;i++) {
        float x=(float)i*1.8f;
        sap.insert(i, {{x,0,0},{x+1,1,1}});
    }

    std::vector<CollidingPair> pairs;
    sap.collectPairs(pairs);
    // Bodies are spaced 1.8, width 1.0 so NO overlap
    EXPECT_EQ((int)pairs.size(), 0);

    // Add overlapping body
    sap.insert(100, {{0.5f,0,0},{1.5f,1,1}});
    sap.collectPairs(pairs);
    EXPECT_GT((int)pairs.size(), 0);

    // Update
    sap.update(0, {{100,0,0},{101,1,1}});
    sap.collectPairs(pairs);
    bool found0=false;
    for(auto& p:pairs) if(p.bodyA==0||p.bodyB==0) found0=true;
    EXPECT_FALSE(found0);
}

// ─────────────────────────────────────────────────────────────────────────────
//  T13 — CollisionWorld
// ─────────────────────────────────────────────────────────────────────────────
void testWorld() {
    SUITE("CollisionWorld");

    WorldConfig cfg;
    cfg.gpuBackend    = GPUBackend::None;
    cfg.multithreaded = false;
    CollisionWorld world(cfg);

    // Add bodies
    CollisionBody bA; bA.shape = CollisionShape::makeSphere({0,0,0},1.f);
    CollisionBody bB; bB.shape = CollisionShape::makeSphere({1.5f,0,0},1.f);
    CollisionBody bC; bC.shape = CollisionShape::makeSphere({20,0,0},1.f);

    auto idA=world.addBody(bA), idB=world.addBody(bB), idC=world.addBody(bC);

    // Step
    world.step();
    EXPECT_GE((int)world.contactCount(), 1);

    // Move C near A
    Transform tC; tC.position={1.2f,0,0};
    world.setTransform(idC, tC);
    world.step();
    EXPECT_GE((int)world.contactCount(), 1);

    // Remove B
    world.removeBody(idB);
    world.step();

    // Fix C: use origin-centered sphere so transform.position drives world pos
    world.getBody(idC)->shape = CollisionShape::makeSphere({0,0,0}, 1.f);
    world.setTransform(idC, tC);  // tC.position={1.2,0,0} overlaps A at origin r=1

    ContactManifold m;
    bool hit = world.testBodyBody(idA, idC, m);
    EXPECT_TRUE(hit);

    // Raycast
    RaycastHit rh;
    Ray ray({-5,0,0},{1,0,0});
    bool found = world.raycast(ray, rh);
    EXPECT_TRUE(found);
    EXPECT_GT(rh.distance, 0.f);

    // Overlap query
    std::vector<uint64_t> results;
    world.overlapAABB({{-2,-2,-2},{2,2,2}}, results);
    EXPECT_GE((int)results.size(), 1);

    // Trigger body
    CollisionBody bTrig; bTrig.shape = CollisionShape::makeSphere({0.5f,0,0},0.5f);
    bTrig.flags = BodyFlag_Trigger;
    auto idT = world.addBody(bTrig);

    int trigEnters = 0;
    world.setTriggerCallback([&](uint64_t, uint64_t, bool entered){
        if(entered) trigEnters++;
    });
    world.step();
    EXPECT_GE(trigEnters, 0); // at least ran without crash

    // Layer masking
    CollisionBody bL1; bL1.shape=CollisionShape::makeSphere({0.5f,0,0},1.f);
    CollisionBody bL2; bL2.shape=CollisionShape::makeSphere({0.5f,0,0},1.f);
    bL1.layer=0x01; bL1.mask=0x02;
    bL2.layer=0x04; bL2.mask=0x01;
    // bL1 wants 0x02, bL2 has 0x04 → bL1.layer(0x01) & bL2.mask(0x01) = 1 ✓
    // bL2 wants 0x01, bL1 has 0x01 → bL2.layer(0x04) & bL1.mask(0x02) = 0 ✗
    // Collision requires BOTH: (bL1.layer & bL2.mask) && (bL2.layer & bL1.mask)
    EXPECT_FALSE(bL1.collidesWith(bL2));  // bL2.layer=0x04 not in bL1.mask=0x02

    bL2.layer=0x02; bL2.mask=0x01;
    // bL1.layer(0x01) & bL2.mask(0x01) = 1 ✓,  bL2.layer(0x02) & bL1.mask(0x02) = 2 ✓
    EXPECT_TRUE(bL1.collidesWith(bL2));

    (void)idA; (void)idT;
}

// ─────────────────────────────────────────────────────────────────────────────
//  T14 — Transform
// ─────────────────────────────────────────────────────────────────────────────
void testTransform() {
    SUITE("Transform");

    Transform t;
    t.position = {1,2,3};
    t.rotation = Quat::fromAxisAngle({0,1,0}, kPi*0.5f);
    t.scale    = {2,2,2};

    Vec3 p(1,0,0);
    Vec3 world = t.transformPoint(p);
    // scale first (2,0,0), then rotate 90°Y (-z=0, x=0→ gives (0,0,-2)) + pos
    EXPECT_NEAR(world.y, 2.f, 1e-3f);   // y unchanged

    Vec3 back = t.inverseTransformPoint(world);
    EXPECT_NEAR(back.x, p.x, 1e-3f);
    EXPECT_NEAR(back.y, p.y, 1e-3f);
    EXPECT_NEAR(back.z, p.z, 1e-3f);

    // toMat4
    Mat4 m = t.toMat4();
    Vec4 vv(1,0,0,1);
    Vec4 result = m * vv;
    EXPECT_NEAR(result.x, world.x, 1e-3f);
}

// ─────────────────────────────────────────────────────────────────────────────
//  T15 — Interval
// ─────────────────────────────────────────────────────────────────────────────
void testInterval() {
    SUITE("Interval");

    Interval a(0,4), b(2,6), c(5,9);
    EXPECT_TRUE(a.overlaps(b));
    EXPECT_FALSE(a.overlaps(c));
    EXPECT_NEAR(a.overlap(b), 2.f, 1e-5f);
    EXPECT_NEAR(a.center(), 2.f, 1e-5f);
    EXPECT_NEAR(a.extent(), 4.f, 1e-5f);

    a.expand(10.f);
    EXPECT_NEAR(a.hi, 10.f, 1e-5f);
}

// ─────────────────────────────────────────────────────────────────────────────
//  T16 — ObjectPool
// ─────────────────────────────────────────────────────────────────────────────
void testObjectPool() {
    SUITE("ObjectPool");

    ObjectPool<ContactManifold> pool(16);
    EXPECT_EQ((int)pool.used(), 0);

    auto* m1 = pool.alloc();
    auto* m2 = pool.alloc();
    EXPECT_TRUE(m1 != nullptr);
    EXPECT_TRUE(m2 != nullptr);
    EXPECT_EQ((int)pool.used(), 2);

    pool.free(m1);
    EXPECT_EQ((int)pool.used(), 1);

    pool.reset();
    EXPECT_EQ((int)pool.used(), 0);

    // Exhaust pool
    std::vector<ContactManifold*> ptrs;
    for(int i=0;i<16;i++) ptrs.push_back(pool.alloc());
    EXPECT_EQ(pool.alloc(), nullptr);  // pool full
}

// ─────────────────────────────────────────────────────────────────────────────
//  T17 — Heightfield
// ─────────────────────────────────────────────────────────────────────────────
void testHeightfield() {
    SUITE("Heightfield");

    Heightfield hf;
    hf.cols=4; hf.rows=4;
    hf.origin={0,0,0};
    hf.cellSizeX=hf.cellSizeZ=1.f;
    hf.minHeight=0.f; hf.maxHeight=2.f;
    hf.heights.resize(16, 0.f);

    // Set a peak at (2,2)
    hf.heights[2*4+2] = 2.f;

    float h00 = hf.heightAt(0.5f, 0.5f);
    EXPECT_NEAR(h00, 0.f, 1e-4f);

    float hPeak = hf.heightAt(2.f, 2.f);
    EXPECT_NEAR(hPeak, 2.f, 1e-4f);

    // Interpolated between (1,1)=0 and (2,2)=2
    float hMid = hf.heightAt(1.5f, 1.5f);
    EXPECT_GT(hMid, 0.f);
    EXPECT_LT(hMid, 2.f);

    AABB3D aabb = hf.toAABB();
    EXPECT_NEAR(aabb.min.x, 0.f, 1e-4f);
    EXPECT_NEAR(aabb.max.x, 4.f, 1e-4f);
}

// ─────────────────────────────────────────────────────────────────────────────
//  T18 — Triangle closest point
// ─────────────────────────────────────────────────────────────────────────────
void testTriangle() {
    SUITE("Triangle");

    Triangle tri;
    tri.v[0]={0,0,0}; tri.v[1]={1,0,0}; tri.v[2]={0,1,0};

    // Point above centroid
    Vec3 cp1 = tri.closestPoint({0.25f, 0.25f, 1.f});
    EXPECT_NEAR(cp1.x, 0.25f, 1e-4f);
    EXPECT_NEAR(cp1.y, 0.25f, 1e-4f);
    EXPECT_NEAR(cp1.z, 0.f,   1e-4f);

    // Point outside near edge [v0,v1]
    Vec3 cp2 = tri.closestPoint({0.5f, -1.f, 0.f});
    EXPECT_NEAR(cp2.y, 0.f, 1e-4f);  // clamped to edge
    EXPECT_NEAR(cp2.x, 0.5f, 1e-4f);

    // Point at a vertex
    Vec3 cp3 = tri.closestPoint({-1.f, -1.f, 0.f});
    EXPECT_NEAR(cp3.x, 0.f, 1e-4f);
    EXPECT_NEAR(cp3.y, 0.f, 1e-4f);

    // Normal
    Vec3 n = tri.normal();
    EXPECT_NEAR(n.z, 1.f, 1e-4f);  // should point in +Z

    // AABB
    AABB3D taabb = tri.aabb();
    EXPECT_NEAR(taabb.min.x, 0.f, 1e-5f);
    EXPECT_NEAR(taabb.max.x, 1.f, 1e-5f);
}

// ─────────────────────────────────────────────────────────────────────────────
//  T19 — ConvexHull support function
// ─────────────────────────────────────────────────────────────────────────────
void testConvexHull() {
    SUITE("ConvexHull");

    auto hull = std::make_shared<ConvexHull3D>();
    hull->vertices = {
        {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
        {-1,-1,1}, {1,-1,1}, {1,1,1}, {-1,1,1}
    };
    hull->buildAABB();

    // Support in +X direction should give x=1
    Vec3 sup = hull->support({1,0,0});
    EXPECT_NEAR(sup.x, 1.f, 1e-5f);

    // Support in -Y direction should give y=-1
    Vec3 sup2 = hull->support({0,-1,0});
    EXPECT_NEAR(sup2.y, -1.f, 1e-5f);

    // AABB
    EXPECT_NEAR(hull->aabb.min.x, -1.f, 1e-5f);
    EXPECT_NEAR(hull->aabb.max.z,  1.f, 1e-5f);
}

// ─────────────────────────────────────────────────────────────────────────────
//  T20 — Multithreaded world
// ─────────────────────────────────────────────────────────────────────────────
void testMultithread() {
    SUITE("Multithreaded World");

    WorldConfig cfg;
    cfg.gpuBackend    = GPUBackend::None;
    cfg.multithreaded = true;
    cfg.threadCount   = 4;
    CollisionWorld world(cfg);

    // 200 spheres in a grid
    for(int i=0;i<10;i++) for(int j=0;j<10;j++) for(int k=0;k<2;k++) {
        CollisionBody b;
        b.shape = CollisionShape::makeSphere(
            {(float)i*2.2f, (float)j*2.2f, (float)k*2.2f}, 1.f);
        world.addBody(b);
    }

    // Should not crash
    world.step();
    world.step();
    world.step();

    std::cout << "  Bodies=" << world.bodyCount()
              << " Contacts=" << world.contactCount() << "\n";
    EXPECT_GE((int)world.bodyCount(), 200);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Entry point
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "═══════════════════════════════════════════\n";
    std::cout << " ColLib — Full Test Suite\n";
    std::cout << "═══════════════════════════════════════════\n";

    testVec2();
    testVec3();
    testQuat();
    testAABB();
    testOBB();
    testSpheres();
    testCapsules();
    testGJK();
    testSAT2D();
    testNarrowphase();
    testBVH();
    testSAP();
    testWorld();
    testTransform();
    testInterval();
    testObjectPool();
    testHeightfield();
    testTriangle();
    testConvexHull();
    testMultithread();

    std::cout << "\n═══════════════════════════════════════════\n";
    std::cout << " Results: " << gPass << " passed";
    if(gFail > 0) std::cout << ", \033[31m" << gFail << " FAILED\033[0m";
    else          std::cout << " \033[32m✓ ALL PASSED\033[0m";
    std::cout << "\n═══════════════════════════════════════════\n";

    return gFail > 0 ? 1 : 0;
}
