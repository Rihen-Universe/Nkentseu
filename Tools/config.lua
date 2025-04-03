--[[
@File config.lua
@Description Configuration centrale du système de build - Gère la configuration projet, le logging et les outils de compilation
@Author [Votre Nom]
@Date [2025-01-20]
@License Rihen
]]

-------------------------------------------------------------------------------
-- Initialisation du Logger (Exemple d'implémentation)
-------------------------------------------------------------------------------

-------------------------------------------------------------------------------
-- Système de Logging Étendu
-------------------------------------------------------------------------------

--[[
@Module Logger
@Description Système centralisé de gestion des logs avec sortie colorée et niveaux de gravité
@Implémentation:
  - Gestion multi-niveaux (DEBUG à FATAL)
  - Support des codes couleurs ANSI
  - Configuration dynamique
@Author [Votre Nom]
@Date [2025-01-20]
]]
Logger = {
    --[[
    @Table Config
    @Description Configuration du système de logging
    @Field EnableColors (bool) Active les couleurs ANSI
    @Field LogLevel (string) Niveau de log minimum affiché
    @Field ColorCodes (table) Mapping niveau->code couleur ANSI
    ]]
    Config = {
        EnableColors = true,
        LogLevel = "INFO",
        ColorCodes = {
            DEBUG = "\27[36m",   -- Cyan
            INFO = "\27[32m",     -- Vert
            WARNING = "\27[33m",  -- Jaune
            ERROR = "\27[31m",    -- Rouge
            FATAL = "\27[35m",    -- Magenta
            RESET = "\27[0m"
        }
    }
}

--[[
@Function Logger.Init
@Description Initialise la configuration du logger avec des paramètres personnalisés
@Param Config (table) Table de configuration partielle (merge avec la config par défaut)
@Usage:
  Logger.Init({LogLevel = "DEBUG", EnableColors = false})
]]
function Logger.Init(Config)
    for k, v in pairs(Config) do
        Logger.Config[k] = v
    end
end

--[[
@Function Logger.Log
@Description Méthode interne de log générique
@Param Level (string) Niveau de log conforme aux clés de ColorCodes
@Param Message (string) Message à logger
@Private
]]
function Logger.Log(Level, Message)
    local levels = {DEBUG=1, INFO=2, WARNING=3, ERROR=4, FATAL=5}
    if levels[Level] < levels[Logger.Config.LogLevel] then return end

    local header = "["..Level.."]"
    if Logger.Config.EnableColors then
        header = Logger.Config.ColorCodes[Level]..header..Logger.Config.ColorCodes.RESET
    end
    
    print(string.format("%s %s | %s", header, os.date("%Y-%m-%d %H:%M:%S"), Message))
end

-- Niveaux de log spécifiques
--[[
@Function Logger.Debug
@Description Log un message de débogage (niveau 1)
@Param Message (string) Message technique pour développeurs
]]
function Logger.Debug(Message)
    Logger.Log("DEBUG", Message)
end

--[[
@Function Logger.Info
@Description Log un message informatif (niveau 2)
@Param Message (string) Information sur l'exécution
]]
function Logger.Info(Message)
    Logger.Log("INFO", Message)
end

--[[
@Function Logger.Warning
@Description Log un avertissement (niveau 3)
@Param Message (string) Avertissement non critique
]]
function Logger.Warning(Message)
    Logger.Log("WARNING", Message)
end

--[[
@Function Logger.Error
@Description Log une erreur (niveau 4)
@Param Message (string) Erreur critique mais non fatale
]]
function Logger.Error(Message)
    Logger.Log("ERROR", Message)
end

--[[
@Function Logger.Fatal
@Description Log une erreur fatale (niveau 5) et quitte l'application
@Param Message (string) Message d'erreur avant extinction
]]
function Logger.Fatal(Message)
    Logger.Log("FATAL", Message)
    os.exit(1)
end

-------------------------------------------------------------------------------
-- Exemple d'utilisation
-------------------------------------------------------------------------------

-- Initialisation personnalisée
Logger.Init({
    LogLevel = "DEBUG",
    EnableColors = true
})

-------------------------------------------------------------------------------
-- Configuration Globale
-------------------------------------------------------------------------------

--[[
@Table ProjectConfig
@Description Configuration centrale pour le build system
@Structure:
  - Common: Paramètres généraux inter-projets
  - Build: Répertoires de sortie
  - ProjectTypes: Templates de projets
  - Configurations: Environnements de build
  - Clang: Configuration spécifique au toolchain
]]
ProjectConfig = {
    --[[
    @Section Common
    @Description Paramètres communs à tous les projets
    @Field Language: Langage cible
    @Field CppDialect: Version du C++
    @Field Architecture: Architecture cible
    ]]
    Common = {
        Language = "C++",
        CppDialect = "C++20",
        Architecture = "x64",
        Warnings = "Extra",
        StaticRuntime = "On",
        DefaultToolset = "clang",
        System = "native"
    },

    --[[
    @Section Build
    @Description Structure des répertoires de build
    @Variables spéciales:
      - %{cfg.buildcfg}: Configuration (Debug/Release)
      - %{cfg.system}: OS cible
      - %{cfg.architecture}: Architecture
    ]]
    Build = {
        OutputDir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}",
        BinDir = "%{wks.location}/Build/bin/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}",
        ObjDir = "%{wks.location}/Build/obj/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
    },

    --[[
    @Section ProjectTypes
    @Description Templates pour différents types de projets
    @Keys:
      - Library: Bibliothèque statique
      - Application: Exécutable
      - SharedLibrary: Bibliothèque dynamique
    ]]
    ProjectTypes = {
        Library = {
            Kind = "StaticLib",
            ExportsDefines = { "NKENTSEU_EXPORTS", "NKENTSEU_STATIC" }
        },
        Application = {
            Kind = "ConsoleApp"
        },
        SharedLibrary = {
            Kind = "SharedLib",
            ExportsDefines = { "NKENTSEU_EXPORTS", "NKENTSEU_SHARED_LIB" }
        }
    },

    --[[
    @Section Configurations
    @Description Liste des configurations de build supportées
    ]]
    Configurations = {
        "Debug",
        "Release"
    },

    --[[
    @Section ConfigurationSettings
    @Description Paramètres par configuration de build
    @Keys:
      - Debug: Symboles de debug, optimisations minimales
      - Release: Optimisations maximales
    ]]
    ConfigurationSettings = {
        Debug = {
            Symbols = "On",
            Optimize = "Debug",
            Defines = { "NKENTSEU_DEBUG" }
        },
        Release = {
            Symbols = "Off",
            Optimize = "Full",
            Defines = { "NKENTSEU_RELEASE" }
        }
    },

    --[[
    @Section Clang
    @Description Configuration avancée pour le compilateur Clang
    @Structure:
      - Flags: Options communes à toutes les plateformes
      - OSFlags: Options spécifiques par OS
      - LinkFlags: Options d'édition des liens
    ]]
    Clang = {
        Toolset = "clang",
        Flags = { 
            "-Wall", 
            "-Wextra", 
            "-Wpedantic",
            "-fcolor-diagnostics",
        },
        
        OSFlags = {
            Windows = {},
            Linux = {"-stdlib=libc++"},
            MacOS = {"-static"}
        },
        
        LinkFlags = {
            Common = {
                "-fuse-ld=lld",
                "-stdlib=libstdc++",
            },
            Windows = {
                "-lwinpthread",
                "-static-libstdc++",
                "-static"
            },
            Linux = {
                "-lpthread",
                "-stdlib=libc++",
                "-licuuc",
                "-licudata"
            },
            MacOS = {
                "-lpthread",
                "-static"
            }
        }
    },

    --[[
    @Section CpuOptimizations
    @Description Optimisations spécifiques au processeur
    @Keys:
      - Avx2: Instructions AVX2/FMA
      - Sse4: Instructions SSE4.1
    ]]
    CpuOptimizations = {
        Avx2 = {
            Defines = { "NKENTSEU_USE_AVX2" },
            WindowsFlags = { "/arch:AVX2" },
            UnixFlags = { "-mavx2", "-mfma" }
        },
        Sse4 = {
            Defines = { "NKENTSEU_USE_SSE4" },
            WindowsFlags = { "/arch:SSE4.1" },
            UnixFlags = { "-msse4.1" }
        }
    }
}

-------------------------------------------------------------------------------
-- Chemins des Bibliothèques
-------------------------------------------------------------------------------

--[[
@Table LibPaths
@Description Répertoires des dépendances logicielles
@Structure:
  - External: Bibliothèques tierces
  - Internal: Modules internes au projet
]]
LibPaths = {
    External = {
        Base = "%{wks.location}/External",
        Libs = "%{wks.location}/External/Libs"
    },
    Internal = {
        Nkentseu = { 
            path = "%{wks.location}/Core/Nkentseu", 
            include = "%{wks.location}/Core/Nkentseu/src", 
            pch = "%{wks.location}/Core/Nkentseu/pch" 
        },
        Unitest = { 
            path = "%{wks.location}/Core/Unitest", 
            include = "%{wks.location}/Core/Unitest/src", 
            pch = "%{wks.location}/Core/Unitest/pch" 
        },
        Nova = { 
            path = "%{wks.location}/Engine/Nova", 
            include = "%{wks.location}/Engine/Nova/src"
        }
    }
}

-------------------------------------------------------------------------------
-- Fonctions de Configuration
-------------------------------------------------------------------------------

--[[
@Function ConfigureProject
@Description Initialise un projet avec les paramètres par défaut
@Param ProjectType (string) Type de projet (Library/Application/SharedLibrary)
@Param ProjectName (string) Nom du projet
@SideEffects:
  - Définit le langage et l'architecture
  - Configure les répertoires de sortie
  - Applique les options de compilation
]]
function ConfigureProject(ProjectType, ProjectName)
    -- Configuration de base
    language(ProjectConfig.Common.Language)
    cppdialect(ProjectConfig.Common.CppDialect)
    architecture(ProjectConfig.Common.Architecture)
    warnings(ProjectConfig.Common.Warnings)
    staticruntime(ProjectConfig.Common.StaticRuntime)

    local current_os = os.target()
    toolset(ProjectConfig.Common.DefaultToolset)

    -- Configuration spécifique au type
    local ProjectTypeConfig = ProjectConfig.ProjectTypes[ProjectType]
    kind(ProjectTypeConfig.Kind)
    
    -- Répertoires de sortie
    targetdir(ProjectConfig.Build.BinDir .. "/" .. ProjectName)
    objdir(ProjectConfig.Build.ObjDir .. "/" .. ProjectName)
    
    configurations(ProjectConfig.Configurations)
    
    -- Paramètres par configuration
    for ConfigName, ConfigSettings in pairs(ProjectConfig.ConfigurationSettings) do
        filter({ "configurations:" .. ConfigName })
            symbols(ConfigSettings.Symbols)
            optimize(ConfigSettings.Optimize)
            defines(ConfigSettings.Defines)
        filter({})
    end
    
    if ProjectTypeConfig.ExportsDefines then
        defines(ProjectTypeConfig.ExportsDefines)
    end

    -- Configuration Clang multi-OS
    filter { "toolset:clang" }
        buildoptions(ProjectConfig.Clang.Flags)
        linkoptions(ProjectConfig.Clang.LinkFlags.Common)
        
        filter { "toolset:clang", "system:windows" }
            buildoptions(ProjectConfig.Clang.OSFlags.Windows)
            linkoptions(ProjectConfig.Clang.LinkFlags.Windows)

        filter { "toolset:clang", "system:linux" }
            buildoptions(ProjectConfig.Clang.OSFlags.Linux)
            linkoptions(ProjectConfig.Clang.LinkFlags.Linux)

        filter { "toolset:clang", "system:macosx" }
            buildoptions(ProjectConfig.Clang.OSFlags.MacOS)
            linkoptions(ProjectConfig.Clang.LinkFlags.MacOS)

    filter {}
end

--[[
@Function AddDependencies
@Description Ajoute des dépendances entre projets
@Param Dependencies (table) Liste des noms de projets dépendants
@ErrorHandling: Log un warning si dépendance inconnue
]]
function AddDependencies(Dependencies)
    for _, Dependency in ipairs(Dependencies) do
        if LibPaths.Internal[Dependency] then
            links(Dependency)
            includedirs(LibPaths.Internal[Dependency])
        else
            Logger.Warning("Dépendance inconnue détectée: " .. Dependency)
        end
    end
end

-------------------------------------------------------------------------------
-- Gestion des Bibliothèques Externes (Stub)
-------------------------------------------------------------------------------

--[[
@Function AddExternalLibraries
@Description Interface pour les dépendances externes (à implémenter)
@Param Libraries (table) Liste des bibliothèques externes
]]
function AddExternalLibraries(Libraries)
    for _, Library in ipairs(Libraries) do
        if LibPaths.External[Library] then
        end
    end
end

-------------------------------------------------------------------------------
-- Optimisations Processeur
-------------------------------------------------------------------------------

--[[
@Function ConfigureCpuOptimization
@Description Applique des optimisations processeur spécifiques
@Param Optimization (string) Type d'optimisation (Avx2/Sse4/none)
]]
function ConfigureCpuOptimization(Optimization)
    if not Optimization then return end
    
    local CpuConfig = ProjectConfig.CpuOptimizations[Optimization]
    if not CpuConfig then
        Logger.Info("Aucune optimisation CPU spécifique sélectionnée")
        return
    end
    
    defines(CpuConfig.Defines)
    
    filter({ "action:vs*" })
        buildoptions(CpuConfig.WindowsFlags)
    filter({ "system:linux or system:macosx" })
        buildoptions(CpuConfig.UnixFlags)
    filter({})
end

-------------------------------------------------------------------------------
-- Gestion des en-têtes précompilés
-------------------------------------------------------------------------------

--[[
@Function ConfigurePch
@Description Configure les en-têtes précompilés (PCH)
@Param PchPath (string) Chemin relatif vers le dossier PCH
@ErrorHandling: Log un warning si dossier inexistant
]]
function ConfigurePch(PchPath)
    local fullPath = path.getabsolute("pch")
    
    if not os.isdir(fullPath) then
        Logger.Warning("Le dossier PCH n'existe pas: " .. fullPath)
        return
    end
    
    pchheader("pch/pch.h")
    pchsource("pch/pch.cpp")
    
    files {
        "pch/**.h",
        "pch/**.cpp"
    }

    includedirs {
        "pch"
    }
    
    filter {}
    
    Logger.Info("Configuration PCH pour: " .. fullPath)
end

-------------------------------------------------------------------------------
-- Options de Compilation
-------------------------------------------------------------------------------

--[[
@Option arch
@Description Sélection d'optimisations matérielles
@Allowed:
  - avx2: Activer AVX2 + FMA
  - sse4: Activer SSE4.1
  - none: Aucune optimisation
]]
newoption {
    trigger = "arch",
    value = "TYPE",
    description = "Spécifier l'architecture d'optimisation",
    allowed = {
        { "avx2", "Activer AVX2 + FMA" },
        { "sse4", "Activer SSE4.1" },
        { "none", "Aucune optimisation" }
    }
}

-- Application des optimisations
if _OPTIONS["arch"] then
    ConfigureCpuOptimization(_OPTIONS["arch"]:lower())
end

-- Fin du fichier config.lua
-- Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
-- Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
-- © Rihen 2025 - Tous droits réservés.