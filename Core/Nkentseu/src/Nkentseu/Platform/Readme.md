Voici le nouveau fichier README.md structuré pour guider les utilisateurs de la bibliothèque :

# 📚 Module Plateforme - NKENTSEU Framework

**Couche d'abstraction multiplateforme** pour applications C++ avec détection système avancée et gestion optimisée des spécificités matérielles.

![Multiplateforme](https://via.placeholder.com/800x200.png?text=Windows|Linux|macOS|Android|Embedded|WebAssembly)

## 🚀 Composants Clés

### 🔍 Détection de Plateforme (`PlatformDetection.h`)
Détecte automatiquement l'architecture, le système d'exploitation et l'environnement d'exécution.

**Macros principales :**
```cpp
// Architectures
NKENTSEU_ARCH_X64    // x86-64
NKENTSEU_ARCH_ARM64  // ARM 64-bit

// Systèmes d'exploitation
NKENTSEU_PLATFORM_WINDOWS
NKENTSEU_PLATFORM_LINUX
NKENTSEU_PLATFORM_MACOS

// Environnements spéciaux
NKENTSEU_ENV_WSL      // WSL 2
NKENTSEU_ENV_WEBASM   // WebAssembly
```

**Exemple d'utilisation :**
```cpp
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    // Code spécifique Windows
#elif defined(NKENTSEU_PLATFORM_LINUX)
    // Code spécifique Linux
#endif
```

### ℹ️ Informations Plateforme (`Platform.h`)
Fournit des informations détaillées sur l'environnement d'exécution via la classe `Platform`.

**Méthodes principales :**
```cpp
const PlatformInfo info = nkentseu::Platform::GetPlatformInfo();

// Exemple de sortie :
// Architecture : X64
// Plateforme : Windows
// Environnement : Native
// Serveur d'affichage : Win32
```

### 🚀 Optimisations (`Inline.h`)
Contrôle l'inlining des fonctions pour l'optimisation.

**Macros disponibles :**
```cpp
NKENTSEU_FORCE_INLINE // Force l'inlining
NKENTSEU_NOINLINE     // Désactive l'inlining
NKENTSEU_NOEXCEPT     // Spécifie l'absence d'exceptions
```

**Exemple :**
```cpp
NKENTSEU_FORCE_INLINE void calculCritique() { 
    // Code optimisé 
}
```

### 📦 Gestion des Exports (`Exports.h`)
Facilite la création de bibliothèques partagées.

**Utilisation typique :**
```cpp
class NKENTSEU_API MaClasseExportee {
    // ...
};
```

### 🛑 Assertions Avancées (`Assertion.h`)
Système d'assertions intégré au logger.

**Exemple :**
```cpp
NKENTSEU_ASSERT(texture != nullptr, 
    "La texture doit être initialisée avant le rendu !");
```

## 💻 Exemple Complet

```cpp
#include <Nkentseu/Platform/Platform.h>
#include <Nkentseu/Platform/Assertion.h>

void InitialisationSysteme() {
    // Récupération des informations
    const auto info = nkentseu::Platform::GetPlatformInfo();
    
    // Vérification de l'architecture
    NKENTSEU_ASSERT(info.architecture == nkentseu::Arch::X64, 
        "Uniquement supporté en 64-bit");

    // Adaptation au système d'affichage
    switch(info.display) {
        case nkentseu::DisplaySystem::Win32:
            InitialiserDirectX();
            break;
        case nkentseu::DisplaySystem::Wayland:
            InitialiserVulkan();
            break;
        default:
            NKENTSEU_ASSERT(false, "Système non supporté");
    }

    // Fonction critique optimisée
    NKENTSEU_FORCE_INLINE void BoucleRendu() { 
        // ... 
    }
}
```

## 📦 Intégration avec Premake

Ajoutez cette configuration à votre `premake5.lua` :
```lua
include "Nkentseu/Platform"
project "MonProjet"
    dependson { "Nkentseu-Platform" }
    links { "Nkentseu-Platform" }
```

## 🔗 Dépendances

- **Nkentseu-Logger** : Pour les assertions et logging
- **LibICU** (Linux) : Gestion Unicode avancée
- **Windows SDK** (Windows) : Intégration Win32

## 🌍 Politique de Support

| Plateforme      | Version Minimale | Statut       |
|-----------------|------------------|--------------|
| Windows         | 10 (19045+)      | ✅ Officiel  |
| Linux           | Kernel 5.15+     | ✅ Officiel  |
| macOS           | 12.6+            | ✅ Officiel  |
| Android         | API 31+          | 🚧 Bêta      |
| WebAssembly     | Emscripten 3.1+  | 🚧 Expérimental |

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
