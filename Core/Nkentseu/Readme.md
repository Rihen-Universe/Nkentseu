# 🚀 NKENTSEU Framework - Bibliothèque Modulaire C++

**Plateforme de développement haute performance** pour applications critiques avec architecture modulaire et gestion cross-plateforme.

![Bannière Framework](https://via.placeholder.com/1600x400.png?text=NKENTSEU+Framework+-+C+++Moderne+Multiplateforme)

## 📦 Modules Principaux

### 1. [📅 Epoch - Gestion Temporelle Avancée](./src/Nkentseu/Epoch/README.md)
**Chronométrie précise et manipulation de dates**  
✅ Dates grégoriennes (1601-30827)  
✅ Mesures nanosecondes  
✅ Calculs de durées complexes  
🔗 [Documentation complète](./src/Nkentseu/Epoch/README.md)

### 2. [📜 Logger - Journalisation Industrielle](./src/Nkentseu/Logger/README.md)
**Système de logging multi-cibles et sécurisé**  
🚦 8 niveaux de criticité  
🌐 Sortie console/fichier/réseau  
📊 Statistiques en temps réel  
🔗 [Guide d'implémentation](./src/Nkentseu/Logger/README.md)

### 3. [📦 Memory - Gestion de Mémoire Intelligente](./src/Nkentseu/Memory/README.md)
**Allocations sécurisées avec traçabilité complète**  
🧠 Smart pointers surchargés  
🔍 Détection de fuites mémoire  
📈 Profiling des allocations  
🔗 [Best practices](./src/Nkentseu/Memory/README.md)

### 4. [🌐 Platform - Abstraction Multiplateforme](./src/Nkentseu/Platform/README.md)
**Couche d'unification des systèmes d'exploitation**  
🖥️ Détection automatique d'architecture  
🔧 Optimisations spécifiques  
🚨 Assertions contextuelles  
🔗 [Table des plateformes supportées](./src/Nkentseu/Platform/README.md)

---

## 🛠 Démarrage Rapide

### Prérequis
- Compilateur C++20
- [Premake5](https://premake.github.io/)
- CMake 3.15+

### Installation
```bash
git clone https://github.com/rihen/nkentseu.git
cd nkentseu
premake5 gmake2
make -j8
```

### Intégration
```lua
-- premake5.lua
workspace "MonProjet"
    configurations { "Debug", "Release" }
    include "Nkentseu"
    
project "Application"
    kind "ConsoleApp"
    links { "Nkentseu" }
```

---

## 🌟 Fonctionnalités Clés

| Module       | Points Forts                                  | Performances | Sécurité |
|--------------|-----------------------------------------------|--------------|----------|
| **Epoch**    | Précision nanoseconde, Dates validées         | ⚡⚡⚡⚡       | 🔒🔒🔒🔒  |
| **Logger**   | Journalisation structurée, Multi-cibles       | ⚡⚡⚡        | 🔒🔒🔒🔒  |
| **Memory**   | Gestion cycle de vie, Anti-fuites             | ⚡⚡⚡⚡       | 🔒🔒🔒🔒🔒|
| **Platform** | Abstraction OS, Optimisations spécifiques     | ⚡⚡⚡⚡⚡     | 🔒🔒🔒    |

---

## 🧩 Exemple Cross-Module

```cpp
#include <Nkentseu/Platform/Platform.h>
#include <Nkentseu/Epoch/Chrono.h>
#include <Nkentseu/Logger/Logger.h>

void InitEngine() {
    // Vérification plateforme
    auto platform = nkentseu::Platform::GetPlatformInfo();
    NKENTSEU_ASSERT(platform.architecture == Arch::X64, "32-bit non supporté");

    // Benchmark d'initialisation
    nkentseu::Chrono initTimer;
    
    // ... Code d'initialisation ...

    // Journalisation des résultats
    logger.Info("Initialisation réussie en {}", initTimer.Elapsed().ToString())
          .Debug("Plateforme: {}", platform.osName);
}
```

---

## 📚 Documentation Technique

| Module       | API Reference | Guide d'Optimisation | Exemples Code |
|--------------|---------------|-----------------------|---------------|
| **Epoch**    | [📘](./src/Nkentseu/Epoch/Readme.md) | [🚀](./src/Nkentseu/Epoch/Readme.md) | [💻](./src/Nkentseu/Epoch/Readme.md/) |
| **Logger**   | [📘](./src/Nkentseu/Logger/Readme.md) | [🚀](./src/Nkentseu/Logger/Readme.md) | [💻](./src/Nkentseu/Logger/Readme.md/) |
| **Memory**   | [📘](./src/Nkentseu/Memory/Readme.md) | [🚀](./src/Nkentseu/Memory/Readme.md) | [💻](./src/Nkentseu/Memory/Readme.md/) |
| **Platform** | [📘](./src/Nkentseu/Platform/Readme.md) | [🚀](./src/Nkentseu/Platform/Readme.md) | [💻](./src/Nkentseu/Platform/Readme.md/) |

---

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
