#pragma once
// =============================================================================
// Components.h  —  Style Unity
// Composants gameplay prêts à l'emploi démontrant le pattern.
// =============================================================================
#include "GameObject.h"
#include <cmath>
#include <functional>
#include <string>
#include <unordered_map>

namespace Unity {

// ---------------------------------------------------------------------------
// Renderer  —  affiche un mesh (simulé ici par un label)
// ---------------------------------------------------------------------------
class Renderer : public Component {
public:
    std::string meshName;
    std::string materialName;
    bool        castShadow  = true;
    bool        visible     = true;

    explicit Renderer(std::string mesh = "cube",
                      std::string mat  = "default")
        : meshName(std::move(mesh)), materialName(std::move(mat)) {}

    void Awake() override {
        // En vrai : charger le mesh depuis le ResourceManager
        printf("[Renderer] Awake — mesh='%s' mat='%s'\n",
               meshName.c_str(), materialName.c_str());
    }

    void OnDestroy() override {
        printf("[Renderer] OnDestroy — libère les ressources GPU\n");
    }
};

// ---------------------------------------------------------------------------
// Rigidbody  —  physique simplifiée (intégration d'Euler)
// ---------------------------------------------------------------------------
class Rigidbody : public Component {
public:
    Vec3  velocity;
    Vec3  acceleration;
    float mass        = 1.0f;
    float drag        = 0.01f;     // frottement linéaire
    bool  useGravity  = true;
    bool  isKinematic = false;     // si true : piloté par code, pas la physique

    static constexpr float GravityY = -9.81f;

    void AddForce(const Vec3& force) {
        // F = m*a  →  a += F/m
        acceleration.x += force.x / mass;
        acceleration.y += force.y / mass;
        acceleration.z += force.z / mass;
    }

    void AddImpulse(const Vec3& impulse) {
        velocity.x += impulse.x / mass;
        velocity.y += impulse.y / mass;
        velocity.z += impulse.z / mass;
    }

    void Update(float dt) override {
        if (isKinematic) return;

        // Gravité
        if (useGravity) {
            acceleration.y += GravityY;
        }

        // Intégration d'Euler semi-implicite
        velocity.x += acceleration.x * dt;
        velocity.y += acceleration.y * dt;
        velocity.z += acceleration.z * dt;

        // Frottement
        velocity.x *= (1.0f - drag * dt);
        velocity.y *= (1.0f - drag * dt);
        velocity.z *= (1.0f - drag * dt);

        // Déplace le Transform
        Transform* t = GetComponent<Transform>();
        if (t) t->Translate(velocity * dt);

        // Reset accélération (les forces sont ré-appliquées chaque frame)
        acceleration = {};
    }
};

// ---------------------------------------------------------------------------
// Health  —  points de vie avec événements
// ---------------------------------------------------------------------------
class Health : public Component {
public:
    float maxHp = 100.f;

    using DeathCallback   = std::function<void(GameObject*)>;
    using DamageCallback  = std::function<void(float dmg, float current)>;

    DeathCallback  onDeath;
    DamageCallback onDamage;

    void Awake() override { mHp = maxHp; }

    float GetHp()        const { return mHp; }
    bool  IsAlive()      const { return mHp > 0.f; }
    float GetPercent()   const { return mHp / maxHp; }

    void TakeDamage(float amount) {
        if (!IsAlive()) return;
        mHp -= amount;
        if (mHp < 0.f) mHp = 0.f;
        if (onDamage) onDamage(amount, mHp);
        if (mHp <= 0.f && onDeath) onDeath(GetGameObject());
    }

    void Heal(float amount) {
        mHp = std::min(mHp + amount, maxHp);
    }

    void Revive() { mHp = maxHp; }

private:
    float mHp = 100.f;
};

// ---------------------------------------------------------------------------
// MoveToTarget  —  suit une cible (démontre GetComponent cross-comp)
// ---------------------------------------------------------------------------
class MoveToTarget : public Component {
public:
    GameObject* target   = nullptr;
    float       speed    = 3.f;
    float       stopDist = 0.5f;

    void Update(float dt) override {
        if (!target) return;
        Transform* myT  = GetComponent<Transform>();
        Transform* tgtT = target->GetTransform();
        if (!myT || !tgtT) return;

        Vec3 myPos  = myT->WorldPosition();
        Vec3 tgtPos = tgtT->WorldPosition();
        Vec3 delta  = tgtPos - myPos;
        float dist  = delta.Length();

        if (dist < stopDist) return;

        Vec3 dir = delta.Normalized();
        myT->Translate(dir * (speed * dt));

        // Regarde vers la cible
        myT->LookAt(tgtPos);
    }
};

// ---------------------------------------------------------------------------
// RotateAround  —  rotation continue sur l'axe Y
// ---------------------------------------------------------------------------
class RotateAround : public Component {
public:
    float degreesPerSecond = 90.f;

    void Update(float dt) override {
        if (Transform* t = GetComponent<Transform>())
            t->Rotate(degreesPerSecond * dt);
    }
};

// ---------------------------------------------------------------------------
// Oscillate  —  mouvement sinusoïdal sur Y
// ---------------------------------------------------------------------------
class Oscillate : public Component {
public:
    float amplitude = 1.f;
    float frequency = 1.f;   // Hz

    void Start() override {
        mBaseY = GetComponent<Transform>()->position.y;
    }

    void Update(float dt) override {
        mTime += dt;
        Transform* t = GetComponent<Transform>();
        if (t) t->position.y = mBaseY + std::sin(mTime * frequency * 6.2831f) * amplitude;
    }

private:
    float mBaseY = 0.f;
    float mTime  = 0.f;
};

// ---------------------------------------------------------------------------
// Timer  —  déclenche un callback après N secondes
// ---------------------------------------------------------------------------
class Timer : public Component {
public:
    float                    duration  = 1.f;
    bool                     loop      = false;
    std::function<void()>    onExpire;

    void Start() override { mElapsed = 0.f; mFired = false; }

    void Update(float dt) override {
        if (mFired && !loop) return;
        mElapsed += dt;
        if (mElapsed >= duration) {
            mElapsed = loop ? 0.f : duration;
            mFired   = true;
            if (onExpire) onExpire();
        }
    }

    float Progress() const { return std::min(mElapsed / duration, 1.f); }
    void  Reset()          { mElapsed = 0.f; mFired = false; }

private:
    float mElapsed = 0.f;
    bool  mFired   = false;
};

} // namespace Unity
