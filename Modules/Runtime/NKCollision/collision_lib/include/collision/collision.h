#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  collision.h — single include for the whole library
//
//  Usage:
//    #include <collision/collision.h>
//    col::CollisionWorld world;
//    auto id = world.addBody({ .shape=col::CollisionShape::makeSphere({0,0,0},1.f) });
//    world.step();
// ─────────────────────────────────────────────────────────────────────────────
#include "math.h"
#include "shapes.h"
#include "narrowphase.h"
#include "broadphase.h"
#include "gpu_backend.h"
#include "world.h"
#include "persistent_contacts.h"
#include "ccd.h"
#include "debug_draw.h"
