--[[
@File premake5.lua
@Description Configuration du système de tests Unitest - Framework de tests unitaires
@Key Features:
  - Bibliothèque statique indépendante
  - Dépendance: Nkentseu
  - Gestion des exports Windows
@Author [Votre Nom]
@Date [2025-01-20]
@License Rihen
]]

-- include "../Tools/config.lua"

--[[
@Project Unitest
@Description Bibliothèque de tests unitaires
@Configuration:
  - Type: StaticLib
  - Initialisation standard via ConfigureProject()
]]
project "Unitest"
    --kind("StaticLib")
    ConfigureProject("Library", "Unitest")
    
    -- Configuration PCH spécifique
    ConfigurePch(LibPaths.Internal.Unitest.pch)
    
    -- Fichiers sources
    files {
        "src/**.h",
        "src/**.cpp"
    }
    
    -- Répertoires d'inclusion
    includedirs {
        LibPaths.Internal.Unitest.include
    }
    
    --[[
    @Section Dépendances
    @Description Nécessite uniquement la bibliothèque Nkentseu
    ]]
    AddDependencies({ "Nkentseu" })
    
    --[[
    @Section Configuration Windows (DLL)
    @Description Export des symboles pour les builds dynamiques
    ]]
    filter { "kind:SharedLib" }
        defines { "NKENTSEU_UNITEST_EXPORTS" }
    filter {}

-- Fin du fichier Unitest/premake5.lua
-- Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
-- Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
-- © Rihen 2025 - Tous droits réservés.