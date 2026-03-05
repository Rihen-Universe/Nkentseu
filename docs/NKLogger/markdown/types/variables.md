# 📦 Variables

> 44 éléments

[🏠 Accueil](../index.md) | [🎯 Types](./index.md)

## Liste

- **[`SHORT_PATTERN`](../files/Pattern.h.md#short_pattern)** — Pattern court (pour logs de production) Format: 12:34:56 INF Message
- **[`argsCopy`](../files/Logger.cpp.md#argscopy)** — Formatage variadique
- **[`false`](../files/Logger.cpp.md#false)** — Vérifie si un niveau devrait être loggé
- **[`formatted`](../files/ConsoleSink.cpp.md#formatted)** — Logge un message dans la console
- **[`hConsole`](../files/ConsoleSink.cpp.md#hconsole)** — Obtient le code de réinitialisation de couleur
- **[`i`](../files/Formatter.cpp.md#i)** — Parse le pattern en tokens
- **[`i`](../files/RotatingFileSink.cpp.md#i)** — Effectue la rotation des fichiers
- **[`instance`](../files/Log.cpp.md#instance)** — Destructeur
- **[`instance`](../files/Log.cpp.md#instance)** — Obtient l'instance singleton
- **[`instance`](../files/Registry.cpp.md#instance)** — Obtient l'instance singleton du registre
- **[`instance`](../files/Registry.cpp.md#instance)** — Nettoie le registre (détruit tous les loggers)
- **[`it`](../files/Registry.cpp.md#it)** — Obtient un logger par son nom (crée si non existant)
- **[`m_Enabled`](../files/Logger.cpp.md#m_enabled)** — Vérifie si le logger est actif
- **[`m_FlushInterval`](../files/AsyncSink.cpp.md#m_flushinterval)** — Définit l'intervalle de flush
- **[`m_GlobalLevel`](../files/Registry.cpp.md#m_globallevel)** — Obtient le niveau de log global
- **[`m_MaxDays`](../files/DailyFileSink.cpp.md#m_maxdays)** — Définit le nombre maximum de jours à conserver
- **[`m_MaxFiles`](../files/RotatingFileSink.cpp.md#m_maxfiles)** — Définit le nombre maximum de fichiers
- **[`m_MaxQueueSize`](../files/AsyncSink.cpp.md#m_maxqueuesize)** — Définit la taille maximum de la file
- **[`m_MaxQueueSize`](../files/AsyncSink.cpp.md#m_maxqueuesize)** — Obtient la taille maximum de la file
- **[`m_MaxSize`](../files/RotatingFileSink.cpp.md#m_maxsize)** — Définit la taille maximum des fichiers
- **[`m_MaxSize`](../files/RotatingFileSink.cpp.md#m_maxsize)** — Obtient la taille maximum des fichiers
- **[`m_RotationMinute`](../files/DailyFileSink.cpp.md#m_rotationminute)** — Obtient la minute de rotation
- **[`m_Stream`](../files/ConsoleSink.cpp.md#m_stream)** — Définit le flux de console
- **[`m_Stream`](../files/ConsoleSink.cpp.md#m_stream)** — Obtient le flux de console courant
- **[`m_UseColors`](../files/ConsoleSink.cpp.md#m_usecolors)** — Active ou désactive les couleurs
- **[`m_UseColors`](../files/ConsoleSink.cpp.md#m_usecolors)** — Vérifie si les couleurs sont activées
- **[`m_UseStderrForErrors`](../files/ConsoleSink.cpp.md#m_usestderrforerrors)** — Définit si le sink doit utiliser stderr pour les niveaux d'erreur
- **[`message`](../files/Logger.cpp.md#message)** — Log avec message string et informations de source
- **[`message`](../files/Logger.cpp.md#message)** — Log avec format string, informations de source et va_list
- **[`msg`](../files/Logger.cpp.md#msg)** — Log interne avec informations de source
- **[`msg`](../files/AsyncSink.cpp.md#msg)** — Log asynchrone avec message pré-formaté
- **[`msg`](../files/AsyncSink.cpp.md#msg)** — Fonction du thread de traitement
- **[`name`](../files/Log.h.md#name)** — Obtient l'instance singleton du logger par défaut
- **[`now`](../files/DailyFileSink.cpp.md#now)** — Vérifie et effectue la rotation si nécessaire
- **[`queueSize`](../files/AsyncSink.h.md#queuesize)** — Constructeur avec nom et configuration
- **[`rotatedFile`](../files/DailyFileSink.cpp.md#rotatedfile)** — Effectue la rotation quotidienne
- **[`sourceFile`](../files/Log.h.md#sourcefile)** — Configure le nom du logger (retourne *this pour chaînage)
- **[`sourceFile`](../files/Logger.h.md#sourcefile)** — Vérifie si le logger est actif
- **[`threadId`](../files/LogMessage.h.md#threadid)** — ID du thread émetteur
- **[`threadName`](../files/LogMessage.h.md#threadname)** — Nom du thread (optionnel)
- **[`timePoint`](../files/LogMessage.h.md#timepoint)** — Heure de réception du message
- **[`timestamp`](../files/LogMessage.h.md#timestamp)** — Timestamp en nanosecondes depuis l'epoch
- **[`tm`](../files/Formatter.cpp.md#tm)** — Formate un token individuel
- **[`true`](../files/RotatingFileSink.cpp.md#true)** — Force la rotation du fichier
