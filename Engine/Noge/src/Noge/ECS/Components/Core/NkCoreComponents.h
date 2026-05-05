#pragma once
// =============================================================================
// Nkentseu/ECS/Components/Core/NkCoreComponents.h — En-tête de convenance
// =============================================================================
// Inclut tous les composants ECS fondamentaux en un seul include.
//
// DUPLICATION RÉSOLUE :
//   Les anciennes structs NkTransformComponent, NkNameComponent, NkTagComponent,
//   NkInactiveComponent, NkLocalTransform, NkWorldTransform étaient définies
//   dans plusieurs fichiers différents. Elles sont maintenant consolidées :
//
//   • NkTransform  (position+rotation+scale+hierarchy dirty flags) → NkTransform.h
//   • NkParent     (référence parent ECS)                          → NkTransform.h
//   • NkChildren   (liste inline des enfants, max 64)              → NkTransform.h
//   • NkName       (nom lisible, 128 chars)                        → NkTag.h
//   • NkTag        (bitfield 64 tags prédéfinis + utilisateur)     → NkTag.h
//   • NkLayer      (couche rendu/collision)                        → NkTag.h
//   • NkInactive   (tag : entité désactivée)                       → NkTag.h
//   • NkStatic     (tag : entité statique)                         → NkTag.h
//   • NkPersist    (tag : survive au changement de scène)          → NkTag.h
// =============================================================================

#include "NkTransform.h"
#include "NkTag.h"
