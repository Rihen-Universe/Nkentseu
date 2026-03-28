#pragma once
// =============================================================================
// Transform.h  —  Style Unity
// Composant obligatoire sur tout GameObject.
// Gère position, rotation (Euler en degrés), échelle, et hiérarchie parent/enfant.
// =============================================================================
#include "Component.h"
#include <cmath>
#include <vector>
#include <algorithm>

namespace Unity {

// Petit vecteur 3D minimaliste (remplacer par glm::vec3 en production)
struct Vec3 {
    float x = 0, y = 0, z = 0;
    constexpr Vec3() = default;
    constexpr Vec3(float x,float y,float z) : x(x),y(y),z(z){}
    Vec3 operator+(const Vec3& o) const { return {x+o.x,y+o.y,z+o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x-o.x,y-o.y,z-o.z}; }
    Vec3 operator*(float s)       const { return {x*s, y*s, z*s}; }
    float Length() const { return std::sqrt(x*x+y*y+z*z); }
    Vec3  Normalized() const {
        float l = Length();
        return l > 1e-6f ? Vec3{x/l,y/l,z/l} : Vec3{};
    }
};

// ---------------------------------------------------------------------------
class Transform : public Component {
public:
    Vec3 position;
    Vec3 rotation;   // Euler angles, degrés
    Vec3 scale{1,1,1};

    // ── Hiérarchie ─────────────────────────────────────────────────────────
    Transform* GetParent() const { return mParent; }

    void SetParent(Transform* parent) {
        // Détachement de l'ancien parent
        if (mParent) {
            auto& siblings = mParent->mChildren;
            auto it2 = std::remove(siblings.begin(), siblings.end(), this);
            siblings.erase(it2, siblings.end());
        }
        mParent = parent;
        if (mParent) mParent->mChildren.push_back(this);
    }

    const std::vector<Transform*>& GetChildren() const { return mChildren; }

    // ── Positions monde (simplifié : ignore la rotation pour la clarté) ───
    Vec3 WorldPosition() const {
        if (!mParent) return position;
        Vec3 p = mParent->WorldPosition();
        Vec3 s = mParent->WorldScale();
        return {p.x + position.x * s.x,
                p.y + position.y * s.y,
                p.z + position.z * s.z};
    }

    Vec3 WorldScale() const {
        if (!mParent) return scale;
        Vec3 ps = mParent->WorldScale();
        return {scale.x * ps.x, scale.y * ps.y, scale.z * ps.z};
    }

    Vec3 Forward() const {
        // Direction locale Z transformée par la rotation Y
        float rad = rotation.y * (3.14159265f / 180.f);
        return {std::sin(rad), 0, std::cos(rad)};
    }

    Vec3 Right() const {
        Vec3 f = Forward();
        return {f.z, 0, -f.x};
    }

    // Méthodes utilitaires
    void Translate(const Vec3& delta) { position = position + delta; }
    void Rotate(float yDeg)           { rotation.y += yDeg; }
    void LookAt(const Vec3& target) {
        Vec3 dir = (target - WorldPosition()).Normalized();
        if (dir.Length() < 1e-6f) return;
        rotation.y = std::atan2(dir.x, dir.z) * (180.f / 3.14159265f);
    }

private:
    Transform*              mParent   = nullptr;
    std::vector<Transform*> mChildren;
};

} // namespace Unity
