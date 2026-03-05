--[[
@File premake5.lua
@Description Configuration centrale du workspace - Définit l'organisation des projets et les paramètres globaux
@Structure:
  - Déclaration du workspace principal
  - Configuration des environnements de build
  - Inclusion des projets enfants
@Author [Votre Nom]
@Date [2025-01-20]
@License Rihen
]]

-- Inclusion de la configuration centrale
include "Tools/config.lua"

--[[
@Workspace NkentseuCLI
@Description Conteneur principal pour l'ensemble des projets
@Param startproject: Projet lancé par défaut dans l'IDE
]]
workspace "NkentseuCLI"
    startproject "Shell"
    
    --[[
    @Section Configurations Globales
    @Description Paramètres communs à tous les projets
    @Field configurations: Liste des profiles de compilation
    @Field platforms: Architecture cible
    ]]
    configurations { "Debug", "Release" }
    platforms { "x64" }
    
    --[[
    @Section Inclusion des Projets
    @Description Liste des sous-projets du workspace
    @Order:
      1. Nkentseu - Bibliothèque centrale
      2. Unitest - Système de tests
      3. Nova - Application principale
    ]]

    group "Core"
      include "Core/Nkentseu"
      include "Core/Unitest"
    group ""

    group "Engine"
      include "Engine/Nova"
    group	""

-- Fin du fichier premake5.lua
-- Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
-- Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
-- © Rihen 2025 - Tous droits réservés.