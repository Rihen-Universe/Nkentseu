#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
config.jenga — Configuration globale du build Nkentseu
=======================================================
Ce fichier centralise deux responsabilités :

  1. _GLOBAL_KIND : interrupteur unique static ↔ shared pour toute la solution.
     Changer _GLOBAL_KIND ici suffit à rebâtir tous les modules avec le bon linkage.

  2. useglobalkind(modulename) : helper à appeler dans chaque .jenga pour :
       a) Déclarer le ProjectKind + la macro d'export du module courant.
       b) Injecter automatiquement les defines NKENTSEU_XXXXX_STATIC_LIB
          pour chaque dépendance directe ET transitive, uniquement en mode STATIC.
          En mode SHARED, rien n'est défini (les DLL gèrent leurs symboles seuls).

Usage dans un .jenga (à l'intérieur de `with project("NKNetwork"):`):
    from config import *
    with project("NKNetwork"):
        useglobalkind("network")   # ← remplace kindexport(...) + defines([deps...])
        ...

Règle de nommage :
    Clé courte    Préfixe export          Define static émis
    ----------    ---------------         --------------------------
    "platform"    NKENTSEU_PLATFORM   →   NKENTSEU_PLATFORM_STATIC_LIB
    "network"     NKENTSEU_NETWORK    →   NKENTSEU_NETWORK_STATIC_LIB
    ...
"""

from Jenga import *                                         # noqa: F401, F403
try:
    from Jenga import ProjectKind, kindexport, defines      # noqa: F811
except ImportError:
    pass  # Résolu à l'exécution par le runtime Jenga

# =============================================================================
# SECTION 1 : KIND GLOBAL
# =============================================================================
# Modifiez uniquement cette valeur pour passer tous les modules en shared lib.
#
#   ProjectKind.STATIC_LIB  → bibliothèque statique (.lib / .a)
#                              chaque consommateur reçoit NKENTSEU_XXXXX_STATIC_LIB
#   ProjectKind.SHARED_LIB  → bibliothèque dynamique (.dll / .so / .dylib)
#                              aucun define _STATIC_LIB n'est émis
# =============================================================================

_GLOBAL_KIND = ProjectKind.STATIC_LIB

# =============================================================================
# SECTION 2 : REGISTRE DES MODULES
# =============================================================================
# Format : clé_courte → (préfixe_export, [dépendances_directes])
#
#   clé_courte      : nom minuscule sans "nk" ni "nkentseu" (ex: "network")
#   préfixe_export  : racine de la macro d'export (ex: "NKENTSEU_NETWORK")
#                     kindexport génère {préfixe}_STATIC_LIB ou {préfixe}_EXPORTS
#   dépendances     : liste de clés courtes des dépendances DIRECTES
#                     useglobalkind résoud les transitives automatiquement
#
# IMPORTANT : les dépendances ne listent que les deps directes déclarées dans
# dependson([...]). Le parcours DFS construit la fermeture transitive.
# =============================================================================

_REGISTRY: dict = {
    # ---- Foundation ----------------------------------------------------------
    # Pas de dépendance externe, linkés en static par convention.
    "platform"      : ("NKENTSEU_PLATFORM",      []),
    "core"          : ("NKENTSEU_CORE",           ["platform"]),
    "memory"        : ("NKENTSEU_MEMORY",         ["platform", "core"]),
    "containers"    : ("NKENTSEU_CONTAINERS",     ["platform", "core", "memory"]),
    "math"          : ("NKENTSEU_MATH",           ["platform", "core", "memory", "containers"]),

    # ---- System --------------------------------------------------------------
    "threading"     : ("NKENTSEU_THREADING",      ["platform", "core", "memory", "containers"]),
    "logger"        : ("NKENTSEU_LOGGER",         ["platform", "core", "memory", "containers", "threading"]),
    "filesystem"    : ("NKENTSEU_FILESYSTEM",     ["platform", "core", "memory", "containers", "threading", "logger"]),
    "stream"        : ("NKENTSEU_STREAM",         ["platform", "core", "logger", "filesystem"]),
    "time"          : ("NKENTSEU_TIME",           ["platform", "core", "memory", "containers", "logger"]),
    "reflection"    : ("NKENTSEU_REFLECTION",     ["platform", "core", "memory", "containers", "threading", "logger"]),
    "serialization" : ("NKENTSEU_SERIALIZATION",  ["platform", "core", "memory", "containers", "reflection", "filesystem", "logger"]),
    "network"       : ("NKENTSEU_NETWORK",        ["platform", "core", "memory", "containers", "logger", "threading", "math"]),

    # ---- Runtime -------------------------------------------------------------
    "event"         : ("NKENTSEU_EVENT",          ["platform", "core", "memory", "containers", "logger", "math"]),
    "window"        : ("NKENTSEU_WINDOW",         ["platform", "core", "memory", "containers", "logger", "math", "event"]),
    "context"       : ("NKENTSEU_CONTEXT",        ["platform", "core", "memory", "containers", "logger", "math", "event", "window"]),
    "rhi"           : ("NKENTSEU_RHI",            ["platform", "core", "memory", "containers", "logger", "math", "event", "window"]),
    "renderer"      : ("NKENTSEU_RENDERER",       ["platform", "core", "memory", "containers", "logger", "math", "event", "window", "rhi"]),
    "image"         : ("NKENTSEU_IMAGE",          ["platform", "core", "memory", "containers", "logger", "filesystem"]),
    "font"          : ("NKENTSEU_FONT",           ["platform", "core", "memory", "containers", "logger", "math", "image"]),
    "ecs"           : ("NKENTSEU_ECS",            ["platform", "core", "memory", "containers", "logger", "math"]),
    "camera"        : ("NKENTSEU_CAMERA",         ["platform", "core", "memory", "containers", "logger", "math"]),
    "ui"            : ("NKENTSEU_UI",             ["platform", "core", "memory", "containers", "logger", "math", "event", "window", "renderer"]),
    "audio"         : ("NKENTSEU_AUDIO",          ["platform", "core", "memory", "containers"]),

    # ---- Engine --------------------------------------------------------------
    # Module intégrateur : agrège tous les modules Runtime + System.
    # "nkentseu" et "engine" sont deux alias de la même entrée.
    "nkentseu"      : ("NKENTSEU_ENGINE",         ["platform", "core", "memory", "containers",
                                                   "math", "threading", "logger", "time",
                                                   "event", "window", "context", "rhi",
                                                   "font", "image", "ui"]),
    "engine"        : ("NKENTSEU_ENGINE",         ["platform", "core", "memory", "containers",
                                                   "math", "threading", "logger", "time",
                                                   "event", "window", "context", "rhi",
                                                   "font", "image", "ui"]),
}

# =============================================================================
# SECTION 3 : HELPERS INTERNES
# =============================================================================

def _collect_transitive(key: str, visited: set) -> None:
    """
    DFS récursif : remplit `visited` avec key + toutes ses dépendances transitives.
    Ignore les clés absentes du registre pour ne pas bloquer le build.

    Exemple pour "network" :
        visited = {"network", "platform", "core", "memory", "containers",
                   "logger", "threading", "math"}
    """
    if key in visited:
        return
    visited.add(key)
    entry = _REGISTRY.get(key)
    if entry is None:
        return
    for dep in entry[1]:
        _collect_transitive(dep, visited)


# =============================================================================
# SECTION 4 : useglobalkind
# =============================================================================

def useglobalkind(modulename: str) -> None:
    """
    Configure le kind et les defines d'export/import pour un module Nkentseu.

    Doit être appelé à l'intérieur d'un bloc `with project("XXX"):`, après
    avoir importé ce fichier (`from config import *`).

    Paramètre
    ----------
    modulename : str
        Clé courte du module dans _REGISTRY (insensible à la casse).
        Exemples : "network", "logger", "reflection", "ui", "platform"

    Comportement
    ------------
    STATIC_LIB (défaut) :
        • kindexport(ProjectKind.STATIC_LIB, "NKENTSEU_XXXXX")
          → le projet compile en .lib/.a
          → la macro NKENTSEU_XXXXX_STATIC_LIB est définie DANS le module
        • defines(["NKENTSEU_DEP_STATIC_LIB", ...])
          → chaque dépendance transitive reçoit son _STATIC_LIB

    SHARED_LIB :
        • kindexport(ProjectKind.SHARED_LIB, "NKENTSEU_XXXXX")
          → le projet compile en .dll/.so
          → la macro NKENTSEU_XXXXX_EXPORTS est définie pour marquer les symboles
        • Aucun define _STATIC_LIB n'est émis (les DLL exposent leurs symboles
          via __declspec(dllimport) / visibility=default)

    Exemple
    -------
    # NKNetwork.jenga
    from config import *

    with project("NKNetwork"):
        useglobalkind("network")
        # remplace :
        #   kindexport(ProjectKind.STATIC_LIB, "NKENTSEU_NETWORK")
        #   defines(["NKENTSEU_PLATFORM_STATIC_LIB",
        #            "NKENTSEU_CORE_STATIC_LIB",
        #            "NKENTSEU_MEMORY_STATIC_LIB",
        #            "NKENTSEU_CONTAINERS_STATIC_LIB",
        #            "NKENTSEU_LOGGER_STATIC_LIB",
        #            "NKENTSEU_THREADING_STATIC_LIB",
        #            "NKENTSEU_MATH_STATIC_LIB"])
        language("C++")
        ...
    """
    key = modulename.strip().lower()

    if key not in _REGISTRY:
        raise KeyError(
            f"[config.jenga] useglobalkind : module inconnu '{modulename}'.\n"
            f"  Clés disponibles : {sorted(_REGISTRY.keys())}"
        )

    export_prefix, _ = _REGISTRY[key]

    # ------------------------------------------------------------------
    # Étape 1 : déclare le kind + macro d'export pour CE module
    # ------------------------------------------------------------------
    kindexport(_GLOBAL_KIND, export_prefix)

    # ------------------------------------------------------------------
    # Étape 2 : collecte toutes les dépendances transitives
    # ------------------------------------------------------------------
    visited: set = set()
    _collect_transitive(key, visited)
    visited.discard(key)   # ce module n'est pas sa propre dépendance

    # ------------------------------------------------------------------
    # Étape 3 : émet les defines de dépendances selon le kind global
    #   STATIC_LIB → NKENTSEU_XXXXX_STATIC_LIB pour chaque dep
    #   SHARED_LIB → rien (les imports DLL sont gérés par kindexport)
    # ------------------------------------------------------------------
    if _GLOBAL_KIND == ProjectKind.STATIC_LIB:
        dep_defines = [
            f"{_REGISTRY[dep][0]}_STATIC_LIB"
            for dep in visited
            if dep in _REGISTRY
        ]
        if dep_defines:
            defines(sorted(dep_defines))   # sorted : build reproductible


# =============================================================================
# SECTION 5 : useappdeps
# =============================================================================

def useappdeps(*modulekeys: str) -> None:
    """
    Injecte les defines NKENTSEU_XXXXX_STATIC_LIB pour une application
    (consoleapp / windowedapp) qui consomme les modules listés.

    Contrairement à useglobalkind(), cette fonction ne configure PAS le kind
    du projet (windowedapp / consoleapp sont déclarés dans les filtres
    platform-spécifiques du .jenga de l'application).

    Paramètres
    ----------
    *modulekeys : str
        Une ou plusieurs clés courtes de modules dans _REGISTRY.
        Les dépendances transitives sont résolues automatiquement.

    Comportement
    ------------
    STATIC_LIB : émet defines([NKENTSEU_XXXXX_STATIC_LIB, ...]) pour chaque
                 module listé et toutes ses dépendances transitives.
    SHARED_LIB : ne fait rien (les DLL gèrent leurs imports seules).

    Exemples
    --------
    # Application utilisant le moteur complet
    with project("MySandbox"):
        useappdeps("engine")
        windowedapp()           # mis dans un filtre platform le cas échéant
        ...

    # Application utilisant uniquement le réseau et le logger
    with project("MyServer"):
        useappdeps("network", "logger")
        consoleapp()
        ...

    # Application utilisant le moteur + le réseau
    with project("MyGame"):
        useappdeps("engine", "network")
        windowedapp()
        ...
    """
    if _GLOBAL_KIND != ProjectKind.STATIC_LIB:
        return   # En mode shared : pas de _STATIC_LIB à définir

    visited: set = set()
    for name in modulekeys:
        key = name.strip().lower()
        if key not in _REGISTRY:
            raise KeyError(
                f"[config.jenga] useappdeps : module inconnu '{name}'.\n"
                f"  Clés disponibles : {sorted(_REGISTRY.keys())}"
            )
        _collect_transitive(key, visited)

    dep_defines = [
        f"{_REGISTRY[dep][0]}_STATIC_LIB"
        for dep in visited
        if dep in _REGISTRY
    ]
    if dep_defines:
        defines(sorted(dep_defines))   # sorted : build reproductible
