# 📜 Module Logger - Système de Journalisation Avancé

Le module **Logger** fournit une solution complète de journalisation pour les applications C++, avec support multi-cibles, formatage avancé et gestion des niveaux de criticité.

## 🌟 Fonctionnalités Clés

- 🎯 **Multi-cibles** : Console, fichiers, réseau (via implémentation custom)
- 🚦 **8 niveaux de sévérité** : De `TRACE` à `FATAL`
- 🌈 **Couleurs ANSI** (support Windows/Linux/macOS)
- 📁 **Rotation automatique** des fichiers logs
- 🔧 **Formatage personnalisable** : Styles `{}`, `[]`, `<>`, `()`
- 📍 **Tracking contexte** : Fichier/ligne/fonction source
- 🧵 **Thread-safe** : Prêt pour applications multi-thread

## 🛠 Installation

### Prérequis
- Compilateur C++20
- [Premake5](https://premake.github.io/)
- Modules nkentseu requis : `Platform`, `Memory`, `Epoch`

### Configuration
```lua
project "MyApp/MyLib"
    kind "SharedLib/StaticLib/ConsoleApp"
    includedirs { "chemin/vers/nkentseu/include" }
    links { "Nkentseu" }
```

## 🚀 Utilisation Rapide

### Journalisation de base
```cpp
#include <Nkentseu/Logger/Logger.h>

void InitSystem() {
    // Utilisation directe du logger système
    logger.Info("Démarrage de l'application");
    
    logger.Warning("Configuration non optimale")
          .Debug("Mémoire disponible: {} Mo", GetAvailableMemory());
    
    try {
        LoadConfig();
    } catch (const std::exception& e) {
        logger.Errors("Échec chargement config: {}", e.what());
    }
}
```

### Création d'un logger personnalisé
```cpp
auto monLogger = nkentseu::Logger("MonApp");
monLogger.AddTarget(Memory.MakeShared<FileLogger>("logs/", "app"));
monLogger.AddTarget(Memory.MakeShared<ConsoleLogger>());

monLogger.Info("Logger personnalisé prêt !");
```

## 🧩 Composants Principaux

### `LogSeverity.h`
**Niveaux de criticité**  
```cpp
logger.Trace("Détail bas niveau");    // Suivi d'exécution
logger.Debug("Variable: {}", x);      // Débogage
logger.Info("Nouvel utilisateur");    // Information
logger.Warning("API lente");          // Avertissement
logger.Errors("Fichier manquant");    // Erreur
logger.Critical("DB offline");        // Critique
logger.Fatal("Corruption mémoire");   // Fatal
```

### `LoggerTarget.h`
**Interface pour cibles de log**  
Implémentez vos propres cibles :
```cpp
class NetworkLogger : public LoggerTarget {
    void Write(const LogMessage& msg) override {
        // Envoi du msg.formaté sur le réseau
    }
};
```

### `ConsoleLogger.h`
**Sortie console avec couleurs**  
![Exemple de sortie colorée](https://via.placeholder.com/600x200.png?text=Sortie+Console+ANSI)

### `FileLogger.h`
**Journalisation fichier avec rotation**  
```cpp
auto fileLogger = Memory.MakeShared<FileLogger>("logs/", "app");
fileLogger->SetMaxFileSize(100); // Rotation à 100Mo
fileLogger->SetRotationPolicy(7); // Conservation 7 jours
```

### `FormatterSystem.h`
**Formatage avancé des messages**  
```cpp
// Style curly par défaut
logger.Info("Bonjour {0}, vous avez {1} messages", "Alice", 5);

// Style personnalisé
Formatter.FormatSquare("[0] erreurs en [1]", 3, "moduleX");
```

## ⚙️ Configuration Avancée

### Niveaux de Log
```cpp
// Filtrage des logs
logger.SetMinSeverity(LogSeverity::Level::Info);

// Niveau global
nkentseu::Logger::SystemLogger().SetLevel(LogSeverity::Level::Debug);
```

### Formatage Personnalisé
```cpp
// Style des placeholders
logger.SetStyle(FormatterSystem::Style::Square); // Utilise [0], [1]

// Autoriser plusieurs styles
logger.SetAnyStyle(true); // Accepte {0} et [1] dans le même message
```

### Cibles Spécialisées
```cpp
// Envoi des erreurs critiques par email
class EmailLogger : public LoggerTarget {
    void Write(const LogMessage& msg) override {
        if (msg.severity >= LogSeverity::Level::Critical) {
            SendEmail("admin@site.com", "ERREUR", msg.message);
        }
    }
};

logger.AddTarget(Memory.MakeShared<EmailLogger>());
```

## 📋 Format des Logs
```
[MonApp][INFOS] [2024-04-10 14:35:22] [main.cpp:42 -> Init()] >> Démarrage réussi
[MonApp][ERROR] [2024-04-10 14:35:23] [network.cpp:127 -> Connect()] >> Timeout API
```

## ⚠️ Bonnes Pratiques

- Utilisez `logger` macro pour le contexte automatique
- Évitez les logs haute fréquence en production (`TRACE`/`DEBUG`)
- Privilégiez le formatage structuré pour l'analyse
- Configurez des politiques de rotation adaptées

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
