# Unitest - Framework de Tests Unitaires pour la Suite nkentseu

**Unitest** est une bibliothèque de tests unitaires intégrée à la suite **nkentseu**, conçue pour offrir une solution robuste et flexible pour la validation de code C++. Elle intègre des fonctionnalités avancées de journalisation, des macros d'assertion puissantes et une configuration personnalisable.

## Fonctionnalités Clés

- 🧩 **Intégration transparente** avec la suite nkentseu
- 🛠 **Macros d'assertion** riches (égalité, approximation, conteneurs, etc.)
- 📊 **Journalisation détaillée** avec sortie console et fichiers
- 📈 **Rapports de test** structurés avec statistiques
- 🔧 **Configuration flexible** de la verbosité
- 🧪 **Support des tests** orientés objet et fonctionnels

## Installation

### Prérequis
- Compilateur C++20
- [Premake5](https://premake.github.io/)
- Dépendance : Bibliothèque **Nkentseu**

### Configuration du Projet
Ajoutez à votre `premake5.lua` :
```lua
-- Configuration de Unitest
project "Unitest"
    kind "SharedLib"
    includedirs { "chemin/vers/nkentseu/include" }
    files { "src/Unitest/**.h", "src/Unitest/**.cpp" }
    links { "Nkentseu" }

-- Configuration de votre application
project "MonApp"
    kind "ConsoleApp"
    links { "Nkentseu", "Unitest" }
```

## Utilisation

### Structure de Base d'un Test
```cpp
// MathTest.h
#pragma once
#include <Unitest/Unitest.h>

namespace nkentseu {
    class MathTest {
    public:
        MathTest() {
            // Enregistrement des tests
            UNITEST_REGISTRY(
                UnitestRegisterIClass(MathTest::RunAllTests, "Tests mathématiques complets")
            );
        }

        void RunAllTests(const std::string& context) {
            TestAddition();
            TestSquareRoot();
        }

    private:
        void TestAddition() {
            UNITEST_EQUAL(2 + 2, 4, "Addition de base");
            UNITEST_EQUAL(-1 + 5, 4, "Addition avec négatifs");
        }

        void TestSquareRoot() {
            UNITEST_APPROX(std::sqrt(4.0), 2.0, 0.001, "Racine carrée normale");
            UNITEST_TRUE(std::isnan(std::sqrt(-1.0)), "Racine carrée négative");
        }
    };

    // Instance globale pour auto-enregistrement
    MathTest mathTest;
}
```

### Exécution des Tests
```cpp
// main.cpp
#include <Unitest/Unitest.h>
#include "MathTest.h"

int main() {
    return nkentseu::RunTest();
}
```

## Macros d'Assertion

| Macro | Description | Exemple |
|-------|-------------|---------|
| `UNITEST_EQUAL` | Vérifie l'égalité stricte | `UNITEST_EQUAL(a, b, "Message")` |
| `UNITEST_APPROX` | Comparaison avec marge d'erreur | `UNITEST_APPROX(a, b, 0.001, "Précision")` |
| `UNITEST_TRUE` | Vérifie une condition true | `UNITEST_TRUE(cond, "Échec")` |
| `UNITEST_FALSE` | Vérifie une condition false | `UNITEST_FALSE(cond, "Échec")` |
| `UNITEST_NULL` | Vérifie un pointeur null | `UNITEST_NULL(ptr, "Ptr non null")` |
| `UNITEST_EMPTY` | Vérifie un conteneur vide | `UNITEST_EMPTY(vec, "Non vide")` |

## Configuration Avancée

### Personnalisation de la Sortie
```cpp
// Désactiver les détails des succès
nkentseu::Unitest.PrintPassedDetails(false);

// Activer tous les détails
nkentseu::Unitest.PrintDetails(true);
```

### Journalisation
Les logs sont générés dans :
- **Console** : Sortie colorée
- **Fichiers** : `./logs/unitest/unitest_<timestamp>.log`

Format des logs :
```
[2024-04-10 14:30:45] [UnitestSystem] [INFO] [MathTest.cpp:42] Test réussi : Addition de base
```

## Résultat d'Exécution

Sortie typique :
```
********************************************************
**  Demarrage de la suite de tests unitaires          **
********************************************************

[TestAll] > 2/2 Failed;
[EdgeCases] > 1/1 Passed;

********************************************************
**  Execution terminee : 2 echec(s) detecte(s)       **
********************************************************
```

## Contribution
Les contributions sont soumises sous licence **Rihen**. Veuillez respecter :
- Convention de code de la suite nkentseu
- Tests unitaires complets pour les nouvelles fonctionnalités
- Documentation mise à jour

---

**License** : © Rihen 2024 - Tous droits réservés.