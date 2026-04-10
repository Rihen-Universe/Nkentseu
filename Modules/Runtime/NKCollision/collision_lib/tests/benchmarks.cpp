// benchmarks.cpp — ColLib advanced benchmarks
// Compile: g++ -std=c++17 -O3 -msse4.2 -mavx2 -I ../include ../src/shapes.cpp benchmarks.cpp -lpthread -o run_bench

#include <collision/collision.h>
#include <collision/ccd.h>
#include <collision/persistent_contacts.h>
#include <collision/debug_draw.h>
#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <iomanip>
#include <algorithm>

using namespace col;
using namespace std::chrono;

// ─────────────────────────────────────────────────────────────────────────────
//  Timer helper
// ─────────────────────────────────────────────────────────────────────────────
struct Timer {
    steady_clock::time_point t0 = steady_clock::now();
    double elapsed_ms() const {
        return duration_cast<microseconds>(steady_clock::now()-t0).count()/1000.0;
    }
    void reset() { t0 = steady_clock::now(); }
};

static void printRow(const std::string& name, int N, double ms,
                     size_t contacts, double mem_kb=0)
{
    std::cout << std::left  << std::setw(32) << name
              << std::right << std::setw(7)  << N       << " bodies | "
              << std::setw(8) << std::fixed << std::setprecision(3) << ms << " ms/frame | "
              << std::setw(6) << contacts << " contacts";
    if(mem_kb>0) std::cout << " | " << std::setw(6) << (int)mem_kb << " KB";
    std::cout << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Benchmark 1 — BVH only (no narrowphase)
// ─────────────────────────────────────────────────────────────────────────────
void benchBroadphaseOnly(int N) {
    DynamicBVH bvh;
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-50.f, 50.f);

    std::vector<uint64_t> ids(N);
    for(int i=0;i<N;i++){
        float x=dist(rng),y=dist(rng),z=dist(rng);
        AABB3D aabb{{x-.5f,y-.5f,z-.5f},{x+.5f,y+.5f,z+.5f}};
        bvh.insert(i,aabb);
        ids[i]=i;
    }

    // Warm up
    std::vector<CollidingPair> pairs;
    bvh.collectPairs(pairs);

    Timer t;
    const int FRAMES = 20;
    for(int f=0;f<FRAMES;f++){
        // Simulate random movement
        for(int i=0;i<N/10;i++){
            float x=dist(rng),y=dist(rng),z=dist(rng);
            AABB3D aabb{{x-.5f,y-.5f,z-.5f},{x+.5f,y+.5f,z+.5f}};
            bvh.update(ids[rng()%N], aabb);
        }
        pairs.clear();
        bvh.collectPairs(pairs);
    }
    printRow("BVH broadphase", N, t.elapsed_ms()/FRAMES, pairs.size());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Benchmark 2 — Full pipeline (broadphase + narrowphase)
// ─────────────────────────────────────────────────────────────────────────────
void benchFullPipeline(int N, bool multithreaded=false) {
    WorldConfig cfg;
    cfg.gpuBackend   = GPUBackend::None;
    cfg.multithreaded = multithreaded;
    cfg.threadCount  = multithreaded ? 4 : 1;
    CollisionWorld world(cfg);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-20.f, 20.f);

    // Mix of shapes
    for(int i=0;i<N;i++){
        CollisionBody b;
        float x=dist(rng),y=dist(rng),z=dist(rng);
        int type = i % 4;
        switch(type){
        case 0: b.shape=CollisionShape::makeSphere({x,y,z},0.5f); break;
        case 1: b.shape=CollisionShape::makeAABB3D({x-.4f,y-.4f,z-.4f},{x+.4f,y+.4f,z+.4f}); break;
        case 2: b.shape=CollisionShape::makeCapsule3D({x,y-.5f,z},{x,y+.5f,z},0.3f); break;
        case 3: b.shape=CollisionShape::makeOBB3D({x,y,z},{.4f,.4f,.4f},
                    Quat::fromAxisAngle({0,1,0},(float)i*0.3f)); break;
        }
        b.velocity = {dist(rng)*0.1f, dist(rng)*0.1f, 0};
        world.addBody(b);
    }

    // Warm up
    world.step();

    Timer t;
    const int FRAMES = 30;
    for(int f=0;f<FRAMES;f++) world.step();
    std::string name = multithreaded ? "Full pipeline (4t)" : "Full pipeline (1t)";
    printRow(name, N, t.elapsed_ms()/FRAMES, world.contactCount());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Benchmark 3 — Dense sphere packing (worst-case contacts)
// ─────────────────────────────────────────────────────────────────────────────
void benchDensePacking(int N) {
    WorldConfig cfg; cfg.gpuBackend=GPUBackend::None; cfg.multithreaded=true;
    CollisionWorld world(cfg);

    // Grid packing: each sphere touches 6 neighbors
    int side = (int)std::cbrt(N) + 1;
    int count=0;
    for(int x=0;x<side&&count<N;x++)
    for(int y=0;y<side&&count<N;y++)
    for(int z=0;z<side&&count<N;z++,count++){
        CollisionBody b;
        b.shape=CollisionShape::makeSphere(
            {float(x)*1.8f, float(y)*1.8f, float(z)*1.8f}, 1.f);
        world.addBody(b);
    }
    world.step();

    Timer t;
    const int FRAMES = 10;
    for(int f=0;f<FRAMES;f++) world.step();
    printRow("Dense sphere grid", N, t.elapsed_ms()/FRAMES, world.contactCount());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Benchmark 4 — Mostly static scene (typical game level)
// ─────────────────────────────────────────────────────────────────────────────
void benchMostlyStatic(int staticCount, int dynamicCount) {
    WorldConfig cfg; cfg.gpuBackend=GPUBackend::None; cfg.multithreaded=true;
    CollisionWorld world(cfg);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> pos(-100.f, 100.f);

    // Static geometry (floor tiles + walls)
    for(int i=0;i<staticCount;i++){
        CollisionBody b;
        float x=pos(rng), z=pos(rng);
        b.shape = CollisionShape::makeAABB3D({x-1.f,-0.1f,z-1.f},{x+1.f,0.1f,z+1.f});
        b.flags = BodyFlag_Static;
        world.addBody(b);
    }
    // Dynamic characters/projectiles
    for(int i=0;i<dynamicCount;i++){
        CollisionBody b;
        b.shape = CollisionShape::makeCapsule3D(
            {pos(rng),0.3f,pos(rng)}, {pos(rng),1.5f,pos(rng)}, 0.3f);
        b.velocity = {pos(rng)*0.1f, 0.f, pos(rng)*0.1f};
        world.addBody(b);
    }
    world.step();

    Timer t;
    const int FRAMES=20;
    for(int f=0;f<FRAMES;f++) world.step();
    int N=staticCount+dynamicCount;
    printRow("Mostly static scene", N, t.elapsed_ms()/FRAMES, world.contactCount());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Benchmark 5 — Persistent contacts overhead
// ─────────────────────────────────────────────────────────────────────────────
void benchPersistentContacts(int N) {
    ContactCache cache;
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> pos(-5.f, 5.f);

    // Simulate N contacts per frame, tracked over 100 frames
    Timer t;
    const int FRAMES = 100;
    for(int f=0;f<FRAMES;f++){
        for(int i=0;i<N;i++){
            ContactManifold m;
            m.hit=true; m.bodyA=i; m.bodyB=i+N;
            ContactPoint cp;
            cp.positionA={pos(rng),pos(rng),pos(rng)};
            cp.positionB=cp.positionA+Vec3(0,.01f,0);
            cp.normal={0,1,0};
            cp.penetrationDepth=0.01f;
            m.addPoint(cp);
            Transform ta, tb;
            cache.update(m, ta, tb, f);
        }
        cache.evict(f);
    }
    printRow("Persistent cache", N, t.elapsed_ms()/FRAMES, cache.size());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Benchmark 6 — CCD TOI solver
// ─────────────────────────────────────────────────────────────────────────────
void benchCCD(int N) {
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> pos(-10.f,10.f);
    std::uniform_real_distribution<float> vel(-5.f,5.f);

    Timer t;
    int hits=0;
    for(int i=0;i<N;i++){
        Vec3 c0{pos(rng),pos(rng),pos(rng)};
        Vec3 c1=c0+Vec3{vel(rng),vel(rng),vel(rng)}*0.016f;
        Vec3 bc{pos(rng),pos(rng),pos(rng)};
        Vec3 bc1=bc+Vec3{vel(rng),vel(rng),vel(rng)}*0.016f;
        auto r=SweptSphere::sphereVsSphere(c0,c1,0.5f,bc,bc1,0.5f);
        if(r.hit) hits++;
    }
    double ms=t.elapsed_ms();
    std::cout << std::left  << std::setw(32) << "CCD swept sphere"
              << std::right << std::setw(7)  << N << " tests  | "
              << std::fixed << std::setprecision(3) << ms << " ms total | "
              << hits << " hits\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Benchmark 7 — SAT narrowphase microbench
// ─────────────────────────────────────────────────────────────────────────────
void benchSAT(int N) {
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> pos(-0.9f,0.9f);
    std::uniform_real_distribution<float> ang(0.f,6.28f);

    int hits=0;
    Timer t;
    for(int i=0;i<N;i++){
        OBB3D a{{0,0,0},{.5f,.5f,.5f},Quat::fromAxisAngle({0,1,0},ang(rng))};
        OBB3D b{{pos(rng),pos(rng),pos(rng)},{.5f,.5f,.5f},
                 Quat::fromAxisAngle({1,0,0},ang(rng))};
        auto r=SAT3D::testOBBOBB(a,b);
        if(r.hit) hits++;
    }
    double ms=t.elapsed_ms();
    std::cout << std::left  << std::setw(32) << "SAT OBB vs OBB"
              << std::right << std::setw(7)  << N << " tests  | "
              << std::fixed << std::setprecision(3) << ms << " ms total | "
              << hits << " hits ("
              << std::setprecision(1) << 100.f*hits/N << "%)\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Benchmark 8 — BVH query throughput
// ─────────────────────────────────────────────────────────────────────────────
void benchBVHQuery(int N, int queries) {
    DynamicBVH bvh;
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-50.f,50.f);
    for(int i=0;i<N;i++){
        float x=dist(rng),y=dist(rng),z=dist(rng);
        bvh.insert(i,AABB3D{{x-.5f,y-.5f,z-.5f},{x+.5f,y+.5f,z+.5f}});
    }

    size_t totalHits=0;
    Timer t;
    for(int q=0;q<queries;q++){
        float x=dist(rng),y=dist(rng),z=dist(rng);
        AABB3D query{{x-2.f,y-2.f,z-2.f},{x+2.f,y+2.f,z+2.f}};
        bvh.query(query,[&](uint64_t){ totalHits++; return true; });
    }
    double ms=t.elapsed_ms();
    std::cout << std::left  << std::setw(32) << "BVH point query"
              << std::right << std::setw(7)  << N << " bodies | "
              << std::fixed << std::setprecision(3) << ms << " ms/"
              << queries << " queries | "
              << totalHits/queries << " avg hits/query\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Benchmark 9 — DebugDraw overhead
// ─────────────────────────────────────────────────────────────────────────────
void benchDebugDraw(int N) {
    DebugDraw dd;
    int lineCount=0;
    dd.setLineCallback([&](const Vec3&,const Vec3&,Color){ lineCount++; });

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> pos(-10.f,10.f);

    Timer t;
    for(int i=0;i<N;i++){
        Vec3 c{pos(rng),pos(rng),pos(rng)};
        dd.drawSphere(c,1.f,Color::Green());
    }
    double ms=t.elapsed_ms();
    std::cout << std::left  << std::setw(32) << "DebugDraw spheres"
              << std::right << std::setw(7)  << N << " shapes | "
              << std::fixed << std::setprecision(3) << ms << " ms | "
              << lineCount << " lines emitted\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║          ColLib — Advanced Benchmark Suite                   ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n\n";

    std::cout << "── BVH Broadphase ──────────────────────────────────────────────\n";
    benchBroadphaseOnly(500);
    benchBroadphaseOnly(2000);
    benchBroadphaseOnly(10000);

    std::cout << "\n── Full Pipeline (single-thread) ───────────────────────────────\n";
    benchFullPipeline(256, false);
    benchFullPipeline(1024, false);
    benchFullPipeline(4096, false);

    std::cout << "\n── Full Pipeline (4 threads) ───────────────────────────────────\n";
    benchFullPipeline(256,  true);
    benchFullPipeline(1024, true);
    benchFullPipeline(4096, true);

    std::cout << "\n── Dense Packing (worst-case contacts) ─────────────────────────\n";
    benchDensePacking(125);
    benchDensePacking(512);
    benchDensePacking(1000);

    std::cout << "\n── Mostly Static Scene ─────────────────────────────────────────\n";
    benchMostlyStatic(500, 50);
    benchMostlyStatic(2000, 200);

    std::cout << "\n── Persistent Contact Cache ────────────────────────────────────\n";
    benchPersistentContacts(100);
    benchPersistentContacts(1000);
    benchPersistentContacts(10000);

    std::cout << "\n── CCD Swept Sphere ────────────────────────────────────────────\n";
    benchCCD(10000);
    benchCCD(100000);
    benchCCD(1000000);

    std::cout << "\n── SAT Narrowphase microbench ──────────────────────────────────\n";
    benchSAT(10000);
    benchSAT(100000);

    std::cout << "\n── BVH Query Throughput ────────────────────────────────────────\n";
    benchBVHQuery(1000,  1000);
    benchBVHQuery(10000, 1000);
    benchBVHQuery(50000, 1000);

    std::cout << "\n── Debug Draw overhead ─────────────────────────────────────────\n";
    benchDebugDraw(100);
    benchDebugDraw(1000);

    std::cout << "\nDone.\n";
    return 0;
}
