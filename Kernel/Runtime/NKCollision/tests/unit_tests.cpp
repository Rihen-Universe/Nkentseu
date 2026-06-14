// tests/unit_tests.cpp
// Standalone test suite — no external framework needed
// Compile: g++ -std=c++17 -O2 -I ../include ../src/shapes.cpp unit_tests.cpp -lpthread -o run_tests

#include <collision/collision.h>
#include <iostream>
#include <cmath>
#include <cassert>
#include <sstream>
#include <functional>
#include <vector>

using namespace col;

// ─────────────────────────────────────────────────────────────────────────────
//  Minimal test harness
// ─────────────────────────────────────────────────────────────────────────────
static int sPass=0, sFail=0;
#define EXPECT_TRUE(expr) do { \
    if(!(expr)) { \
        std::cerr << "  FAIL: " #expr " (line " << __LINE__ << ")\n"; sFail++; \
    } else { sPass++; } \
} while(0)
#define EXPECT_FALSE(expr)    EXPECT_TRUE(!(expr))
#define EXPECT_NEAR(a,b,eps)  EXPECT_TRUE(std::abs((a)-(b)) < (eps))
#define EXPECT_GT(a,b)        EXPECT_TRUE((a)>(b))
#define EXPECT_LT(a,b)        EXPECT_TRUE((a)<(b))
#define TEST(name) void name(); struct _Reg_##name { _Reg_##name(){tests().push_back({#name,name});} } _reg_##name; void name()
static std::vector<std::pair<std::string,std::function<void()>>>& tests(){
    static std::vector<std::pair<std::string,std::function<void()>>> v; return v;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Math tests
// ─────────────────────────────────────────────────────────────────────────────
TEST(Vec2_basic) {
    Vec2 a{3,4};
    EXPECT_NEAR(a.length(), 5.f, 1e-5f);
    Vec2 n = a.normalized();
    EXPECT_NEAR(n.length(), 1.f, 1e-5f);
    EXPECT_NEAR(n.x, 0.6f, 1e-5f);
    EXPECT_NEAR(n.y, 0.8f, 1e-5f);
    EXPECT_NEAR(Vec2(1,0).dot(Vec2(0,1)), 0.f, 1e-6f);
    EXPECT_NEAR(Vec2(1,0).cross(Vec2(0,1)), 1.f, 1e-6f);
}

TEST(Vec3_basic) {
    Vec3 a{1,0,0}, b{0,1,0};
    Vec3 c = a.cross(b);
    EXPECT_NEAR(c.x, 0.f, 1e-6f);
    EXPECT_NEAR(c.y, 0.f, 1e-6f);
    EXPECT_NEAR(c.z, 1.f, 1e-6f);
    EXPECT_NEAR(a.dot(b), 0.f, 1e-6f);
    EXPECT_NEAR(Vec3(1,2,3).length(), std::sqrt(14.f), 1e-5f);
}

TEST(Quat_rotation) {
    // 90° rotation around Y axis
    Quat q = Quat::fromAxisAngle(Vec3::Up(), kPi*0.5f);
    Vec3 v = q.rotate(Vec3::Right());  // (1,0,0) rotated 90° around Y → (0,0,-1)
    EXPECT_NEAR(v.x, 0.f, 1e-5f);
    EXPECT_NEAR(v.y, 0.f, 1e-5f);
    EXPECT_NEAR(v.z, -1.f, 1e-5f);
}

TEST(Quat_toMat3) {
    Quat q = Quat::fromAxisAngle({0,0,1}, kPi*0.5f);  // 90° around Z
    Mat3 m = q.toMat3();
    Vec3 r = m * Vec3::Right();   // (1,0,0) → (0,1,0)
    EXPECT_NEAR(r.x, 0.f, 1e-5f);
    EXPECT_NEAR(r.y, 1.f, 1e-5f);
    EXPECT_NEAR(r.z, 0.f, 1e-5f);
}

TEST(Transform_roundtrip) {
    Transform t;
    t.position = {1,2,3};
    t.rotation = Quat::fromAxisAngle(Vec3::Up(), kPi*0.25f);
    t.scale    = {2,2,2};

    Vec3 p{1,0,0};
    Vec3 world = t.transformPoint(p);
    Vec3 back  = t.inverseTransformPoint(world);
    EXPECT_NEAR(back.x, p.x, 1e-4f);
    EXPECT_NEAR(back.y, p.y, 1e-4f);
    EXPECT_NEAR(back.z, p.z, 1e-4f);
}

TEST(Ray_construction) {
    Ray r({0,0,0},{1,1,0});
    EXPECT_NEAR(r.dir.length(), 1.f, 1e-5f);
    Vec3 p = r.at(5.f);
    EXPECT_NEAR(p.x, p.y, 1e-5f);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Shape tests
// ─────────────────────────────────────────────────────────────────────────────
TEST(AABB3D_basic) {
    AABB3D a{{-1,-1,-1},{1,1,1}};
    EXPECT_TRUE(a.valid());
    EXPECT_NEAR(a.volume(), 8.f, 1e-5f);
    EXPECT_NEAR(a.surfaceArea(), 24.f, 1e-5f);
    EXPECT_TRUE(a.contains({0,0,0}));
    EXPECT_FALSE(a.contains({2,0,0}));

    AABB3D b{{0,0,0},{2,2,2}};
    EXPECT_TRUE(a.overlaps(b));

    AABB3D c{{3,3,3},{5,5,5}};
    EXPECT_FALSE(a.overlaps(c));

    // Merge
    AABB3D m = AABB3D::merge(a, c);
    EXPECT_NEAR(m.min.x, -1.f, 1e-5f);
    EXPECT_NEAR(m.max.x,  5.f, 1e-5f);
}

TEST(AABB3D_raycast) {
    AABB3D box{{-1,-1,-1},{1,1,1}};
    Ray r({-5,0,0},{1,0,0});
    float t;
    EXPECT_TRUE(box.raycast(r, t));
    EXPECT_NEAR(t, 4.f, 1e-4f);

    // Miss (ray above box)
    Ray miss({-5,2,0},{1,0,0});
    float tm;
    EXPECT_FALSE(box.raycast(miss, tm));
}

TEST(OBB3D_support) {
    OBB3D obb({0,0,0},{1,1,1}, Quat::identity());
    Vec3 s = obb.support({1,0,0});
    EXPECT_NEAR(s.x, 1.f, 1e-5f);
    s = obb.support({-1,0,0});
    EXPECT_NEAR(s.x, -1.f, 1e-5f);
}

TEST(Sphere_overlap) {
    Sphere a{{0,0,0}, 1.f};
    Sphere b{{1.5f,0,0}, 1.f};
    EXPECT_TRUE(a.overlaps(b));

    Sphere c{{3,0,0}, 1.f};
    EXPECT_FALSE(a.overlaps(c));
    EXPECT_NEAR(a.volume(), (4.f/3.f)*kPi, 1e-3f);
}

TEST(Capsule3D_closestPoint) {
    Capsule3D cap{{0,0,0},{0,4,0}, 0.5f};
    Vec3 cp = cap.closestPoint({2,2,0});
    EXPECT_NEAR(cp.x, 0.f, 1e-5f);
    EXPECT_NEAR(cp.y, 2.f, 1e-5f);
    EXPECT_NEAR(cp.z, 0.f, 1e-5f);

    // Before start
    Vec3 cp2 = cap.closestPoint({0,-5,0});
    EXPECT_NEAR(cp2.y, 0.f, 1e-5f);

    // After end
    Vec3 cp3 = cap.closestPoint({0,10,0});
    EXPECT_NEAR(cp3.y, 4.f, 1e-5f);
}

TEST(CollisionShape_worldAABB) {
    CollisionShape s = CollisionShape::makeSphere({0,0,0}, 1.f);
    Transform t;
    t.position = {5,5,5};
    t.scale    = {2,2,2};
    AABB3D aabb = s.worldAABB(t);
    EXPECT_NEAR(aabb.center().x, 5.f, 1e-4f);
    EXPECT_NEAR(aabb.extents().x, 2.f, 1e-4f); // radius 1 * scale 2
}

// ─────────────────────────────────────────────────────────────────────────────
//  SAT tests
// ─────────────────────────────────────────────────────────────────────────────
TEST(SAT_SphereSphere_hit) {
    Sphere a{{0,0,0},1.f}, b{{1.5f,0,0},1.f};
    auto r = SAT3D::testSphereSphere(a,b);
    EXPECT_TRUE(r.hit);
    EXPECT_NEAR(r.depth, 0.5f, 1e-4f);
    EXPECT_NEAR(r.normal.x, 1.f, 1e-4f);
}

TEST(SAT_SphereSphere_miss) {
    Sphere a{{0,0,0},1.f}, b{{3,0,0},1.f};
    auto r = SAT3D::testSphereSphere(a,b);
    EXPECT_FALSE(r.hit);
}

TEST(SAT_AABB_hit) {
    AABB3D a{{-1,-1,-1},{1,1,1}}, b{{0.5f,-1,-1},{2,1,1}};
    auto r = SAT3D::testAABBAABB(a,b);
    EXPECT_TRUE(r.hit);
    EXPECT_NEAR(r.depth, 0.5f, 1e-4f);
}

TEST(SAT_AABB_miss) {
    AABB3D a{{-1,-1,-1},{1,1,1}}, b{{2,-1,-1},{4,1,1}};
    auto r = SAT3D::testAABBAABB(a,b);
    EXPECT_FALSE(r.hit);
}

TEST(SAT_OBB_hit) {
    OBB3D a{{0,0,0},{1,1,1},Quat::identity()};
    OBB3D b{{1.5f,0,0},{1,1,1},Quat::fromAxisAngle({0,1,0},kPi*0.1f)};
    auto r = SAT3D::testOBBOBB(a,b);
    EXPECT_TRUE(r.hit);
    EXPECT_GT(r.depth, 0.f);
}

TEST(SAT_SphereAABB) {
    Sphere s{{0,0,0},1.f};
    AABB3D b{{0.5f,-2,-2},{4,2,2}};
    auto r = SAT3D::testSphereAABB(s,b);
    EXPECT_TRUE(r.hit);

    Sphere s2{{0,0,0},0.3f};
    auto r2 = SAT3D::testSphereAABB(s2,b);
    EXPECT_FALSE(r2.hit);
}

TEST(SAT_SphereOBB) {
    // Sphere center at 1.5, radius 1 → closest point on OBB is (1,0,0), dist=0.5 < radius → overlap
    Sphere s{{1.5f,0,0},1.f};
    OBB3D obb{{0,0,0},{1,1,1},Quat::identity()};
    auto r = SAT3D::testSphereOBB(s,obb);
    EXPECT_TRUE(r.hit);
    EXPECT_NEAR(r.depth, 0.5f, 1e-4f);

    // No overlap: center at (3,0,0), radius 1, OBB goes to 1 → gap of 1
    Sphere s2{{3,0,0},1.f};
    auto r2 = SAT3D::testSphereOBB(s2,obb);
    EXPECT_FALSE(r2.hit);
}

TEST(SAT_CapsuleSphere) {
    Capsule3D cap{{0,0,0},{0,4,0},0.5f};
    Sphere s{{0.8f,2,0},0.5f};
    auto r = SAT3D::testCapsuleSphere(cap,s);
    EXPECT_TRUE(r.hit);

    Sphere s2{{5,2,0},0.5f};
    auto r2 = SAT3D::testCapsuleSphere(cap,s2);
    EXPECT_FALSE(r2.hit);
}

TEST(SAT_CapsuleCapsule) {
    Capsule3D a{{0,0,0},{0,4,0},0.5f};
    Capsule3D b{{0.8f,0,0},{0.8f,4,0},0.5f};
    auto r = SAT3D::testCapsuleCapsule(a,b);
    EXPECT_TRUE(r.hit);
    EXPECT_NEAR(r.depth, 0.2f, 1e-3f);

    Capsule3D c{{5,0,0},{5,4,0},0.5f};
    auto r2 = SAT3D::testCapsuleCapsule(a,c);
    EXPECT_FALSE(r2.hit);
}

TEST(SAT2D_circles) {
    Circle2D a{{0,0},1.f}, b{{1.5f,0},1.f};
    auto r = SAT2D::testCircleCircle(a,b);
    EXPECT_TRUE(r.hit);
    EXPECT_NEAR(r.depth, 0.5f, 1e-4f);

    Circle2D c{{5,0},1.f};
    auto r2 = SAT2D::testCircleCircle(a,c);
    EXPECT_FALSE(r2.hit);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Narrowphase tests
// ─────────────────────────────────────────────────────────────────────────────
TEST(Narrowphase_spheres) {
    CollisionShape a = CollisionShape::makeSphere({0,0,0},1.f);
    CollisionShape b = CollisionShape::makeSphere({1.5f,0,0},1.f);
    Transform ta, tb;
    auto m = Narrowphase::generateContacts(a,ta,b,tb);
    EXPECT_TRUE(m.hit);
    EXPECT_GT(m.count, 0);
    EXPECT_NEAR(m.points[0].penetrationDepth, 0.5f, 1e-3f);
}

TEST(Narrowphase_contact_normal_direction) {
    // Normal should point from A to B
    CollisionShape a = CollisionShape::makeSphere({0,0,0},1.f);
    CollisionShape b = CollisionShape::makeSphere({1.5f,0,0},1.f);
    Transform ta, tb;
    auto m = Narrowphase::generateContacts(a,ta,b,tb);
    EXPECT_TRUE(m.hit);
    // Normal x > 0 (pointing right, A→B)
    EXPECT_GT(m.points[0].normal.x, 0.f);
}

TEST(Narrowphase_AABB_miss) {
    CollisionShape a = CollisionShape::makeAABB3D({-1,-1,-1},{1,1,1});
    CollisionShape b = CollisionShape::makeAABB3D({3,-1,-1},{5,1,1});
    Transform ta, tb;
    auto m = Narrowphase::generateContacts(a,ta,b,tb);
    EXPECT_FALSE(m.hit);
}

TEST(Narrowphase_sphere_AABB) {
    CollisionShape a = CollisionShape::makeSphere({0,0,0},1.f);
    CollisionShape b = CollisionShape::makeAABB3D({0.5f,-2,-2},{4,2,2});
    Transform ta, tb;
    auto m = Narrowphase::generateContacts(a,ta,b,tb);
    EXPECT_TRUE(m.hit);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Broadphase / BVH tests
// ─────────────────────────────────────────────────────────────────────────────
TEST(BVH_insert_query) {
    DynamicBVH bvh;
    for(uint64_t i=0;i<20;i++) {
        float x = float(i)*2.f;
        bvh.insert(i, AABB3D{{x-0.5f,-0.5f,-0.5f},{x+0.5f,0.5f,0.5f}});
    }
    std::vector<uint64_t> found;
    bvh.query(AABB3D{{5,-1,-1},{9,1,1}}, [&](uint64_t id){ found.push_back(id); return true; });
    EXPECT_GT(found.size(), size_t(0));
}

TEST(BVH_remove) {
    DynamicBVH bvh;
    bvh.insert(1, AABB3D{{-1,-1,-1},{1,1,1}});
    bvh.insert(2, AABB3D{{0,-1,-1},{2,1,1}});
    bvh.remove(1);

    std::vector<uint64_t> found;
    bvh.query(AABB3D{{-1,-1,-1},{1,1,1}}, [&](uint64_t id){ found.push_back(id); return true; });
    // Only body 2 should remain (it overlaps)
    for(auto id : found) { EXPECT_TRUE(id==2); }
}

TEST(BVH_update) {
    DynamicBVH bvh;
    bvh.insert(1, AABB3D{{-1,-1,-1},{1,1,1}});

    // Move far away
    bvh.update(1, AABB3D{{100,100,100},{102,102,102}});

    std::vector<uint64_t> found;
    bvh.query(AABB3D{{-2,-2,-2},{2,2,2}}, [&](uint64_t id){ found.push_back(id); return true; });
    EXPECT_TRUE(found.empty());  // should not be found at origin anymore
}

TEST(BVH_pairs) {
    DynamicBVH bvh;
    // 3 overlapping boxes
    bvh.insert(1, AABB3D{{0,0,0},{2,2,2}});
    bvh.insert(2, AABB3D{{1,0,0},{3,2,2}});
    bvh.insert(3, AABB3D{{10,0,0},{12,2,2}});  // isolated

    std::vector<CollidingPair> pairs;
    bvh.collectPairs(pairs);
    // Only (1,2) should overlap
    EXPECT_TRUE(pairs.size() >= 1);
    bool found12 = false;
    for(auto& p : pairs)
        if((p.bodyA==1&&p.bodyB==2)||(p.bodyA==2&&p.bodyB==1)) found12=true;
    EXPECT_TRUE(found12);
}

TEST(BVH_ray) {
    DynamicBVH bvh;
    bvh.insert(1, AABB3D{{-1,-1,-1},{1,1,1}});
    bvh.insert(2, AABB3D{{5,-1,-1},{7,1,1}});

    Ray ray({-5,0,0},{1,0,0});
    std::vector<uint64_t> hits;
    bvh.rayQuery(ray, [&](uint64_t id, float){ hits.push_back(id); return true; });
    EXPECT_GT(hits.size(), size_t(0));
}

TEST(SAP_pairs) {
    SweepAndPrune sap;
    sap.insert(1, AABB3D{{0,0,0},{2,2,2}});
    sap.insert(2, AABB3D{{1,0,0},{3,2,2}});
    sap.insert(3, AABB3D{{10,0,0},{12,2,2}});

    std::vector<CollidingPair> pairs;
    sap.collectPairs(pairs);
    bool found12 = false;
    for(auto& p : pairs)
        if((p.bodyA==1&&p.bodyB==2)||(p.bodyA==2&&p.bodyB==1)) found12=true;
    EXPECT_TRUE(found12);
    // Body 3 should not pair with anything
    for(auto& p : pairs)
        EXPECT_FALSE(p.bodyA==3 || p.bodyB==3);
}

// ─────────────────────────────────────────────────────────────────────────────
//  CollisionWorld tests
// ─────────────────────────────────────────────────────────────────────────────
TEST(World_add_remove) {
    WorldConfig cfg; cfg.gpuBackend=GPUBackend::None; cfg.multithreaded=false;
    CollisionWorld world(cfg);

    CollisionBody b; b.shape=CollisionShape::makeSphere({0,0,0},1.f);
    auto id1 = world.addBody(b);
    auto id2 = world.addBody(b);

    EXPECT_TRUE(world.bodyCount()==2);
    world.removeBody(id1);
    EXPECT_TRUE(world.bodyCount()==1);
    world.removeBody(id2);
    EXPECT_TRUE(world.bodyCount()==0);
}

TEST(World_contacts) {
    WorldConfig cfg; cfg.gpuBackend=GPUBackend::None; cfg.multithreaded=false;
    CollisionWorld world(cfg);

    CollisionBody a; a.shape=CollisionShape::makeSphere({0,0,0},1.f);
    CollisionBody b; b.shape=CollisionShape::makeSphere({1.5f,0,0},1.f);
    CollisionBody c; c.shape=CollisionShape::makeSphere({10,0,0},1.f);
    world.addBody(a); world.addBody(b); world.addBody(c);
    world.step();

    EXPECT_TRUE(world.contactCount()>=1);
}

TEST(World_callback) {
    WorldConfig cfg; cfg.gpuBackend=GPUBackend::None; cfg.multithreaded=false;
    CollisionWorld world(cfg);

    int callbackFired=0;
    world.setContactCallback([&](const ContactManifold&,
                                  const CollisionBody&, const CollisionBody&){
        callbackFired++;
    });

    CollisionBody a; a.shape=CollisionShape::makeSphere({0,0,0},1.f);
    CollisionBody b; b.shape=CollisionShape::makeSphere({1.5f,0,0},1.f);
    world.addBody(a); world.addBody(b);
    world.step();
    EXPECT_GT(callbackFired, 0);
}

TEST(World_layer_mask) {
    WorldConfig cfg; cfg.gpuBackend=GPUBackend::None; cfg.multithreaded=false;
    CollisionWorld world(cfg);

    // Layer 1 and layer 2 do NOT collide
    CollisionBody a;
    a.shape=CollisionShape::makeSphere({0,0,0},1.f);
    a.layer=1; a.mask=1;  // only collides with layer 1

    CollisionBody b;
    b.shape=CollisionShape::makeSphere({1.5f,0,0},1.f);
    b.layer=2; b.mask=2;  // only collides with layer 2

    world.addBody(a); world.addBody(b);
    world.step();
    EXPECT_TRUE(world.contactCount()==0);  // different layers
}

TEST(World_static_static_skip) {
    WorldConfig cfg; cfg.gpuBackend=GPUBackend::None; cfg.multithreaded=false;
    CollisionWorld world(cfg);

    CollisionBody a; a.shape=CollisionShape::makeSphere({0,0,0},1.f); a.flags=BodyFlag_Static;
    CollisionBody b; b.shape=CollisionShape::makeSphere({1.5f,0,0},1.f); b.flags=BodyFlag_Static;
    world.addBody(a); world.addBody(b);
    world.step();
    EXPECT_TRUE(world.contactCount()==0);  // static-static skipped
}

TEST(World_trigger) {
    WorldConfig cfg; cfg.gpuBackend=GPUBackend::None; cfg.multithreaded=false;
    CollisionWorld world(cfg);

    int triggerFired=0;
    world.setTriggerCallback([&](uint64_t, uint64_t, bool entered){
        if(entered) triggerFired++;
    });

    CollisionBody a; a.shape=CollisionShape::makeSphere({0,0,0},1.f);
    CollisionBody b; b.shape=CollisionShape::makeSphere({1.5f,0,0},1.f); b.flags=BodyFlag_Trigger;
    world.addBody(a); world.addBody(b);
    world.step();
    EXPECT_GT(triggerFired,0);
}

TEST(World_raycast) {
    WorldConfig cfg; cfg.gpuBackend=GPUBackend::None; cfg.multithreaded=false;
    CollisionWorld world(cfg);

    CollisionBody b; b.shape=CollisionShape::makeAABB3D({-1,-1,-1},{1,1,1});
    world.addBody(b);

    Ray r({-5,0,0},{1,0,0});
    RaycastHit hit;
    EXPECT_TRUE(world.raycast(r,hit));
    EXPECT_NEAR(hit.distance, 4.f, 1e-3f);
    EXPECT_NEAR(hit.point.x, -1.f, 1e-3f);

    Ray miss({-5,5,0},{1,0,0});
    EXPECT_FALSE(world.raycast(miss,hit));
}

TEST(World_setTransform) {
    WorldConfig cfg; cfg.gpuBackend=GPUBackend::None; cfg.multithreaded=false;
    CollisionWorld world(cfg);

    // A at origin, B far away — no contact
    CollisionBody a; a.shape=CollisionShape::makeSphere({0,0,0},1.f);
    CollisionBody b; b.shape=CollisionShape::makeSphere({0,0,0},1.f);
    world.addBody(a);
    auto idB = world.addBody(b);

    // Move B far away initially
    Transform tFar; tFar.position={10,0,0};
    world.setTransform(idB, tFar);
    world.step();
    EXPECT_TRUE(world.contactCount()==0);

    // Move B close to A
    Transform tClose; tClose.position={1.5f,0,0};
    world.setTransform(idB, tClose);
    world.step();
    EXPECT_TRUE(world.contactCount()>=1);
}

TEST(World_multithreaded) {
    WorldConfig cfg; cfg.gpuBackend=GPUBackend::None; cfg.multithreaded=true; cfg.threadCount=4;
    CollisionWorld world(cfg);

    // Add 200 overlapping spheres in a grid
    for(int i=0;i<200;i++){
        CollisionBody b;
        float x=float(i%10)*1.8f, y=float(i/10)*1.8f;
        b.shape=CollisionShape::makeSphere({x,y,0},1.f);
        world.addBody(b);
    }
    world.step();
    // Should detect contacts
    EXPECT_GT(world.contactCount(), size_t(0));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Interval tests
// ─────────────────────────────────────────────────────────────────────────────
TEST(Interval_overlap) {
    Interval a{0,3}, b{2,5};
    EXPECT_TRUE(a.overlaps(b));
    EXPECT_NEAR(a.overlap(b), 1.f, 1e-5f);

    Interval c{4,6};
    EXPECT_FALSE(a.overlaps(c));

    Interval d{1,2};
    EXPECT_TRUE(d.contains(1.5f));
    EXPECT_FALSE(d.contains(3.f));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Triangle tests
// ─────────────────────────────────────────────────────────────────────────────
TEST(Triangle_closestPoint) {
    Triangle tri;
    tri.v[0]={0,0,0}; tri.v[1]={2,0,0}; tri.v[2]={1,2,0};

    // Point above centroid → closest = centroid-ish (inside triangle)
    Vec3 p={1,0.5f,5};
    Vec3 cp = tri.closestPoint(p);
    EXPECT_NEAR(cp.z, 0.f, 1e-5f);  // on the triangle plane

    // Point outside edge 0→1
    Vec3 p2={1,-1,0};
    Vec3 cp2 = tri.closestPoint(p2);
    EXPECT_NEAR(cp2.y, 0.f, 1e-5f); // clamped to edge
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "═══════════════════════════════════\n";
    std::cout << " ColLib — Unit Test Suite\n";
    std::cout << "═══════════════════════════════════\n";

    for(auto& [name,fn] : tests()) {
        std::cout << "  " << name << " ... ";
        int prevFail = sFail;
        fn();
        if(sFail == prevFail) std::cout << "OK\n";
        else                  std::cout << "FAIL\n";
    }

    std::cout << "═══════════════════════════════════\n";
    std::cout << "  Passed: " << sPass << "\n";
    std::cout << "  Failed: " << sFail << "\n";
    std::cout << "═══════════════════════════════════\n";
    return sFail > 0 ? 1 : 0;
}
