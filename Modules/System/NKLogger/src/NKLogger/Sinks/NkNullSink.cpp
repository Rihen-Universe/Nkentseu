// =============================================================================
// NKLogger/Sinks/NkNullSink.cpp
// Implémentation du sink nul (no-op) pour désactivation ou testing.
//
// Design :
//  - Inclusion de pch.h en premier pour compilation précompilée
//  - Aucune macro NKENTSEU_LOGGER_API sur les méthodes (héritée de la classe)
//  - Toutes les méthodes sont des no-op : aucun traitement, aucune allocation
//  - Thread-safe par conception : aucun état mutable, aucune synchronisation
//  - Namespace unique : nkentseu (pas de sous-namespace logger)
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#include "pch.h"

#include "NKLogger/Sinks/NkNullSink.h"
#include "NKLogger/NkLogMessage.h"
#include "NKLogger/NkLoggerFormatter.h"


// -------------------------------------------------------------------------
// SECTION 1 : NAMESPACE PRINCIPAL - IMPLÉMENTATIONS DES MÉTHODES
// -------------------------------------------------------------------------
// Implémentation des méthodes publiques de NkNullSink.
// Aucune macro NKENTSEU_LOGGER_API : export hérité de la déclaration de classe.

namespace nkentseu {


	// -------------------------------------------------------------------------
	// MÉTHODE : Constructeur
	// DESCRIPTION : Initialisation sans état, sans allocation
	// -------------------------------------------------------------------------
	NkNullSink::NkNullSink() {
		// Aucun état à initialiser : objet prêt à l'emploi immédiat
		// noexcept implicite : aucune exception possible
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Destructeur
	// DESCRIPTION : Cleanup trivial : aucune ressource à libérer
	// -------------------------------------------------------------------------
	NkNullSink::~NkNullSink() {
		// Aucune ressource à libérer : fichier, mutex, buffer, etc.
		// noexcept implicite : destruction safe dans tous les contextes
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Log
	// DESCRIPTION : Ignore silencieusement le message (no-op)
	// -------------------------------------------------------------------------
	void NkNullSink::Log(const NkLogMessage& message) {
		// No-op : le message est ignoré sans traitement ni allocation
		// Thread-safe : aucun état mutable, safe pour appel concurrent
		// Performance : overhead minimal, potentiellement optimisé par le compilateur

		// Note : le paramètre 'message' est intentionnellement non utilisé
		// Pour éviter les warnings compiler : (void)message; si nécessaire
		(void)message;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Flush
	// DESCRIPTION : No-op : aucun buffer à flush pour ce sink
	// -------------------------------------------------------------------------
	void NkNullSink::Flush() {
		// No-op : ce sink n'a aucun buffer interne à vider
		// Thread-safe : safe pour appel concurrent
		// Retour immédiat : aucun overhead

		// Note : méthode requise par l'interface NkISink pour compatibilité
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : SetFormatter
	// DESCRIPTION : Ignore le formatter fourni (no-op avec destruction RAII)
	// -------------------------------------------------------------------------
	void NkNullSink::SetFormatter(memory::NkUniquePtr<NkLoggerFormatter> formatter) {
		// No-op : ce sink n'utilise pas de formatter interne
		// Le formatter passé est détruit automatiquement via RAII de NkUniquePtr
		// Thread-safe : safe pour appel concurrent

		// Note : le paramètre 'formatter' est intentionnellement non utilisé
		// La destruction RAII se produit à la sortie de scope
		(void)formatter;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : SetPattern
	// DESCRIPTION : Ignore le pattern fourni (no-op)
	// -------------------------------------------------------------------------
	void NkNullSink::SetPattern(const NkString& pattern) {
		// No-op : ce sink n'utilise pas de pattern interne
		// Thread-safe : safe pour appel concurrent
		// Aucun état modifié : retour immédiat

		// Note : le paramètre 'pattern' est intentionnellement non utilisé
		(void)pattern;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetFormatter
	// DESCRIPTION : Retourne nullptr : aucun formatter associé
	// -------------------------------------------------------------------------
	NkLoggerFormatter* NkNullSink::GetFormatter() const {
		// Retour constant : ce sink ne gère pas de formatter interne
		// Thread-safe : lecture safe sans synchronisation
		// Compatible avec l'API NkISink : retour de pointeur brut

		return nullptr;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetPattern
	// DESCRIPTION : Retourne une chaîne vide : aucun pattern associé
	// -------------------------------------------------------------------------
	NkString NkNullSink::GetPattern() const {
		// Retour constant : ce sink ne gère pas de pattern interne
		// Thread-safe : lecture safe sans synchronisation
		// Compatible avec l'API NkISink : retour de NkString par valeur

		return NkString{};
	}


} // namespace nkentseu


// =============================================================================
// NOTES D'IMPLÉMENTATION ET BONNES PRATIQUES
// =============================================================================
/*
	1. NO-OP OPTIMISÉ PAR LE COMPILATEUR :
	   - Toutes les méthodes sont triviales : potentiellement inline et éliminées
	   - (void)param; pour éviter les warnings "unused parameter" en mode strict
	   - Pour performance critique : vérifier l'assembly généré pour confirmation

	2. THREAD-SAFETY GARANTIE :
	   - Aucun état mutable : pas de variables membres modifiables
	   - Aucune synchronisation : pas de mutex, atomics, ou barrières mémoire
	   - Safe pour appel concurrent depuis n'importe quel thread, n'importe quand

	3. GESTION RAII DES PARAMÈTRES :
	   - SetFormatter() : le NkUniquePtr est détruit à la sortie de scope
	   - Pas de fuite mémoire : même si le sink ignore le formatter
	   - Comportement cohérent avec les autres sinks qui adoptent le formatter

	4. COMPATIBILITÉ AVEC CODE GÉNÉRIQUE :
	   - GetFormatter() retourne nullptr : code qui teste if (fmt) fonctionne
	   - GetPattern() retourne chaîne vide : code qui teste .Empty() fonctionne
	   - Aucune exception : safe dans tous les contextes, y compris noexcept

	5. UTILISATION DANS LES TESTS :
	   - NkNullSink est idéal pour les tests unitaires sans effets de bord
	   - Alternative pour vérification : créer un MockSink avec Google Mock
	   - Pour tests de performance : mesurer l'overhead réel avec chrono

	6. EXTENSIBILITÉ MINIMALE MAIS SUFFISANTE :
	   - Cette classe est conçue pour être finale : pas prévue pour héritage
	   - Pour variantes : créer de nouvelles classes plutôt que dériver
	   - Documenter clairement le contrat no-op pour éviter les mauvaises surprises
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================