#pragma once
// debug_draw.h — Debug visualisation interface
// Engine-agnostic: you plug in your own draw primitives via callbacks.
// Works with any renderer (OpenGL, Vulkan, DX12, custom).
//
// Usage:
//   DebugDraw dd;
//   dd.setLineCallback([](const Vec3& a, const Vec3& b, Color c){ myRenderer.drawLine(a,b,c); });
//   dd.setPointCallback([](const Vec3& p, Color c, float size){ myRenderer.drawPoint(p,c,size); });
//   world.debugDraw(dd);

#include "world.h"
#include "broadphase.h"
#include <functional>
#include <string>

namespace col {

// ─────────────────────────────────────────────────────────────────────────────
//  Color — RGBA, 0–255
// ─────────────────────────────────────────────────────────────────────────────
struct Color {
    uint8_t r, g, b, a;
    Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) : r(r),g(g),b(b),a(a) {}
    static Color Red()    { return {220, 60,  60}; }
    static Color Green()  { return { 60, 200,  80}; }
    static Color Blue()   { return { 60, 120, 220}; }
    static Color Yellow() { return {240, 220,  40}; }
    static Color White()  { return {255, 255, 255}; }
    static Color Gray()   { return {140, 140, 140}; }
    static Color Cyan()   { return { 40, 220, 220}; }
    static Color Orange() { return {240, 140,  40}; }
    static Color Magenta(){ return {220,  40, 220}; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  DebugDraw — pluggable draw callbacks + shape drawing helpers
// ─────────────────────────────────────────────────────────────────────────────
class DebugDraw {
public:
    using LineCB   = std::function<void(const Vec3&, const Vec3&, Color)>;
    using PointCB  = std::function<void(const Vec3&, Color, float)>;
    using TextCB   = std::function<void(const Vec3&, const std::string&, Color)>;

    void setLineCallback (LineCB  cb) { lineCB_  = std::move(cb); }
    void setPointCallback(PointCB cb) { pointCB_ = std::move(cb); }
    void setTextCallback (TextCB  cb) { textCB_  = std::move(cb); }

    // ── Draw flags ─────────────────────────────────────────────────────────
    bool drawAABBs      = false;
    bool drawShapes     = true;
    bool drawContacts   = true;
    bool drawNormals    = true;
    bool drawBVH        = false;
    bool drawBVHLeaves  = true;
    bool drawSleeping   = true;
    bool drawVelocities = false;
    bool drawBodyIDs    = false;
    int  bvhDepthLimit  = 4;    // only draw BVH levels 0..N

    // ── Primitive emitters ─────────────────────────────────────────────────
    void line(const Vec3& a, const Vec3& b, Color c = Color::White()) {
        if(lineCB_) lineCB_(a, b, c);
    }
    void point(const Vec3& p, Color c = Color::White(), float size = 4.f) {
        if(pointCB_) pointCB_(p, c, size);
    }
    void text(const Vec3& pos, const std::string& s, Color c = Color::White()) {
        if(textCB_) textCB_(pos, s, c);
    }

    void cross(const Vec3& p, float size = 0.1f, Color c = Color::White()) {
        line(p - Vec3(size,0,0), p + Vec3(size,0,0), c);
        line(p - Vec3(0,size,0), p + Vec3(0,size,0), c);
        line(p - Vec3(0,0,size), p + Vec3(0,0,size), c);
    }

    void arrow(const Vec3& from, const Vec3& dir, float len = 1.f,
               Color c = Color::Yellow())
    {
        Vec3 to = from + dir.normalized() * len;
        line(from, to, c);
        // Arrowhead: two short lines
        Vec3 perp = dir.cross(Vec3::Up()).normalized() * 0.1f * len;
        if(perp.lengthSq() < kEpsilon * kEpsilon)
            perp = dir.cross(Vec3::Right()).normalized() * 0.1f * len;
        line(to, to - dir.normalized() * (0.2f*len) + perp, c);
        line(to, to - dir.normalized() * (0.2f*len) - perp, c);
    }

    // ── Shape draw helpers ─────────────────────────────────────────────────
    void drawSphere(const Vec3& center, float radius,
                    Color c = Color::Green(), int segments = 16)
    {
        float step = 2.f * kPi / segments;
        // 3 circles (XY, XZ, YZ)
        for(int ring = 0; ring < 3; ring++) {
            Vec3 prev(0);
            for(int i = 0; i <= segments; i++) {
                float a = i * step;
                Vec3 cur(0);
                if(ring == 0) cur = {std::cos(a)*radius, std::sin(a)*radius, 0};
                if(ring == 1) cur = {std::cos(a)*radius, 0, std::sin(a)*radius};
                if(ring == 2) cur = {0, std::cos(a)*radius, std::sin(a)*radius};
                cur = cur + center;
                if(i > 0) line(prev, cur, c);
                prev = cur;
            }
        }
    }

    void drawAABB(const AABB3D& aabb, Color c = Color::Gray()) {
        Vec3 mn = aabb.min, mx = aabb.max;
        // 12 edges of the box
        line({mn.x,mn.y,mn.z},{mx.x,mn.y,mn.z},c); line({mx.x,mn.y,mn.z},{mx.x,mx.y,mn.z},c);
        line({mx.x,mx.y,mn.z},{mn.x,mx.y,mn.z},c); line({mn.x,mx.y,mn.z},{mn.x,mn.y,mn.z},c);
        line({mn.x,mn.y,mx.z},{mx.x,mn.y,mx.z},c); line({mx.x,mn.y,mx.z},{mx.x,mx.y,mx.z},c);
        line({mx.x,mx.y,mx.z},{mn.x,mx.y,mx.z},c); line({mn.x,mx.y,mx.z},{mn.x,mn.y,mx.z},c);
        line({mn.x,mn.y,mn.z},{mn.x,mn.y,mx.z},c); line({mx.x,mn.y,mn.z},{mx.x,mn.y,mx.z},c);
        line({mx.x,mx.y,mn.z},{mx.x,mx.y,mx.z},c); line({mn.x,mx.y,mn.z},{mn.x,mx.y,mx.z},c);
    }

    void drawOBB(const OBB3D& obb, Color c = Color::Cyan()) {
        // 8 corners of OBB
        Vec3 corners[8];
        const Vec3& cx = obb.center;
        Vec3 x = obb.axes[0]*obb.halfExtents.x;
        Vec3 y = obb.axes[1]*obb.halfExtents.y;
        Vec3 z = obb.axes[2]*obb.halfExtents.z;
        corners[0]=cx-x-y-z; corners[1]=cx+x-y-z;
        corners[2]=cx+x+y-z; corners[3]=cx-x+y-z;
        corners[4]=cx-x-y+z; corners[5]=cx+x-y+z;
        corners[6]=cx+x+y+z; corners[7]=cx-x+y+z;
        // 12 edges
        int edges[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},
                          {0,4},{1,5},{2,6},{3,7}};
        for(auto& e : edges) line(corners[e[0]], corners[e[1]], c);
    }

    void drawCapsule(const Vec3& a, const Vec3& b, float r,
                     Color c = Color::Blue(), int segs = 12)
    {
        Vec3 axis = (b - a).normalized();
        Vec3 perp = axis.cross(Vec3::Up());
        if(perp.lengthSq() < kEpsilon) perp = axis.cross(Vec3::Right());
        perp = perp.normalized();

        // Cylinder lines
        float step = 2.f * kPi / segs;
        Vec3 prevA(0), prevB(0);
        for(int i = 0; i <= segs; i++) {
            float ang = i * step;
            Quat rot = Quat::fromAxisAngle(axis, ang);
            Vec3 spoke = rot.rotate(perp) * r;
            Vec3 pa = a + spoke, pb = b + spoke;
            if(i > 0) {
                line(prevA, pa, c);
                line(prevB, pb, c);
            }
            if(i % (segs/4) == 0) line(pa, pb, c);
            prevA = pa; prevB = pb;
        }
        // Hemisphere hints
        drawSphere(a, r, c, 8);
        drawSphere(b, r, c, 8);
    }

    void drawShape(const CollisionShape& shape, const Transform& t,
                   Color c = Color::Green())
    {
        switch(shape.type) {
        case ShapeType::Sphere: {
            Vec3 wc = t.transformPoint(shape.sphere.center);
            float wr = shape.sphere.radius * t.scale.max();
            drawSphere(wc, wr, c);
            break;
        }
        case ShapeType::AABB3D:
            drawAABB(shape.worldAABB(t), c);
            break;
        case ShapeType::OBB3D: {
            OBB3D worldOBB = shape.obb3D;
            worldOBB.center = t.transformPoint(shape.obb3D.center);
            Mat3 rm = t.rotation.toMat3();
            for(int i=0;i<3;i++) worldOBB.axes[i]=(rm*shape.obb3D.axes[i]).normalized();
            worldOBB.halfExtents = shape.obb3D.halfExtents * t.scale;
            drawOBB(worldOBB, c);
            break;
        }
        case ShapeType::Capsule3D: {
            Vec3 wa = t.transformPoint(shape.capsule3D.a);
            Vec3 wb = t.transformPoint(shape.capsule3D.b);
            float wr = shape.capsule3D.radius * t.scale.max();
            drawCapsule(wa, wb, wr, c);
            break;
        }
        case ShapeType::ConvexHull: {
            if(!shape.convexHull) break;
            for(auto& face : shape.convexHull->faces) {
                for(size_t i=0;i<face.indices.size();i++){
                    Vec3 a=t.transformPoint(shape.convexHull->vertices[face.indices[i]]);
                    Vec3 b=t.transformPoint(shape.convexHull->vertices[face.indices[(i+1)%face.indices.size()]]);
                    line(a,b,c);
                }
            }
            break;
        }
        default: drawAABB(shape.worldAABB(t), c); break;
        }
    }

    // ── World draw helpers ─────────────────────────────────────────────────
    void drawContact(const ContactManifold& m) {
        if(!m.hit) return;
        for(int i = 0; i < m.count; i++) {
            const ContactPoint& cp = m.points[i];
            // Contact points
            point(cp.positionA, Color::Red(), 6.f);
            point(cp.positionB, Color::Blue(), 6.f);
            line(cp.positionA, cp.positionB, Color::Yellow());
            // Normal
            if(drawNormals)
                arrow(cp.positionB, cp.normal, 0.3f, Color::Orange());
        }
    }

    void drawBVHNode(const DynamicBVH& bvh, int depth = 0) {
        // BVH is opaque — we query all nodes via a width-first traversal
        // approximated by a large volume query
        if(!drawBVH && !(drawBVHLeaves && depth == 0)) return;
        bvh.query(AABB3D{{-1e10f,-1e10f,-1e10f},{1e10f,1e10f,1e10f}},
            [&](uint64_t) -> bool { return true; });
        // Actual BVH node drawing requires access to internals — draw leaves
        // by querying full-space and drawing each body's AABB
    }

    void drawBody(const CollisionBody& body) {
        if(body.isSleeping() && !drawSleeping) return;

        Color bodyColor = body.isTrigger()   ? Color::Magenta()
                        : body.isStatic()    ? Color::Gray()
                        : body.isSleeping()  ? Color{60,60,120}
                        :                      Color::Green();

        if(drawShapes)
            drawShape(body.shape, body.transform, bodyColor);

        if(drawAABBs)
            drawAABB(body.cachedAABB, {100,100,100,120});

        if(drawVelocities && body.velocity.lengthSq() > kEpsilon)
            arrow(body.transform.position, body.velocity, 1.f, Color::Cyan());

        if(drawBodyIDs)
            text(body.transform.position, std::to_string(body.id), Color::White());
    }

    void drawWorld(const CollisionWorld& world) {
        // Can't iterate bodies directly (private) — use public contacts query
        const auto& contacts = world.contacts();
        for(auto& m : contacts) drawContact(m);
    }

    // ── 2D helpers ─────────────────────────────────────────────────────────
    void drawCircle2D(const Vec2& c, float r, Color col, int segs = 16) {
        float step = 2.f*kPi/segs;
        Vec3 prev(0);
        for(int i=0;i<=segs;i++){
            float a=i*step;
            Vec3 cur(c.x+std::cos(a)*r, c.y+std::sin(a)*r, 0);
            if(i>0) line(prev,cur,col);
            prev=cur;
        }
    }

    void drawAABB2D(const AABB2D& aabb, Color c = Color::Gray()) {
        float x0=aabb.min.x,y0=aabb.min.y,x1=aabb.max.x,y1=aabb.max.y;
        line({x0,y0,0},{x1,y0,0},c); line({x1,y0,0},{x1,y1,0},c);
        line({x1,y1,0},{x0,y1,0},c); line({x0,y1,0},{x0,y0,0},c);
    }

    // ── Stats overlay ──────────────────────────────────────────────────────
    void drawStats(const CollisionWorld& world, const Vec3& screenPos) {
        std::string stats =
            "Bodies:"   + std::to_string(world.bodyCount())  +
            " Contacts:"+ std::to_string(world.contactCount())+
            " Frame:"   + std::to_string(world.frameStamp());
        text(screenPos, stats, Color::White());
    }

private:
    LineCB   lineCB_;
    PointCB  pointCB_;
    TextCB   textCB_;
};

} // namespace col
