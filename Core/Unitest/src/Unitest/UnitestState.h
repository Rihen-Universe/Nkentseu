/**
* @File UnitestState.h
* @Description Définit l'énumération `UnitestState` représentant les états d'un test unitaire.
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2024-04-10
* @License Rihen
*/

#pragma once

#include "Nkentseu/Nkentseu.h"

namespace nkentseu {

    /**
    * @Enum UnitestState : Représente les états possibles d'un test unitaire.
    *
    * @Description :
    * Cette énumération définit tous les états possibles pendant le cycle de vie d'un test unitaire,
    * incluant l'exécution, les résultats finaux et les erreurs critiques.
    *
    * @Values :
    * - Indeterminate : État initial/indéterminé avant l'exécution du test.
    * - Running : Le test est en cours d'exécution.
    * - Ignored : Le test a été ignoré via des tags ou des paramètres.
    * - Passed : Le test a validé toutes les assertions avec succès.
    * - Failed : Le test a échoué sur au moins une assertion non critique.
    * - AssertionFailed : Échec critique d'une assertion obligatoire.
    * - UnhandledException : Exception non capturée pendant l'exécution.
    * - Timeout : Délai d'exécution maximal dépassé.
    */
    enum class UnitestState {
        Indeterminate,
        Running,
        Ignored,
        Passed,
        Failed,
        AssertionFailed,
        UnhandledException,
        Timeout,
    };
} // namespace nkentseu