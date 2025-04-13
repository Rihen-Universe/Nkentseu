--[[
@File premake5.lua
@Description Configuration de la bibliothèque Nkentseu - Contient les fonctionnalités centrales du système
@Key Features:
  - Gestion des PCH (Precompiled Headers)
  - Configuration multi-plateforme
  - Export des symboles pour Windows
@Author [Votre Nom]
@Date [2025-01-20]
@License Rihen
]]

-- Inclusion de la configuration centrale (chemin relatif)
-- include "../Tools/config.lua"

--[[
@Project Nkentseu
@Description Bibliothèque statique contenant les composants fondamentaux
@Configuration:
  - Type: StaticLib
  - Utilise ConfigureProject() pour l'initialisation standard
]]
project "Nkentseu"
    -- kind("StaticLib")
    ConfigureProject("SharedLibrary", "Nkentseu")
    
    --[[
    @Configuration PCH
    @Description Configure les en-têtes précompilés du projet
    @Param: Chemin extrait de LibPaths.Internal.Nkentseu.pch
    ]]
    ConfigurePch(LibPaths.Internal.Nkentseu.pch)
    
    --[[
    @Section Fichiers Sources
    @Description Structure des sources du projet
    @Patterns:
      - **.h: Tous les fichiers d'en-tête
      - **.cpp: Tous les fichiers source C++
    ]]
    files {
        "src/**.h",
        "src/**.cpp",
    }
    
    --[[
    @Section Include Directories
    @Description Répertoires d'inclusion pour le projet
    @Utilise: Chemin défini dans LibPaths.Internal.Nkentseu.include
    ]]
    includedirs {
        LibPaths.Internal.Nkentseu.include
    }
    
    --[[
    @Section Configuration Windows (DLL)
    @Description Définit les symboles d'export pour les builds dynamiques
    @Filter: Ne s'applique que pour les bibliothèques partagées
    ]]
    filter { "kind:SharedLib" }
        defines { "NKENTSEU_NKENTSEU_EXPORTS" }
    filter {}

-- Fin du fichier Nkentseu/premake5.lua
-- Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
-- Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
-- © Rihen 2025 - Tous droits réservés.