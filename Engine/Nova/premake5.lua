--[[
@File premake5.lua
@Description Configuration de l'application Nova - Point d'entrée principal du système
@Key Features:
  - Application console exécutable
  - Dépendances: Nkentseu + Unitest
  - Configuration spécifique au debug
@Author [Votre Nom]
@Date [2025-01-20]
@License Rihen
]]

-- include "../Tools/config.lua"

--[[
@Project Nova
@Description Application exécutable principale
@Configuration:
  - Type: ConsoleApp
  - Initialisation via ConfigureProject()
]]
project "Nova"
    --kind("ConsoleApp")
    ConfigureProject("Application", "Nova")
    
    -- Fichiers sources
    files {
        "src/**.h",
        "src/**.cpp"
    }
    
    --[[
    @Section Include Directories
    @Description Accès aux headers du projet Nova
    ]]
    includedirs {
        LibPaths.Internal.Nova.include
    }
    
    --[[
    @Section Dépendances
    @Description Liste des projets requis pour le linking
    @Implementation: Utilise AddDependencies() du config.lua
    ]]
    AddDependencies({ "Nkentseu", "Unitest" })
    
    --[[
    @Section Configuration Debug
    @Description Paramètres spécifiques au mode débogage
    @Field debugdir: Répertoire d'exécution pour l'IDE
    ]]
    filter { "configurations:Debug" }
        debugdir "../Bin/%{cfg.buildcfg}"
    filter {}

-- Fin du fichier Nova/premake5.lua
-- Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
-- Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
-- © Rihen 2025 - Tous droits réservés.