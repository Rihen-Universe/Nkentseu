#include "NkAIDriver.h"
#include "NKLogger/NkLog.h"
#include <cstring>

namespace nkentseu {
    namespace humanoid {

        bool NkAIDriver::Init(const NkAIDriverConfig& config,
                               const NkPersonality& personality) noexcept {
            mConfig = config;
            mBehavior.Init(personality);

            if (config.enableConversation) {
                mConversation.Init(config.backendType,
                                    config.modelName.CStr(),
                                    config.endpoint.CStr(),
                                    config.apiKey.IsEmpty() ? nullptr
                                                            : config.apiKey.CStr());
            }

            logger.Infof("[NkAIDriver] Init OK — async={} conversation={}\n",
                         config.asyncResponse, config.enableConversation);
            return true;
        }

        void NkAIDriver::Shutdown() noexcept {
            // Attendre que le thread de réponse se termine
            mThinking = false;
            mConversation.Shutdown();
        }

        // =====================================================================
        void NkAIDriver::Update(nk_float32 dt,
                                 nk_float32 pain,
                                 nk_float32 anxiety,
                                 nk_float32 fatigue) noexcept {
            if (mConfig.enableBehavior)
                mBehavior.Update(dt, pain, anxiety, fatigue);

            // Récupérer la réponse LLM si elle est prête
            PollResponse();
        }

        // =====================================================================
        void NkAIDriver::AskQuestion(const char* question,
                                      nk_float32 painLevel,
                                      nk_float32 anxietyLevel,
                                      nk_float32 fatigueLevel,
                                      nk_float32 breathDifficulty,
                                      nk_float32 hr, nk_float32 temp, nk_float32 spo2,
                                      NkAIResponseCallback callback) noexcept {
            if (mThinking) {
                logger.Warnf("[NkAIDriver] Réponse déjà en cours\n");
                return;
            }

            // Ajouter le stimulus "question" au behavior
            if (mConfig.enableBehavior)
                mBehavior.OnQuestion(question);

            if (!mConfig.enableConversation) {
                // Fallback règles
                NkConvResponse fake;
                fake.text    = NkString("Je... je ne sais pas.");
                fake.success = true;
                if (callback) callback(fake.text, 0.f);
                return;
            }

            // Préparer la requête
            NkConvRequest req;
            req.userMessage        = NkString(question);
            req.painLevel          = painLevel;
            req.anxietyLevel       = anxietyLevel;
            req.fatigueLevel       = fatigueLevel;
            req.breathingDifficulty= breathDifficulty;
            req.heartRate          = hr;
            req.temperature        = temp;
            req.spo2               = spo2;
            req.maxTokens          = 120;
            req.temperature_llm    = 0.7f;

            mPending.ready    = false;
            mPending.callback = callback;
            mThinking         = true;

            if (mConfig.asyncResponse) {
                // Lancer dans un thread séparé (NkThread ou std::thread)
                // Pour simplifier : ici on utilise NkThread du système threading
                // En production : utiliser NkThreadPool
                nkentseu::threading::NkThread workerThread([this, req]() {
                    NkConvResponse resp = mConversation.Ask(req);
                    mPending.response = resp;
                    mPending.ready    = true;
                    mThinking         = false;
                });
                workerThread.Detach();
            } else {
                // Synchrone (bloque le thread principal)
                mPending.response = mConversation.Ask(req);
                mPending.ready    = true;
                mThinking         = false;
                PollResponse();
            }
        }

        bool NkAIDriver::PollResponse() noexcept {
            if (!mPending.ready) return false;

            const auto& resp = mPending.response;

            // Notifier via callback
            if (mPending.callback)
                mPending.callback(resp.text, resp.emotionIntensity);
            else if (mDefaultCallback)
                mDefaultCallback(resp.text, resp.emotionIntensity);

            // Adapter le comportement selon l'intensité émotionnelle détectée
            if (mConfig.enableBehavior && resp.emotionIntensity > 0.3f) {
                NkStimulus s;
                s.type      = NkStimulusType::Question;
                s.intensity = resp.emotionIntensity;
                mBehavior.AddStimulus(s);
            }

            mPending.ready = false;
            return true;
        }

        void NkAIDriver::ResetConversation() noexcept {
            mConversation.ResetHistory();
            logger.Infof("[NkAIDriver] Historique de conversation réinitialisé\n");
        }

    } // namespace humanoid
} // namespace nkentseu
