#pragma once
#include "shapes.h"
#include <array>
#include <vector>

namespace col {

// ─────────────────────────────────────────────────────────────────────────────
//  Contact result
// ─────────────────────────────────────────────────────────────────────────────
struct ContactPoint {
    Vec3  positionA;      // contact point on shape A (world space)
    Vec3  positionB;      // contact point on shape B (world space)
    Vec3  normal;         // from A to B
    float penetrationDepth;
    float restitution = 0.f;
    float friction    = 0.3f;
};

struct ContactManifold {
    static constexpr int kMaxContacts = 4;
    ContactPoint points[kMaxContacts];
    int          count         = 0;
    bool         hit           = false;
    uint64_t     bodyA         = 0;
    uint64_t     bodyB         = 0;
    uint32_t     frameStamp    = 0;

    void addPoint(const ContactPoint& cp) {
        if(count < kMaxContacts) points[count++] = cp;
    }
    void clear() { count=0; hit=false; }
};

// 2D contact
struct ContactManifold2D {
    static constexpr int kMaxContacts = 2;
    struct Point { Vec2 position; float penetration; };
    Point  points[kMaxContacts];
    int    count  = 0;
    Vec2   normal = {0,1};
    bool   hit    = false;
};

// ─────────────────────────────────────────────────────────────────────────────
//  GJK — Gilbert-Johnson-Keerthi (3D)
//  Returns true if shapes overlap; populates simplex for EPA
// ─────────────────────────────────────────────────────────────────────────────
struct GJKSimplex {
    Vec3 pts[4];
    int  n = 0;
    void push(const Vec3& v) {
        assert(n<4);
        pts[n++] = v;
    }
    Vec3& operator[](int i) { return pts[i]; }
    const Vec3& operator[](int i) const { return pts[i]; }
};

// Minkowski difference support
inline Vec3 minkowskiSupport(
    const CollisionShape& a, const Transform& ta,
    const CollisionShape& b, const Transform& tb,
    const Vec3& dir)
{
    Vec3 sa = a.support3D( dir, ta);
    Vec3 sb = b.support3D(-dir, tb);
    return sa - sb;
}

class GJK3D {
public:
    static constexpr int kMaxIter = 64;

    struct Result {
        bool      hit;
        GJKSimplex simplex;
        float     distance;   // valid when !hit
        Vec3      closestA, closestB;
    };

    static Result test(
        const CollisionShape& a, const Transform& ta,
        const CollisionShape& b, const Transform& tb)
    {
        Result res; res.hit = false;
        GJKSimplex& s = res.simplex;

        Vec3 dir = tb.position - ta.position;
        if(dir.lengthSq() < kEpsilon*kEpsilon) dir = Vec3::Right();

        Vec3 sup = minkowskiSupport(a,ta,b,tb,dir);
        s.push(sup);
        dir = -sup;

        for(int iter=0; iter<kMaxIter; iter++) {
            if(dir.lengthSq() < kEpsilon*kEpsilon) { res.hit=true; break; }

            sup = minkowskiSupport(a,ta,b,tb,dir);

            if(sup.dot(dir) < -kEpsilon) {
                // No intersection, find closest distance
                res.distance = std::sqrt(closestDistanceSq(s));
                return res;
            }
            s.push(sup);

            if(doSimplex(s, dir)) {
                res.hit = true;
                return res;
            }
        }
        res.hit = true;
        return res;
    }

private:
    // Returns true if origin is enclosed
    static bool doSimplex(GJKSimplex& s, Vec3& dir) {
        switch(s.n) {
        case 2: return doLine(s, dir);
        case 3: return doTriangle(s, dir);
        case 4: return doTetrahedron(s, dir);
        }
        return false;
    }

    static bool doLine(GJKSimplex& s, Vec3& dir) {
        Vec3 a=s[1], b=s[0];
        Vec3 ab=b-a, ao=-a;
        if(ab.dot(ao)>0) {
            dir = ab.cross(ao).cross(ab);
        } else {
            s.pts[0]=a; s.n=1;
            dir = ao;
        }
        return false;
    }

    static bool doTriangle(GJKSimplex& s, Vec3& dir) {
        Vec3 a=s[2], b=s[1], c=s[0];
        Vec3 ab=b-a, ac=c-a, ao=-a;
        Vec3 abc = ab.cross(ac);

        if(abc.cross(ac).dot(ao)>0) {
            if(ac.dot(ao)>0) {
                s.pts[0]=c; s.pts[1]=a; s.n=2;
                dir = ac.cross(ao).cross(ac);
            } else {
                return doLine2(s, a, b, ao, dir);
            }
        } else if(ab.cross(abc).dot(ao)>0) {
            return doLine2(s, a, b, ao, dir);
        } else {
            if(abc.dot(ao)>0) {
                dir = abc;
            } else {
                std::swap(s.pts[0],s.pts[1]);
                dir = -abc;
            }
        }
        return false;
    }

    static bool doLine2(GJKSimplex& s, const Vec3& a, const Vec3& b,
                        const Vec3& ao, Vec3& dir) {
        if(b.dot(ao)>0) {  // wait — using (b-a).dot(ao)
            if((b-a).dot(ao)>0) {
                s.pts[0]=b; s.pts[1]=a; s.n=2;
                dir = (b-a).cross(ao).cross(b-a);
            } else {
                s.pts[0]=a; s.n=1;
                dir = ao;
            }
        } else {
            s.pts[0]=a; s.n=1;
            dir = ao;
        }
        return false;
    }

    static bool doTetrahedron(GJKSimplex& s, Vec3& dir) {
        Vec3 a=s[3], b=s[2], c=s[1], d=s[0];
        Vec3 ab=b-a, ac=c-a, ad=d-a, ao=-a;
        Vec3 abc=ab.cross(ac), acd=ac.cross(ad), adb=ad.cross(ab);

        auto sameDir = [](const Vec3& v, const Vec3& w){ return v.dot(w)>0; };

        if(sameDir(abc,ao)) {
            s.pts[0]=c; s.pts[1]=b; s.pts[2]=a; s.n=3;
            return doTriangle(s,dir);
        }
        if(sameDir(acd,ao)) {
            s.pts[0]=d; s.pts[1]=c; s.pts[2]=a; s.n=3;
            return doTriangle(s,dir);
        }
        if(sameDir(adb,ao)) {
            s.pts[0]=b; s.pts[1]=d; s.pts[2]=a; s.n=3;
            return doTriangle(s,dir);
        }
        return true;  // origin inside tetrahedron
    }

    static float closestDistanceSq(const GJKSimplex& s) {
        // Simplified: return length of last added point
        if(s.n==0) return kInfinity;
        return s.pts[s.n-1].lengthSq();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  EPA — Expanding Polytope Algorithm
//  Finds penetration depth + contact normal from GJK simplex
// ─────────────────────────────────────────────────────────────────────────────
class EPA3D {
public:
    static constexpr int   kMaxIter  = 64;
    static constexpr float kEpsilonEPA = 1e-4f;

    struct Result {
        Vec3  normal;
        float depth;
        bool  valid;
    };

    static Result solve(
        const GJKSimplex& simplex,
        const CollisionShape& a, const Transform& ta,
        const CollisionShape& b, const Transform& tb)
    {
        // Convert simplex to polytope faces
        std::vector<Vec3> verts = {simplex[0], simplex[1], simplex[2], simplex[3]};
        std::vector<std::array<int,3>> faces = {
            {0,1,2},{0,3,1},{0,2,3},{1,3,2}
        };
        // Ensure faces point outward
        for(auto& f : faces) ensureOutward(verts,f);

        for(int iter=0; iter<kMaxIter; iter++) {
            // Find closest face to origin
            float    minDist = kInfinity;
            Vec3     minNorm(0);
            int      minFace = 0;

            for(int i=0;i<(int)faces.size();i++) {
                auto& f = faces[i];
                Vec3 n = faceNormal(verts,f);
                float d = n.dot(verts[f[0]]);
                if(d<0) { n=-n; d=-d; }
                if(d<minDist) { minDist=d; minNorm=n; minFace=i; }
            }

            // Find new support point along minNorm
            Vec3 sup = minkowskiSupport(a,ta,b,tb,minNorm);
            float newDist = minNorm.dot(sup);

            if(std::abs(newDist-minDist) < kEpsilonEPA) {
                return { minNorm.normalized(), minDist, true };
            }

            // Expand polytope — find silhouette edges
            std::vector<std::pair<int,int>> silhouette;
            std::vector<bool> remove(faces.size(), false);
            findSilhouette(verts, faces, sup, remove, silhouette);

            // Remove back-facing
            for(int i=(int)faces.size()-1; i>=0; i--)
                if(remove[i]) faces.erase(faces.begin()+i);

            // Add new vertex
            int newIdx = (int)verts.size();
            verts.push_back(sup);

            // Patch with new faces
            for(auto& [i0,i1] : silhouette) {
                faces.push_back({i0, i1, newIdx});
                ensureOutward(verts, faces.back());
            }
        }
        return { Vec3(0,1,0), 0.f, false };
    }

private:
    static Vec3 faceNormal(const std::vector<Vec3>& v,
                           const std::array<int,3>& f) {
        return (v[f[1]]-v[f[0]]).cross(v[f[2]]-v[f[0]]).normalized();
    }
    static void ensureOutward(const std::vector<Vec3>& v,
                              std::array<int,3>& f) {
        Vec3 n = faceNormal(v,f);
        if(n.dot(v[f[0]]) < 0) std::swap(f[1],f[2]);
    }
    static void findSilhouette(
        const std::vector<Vec3>& verts,
        const std::vector<std::array<int,3>>& faces,
        const Vec3& sup,
        std::vector<bool>& remove,
        std::vector<std::pair<int,int>>& silhouette)
    {
        for(int i=0;i<(int)faces.size();i++) {
            auto& f = faces[i];
            Vec3 n = faceNormal(verts,f);
            if(n.dot(sup - verts[f[0]]) > 0) {
                remove[i] = true;
                // Each edge of this face is a silhouette candidate
                for(int k=0;k<3;k++){
                    int ia=f[k], ib=f[(k+1)%3];
                    // Add if not shared by another removed face
                    bool shared=false;
                    for(int j=0;j<(int)faces.size();j++){
                        if(j==i||!remove[j]) continue;
                        auto& g=faces[j];
                        for(int l=0;l<3;l++){
                            if((g[l]==ia&&g[(l+1)%3]==ib)||
                               (g[l]==ib&&g[(l+1)%3]==ia)){
                                shared=true; break;
                            }
                        }
                        if(shared) break;
                    }
                    if(!shared) silhouette.push_back({ia,ib});
                }
            }
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  SAT — Separating Axis Theorem (fast for boxes / OBBs)
// ─────────────────────────────────────────────────────────────────────────────
class SAT3D {
public:
    struct Result {
        bool  hit;
        Vec3  normal;   // separation axis (points from A to B)
        float depth;    // penetration depth (>0 if overlapping)
    };

    // OBB vs OBB — 15 axes test
    static Result testOBBOBB(const OBB3D& a, const OBB3D& b) {
        // 3 face normals of A
        // 3 face normals of B
        // 3x3 = 9 cross products of edges
        float minOverlap = kInfinity;
        Vec3  bestAxis(0);

        auto test = [&](const Vec3& axis) -> bool {
            if(axis.lengthSq() < kEpsilon*kEpsilon) return true;
            Vec3  nAxis = axis.normalized();
            Interval ia = a.project(nAxis);
            Interval ib = b.project(nAxis);
            if(!ia.overlaps(ib)) return false;
            float ov = ia.overlap(ib);
            if(ov < minOverlap) { minOverlap=ov; bestAxis=nAxis; }
            return true;
        };

        // A's axes
        for(int i=0;i<3;i++) if(!test(a.axes[i])) return {false,{},0};
        // B's axes
        for(int i=0;i<3;i++) if(!test(b.axes[i])) return {false,{},0};
        // Cross products
        for(int i=0;i<3;i++) for(int j=0;j<3;j++) {
            Vec3 cross = a.axes[i].cross(b.axes[j]);
            if(!test(cross)) return {false,{},0};
        }

        // Ensure normal points from A to B
        Vec3 d = b.center - a.center;
        if(d.dot(bestAxis)<0) bestAxis=-bestAxis;

        return { true, bestAxis, minOverlap };
    }

    // AABB vs AABB (degenerate SAT, 3 axes)
    static Result testAABBAABB(const AABB3D& a, const AABB3D& b) {
        Vec3 d = b.center() - a.center();
        Vec3 overlap = a.extents() + b.extents() - d.abs();

        if(overlap.x<=0||overlap.y<=0||overlap.z<=0) return {false,{},0};

        Vec3 n(0); float depth;
        if(overlap.x<overlap.y && overlap.x<overlap.z) {
            n={d.x>0?1.f:-1.f,0,0}; depth=overlap.x;
        } else if(overlap.y<overlap.z) {
            n={0,d.y>0?1.f:-1.f,0}; depth=overlap.y;
        } else {
            n={0,0,d.z>0?1.f:-1.f}; depth=overlap.z;
        }
        return { true, n, depth };
    }

    // Sphere vs Sphere
    static Result testSphereSphere(const Sphere& a, const Sphere& b) {
        Vec3  d    = b.center - a.center;
        float distSq = d.lengthSq();
        float radSum = a.radius + b.radius;
        if(distSq >= radSum*radSum) return {false,{},0};
        float dist = std::sqrt(distSq);
        Vec3  n    = dist>kEpsilon ? d/dist : Vec3::Up();
        return { true, n, radSum-dist };
    }

    // Sphere vs AABB
    static Result testSphereAABB(const Sphere& s, const AABB3D& b) {
        Vec3 closest = Vec3::Min(Vec3::Max(s.center, b.min), b.max);
        Vec3 d       = s.center - closest;
        float distSq = d.lengthSq();
        if(distSq >= s.radius*s.radius) return {false,{},0};
        float dist = std::sqrt(distSq);
        Vec3  n    = dist>kEpsilon ? d/dist : Vec3::Up();
        return { true, n, s.radius-dist };
    }

    // Sphere vs OBB
    static Result testSphereOBB(const Sphere& s, const OBB3D& obb) {
        // Transform sphere center into OBB local space
        Vec3 local = s.center - obb.center;
        Vec3 localSphere = {
            local.dot(obb.axes[0]),
            local.dot(obb.axes[1]),
            local.dot(obb.axes[2])
        };
        Vec3 clamped = Vec3::Min(Vec3::Max(localSphere, -obb.halfExtents), obb.halfExtents);
        Vec3 closest = obb.center
            + obb.axes[0]*clamped.x
            + obb.axes[1]*clamped.y
            + obb.axes[2]*clamped.z;
        Vec3  d    = s.center - closest;
        float distSq = d.lengthSq();
        if(distSq >= s.radius*s.radius) return {false,{},0};
        float dist = std::sqrt(distSq);
        Vec3  n    = dist>kEpsilon ? d/dist : Vec3::Up();
        return { true, n, s.radius-dist };
    }

    // Capsule vs Sphere
    static Result testCapsuleSphere(const Capsule3D& cap, const Sphere& s) {
        Vec3 closest = cap.closestPoint(s.center);
        return testSphereSphere({closest, cap.radius}, s);
    }

    // Capsule vs Capsule
    static Result testCapsuleCapsule(const Capsule3D& a, const Capsule3D& b) {
        // Closest point between two segments
        Vec3 d1=a.b-a.a, d2=b.b-b.a, r=a.a-b.a;
        float e=d2.dot(d2), f=d2.dot(r);
        float s,t;
        if(e<kEpsilon) {
            s=0; t=f/e;
        } else {
            float c=d1.dot(r);
            float dd=d1.dot(d1);
            float denom=dd*e - d1.dot(d2)*d1.dot(d2);
            if(std::abs(denom)>kEpsilon) {
                s=std::clamp((d1.dot(d2)*f-c*e)/denom,0.f,1.f);
            } else { s=0; }
            t=std::clamp((d1.dot(d2)*s+f)/e,0.f,1.f);
            s=std::clamp((t*d1.dot(d2)-c)/dd,0.f,1.f);
            t=std::clamp((s*d1.dot(d2)+f)/e,0.f,1.f);
        }
        Vec3 cA = a.a+d1*s;
        Vec3 cB = b.a+d2*t;
        return testSphereSphere({cA,a.radius},{cB,b.radius});
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  SAT 2D
// ─────────────────────────────────────────────────────────────────────────────
class SAT2D {
public:
    struct Result {
        bool  hit;
        Vec2  normal;
        float depth;
    };

    static Result testOBBOBB(const OBB2D& a, const OBB2D& b) {
        float minOv = kInfinity;
        Vec2  bestN(0);
        Vec2  axes[4] = { a.axes[0], a.axes[1], b.axes[0], b.axes[1] };

        for(auto& ax : axes) {
            Interval ia = a.project(ax);
            Interval ib = b.project(ax);
            if(!ia.overlaps(ib)) return {false,{},0};
            float ov = ia.overlap(ib);
            if(ov < minOv) { minOv=ov; bestN=ax; }
        }
        Vec2 d = b.center - a.center;
        if(d.dot(bestN)<0) bestN=-bestN;
        return { true, bestN, minOv };
    }

    static Result testCircleCircle(const Circle2D& a, const Circle2D& b) {
        Vec2  d   = b.center - a.center;
        float sum = a.radius + b.radius;
        float len = d.length();
        if(len >= sum) return {false,{},0};
        Vec2 n = len>kEpsilon ? d/len : Vec2(0,1);
        return { true, n, sum-len };
    }

    static Result testPolygonPolygon(const ConvexPolygon2D& a, const ConvexPolygon2D& b) {
        float minOv = kInfinity;
        Vec2  bestN(0);

        auto testAxes = [&](const ConvexPolygon2D& poly, bool& separated) {
            for(size_t i=0;i<poly.vertices.size();i++) {
                Vec2 edge = poly.vertices[(i+1)%poly.vertices.size()] - poly.vertices[i];
                Vec2 axis = edge.perp().normalized();
                Interval ia, ib;
                for(auto& v : a.vertices) ia.expand(axis.dot(v));
                for(auto& v : b.vertices) ib.expand(axis.dot(v));
                if(!ia.overlaps(ib)) { separated=true; return; }
                float ov = ia.overlap(ib);
                if(ov < minOv) { minOv=ov; bestN=axis; }
            }
        };

        bool sep = false;
        testAxes(a, sep); if(sep) return {false,{},0};
        testAxes(b, sep); if(sep) return {false,{},0};

        Vec2 d = Vec2(0);
        for(auto& v : b.vertices) d += v;
        for(auto& v : a.vertices) d -= v;
        if(d.dot(bestN)<0) bestN=-bestN;
        return { true, bestN, minOv };
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Narrowphase dispatcher — picks the best algorithm per shape pair
// ─────────────────────────────────────────────────────────────────────────────
class Narrowphase {
public:
    // Full 3D contact generation
    static ContactManifold generateContacts(
        const CollisionShape& a, const Transform& ta,
        const CollisionShape& b, const Transform& tb)
    {
        ContactManifold m;
        m.hit = false;

        // Fast-path dispatch by type pair
        using ST = ShapeType;
        auto typeA = a.type, typeB = b.type;

        // Sphere-Sphere
        if(typeA==ST::Sphere && typeB==ST::Sphere) {
            auto res = SAT3D::testSphereSphere(a.sphere, b.sphere);
            if(res.hit) fillManifold(m, a.sphere.center, b.sphere.center, res.normal, res.depth);
            return m;
        }
        // Sphere-AABB
        if(typeA==ST::Sphere && typeB==ST::AABB3D) {
            auto res = SAT3D::testSphereAABB(a.sphere, b.aabb3D);
            if(res.hit) {
                Vec3 cA=a.sphere.center, cB=cA-res.normal*res.depth;
                fillManifold(m, cA, cB, res.normal, res.depth);
            }
            return m;
        }
        if(typeA==ST::AABB3D && typeB==ST::Sphere) {
            auto res = SAT3D::testSphereAABB(b.sphere, a.aabb3D);
            if(res.hit) {
                Vec3 cB=b.sphere.center, cA=cB-res.normal*res.depth;
                fillManifold(m, cA, cB, -res.normal, res.depth);
            }
            return m;
        }
        // AABB-AABB
        if(typeA==ST::AABB3D && typeB==ST::AABB3D) {
            auto res = SAT3D::testAABBAABB(a.aabb3D, b.aabb3D);
            if(res.hit) {
                Vec3 cA=a.aabb3D.center(), cB=b.aabb3D.center();
                fillManifold(m, cA, cB, res.normal, res.depth);
            }
            return m;
        }
        // OBB-OBB
        if(typeA==ST::OBB3D && typeB==ST::OBB3D) {
            auto res = SAT3D::testOBBOBB(a.obb3D, b.obb3D);
            if(res.hit) fillManifold(m, a.obb3D.center, b.obb3D.center, res.normal, res.depth);
            return m;
        }
        // Sphere-OBB
        if(typeA==ST::Sphere && typeB==ST::OBB3D) {
            auto res = SAT3D::testSphereOBB(a.sphere, b.obb3D);
            if(res.hit) {
                Vec3 cA=a.sphere.center;
                fillManifold(m, cA, cA-res.normal*res.depth, res.normal, res.depth);
            }
            return m;
        }
        // Capsule-Sphere
        if(typeA==ST::Capsule3D && typeB==ST::Sphere) {
            auto res = SAT3D::testCapsuleSphere(a.capsule3D, b.sphere);
            if(res.hit) {
                Vec3 cA=a.capsule3D.closestPoint(b.sphere.center);
                fillManifold(m, cA, b.sphere.center, res.normal, res.depth);
            }
            return m;
        }
        // Capsule-Capsule
        if(typeA==ST::Capsule3D && typeB==ST::Capsule3D) {
            auto res = SAT3D::testCapsuleCapsule(a.capsule3D, b.capsule3D);
            if(res.hit) {
                fillManifold(m, a.capsule3D.center(), b.capsule3D.center(),
                             res.normal, res.depth);
            }
            return m;
        }

        // Generic GJK + EPA fallback
        auto gjk = GJK3D::test(a,ta,b,tb);
        if(gjk.hit) {
            auto epa = EPA3D::solve(gjk.simplex, a,ta,b,tb);
            if(epa.valid) {
                Vec3 cA = a.support3D( epa.normal,ta);
                Vec3 cB = b.support3D(-epa.normal,tb);
                fillManifold(m, cA, cB, epa.normal, epa.depth);
            }
        }
        return m;
    }

    // 2D contact generation
    static ContactManifold2D generateContacts2D(
        const CollisionShape& a, const CollisionShape& b)
    {
        ContactManifold2D m;
        using ST = ShapeType;

        if(a.type==ST::Circle2D && b.type==ST::Circle2D) {
            auto res = SAT2D::testCircleCircle(a.circle2D, b.circle2D);
            if(res.hit) {
                m.hit=true; m.normal=res.normal;
                m.count=1;
                m.points[0] = {
                    a.circle2D.center + res.normal*a.circle2D.radius,
                    res.depth
                };
            }
            return m;
        }
        if(a.type==ST::OBB2D && b.type==ST::OBB2D) {
            auto res = SAT2D::testOBBOBB(a.obb2D, b.obb2D);
            if(res.hit) {
                m.hit=true; m.normal=res.normal;
                m.count=1;
                m.points[0] = { a.obb2D.center + res.normal*res.depth*0.5f, res.depth };
            }
            return m;
        }
        return m;
    }

private:
    static void fillManifold(ContactManifold& m,
                             const Vec3& cA, const Vec3& cB,
                             const Vec3& n, float depth)
    {
        m.hit = true;
        ContactPoint cp;
        cp.positionA       = cA;
        cp.positionB       = cB;
        cp.normal          = n;
        cp.penetrationDepth= depth;
        m.addPoint(cp);
    }
};

} // namespace col
