#include "../include/collision/shapes.h"
#include <cstring>

namespace col {

// ─────────────────────────────────────────────────────────────────────────────
//  CollisionShape::worldAABB
// ─────────────────────────────────────────────────────────────────────────────
AABB3D CollisionShape::worldAABB(const Transform& t) const {
    switch(type) {
    case ShapeType::Sphere: {
        Vec3 worldCenter = t.transformPoint(sphere.center);
        float r = sphere.radius * std::max({t.scale.x, t.scale.y, t.scale.z});
        return { worldCenter - Vec3(r), worldCenter + Vec3(r) };
    }
    case ShapeType::AABB3D: {
        // Transform all 8 corners
        AABB3D result;
        Vec3 corners[8] = {
            {aabb3D.min.x,aabb3D.min.y,aabb3D.min.z},
            {aabb3D.max.x,aabb3D.min.y,aabb3D.min.z},
            {aabb3D.min.x,aabb3D.max.y,aabb3D.min.z},
            {aabb3D.max.x,aabb3D.max.y,aabb3D.min.z},
            {aabb3D.min.x,aabb3D.min.y,aabb3D.max.z},
            {aabb3D.max.x,aabb3D.min.y,aabb3D.max.z},
            {aabb3D.min.x,aabb3D.max.y,aabb3D.max.z},
            {aabb3D.max.x,aabb3D.max.y,aabb3D.max.z},
        };
        for(auto& c : corners) result.expand(t.transformPoint(c));
        return result;
    }
    case ShapeType::OBB3D: {
        OBB3D worldOBB = obb3D;
        worldOBB.center = t.transformPoint(obb3D.center);
        Mat3 rotMat = t.rotation.toMat3();
        for(int i=0;i<3;i++) worldOBB.axes[i] = (rotMat * obb3D.axes[i]).normalized();
        worldOBB.halfExtents = obb3D.halfExtents * t.scale;
        return worldOBB.toAABB();
    }
    case ShapeType::Capsule3D: {
        Vec3 wa = t.transformPoint(capsule3D.a);
        Vec3 wb = t.transformPoint(capsule3D.b);
        float r = capsule3D.radius * std::max({t.scale.x,t.scale.y,t.scale.z});
        Vec3 rv(r);
        return { Vec3::Min(wa,wb)-rv, Vec3::Max(wa,wb)+rv };
    }
    case ShapeType::ConvexHull: {
        if(!convexHull) return {};
        AABB3D result;
        for(auto& v : convexHull->vertices) result.expand(t.transformPoint(v));
        return result;
    }
    case ShapeType::TriangleMesh: {
        if(!triMesh) return {};
        AABB3D result;
        for(auto& v : triMesh->vertices) result.expand(t.transformPoint(v));
        return result;
    }
    case ShapeType::Heightfield: {
        if(!heightfield) return {};
        return heightfield->toAABB();
    }
    // 2D shapes — flat in XY
    case ShapeType::Circle2D: {
        Vec3 c(circle2D.center.x, circle2D.center.y, 0);
        float r=circle2D.radius;
        return { c-Vec3(r,r,0.01f), c+Vec3(r,r,0.01f) };
    }
    case ShapeType::AABB2D: {
        return {
            Vec3(aabb2D.min.x,aabb2D.min.y,-0.01f),
            Vec3(aabb2D.max.x,aabb2D.max.y, 0.01f)
        };
    }
    default: return {};
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  CollisionShape::support3D — GJK support function
// ─────────────────────────────────────────────────────────────────────────────
Vec3 CollisionShape::support3D(const Vec3& dir, const Transform& t) const {
    // Transform dir to local space
    Vec3 localDir = t.rotation.conjugate().rotate(dir);
    // Scale direction for non-uniform scale
    localDir = Vec3(localDir.x/t.scale.x, localDir.y/t.scale.y, localDir.z/t.scale.z);
    float len = localDir.length();
    if(len > kEpsilon) localDir = localDir / len;

    Vec3 localResult;
    switch(type) {
    case ShapeType::Sphere:
        localResult = sphere.center + localDir * sphere.radius;
        break;
    case ShapeType::AABB3D:
        localResult = aabb3D.support(localDir);
        break;
    case ShapeType::OBB3D:
        localResult = obb3D.support(localDir);
        break;
    case ShapeType::Capsule3D:
        localResult = capsule3D.support(localDir);
        break;
    case ShapeType::ConvexHull:
        if(convexHull) localResult = convexHull->support(localDir);
        break;
    default:
        localResult = Vec3(0);
        break;
    }
    return t.transformPoint(localResult);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Triangle::closestPoint
// ─────────────────────────────────────────────────────────────────────────────
Vec3 Triangle::closestPoint(const Vec3& p) const {
    Vec3 ab=v[1]-v[0], ac=v[2]-v[0], ap=p-v[0];
    float d1=ab.dot(ap), d2=ac.dot(ap);
    if(d1<=0&&d2<=0) return v[0];

    Vec3 bp=p-v[1];
    float d3=ab.dot(bp), d4=ac.dot(bp);
    if(d3>=0&&d4<=d3) return v[1];

    Vec3 cp=p-v[2];
    float d5=ab.dot(cp), d6=ac.dot(cp);
    if(d6>=0&&d5<=d6) return v[2];

    float vc=d1*d4-d3*d2;
    if(vc<=0&&d1>=0&&d3<=0) return v[0]+ab*(d1/(d1-d3));

    float vb=d5*d2-d1*d6;
    if(vb<=0&&d2>=0&&d6<=0) return v[0]+ac*(d2/(d2-d6));

    float va=d3*d6-d5*d4;
    if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0) {
        float w=(d4-d3)/((d4-d3)+(d5-d6));
        return v[1]+(v[2]-v[1])*w;
    }

    float denom=1.f/(va+vb+vc);
    float t2=vb*denom, w2=vc*denom;
    return v[0]+ab*t2+ac*w2;
}

} // namespace col
