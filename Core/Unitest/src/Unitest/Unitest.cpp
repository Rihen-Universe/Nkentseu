/**
* @File Unitest.cpp
* @Description Implémentation du gestionnaire de tests unitaires
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2024-04-10
* @License Rihen
*/

#include "pch.h"
#include "Unitest.h"

#include <Nkentseu/Logger/ConsoleLogger.h>
#include <Nkentseu/Logger/FileLogger.h>
#include <Nkentseu/Memory/Memory.h>

namespace nkentseu {

    /**
    * @Function UnitestSystem::UnitestSystem
    * @description Constructeur privé initialisant le système de journalisation
    * @note Le nom du logger est fixé à "Unitest" pour l'identification des logs
    */
   UnitestSystem::UnitestSystem() : m_Logger("UnitestSystem") {}

    /**
    * @Function UnitestSystem::Instance
    * @description Implémentation du pattern Singleton
    * @return (UnitestSystem&) Référence à l'instance unique
    * @warning Ne pas tenter de destruction manuelle de l'instance
    */
   UnitestSystem& UnitestSystem::Instance() {
        static UnitestSystem unitest;
        return unitest;
    }

    /**
    * @Function UnitestSystem::Register
    * @description Enregistre un nouveau cas de test dans le registre
    * @param name Nom unique identifiant le test
    * @param function Pointeur vers la fonction de test
    * @param description Information contextuelle optionnelle
    * @throws std::invalid_argument Si le nom est vide ou existe déjà
    * @note Verrouillage implicite via pattern Singleton
    */
    void UnitestSystem::Register(const std::string& name, UnitestCaseFunction function, const std::string& description) {
        if (name.empty()) {
            throw std::invalid_argument("Nom de test invalide");
        }
        auto entry = Memorys.MakeShared<UnitestCase>(name, name, function, description);
        m_UnitestList[name] = entry;
        m_CurrentRegister = name;
    }

    /**
    * @Function UnitestSystem::Run
    * @description Exécute la suite complète de tests
    * @return (int32) Nombre total d'assertions échouées
    * @process
    * 1. Réinitialisation des compteurs
    * 2. Exécution séquentielle des tests
    * 3. Collecte des résultats
    * 4. Génération des rapports détaillés
    */
    int32 UnitestSystem::Run() {
        uint32 totalRun = 0, totalFailed = 0, totalPassed = 0;
        std::string failedTests, passedTests, noTests;

        // Parcours de tous les tests enregistrés
        for (const auto& [name, entry] : m_UnitestList) {
            entry->Reset();
            m_CurrentRegister = name;

            if (entry->Run(name)) {
                uint32 contextRun = entry->GetUnitestInfoCount();
                uint32 contextFailed = 0, contextPassed = 0;

                // Analyse détaillée des résultats
                for (const auto& unitestInfo : entry->GetUnitestInfos()) {
                    if (unitestInfo.IsSuccessfull()) {
                        contextPassed++;
                        if (m_PrintPassedDetails) {
                            m_Logger.Source(unitestInfo.GetFile(), unitestInfo.GetLine(), 
                                unitestInfo.GetFunction()).Info("{0}", unitestInfo.GetMessage());
                        }
                    } else {
                        contextFailed++;
                        if (m_PrintFailedDetails) {
                            m_Logger.Source(unitestInfo.GetFile(), unitestInfo.GetLine(), 
                                unitestInfo.GetFunction()).Errors("{0}", unitestInfo.GetMessage());
                        }
                    }
                }

                // Mise à jour des métriques globales
                totalRun += contextRun;
                totalFailed += contextFailed;
                totalPassed += contextPassed;

                // Génération des rapports par contexte
                if (contextRun > 0) {
                    if (contextFailed > 0) {
                        failedTests += Formatter.FormatCurly("\n\t[{0}] > {1} / {2} Failed;", name, contextFailed, contextRun);
                    }
                    if (contextPassed > 0) {
                        passedTests += Formatter.FormatCurly("\n\t[{0}] > {1} / {2} Passed;", name, contextPassed, contextRun);
                    }
                } else if (contextRun == 0) {
                    noTests += Formatter.FormatCurly("\n\t[{0}] > Aucun test detecte;", name);
                }
            } else {
                m_Logger.Warning("Test {0} non execute - Contexte invalide", name);
            }
        }

        // Logging des résultats consolidés
        if (!failedTests.empty()) {
            m_Logger.Source().Errors("{0} Echecs / {1} Total > \n[{2}\n]", totalFailed, totalRun, failedTests);
        }
        if (!passedTests.empty()) {
            m_Logger.Source().Info("{0} Reussites / {1} Total > \n[{2}\n]", totalPassed, totalRun, passedTests);
        }
        if (!noTests.empty()) {
            m_Logger.Source().Warning("Tests sans assertions > \n[{0}\n]", noTests);
        }

        return totalFailed;
    }

    /**
    * @Function UnitestSystem::VerifyInternal
    * @description Implémentation centrale du mécanisme de vérification
    * @param condition Résultat booléen à valider
    * @param file Fichier source de l'assertion
    * @param line Ligne du code source
    * @param function Fonction parente
    * @param message Message formaté à journaliser
    * @return (UnitestState) État résultant de la vérification
    * @warning Logge un avertissement si le test n'est pas enregistré
    */
    UnitestState UnitestSystem::VerifyInternal(bool condition, const std::source_location& loc, const Date& date, const Time& time, const std::string& message) {
        if (m_CurrentRegister.empty() || !m_UnitestList.count(m_CurrentRegister)) {
            if (IsDetailPrint()) {
                m_Logger.Source(loc, date, time).Warning("Test {0} non enregistre - Verification ignoree", m_CurrentRegister);
            }
            return UnitestState::Indeterminate;
        }

        m_UnitestList.at(m_CurrentRegister)->AddUnitestInfo(UnitestInfo(m_CurrentRegister, condition, loc.file_name(), loc.line(), loc.function_name(), message));
        return condition ? UnitestState::Passed : UnitestState::Failed;
    }

    ///////////////////////////////////////////////////////////////////////////
    /// @region Accesseurs de configuration d'affichage
    ///////////////////////////////////////////////////////////////////////////

    /**
    * @Function IsPassedDetailPrint
    * @description Indique si l'affichage des succès est activé
    * @return (bool) État actuel de l'option
    */
    bool UnitestSystem::IsPassedDetailPrint() const { return m_PrintPassedDetails; }

    /**
    * @Function PrintPassedDetails
    * @description Active/désactive l'affichage des assertions réussies
    * @param print Nouvel état de l'option
    */
    void UnitestSystem::PrintPassedDetails(bool print) { m_PrintPassedDetails = print; }

    /**
    * @Function IsFailedDetailPrint
    * @description Indique si l'affichage des échecs est activé
    * @return (bool) État actuel de l'option
    */
    bool UnitestSystem::IsFailedDetailPrint() const { return m_PrintFailedDetails; }

    /**
    * @Function PrintFailedDetails
    * @description Active/désactive l'affichage des assertions échouées
    * @param print Nouvel état de l'option
    */
    void UnitestSystem::PrintFailedDetails(bool print) { m_PrintFailedDetails = print; }

    /**
    * @Function IsDetailPrint
    * @description Indique si l'affichage global est activé
    * @return (bool) État actuel de l'option
    */
    bool UnitestSystem::IsDetailPrint() const { return m_PrintDetails; }

    /**
    * @Function PrintDetails
    * @description Active/désactive tous les affichages détaillés
    * @param print Nouvel état global
    */
    void UnitestSystem::PrintDetails(bool print) { m_PrintDetails = print; }

    Logger& UnitestSystem::Get() { 
        static bool loading = false;

        if (!loading){
            m_Logger.AddTarget(Memorys.MakeShared<ConsoleLogger>("UnitestConsoleLogger"));
            m_Logger.AddTarget(Memorys.MakeShared<FileLogger>("UnitestFileLogger", "./logs/unitest", "unitest"));
            loading = true;
        }

        return m_Logger; 
    }

    /**
    * @Function RunTest
    * @description Point d'entrée standard pour l'exécution des tests
    * @return (int32) Code de retour compatible avec les processus
    * @process
    * 1. Affichage du bandeau de démarrage
    * 2. Exécution de la suite de tests
    * 3. Affichage du résumé final
    * 4. Attente de confirmation utilisateur
    */
    int32 RunTest() {
        // Configuration de l'affichage initial
        std::string message;
        message += "\n\n\033[1;36m"; // Code couleur cyan
        message += "********************************************************\n";
        message += "**  Demarrage de la suite de tests unitaires          **\n";
        message += "********************************************************\n\n";
        message += "\033[0m"; // Réinitialisation des couleurs
        
        Unitest.Get().Trace("{0}", message);

        // Exécution principale
        int32_t result = Unitest.Run();

        // Affichage des résultats consolidés
        message.clear();
        message += "\n\n\033[1;35m"; // Code couleur magenta
        message += "********************************************************\n";
        message += Formatter.FormatCurly("**  Execution terminee : {0} echec(s) detecte(s)      **\n", result);
        message += "********************************************************\n\n";
        message += "\033[0m"; // Réinitialisation des couleurs
        
        Unitest.Get().Trace("{0}", message);

        // Pause finale pour examen des résultats
        Unitest.Get().Trace("\033[1;33mAppuyez sur une touche pour terminer...\033[0m");
        std::getchar();

        Unitest.Get().ClearTargetsAndFlush();
        Unitest.Get().Shutdown();

        return result;
    }

} // namespace nkentseu