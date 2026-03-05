# 🔍 Recherche Alphabétique

> 275 éléments

[🏠 Accueil](./index.md)

## Index

[A](#a) [C](#c) [D](#d) [E](#e) [F](#f) [G](#g) [H](#h) [I](#i) [L](#l) [M](#m) [N](#n) [O](#o) [P](#p) [Q](#q) [R](#r) [S](#s) [T](#t) [U](#u) [W](#w) 

---

<a name="a"></a>

## A

- ⚙️ **[`AddSink`](./files/Logger.h.md#addsink)** — Destructeur du logger
- ⚙️ **[`AddSink`](./files/Logger.h.md#addsink)** — Ajoute un sink au logger
- ⚙️ **[`AddSink`](./files/DistributingSink.h.md#addsink)** — Ajoute un sous-sink
- 📦 **[`argsCopy`](./files/Logger.cpp.md#argscopy)** — Formatage variadique

<a name="c"></a>

## C

- ⚙️ **[`Clear`](./files/Registry.h.md#clear)** — Supprime tous les loggers du registre
- ⚙️ **[`ClearSinks`](./files/Logger.h.md#clearsinks)** — Supprime tous les sinks du logger
- ⚙️ **[`ClearSinks`](./files/DistributingSink.h.md#clearsinks)** — Supprime tous les sous-sinks
- ⚙️ **[`Close`](./files/FileSink.h.md#close)** — Ferme le fichier
- ⚙️ **[`ConsoleSink`](./files/ConsoleSink.h.md#consolesink)** — Constructeur par défaut (stdout, avec couleurs)
- ⚙️ **[`ConsoleSink`](./files/ConsoleSink.h.md#consolesink)** — Constructeur avec flux spécifique
- ⚙️ **[`CreateLogger`](./files/Registry.cpp.md#createlogger)** — Crée un logger avec un nom spécifique
- ⚙️ **[`Critical`](./files/Logger.h.md#critical)** — Log critical avec format string
- ⚙️ **[`Critical`](./files/Logger.h.md#critical)** — Log critical avec stream style

<a name="d"></a>

## D

- ⚙️ **[`Debug`](./files/Logger.h.md#debug)** — Log debug avec format string
- ⚙️ **[`Debug`](./files/Logger.h.md#debug)** — Log debug avec stream style
- ⚙️ **[`DistributingSink`](./files/DistributingSink.h.md#distributingsink)** — Constructeur par défaut
- ⚙️ **[`DistributingSink`](./files/DistributingSink.h.md#distributingsink)** — Constructeur avec liste initiale de sinks
- ⚙️ **[`Drop`](./files/Registry.cpp.md#drop)** — Supprime un logger spécifique
- ⚙️ **[`DropAll`](./files/Registry.cpp.md#dropall)** — Supprime tous les loggers

<a name="e"></a>

## E

- ⚙️ **[`Error`](./files/Logger.h.md#error)** — Log error avec format string
- ⚙️ **[`Error`](./files/Logger.h.md#error)** — Log error avec stream style
- ⚙️ **[`Exists`](./files/Registry.h.md#exists)** — Vérifie si un logger existe

<a name="f"></a>

## F

- 📦 **[`false`](./files/Logger.cpp.md#false)** — Vérifie si un niveau devrait être loggé
- ⚙️ **[`Fatal`](./files/Logger.h.md#fatal)** — Log fatal avec format string
- ⚙️ **[`Fatal`](./files/Logger.h.md#fatal)** — Log fatal avec stream style
- ⚙️ **[`FileSink`](./files/FileSink.h.md#filesink)** — Constructeur avec chemin de fichier
- ⚙️ **[`Flush`](./files/Logger.h.md#flush)** — Force le flush de tous les sinks
- ⚙️ **[`Flush`](./files/Sink.h.md#flush)** — Force l'écriture des données en attente
- ⚙️ **[`Flush`](./files/AsyncSink.h.md#flush)** — Force le flush des messages en attente
- ⚙️ **[`Flush`](./files/ConsoleSink.h.md#flush)** — Force l'écriture des données en attente
- ⚙️ **[`Flush`](./files/DistributingSink.h.md#flush)** — Force le flush de tous les sous-sinks
- ⚙️ **[`Flush`](./files/FileSink.h.md#flush)** — Force l'écriture des données en attente
- ⚙️ **[`Flush`](./files/NullSink.h.md#flush)** — No-op
- ⚙️ **[`FlushAll`](./files/Registry.h.md#flushall)** — Force le flush de tous les loggers
- ⚙️ **[`for`](./files/Formatter.cpp.md#for)** — Formate un message de log avec des couleurs
- ⚙️ **[`Format`](./files/Formatter.cpp.md#format)** — Obtient le pattern courant
- ⚙️ **[`Format`](./files/Formatter.cpp.md#format)** — Formate un message de log
- ⚙️ **[`Format`](./files/Formatter.h.md#format)** — Formate un message de log
- ⚙️ **[`Format`](./files/Formatter.h.md#format)** — Formate un message de log avec des couleurs
- 📦 **[`formatted`](./files/ConsoleSink.cpp.md#formatted)** — Logge un message dans la console
- ⚙️ **[`Formatter`](./files/Formatter.h.md#formatter)** — Constructeur par défaut (pattern par défaut)
- ⚙️ **[`Formatter`](./files/Formatter.h.md#formatter)** — Constructeur avec pattern spécifique

<a name="g"></a>

## G

- ⚙️ **[`Get`](./files/Registry.h.md#get)** — Obtient un logger par son nom
- ⚙️ **[`GetDefaultLogger`](./files/Registry.cpp.md#getdefaultlogger)** — Obtient le logger par défaut
- ⚙️ **[`GetFilename`](./files/FileSink.h.md#getfilename)** — Obtient le nom du fichier
- ⚙️ **[`GetFileSize`](./files/FileSink.h.md#getfilesize)** — Obtient la taille actuelle du fichier
- ⚙️ **[`GetFormatter`](./files/Sink.h.md#getformatter)** — Obtient le formatter courant
- ⚙️ **[`GetFormatter`](./files/ConsoleSink.h.md#getformatter)** — Obtient le formatter courant
- ⚙️ **[`GetFormatter`](./files/DistributingSink.h.md#getformatter)** — Obtient le formatter (du premier sink)
- ⚙️ **[`GetFormatter`](./files/FileSink.h.md#getformatter)** — Obtient le formatter courant
- ⚙️ **[`GetFormatter`](./files/NullSink.h.md#getformatter)** — Retourne nullptr
- ⚙️ **[`GetGlobalLevel`](./files/Registry.h.md#getgloballevel)** — Obtient le niveau de log global
- ⚙️ **[`GetGlobalPattern`](./files/Registry.h.md#getglobalpattern)** — Obtient le pattern global
- ⚙️ **[`GetLevel`](./files/Logger.h.md#getlevel)** — Obtient le niveau de log courant
- ⚙️ **[`GetLevel`](./files/Sink.h.md#getlevel)** — Obtient le niveau minimum de log
- ⚙️ **[`GetLocalTime`](./files/LogMessage.h.md#getlocaltime)** — Obtient l'heure sous forme de structure tm
- ⚙️ **[`GetLogger`](./files/Registry.cpp.md#getlogger)** — Obtient un logger par son nom
- ⚙️ **[`GetLoggerCount`](./files/Registry.h.md#getloggercount)** — Obtient le nombre de loggers enregistrés
- ⚙️ **[`GetLoggerNames`](./files/Registry.h.md#getloggernames)** — Obtient la liste de tous les noms de loggers
- ⚙️ **[`GetMaxQueueSize`](./files/AsyncSink.h.md#getmaxqueuesize)** — Obtient la taille maximum de la file
- ⚙️ **[`GetMaxSize`](./files/RotatingFileSink.h.md#getmaxsize)** — Obtient la taille maximum des fichiers
- ⚙️ **[`GetMicros`](./files/LogMessage.h.md#getmicros)** — Obtient le timestamp en microsecondes
- ⚙️ **[`GetMillis`](./files/LogMessage.h.md#getmillis)** — Obtient le timestamp en millisecondes
- ⚙️ **[`GetName`](./files/Logger.h.md#getname)** — Obtient le nom du logger
- ⚙️ **[`GetOrCreate`](./files/Registry.h.md#getorcreate)** — Obtient un logger par son nom (crée si non existant)
- ⚙️ **[`GetPattern`](./files/Formatter.h.md#getpattern)** — Obtient le pattern courant
- ⚙️ **[`GetPattern`](./files/Sink.h.md#getpattern)** — Obtient le pattern courant
- ⚙️ **[`GetPattern`](./files/ConsoleSink.h.md#getpattern)** — Obtient le pattern courant
- ⚙️ **[`GetPattern`](./files/DistributingSink.h.md#getpattern)** — Obtient le pattern (du premier sink)
- ⚙️ **[`GetPattern`](./files/FileSink.h.md#getpattern)** — Obtient le pattern courant
- ⚙️ **[`GetPattern`](./files/NullSink.h.md#getpattern)** — Retourne une chaîne vide
- ⚙️ **[`GetQueueSize`](./files/AsyncSink.h.md#getqueuesize)** — Obtient la taille actuelle de la file d'attente
- ⚙️ **[`GetRotationHour`](./files/DailyFileSink.h.md#getrotationhour)** — Obtient l'heure de rotation
- ⚙️ **[`GetRotationMinute`](./files/DailyFileSink.h.md#getrotationminute)** — Obtient la minute de rotation
- ⚙️ **[`GetSeconds`](./files/LogMessage.h.md#getseconds)** — Obtient le timestamp en secondes
- ⚙️ **[`GetSinkCount`](./files/Logger.h.md#getsinkcount)** — Obtient le nombre de sinks attachés
- ⚙️ **[`GetSinkCount`](./files/DistributingSink.h.md#getsinkcount)** — Obtient la liste des sous-sinks
- ⚙️ **[`GetStream`](./files/ConsoleSink.h.md#getstream)** — Obtient le flux de console courant
- ⚙️ **[`GetUTCTime`](./files/LogMessage.h.md#getutctime)** — Obtient l'heure sous forme de structure tm (UTC)
- ⚙️ **[`gmtime_s`](./files/LogMessage.cpp.md#gmtime_s)** — Obtient l'heure sous forme de structure tm (UTC)

<a name="h"></a>

## H

- 📦 **[`hConsole`](./files/ConsoleSink.cpp.md#hconsole)** — Obtient le code de réinitialisation de couleur

<a name="i"></a>

## I

- 📦 **[`i`](./files/Formatter.cpp.md#i)** — Parse le pattern en tokens
- 📦 **[`i`](./files/RotatingFileSink.cpp.md#i)** — Effectue la rotation des fichiers
- ⚙️ **[`Info`](./files/Logger.h.md#info)** — Log info avec format string
- ⚙️ **[`Info`](./files/Logger.h.md#info)** — Log info avec stream style
- ⚙️ **[`Initialize`](./files/Log.h.md#initialize)** — Initialise le logger par défaut
- ⚙️ **[`Initialize`](./files/Registry.h.md#initialize)** — Initialise le registre avec des paramètres par défaut
- 📦 **[`instance`](./files/Log.cpp.md#instance)** — Destructeur
- 📦 **[`instance`](./files/Log.cpp.md#instance)** — Obtient l'instance singleton
- 📦 **[`instance`](./files/Registry.cpp.md#instance)** — Obtient l'instance singleton du registre
- 📦 **[`instance`](./files/Registry.cpp.md#instance)** — Nettoie le registre (détruit tous les loggers)
- ⚙️ **[`Instance`](./files/Registry.h.md#instance)** — Obtient l'instance singleton du registre
- ⚙️ **[`IsColorEnabled`](./files/ConsoleSink.h.md#iscolorenabled)** — Vérifie si les couleurs sont activées
- ⚙️ **[`IsEnabled`](./files/Sink.h.md#isenabled)** — Vérifie si le sink est activé
- ⚙️ **[`IsOpen`](./files/FileSink.h.md#isopen)** — Vérifie si le fichier est ouvert
- ⚙️ **[`IsRunning`](./files/AsyncSink.h.md#isrunning)** — Vérifie si le logger est en cours d'exécution
- ⚙️ **[`IsValid`](./files/LogMessage.h.md#isvalid)** — Vérifie si le message est valide
- 📦 **[`it`](./files/Registry.cpp.md#it)** — Obtient un logger par son nom (crée si non existant)

<a name="l"></a>

## L

- ⚙️ **[`localtime_s`](./files/LogMessage.cpp.md#localtime_s)** — Obtient l'heure sous forme de structure tm
- ⚙️ **[`localtime_s`](./files/DailyFileSink.cpp.md#localtime_s)** — Constructeur avec configuration
- ⚙️ **[`lock`](./files/Logger.cpp.md#lock)** — Destructeur du logger
- ⚙️ **[`lock`](./files/Logger.cpp.md#lock)** — Ajoute un sink au logger
- ⚙️ **[`lock`](./files/Logger.cpp.md#lock)** — Supprime tous les sinks du logger
- ⚙️ **[`lock`](./files/Logger.cpp.md#lock)** — Obtient le nombre de sinks attachés
- ⚙️ **[`lock`](./files/Logger.cpp.md#lock)** — Définit le pattern de formatage
- ⚙️ **[`lock`](./files/Logger.cpp.md#lock)** — Obtient le niveau de log courant
- ⚙️ **[`lock`](./files/Logger.cpp.md#lock)** — Log fatal avec stream style
- ⚙️ **[`lock`](./files/Logger.cpp.md#lock)** — Force le flush de tous les sinks
- ⚙️ **[`lock`](./files/Logger.cpp.md#lock)** — Active ou désactive le logger
- ⚙️ **[`lock`](./files/Registry.cpp.md#lock)** — Initialise le registre avec des paramètres par défaut
- ⚙️ **[`lock`](./files/Registry.cpp.md#lock)** — Désenregistre un logger du registre
- ⚙️ **[`lock`](./files/Registry.cpp.md#lock)** — Obtient un logger par son nom
- ⚙️ **[`lock`](./files/Registry.cpp.md#lock)** — Supprime tous les loggers du registre
- ⚙️ **[`lock`](./files/Registry.cpp.md#lock)** — Obtient la liste de tous les noms de loggers
- ⚙️ **[`lock`](./files/Registry.cpp.md#lock)** — Définit le niveau de log global
- ⚙️ **[`lock`](./files/Registry.cpp.md#lock)** — Définit le pattern global
- ⚙️ **[`lock`](./files/Registry.cpp.md#lock)** — Obtient le pattern global
- ⚙️ **[`lock`](./files/Registry.cpp.md#lock)** — Force le flush de tous les loggers
- ⚙️ **[`lock`](./files/Registry.cpp.md#lock)** — Définit le logger par défaut
- ⚙️ **[`lock`](./files/Registry.cpp.md#lock)** — Obtient le logger par défaut
- ⚙️ **[`lock`](./files/AsyncSink.cpp.md#lock)** — Vérifie si le logger est en cours d'exécution
- ⚙️ **[`lock`](./files/AsyncSink.cpp.md#lock)** — Obtient l'intervalle de flush
- ⚙️ **[`lock`](./files/AsyncSink.cpp.md#lock)** — Ajoute un message à la file
- ⚙️ **[`lock`](./files/AsyncSink.cpp.md#lock)** — Traite un message de la file
- ⚙️ **[`lock`](./files/AsyncSink.cpp.md#lock)** — Vide toute la file d'attente
- ⚙️ **[`lock`](./files/ConsoleSink.cpp.md#lock)** — Force l'écriture des données en attente
- ⚙️ **[`lock`](./files/ConsoleSink.cpp.md#lock)** — Définit le formatter pour ce sink
- ⚙️ **[`lock`](./files/ConsoleSink.cpp.md#lock)** — Définit le pattern de formatage
- ⚙️ **[`lock`](./files/ConsoleSink.cpp.md#lock)** — Obtient le formatter courant
- ⚙️ **[`lock`](./files/ConsoleSink.cpp.md#lock)** — Obtient le pattern courant
- ⚙️ **[`lock`](./files/ConsoleSink.cpp.md#lock)** — Vérifie si le sink utilise stderr pour les erreurs
- ⚙️ **[`lock`](./files/DailyFileSink.cpp.md#lock)** — Obtient l'heure de rotation
- ⚙️ **[`lock`](./files/DailyFileSink.cpp.md#lock)** — Obtient le nombre maximum de jours à conserver
- ⚙️ **[`lock`](./files/DistributingSink.cpp.md#lock)** — Destructeur
- ⚙️ **[`lock`](./files/DistributingSink.cpp.md#lock)** — Distribue un message à tous les sous-sinks
- ⚙️ **[`lock`](./files/DistributingSink.cpp.md#lock)** — Force le flush de tous les sous-sinks
- ⚙️ **[`lock`](./files/DistributingSink.cpp.md#lock)** — Définit le formatter pour tous les sous-sinks
- ⚙️ **[`lock`](./files/DistributingSink.cpp.md#lock)** — Définit le pattern de formatage pour tous les sous-sinks
- ⚙️ **[`lock`](./files/DistributingSink.cpp.md#lock)** — Obtient le formatter (du premier sink)
- ⚙️ **[`lock`](./files/DistributingSink.cpp.md#lock)** — Obtient le pattern (du premier sink)
- ⚙️ **[`lock`](./files/DistributingSink.cpp.md#lock)** — Ajoute un sous-sink
- ⚙️ **[`lock`](./files/DistributingSink.cpp.md#lock)** — Supprime un sous-sink
- ⚙️ **[`lock`](./files/DistributingSink.cpp.md#lock)** — Supprime tous les sous-sinks
- ⚙️ **[`lock`](./files/DistributingSink.cpp.md#lock)** — Obtient la liste des sous-sinks
- ⚙️ **[`lock`](./files/DistributingSink.cpp.md#lock)** — Vérifie si un sink spécifique est présent
- ⚙️ **[`lock`](./files/FileSink.cpp.md#lock)** — Logge un message dans le fichier
- ⚙️ **[`lock`](./files/FileSink.cpp.md#lock)** — Force l'écriture des données en attente
- ⚙️ **[`lock`](./files/FileSink.cpp.md#lock)** — Définit le formatter pour ce sink
- ⚙️ **[`lock`](./files/FileSink.cpp.md#lock)** — Définit le pattern de formatage
- ⚙️ **[`lock`](./files/FileSink.cpp.md#lock)** — Obtient le formatter courant
- ⚙️ **[`lock`](./files/FileSink.cpp.md#lock)** — Obtient le pattern courant
- ⚙️ **[`lock`](./files/FileSink.cpp.md#lock)** — Ouvre le fichier
- ⚙️ **[`lock`](./files/FileSink.cpp.md#lock)** — Ferme le fichier
- ⚙️ **[`lock`](./files/FileSink.cpp.md#lock)** — Vérifie si le fichier est ouvert
- ⚙️ **[`lock`](./files/FileSink.cpp.md#lock)** — Obtient le nom du fichier
- ⚙️ **[`lock`](./files/FileSink.cpp.md#lock)** — Obtient la taille actuelle du fichier
- ⚙️ **[`lock`](./files/FileSink.cpp.md#lock)** — Définit le mode d'ouverture
- ⚙️ **[`lock`](./files/FileSink.cpp.md#lock)** — Obtient le mode d'ouverture
- ⚙️ **[`lock`](./files/RotatingFileSink.cpp.md#lock)** — Obtient le nombre maximum de fichiers
- ⚙️ **[`Log`](./files/Logger.h.md#log)** — Log avec format string (style printf)
- ⚙️ **[`Log`](./files/Logger.h.md#log)** — Log avec format string et informations de source
- ⚙️ **[`Log`](./files/Logger.h.md#log)** — Log avec message string et informations de source
- ⚙️ **[`Log`](./files/Logger.h.md#log)** — Log avec format string, informations de source et va_list
- ⚙️ **[`Log`](./files/Logger.h.md#log)** — Log avec stream style
- ⚙️ **[`Log`](./files/AsyncSink.h.md#log)** — Destructeur
- ⚙️ **[`Log`](./files/AsyncSink.h.md#log)** — Log asynchrone
- ⚙️ **[`Log`](./files/ConsoleSink.h.md#log)** — Destructeur
- ⚙️ **[`Log`](./files/ConsoleSink.h.md#log)** — Logge un message dans la console
- ⚙️ **[`Log`](./files/DailyFileSink.h.md#log)** — Constructeur avec configuration
- ⚙️ **[`Log`](./files/DailyFileSink.h.md#log)** — Destructeur
- ⚙️ **[`Log`](./files/DailyFileSink.h.md#log)** — Logge un message avec vérification de rotation quotidienne
- ⚙️ **[`Log`](./files/DistributingSink.h.md#log)** — Destructeur
- ⚙️ **[`Log`](./files/DistributingSink.h.md#log)** — Distribue un message à tous les sous-sinks
- ⚙️ **[`Log`](./files/FileSink.h.md#log)** — Destructeur
- ⚙️ **[`Log`](./files/FileSink.h.md#log)** — Logge un message dans le fichier
- ⚙️ **[`Log`](./files/NullSink.h.md#log)** — Constructeur par défaut
- ⚙️ **[`Log`](./files/NullSink.h.md#log)** — Destructeur
- ⚙️ **[`Log`](./files/NullSink.h.md#log)** — Ignore le message (no-op)
- ⚙️ **[`Log`](./files/RotatingFileSink.h.md#log)** — Constructeur avec configuration de rotation
- ⚙️ **[`Log`](./files/RotatingFileSink.h.md#log)** — Destructeur
- ⚙️ **[`Log`](./files/RotatingFileSink.h.md#log)** — Logge un message avec vérification de rotation
- ⚙️ **[`Logger`](./files/Logger.h.md#logger)** — Constructeur de logger avec nom
- ⚙️ **[`LogLevelToANSIColor`](./files/Formatter.cpp.md#logleveltoansicolor)** — Formate un nombre avec padding
- ⚙️ **[`LogLevelToANSIColor`](./files/Formatter.cpp.md#logleveltoansicolor)** — Obtient le code couleur ANSI pour un niveau de log
- ⚙️ **[`LogLevelToANSIColor`](./files/LogLevel.cpp.md#logleveltoansicolor)** — Obtient la couleur ANSI associée à un niveau de log
- ⚙️ **[`LogLevelToANSIColor`](./files/LogLevel.h.md#logleveltoansicolor)** — Obtient la couleur ANSI associée à un niveau de log
- ⚙️ **[`LogLevelToANSIColor`](./files/ConsoleSink.cpp.md#logleveltoansicolor)** — Obtient le code couleur pour un niveau de log
- ⚙️ **[`LogLevelToShortString`](./files/LogLevel.cpp.md#logleveltoshortstring)** — Convertit un LogLevel en chaîne courte (3 caractères)
- ⚙️ **[`LogLevelToShortString`](./files/LogLevel.h.md#logleveltoshortstring)** — Convertit un LogLevel en chaîne courte (3 caractères)
- ⚙️ **[`LogLevelToString`](./files/LogLevel.cpp.md#logleveltostring)** — Convertit un LogLevel en chaîne de caractères
- ⚙️ **[`LogLevelToString`](./files/LogLevel.h.md#logleveltostring)** — Convertit un LogLevel en chaîne de caractères
- ⚙️ **[`LogLevelToString`](./files/LogLevel.h.md#logleveltostring)** — Niveau fatal - messages fatals (arrêt de l'application)
- ⚙️ **[`LogLevelToString`](./files/LogLevel.h.md#logleveltostring)** — Désactivation complète du logging
- ⚙️ **[`LogLevelToWindowsColor`](./files/LogLevel.cpp.md#logleveltowindowscolor)** — Obtient la couleur Windows associée à un niveau de log
- ⚙️ **[`LogLevelToWindowsColor`](./files/LogLevel.h.md#logleveltowindowscolor)** — Obtient la couleur Windows associée à un niveau de log

<a name="m"></a>

## M

- 📦 **[`m_Enabled`](./files/Logger.cpp.md#m_enabled)** — Vérifie si le logger est actif
- 📦 **[`m_FlushInterval`](./files/AsyncSink.cpp.md#m_flushinterval)** — Définit l'intervalle de flush
- 📦 **[`m_GlobalLevel`](./files/Registry.cpp.md#m_globallevel)** — Obtient le niveau de log global
- 📦 **[`m_MaxDays`](./files/DailyFileSink.cpp.md#m_maxdays)** — Définit le nombre maximum de jours à conserver
- 📦 **[`m_MaxFiles`](./files/RotatingFileSink.cpp.md#m_maxfiles)** — Définit le nombre maximum de fichiers
- 📦 **[`m_MaxQueueSize`](./files/AsyncSink.cpp.md#m_maxqueuesize)** — Définit la taille maximum de la file
- 📦 **[`m_MaxQueueSize`](./files/AsyncSink.cpp.md#m_maxqueuesize)** — Obtient la taille maximum de la file
- 📦 **[`m_MaxSize`](./files/RotatingFileSink.cpp.md#m_maxsize)** — Définit la taille maximum des fichiers
- 📦 **[`m_MaxSize`](./files/RotatingFileSink.cpp.md#m_maxsize)** — Obtient la taille maximum des fichiers
- 📦 **[`m_RotationMinute`](./files/DailyFileSink.cpp.md#m_rotationminute)** — Obtient la minute de rotation
- 📦 **[`m_Stream`](./files/ConsoleSink.cpp.md#m_stream)** — Définit le flux de console
- 📦 **[`m_Stream`](./files/ConsoleSink.cpp.md#m_stream)** — Obtient le flux de console courant
- 📦 **[`m_UseColors`](./files/ConsoleSink.cpp.md#m_usecolors)** — Active ou désactive les couleurs
- 📦 **[`m_UseColors`](./files/ConsoleSink.cpp.md#m_usecolors)** — Vérifie si les couleurs sont activées
- 📦 **[`m_UseStderrForErrors`](./files/ConsoleSink.cpp.md#m_usestderrforerrors)** — Définit si le sink doit utiliser stderr pour les niveaux d'erreur
- 📦 **[`message`](./files/Logger.cpp.md#message)** — Log avec message string et informations de source
- 📦 **[`message`](./files/Logger.cpp.md#message)** — Log avec format string, informations de source et va_list
- 📦 **[`msg`](./files/Logger.cpp.md#msg)** — Log interne avec informations de source
- 📦 **[`msg`](./files/AsyncSink.cpp.md#msg)** — Log asynchrone avec message pré-formaté
- 📦 **[`msg`](./files/AsyncSink.cpp.md#msg)** — Fonction du thread de traitement

<a name="n"></a>

## N

- 📦 **[`name`](./files/Log.h.md#name)** — Obtient l'instance singleton du logger par défaut
- ⚙️ **[`Named`](./files/Log.h.md#named)** — Configure les informations de source pour le prochain log
- 📦 **[`now`](./files/DailyFileSink.cpp.md#now)** — Vérifie et effectue la rotation si nécessaire

<a name="o"></a>

## O

- ⚙️ **[`Open`](./files/FileSink.h.md#open)** — Ouvre le fichier (si non ouvert)

<a name="p"></a>

## P

- ⚙️ **[`path`](./files/FileSink.cpp.md#path)** — Constructeur avec chemin de fichier

<a name="q"></a>

## Q

- 📦 **[`queueSize`](./files/AsyncSink.h.md#queuesize)** — Constructeur avec nom et configuration

<a name="r"></a>

## R

- ⚙️ **[`Register`](./files/Registry.h.md#register)** — Enregistre un logger dans le registre
- ⚙️ **[`RemoveSink`](./files/DistributingSink.h.md#removesink)** — Supprime un sous-sink
- ⚙️ **[`Reset`](./files/LogMessage.h.md#reset)** — Réinitialise le message
- 📦 **[`rotatedFile`](./files/DailyFileSink.cpp.md#rotatedfile)** — Effectue la rotation quotidienne

<a name="s"></a>

## S

- ⚙️ **[`SetColorEnabled`](./files/ConsoleSink.h.md#setcolorenabled)** — Active ou désactive les couleurs
- ⚙️ **[`SetDefaultLogger`](./files/Registry.h.md#setdefaultlogger)** — Définit le logger par défaut
- ⚙️ **[`SetEnabled`](./files/Sink.h.md#setenabled)** — Active ou désactive le sink
- ⚙️ **[`SetFilename`](./files/FileSink.h.md#setfilename)** — Définit un nouveau nom de fichier
- ⚙️ **[`SetFormatter`](./files/Logger.h.md#setformatter)** — Définit le formatter pour tous les sinks
- ⚙️ **[`SetFormatter`](./files/Sink.h.md#setformatter)** — Définit le formatter pour ce sink
- ⚙️ **[`SetFormatter`](./files/ConsoleSink.h.md#setformatter)** — Définit le formatter pour ce sink
- ⚙️ **[`SetFormatter`](./files/DistributingSink.h.md#setformatter)** — Définit le formatter pour tous les sous-sinks
- ⚙️ **[`SetFormatter`](./files/FileSink.h.md#setformatter)** — Définit le formatter pour ce sink
- ⚙️ **[`SetFormatter`](./files/NullSink.h.md#setformatter)** — No-op
- ⚙️ **[`SetGlobalLevel`](./files/Registry.h.md#setgloballevel)** — Définit le niveau de log global
- ⚙️ **[`SetGlobalPattern`](./files/Registry.h.md#setglobalpattern)** — Définit le pattern global
- ⚙️ **[`SetLevel`](./files/Logger.h.md#setlevel)** — Définit le niveau de log minimum
- ⚙️ **[`SetLevel`](./files/Sink.h.md#setlevel)** — Définit le niveau minimum de log pour ce sink
- ⚙️ **[`SetMaxDays`](./files/DailyFileSink.h.md#setmaxdays)** — Définit le nombre maximum de jours à conserver
- ⚙️ **[`SetMaxFiles`](./files/RotatingFileSink.h.md#setmaxfiles)** — Définit le nombre maximum de fichiers
- ⚙️ **[`SetMaxQueueSize`](./files/AsyncSink.h.md#setmaxqueuesize)** — Définit la taille maximum de la file
- ⚙️ **[`SetMaxSize`](./files/RotatingFileSink.h.md#setmaxsize)** — Définit la taille maximum des fichiers
- ⚙️ **[`SetPattern`](./files/Formatter.h.md#setpattern)** — Destructeur
- ⚙️ **[`SetPattern`](./files/Formatter.h.md#setpattern)** — Définit le pattern de formatage
- ⚙️ **[`SetPattern`](./files/Logger.h.md#setpattern)** — Définit le pattern de formatage
- ⚙️ **[`SetPattern`](./files/Sink.h.md#setpattern)** — Définit le pattern de formatage
- ⚙️ **[`SetPattern`](./files/ConsoleSink.h.md#setpattern)** — Définit le pattern de formatage
- ⚙️ **[`SetPattern`](./files/DistributingSink.h.md#setpattern)** — Définit le pattern de formatage pour tous les sous-sinks
- ⚙️ **[`SetPattern`](./files/FileSink.h.md#setpattern)** — Définit le pattern de formatage
- ⚙️ **[`SetPattern`](./files/NullSink.h.md#setpattern)** — No-op
- ⚙️ **[`SetRotationTime`](./files/DailyFileSink.h.md#setrotationtime)** — Définit l'heure de rotation
- ⚙️ **[`SetStream`](./files/ConsoleSink.h.md#setstream)** — Définit le flux de console
- 📦 **[`SHORT_PATTERN`](./files/Pattern.h.md#short_pattern)** — Pattern court (pour logs de production) Format: 12:34:56 INF Message
- ⚙️ **[`ShortStringToLogLevel`](./files/LogLevel.h.md#shortstringtologlevel)** — Convertit une chaîne courte en LogLevel
- ⚙️ **[`ShouldLog`](./files/Logger.h.md#shouldlog)** — Vérifie si un niveau devrait être loggé
- ⚙️ **[`ShouldLog`](./files/Sink.h.md#shouldlog)** — Vérifie si un niveau devrait être loggé
- ⚙️ **[`Shutdown`](./files/Log.h.md#shutdown)** — Nettoie et détruit le logger par défaut
- ⚙️ **[`Shutdown`](./files/Registry.h.md#shutdown)** — Nettoie le registre (détruit tous les loggers)
- 📦 **[`sourceFile`](./files/Log.h.md#sourcefile)** — Configure le nom du logger (retourne *this pour chaînage)
- 📦 **[`sourceFile`](./files/Logger.h.md#sourcefile)** — Vérifie si le logger est actif
- ⚙️ **[`Start`](./files/AsyncSink.h.md#start)** — Démarre le thread de traitement
- ⚙️ **[`Stop`](./files/AsyncSink.h.md#stop)** — Arrête le thread de traitement
- ⚙️ **[`StringToLogLevel`](./files/LogLevel.h.md#stringtologlevel)** — Convertit une chaîne en LogLevel

<a name="t"></a>

## T

- 📦 **[`threadId`](./files/LogMessage.h.md#threadid)** — ID du thread émetteur
- 📦 **[`threadName`](./files/LogMessage.h.md#threadname)** — Nom du thread (optionnel)
- 📦 **[`timePoint`](./files/LogMessage.h.md#timepoint)** — Heure de réception du message
- 📦 **[`timestamp`](./files/LogMessage.h.md#timestamp)** — Timestamp en nanosecondes depuis l'epoch
- 📦 **[`tm`](./files/Formatter.cpp.md#tm)** — Formate un token individuel
- ⚙️ **[`Trace`](./files/Logger.h.md#trace)** — Log trace avec format string
- ⚙️ **[`Trace`](./files/Logger.h.md#trace)** — Log trace avec stream style
- 📦 **[`true`](./files/RotatingFileSink.cpp.md#true)** — Force la rotation du fichier

<a name="u"></a>

## U

- ⚙️ **[`Unregister`](./files/Registry.h.md#unregister)** — Désenregistre un logger du registre

<a name="w"></a>

## W

- ⚙️ **[`Warn`](./files/Logger.h.md#warn)** — Log warning avec format string
- ⚙️ **[`Warn`](./files/Logger.h.md#warn)** — Log warning avec stream style

