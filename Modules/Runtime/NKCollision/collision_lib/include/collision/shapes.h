#pragma once
#include "math.h"
#include <vector>
#include <array>
#include <memory>
#include <functional>

namespace col {

// ─────────────────────────────────────────────────────────────────────────────
//  Shape types enum
// ─────────────────────────────────────────────────────────────────────────────
enum class ShapeType : uint8_t {
    // 2D
    Circle2D = 0,
    AABB2D,
    OBB2D,
    Capsule2D,
    Polygon2D,
    // 3D
    Sphere,
    AABB3D,
    OBB3D,
    Capsule3D,
    ConvexHull,
    TriangleMesh,
    Heightfield,
    Compound,
    Count
};

// ─────────────────────────────────────────────────────────────────────────────
//  AABB2D
// ─────────────────────────────────────────────────────────────────────────────
struct AABB2D {
    Vec2 min, max;

    AABB2D() : min(kInfinity), max(-kInfinity) {}
    AABB2D(const Vec2& min, const Vec2& max) : min(min), max(max) {}
    AABB2D(const Vec2& center, float halfW, float halfH)
        : min(center.x-halfW, center.y-halfH),
          max(center.x+halfW, center.y+halfH) {}

    Vec2  center()  const { return (min+max)*0.5f; }
    Vec2  extents() const { return (max-min)*0.5f; }
    Vec2  size()    const { return max-min; }
    float area()    const { Vec2 s=size(); return s.x*s.y; }
    float perimeter()const{ Vec2 s=size(); return 2*(s.x+s.y); }
    bool  valid()   const { return min.x<=max.x && min.y<=max.y; }

    void  expand(const Vec2& p)   { min=Vec2::Min(min,p); max=Vec2::Max(max,p); }
    void  expand(const AABB2D& o) { min=Vec2::Min(min,o.min); max=Vec2::Max(max,o.max); }
    void  fatten(float margin)    { min=min-Vec2(margin); max=max+Vec2(margin); }

    bool contains(const Vec2& p)   const { return p.x>=min.x&&p.x<=max.x&&p.y>=min.y&&p.y<=max.y; }
    bool contains(const AABB2D& o) const { return o.min.x>=min.x&&o.max.x<=max.x&&o.min.y>=min.y&&o.max.y<=max.y; }
    bool overlaps(const AABB2D& o) const { return min.x<=o.max.x&&max.x>=o.min.x&&min.y<=o.max.y&&max.y>=o.min.y; }
    float overlapArea(const AABB2D& o) const {
        float dx = std::min(max.x,o.max.x)-std::max(min.x,o.min.x);
        float dy = std::min(max.y,o.max.y)-std::max(min.y,o.min.y);
        return (dx>0&&dy>0) ? dx*dy : 0.f;
    }

    // Support function for GJK
    Vec2 support(const Vec2& dir) const {
        return { dir.x>0 ? max.x : min.x, dir.y>0 ? max.y : min.y };
    }

    static AABB2D merge(const AABB2D& a, const AABB2D& b) {
        return { Vec2::Min(a.min,b.min), Vec2::Max(a.max,b.max) };
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  AABB3D — axis-aligned bounding box in 3D
// ─────────────────────────────────────────────────────────────────────────────
struct AABB3D {
    Vec3 min, max;

    AABB3D() : min(kInfinity), max(-kInfinity) {}
    AABB3D(const Vec3& min, const Vec3& max) : min(min), max(max) {}
    static AABB3D fromCenterHalfExtents(const Vec3& center, const Vec3& halfExtents) {
        return { center-halfExtents, center+halfExtents };
    }

    Vec3  center()  const { return (min+max)*0.5f; }
    Vec3  extents() const { return (max-min)*0.5f; }
    Vec3  size()    const { return max-min; }
    float volume()  const { Vec3 s=size(); return s.x*s.y*s.z; }
    float surfaceArea() const {
        Vec3 s=size();
        return 2.f*(s.x*s.y + s.y*s.z + s.z*s.x);
    }
    bool valid() const { return min.x<=max.x && min.y<=max.y && min.z<=max.z; }

    void expand(const Vec3& p)    { min=Vec3::Min(min,p); max=Vec3::Max(max,p); }
    void expand(const AABB3D& o)  { min=Vec3::Min(min,o.min); max=Vec3::Max(max,o.max); }
    void fatten(float margin)     { min=min-Vec3(margin); max=max+Vec3(margin); }

    bool contains(const Vec3& p)   const {
        return p.x>=min.x&&p.x<=max.x&&p.y>=min.y&&p.y<=max.y&&p.z>=min.z&&p.z<=max.z;
    }
    bool overlaps(const AABB3D& o) const {
        return min.x<=o.max.x&&max.x>=o.min.x&&
               min.y<=o.max.y&&max.y>=o.min.y&&
               min.z<=o.max.z&&max.z>=o.min.z;
    }

    // Fast ray-AABB slab test
    bool raycast(const Ray& ray, float& tHit) const {
        Vec3 inv = ray.invDir();
        Vec3 t0  = (min - ray.origin) * inv;
        Vec3 t1  = (max - ray.origin) * inv;
        Vec3 tNear = Vec3::Min(t0,t1);
        Vec3 tFar  = Vec3::Max(t0,t1);
        float tmin = std::max({tNear.x, tNear.y, tNear.z, ray.tMin});
        float tmax = std::min({tFar.x,  tFar.y,  tFar.z,  ray.tMax});
        if(tmin>tmax) return false;
        tHit = tmin;
        return true;
    }

    // Support function for GJK
    Vec3 support(const Vec3& dir) const {
        return { dir.x>0?max.x:min.x, dir.y>0?max.y:min.y, dir.z>0?max.z:min.z };
    }

    static AABB3D merge(const AABB3D& a, const AABB3D& b) {
        return { Vec3::Min(a.min,b.min), Vec3::Max(a.max,b.max) };
    }
    // Half-extents along an axis (for SAT)
    float project(const Vec3& axis) const {
        Vec3 e = extents();
        return e.x*std::abs(axis.x) + e.y*std::abs(axis.y) + e.z*std::abs(axis.z);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  OBB2D — oriented bounding box 2D
// ─────────────────────────────────────────────────────────────────────────────
struct OBB2D {
    Vec2  center;
    Vec2  axes[2];      // local X and Y (unit vectors)
    Vec2  halfExtents;

    OBB2D() : center(0), halfExtents(1) {
        axes[0]={1,0}; axes[1]={0,1};
    }
    OBB2D(const Vec2& c, const Vec2& he, float angle)
        : center(c), halfExtents(he) {
        float cs=cosf(angle), sn=sinf(angle);
        axes[0]={cs,sn}; axes[1]={-sn,cs};
    }

    Vec2 support(const Vec2& dir) const {
        float d0 = dir.dot(axes[0]);
        float d1 = dir.dot(axes[1]);
        return center + axes[0]*(d0>0?halfExtents.x:-halfExtents.x)
                      + axes[1]*(d1>0?halfExtents.y:-halfExtents.y);
    }
    AABB2D toAABB() const {
        Vec2 x = axes[0]*halfExtents.x;
        Vec2 y = axes[1]*halfExtents.y;
        Vec2 ext{ std::abs(x.x)+std::abs(y.x), std::abs(x.y)+std::abs(y.y) };
        return { center-ext, center+ext };
    }
    // Project onto axis for SAT
    Interval project(const Vec2& axis) const {
        float c = axis.dot(center);
        float r = std::abs(axis.dot(axes[0]))*halfExtents.x
                + std::abs(axis.dot(axes[1]))*halfExtents.y;
        return { c-r, c+r };
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  OBB3D — oriented bounding box 3D
// ─────────────────────────────────────────────────────────────────────────────
struct OBB3D {
    Vec3 center;
    Vec3 axes[3];       // local X, Y, Z (unit vectors)
    Vec3 halfExtents;

    OBB3D() : center(0), halfExtents(1) {
        axes[0]=Vec3::Right(); axes[1]=Vec3::Up(); axes[2]=Vec3::Forward();
    }
    OBB3D(const Vec3& c, const Vec3& he, const Mat3& rot)
        : center(c), halfExtents(he) {
        axes[0]=rot.col(0); axes[1]=rot.col(1); axes[2]=rot.col(2);
    }
    OBB3D(const Vec3& c, const Vec3& he, const Quat& q)
        : center(c), halfExtents(he) {
        Mat3 r = q.toMat3();
        axes[0]=r.col(0); axes[1]=r.col(1); axes[2]=r.col(2);
    }

    Vec3 support(const Vec3& dir) const {
        return center
            + axes[0]*(dir.dot(axes[0])>0 ? halfExtents.x : -halfExtents.x)
            + axes[1]*(dir.dot(axes[1])>0 ? halfExtents.y : -halfExtents.y)
            + axes[2]*(dir.dot(axes[2])>0 ? halfExtents.z : -halfExtents.z);
    }
    AABB3D toAABB() const {
        Vec3 ext = {
            std::abs(axes[0].x)*halfExtents.x + std::abs(axes[1].x)*halfExtents.y + std::abs(axes[2].x)*halfExtents.z,
            std::abs(axes[0].y)*halfExtents.x + std::abs(axes[1].y)*halfExtents.y + std::abs(axes[2].y)*halfExtents.z,
            std::abs(axes[0].z)*halfExtents.x + std::abs(axes[1].z)*halfExtents.y + std::abs(axes[2].z)*halfExtents.z,
        };
        return { center-ext, center+ext };
    }
    Interval project(const Vec3& axis) const {
        float c = axis.dot(center);
        float r = std::abs(axis.dot(axes[0]))*halfExtents.x
                + std::abs(axis.dot(axes[1]))*halfExtents.y
                + std::abs(axis.dot(axes[2]))*halfExtents.z;
        return { c-r, c+r };
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Sphere
// ─────────────────────────────────────────────────────────────────────────────
struct Sphere {
    Vec3  center;
    float radius;

    Sphere() : center(0), radius(1) {}
    Sphere(const Vec3& c, float r) : center(c), radius(r) {}

    Vec3 support(const Vec3& dir) const { return center + dir.normalized()*radius; }
    AABB3D toAABB() const {
        Vec3 r(radius);
        return { center-r, center+r };
    }
    bool contains(const Vec3& p) const { return (p-center).lengthSq()<=radius*radius; }
    bool overlaps(const Sphere& o) const {
        float d = radius+o.radius;
        return (center-o.center).lengthSq() <= d*d;
    }
    float volume() const { return (4.f/3.f)*kPi*radius*radius*radius; }
    float surfaceArea() const { return 4.f*kPi*radius*radius; }
};

// Circle2D is just a 2D sphere
struct Circle2D {
    Vec2  center;
    float radius;

    Circle2D() : center(0), radius(1) {}
    Circle2D(const Vec2& c, float r) : center(c), radius(r) {}

    Vec2 support(const Vec2& dir) const { return center + dir.normalized()*radius; }
    AABB2D toAABB() const {
        Vec2 r(radius);
        return { center-r, center+r };
    }
    bool overlaps(const Circle2D& o) const {
        float d = radius+o.radius;
        return (center-o.center).lengthSq() <= d*d;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Capsule3D — segment swept sphere
// ─────────────────────────────────────────────────────────────────────────────
struct Capsule3D {
    Vec3  a, b;         // segment endpoints
    float radius;

    Capsule3D() : a(0,0,0), b(0,1,0), radius(0.5f) {}
    Capsule3D(const Vec3& a, const Vec3& b, float r) : a(a), b(b), radius(r) {}

    Vec3 support(const Vec3& dir) const {
        Vec3 ab = b-a;
        float t = dir.dot(ab);
        Vec3 tip = (t>0) ? b : a;
        return tip + dir.normalized()*radius;
    }
    AABB3D toAABB() const {
        Vec3 r(radius);
        AABB3D box(Vec3::Min(a,b)-r, Vec3::Max(a,b)+r);
        return box;
    }
    float length() const { return (b-a).length(); }
    Vec3  center() const { return (a+b)*0.5f; }
    Vec3  dir()    const { return (b-a).normalized(); }

    // Closest point on segment to point p
    Vec3 closestPoint(const Vec3& p) const {
        Vec3 ab=b-a;
        float len2 = ab.lengthSq();
        if(len2 < kEpsilon*kEpsilon) return a;  // degenerate: point capsule
        float t = (p-a).dot(ab)/len2;
        t = std::clamp(t,0.f,1.f);
        return a+ab*t;
    }
};

struct Capsule2D {
    Vec2  a, b;
    float radius;
    Capsule2D() : a(0,0), b(0,1), radius(0.5f) {}
    Capsule2D(const Vec2& a, const Vec2& b, float r) : a(a), b(b), radius(r) {}

    Vec2 support(const Vec2& dir) const {
        float ta = dir.dot(a), tb = dir.dot(b);
        Vec2 tip = tb>ta ? b : a;
        return tip + dir.normalized()*radius;
    }
    AABB2D toAABB() const {
        Vec2 r(radius);
        return { Vec2::Min(a,b)-r, Vec2::Max(a,b)+r };
    }
    Vec2 closestPoint(const Vec2& p) const {
        Vec2 ab=b-a;
        float t = (p-a).dot(ab)/ab.lengthSq();
        t = std::clamp(t,0.f,1.f);
        return a+ab*t;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  ConvexHull — arbitrary convex polyhedron (3D) or polygon (2D)
// ─────────────────────────────────────────────────────────────────────────────
struct ConvexHull3D {
    std::vector<Vec3> vertices;
    // Faces: each face is a list of vertex indices (CCW from outside)
    struct Face { std::vector<int> indices; Vec3 normal; };
    std::vector<Face> faces;
    // Edges: pairs of vertex indices
    struct Edge { int a, b; };
    std::vector<Edge> edges;

    AABB3D aabb;    // cached

    void buildAABB() {
        aabb = AABB3D();
        for(auto& v : vertices) aabb.expand(v);
    }

    // GJK support function — O(n) but can be improved with hill-climbing
    Vec3 support(const Vec3& dir) const {
        float best = -kInfinity;
        Vec3  result(0);
        for(auto& v : vertices) {
            float d = v.dot(dir);
            if(d>best) { best=d; result=v; }
        }
        return result;
    }

    // Transform all vertices by a transform
    ConvexHull3D transformed(const Transform& t) const {
        ConvexHull3D out;
        out.vertices.resize(vertices.size());
        for(size_t i=0;i<vertices.size();i++)
            out.vertices[i] = t.transformPoint(vertices[i]);
        out.faces  = faces;
        out.edges  = edges;
        out.buildAABB();
        return out;
    }
};

struct ConvexPolygon2D {
    std::vector<Vec2> vertices;   // CCW order
    AABB2D aabb;

    void buildAABB() {
        aabb = AABB2D();
        for(auto& v : vertices) aabb.expand(v);
    }
    Vec2 support(const Vec2& dir) const {
        float best = -kInfinity;
        Vec2  result(0);
        for(auto& v : vertices) {
            float d = v.dot(dir);
            if(d>best) { best=d; result=v; }
        }
        return result;
    }
    // Normals for SAT
    std::vector<Vec2> normals() const {
        std::vector<Vec2> n;
        n.reserve(vertices.size());
        for(size_t i=0;i<vertices.size();i++){
            Vec2 edge = vertices[(i+1)%vertices.size()] - vertices[i];
            n.push_back(edge.perp().normalized());
        }
        return n;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  TriangleMesh — for static geometry (terrain, walls)
// ─────────────────────────────────────────────────────────────────────────────
struct Triangle {
    Vec3 v[3];
    Vec3 normal() const { return (v[1]-v[0]).cross(v[2]-v[0]).normalized(); }
    Vec3 centroid() const { return (v[0]+v[1]+v[2])*(1.f/3.f); }
    AABB3D aabb() const {
        return { Vec3::Min(v[0], Vec3::Min(v[1],v[2])),
                 Vec3::Max(v[0], Vec3::Max(v[1],v[2])) };
    }
    // Closest point on triangle to p
    Vec3 closestPoint(const Vec3& p) const;
};

struct TriangleMesh {
    std::vector<Vec3>    vertices;
    std::vector<uint32_t>indices;  // triplets
    AABB3D               aabb;
    // BVH over triangles is built separately

    size_t triangleCount() const { return indices.size()/3; }
    Triangle getTriangle(size_t i) const {
        return { vertices[indices[i*3+0]],
                 vertices[indices[i*3+1]],
                 vertices[indices[i*3+2]] };
    }
    void buildAABB() {
        aabb = AABB3D();
        for(auto& v : vertices) aabb.expand(v);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Heightfield — terrain grid
// ─────────────────────────────────────────────────────────────────────────────
struct Heightfield {
    std::vector<float> heights;  // row-major [z][x]
    int   cols, rows;
    Vec3  origin;
    float cellSizeX, cellSizeZ;
    float minHeight, maxHeight;

    float getHeight(int x, int z) const {
        x = std::clamp(x,0,cols-1);
        z = std::clamp(z,0,rows-1);
        return heights[z*cols+x];
    }
    float heightAt(float wx, float wz) const {
        float lx = (wx-origin.x)/cellSizeX;
        float lz = (wz-origin.z)/cellSizeZ;
        int   x0=(int)lx, z0=(int)lz;
        float fx=lx-x0, fz=lz-z0;
        float h00=getHeight(x0,z0),   h10=getHeight(x0+1,z0);
        float h01=getHeight(x0,z0+1), h11=getHeight(x0+1,z0+1);
        return h00*(1-fx)*(1-fz) + h10*fx*(1-fz) + h01*(1-fx)*fz + h11*fx*fz;
    }
    AABB3D toAABB() const {
        return { origin,
                 origin + Vec3(cols*cellSizeX, maxHeight-minHeight, rows*cellSizeZ) };
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  CollisionShape — type-erased wrapper (tag-union style, no vtable)
// ─────────────────────────────────────────────────────────────────────────────
struct CollisionShape {
    ShapeType type;
    union {
        Circle2D       circle2D;
        AABB2D         aabb2D;
        OBB2D          obb2D;
        Capsule2D      capsule2D;
        Sphere         sphere;
        AABB3D         aabb3D;
        OBB3D          obb3D;
        Capsule3D      capsule3D;
    };
    // Heap shapes (polygon / mesh / hull)
    std::shared_ptr<ConvexPolygon2D> polygon2D;
    std::shared_ptr<ConvexHull3D>    convexHull;
    std::shared_ptr<TriangleMesh>    triMesh;
    std::shared_ptr<Heightfield>     heightfield;

    CollisionShape() : type(ShapeType::Sphere) { new(&sphere) Sphere(); }

    // ── Factory helpers ──────────────────────────────────────────────────────
    static CollisionShape makeSphere(const Vec3& c, float r) {
        CollisionShape s; s.type=ShapeType::Sphere;
        new(&s.sphere) Sphere(c,r); return s;
    }
    static CollisionShape makeCircle2D(const Vec2& c, float r) {
        CollisionShape s; s.type=ShapeType::Circle2D;
        new(&s.circle2D) Circle2D(c,r); return s;
    }
    static CollisionShape makeAABB3D(const Vec3& min, const Vec3& max) {
        CollisionShape s; s.type=ShapeType::AABB3D;
        new(&s.aabb3D) AABB3D(min,max); return s;
    }
    static CollisionShape makeAABB2D(const Vec2& min, const Vec2& max) {
        CollisionShape s; s.type=ShapeType::AABB2D;
        new(&s.aabb2D) AABB2D(min,max); return s;
    }
    static CollisionShape makeOBB3D(const Vec3& c, const Vec3& he, const Quat& q) {
        CollisionShape s; s.type=ShapeType::OBB3D;
        new(&s.obb3D) OBB3D(c,he,q); return s;
    }
    static CollisionShape makeCapsule3D(const Vec3& a, const Vec3& b, float r) {
        CollisionShape s; s.type=ShapeType::Capsule3D;
        new(&s.capsule3D) Capsule3D(a,b,r); return s;
    }
    static CollisionShape makeConvexHull(std::shared_ptr<ConvexHull3D> hull) {
        CollisionShape s; s.type=ShapeType::ConvexHull;
        s.convexHull=std::move(hull); return s;
    }
    static CollisionShape makeTriangleMesh(std::shared_ptr<TriangleMesh> mesh) {
        CollisionShape s; s.type=ShapeType::TriangleMesh;
        s.triMesh=std::move(mesh); return s;
    }
    static CollisionShape makeHeightfield(std::shared_ptr<Heightfield> hf) {
        CollisionShape s; s.type=ShapeType::Heightfield;
        s.heightfield=std::move(hf); return s;
    }

    // Get world-space AABB (for broadphase)
    AABB3D worldAABB(const Transform& t) const;

    // GJK support function dispatched by type
    Vec3 support3D(const Vec3& dir, const Transform& t) const;
    Vec2 support2D(const Vec2& dir) const;

    bool is2D() const {
        return type==ShapeType::Circle2D || type==ShapeType::AABB2D ||
               type==ShapeType::OBB2D   || type==ShapeType::Capsule2D ||
               type==ShapeType::Polygon2D;
    }
};

} // namespace col
