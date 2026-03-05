# 📦 Module Memory - Gestion Avancée de Mémoire

Le module **Memory** offre une gestion mémoire industrielle pour la suite **nkentseu**, combinant sécurité, traçabilité et hautes performances avec des outils modernes de gestion de ressources.

## 🧩 Composants Clés

### `UniquePtr.h` - Pointeur Intelligent Exclusif
**🛡️ Possession Unique**  
```cpp
auto texture = Memory.MakeUnique<Texture>("MainTexture", 2048, 2048);
```
- 🚫 Interdit la copie (`delete` sur constructeur copie)
- ➡️ Transfert propriété via `std::move()`
- 📍 Tracking automatique (fichier/ligne/fonction)
- 🧬 Support objets/tableaux via template

### `SharedPtr.h` - Pointeur Intelligent Partageable
**🔢 Comptage Atomique**  
```cpp
auto particles = Memory.MakeSharedArray<Particle>("Explosion", 1000);
```
- 🧵 Thread-safe (compteur atomique)
- 🔄 Détection cycles mémoire via `weak_ptr`
- 📊 Statistiques d'utilisation en temps réel
- 🚨 Auto-destruction quand `use_count == 0`

### `Memory.h` - Système Central
**🎮 Singleton Global**  
```cpp
// Allocation manuelle avec tracking
auto* scene = Memory.New<SceneGraph>("Scene3D", camera);
```
- 🕵️♂️ Tracking complet des allocations
- 📝 Journalisation intégrée avec Logger
- 🔄 Gestion unifiée smart pointers/allocations brutes

## 🚀 Fonctionnalités Phares

| Fonctionnalité               | UniquePtr | SharedPtr | Description                         |
|------------------------------|-----------|-----------|-------------------------------------|
| Gestion cycle de vie         | ✅         | ✅         | Destruction automatique             |
| Support multithread          | ❌         | ✅         | Comptage atomique                   |
| Transfert propriété          | ✅         | ❌         | Via sémantique de déplacement       |
| Détection fuites             | ✅         | ✅         | Rapport détaillé à la fermeture     |
| Tracking fichier/ligne       | ✅         | ✅         | Intégration macro `Memory`          |
| Optimisation LTO             | ✅         | ✅         | Inlining des méthodes critiques     |

## 📋 Utilisation Courante

### Création d'Objets Complexes
```cpp
// Construction avec paramètres
auto player = Memory.MakeUnique<Entity>("Player",
    Position{0, 0, 0},
    Health{100},
    Inventory{}
);

// Accès sécurisé
player->Attack(enemy);
if (player->IsAlive()) {
    player->AddExperience(150);
}
```

### Gestion de Tableaux Performants
```cpp
// Allocation d'un tableau de 1M d'éléments
auto terrain = Memory.MakeSharedArray<float>("Heightmap", 1024*1024);

// Modification parallèle (thread-safe)
#pragma omp parallel for
for (size_t i = 0; i < terrain.Length(); ++i) {
    terrain[i] = GenerateHeight(i);
}
```

### Pattern Factory avec Ownership
```cpp
class TextureFactory {
public:
    UniquePtr<Texture> Create(const std::string& name) {
        return Memory.MakeUnique<Texture>(name, GL_RGBA8);
    }
};

// Usage
auto factory = TextureFactory{};
auto diffuse = factory.Create("RockDiffuse");
auto normal = factory.Create("RockNormal");
```

## ⚠️ Avertissements Critiques

1. **Interopérabilité C++17**  
   ```cpp
   // Danger ! Mixage smart pointers natifs/custom
   auto p1 = std::make_unique<int>(42); // À ÉVITER
   auto p2 = Memory.MakeUnique<int>("SafePtr", 42); // OK
   ```

2. **Captures Async**  
   ```cpp
   // Fuite potentielle dans les lambdas
   auto data = Memory.MakeShared<SensorData>("IoT");
   std::thread([=] { 
       // Conserve une référence non comptée
       Process(data.Get()); 
   }).detach();
   ```

3. **Dépendances Circulaires**  
   ```cpp
   class A { SharedPtr<B> b; };
   class B { SharedPtr<A> a; }; // Cycle ! Utiliser WeakPtr
   ```

## 🔍 Exemple de Rapport de Fuite

```log
[MemorySystem] *** MEMORY LEAK(S) DETECTED: 3 ***
[MemorySystem] [Critical] PhysicsMesh (0x7f8aab00)
  ↳ Allocé dans: World.cpp:127 (LoadLevel())
  ↳ Taille: 48 MB (Triangles: 1,024,812)
  ↳ Stacktrace:
     0x00007f8aab112344 - World::LoadResource()
     0x00007f8aab112899 - World::Init()

[MemorySystem] [Warning] TempBuffer (0x7f8aab00)
  ↳ Allocé dans: Renderer.cpp:42 (CreateFrameBuffer())
  ↳ Durée de vie: 2h34m (Suspect !)
```

## 📜 Licence
```text
Copyright 2025 Rihen  
Sous licence GNU GENERAL PUBLIC LICENSE Version 3  
Utilisation commerciale soumise à autorisation écrite  
Contributions sous licence Rihen  
```

---

## 📬 Support & Communauté

**Équipe Technique**  
[rihen.universe@gmail.com](rihen.universe@gmail.com)  
📞 (+237) 693 761 773 (24/7/365)  

**Ressources :**
- [Documentation Officielle](https://rihen.com/docs/nkentseu)
- [Forum Communautaire](https://forum.rihen.com/nkentseu)
- [Dépôt GitHub](https://github.com/rihen/nkentseu)
