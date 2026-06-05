// example_usage.cpp — démonstration complète de la bibliothèque
#include <collision/collision.h>
#include <iostream>
#include <cassert>
#include <chrono>

using namespace col;
using namespace std::chrono;

// ─────────────────────────────────────────────────────────────────────────────
//  Test 1 — Sphere vs Sphere (SAT)
// ─────────────────────────────────────────────────────────────────────────────
void testSphereSphere() {
    std::cout << "[Test] Sphere vs Sphere\n";

    CollisionShape sa = CollisionShape::makeSphere({0,0,0}, 1.f);
    CollisionShape sb = CollisionShape::makeSphere({1.5f,0,0}, 1.f);

    Transform ta, tb;
    auto m = Narrowphase::generateContacts(sa,ta,sb,tb);

    assert(m.hit && "Spheres should collide!");
    assert(m.count==1);
    assert(m.points[0].penetrationDepth > 0.f);
    std::cout << "  Hit! depth=" << m.points[0].penetrationDepth
              << " normal=(" << m.points[0].normal.x << ","
              << m.points[0].normal.y << "," << m.points[0].normal.z << ")\n";

    // Separated spheres
    CollisionShape sc = CollisionShape::makeSphere({3.f,0,0}, 1.f);
    auto m2 = Narrowphase::generateContacts(sa,ta,sc,tb);
    assert(!m2.hit && "Spheres should NOT collide!");
    std::cout << "  Separated: OK\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Test 2 — AABB vs AABB (SAT)
// ─────────────────────────────────────────────────────────────────────────────
void testAABBAABB() {
    std::cout << "[Test] AABB vs AABB\n";

    CollisionShape sa = CollisionShape::makeAABB3D({-1,-1,-1},{1,1,1});
    CollisionShape sb = CollisionShape::makeAABB3D({0.5f,-1,-1},{2.5f,1,1});

    Transform ta, tb;
    auto m = Narrowphase::generateContacts(sa,ta,sb,tb);
    assert(m.hit);
    std::cout << "  Hit! depth=" << m.points[0].penetrationDepth << "\n";

    // No overlap
    CollisionShape sc = CollisionShape::makeAABB3D({2,-1,-1},{4,1,1});
    auto m2 = Narrowphase::generateContacts(sa,ta,sc,tb);
    assert(!m2.hit);
    std::cout << "  Separated: OK\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Test 3 — OBB vs OBB (SAT, 15 axes)
// ─────────────────────────────────────────────────────────────────────────────
void testOBBOBB() {
    std::cout << "[Test] OBB vs OBB\n";

    OBB3D a({0,0,0}, {1,1,1}, Quat::identity());
    OBB3D b({1.5f,0,0}, {1,1,1}, Quat::fromAxisAngle({0,1,0}, 3.14159f*0.25f));

    auto res = SAT3D::testOBBOBB(a, b);
    std::cout << "  Result: " << (res.hit ? "HIT" : "MISS")
              << " depth=" << res.depth << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Test 4 — BVH broadphase
// ─────────────────────────────────────────────────────────────────────────────
void testBVH() {
    std::cout << "[Test] Dynamic BVH\n";

    DynamicBVH bvh;
    // Insert 10 bodies
    for(uint64_t i=0;i<10;i++) {
        float x = (float)i * 2.f;
        AABB3D aabb({x-0.5f,-0.5f,-0.5f},{x+0.5f,0.5f,0.5f});
        bvh.insert(i, aabb);
    }

    // Query center region
    std::vector<uint64_t> found;
    bvh.query(AABB3D({3.f,-1.f,-1.f},{7.f,1.f,1.f}),
              [&](uint64_t id){ found.push_back(id); return true; });

    std::cout << "  Found " << found.size() << " bodies in query region\n";
    assert(found.size()>0);

    // Update one body
    bvh.update(5, AABB3D({20.f,-0.5f,-0.5f},{21.f,0.5f,0.5f}));

    // Collect pairs
    std::vector<CollidingPair> pairs;
    bvh.collectPairs(pairs);
    std::cout << "  Pairs: " << pairs.size() << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Test 5 — CollisionWorld full pipeline
// ─────────────────────────────────────────────────────────────────────────────
void testWorld() {
    std::cout << "[Test] CollisionWorld full pipeline\n";

    WorldConfig cfg;
    cfg.gpuBackend   = GPUBackend::None;  // CPU in this test
    cfg.multithreaded = false;
    CollisionWorld world(cfg);

    // Register callbacks
    int contactCount = 0;
    world.setContactCallback([&](const ContactManifold& m,
                                 const CollisionBody& a,
                                 const CollisionBody& b){
        (void)a; (void)b;
        contactCount++;
        (void)m;
    });

    // Add overlapping spheres
    CollisionBody bodyA;
    bodyA.shape = CollisionShape::makeSphere({0,0,0}, 1.f);

    CollisionBody bodyB;
    bodyB.shape = CollisionShape::makeSphere({1.5f,0,0}, 1.f);

    CollisionBody bodyC;
    bodyC.shape = CollisionShape::makeSphere({10.f,0,0}, 1.f);

    auto idA = world.addBody(bodyA);
    auto idB = world.addBody(bodyB);
    auto idC = world.addBody(bodyC);
    (void)idA; (void)idB; (void)idC;

    world.step();

    std::cout << "  Contacts: " << world.contactCount()
              << ", callbacks: " << contactCount << "\n";
    assert(world.contactCount() >= 1);

    // Move C close to A
    Transform tC;
    tC.position = {1.2f,0,0};
    world.setTransform(idC, tC);
    world.step();
    std::cout << "  After move: contacts=" << world.contactCount() << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Benchmark — N spheres, full pipeline
// ─────────────────────────────────────────────────────────────────────────────
void benchmark(int N) {
    std::cout << "[Bench] " << N << " spheres\n";

    WorldConfig cfg;
    cfg.gpuBackend = GPUBackend::None;
    cfg.multithreaded = true;
    CollisionWorld world(cfg);

    // Dense sphere packing
    for(int i=0;i<N;i++) {
        CollisionBody b;
        float x=(i%10)*2.2f, y=((i/10)%10)*2.2f, z=(i/100)*2.2f;
        b.shape = CollisionShape::makeSphere({x,y,z}, 1.f);
        b.velocity = {0.1f,0,0};
        world.addBody(b);
    }

    // Warm up
    world.step();

    // Measure 10 frames
    auto t0 = high_resolution_clock::now();
    for(int f=0;f<10;f++) world.step();
    auto t1 = high_resolution_clock::now();

    double ms = duration_cast<microseconds>(t1-t0).count() / 10000.0;
    std::cout << "  " << world.contactCount() << " contacts/frame, "
              << ms << " ms/frame\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Test 6 — Capsule collision
// ─────────────────────────────────────────────────────────────────────────────
void testCapsules() {
    std::cout << "[Test] Capsule vs Capsule\n";

    Capsule3D a({0,0,0},{0,2,0},0.5f);
    Capsule3D b({0.8f,0,0},{0.8f,2,0},0.5f);

    auto res = SAT3D::testCapsuleCapsule(a,b);
    assert(res.hit);
    std::cout << "  Hit! depth=" << res.depth << "\n";

    Capsule3D c({5,0,0},{5,2,0},0.5f);
    auto res2 = SAT3D::testCapsuleCapsule(a,c);
    assert(!res2.hit);
    std::cout << "  Separated: OK\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Test 7 — Raycast
// ─────────────────────────────────────────────────────────────────────────────
void testRaycast() {
    std::cout << "[Test] Raycast\n";

    WorldConfig cfg; cfg.gpuBackend=GPUBackend::None;
    CollisionWorld world(cfg);

    CollisionBody b;
    b.shape = CollisionShape::makeAABB3D({-1,-1,-1},{1,1,1});
    world.addBody(b);

    Ray ray({-5,0,0},{1,0,0});
    RaycastHit hit;
    bool found = world.raycast(ray, hit);
    assert(found);
    std::cout << "  Hit at t=" << hit.distance
              << " point=(" << hit.point.x << ","
              << hit.point.y << "," << hit.point.z << ")\n";

    Ray miss({-5,5,0},{1,0,0});
    bool miss2 = world.raycast(miss, hit);
    // AABB is small so ray at y=5 should miss
    std::cout << "  Miss: " << (!miss2?"OK":"unexpected hit") << "\n";
}

int main() {
    std::cout << "═══════════════════════════════════════\n";
    std::cout << " ColLib — Test Suite\n";
    std::cout << "═══════════════════════════════════════\n";

    testSphereSphere();
    testAABBAABB();
    testOBBOBB();
    testBVH();
    testWorld();
    testCapsules();
    testRaycast();

    std::cout << "\n── Benchmarks ──\n";
    benchmark(100);
    benchmark(1000);
    benchmark(5000);

    std::cout << "\n✓ All tests passed.\n";
    return 0;
}
