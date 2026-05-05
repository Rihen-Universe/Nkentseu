#pragma once
// -----------------------------------------------------------------------------
// FICHIER: NkECS/Components/Physics/NkPhysics.h
// DESCRIPTION: Composants physique 2D et 3D.
//   NkRigidbody2D   — corps rigide 2D
//   NkRigidbody3D   — corps rigide 3D
//   NkCollider2D    — collider 2D (box, circle, polygon, capsule, edge)
//   NkCollider3D    — collider 3D (box, sphere, capsule, convex, mesh, terrain)
//   NkJoint2D       — contrainte 2D
//   NkJoint3D       — contrainte 3D
//   NkTrigger2D     — zone de déclenchement 2D (pas de physique, juste overlap)
//   NkTrigger3D     — zone de déclenchement 3D
// -----------------------------------------------------------------------------

#include "../../NkECSDefines.h"
#include "../../Core/NkTypeRegistry.h"
#include "../Core/NkTransform.h"

namespace nkentseu { namespace ecs {

// =============================================================================
// Enums communs
// =============================================================================

enum class NkBodyType : uint8 {
    Dynamic   = 0,  // affecté par les forces et les collisions
    Kinematic = 1,  // contrôlé par code, collisions mais pas affecté par forces
    Static    = 2,  // ne bouge jamais
};

enum class NkCollisionDetection : uint8 {
    Discrete    = 0,  // standard
    Continuous  = 1,  // bullet (évite tunneling pour objets rapides)
    ContinuousDynamic = 2,
};

enum class NkInterpolation : uint8 {
    None         = 0,
    Interpolate  = 1,  // interpolation entre les frames physique
    Extrapolate  = 2,
};

// =============================================================================
// NkRigidbody2D — corps rigide 2D (Box2D / physique maison)
// =============================================================================

struct NkRigidbody2D {
    NkBodyType  bodyType    = NkBodyType::Dynamic;
    float32     mass        = 1.f;         // kg
    float32     gravityScale = 1.f;
    float32     drag        = 0.f;         // résistance linéaire
    float32     angularDrag = 0.05f;       // résistance angulaire
    bool        freezePosX = false;
    bool        freezePosY = false;
    bool        freezeRot  = false;

    // ── Matériau physique ─────────────────────────────────────────────────────
    float32     friction        = 0.4f;   // coefficient de friction [0..1]
    float32     restitution     = 0.0f;   // rebond [0..1]
    float32     frictionCombine = 0.5f;
    float32     restitutionCombine = 0.f;

    // ── État runtime ──────────────────────────────────────────────────────────
    NkVec2      velocity        = {};
    float32     angularVelocity = 0.f;
    bool        isSleeping      = false;
    NkVec2      force           = {};     // force accumulée
    float32     torque          = 0.f;

    NkInterpolation interpolation = NkInterpolation::Interpolate;

    // ── API runtime ───────────────────────────────────────────────────────────
    void AddForce(NkVec2 f) noexcept          { force += f; }
    void AddImpulse(NkVec2 imp) noexcept      { velocity += imp * (mass > 0.f ? 1.f/mass : 0.f); }
    void AddTorque(float32 t) noexcept        { torque += t; }
    void SetVelocity(NkVec2 v) noexcept       { velocity = v; }
    void Stop() noexcept                      { velocity = {}; angularVelocity = 0.f; }
};
NK_COMPONENT(NkRigidbody2D)

// =============================================================================
// NkRigidbody3D — corps rigide 3D (Bullet / Havok / maison)
// =============================================================================

struct NkRigidbody3D {
    NkBodyType  bodyType    = NkBodyType::Dynamic;
    float32     mass        = 1.f;
    float32     gravityScale = 1.f;
    float32     drag        = 0.f;
    float32     angularDrag = 0.05f;
    bool        freezePosX = false, freezePosY = false, freezePosZ = false;
    bool        freezeRotX = false, freezeRotY = false, freezeRotZ = false;

    float32     friction    = 0.4f;
    float32     restitution = 0.f;

    NkVec3      velocity        = {};
    NkVec3      angularVelocity = {};
    NkVec3      centerOfMass    = {};      // offset depuis le pivot
    NkVec3      inertiaTensor   = {1,1,1}; // moment d'inertie (calculé auto)
    bool        isSleeping      = false;

    NkCollisionDetection collisionDetection = NkCollisionDetection::Discrete;
    NkInterpolation      interpolation      = NkInterpolation::Interpolate;

    // Forces accumulées (cleared each physics step)
    NkVec3  force  = {};
    NkVec3  torque = {};

    void AddForce(NkVec3 f) noexcept             { force += f; }
    void AddImpulse(NkVec3 imp) noexcept         { velocity += imp * (mass > 0.f ? 1.f/mass : 0.f); }
    void AddTorque(NkVec3 t) noexcept            { torque += t; }
    void AddForceAtPoint(NkVec3 f, NkVec3 worldPoint) noexcept {
        force += f;
        // torque = (worldPoint - position) × force — calculé par le système physique
    }
    void SetVelocity(NkVec3 v) noexcept          { velocity = v; }
    void Stop() noexcept                         { velocity = {}; angularVelocity = {}; }
};
NK_COMPONENT(NkRigidbody3D)

// =============================================================================
// NkCollider2D — collider 2D
// =============================================================================

enum class NkCollider2DShape : uint8 {
    Box      = 0,
    Circle   = 1,
    Capsule  = 2,
    Polygon  = 3,   // convex polygon (max 8 sommets)
    Edge     = 4,   // ligne simple (sol, mur)
    Chain    = 5,   // chaîne de segments
};

enum class NkCapsuleDirection2D : uint8 { Vertical = 0, Horizontal = 1 };

struct NkCollider2D {
    NkCollider2DShape shape      = NkCollider2DShape::Box;
    NkVec2            offset     = {};
    bool              isTrigger  = false;
    uint32            layer      = 0;
    uint32            layerMask  = 0xFFFFFFFFu;
    float32           friction   = 0.4f;
    float32           restitution = 0.f;
    bool              enabled    = true;

    // ── Paramètres selon la forme ─────────────────────────────────────────────
    NkVec2  boxSize     = {1.f, 1.f};
    float32 circleRadius = 0.5f;
    NkVec2  capsuleSize = {0.5f, 1.f};
    NkCapsuleDirection2D capsuleDir = NkCapsuleDirection2D::Vertical;

    // Polygon (max 8 sommets)
    static constexpr uint32 kMaxPolyVerts = 8u;
    NkVec2  polyVerts[kMaxPolyVerts] = {};
    uint32  polyVertCount = 0;

    // Edge / Chain
    static constexpr uint32 kMaxChainPoints = 64u;
    NkVec2  chainPoints[kMaxChainPoints] = {};
    uint32  chainPointCount = 0;
    bool    chainLoop = false;

    // ID physique interne (géré par le système physique)
    uint64  physicsBodyId   = 0;
    uint64  physicsShapeId  = 0;
};
NK_COMPONENT(NkCollider2D)

// =============================================================================
// NkCollider3D — collider 3D
// =============================================================================

enum class NkCollider3DShape : uint8 {
    Box         = 0,
    Sphere      = 1,
    Capsule     = 2,
    Cylinder    = 3,
    ConvexMesh  = 4,  // convex hull d'un mesh
    TriangleMesh = 5, // mesh précis (statique uniquement)
    Terrain     = 6,  // heightmap terrain
};

enum class NkCapsuleDirection3D : uint8 { X = 0, Y = 1, Z = 2 };

struct NkCollider3D {
    NkCollider3DShape shape       = NkCollider3DShape::Box;
    NkVec3            center      = {};
    bool              isTrigger   = false;
    uint32            layer       = 0;
    uint32            layerMask   = 0xFFFFFFFFu;
    float32           friction    = 0.4f;
    float32           restitution = 0.f;
    bool              enabled     = true;

    // ── Paramètres selon la forme ─────────────────────────────────────────────
    NkVec3  boxSize         = {1.f, 1.f, 1.f};
    float32 sphereRadius    = 0.5f;
    float32 capsuleRadius   = 0.25f;
    float32 capsuleHeight   = 1.f;
    NkCapsuleDirection3D capsuleDir = NkCapsuleDirection3D::Y;

    // Mesh convex ou triangle
    uint32  meshId          = 0;

    // Terrain
    uint32  terrainHeightmapId = 0;
    NkVec3  terrainSize    = {100.f, 10.f, 100.f};

    // ID physique interne
    uint64  physicsBodyId  = 0;
    uint64  physicsShapeId = 0;
};
NK_COMPONENT(NkCollider3D)

// =============================================================================
// NkPhysicsMaterial — matériau physique partageable
// =============================================================================

struct NkPhysicsMaterial {
    float32 staticFriction  = 0.4f;
    float32 dynamicFriction = 0.4f;
    float32 restitution     = 0.f;
    char    name[64]        = "Default";
};
NK_COMPONENT(NkPhysicsMaterial)

// =============================================================================
// NkJoint2D — contraintes entre deux corps 2D
// =============================================================================

enum class NkJoint2DType : uint8 {
    Fixed    = 0,  // corps soudés
    Hinge    = 1,  // charnière (rotation autour d'un point)
    Slider   = 2,  // glissière (translation sur un axe)
    Spring   = 3,  // ressort (distance + amortisseur)
    Distance = 4,  // distance fixe
};

struct NkJoint2D {
    NkJoint2DType type       = NkJoint2DType::Hinge;
    NkEntityId    connectedBody = NkEntityId::Invalid();
    NkVec2        anchor     = {};
    NkVec2        connectedAnchor = {};
    bool          enableCollision = false;
    bool          breakable  = false;
    float32       breakForce = 1e6f;

    // Hinge
    bool    useLimits    = false;
    float32 minAngle     = -45.f;
    float32 maxAngle     = 45.f;
    bool    useMotor     = false;
    float32 motorSpeed   = 0.f;
    float32 motorMaxTorque = 1000.f;

    // Spring / Distance
    float32 distance     = 1.f;
    float32 frequency    = 1.f;   // Hz
    float32 dampingRatio = 0.5f;
};
NK_COMPONENT(NkJoint2D)

// =============================================================================
// NkJoint3D — contraintes entre deux corps 3D
// =============================================================================

enum class NkJoint3DType : uint8 {
    Fixed    = 0,
    Hinge    = 1,
    Slider   = 2,
    BallSocket = 3,
    Spring   = 4,
    Universal = 5,
};

struct NkJoint3D {
    NkJoint3DType type          = NkJoint3DType::Hinge;
    NkEntityId    connectedBody = NkEntityId::Invalid();
    NkVec3        anchor        = {};
    NkVec3        connectedAnchor = {};
    NkVec3        axis          = {0,1,0}; // axe de la charnière
    bool          enableCollision = false;
    bool          breakable     = false;
    float32       breakForce    = 1e6f;
    float32       breakTorque   = 1e6f;

    // Limites
    bool    useLimits    = false;
    float32 minLimit     = -90.f;
    float32 maxLimit     = 90.f;

    // Moteur
    bool    useMotor     = false;
    float32 targetVelocity = 0.f;
    float32 maxForce     = 1000.f;

    // Spring
    float32 spring       = 0.f;
    float32 damper       = 0.f;
    float32 targetPosition = 0.f;
};
NK_COMPONENT(NkJoint3D)

// =============================================================================
// NkCharacterController — contrôleur de personnage (alternative au rigidbody)
// Gère le mouvement, le grounding et les slopes sans physique complète.
// =============================================================================

struct NkCharacterController {
    float32 radius       = 0.3f;
    float32 height       = 1.8f;
    float32 slopeLimit   = 45.f;   // angle max de pente (degrés)
    float32 stepOffset   = 0.3f;   // hauteur max d'une marche
    float32 skinWidth    = 0.08f;  // marge d'enveloppe (évite tunneling)
    float32 minMoveDistance = 0.001f;
    NkVec3  center       = {0.f, 0.9f, 0.f};

    // ── État runtime ──────────────────────────────────────────────────────────
    NkVec3  velocity     = {};
    bool    isGrounded   = false;
    bool    isOnSlope    = false;
    float32 groundAngle  = 0.f;
    NkVec3  groundNormal = {0,1,0};

    // ── API ───────────────────────────────────────────────────────────────────
    void Move(NkVec3 motion) noexcept { velocity = motion; }
};
NK_COMPONENT(NkCharacterController)

}} // namespace nkentseu::ecs
