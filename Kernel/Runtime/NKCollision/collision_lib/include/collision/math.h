#pragma once
#include <cmath>
#include <cassert>
#include <algorithm>
#include <immintrin.h>  // SSE/AVX SIMD

namespace col {

// ─────────────────────────────────────────────────────────────────────────────
//  Constants
// ─────────────────────────────────────────────────────────────────────────────
constexpr float kEpsilon   = 1e-6f;
constexpr float kPi        = 3.14159265358979323846f;
constexpr float kInfinity  = 1e30f;

// ─────────────────────────────────────────────────────────────────────────────
//  Vec2
// ─────────────────────────────────────────────────────────────────────────────
struct Vec2 {
    float x, y;

    Vec2()                        : x(0), y(0) {}
    Vec2(float x, float y)        : x(x), y(y) {}
    explicit Vec2(float s)        : x(s), y(s) {}

    Vec2  operator+(const Vec2& o) const { return {x+o.x, y+o.y}; }
    Vec2  operator-(const Vec2& o) const { return {x-o.x, y-o.y}; }
    Vec2  operator*(float s)       const { return {x*s,   y*s  }; }
    Vec2  operator/(float s)       const { float inv=1.f/s; return {x*inv, y*inv}; }
    Vec2  operator-()              const { return {-x, -y}; }
    Vec2& operator+=(const Vec2& o)      { x+=o.x; y+=o.y; return *this; }
    Vec2& operator-=(const Vec2& o)      { x-=o.x; y-=o.y; return *this; }
    Vec2& operator*=(float s)            { x*=s;   y*=s;   return *this; }
    bool  operator==(const Vec2& o) const{ return x==o.x && y==o.y; }

    float dot(const Vec2& o)  const { return x*o.x + y*o.y; }
    float cross(const Vec2& o)const { return x*o.y - y*o.x; }  // scalar (z-component)
    float lengthSq()          const { return x*x + y*y; }
    float length()            const { return std::sqrt(lengthSq()); }
    Vec2  normalized()        const { float l=length(); return l>kEpsilon ? (*this)/l : Vec2(0); }
    Vec2  perp()              const { return {-y, x}; }
    Vec2  abs()               const { return {std::abs(x), std::abs(y)}; }
    float min()               const { return std::min(x,y); }
    float max()               const { return std::max(x,y); }

    static Vec2 Min(const Vec2& a, const Vec2& b) { return {std::min(a.x,b.x), std::min(a.y,b.y)}; }
    static Vec2 Max(const Vec2& a, const Vec2& b) { return {std::max(a.x,b.x), std::max(a.y,b.y)}; }
    static Vec2 Lerp(const Vec2& a, const Vec2& b, float t) { return a + (b-a)*t; }
};
inline Vec2 operator*(float s, const Vec2& v) { return v*s; }

// ─────────────────────────────────────────────────────────────────────────────
//  Vec3 (SIMD-aligned for SSE)
// ─────────────────────────────────────────────────────────────────────────────
struct alignas(16) Vec3 {
    float x, y, z, _w;  // padding for SIMD loads

    Vec3()                               : x(0), y(0), z(0), _w(0) {}
    Vec3(float x, float y, float z)      : x(x), y(y), z(z), _w(0) {}
    explicit Vec3(float s)               : x(s), y(s), z(s), _w(0) {}
    Vec3(const Vec2& v, float z=0)       : x(v.x), y(v.y), z(z), _w(0) {}

    Vec3  operator+(const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vec3  operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    Vec3  operator*(float s)       const { return {x*s,   y*s,   z*s  }; }
    Vec3  operator/(float s)       const { float inv=1.f/s; return {x*inv, y*inv, z*inv}; }
    Vec3  operator*(const Vec3& o) const { return {x*o.x, y*o.y, z*o.z}; }
    Vec3  operator/(const Vec3& o) const { return {x/o.x, y/o.y, z/o.z}; }
    Vec3  operator-()              const { return {-x,-y,-z}; }
    Vec3& operator+=(const Vec3& o)      { x+=o.x; y+=o.y; z+=o.z; return *this; }
    Vec3& operator-=(const Vec3& o)      { x-=o.x; y-=o.y; z-=o.z; return *this; }
    Vec3& operator*=(float s)            { x*=s; y*=s; z*=s; return *this; }
    bool  operator==(const Vec3& o) const{ return x==o.x && y==o.y && z==o.z; }

    float dot(const Vec3& o)   const { return x*o.x + y*o.y + z*o.z; }
    Vec3  cross(const Vec3& o) const {
        return { y*o.z - z*o.y,
                 z*o.x - x*o.z,
                 x*o.y - y*o.x };
    }
    float lengthSq()  const { return x*x + y*y + z*z; }
    float length()    const { return std::sqrt(lengthSq()); }
    Vec3  normalized()const { float l=length(); return l>kEpsilon ? (*this)/l : Vec3(0); }
    Vec3  abs()       const { return {std::abs(x), std::abs(y), std::abs(z)}; }
    float min()       const { return std::min({x,y,z}); }
    float max()       const { return std::max({x,y,z}); }
    Vec2  xy()        const { return {x,y}; }

    // SIMD helpers
    __m128 simd() const { return _mm_load_ps(&x); }
    static Vec3 fromSIMD(__m128 m) { Vec3 v; _mm_store_ps(&v.x, m); return v; }

    static Vec3 Min(const Vec3& a, const Vec3& b) {
        return { std::min(a.x,b.x), std::min(a.y,b.y), std::min(a.z,b.z) };
    }
    static Vec3 Max(const Vec3& a, const Vec3& b) {
        return { std::max(a.x,b.x), std::max(a.y,b.y), std::max(a.z,b.z) };
    }
    static Vec3 Lerp(const Vec3& a, const Vec3& b, float t) { return a + (b-a)*t; }
    static Vec3 Zero()    { return {0,0,0}; }
    static Vec3 One()     { return {1,1,1}; }
    static Vec3 Up()      { return {0,1,0}; }
    static Vec3 Right()   { return {1,0,0}; }
    static Vec3 Forward() { return {0,0,1}; }
};
inline Vec3 operator*(float s, const Vec3& v) { return v*s; }

// ─────────────────────────────────────────────────────────────────────────────
//  Vec4
// ─────────────────────────────────────────────────────────────────────────────
struct alignas(16) Vec4 {
    float x, y, z, w;
    Vec4()                                   : x(0),y(0),z(0),w(0) {}
    Vec4(float x,float y,float z,float w)    : x(x),y(y),z(z),w(w) {}
    Vec4(const Vec3& v, float w)             : x(v.x),y(v.y),z(v.z),w(w) {}

    Vec3 xyz()  const { return {x,y,z}; }
    float dot(const Vec4& o) const { return x*o.x+y*o.y+z*o.z+w*o.w; }
    __m128 simd() const { return _mm_load_ps(&x); }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Mat3 — 3×3 rotation / scale
// ─────────────────────────────────────────────────────────────────────────────
struct Mat3 {
    float m[3][3];

    Mat3() { identity(); }
    void identity() {
        m[0][0]=1; m[0][1]=0; m[0][2]=0;
        m[1][0]=0; m[1][1]=1; m[1][2]=0;
        m[2][0]=0; m[2][1]=0; m[2][2]=1;
    }
    Vec3 col(int c) const { return {m[0][c], m[1][c], m[2][c]}; }
    Vec3 row(int r) const { return {m[r][0], m[r][1], m[r][2]}; }

    Vec3 operator*(const Vec3& v) const {
        return {
            m[0][0]*v.x + m[0][1]*v.y + m[0][2]*v.z,
            m[1][0]*v.x + m[1][1]*v.y + m[1][2]*v.z,
            m[2][0]*v.x + m[2][1]*v.y + m[2][2]*v.z
        };
    }
    Mat3 operator*(const Mat3& o) const {
        Mat3 r;
        for(int i=0;i<3;i++) for(int j=0;j<3;j++) {
            r.m[i][j] = 0;
            for(int k=0;k<3;k++) r.m[i][j] += m[i][k]*o.m[k][j];
        }
        return r;
    }
    Mat3 transposed() const {
        Mat3 r;
        for(int i=0;i<3;i++) for(int j=0;j<3;j++) r.m[i][j] = m[j][i];
        return r;
    }
    static Mat3 fromAxisAngle(const Vec3& axis, float angle) {
        float c=cosf(angle), s=sinf(angle), t=1-c;
        Vec3 a = axis.normalized();
        Mat3 r;
        r.m[0][0]=t*a.x*a.x+c;     r.m[0][1]=t*a.x*a.y-s*a.z; r.m[0][2]=t*a.x*a.z+s*a.y;
        r.m[1][0]=t*a.x*a.y+s*a.z; r.m[1][1]=t*a.y*a.y+c;     r.m[1][2]=t*a.y*a.z-s*a.x;
        r.m[2][0]=t*a.x*a.z-s*a.y; r.m[2][1]=t*a.y*a.z+s*a.x; r.m[2][2]=t*a.z*a.z+c;
        return r;
    }
    static Mat3 fromScale(const Vec3& s) {
        Mat3 r;
        r.m[0][0]=s.x; r.m[1][1]=s.y; r.m[2][2]=s.z;
        return r;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Mat4 — 4×4 transform
// ─────────────────────────────────────────────────────────────────────────────
struct alignas(16) Mat4 {
    float m[4][4];

    Mat4() { identity(); }
    void identity() {
        for(int i=0;i<4;i++) for(int j=0;j<4;j++) m[i][j]=(i==j?1.f:0.f);
    }
    Vec4 operator*(const Vec4& v) const {
        return {
            m[0][0]*v.x+m[0][1]*v.y+m[0][2]*v.z+m[0][3]*v.w,
            m[1][0]*v.x+m[1][1]*v.y+m[1][2]*v.z+m[1][3]*v.w,
            m[2][0]*v.x+m[2][1]*v.y+m[2][2]*v.z+m[2][3]*v.w,
            m[3][0]*v.x+m[3][1]*v.y+m[3][2]*v.z+m[3][3]*v.w
        };
    }
    Vec3 transformPoint(const Vec3& p) const {
        Vec4 r = (*this)*Vec4(p,1.f);
        return r.xyz()*(1.f/r.w);
    }
    Vec3 transformDir(const Vec3& d) const {
        return ((*this)*Vec4(d,0.f)).xyz();
    }
    Mat4 operator*(const Mat4& o) const {
        Mat4 r;
        for(int i=0;i<4;i++) for(int j=0;j<4;j++){
            r.m[i][j]=0;
            for(int k=0;k<4;k++) r.m[i][j]+=m[i][k]*o.m[k][j];
        }
        return r;
    }
    Mat4 transposed() const {
        Mat4 r;
        for(int i=0;i<4;i++) for(int j=0;j<4;j++) r.m[i][j]=m[j][i];
        return r;
    }
    static Mat4 translation(const Vec3& t) {
        Mat4 r;
        r.m[0][3]=t.x; r.m[1][3]=t.y; r.m[2][3]=t.z;
        return r;
    }
    static Mat4 scale(const Vec3& s) {
        Mat4 r;
        r.m[0][0]=s.x; r.m[1][1]=s.y; r.m[2][2]=s.z;
        return r;
    }
    static Mat4 fromMat3(const Mat3& rot, const Vec3& pos) {
        Mat4 r;
        for(int i=0;i<3;i++) for(int j=0;j<3;j++) r.m[i][j]=rot.m[i][j];
        r.m[0][3]=pos.x; r.m[1][3]=pos.y; r.m[2][3]=pos.z;
        return r;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Quaternion
// ─────────────────────────────────────────────────────────────────────────────
struct Quat {
    float x, y, z, w;

    Quat()                               : x(0),y(0),z(0),w(1) {}
    Quat(float x,float y,float z,float w): x(x),y(y),z(z),w(w) {}

    Quat operator*(const Quat& o) const {
        return {
             w*o.x + x*o.w + y*o.z - z*o.y,
             w*o.y - x*o.z + y*o.w + z*o.x,
             w*o.z + x*o.y - y*o.x + z*o.w,
             w*o.w - x*o.x - y*o.y - z*o.z
        };
    }
    Vec3 rotate(const Vec3& v) const {
        Vec3 qv{x,y,z};
        Vec3 t = 2.f * qv.cross(v);
        return v + w*t + qv.cross(t);
    }
    Quat conjugate() const { return {-x,-y,-z,w}; }
    Quat normalized() const {
        float l=sqrtf(x*x+y*y+z*z+w*w);
        return {x/l,y/l,z/l,w/l};
    }
    Mat3 toMat3() const {
        float xx=x*x, yy=y*y, zz=z*z;
        float xy=x*y, xz=x*z, yz=y*z;
        float wx=w*x, wy=w*y, wz=w*z;
        Mat3 m;
        m.m[0][0]=1-2*(yy+zz); m.m[0][1]=2*(xy-wz);   m.m[0][2]=2*(xz+wy);
        m.m[1][0]=2*(xy+wz);   m.m[1][1]=1-2*(xx+zz); m.m[1][2]=2*(yz-wx);
        m.m[2][0]=2*(xz-wy);   m.m[2][1]=2*(yz+wx);   m.m[2][2]=1-2*(xx+yy);
        return m;
    }
    static Quat fromAxisAngle(const Vec3& axis, float angle) {
        float s=sinf(angle*0.5f);
        Vec3 a=axis.normalized();
        return {a.x*s, a.y*s, a.z*s, cosf(angle*0.5f)};
    }
    static Quat identity() { return {0,0,0,1}; }
    static Quat slerp(const Quat& a, const Quat& b, float t) {
        float dot = a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;
        Quat bb = dot<0 ? Quat{-b.x,-b.y,-b.z,-b.w} : b;
        dot = std::abs(dot);
        if(dot>0.9995f) {
            Quat r{a.x+(bb.x-a.x)*t, a.y+(bb.y-a.y)*t,
                   a.z+(bb.z-a.z)*t, a.w+(bb.w-a.w)*t};
            return r.normalized();
        }
        float theta0=acosf(dot), theta=theta0*t;
        float s0=cosf(theta)-dot*sinf(theta)/sinf(theta0);
        float s1=sinf(theta)/sinf(theta0);
        return {s0*a.x+s1*bb.x, s0*a.y+s1*bb.y,
                s0*a.z+s1*bb.z, s0*a.w+s1*bb.w};
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Transform — position + rotation + scale
// ─────────────────────────────────────────────────────────────────────────────
struct Transform {
    Vec3 position;
    Quat rotation;
    Vec3 scale;

    Transform() : position(0), rotation(Quat::identity()), scale(1) {}
    Transform(const Vec3& p, const Quat& r={0,0,0,1}, const Vec3& s={1,1,1})
        : position(p), rotation(r), scale(s) {}

    Vec3 transformPoint(const Vec3& p) const {
        return position + rotation.rotate(scale*p);
    }
    Vec3 transformDir(const Vec3& d) const {
        return rotation.rotate(scale*d);
    }
    Vec3 inverseTransformPoint(const Vec3& p) const {
        Vec3 local = p - position;
        Vec3 r = rotation.conjugate().rotate(local);
        return { r.x/scale.x, r.y/scale.y, r.z/scale.z };
    }
    Mat4 toMat4() const {
        return Mat4::fromMat3(rotation.toMat3(), position) * Mat4::scale(scale);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Ray
// ─────────────────────────────────────────────────────────────────────────────
struct Ray {
    Vec3 origin;
    Vec3 dir;       // normalized
    float tMin = 0.f;
    float tMax = kInfinity;

    Ray(const Vec3& o, const Vec3& d) : origin(o), dir(d.normalized()) {}
    Vec3 at(float t) const { return origin + dir*t; }
    // Precomputed inverse dir for fast AABB tests
    Vec3 invDir() const {
        return { 1.f/dir.x, 1.f/dir.y, 1.f/dir.z };
    }
};

struct Ray2D {
    Vec2 origin;
    Vec2 dir;
    float tMin = 0.f;
    float tMax = kInfinity;
    Ray2D(const Vec2& o, const Vec2& d) : origin(o), dir(d.normalized()) {}
    Vec2 at(float t) const { return origin + dir*t; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Interval — scalar interval [lo, hi]
// ─────────────────────────────────────────────────────────────────────────────
struct Interval {
    float lo, hi;
    Interval() : lo(kInfinity), hi(-kInfinity) {}
    Interval(float lo, float hi) : lo(lo), hi(hi) {}
    bool overlaps(const Interval& o) const { return lo<=o.hi && o.lo<=hi; }
    float overlap(const Interval& o) const { return std::min(hi,o.hi)-std::max(lo,o.lo); }
    bool contains(float v)           const { return v>=lo && v<=hi; }
    void expand(float v) { lo=std::min(lo,v); hi=std::max(hi,v); }
    float center()  const { return (lo+hi)*0.5f; }
    float extent()  const { return hi-lo; }
};

} // namespace col
