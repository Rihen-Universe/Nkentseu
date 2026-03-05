# Guide de Configuration du Système de Fenêtres (Windowing)

## Vue d'ensemble

Le nouveau système de détection `NKENTSEU_WINDOWING_*` remplace les ancien `NKENTSEU_PLATFORM_*` pour les sytèmes de fenêtres (XCB, Xlib, Wayland).

Trois options de sélection de fenêtres:
- **XCB** (X11 Core Protocol - moderne)
- **Xlib** (X11 legacy)
- **Wayland** (nouveau protocole d'affichage)

## Configuration par CMakeLists.txt

### Option 1 : Détection Automatique (Défaut)

Aucune configuration nécessaire. Les systèmes de fenêtres disponibles sont détectés automatiquement:

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(MyNkentseuProject)

add_executable(my_app src/main.cpp)

# Inclure Nkentseu
find_package(Nkentseu REQUIRED)
target_link_libraries(my_app Nkentseu::Nkentseu)
```

### Option 2 : Forcer XCB Uniquement

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyNkentseuProject)

add_executable(my_app src/main.cpp)

# Forcer XCB uniquement
add_compile_definitions(NKENTSEU_FORCE_WINDOWING_XCB_ONLY)

find_package(Nkentseu REQUIRED)
target_link_libraries(my_app Nkentseu::Nkentseu)
```

### Option 3 : Forcer Xlib Uniquement

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyNkentseuProject)

add_executable(my_app src/main.cpp)

# Forcer Xlib uniquement
add_compile_definitions(NKENTSEU_FORCE_WINDOWING_XLIB_ONLY)

find_package(Nkentseu REQUIRED)
target_link_libraries(my_app Nkentseu::Nkentseu)
```

### Option 4 : Forcer Wayland Uniquement

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyNkentseuProject)

add_executable(my_app src/main.cpp)

# Forcer Wayland uniquement
add_compile_definitions(NKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY)

find_package(Nkentseu REQUIRED)
target_link_libraries(my_app Nkentseu::Nkentseu)
```

## Configuration par Jenga

### Créer un projet avec configuration XCB

```bash
# Créer un projet interactif
jenga project MyApp --kind windowed --interactive

# Ou pré-configurer le CMakeLists.txt après création
jenga project MyApp --kind windowed --location .
```

Ensuite, éditer `MyApp/CMakeLists.txt`:

```cmake
# MyApp/CMakeLists.txt
add_compile_definitions(NKENTSEU_FORCE_WINDOWING_XCB_ONLY)
```

### Via script Jenga personnalisé

Créer un script `create_xcb_project.bat` (Windows) ou `create_xcb_project.sh` (Linux/macOS):

**Windows:**
```batch
@echo off
setlocal enabledelayedexpansion

set PROJECT_NAME=%1
if "%PROJECT_NAME%"=="" (
    echo Usage: create_xcb_project.bat ProjectName
    exit /b 1
)

REM Créer le projet standard
jenga project %PROJECT_NAME% --kind windowed --location .

REM Ajouter la configuration XCB au CMakeLists.txt
cd %PROJECT_NAME%
echo # Ajouter après "add_executable" >> CMakeLists.txt
echo add_compile_definitions(NKENTSEU_FORCE_WINDOWING_XCB_ONLY) >> CMakeLists.txt

echo Project %PROJECT_NAME% created with XCB-only configuration!
cd ..
```

**Linux/macOS:**
```bash
#!/bin/bash

PROJECT_NAME=$1
if [ -z "$PROJECT_NAME" ]; then
    echo "Usage: ./create_xcb_project.sh ProjectName"
    exit 1
fi

# Créer le projet standard
jenga project "$PROJECT_NAME" --kind windowed --location .

# Ajouter la configuration XCB
cd "$PROJECT_NAME"
echo "add_compile_definitions(NKENTSEU_FORCE_WINDOWING_XCB_ONLY)" >> CMakeLists.txt
cd ..

echo "Project $PROJECT_NAME created with XCB-only configuration!"
```

## Configuration par Ligne de Commande de Compilation

### Avec g++/clang++

```bash
# XCB uniquement
g++ -DNKENTSEU_FORCE_WINDOWING_XCB_ONLY main.cpp -o myapp

# Xlib uniquement
g++ -DNKENTSEU_FORCE_WINDOWING_XLIB_ONLY main.cpp -o myapp

# Wayland uniquement
g++ -DNKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY main.cpp -o myapp
```

### Avec MSVC (Windows)

```bash
cl /DNKENTSEU_FORCE_WINDOWING_XCB_ONLY main.cpp
```

## Utilisation dans le Code Source

Une fois configuré, utilisez les macros conditionnelles:

```cpp
#include "NkPlatformDetect.h"

// Exécuter du code uniquement pour XCB
NKENTSEU_XCB_ONLY({
    // Code spécifique à XCB
    xcb_connection_t* conn = xcb_connect(NULL, NULL);
});

// Exécuter du code uniquement pour Xlib
NKENTSEU_XLIB_ONLY({
    // Code spécifique à Xlib
    Display* display = XOpenDisplay(NULL);
});

// Exécuter du code uniquement pour Wayland
NKENTSEU_WAYLAND_ONLY({
    // Code spécifique à Wayland
    wl_display* display = wl_display_connect(NULL);
});

// Exécuter du code pour toute implémentation X11
NKENTSEU_X11_ONLY({
    // Code pour XCB ou Xlib
});

// Exécuter du code sauf pour une implémentation
NKENTSEU_NOT_XCB({
    // Code exécuté pour Xlib ou Wayland
});
```

## Macros de Détection Disponibles

### Macros de Sélection (Options de Compilation)

```cpp
#define NKENTSEU_FORCE_WINDOWING_XCB_ONLY      // Force XCB
#define NKENTSEU_FORCE_WINDOWING_XLIB_ONLY     // Force Xlib
#define NKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY  // Force Wayland
```

### Macros de Détection (Définies Automatiquement)

```cpp
#define NKENTSEU_WINDOWING_XCB      // XCB disponible
#define NKENTSEU_WINDOWING_XLIB     // Xlib disponible
#define NKENTSEU_WINDOWING_WAYLAND  // Wayland disponible
#define NKENTSEU_WINDOWING_X11      // XCB ou Xlib disponible

// Chaîne avec le système préféré détecté
#define NKENTSEU_WINDOWING_PREFERRED  "XCB" / "Xlib" / "Wayland" / "Unknown"
```

### Macros d'Exécution Conditionnelle

```cpp
// Exécution XCB
#define NKENTSEU_XCB_ONLY(...)    // Code si XCB
#define NKENTSEU_NOT_XCB(...)     // Code si pas XCB

// Exécution Xlib
#define NKENTSEU_XLIB_ONLY(...)   // Code si Xlib
#define NKENTSEU_NOT_XLIB(...)    // Code si pas Xlib

// Exécution Wayland
#define NKENTSEU_WAYLAND_ONLY(...) // Code si Wayland
#define NKENTSEU_NOT_WAYLAND(...) // Code si pas Wayland

// Exécution X11 (XCB ou Xlib)
#define NKENTSEU_X11_ONLY(...)    // Code si X11
#define NKENTSEU_NOT_X11(...)     // Code si pas X11
```

## Exemple Complet : Projet Multi-Windowing

**CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.15)
project(MultiWindowingApp)

# Configuration : par défaut détection auto, sinon décommenter une ligne
# add_compile_definitions(NKENTSEU_FORCE_WINDOWING_XCB_ONLY)
# add_compile_definitions(NKENTSEU_FORCE_WINDOWING_XLIB_ONLY)
# add_compile_definitions(NKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY)

add_executable(app src/main.cpp src/window_manager.cpp)

find_package(Nkentseu REQUIRED)
target_link_libraries(app Nkentseu::Nkentseu)

# Inclure les en-têtes
target_include_directories(app PRIVATE include)
```

**src/window_manager.cpp:**
```cpp
#include "NkPlatformDetect.h"
#include "window_manager.h"

WindowManager::WindowManager() {
    #if defined(NKENTSEU_WINDOWING_XCB)
        impl_type = "XCB";
    #elif defined(NKENTSEU_WINDOWING_XLIB)
        impl_type = "Xlib";
    #elif defined(NKENTSEU_WINDOWING_WAYLAND)
        impl_type = "Wayland";
    #else
        impl_type = "Unknown";
    #endif
    
    std::cout << "Window System: " << impl_type << std::endl;
}

void WindowManager::CreateWindow() {
    NKENTSEU_XCB_ONLY({
        // Implémentation XCB
        xcb_connection_t* conn = xcb_connect(NULL, NULL);
        // ... XCB implementation
        xcb_disconnect(conn);
    });
    
    NKENTSEU_XLIB_ONLY({
        // Implémentation Xlib
        Display* display = XOpenDisplay(NULL);
        // ... Xlib implementation
        XCloseDisplay(display);
    });
    
    NKENTSEU_WAYLAND_ONLY({
        // Implémentation Wayland
        struct wl_display* display = wl_display_connect(NULL);
        // ... Wayland implementation
        wl_display_disconnect(display);
    });
}
```

**src/main.cpp:**
```cpp
#include "NkPlatformDetect.h"
#include "window_manager.h"
#include <iostream>

int main() {
    std::cout << "Creating windowing application..." << std::endl;
    std::cout << "Preferred windowing system: " << NKENTSEU_WINDOWING_PREFERRED << std::endl;
    
    WindowManager mgr;
    mgr.CreateWindow();
    
    return 0;
}
```

## Résolution de Problèmes

### Erreur : "Multiple windowing systems detected"

Cela signifie que plusieurs en-têtes (xcb/xcb.h, X11/Xlib.h, wayland-client.h) sont disponibles mais aucune préférence n'est définie.

**Solution:** Forcer une implémentation unique:
```cmake
add_compile_definitions(NKENTSEU_FORCE_WINDOWING_XCB_ONLY)
```

### Erreur : "Windowing system headers not found"

L'en-tête spécifié n'est pas disponible sur le système.

**Solution:** Installer les développements (dev packages):
```bash
# Linux - XCB
sudo apt install libxcb1-dev

# Linux - Xlib
sudo apt install libx11-dev

# Linux - Wayland
sudo apt install libwayland-dev
```

### L'application se compile mais choisit la mauvaise implémentation

Assurez-vous que le flag est défini AVANT d'inclure `NkPlatformDetect.h`:

```cpp
// ✓ Correct
#define NKENTSEU_FORCE_WINDOWING_XCB_ONLY
#include "NkPlatformDetect.h"

// ✗ Incorrect
#include "NkPlatformDetect.h"
#define NKENTSEU_FORCE_WINDOWING_XCB_ONLY
```

## Résumé des Options

| Option | Détection | Priorité | Utilité |
|--------|-----------|----------|---------|
| **Auto** (par défaut) | Toutes disponibles | Wayland > XCB > Xlib | Distribution des bureaux |
| **XCB uniquement** | `-DNKENTSEU_FORCE_WINDOWING_XCB_ONLY` | XCB seul | Performance moderne |
| **Xlib uniquement** | `-DNKENTSEU_FORCE_WINDOWING_XLIB_ONLY` | Xlib seul | Compatibilité legacy |
| **Wayland uniquement** | `-DNKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY` | Wayland seul | Futur-proof |

---

**Fichier:** NkPlatformDetect.h  
**Version:** 1.0.0  
**Dernière mise à jour:** 2026-03-03  
**Auteur:** Rihen
