// =============================================================================
// NKLogger/NkLog.cpp
// Implémentation du logger singleton par défaut avec API fluide.
//
// Design :
//  - Inclusion de pch.h en premier pour compilation précompilée
//  - Aucune macro NKENTSEU_LOGGER_API sur les méthodes (héritée de la classe)
//  - Initialisation lazy avec defaults raisonnables si Initialize() non appelée
//  - Sinks console et fichier configurés par défaut pour usage immédiat
//  - Synchronisation thread-safe via NKThreading/NkMutex (hérité de NkLogger)
//  - Namespace unique : nkentseu (pas de sous-namespace logger)
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#include "pch.h"

#include "NKLogger/NkLog.h"
#include "NKLogger/NkLogLevel.h"
#include "NKLogger/NkLoggerFormatter.h"

// Sinks concrets pour configuration par défaut
#include "NKLogger/Sinks/NkConsoleSink.h"
#include "NKLogger/Sinks/NkFileSink.h"


// -------------------------------------------------------------------------
// SECTION 1 : NAMESPACE PRINCIPAL - IMPLÉMENTATIONS
// -------------------------------------------------------------------------
// Implémentation des méthodes statiques et membres de NkLog.
// Aucune macro NKENTSEU_LOGGER_API : export hérité de la déclaration de classe.

namespace nkentseu {


	// -------------------------------------------------------------------------
	// VARIABLE STATIQUE : s_Initialized
	// DESCRIPTION : Indicateur d'initialisation explicite via Initialize()
	// -------------------------------------------------------------------------
	bool NkLog::s_Initialized = false;


	// -------------------------------------------------------------------------
	// MÉTHODE : Constructeur privé
	// DESCRIPTION : Initialisation avec configuration par défaut et sinks
	// -------------------------------------------------------------------------
	NkLog::NkLog(const NkString& name)
		: NkLogger(name) {

		// Configuration par défaut : console + fichier pour usage immédiat
        
		// Sink console : sortie vers stdout/stderr avec support couleurs
		// Sur Android : NkConsoleSink route automatiquement vers logcat
		NkConsoleSink* consoleSinkRaw = new NkConsoleSink();
		consoleSinkRaw->SetColorEnabled(true);  // Activer les couleurs ANSI si supporté
		consoleSinkRaw->SetLevel(NkLogLevel::NK_DEBUG);  // Verbose par défaut en console
        memory::NkSharedPtr<NkISink> consoleSink(consoleSinkRaw);
		AddSink(consoleSink);

		// Sink fichier : persistance des logs dans logs/app.log
		memory::NkSharedPtr<NkISink> fileSink(new NkFileSink("logs/app.log"));
		fileSink->SetLevel(NkLogLevel::NK_INFO);  // Moins verbose en fichier pour production
		fileSink->SetPattern(NkLoggerFormatter::NK_DEFAULT_PATTERN);  // Pattern lisible
		AddSink(fileSink);

		// Configuration globale du logger
		SetLevel(NkLogLevel::NK_INFO);  // Niveau par défaut : info et plus grave
		SetPattern(NkLoggerFormatter::NK_NKENTSEU_PATTERN);  // Pattern avec support couleurs
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Destructeur privé
	// DESCRIPTION : Flush garanti avant destruction du singleton
	// -------------------------------------------------------------------------
	NkLog::~NkLog() {
		// Flush explicite : garantir la persistance des logs en buffer
		Flush();

		// ClearSinks() appelé automatiquement par ~NkLogger() de la base
		// Pas besoin d'appel explicite ici
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Instance (static)
	// DESCRIPTION : Retourne l'instance singleton via Meyer's singleton
	// -------------------------------------------------------------------------
	NkLog& NkLog::Instance() {
		// Meyer's singleton : static local, thread-safe en C++11+
		// L'instance est créée au premier appel, détruite à la fin du programme
		static NkLog instance;
		return instance;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Initialize (static)
	// DESCRIPTION : Configure le logger singleton avec paramètres personnalisés
	// -------------------------------------------------------------------------
	void NkLog::Initialize(
		const NkString& name,
		const NkString& pattern,
		NkLogLevel level
	) {
		// Accès à l'instance singleton (création lazy si premier appel)
		NkLog& instance = Instance();

		// Mise à jour du nom si différent et non vide
		if (!name.Empty() && instance.GetName() != name) {
			instance.SetName(name);
		}

		// Application du pattern et du niveau de log
		instance.SetPattern(pattern);
		instance.SetLevel(level);

		// Marquer comme initialisé explicitement
		s_Initialized = true;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Shutdown (static)
	// DESCRIPTION : Flush et cleanup explicite avant terminaison
	// -------------------------------------------------------------------------
	void NkLog::Shutdown() {
		// Accès à l'instance singleton
		NkLog& instance = Instance();

		// Flush explicite : garantir la persistance des logs en buffer
		instance.Flush();

		// ClearSinks : libération des références partagées vers les sinks
		// Note : les sinks partagés peuvent être utilisés ailleurs, donc pas de destruction
		instance.ClearSinks();
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Named (API fluide)
	// DESCRIPTION : Définit le nom et retourne *this pour chaînage
	// -------------------------------------------------------------------------
	NkLog& NkLog::Named(const NkString& name) {
		// Délégation à SetName() protégé de NkLogger
		SetName(name);
		return *this;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Level (API fluide)
	// DESCRIPTION : Définit le niveau de log et retourne *this pour chaînage
	// -------------------------------------------------------------------------
	NkLog& NkLog::Level(NkLogLevel level) {
		// Délégation à SetLevel() de NkLogger
		SetLevel(level);
		return *this;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Pattern (API fluide)
	// DESCRIPTION : Définit le pattern et retourne *this pour chaînage
	// -------------------------------------------------------------------------
	NkLog& NkLog::Pattern(const NkString& pattern) {
		// Délégation à SetPattern() de NkLogger
		SetPattern(pattern);
		return *this;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Source (override fluide)
	// DESCRIPTION : Configure les métadonnées de source et retourne *this
	// -------------------------------------------------------------------------
	NkLog& NkLog::Source(const char* sourceFile, uint32 sourceLine, const char* functionName) {
		// Appel à la méthode de base pour configuration des métadonnées
		NkLogger::Source(sourceFile, sourceLine, functionName);
		return *this;
	}


} // namespace nkentseu


// =============================================================================
// NOTES D'IMPLÉMENTATION ET BONNES PRATIQUES
// =============================================================================
/*
	1. SINGLETON MEYER'S :
	   - static local dans Instance() : thread-safe en C++11+ sans mutex explicite
	   - Destruction à la fin du programme : ordre indéterminé, éviter les dépendances
	   - Pour contrôle de l'ordre de destruction : utiliser Shutdown() explicitement

	2. CONFIGURATION PAR DÉFAUT :
	   - Console + fichier : couverture des cas d'usage courants (dev + prod)
	   - Niveaux différents : DEBUG en console, INFO en fichier pour équilibre
	   - Patterns adaptés : couleurs en console, lisible en fichier

	3. THREAD-SAFETY HÉRITÉE :
	   - Toutes les méthodes de NkLogger sont thread-safe via m_Mutex
	   - NkLog ne rajoute pas de synchronisation : réutilisation de l'infrastructure
	   - Les sinks partagés doivent être thread-safe individuellement

	4. GESTION DES SINKS PAR DÉFAUT :
	   - NkConsoleSink : route vers stdout/stderr, couleurs ANSI si supporté
	   - NkFileSink : écriture dans logs/app.log, rotation à implémenter si nécessaire
	   - Pour personnalisation : ClearSinks() puis AddSink() avec sinks custom

	5. INITIALISATION LAZY VS EXPLICITE :
	   - Lazy : Instance() crée le logger au premier usage avec defaults
	   - Explicite : Initialize() permet de configurer avant tout logging
	   - Recommandation : appeler Initialize() au startup pour contrôle total

	6. EXTENSIBILITÉ :
	   - Pour ajouter des sinks par défaut : modifier le constructeur NkLog()
	   - Pour changer les defaults : modifier les valeurs dans Initialize()
	   - Pour features spécifiques : ajouter des méthodes dans NkLog uniquement

	7. COMPATIBILITÉ MULTIPLATEFORME :
	   - NkConsoleSink gère automatiquement stdout/stderr vs logcat sur Android
	   - NkFileSink utilise fopen/fwrite portable, avec fallback en cas d'erreur
	   - Les chemins de fichiers : utiliser NKCore/NkPath pour normalisation cross-platform
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================