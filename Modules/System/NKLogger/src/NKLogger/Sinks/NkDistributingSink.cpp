// =============================================================================
// NKLogger/Sinks/NkDistributingSink.cpp
// Implémentation du sink composite pour distribution broadcast aux sous-sinks.
//
// Design :
//  - Inclusion de pch.h en premier pour compilation précompilée
//  - Aucune macro NKENTSEU_LOGGER_API sur les méthodes (héritée de la classe)
//  - Fonctions utilitaires dans namespace anonyme si nécessaires
//  - Synchronisation thread-safe via NKThreading/NkMutex pour toutes les opérations
//  - Clonage de formatter via pattern pour indépendance des sous-sinks
//  - Namespace unique : nkentseu (pas de sous-namespace logger)
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#include "pch.h"

#include "NKLogger/Sinks/NkDistributingSink.h"
#include "NKLogger/NkLogMessage.h"
#include "NKLogger/NkLogLevel.h"
#include "NKLogger/NkLoggerFormatter.h"
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkScopedLock.h"


// -------------------------------------------------------------------------
// SECTION 1 : NAMESPACE PRINCIPAL - IMPLÉMENTATIONS DES MÉTHODES
// -------------------------------------------------------------------------
// Implémentation des méthodes publiques de NkDistributingSink.
// Aucune macro NKENTSEU_LOGGER_API : export hérité de la déclaration de classe.

namespace nkentseu {


	// -------------------------------------------------------------------------
	// MÉTHODE : Constructeur par défaut
	// DESCRIPTION : Initialisation avec collection vide de sous-sinks
	// -------------------------------------------------------------------------
	NkDistributingSink::NkDistributingSink() {
		// Collection m_Sinks initialisée vide par défaut du constructeur NkVector
		// Mutex m_Mutex initialisé automatiquement par son constructeur
		// Aucun état supplémentaire requis : objet prêt à l'emploi immédiat
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Constructeur avec liste initiale
	// DESCRIPTION : Initialisation avec copie des shared_ptr fournis
	// -------------------------------------------------------------------------
	NkDistributingSink::NkDistributingSink(
		const NkVector<memory::NkSharedPtr<NkISink>>& sinks
	) : m_Sinks(sinks) {
		// Copie des shared_ptr : incrémentation des refcount des sinks fournis
		// Ownership partagé : les sinks peuvent être utilisés ailleurs simultanément
		// Thread-safe : construction sans verrouillage requis (premier accès)
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Destructeur
	// DESCRIPTION : Libération des références partagées vers les sous-sinks
	// -------------------------------------------------------------------------
	NkDistributingSink::~NkDistributingSink() {
		// Destructeur par défaut : m_Sinks vidé, refcount décrémentés automatiquement
		// Les sinks partagés ne sont détruits que si plus aucun propriétaire
		// Mutex m_Mutex détruit automatiquement après vidage de m_Sinks
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Log
	// DESCRIPTION : Distribution broadcast du message à tous les sous-sinks valides
	// -------------------------------------------------------------------------
	void NkDistributingSink::Log(const NkLogMessage& message) {
		// Filtrage précoce : éviter toute distribution si message ignoré globalement
		if (!IsEnabled() || !ShouldLog(message.level)) {
			return;
		}

		// Acquisition du mutex pour itération thread-safe sur m_Sinks
		threading::NkScopedLock lock(m_Mutex);

		// Distribution à chaque sous-sink non-null individuellement
		for (auto& sink : m_Sinks) {
			// Vérification de validité : ignorer silencieusement les entries null
			if (sink) {
				// Appel de Log() sur le sous-sink : filtrage local appliqué si configuré
				sink->Log(message);
			}
		}
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Flush
	// DESCRIPTION : Propagation du flush à tous les sous-sinks pour persistance
	// -------------------------------------------------------------------------
	void NkDistributingSink::Flush() {
		// Acquisition du mutex pour itération thread-safe sur m_Sinks
		threading::NkScopedLock lock(m_Mutex);

		// Propagation à chaque sous-sink non-null individuellement
		for (auto& sink : m_Sinks) {
			// Vérification de validité : ignorer silencieusement les entries null
			if (sink) {
				// Appel de Flush() sur le sous-sink : persistance garantie si supporté
				sink->Flush();
			}
		}
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : SetFormatter
	// DESCRIPTION : Clonage et propagation du formatter à tous les sous-sinks
	// -------------------------------------------------------------------------
	void NkDistributingSink::SetFormatter(memory::NkUniquePtr<NkFormatter> formatter) {
		// Acquisition du mutex pour modification thread-safe de la collection
		threading::NkScopedLock lock(m_Mutex);

		// Propagation via clonage : chaque sink reçoit une copie indépendante
		for (auto& sink : m_Sinks) {
			// Vérification de validité du sink et du formatter source
			if (sink && formatter) {
				// Clonage via le pattern : création d'un nouveau formatter avec même configuration
				auto clonedFormatter = memory::NkMakeUnique<NkFormatter>(formatter->GetPattern());

				// Transfert de propriété du clone vers le sous-sink
				sink->SetFormatter(traits::NkMove(clonedFormatter));
			}
		}
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : SetPattern
	// DESCRIPTION : Propagation directe du pattern à tous les sous-sinks
	// -------------------------------------------------------------------------
	void NkDistributingSink::SetPattern(const NkString& pattern) {
		// Acquisition du mutex pour itération thread-safe sur m_Sinks
		threading::NkScopedLock lock(m_Mutex);

		// Propagation directe : appel de SetPattern() sur chaque sous-sink non-null
		for (auto& sink : m_Sinks) {
			if (sink) {
				sink->SetPattern(pattern);
			}
		}
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetFormatter
	// DESCRIPTION : Retour du formatter du premier sous-sink non-null (convenance)
	// -------------------------------------------------------------------------
	NkFormatter* NkDistributingSink::GetFormatter() const {
		// Acquisition du mutex pour lecture protégée de m_Sinks
		threading::NkScopedLock lock(m_Mutex);

		// Retour du formatter du premier sink valide, ou nullptr si aucun
		if (!m_Sinks.Empty() && m_Sinks[0]) {
			return m_Sinks[0]->GetFormatter();
		}

		return nullptr;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetPattern
	// DESCRIPTION : Retour du pattern du premier sous-sink non-null (convenance)
	// -------------------------------------------------------------------------
	NkString NkDistributingSink::GetPattern() const {
		// Acquisition du mutex pour lecture protégée de m_Sinks
		threading::NkScopedLock lock(m_Mutex);

		// Retour du pattern du premier sink valide, ou chaîne vide si aucun
		if (!m_Sinks.Empty() && m_Sinks[0]) {
			return m_Sinks[0]->GetPattern();
		}

		return NkString{};
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : AddSink
	// DESCRIPTION : Ajout thread-safe d'un sous-sink à la collection
	// -------------------------------------------------------------------------
	void NkDistributingSink::AddSink(memory::NkSharedPtr<NkISink> sink) {
		// Ignoré silencieusement si sink est nullptr : robustesse contre erreurs appelant
		if (!sink) {
			return;
		}

		// Acquisition du mutex pour modification thread-safe de m_Sinks
		threading::NkScopedLock lock(m_Mutex);

		// Ajout en fin de collection : ordre d'insertion préservé pour distribution
		m_Sinks.PushBack(sink);
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : RemoveSink
	// DESCRIPTION : Suppression thread-safe d'un sous-sink par adresse pointeur
	// -------------------------------------------------------------------------
	void NkDistributingSink::RemoveSink(memory::NkSharedPtr<NkISink> sink) {
		// Ignoré silencieusement si sink est nullptr : robustesse contre erreurs appelant
		if (!sink) {
			return;
		}

		// Acquisition du mutex pour modification thread-safe de m_Sinks
		threading::NkScopedLock lock(m_Mutex);

		// Recherche linéaire par comparaison d'adresse pointeur (Get())
		for (usize index = 0; index < m_Sinks.Size(); ++index) {
			// Comparaison par adresse : deux shared_ptr vers même objet = égal
			if (m_Sinks[index].Get() == sink.Get()) {
				// Suppression de l'entry : décrémentation automatique du refcount
				m_Sinks.Erase(m_Sinks.Begin() + static_cast<ptrdiff_t>(index));
				return;  // Sortie après première occurrence trouvée
			}
		}
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : ClearSinks
	// DESCRIPTION : Vidage thread-safe de toute la collection de sous-sinks
	// -------------------------------------------------------------------------
	void NkDistributingSink::ClearSinks() {
		// Acquisition du mutex pour modification thread-safe de m_Sinks
		threading::NkScopedLock lock(m_Mutex);

		// Vidage complet : décrémentation des refcount de tous les sinks
		m_Sinks.Clear();
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetSinks
	// DESCRIPTION : Retour d'une copie de la collection pour itération hors lock
	// -------------------------------------------------------------------------
	NkVector<memory::NkSharedPtr<NkISink>> NkDistributingSink::GetSinks() const {
		// Acquisition du mutex pour lecture protégée pendant la copie
		threading::NkScopedLock lock(m_Mutex);

		// Retour par valeur : copie safe pour itération hors contexte verrouillé
		return m_Sinks;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetSinkCount
	// DESCRIPTION : Retour du nombre courant de sous-sinks (lecture thread-safe)
	// -------------------------------------------------------------------------
	usize NkDistributingSink::GetSinkCount() const {
		// Acquisition du mutex pour lecture protégée
		threading::NkScopedLock lock(m_Mutex);

		// Retour de la taille courante de la collection
		return m_Sinks.Size();
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : ContainsSink
	// DESCRIPTION : Vérification de présence par comparaison d'adresse pointeur
	// -------------------------------------------------------------------------
	bool NkDistributingSink::ContainsSink(memory::NkSharedPtr<NkISink> sink) const {
		// Ignoré silencieusement si sink est nullptr : retour false cohérent
		if (!sink) {
			return false;
		}

		// Acquisition du mutex pour lecture protégée pendant l'itération
		threading::NkScopedLock lock(m_Mutex);

		// Recherche linéaire par comparaison d'adresse pointeur (Get())
		for (const auto& currentSink : m_Sinks) {
			// Comparaison par adresse : deux shared_ptr vers même objet = égal
			if (currentSink.Get() == sink.Get()) {
				return true;  // Trouvé : retour immédiat
			}
		}

		// Non trouvé après parcours complet
		return false;
	}


} // namespace nkentseu


// =============================================================================
// NOTES D'IMPLÉMENTATION ET BONNES PRATIQUES
// =============================================================================
/*
	1. THREAD-SAFETY VIA MUTEX :
	   - Toutes les méthodes publiques acquièrent m_Mutex via NkScopedLock
	   - Log()/Flush() : itération safe sur m_Sinks pendant distribution
	   - GetSinks() : copie pendant lock pour éviter itération hors contexte verrouillé
	   - Les sous-sinks partagés doivent être thread-safe individuellement

	2. CLONAGE DE FORMATTER VIA PATTERN :
	   - SetFormatter() crée un clone via formatter->GetPattern()
	   - Avantage : indépendance des sinks, pas de race condition sur formatter partagé
	   - Inconvénient : overhead de clonage si nombreux sinks + patterns complexes
	   - Alternative future : partager le formatter si immutabilité garantie via flag

	3. COMPARAISON DE SINKS PAR ADRESSE :
	   - RemoveSink/ContainsSink comparent via Get() (adresse pointeur brute)
	   - Deux shared_ptr vers même objet NkISink = considérés égaux
	   - Attention : deux sinks différents avec même configuration ≠ égaux
	   - Pour comparaison sémantique : ajouter méthode virtuelle Equals() dans NkISink si requis

	4. GESTION DES SINKS NULL DANS LA COLLECTION :
	   - Check if (sink) avant appel Log()/Flush() sur chaque sous-sink
	   - Robustesse contre entries null accidentelles dans m_Sinks
	   - Alternative : valider à l'ajout via AddSink() pour éviter les null dans la collection

	5. PERFORMANCE DE LA DISTRIBUTION :
	   - Log() appelle Log() sur chaque sous-sink : overhead linéaire O(n)
	   - Filtrage précoce via ShouldLog() évite distribution si message ignoré globalement
	   - Pour haute fréquence : envisager buffering + flush batché ou distribution async

	6. GESTION MÉMOIRE VIA SHARED_PTR :
	   - m_Sinks contient des NkSharedPtr : refcount géré automatiquement par RAII
	   - AddSink() incrémente, RemoveSink()/ClearSinks() décrémentent
	   - Destruction du distributeur : décrémentation sans destruction forcée des sinks
	   - Safe pour partage : un sink peut appartenir à plusieurs distributeurs simultanément

	7. EXTENSIBILITÉ FUTURES :
	   - Routing conditionnel : ajouter ShouldForward(sink, message) pour filtrage par destination
	   - Priorité de distribution : ordonner m_Sinks par priorité pour traitement séquentiel
	   - Async distribution : queue de messages + thread worker pour Log() non-bloquant
	   - Métriques : compter messages distribués/ignorés par sous-sink pour monitoring

	8. TESTING ET VALIDATION :
	   - Utiliser CountingSink ou MockSink pour vérifier la distribution broadcast
	   - Tester les cas limites : collection vide, sinks null, exceptions dans sous-sinks
	   - Valider le thread-safety avec tests concurrents (TSan, helgrind, etc.)
	   - Benchmarking : mesurer l'overhead de distribution vs sink unique pour différents n
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================