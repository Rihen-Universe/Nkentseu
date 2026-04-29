```cpp
       LRUCache(usize capacity, EvictionCallback onEvict = nullptr)
           : capacity_(capacity), onEvict_(NkMove(onEvict)) {}
     
       bool Get(const Key& key, Value& outValue) {
           auto* entry = cache_.Find(key);
           if (!entry) return false;
         
           // Mise à jour des métadonnées d'accès
           entry->lastAccessTime = NkTimer::GetTimestamp();
           entry->accessCount++;
         
           outValue = entry->value;
           return true;
       }
     
       void Put(const Key& key, Value value) {
           // Si la clé existe, mise à jour
           auto* existing = cache_.Find(key);
           if (existing) {
               existing->value = NkMove(value);
               existing->lastAccessTime = NkTimer::GetTimestamp();
               existing->accessCount++;
               return;
           }
         
           // Éviction si capacité atteinte
           if (cache_.Size() >= capacity_) {
               EvictLRU();
           }
         
           cache_.Insert(key, CacheEntry<Value>(NkMove(value)));
       }
     
       usize Size() const { return cache_.Size(); }
       bool IsFull() const { return cache_.Size() >= capacity_; }
     
   private:
       void EvictLRU() {
           if (cache_.Empty()) return;
         
           // Recherche de l'entrée la moins récemment utilisée
           const Key* lruKey = nullptr;
           uint64_t oldestTime = ~0ULL;
         
           for (const auto& entry : cache_) {
               if (entry.Value().lastAccessTime < oldestTime) {
                   oldestTime = entry.Value().lastAccessTime;
                   lruKey = &entry.Key();
               }
           }
         
           // Callback d'éviction si défini
           if (lruKey && onEvict_) {
               onEvict_(*lruKey, cache_.Find(*lruKey)->value);
           }
         
           if (lruKey) {
               cache_.Remove(*lruKey);
           }
       }
     
       NkHashMap<Key, CacheEntry<Value>> cache_;
       usize capacity_;
       EvictionCallback onEvict_;
   };
   
   // ─────────────────────────────────────────────────────────────
   // Utilisation pratique
   // ─────────────────────────────────────────────────────────────
   void exempleCacheLRU() {
       // Cache de textures avec callback de libération
       LRUCache<NkString, NkString> textureCache(50, 
           [](const NkString& key, const NkString& path) {
               NKENTSEU_LOG_INFO("Éviction texture: %s (libération GPU)", key.CStr());
               // Renderer::ReleaseTexture(key);
           });
     
       // Remplissage
       textureCache.Put("grass", "textures/grass.png");
       textureCache.Put("stone", "textures/stone.png");
       textureCache.Put("wood", "textures/wood.png");
     
       // Accès
       NkString cachedPath;
       if (textureCache.Get("stone", cachedPath)) {
           NKENTSEU_LOG_INFO("Cache hit: %s", cachedPath.CStr());
       }
     
       NKENTSEU_LOG_INFO("Taille cache: %zu / 50", textureCache.Size());
   }
```

---

## 🤝 Comment Contribuer

Nous accueillons avec plaisir les contributions de la communauté ! Voici comment participer au développement de **NKContainers** :

### 🛠️ Workflow de Contribution

1. **Forker le dépôt** sur GitHub/GitLab
2. **Créer une branche** dédiée : `git checkout -b feature/ma-fonctionnalite`
3. **Implémenter** vos modifications en suivant les standards du projet
4. **Ajouter des tests** unitaires dans `NKContainers/Tests/`
5. **Vérifier** que tous les tests passent et que le code est documenté (Doxygen)
6. **Soumettre une Pull Request** avec une description claire des changements

### 📐 Standards de Code


| Aspect             | Règle                                                                             |
| ------------------ | ---------------------------------------------------------------------------------- |
| **Style**          | Formatage clang-format`.clang-format` à la racine                                 |
| **Documentation**  | Commentaires Doxygen obligatoires sur les API publiques                            |
| **Tests**          | Couverture ≥ 85% pour les nouvelles fonctionnalités                              |
| **Performance**    | Aucun régression > 5% sur les benchmarks existants                                |
| **Compatibilité** | Support GCC 9+, Clang 10+, MSVC 2019+ (C++17)                                      |
| **Nommage**        | `PascalCase` pour classes, `camelCase` pour méthodes, `snake_case` pour variables |

### 🧪 Exécuter la Suite de Tests

```bash
# Compiler les tests
mkdir build && cd build
cmake .. -DNKCONTAINERS_ENABLE_TESTS=ON
cmake --build . --target NKContainers_Tests

# Exécuter les tests unitaires
./bin/NKContainers_Tests --gtest_color=yes

# Générer le rapport de couverture (si activé)
cmake .. -DNKCONTAINERS_ENABLE_COVERAGE=ON
cmake --build .
./bin/coverage_report.sh
```

### 🐛 Signaler un Bug

Avant d'ouvrir une issue, veuillez :

1. Vérifier que le bug n'est pas déjà signalé
2. Inclure un **exemple minimal reproductible**
3. Préciser votre environnement : OS, compilateur, version C++, flags de compilation
4. Si possible, fournir un test unitaire échouant

📝 [Ouvrir une Issue](https://github.com/rihen/nkentseu/issues/new)

---

## 📜 Licence

**NKContainers** est distribué sous licence propriétaire avec usage libre pour modification et intégration.

```
Copyright © 2024-2026 Rihen. Tous droits réservés.

Autorisé gratuitement à toute personne obtenant une copie de ce logiciel
et des fichiers de documentation associés (le "Logiciel"), d'utiliser,
copier, modifier, fusionner, publier, distribuer, sous-licencier et/ou
vendre des copies du Logiciel, et de permettre aux personnes à qui le
Logiciel est fourni de le faire, sous réserve des conditions suivantes :

- Le présent avis de copyright et cet avis d'autorisation doivent être
  inclus dans toutes les copies ou portions substantielles du Logiciel.

- Le Logiciel est fourni "TEL QUEL", sans garantie d'aucune sorte,
  expresse ou implicite.

Pour un usage commercial ou enterprise, contacter : license@nkentseu.com
```

---

## 📞 Support & Contact


| Canal             | Lien / Email                                                                 | Usage                                 |
| ----------------- | ---------------------------------------------------------------------------- | ------------------------------------- |
| **Documentation** | [docs.nkentseu.com/containers](https://docs.nkentseu.com/containers)         | Référence API complète, tutoriels  |
| **GitHub Issues** | [github.com/rihen/nkentseu/issues](https://github.com/rihen/nkentseu/issues) | Bugs, demandes de fonctionnalités    |
| **Discord**       | [discord.gg/nkentseu](https://discord.gg/nkentseu)                           | Discussions, entraide communautaire   |
| **Email**         | support@nkentseu.com                                                         | Support technique, licence enterprise |
| **Twitter/X**     | [@nkentseu_dev](https://twitter.com/nkentseu_dev)                            | Annonces, mises à jour               |

---

## 🙏 Remerciements

NKContainers s'inspire des meilleures pratiques des bibliothèques suivantes, tout en restant indépendant et optimisé pour les environnements contraints :

- **STL** (C++ Standard Library) : pour l'API familière et les concepts génériques
- **EASTL** (Electronic Arts) : pour les techniques d'optimisation jeu vidéo et allocation personnalisée
- **absl::flat_hash_map** : pour les stratégies de hachage et gestion de mémoire
- **folly** : pour les patterns de callbacks polymorphes et SBO
- **Boost.Container** : pour les conteneurs spécialisés et la portabilité

Merci à tous les contributeurs, testeurs et utilisateurs qui font vivre ce projet ! 🚀

---

## 📄 Changelog (Extraits)


| Version | Date       | Modifications Majeures                    |
| ------- | ---------- | ----------------------------------------- |
| `1.0.0` | 2026-04-26 | Publication initiale stable               |
| `0.9.0` | 2026-03-15 | Ajout NkFunction avec SBO, NkOctree 3D    |
| `0.8.0` | 2026-02-10 | NkHashMap optimisé, NkQuadTree 2D        |
| `0.7.0` | 2026-01-05 | NkVectorError, itérateurs génériques   |
| `0.6.0` | 2025-11-20 | Support allocateurs personnalisés, CMake |
| `0.5.0` | 2025-09-15 | Premier prototype interne                 |

📜 [Changelog Complet](CHANGELOG.md)

---

## ✅ Prêt à l'Emploi ?

```cpp
// 1. Inclure
#include <NKContainers/NKContainers.h>

// 2. Utiliser
using namespace nkentseu;

NkVector<int> data = { 1, 2, 3 };
NkHashMap<NkString, int> config;
NkFunction<void()> onReady = []() { NKENTSEU_LOG_INFO("Ready!"); };

// 3. Compiler & Profiter
```

> *"La performance n'est pas un accident, c'est un choix d'architecture."*
> — Équipe NKentseu

🌟 **Si ce projet vous est utile, laissez une étoile ⭐ sur le dépôt !**
📦 **Intégrez-le dans vos projets et contribuez à son évolution.**

---

*Document généré le 27 avril 2026 — Version 1.0.0 — Maintenu par Rihen & Communauté NKentseu*
