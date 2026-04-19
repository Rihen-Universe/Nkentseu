#include "NkConversationEngine.h"
#include "NkOllamaBackend.h"
#include "NkClaudeBackend.h"
#include "NkRulesBackend.h"
#include "NKLogger/NkLog.h"
#include <cstdio>
#include <cstring>

namespace nkentseu {
    namespace humanoid {

        NkConversationEngine::~NkConversationEngine() noexcept {
            Shutdown();
        }

        bool NkConversationEngine::Init(NkConvBackendType type,
                                         const char* modelName,
                                         const char* endpoint,
                                         const char* apiKey) noexcept {
            Shutdown();

            switch (type) {
                case NkConvBackendType::OllamaLocal:
                    mBackend = new NkOllamaBackend();
                    break;
                case NkConvBackendType::ClaudeAPI:
                    mBackend = new NkClaudeBackend();
                    break;
                case NkConvBackendType::RulesBased:
                    mBackend = new NkRulesBackend();
                    break;
                default:
                    mBackend = new NkRulesBackend(); // fallback
                    break;
            }

            if (!mBackend->Init(endpoint, modelName, apiKey)) {
                logger.Warnf("[NkConvEngine] Backend {} indisponible, fallback règles\n",
                             mBackend->BackendName());
                delete mBackend;
                mBackend = new NkRulesBackend();
                mBackend->Init("", "", nullptr);
            }

            logger.Infof("[NkConvEngine] Init avec backend: {} model: {}\n",
                         mBackend->BackendName(), modelName ? modelName : "default");
            return true;
        }

        void NkConversationEngine::Shutdown() noexcept {
            delete mBackend;
            mBackend = nullptr;
            mHistory.Clear();
        }

        // =====================================================================
        NkConvResponse NkConversationEngine::Ask(const NkConvRequest& req) noexcept {
            NkConvResponse resp;

            if (!mBackend || !mBackend->IsAvailable()) {
                resp.text    = "Je... je ne peux pas répondre.";
                resp.success = false;
                return resp;
            }

            // ── 1. Construire le contexte complet ─────────────────────────────
            NkVector<NkConvMessage> messages;

            // System prompt (personnalité + état clinique)
            NkConvMessage sys;
            sys.role    = NkConvRole::System;
            sys.content = BuildSystemPrompt(req);
            messages.PushBack(sys);

            // Historique précédent (tronqué)
            nk_usize histStart = mHistory.Size() > kMaxHistory
                ? mHistory.Size() - kMaxHistory : 0;
            for (nk_usize i = histStart; i < mHistory.Size(); ++i)
                messages.PushBack(mHistory[i]);

            // Message courant
            NkConvMessage userMsg;
            userMsg.role    = NkConvRole::User;
            userMsg.content = req.userMessage;
            messages.PushBack(userMsg);

            // ── 2. Appel au backend ───────────────────────────────────────────
            bool ok = mBackend->Complete(messages,
                                          req.temperature_llm,
                                          req.maxTokens,
                                          resp);

            if (ok && !resp.text.IsEmpty()) {
                // Sauvegarder dans l'historique
                PushMessage(NkConvRole::User,      req.userMessage);
                PushMessage(NkConvRole::Assistant, resp.text);
                ParseAnnotations(resp);
                resp.success = true;
            } else {
                resp.text    = "Je... je ne me sens pas bien.";
                resp.success = false;
            }

            return resp;
        }

        void NkConversationEngine::ResetHistory() noexcept {
            mHistory.Clear();
        }

        // =====================================================================
        // Construction du prompt système — le cœur de l'IA comportementale
        // =====================================================================
        NkString NkConversationEngine::BuildSystemPrompt(
            const NkConvRequest& req) const noexcept {

            NkString prompt;

            // ── Identité du personnage ─────────────────────────────────────────
            prompt += "Tu incarnes un patient dans un contexte de simulation médicale "
                      "éducative (Patient Virtuel 3D Emotif — PV3DE).\n\n";

            if (req.personality) {
                const auto& life = req.personality->life;
                const auto& traits = req.personality->traits;

                char buf[256];

                // Identité
                snprintf(buf, sizeof(buf),
                         "## Qui tu es\n"
                         "Nom: %s, %u ans (%s), %s.\n"
                         "Situation: %s\n\n",
                         life.name.CStr(), life.ageYears, life.gender.CStr(),
                         life.profession.CStr(), life.currentSituation.CStr());
                prompt += buf;

                // Personnalité (traduite en instructions comportementales)
                prompt += "## Ta personnalité\n";

                // Extraversion → verbosité
                if (traits.extraversion > 0.65f)
                    prompt += "- Tu parles facilement, tu décris bien tes symptômes.\n";
                else if (traits.extraversion < 0.35f)
                    prompt += "- Tu es peu bavard(e), tes réponses sont brèves, tu attends qu'on t'interroge.\n";

                // Neuroticisme → anxiété
                if (traits.neuroticism > 0.65f)
                    prompt += "- Tu t'angoisses facilement, tu catastrophises parfois.\n";
                else if (traits.neuroticism < 0.35f)
                    prompt += "- Tu gardes ton calme même sous la douleur.\n";

                // Conscienciosité → stoïcisme
                if (traits.conscientiousness > 0.65f)
                    prompt += "- Tu minimises ta douleur ('ça va aller'), tu ne veux pas déranger.\n";

                // Tolérance à la douleur
                if (traits.painTolerance > 0.7f)
                    prompt += "- Tu as une tolérance élevée à la douleur (douleur chronique habituelle).\n";
                else if (traits.painTolerance < 0.3f)
                    prompt += "- Tu es très sensible à la douleur, tu l'exprimes fortement.\n";

                // Confiance
                snprintf(buf, sizeof(buf),
                         "- Ton niveau de confiance envers ce médecin: %.0f%% (%s).\n",
                         traits.trustInAuthority * 100.f,
                         traits.trustInAuthority > 0.6f ? "tu coopères bien" :
                         traits.trustInAuthority > 0.3f ? "tu es réservé(e)" :
                                                          "tu es méfiant(e)");
                prompt += buf;

                // Antécédents médicaux
                if (life.hasChronicPain)
                    prompt += "- Tu souffres de douleurs chroniques depuis des années.\n";
                if (life.hasMedicalAnxiety)
                    prompt += "- Tu as une anxiété médicale — les hôpitaux te stressent.\n";
                if (life.isOnPainkillers)
                    prompt += "- Tu prends des antalgiques, ta douleur ressentie est un peu atténuée.\n";
            }

            // ── État clinique actuel ───────────────────────────────────────────
            prompt += "\n## Ton état clinique actuel\n";

            char stateBuf[512];
            snprintf(stateBuf, sizeof(stateBuf),
                     "- Douleur: %.1f/10 (%s)\n"
                     "- Anxiété: %.0f%% (%s)\n"
                     "- Fatigue: %.0f%%\n"
                     "- Difficulté respiratoire: %.0f%%\n"
                     "- FC: %.0f bpm | T°: %.1f°C | SpO2: %.0f%%\n",
                     req.painLevel,
                     req.painLevel >= 7.f ? "sévère" :
                     req.painLevel >= 4.f ? "modérée" :
                     req.painLevel >= 1.f ? "légère" : "absente",
                     req.anxietyLevel * 100.f,
                     req.anxietyLevel > 0.7f ? "très anxieux" :
                     req.anxietyLevel > 0.3f ? "un peu anxieux" : "calme",
                     req.fatigueLevel * 100.f,
                     req.breathingDifficulty * 100.f,
                     req.heartRate, req.temperature, req.spo2);
            prompt += stateBuf;

            if (!req.topDiagnosis.IsEmpty()) {
                char diagBuf[128];
                snprintf(diagBuf, sizeof(diagBuf),
                         "(Le médecin suspect: %s — mais tu ne le sais pas)\n",
                         req.topDiagnosis.CStr());
                prompt += diagBuf;
            }

            // ── Règles de comportement ────────────────────────────────────────
            prompt += "\n## Règles impératives\n"
                      "1. Tu réponds UNIQUEMENT en tant que patient, jamais en tant qu'IA.\n"
                      "2. Tes réponses font 1 à 4 phrases maximum.\n"
                      "3. Tu utilises un langage parlé, naturel, parfois hésitant si douleur.\n"
                      "4. Tu n'utilises JAMAIS de termes médicaux techniques (tu es patient, pas médecin).\n"
                      "5. Si ta douleur est ≥ 7/10, tu peux grimacer, t'interrompre: "
                      "\"Ah... (pause) ...oui, dans la poitrine.\"\n"
                      "6. Si tu es stoïque, tu minimises: \"C'est supportable\", \"ça va aller\".\n"
                      "7. Reste cohérent avec ta personnalité et tes antécédents.\n"
                      "8. JAMAIS de diagnostic personnel: tu décris des symptômes, "
                      "pas des maladies.\n";

            if (req.personality) {
                const auto& c = req.personality->constraints;
                if (!c.canLieAboutPain)
                    prompt += "9. Tu es honnête sur ta douleur, tu ne la minimises pas.\n";
                if (c.canRefuseToAnswer)
                    prompt += "10. Si une question te met très mal à l'aise, tu peux décliner poliment.\n";
            }

            return prompt;
        }

        void NkConversationEngine::PushMessage(NkConvRole role,
                                               const NkString& content) noexcept {
            // Tronquer l'historique si trop long
            while (mHistory.Size() >= kMaxHistory * 2)
                mHistory.EraseAt(0);

            NkConvMessage msg;
            msg.role    = role;
            msg.content = content;
            mHistory.PushBack(msg);
        }

        void NkConversationEngine::ParseAnnotations(NkConvResponse& resp) noexcept {
            // Détecter l'intensité émotionnelle depuis le texte
            const char* t = resp.text.CStr();
            if (strstr(t, "Ah !") || strstr(t, "...") || strstr(t, "Aïe"))
                resp.emotionIntensity += 0.3f;
            if (strstr(t, "impossible") || strstr(t, "insupportable"))
                resp.emotionIntensity += 0.5f;
            if (strstr(t, "ça va") || strstr(t, "pas grave"))
                resp.emotionIntensity = NkMax(resp.emotionIntensity - 0.2f, 0.f);
            resp.emotionIntensity = NkClamp(resp.emotionIntensity, 0.f, 1.f);
        }

    } // namespace humanoid
} // namespace nkentseu
