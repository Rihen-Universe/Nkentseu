# Guide d'implémentation : PIMPL avec vtable manuelle (sans virtual)

## Introduction

Dans les projets multiplateformes, on doit souvent masquer les détails d'implémentation spécifiques à chaque OS tout en offrant une interface uniforme à l'utilisateur. Deux approches courantes sont :

1. **Héritage avec méthodes virtuelles** : une classe de base abstraite, des classes dérivées par plateforme. L'utilisateur manipule un pointeur (ou référence) vers la classe de base.
2. **PIMPL (Pointer to Implementation) avec virtual** : la classe publique contient un pointeur vers une implémentation opaque, et les appels sont délégués via des méthodes virtuelles de l'impl.

Ces deux méthodes introduisent un **coût d'appel virtuel** (vtable) à chaque opération. Pour des opérations peu fréquentes (création de fenêtre, changement de taille), cela reste négligeable. Mais si vous cherchez à **optimiser les performances** ou à **éviter toute surcharge dynamique** (par exemple pour des appels très fréquents), vous pouvez opter pour une variante du PIMPL sans virtual, en utilisant une **table de fonctions manuelle** (vtable explicite) stockée dans l'implémentation opaque.

Ce document explique comment concevoir ce pattern, ses avantages et inconvénients, et fournit un exemple concret avec une classe `NkWindow`.

---

## Principe général

- La classe publique (`NkWindow`) contient un unique pointeur vers une structure opaque (`NkWindowImpl`) qui est allouée dynamiquement.
- La structure opaque contient deux choses :
  - Les **données spécifiques à la plateforme** (HWND, Display*, etc.).
  - Une **table de fonctions** (vtable) contenant des pointeurs vers les fonctions implémentant chaque opération.
- Les méthodes publiques de `NkWindow` appellent simplement la fonction correspondante via la table, en passant le pointeur vers les données.

Cette approche élimine complètement l'héritage et les appels virtuels C++ : chaque appel est une indirection de pointeur de fonction, ce qui est similaire en coût à un appel virtuel, mais plus explicite et potentiellement plus prévisible. On gagne en liberté car on peut choisir quelle fonction appeler, voire changer dynamiquement le comportement.

---

## Comparaison avec les approches classiques

| Caractéristique | Héritage virtuel | PIMPL virtuel | PIMPL vtable manuelle |
|----------------|------------------|---------------|-----------------------|
| Encapsulation totale | Oui (si classe de base abstraite) | Oui | Oui |
| Dépendances cachées | Non (les dérivées sont visibles) | Oui | Oui |
| Coût d'appel | Virtual (vtable) | Virtual (vtable) | Pointeur de fonction |
| Facilité de maintenance | Simple | Simple | Plus complexe (gestion manuelle de la vtable) |
| Changement dynamique de comportement | Possible via héritage | Possible via changement d'impl | Possible en remplaçant la vtable |
| Allocation | Une seule allocation (objet dérivé) | Une allocation (impl) | Une allocation (impl) |

---

## Structure de données

Voici comment organiser les fichiers pour une classe `NkWindow`.

### `NkWindow.h` – interface publique

```cpp
#pragma once
#include <memory>

namespace nkentseu {

// Structure opaque : seul le nom est connu du public.
struct NkWindowImpl;

class NkWindow {
public:
    NkWindow();
    ~NkWindow();

    // Méthodes publiques
    bool Create(int width, int height, const char* title);
    void Close();
    void SetTitle(const char* title);
    // ... autres méthodes

private:
    std::unique_ptr<NkWindowImpl> m_impl;
};

} // namespace nkentseu
```

### `NkWindowImpl.h` – déclaration de l'implémentation (privé, partagé entre plateformes)

```cpp
#pragma once

#include "NkWindow.h"

namespace nkentseu {

// Définition de la table de fonctions (vtable manuelle)
struct NkWindowImplVtable {
    bool (*create)(NkWindowImpl* impl, int width, int height, const char* title);
    void (*close)(NkWindowImpl* impl);
    void (*setTitle)(NkWindowImpl* impl, const char* title);
    // ... autres pointeurs de fonction
};

// Structure concrète de l'implémentation
struct NkWindowImpl {
    const NkWindowImplVtable* vtable;  // pointeur vers la table de fonctions
    // Données spécifiques à la plateforme (seront définies dans les fichiers .cpp)
    // Exemple commun : un booléen "isOpen"
    bool isOpen;
};

// Fonctions pour obtenir la vtable de chaque plateforme (déclarées, définies dans les .cpp spécifiques)
const NkWindowImplVtable* GetWin32WindowVtable();
const NkWindowImplVtable* GetXLibWindowVtable();
// etc.

} // namespace nkentseu
```

### `NkWindow.cpp` – implémentation commune (indépendante de la plateforme)

```cpp
#include "NkWindow.h"
#include "NkWindowImpl.h"
#include "NkPlatformDetect.h" // pour définir quelle plateforme est utilisée

namespace nkentseu {

NkWindow::NkWindow() : m_impl(nullptr) {}

NkWindow::~NkWindow() {
    Close(); // si nécessaire
}

bool NkWindow::Create(int width, int height, const char* title) {
    // Allouer l'implémentation selon la plateforme
    m_impl = std::make_unique<NkWindowImpl>();
    if (!m_impl) return false;

    // Choisir la vtable appropriée
#if defined(NKENTSEU_PLATFORM_WIN32)
    m_impl->vtable = GetWin32WindowVtable();
#elif defined(NKENTSEU_PLATFORM_XLIB)
    m_impl->vtable = GetXLibWindowVtable();
#else
    #error "Platform not supported"
#endif

    // Appeler la fonction de création via la vtable
    return m_impl->vtable->create(m_impl.get(), width, height, title);
}

void NkWindow::Close() {
    if (m_impl && m_impl->vtable && m_impl->vtable->close) {
        m_impl->vtable->close(m_impl.get());
    }
    m_impl.reset();
}

void NkWindow::SetTitle(const char* title) {
    if (m_impl && m_impl->vtable && m_impl->vtable->setTitle) {
        m_impl->vtable->setTitle(m_impl.get(), title);
    }
}

} // namespace nkentseu
```

### Implémentation spécifique à une plateforme (ex: `NkWin32Window.cpp`)

```cpp
#include "NkWindowImpl.h"
#include <windows.h>

namespace nkentseu {

// Données spécifiques à Win32 (étendues de NkWindowImpl)
struct NkWin32WindowData {
    HWND hwnd;
    HINSTANCE hInstance;
    // etc.
};

// Fonctions implémentant les opérations

static bool win32Create(NkWindowImpl* impl, int width, int height, const char* title) {
    // Créer la fenêtre Win32, remplir les données
    auto* data = new NkWin32WindowData(); // à stocker dans l'impl
    // ... création
    impl->isOpen = true;
    // On peut stocker data dans un champ supplémentaire si on a réservé de la place
    // Mais ici, on pourrait utiliser un placement new dans un buffer interne.
    // Pour simplifier, on suppose que NkWindowImpl a un membre `char platformData[256];`
    // et on utilise placement new.
    return true;
}

static void win32Close(NkWindowImpl* impl) {
    if (impl->isOpen) {
        // Détruire la fenêtre
        auto* data = reinterpret_cast<NkWin32WindowData*>(impl->platformData);
        DestroyWindow(data->hwnd);
        data->~NkWin32WindowData(); // destruction explicite
        impl->isOpen = false;
    }
}

static void win32SetTitle(NkWindowImpl* impl, const char* title) {
    auto* data = reinterpret_cast<NkWin32WindowData*>(impl->platformData);
    SetWindowTextA(data->hwnd, title);
}

// Définition de la vtable pour Win32
static const NkWindowImplVtable win32Vtable = {
    .create   = win32Create,
    .close    = win32Close,
    .setTitle = win32SetTitle,
    // ... autres
};

const NkWindowImplVtable* GetWin32WindowVtable() {
    return &win32Vtable;
}

} // namespace nkentseu
```

### Gestion des données spécifiques

Dans l'exemple ci-dessus, on a utilisé un tableau de bytes `platformData` au sein de `NkWindowImpl` pour stocker les données spécifiques sans avoir à allouer de la mémoire supplémentaire. Cela nécessite de connaître la taille maximale des données pour toutes les plateformes. On peut définir une constante comme :

```cpp
constexpr size_t NK_MAX_PLATFORM_DATA_SIZE = 256; // assez grand pour toutes les plateformes
struct NkWindowImpl {
    const NkWindowImplVtable* vtable;
    bool isOpen;
    alignas(std::max_align_t) char platformData[NK_MAX_PLATFORM_DATA_SIZE];
};
```

Dans les fonctions de création, on utilise le placement new pour construire l'objet spécifique dans ce buffer :

```cpp
static bool win32Create(NkWindowImpl* impl, int width, int height, const char* title) {
    auto* data = new (impl->platformData) NkWin32WindowData();
    // ... initialisation
    return true;
}
```

Et dans `close`, on appelle explicitement le destructeur :

```cpp
static void win32Close(NkWindowImpl* impl) {
    auto* data = reinterpret_cast<NkWin32WindowData*>(impl->platformData);
    data->~NkWin32WindowData();
    impl->isOpen = false;
}
```

Cette technique évite toute allocation supplémentaire et garantit que toutes les données sont stockées dans l'objet principal.

---

## Avantages de l'approche

1. **Aucun appel virtuel** : tous les appels passent par un pointeur de fonction explicite. Le coût est similaire, mais on évite les vtables et le polymorphisme.
2. **Encapsulation totale** : l'utilisateur n'a aucune connaissance des détails d'implémentation.
3. **Flexibilité** : on peut changer la vtable dynamiquement pour modifier le comportement à l'exécution (par exemple, pour basculer entre un backend logiciel et matériel).
4. **Pas de dépendances d'en-têtes** : les en-têtes publics n'incluent aucun fichier spécifique à la plateforme.
5. **Performance prévisible** : l'indirection est explicite et peut être optimisée (par exemple en stockant la vtable dans un registre).

---

## Inconvénients

1. **Complexité de maintenance** : il faut maintenir manuellement la table de fonctions et s'assurer que toutes les fonctions sont implémentées pour chaque plateforme.
2. **Risque d'erreurs** : si on oublie d'initialiser un pointeur de fonction, on aura un crash.
3. **Taille fixe du buffer** : le buffer pour les données spécifiques doit être assez grand pour la plateforme la plus gourmande, ce qui peut gaspiller de la mémoire.
4. **Pas d'héritage** : difficile de réutiliser du code entre plateformes sans duplication (mais on peut factoriser dans des fonctions communes).

---

## Conclusion

L'approche PIMPL avec vtable manuelle est une alternative performante et flexible aux méthodes virtuelles classiques. Elle convient particulièrement aux bibliothèques multiplateformes où les performances sont critiques et où l'on souhaite un contrôle fin sur le dispatch des appels. Cependant, elle demande une discipline de programmation rigoureuse et une bonne connaissance des mécanismes sous-jacents.

Dans le contexte de NkWindow, cette technique peut être utilisée pour les classes de fenêtre, de renderer, ou de système d'événements, en fonction des besoins. L'exemple fourni peut être adapté à d'autres composants en suivant le même schéma.

# Approche par identifiant de fenêtre (ID) avec map backend

## Introduction

Dans les architectures multiplateformes, on cherche souvent à minimiser la taille des objets publics et à découpler l'interface de l'implémentation. Une technique alternative au PIMPL classique ou à l'héritage virtuel consiste à utiliser un simple **identifiant entier (ID)** comme seul membre de la classe publique, et à confier la gestion des implémentations natives à un **backend singleton** qui maintient une table de correspondance (map) entre les ID et les données spécifiques à la plateforme.

Cette approche est notamment utilisée dans des bibliothèques comme GLFW (où les fenêtres sont manipulées via des pointeurs opaques, mais on pourrait les remplacer par des ID). Elle offre une grande légèreté pour les objets publics et facilite le passage de paramètres entre modules.

## Principe général

- La classe publique `NkWindow` ne contient qu'un seul membre : un entier `m_id` (par exemple `uint32_t` ou `size_t`). Cet ID est unique pour chaque fenêtre créée.
- Le backend (par exemple `NkWindowBackend`) est un singleton qui maintient une map associant chaque ID à une structure d'implémentation spécifique à la plateforme (`NkWindowImpl`). Cette structure contient toutes les données natives (HWND, Display*, etc.) ainsi qu'éventuellement une table de fonctions (vtable) si l'on veut éviter le polymorphisme.
- Toutes les opérations sur la fenêtre (`Create`, `Close`, `SetTitle`, etc.) sont déléguées au backend via des méthodes statiques ou des fonctions libres qui prennent l'ID en paramètre. Le backend retrouve l'implémentation correspondante dans sa map et exécute l'opération.

## Schéma de code

### `NkWindow.h` – interface publique

```cpp
#pragma once
#include <cstdint>

namespace nkentseu {

class NkWindow {
public:
    NkWindow();
    ~NkWindow();

    // Méthodes publiques
    bool Create(int width, int height, const char* title);
    void Close();
    void SetTitle(const char* title);
    // ...

    // Accès à l'ID (utile pour passer la fenêtre à d'autres modules)
    uint32_t GetID() const { return m_id; }

private:
    uint32_t m_id; // 0 = invalide
};

} // namespace nkentseu
```

### `NkWindowBackend.h` – déclaration du backend (privé)

```cpp
#pragma once
#include <cstdint>
#include <unordered_map>
#include <memory>

namespace nkentseu {

// Structure opaque pour l'implémentation spécifique (définie dans les .cpp plateforme)
struct NkWindowImpl;

class NkWindowBackend {
public:
    static NkWindowBackend& Instance();

    // Alloue un nouvel ID et crée l'implémentation
    uint32_t CreateWindow(int width, int height, const char* title);

    // Opérations sur une fenêtre existante
    void CloseWindow(uint32_t id);
    void SetWindowTitle(uint32_t id, const char* title);
    // ...

    // Vérifie si un ID est valide
    bool IsValid(uint32_t id) const;

private:
    NkWindowBackend() = default;
    ~NkWindowBackend() = default;

    std::unordered_map<uint32_t, std::unique_ptr<NkWindowImpl>> m_windows;
    uint32_t m_nextId = 1; // Les ID commencent à 1 (0 = invalide)
};

} // namespace nkentseu
```

### `NkWindow.cpp` – implémentation commune

```cpp
#include "NkWindow.h"
#include "NkWindowBackend.h"

namespace nkentseu {

NkWindow::NkWindow() : m_id(0) {}

NkWindow::~NkWindow() {
    Close();
}

bool NkWindow::Create(int width, int height, const char* title) {
    if (m_id != 0) return false; // déjà créée
    m_id = NkWindowBackend::Instance().CreateWindow(width, height, title);
    return m_id != 0;
}

void NkWindow::Close() {
    if (m_id != 0) {
        NkWindowBackend::Instance().CloseWindow(m_id);
        m_id = 0;
    }
}

void NkWindow::SetTitle(const char* title) {
    if (m_id != 0) {
        NkWindowBackend::Instance().SetWindowTitle(m_id, title);
    }
}

} // namespace nkentseu
```

### Implémentation spécifique (ex: `NkWin32Window.cpp`)

```cpp
#include "NkWindowBackend.h"
#include <windows.h>

namespace nkentseu {

// Définition de l'implémentation pour Win32
struct NkWindowImpl {
    HWND hwnd;
    HINSTANCE hInstance;
    // autres données...
};

// Fonctions internes (non exposées)
namespace {
    LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        // Récupérer l'ID depuis la structure utilisateur (via GWLP_USERDATA)
        uint32_t id = (uint32_t)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        // ... traiter l'événement et appeler le système d'événements avec l'ID
        return DefWindowProc(hwnd, msg, wp, lp);
    }
}

uint32_t NkWindowBackend::CreateWindow(int width, int height, const char* title) {
    auto impl = std::make_unique<NkWindowImpl>();
    // ... créer la fenêtre Win32, stocker l'ID dans GWLP_USERDATA
    // Récupérer l'ID
    uint32_t id = m_nextId++;
    impl->hwnd = ...; // après création, on associe l'ID
    SetWindowLongPtr(impl->hwnd, GWLP_USERDATA, (LONG_PTR)id);

    m_windows[id] = std::move(impl);
    return id;
}

void NkWindowBackend::CloseWindow(uint32_t id) {
    auto it = m_windows.find(id);
    if (it != m_windows.end()) {
        // Détruire la fenêtre
        DestroyWindow(it->second->hwnd);
        m_windows.erase(it);
    }
}

void NkWindowBackend::SetWindowTitle(uint32_t id, const char* title) {
    auto it = m_windows.find(id);
    if (it != m_windows.end()) {
        SetWindowTextA(it->second->hwnd, title);
    }
}

// etc.

} // namespace nkentseu
```

## Analyse de performance

Comparons les différentes approches du point de vue du coût d'appel d'une méthode typique (par exemple `SetTitle`).

| Approche | Structure de l'objet public | Coût d'un appel |
|----------|-----------------------------|-----------------|
| Héritage virtuel | Pointeur vers vtable + données (ou pointeur vers impl) | Une indirection (vtable) |
| PIMPL avec vtable manuelle | `std::unique_ptr<Impl>` (pointeur) + éventuellement buffer | Une indirection (pointeur de fonction) |
| PIMPL avec ID | Un entier (4/8 octets) | Recherche dans une map + indirection (appel de fonction sur l'impl trouvée) |

### Détail du coût pour l'approche ID

1. **Recherche dans la map** : selon le conteneur utilisé (`std::unordered_map` ou `std::map`), le coût est O(1) en moyenne (hash) ou O(log n) (arbre). Pour un nombre modéré de fenêtres (disons < 100), la différence est négligeable. Mais pour des milliers de fenêtres, une `unordered_map` bien dimensionnée reste très rapide.
2. **Appel de fonction** : une fois l'implémentation récupérée, on appelle une fonction membre ou une fonction libre sur cette implémentation. C'est une indirection supplémentaire (similaire à un appel virtuel, mais via un pointeur de fonction stocké dans l'impl ou directement une fonction membre).
3. **Comparaison** : l'approche ID ajoute une étape de lookup par rapport aux autres méthodes. Cependant, cette étape est souvent très rapide (quelques cycles CPU) et peut même être optimisée par le compilateur si la map est petite et que l'ID est utilisé fréquemment (mais en général, le lookup reste un surcoût).

### Mesures subjectives

- Si vous avez **peu de fenêtres** (1 à 10), le surcoût de la map est imperceptible.
- Si vous appelez **très fréquemment** des méthodes sur une même fenêtre (par exemple dans une boucle de rendu), l'approche ID peut devenir légèrement plus lente car chaque appel nécessite une recherche. Dans ce cas, il peut être intéressant de mettre en cache le pointeur d'implémentation localement dans le code appelant, mais cela casse le principe d'encapsulation.
- L'approche ID est **plus lente que les approches à pointeur direct**, mais elle offre d'autres avantages.

### Facteurs d'accélération possibles

- Utiliser un `std::unordered_map` avec un hash simple (l'ID lui-même peut servir de hash si on utilise une `std::vector` indexée, mais alors il faut un vecteur de taille fixe ou redimensionnable, ce qui peut être moins adapté si les ID sont très espacés).
- On peut aussi utiliser un tableau de pointeurs indexé par ID si les ID sont consécutifs et que le nombre de fenêtres est limité (par exemple, un pool). Cela donnerait un accès O(1) sans hash.

## Avantages de l'approche par ID

1. **Objet public ultra-léger** : un simple entier, facile à copier, à passer en paramètre, à stocker dans des structures de données.
2. **Pas d'allocation dynamique par fenêtre** : l'objet public n'alloue rien ; la map est gérée centralement. Cela peut réduire la fragmentation mémoire.
3. **Facilité d'intégration avec d'autres langages ou systèmes** : les ID sont faciles à passer via des API C, à sérialiser, etc.
4. **Sécurité** : on peut vérifier la validité de l'ID avant d'accéder à l'implémentation, évitant les pointeurs pendants (si on gère correctement la suppression).
5. **Possibilité de référencer une fenêtre sans avoir à gérer sa durée de vie** : l'objet public peut être détruit sans affecter la fenêtre sous-jacente (si on utilise un comptage de références ou un système de gestion de ressources). Mais ici, l'objet public est propriétaire (le destructeur appelle Close).

## Inconvénients

1. **Surcoût de lookup** : chaque opération nécessite une recherche dans la map.
2. **Complexité accrue** : il faut gérer un backend singleton, une map, et s'assurer de la cohérence des ID (pas de collision, suppression propre).
3. **Pas d'héritage ni de polymorphisme** : difficile d'étendre le comportement sans modifier le backend.
4. **Gestion des événements** : les callbacks système (WndProc, etc.) doivent pouvoir retrouver l'ID à partir du handle natif. Dans l'exemple, on stocke l'ID dans `GWLP_USERDATA`, ce qui est une indirection supplémentaire mais acceptable.

## Comparaison avec les approches précédentes

| Critère | Héritage virtuel | PIMPL vtable | PIMPL ID |
|---------|------------------|--------------|----------|
| Taille de l'objet public | Pointeur (8) + éventuellement données | Pointeur (8) | 4-8 octets |
| Allocation | 1 allocation (objet dérivé) | 1 allocation (impl) | 0 allocation (objet) + map interne |
| Indirections par appel | 1 (vtable) | 1 (pointeur de fonction) | 2 (lookup + pointeur de fonction) |
| Flexibilité | Polymorphisme standard | Contrôle total, pas de virtual | Centralisé, mais moins flexible |
| Sécurité | Pointeur brut (risque de dangling) | unique_ptr (sûr) | ID avec validation (sûr) |
| Facilité de debug | Modérée | Bonne (on peut inspecter l'impl) | Bonne (on peut tracer les ID) |

## Conclusion

L'approche par identifiant est un compromis intéressant lorsque la légèreté des objets publics et la simplicité d'interface sont prioritaires, au prix d'un léger surcoût de lookup. Dans un moteur comme NkWindow, où le nombre de fenêtres est généralement faible (souvent 1 à 5), ce surcoût est négligeable. Pour des applications avec des centaines de fenêtres (par exemple un IDE avec de nombreux panneaux), il faudra évaluer si la map devient un goulot d'étranglement.

En pratique, les performances des maps modernes (unordered_map) sont excellentes, et l'approche ID reste très viable. Si la performance est absolument critique, on peut optimiser en utilisant un tableau indexé (pool) ou en mettant en cache localement le pointeur d'implémentation après le premier accès.

Cette technique est donc une alternative valable, surtout pour les API de haut niveau où la clarté et la simplicité des objets publics priment sur les micro-optimisations.