// tests/test_main.cpp — test suite complet, zero-dependency
// Run: ./test_collision_lib
// Convention: ASSERT_* macros, each test function returns bool

#include <collision/collision.h>
#include <iostream>
#include <cmath>
#include <sstream>
#include <vector>
#include <functional>
#include <iomanip>

using namespace col;

// ─────────────────────────────────────────────────────────────────────────────
//  Micro test framework
// ─────────────────────────────────────────────────────────────────────────────
struct TestRunner {
    int passed=0, failed=0;
    std::vector<std::string> failures;

    void run(const std::string& name, std::function<void()> fn) {
        try { fn(); passed++; std::cout << "  \033[32m✓\033[0m " << name << "\n"; }
        catch(const std::exception& e) {
            failed++;
            failures.push_back(name + ": " + e.what());
            std::cout << "  \033[31m✗\033[0m " << name << "\n    → " << e.what() << "\n";
        }
    }
    bool summary() {
        std::cout << "\n── " << passed << " passed, " << failed << " failed ──\n";
        return failed == 0;
    }
};

// Assertion helper: throws on failure
static void check(bool cond, const char* msg, const char* file, int line) {
    if(!cond) {
        std::ostringstream ss;
        ss << msg << " (" << file << ":" << line << ")";
        throw std::runtime_error(ss.str());
    }
}
static void checkClose(float a, float b, float eps, const char* msg, const char* file, int line) {
    if(std::abs(a-b) > eps) {
        std::ostringstream ss;
        ss << msg << " [got " << a << " expected " << b << " ±" << eps << "]"
           << " (" << file << ":" << line << ")";
        throw std::runtime_error(ss.str());
    }
}

#define ASSERT(cond)               check((cond), #cond, __FILE__, __LINE__)
#define ASSERT_CLOSE(a,b,eps)      checkClose((a),(b),(eps), #a "==" #b, __FILE__, __LINE__)
#define ASSERT_HIT(m)              check((m).hit,   "Expected collision HIT", __FILE__, __LINE__)
#define ASSERT_MISS(m)             check(!(m).hit,  "Expected collision MISS", __FILE__, __LINE__)
#define ASSERT_VEC3_CLOSE(a,b,eps) do { \
    check(std::abs((a).x-(b).x)<(eps) && \
          std::abs((a).y-(b).y)<(eps) && \
          std::abs((a).z-(b).z)<(eps), \
          "Vec3 mismatch: " #a " != " #b, __FILE__, __LINE__); \
} while(0)

// ═════════════════════════════════════════════════════════════════════════════
//  MATH TESTS
// ═════════════════════════════════════════════════════════════════════════════
void testMath(TestRunner& t) {
    std::cout << "\n[Math]\n";

    t.run("Vec3 dot product", []{
        Vec3 a(1,0,0), b(0,1,0);
        ASSERT_CLOSE(a.dot(b), 0.f, kEpsilon);
        ASSERT_CLOSE(a.dot(a), 1.f, kEpsilon);
    });

    t.run("Vec3 cross product", []{
        Vec3 x(1,0,0), y(0,1,0);
        Vec3 z = x.cross(y);
        ASSERT_VEC3_CLOSE(z, Vec3(0,0,1), 1e-5f);
    });

    t.run("Vec3 normalize", []{
        Vec3 v(3,0,0);
        Vec3 n = v.normalized();
        ASSERT_VEC3_CLOSE(n, Vec3(1,0,0), 1e-5f);
        ASSERT_CLOSE(n.length(), 1.f, 1e-5f);
    });

    t.run("Quat rotate", []{
        Quat q = Quat::fromAxisAngle({0,1,0}, kPi*0.5f);
        Vec3 v = q.rotate({1,0,0});
        ASSERT_VEC3_CLOSE(v, Vec3(0,0,-1), 1e-5f);
    });

    t.run("Quat to Mat3 roundtrip", []{
        Quat q = Quat::fromAxisAngle({1,1,0}, kPi/3.f);
        Mat3 m = q.toMat3();
        Vec3 v(1,2,3);
        Vec3 vq = q.rotate(v);
        Vec3 vm = m * v;
        ASSERT_VEC3_CLOSE(vq, vm, 1e-4f);
    });

    t.run("Quat slerp endpoints", []{
        Quat a = Quat::identity();
        Quat b = Quat::fromAxisAngle({0,1,0}, kPi*0.5f);
        Quat s0 = Quat::slerp(a,b,0.f);
        Quat s1 = Quat::slerp(a,b,1.f);
        ASSERT_CLOSE(s0.w, a.w, 1e-4f);
        ASSERT_CLOSE(s1.w, b.w, 1e-4f);
    });

    t.run("AABB3D merge", []{
        AABB3D a({-1,-1,-1},{0,0,0});
        AABB3D b({0,0,0},{1,1,1});
        AABB3D m = AABB3D::merge(a,b);
        ASSERT_VEC3_CLOSE(m.min, Vec3(-1,-1,-1), 1e-5f);
        ASSERT_VEC3_CLOSE(m.max, Vec3( 1, 1, 1), 1e-5f);
    });

    t.run("AABB3D raycast hit", []{
        AABB3D b({-1,-1,-1},{1,1,1});
        Ray r({-5,0,0},{1,0,0});
        float t; bool hit = b.raycast(r,t);
        ASSERT(hit);
        ASSERT_CLOSE(t, 4.f, 1e-4f);
    });

    t.run("AABB3D raycast miss", []{
        AABB3D b({-1,-1,-1},{1,1,1});
        Ray r({-5,5,0},{1,0,0});
        float t; ASSERT(!b.raycast(r,t));
    });

    t.run("Interval overlap", []{
        Interval a(0,2), b(1,3);
        ASSERT(a.overlaps(b));
        ASSERT_CLOSE(a.overlap(b), 1.f, 1e-5f);
    });

    t.run("Interval separated", []{
        Interval a(0,1), b(2,3);
        ASSERT(!a.overlaps(b));
    });

    t.run("Transform roundtrip", []{
        Transform t;
        t.position = {1,2,3};
        t.rotation = Quat::fromAxisAngle({0,1,0}, kPi/4.f);
        t.scale    = {2,2,2};
        Vec3 p(1,0,0);
        Vec3 world = t.transformPoint(p);
        Vec3 back  = t.inverseTransformPoint(world);
        ASSERT_VEC3_CLOSE(back, p, 1e-4f);
    });
}

// ═════════════════════════════════════════════════════════════════════════════
//  SHAPE TESTS
// ═════════════════════════════════════════════════════════════════════════════
void testShapes(TestRunner& t) {
    std::cout << "\n[Shapes]\n";

    t.run("Sphere support", []{
        Sphere s({0,0,0}, 2.f);
        Vec3 sup = s.support({1,0,0});
        ASSERT_VEC3_CLOSE(sup, Vec3(2,0,0), 1e-5f);
        sup = s.support({0,1,0});
        ASSERT_VEC3_CLOSE(sup, Vec3(0,2,0), 1e-5f);
    });

    t.run("OBB3D support aligned", []{
        // OBB centered at origin, halfExtents=(1,2,3), identity rotation
        // support in +X direction: maximizes dot(v, dir) over all 8 corners
        // The corner with max X is (1, ±2, ±3) — support picks max along dir=(1,0,0)
        // so it returns center + axes[0]*1 + axes[1]*(-2) + axes[2]*(-3)
        // but we only care that the X component equals halfExtents.x
        OBB3D obb({0,0,0}, {1,2,3}, Quat::identity());
        Vec3 sup = obb.support({1,0,0});
        ASSERT_CLOSE(sup.x, 1.f, 1e-4f);  // X component = halfExtent.x
        sup = obb.support({0,1,0});
        ASSERT_CLOSE(sup.y, 2.f, 1e-4f);  // Y component = halfExtent.y
        sup = obb.support({0,0,1});
        ASSERT_CLOSE(sup.z, 3.f, 1e-4f);  // Z component = halfExtent.z
    });

    t.run("OBB3D project", []{
        OBB3D obb({0,0,0}, {1,1,1}, Quat::identity());
        Interval iv = obb.project({1,0,0});
        ASSERT_CLOSE(iv.lo, -1.f, 1e-5f);
        ASSERT_CLOSE(iv.hi,  1.f, 1e-5f);
    });

    t.run("Capsule3D closestPoint segment", []{
        Capsule3D c({0,0,0},{0,4,0}, 0.5f);
        Vec3 pt = c.closestPoint({2,2,0});
        ASSERT_VEC3_CLOSE(pt, Vec3(0,2,0), 1e-5f);  // clamped to segment
    });

    t.run("Capsule3D closestPoint at end", []{
        Capsule3D c({0,0,0},{0,4,0}, 0.5f);
        Vec3 pt = c.closestPoint({1,10,0});
        ASSERT_VEC3_CLOSE(pt, Vec3(0,4,0), 1e-5f);  // clamped to b
    });

    t.run("Triangle closestPoint inside", []{
        Triangle tri;
        tri.v[0]={0,0,0}; tri.v[1]={2,0,0}; tri.v[2]={0,2,0};
        Vec3 pt = tri.closestPoint({0.5f,0.5f,0.f});
        ASSERT_VEC3_CLOSE(pt, Vec3(0.5f,0.5f,0), 1e-5f);
    });

    t.run("Triangle normal", []{
        Triangle tri;
        tri.v[0]={0,0,0}; tri.v[1]={1,0,0}; tri.v[2]={0,1,0};
        Vec3 n = tri.normal();
        ASSERT_VEC3_CLOSE(n, Vec3(0,0,1), 1e-5f);
    });

    t.run("CollisionShape worldAABB sphere", []{
        CollisionShape s = CollisionShape::makeSphere({0,0,0}, 1.f);
        Transform t;
        t.position = {5,0,0};
        AABB3D aabb = s.worldAABB(t);
        ASSERT_VEC3_CLOSE(aabb.min, Vec3(4,-1,-1), 1e-4f);
        ASSERT_VEC3_CLOSE(aabb.max, Vec3(6, 1, 1), 1e-4f);
    });

    t.run("CollisionShape worldAABB capsule", []{
        CollisionShape s = CollisionShape::makeCapsule3D({0,0,0},{0,2,0}, 0.5f);
        Transform t;
        AABB3D aabb = s.worldAABB(t);
        ASSERT(aabb.min.y <= -0.5f);
        ASSERT(aabb.max.y >=  2.5f);
    });
}

// ═════════════════════════════════════════════════════════════════════════════
//  NARROWPHASE TESTS
// ═════════════════════════════════════════════════════════════════════════════
void testNarrowphase(TestRunner& t) {
    std::cout << "\n[Narrowphase]\n";

    // ── SAT Sphere-Sphere ────────────────────────────────────────────────────
    t.run("SAT sphere-sphere overlapping", []{
        auto r = SAT3D::testSphereSphere({Vec3(0),1.f},{Vec3(1.5f,0,0),1.f});
        ASSERT_HIT(r);
        ASSERT_CLOSE(r.depth, 0.5f, 1e-4f);
        ASSERT_VEC3_CLOSE(r.normal, Vec3(1,0,0), 1e-4f);
    });

    t.run("SAT sphere-sphere touching", []{
        auto r = SAT3D::testSphereSphere({Vec3(0),1.f},{Vec3(2,0,0),1.f});
        ASSERT_CLOSE(r.depth, 0.f, 1e-4f);
    });

    t.run("SAT sphere-sphere separated", []{
        auto r = SAT3D::testSphereSphere({Vec3(0),1.f},{Vec3(3,0,0),1.f});
        ASSERT_MISS(r);
    });

    t.run("SAT sphere-sphere concentric", []{
        // Exactly same center — should not crash, should return a valid normal
        auto r = SAT3D::testSphereSphere({Vec3(0),1.f},{Vec3(0),1.f});
        ASSERT_HIT(r);
        ASSERT_CLOSE(r.normal.length(), 1.f, 1e-4f);
    });

    // ── SAT AABB-AABB ────────────────────────────────────────────────────────
    t.run("SAT AABB-AABB overlap X axis", []{
        AABB3D a({-2,-1,-1},{0,1,1});
        AABB3D b({-0.5f,-1,-1},{1.5f,1,1});
        auto r = SAT3D::testAABBAABB(a,b);
        ASSERT_HIT(r);
        ASSERT_CLOSE(r.depth, 0.5f, 1e-4f);
    });

    t.run("SAT AABB-AABB separated X", []{
        AABB3D a({-1,-1,-1},{0,1,1});
        AABB3D b({1,-1,-1},{2,1,1});
        ASSERT_MISS(SAT3D::testAABBAABB(a,b));
    });

    t.run("SAT AABB-AABB one inside other", []{
        AABB3D big({-5,-5,-5},{5,5,5});
        AABB3D small({-1,-1,-1},{1,1,1});
        auto r = SAT3D::testAABBAABB(big,small);
        ASSERT_HIT(r);
    });

    // ── SAT OBB-OBB ──────────────────────────────────────────────────────────
    t.run("SAT OBB-OBB axis-aligned hit", []{
        OBB3D a({0,0,0},{1,1,1},Quat::identity());
        OBB3D b({1.5f,0,0},{1,1,1},Quat::identity());
        auto r = SAT3D::testOBBOBB(a,b);
        ASSERT_HIT(r);
        ASSERT_CLOSE(r.depth, 0.5f, 1e-4f);
    });

    t.run("SAT OBB-OBB rotated no hit", []{
        OBB3D a({0,0,0},{1,1,1},Quat::identity());
        OBB3D b({4,0,0},{1,1,1},Quat::fromAxisAngle({0,1,0},kPi/4.f));
        ASSERT_MISS(SAT3D::testOBBOBB(a,b));
    });

    // ── SAT Sphere-AABB ───────────────────────────────────────────────────────
    t.run("SAT sphere-AABB hit", []{
        Sphere s({1.5f,0,0}, 1.f);
        AABB3D b({-1,-1,-1},{1,1,1});
        auto r = SAT3D::testSphereAABB(s,b);
        ASSERT_HIT(r);
        ASSERT_CLOSE(r.depth, 0.5f, 1e-4f);
    });

    t.run("SAT sphere-AABB sphere inside box", []{
        Sphere s({0,0,0}, 0.5f);
        AABB3D b({-2,-2,-2},{2,2,2});
        auto r = SAT3D::testSphereAABB(s,b);
        ASSERT_HIT(r);
    });

    t.run("SAT sphere-AABB miss", []{
        Sphere s({5,0,0}, 1.f);
        AABB3D b({-1,-1,-1},{1,1,1});
        ASSERT_MISS(SAT3D::testSphereAABB(s,b));
    });

    // ── SAT Capsule ───────────────────────────────────────────────────────────
    t.run("SAT capsule-sphere hit", []{
        Capsule3D c({0,-2,0},{0,2,0},0.5f);
        Sphere    s({0.9f,0,0}, 0.5f);
        auto r = SAT3D::testCapsuleSphere(c,s);
        ASSERT_HIT(r);
    });

    t.run("SAT capsule-capsule parallel hit", []{
        Capsule3D a({0,-2,0},{0,2,0},0.5f);
        Capsule3D b({0.8f,-2,0},{0.8f,2,0},0.5f);
        auto r = SAT3D::testCapsuleCapsule(a,b);
        ASSERT_HIT(r);
        ASSERT_CLOSE(r.depth, 0.2f, 1e-4f);
    });

    t.run("SAT capsule-capsule crossed miss", []{
        Capsule3D a({-5,0,0},{5,0,0},0.3f);
        Capsule3D b({0,-5,5},{0,5,5},0.3f);  // offset in Z
        ASSERT_MISS(SAT3D::testCapsuleCapsule(a,b));
    });

    // ── Narrowphase dispatcher ────────────────────────────────────────────────
    t.run("Narrowphase dispatcher sphere-sphere", []{
        auto sa = CollisionShape::makeSphere({0,0,0},1.f);
        auto sb = CollisionShape::makeSphere({1.5f,0,0},1.f);
        Transform ta, tb;
        auto m = Narrowphase::generateContacts(sa,ta,sb,tb);
        ASSERT_HIT(m);
        ASSERT(m.count == 1);
    });

    t.run("Narrowphase dispatcher AABB-AABB", []{
        auto sa = CollisionShape::makeAABB3D({-1,-1,-1},{1,1,1});
        auto sb = CollisionShape::makeAABB3D({0.5f,-1,-1},{2.5f,1,1});
        Transform ta, tb;
        ASSERT_HIT(Narrowphase::generateContacts(sa,ta,sb,tb));
    });

    t.run("Narrowphase contact normal direction", []{
        // A at origin, B to the right — normal should point right
        auto sa = CollisionShape::makeSphere({0,0,0},1.f);
        auto sb = CollisionShape::makeSphere({1.5f,0,0},1.f);
        Transform ta, tb;
        auto m = Narrowphase::generateContacts(sa,ta,sb,tb);
        ASSERT_HIT(m);
        ASSERT(m.points[0].normal.x > 0.f);
    });

    // ── 2D SAT ─────────────────────────────────────────────────────────────
    t.run("SAT 2D circle-circle hit", []{
        Circle2D a({0,0},1.f), b({1.5f,0},1.f);
        auto r = SAT2D::testCircleCircle(a,b);
        ASSERT_HIT(r);
        ASSERT_CLOSE(r.depth, 0.5f, 1e-4f);
    });

    t.run("SAT 2D OBB-OBB hit", []{
        OBB2D a({0,0},{1,1},0.f), b({1.5f,0},{1,1},0.f);
        auto r = SAT2D::testOBBOBB(a,b);
        ASSERT_HIT(r);
    });

    t.run("SAT 2D OBB-OBB rotated miss", []{
        OBB2D a({0,0},{0.5f,2.f},0.f);
        OBB2D b({5,0},{0.5f,2.f},0.f);
        ASSERT_MISS(SAT2D::testOBBOBB(a,b));
    });
}

// ═════════════════════════════════════════════════════════════════════════════
//  BROADPHASE TESTS
// ═════════════════════════════════════════════════════════════════════════════
void testBroadphase(TestRunner& t) {
    std::cout << "\n[Broadphase]\n";

    t.run("BVH insert and query hit", []{
        DynamicBVH bvh;
        bvh.insert(1, {{-1,-1,-1},{1,1,1}});
        bvh.insert(2, {{0.5f,-1,-1},{2.5f,1,1}});
        std::vector<uint64_t> found;
        bvh.query({{-0.5f,-0.5f,-0.5f},{0.5f,0.5f,0.5f}},
                  [&](uint64_t id){ found.push_back(id); return true; });
        ASSERT(found.size() >= 1);
    });

    t.run("BVH query no overlap", []{
        DynamicBVH bvh;
        bvh.insert(1, {{-5,-5,-5},{-4,-4,-4}});
        std::vector<uint64_t> found;
        bvh.query({{4,4,4},{5,5,5}},
                  [&](uint64_t id){ found.push_back(id); return true; });
        ASSERT(found.empty());
    });

    t.run("BVH collect pairs overlap", []{
        DynamicBVH bvh;
        bvh.insert(1, {{-1,-1,-1},{1,1,1}});
        bvh.insert(2, {{0,-1,-1},{2,1,1}});  // overlaps 1
        bvh.insert(3, {{10,-1,-1},{12,1,1}}); // isolated
        std::vector<CollidingPair> pairs;
        bvh.collectPairs(pairs);
        ASSERT(pairs.size() == 1);
        bool found = (pairs[0].bodyA==1&&pairs[0].bodyB==2) ||
                     (pairs[0].bodyA==2&&pairs[0].bodyB==1);
        ASSERT(found);
    });

    t.run("BVH remove body", []{
        DynamicBVH bvh;
        bvh.insert(1,{{-1,-1,-1},{1,1,1}});
        bvh.insert(2,{{0,-1,-1},{2,1,1}});
        bvh.remove(2);
        std::vector<CollidingPair> pairs;
        bvh.collectPairs(pairs);
        ASSERT(pairs.empty());
    });

    t.run("BVH update moves AABB", []{
        DynamicBVH bvh;
        bvh.insert(1,{{-1,-1,-1},{1,1,1}});
        bvh.insert(2,{{10,-1,-1},{12,1,1}});
        // No pairs initially
        std::vector<CollidingPair> pairs;
        bvh.collectPairs(pairs);
        ASSERT(pairs.empty());
        // Move body 2 to overlap body 1
        bvh.update(2,{{0,-1,-1},{2,1,1}});
        bvh.collectPairs(pairs);
        ASSERT(pairs.size() == 1);
    });

    t.run("BVH ray query", []{
        DynamicBVH bvh;
        bvh.insert(1,{{-1,-1,-1},{1,1,1}});
        bvh.insert(2,{{5,-1,-1},{7,1,1}});
        std::vector<uint64_t> hits;
        Ray ray({-10,0,0},{1,0,0});
        bvh.rayQuery(ray,[&](uint64_t id,float){ hits.push_back(id); return true; });
        ASSERT(hits.size() == 2); // both hit along X axis
    });

    t.run("BVH stress: 1000 bodies", []{
        DynamicBVH bvh;
        for(int i=0;i<1000;i++) {
            float x = (float)(i%100)*4.f;   // spacing=4, size=2 → 2 units gap
            float y = (float)(i/100)*4.f;
            bvh.insert((uint64_t)i+1, {{x-1,y-1,-1},{x+1,y+1,1}});
        }
        std::vector<CollidingPair> pairs;
        bvh.collectPairs(pairs);
        ASSERT(pairs.empty());
    });

    t.run("SAP collect pairs", []{
        SweepAndPrune sap;
        sap.insert(1,{{-1,-1,-1},{1,1,1}});
        sap.insert(2,{{0.5f,-1,-1},{2.5f,1,1}});
        sap.insert(3,{{10,-1,-1},{12,1,1}});
        std::vector<CollidingPair> pairs;
        sap.collectPairs(pairs);
        ASSERT(pairs.size() == 1);
    });

    t.run("SAP no pairs after separation", []{
        SweepAndPrune sap;
        sap.insert(1,{{-1,-1,-1},{1,1,1}});
        sap.insert(2,{{0.5f,-1,-1},{2.5f,1,1}});
        sap.update(2,{{5,-1,-1},{7,1,1}});
        std::vector<CollidingPair> pairs;
        sap.collectPairs(pairs);
        ASSERT(pairs.empty());
    });
}

// ═════════════════════════════════════════════════════════════════════════════
//  WORLD TESTS
// ═════════════════════════════════════════════════════════════════════════════
void testWorld(TestRunner& t) {
    std::cout << "\n[CollisionWorld]\n";

    auto makeWorld = []{
        WorldConfig cfg;
        cfg.gpuBackend    = GPUBackend::None;
        cfg.multithreaded = false;
        return CollisionWorld(cfg);
    };

    t.run("World add/remove body", [&]{
        auto w = makeWorld();
        CollisionBody b; b.shape=CollisionShape::makeSphere({0,0,0},1.f);
        auto id = w.addBody(b);
        ASSERT(w.bodyCount() == 1);
        w.removeBody(id);
        ASSERT(w.bodyCount() == 0);
    });

    t.run("World step detects contact", [&]{
        auto w = makeWorld();
        CollisionBody a; a.shape=CollisionShape::makeSphere({0,0,0},1.f);
        CollisionBody b; b.shape=CollisionShape::makeSphere({1.5f,0,0},1.f);
        w.addBody(a); w.addBody(b);
        w.step();
        ASSERT(w.contactCount() >= 1);
    });

    t.run("World step no contact when separated", [&]{
        auto w = makeWorld();
        CollisionBody a; a.shape=CollisionShape::makeSphere({0,0,0},1.f);
        CollisionBody b; b.shape=CollisionShape::makeSphere({10,0,0},1.f);
        w.addBody(a); w.addBody(b);
        w.step();
        ASSERT(w.contactCount() == 0);
    });

    t.run("World contact callback fired", [&]{
        auto w = makeWorld();
        int cbCount = 0;
        w.setContactCallback([&](const ContactManifold&,
                                  const CollisionBody&,
                                  const CollisionBody&){ cbCount++; });
        CollisionBody a; a.shape=CollisionShape::makeSphere({0,0,0},1.f);
        CollisionBody b; b.shape=CollisionShape::makeSphere({1.5f,0,0},1.f);
        w.addBody(a); w.addBody(b);
        w.step();
        ASSERT(cbCount >= 1);
    });

    t.run("World trigger callback no contact response", [&]{
        auto w = makeWorld();
        int trigCount = 0;
        w.setTriggerCallback([&](uint64_t,uint64_t,bool){ trigCount++; });
        CollisionBody a; a.shape=CollisionShape::makeSphere({0,0,0},1.f);
        a.flags |= BodyFlag_Trigger;
        CollisionBody b; b.shape=CollisionShape::makeSphere({1.5f,0,0},1.f);
        w.addBody(a); w.addBody(b);
        w.step();
        ASSERT(trigCount >= 1);
    });

    t.run("World layer/mask filtering", [&]{
        auto w = makeWorld();
        CollisionBody a; a.shape=CollisionShape::makeSphere({0,0,0},1.f);
        a.layer=0x01; a.mask=0x02;
        CollisionBody b; b.shape=CollisionShape::makeSphere({1.5f,0,0},1.f);
        b.layer=0x04; b.mask=0x08;  // different layer, incompatible
        w.addBody(a); w.addBody(b);
        w.step();
        ASSERT(w.contactCount() == 0);
    });

    t.run("World setTransform updates collision", [&]{
        auto w = makeWorld();
        CollisionBody a; a.shape=CollisionShape::makeSphere({0,0,0},1.f);
        CollisionBody b; b.shape=CollisionShape::makeSphere({50,0,0},1.f); // far away
        w.addBody(a);
        auto idB = w.addBody(b);
        w.step();
        ASSERT(w.contactCount() == 0);
        // Move B squarely inside A
        Transform tr; tr.position={0.5f,0,0};
        w.setTransform(idB, tr);
        w.step();
        ASSERT(w.contactCount() >= 1);
    });

    t.run("World raycast hit", [&]{
        auto w = makeWorld();
        CollisionBody a; a.shape=CollisionShape::makeAABB3D({-1,-1,-1},{1,1,1});
        w.addBody(a);
        Ray ray({-5,0,0},{1,0,0});
        RaycastHit hit;
        ASSERT(w.raycast(ray,hit));
        ASSERT_CLOSE(hit.distance, 4.f, 0.1f);
    });

    t.run("World raycast miss", [&]{
        auto w = makeWorld();
        CollisionBody a; a.shape=CollisionShape::makeAABB3D({-1,-1,-1},{1,1,1});
        w.addBody(a);
        Ray ray({-5,100,0},{1,0,0});
        RaycastHit hit;
        ASSERT(!w.raycast(ray,hit));
    });

    t.run("World overlap AABB query", [&]{
        auto w = makeWorld();
        CollisionBody a; a.shape=CollisionShape::makeSphere({0,0,0},1.f);
        CollisionBody b; b.shape=CollisionShape::makeSphere({5,0,0},1.f);
        w.addBody(a); w.addBody(b);
        std::vector<uint64_t> results;
        w.overlapAABB({{-2,-2,-2},{2,2,2}}, results);
        ASSERT(results.size() == 1);
    });

    t.run("World static-static no contact", [&]{
        auto w = makeWorld();
        CollisionBody a; a.shape=CollisionShape::makeSphere({0,0,0},1.f);
        a.flags |= BodyFlag_Static;
        CollisionBody b; b.shape=CollisionShape::makeSphere({1.5f,0,0},1.f);
        b.flags |= BodyFlag_Static;
        w.addBody(a); w.addBody(b);
        w.step();
        // Static-static pairs are skipped by pipeline
        ASSERT(w.contactCount() == 0);
    });

    t.run("World many bodies stress", [&]{
        WorldConfig cfg; cfg.gpuBackend=GPUBackend::None; cfg.multithreaded=false;
        CollisionWorld w(cfg);
        for(int i=0;i<200;i++){
            CollisionBody b;
            b.shape=CollisionShape::makeSphere({0,0,0},1.f); // sphere at local origin
            b.transform.position = {(float)i*4.f, 0, 0}; // spaced 4 units, radius=1 → no overlap
            w.addBody(b);
        }
        w.step();
        ASSERT(w.bodyCount() == 200);
        ASSERT(w.contactCount() == 0);
    });
}

// ═════════════════════════════════════════════════════════════════════════════
//  EDGE CASE TESTS
// ═════════════════════════════════════════════════════════════════════════════
void testEdgeCases(TestRunner& t) {
    std::cout << "\n[Edge cases]\n";

    t.run("Empty world step", []{
        WorldConfig cfg; cfg.gpuBackend=GPUBackend::None;
        CollisionWorld w(cfg);
        w.step(); // must not crash
    });

    t.run("Single body world step", []{
        WorldConfig cfg; cfg.gpuBackend=GPUBackend::None;
        CollisionWorld w(cfg);
        CollisionBody b; b.shape=CollisionShape::makeSphere({0,0,0},1.f);
        w.addBody(b);
        w.step();
        ASSERT(w.contactCount() == 0);
    });

    t.run("Sphere zero radius", []{
        Sphere s({0,0,0},0.f);
        auto r = SAT3D::testSphereSphere(s, Sphere({0.5f,0,0},0.5f));
        ASSERT(r.depth <= 0.5f);
    });

    t.run("Degenerate capsule (zero length)", []{
        Capsule3D c({0,0,0},{0,0,0},1.f);  // point capsule
        Vec3 pt = c.closestPoint({1,0,0});
        ASSERT_VEC3_CLOSE(pt, Vec3(0,0,0), 1e-5f);
    });

    t.run("AABB containing point boundary", []{
        AABB3D b({-1,-1,-1},{1,1,1});
        ASSERT(b.contains({1,0,0}));   // on boundary
        ASSERT(!b.contains({1.01f,0,0})); // just outside
    });

    t.run("BVH single node query", []{
        DynamicBVH bvh;
        bvh.insert(1, {{-1,-1,-1},{1,1,1}});
        std::vector<CollidingPair> pairs;
        bvh.collectPairs(pairs);
        ASSERT(pairs.empty()); // single node, no pair
    });

    t.run("Vec3 zero length normalize", []{
        Vec3 v(0,0,0);
        Vec3 n = v.normalized();
        // Must not NaN
        ASSERT(n.lengthSq() < 1.f); // returns zero vector
    });

    t.run("Interval empty (hi < lo)", []{
        Interval a(2,1); // invalid
        ASSERT(!a.overlaps({0,3})); // should handle gracefully
    });
}

// ═════════════════════════════════════════════════════════════════════════════
//  MAIN
// ═════════════════════════════════════════════════════════════════════════════
int main() {
    std::cout << "\033[1m══════════════════════════════════════════\n";
    std::cout << "  ColLib — Full Test Suite\n";
    std::cout << "══════════════════════════════════════════\033[0m\n";

    TestRunner t;
    testMath(t);
    testShapes(t);
    testNarrowphase(t);
    testBroadphase(t);
    testWorld(t);
    testEdgeCases(t);

    bool ok = t.summary();
    if(!ok) {
        std::cout << "\nFailed tests:\n";
        for(auto& f : t.failures) std::cout << "  • " << f << "\n";
    }
    return ok ? 0 : 1;
}
