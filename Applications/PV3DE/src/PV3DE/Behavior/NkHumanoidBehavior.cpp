#include "NkHumanoidBehavior.h"
#include "NKMath/NKMath.h"
#include "NKLogger/NkLog.h"
#include <cstring>
#include <cstdlib>

namespace nkentseu {
    namespace humanoid {

        void NkHumanoidBehavior::Init(const NkPersonality& p) noexcept {
            mPersonality = p;
            mOutput      = NkBehaviorOutput{};
            mStimuli.Clear();
            mIntentions.Clear();

            // Asymétrie faciale fixe du personnage (définie à la création)
            mOutput.asymmetry = p.traits.facialAsymmetry;

            logger.Infof("[NkHumanoidBehavior] Init — rôle {} extraversion={:.2f} N={:.2f}\n",
                         (int)p.role,
                         p.traits.extraversion,
                         p.traits.neuroticism);
        }

        // =====================================================================
        // Ajout de stimuli
        // =====================================================================
        void NkHumanoidBehavior::AddStimulus(const NkStimulus& s) noexcept {
            if (mStimuli.Size() < 32) mStimuli.PushBack(s);
        }

        void NkHumanoidBehavior::OnPain(nk_float32 intensity) noexcept {
            NkStimulus s;
            s.type      = NkStimulusType::Pain;
            s.intensity = NkClamp(intensity, 0.f, 1.f);
            AddStimulus(s);
        }

        void NkHumanoidBehavior::OnQuestion(const char* text) noexcept {
            NkStimulus s;
            s.type    = NkStimulusType::Question;
            s.content = NkString(text);
            s.intensity = 0.5f;
            AddStimulus(s);
        }

        void NkHumanoidBehavior::OnTouch(nk_float32 intensity, bool /*medical*/) noexcept {
            NkStimulus s;
            s.type      = NkStimulusType::Touch;
            s.intensity = NkClamp(intensity, 0.f, 1.f);
            AddStimulus(s);
        }

        void NkHumanoidBehavior::OnNegativeNews(nk_float32 severity) noexcept {
            NkStimulus s;
            s.type      = NkStimulusType::NegativeNews;
            s.intensity = NkClamp(severity, 0.f, 1.f);
            AddStimulus(s);
        }

        void NkHumanoidBehavior::OnReassurance() noexcept {
            NkStimulus s;
            s.type      = NkStimulusType::PositiveReinforce;
            s.intensity = 0.5f;
            AddStimulus(s);
            RememberEvent("received_reassurance", 0.5f);
        }

        // =====================================================================
        void NkHumanoidBehavior::Update(nk_float32 dt,
                                         nk_float32 pain,
                                         nk_float32 anxiety,
                                         nk_float32 fatigue) noexcept {
            // Lissage de l'état résiduel (mémoire à court terme)
            float s = NkMin(dt * 2.f, 1.f);
            mResidualPain    = NkLerp(mResidualPain,    pain    * 0.1f, s);
            mResidualAnxiety = NkLerp(mResidualAnxiety, anxiety * 0.1f, s);

            // Timers
            mMicroExprTimer   += dt;
            mIdleTimer        += dt;
            mGazeSaccadeTimer += dt;
            if (mCryTimer > 0.f) mCryTimer -= dt;

            // Décroissance des mémoires
            for (nk_uint32 i = 0; i < mMemoryCount; ++i) {
                mMemories[i].valence *= (1.f - mMemories[i].decay * dt);
            }

            // Pipeline de décision
            ProcessStimuli();
            EvaluateIntentions(pain, anxiety, fatigue);
            FilterByPersonality();
            FilterByRole();
            EmitBehaviors();
        }

        // =====================================================================
        // Pipeline
        // =====================================================================
        void NkHumanoidBehavior::ProcessStimuli() noexcept {
            for (nk_usize i = 0; i < mStimuli.Size(); ++i) {
                auto& s = mStimuli[i];
                if (s.isHandled) continue;

                NkBehaviorIntent intent;
                intent.isInvoluntary = false;

                switch (s.type) {
                    case NkStimulusType::Pain:
                        intent.type      = NkIntentType::Wince;
                        intent.priority  = 0.6f + s.intensity * 0.4f;
                        intent.intensity = s.intensity;
                        intent.duration  = 0.3f + s.intensity * 0.5f;
                        intent.isInvoluntary = (s.intensity > 0.7f);
                        mIntentions.PushBack(intent);

                        if (s.intensity > 0.5f) {
                            NkBehaviorIntent reach;
                            reach.type      = NkIntentType::ReachForPain;
                            reach.priority  = 0.5f;
                            reach.intensity = s.intensity;
                            reach.duration  = 2.f;
                            mIntentions.PushBack(reach);
                        }
                        break;

                    case NkStimulusType::NegativeNews:
                        intent.type      = NkIntentType::Microexpression;
                        intent.priority  = 0.7f;
                        intent.intensity = s.intensity;
                        intent.duration  = 0.25f;
                        intent.data      = "shock"; // utilisé par EmitBehaviors
                        mIntentions.PushBack(intent);

                        // Anxiété résiduelle
                        mResidualAnxiety += s.intensity * 0.3f;
                        break;

                    case NkStimulusType::PositiveReinforce:
                        intent.type      = NkIntentType::ExpressEmotion;
                        intent.priority  = 0.4f;
                        intent.intensity = 0.3f;
                        intent.data      = "relief";
                        mIntentions.PushBack(intent);
                        break;

                    case NkStimulusType::SuddenNoise:
                        // Réflexe de sursaut — toujours involontaire
                        intent.type          = NkIntentType::Microexpression;
                        intent.priority      = 0.95f;
                        intent.intensity     = s.intensity;
                        intent.duration      = 0.15f;
                        intent.data          = "startle";
                        intent.isInvoluntary = true;
                        mIntentions.PushBack(intent);
                        break;

                    case NkStimulusType::Touch:
                        // Tension réflexe si toucher sur zone sensible
                        intent.type      = NkIntentType::ExpressEmotion;
                        intent.priority  = 0.5f + s.intensity * 0.3f;
                        intent.intensity = s.intensity * 0.7f;
                        intent.data      = "tension";
                        mIntentions.PushBack(intent);
                        break;

                    default: break;
                }
                s.isHandled = true;
            }

            // Purger les stimuli traités
            nk_usize write = 0;
            for (nk_usize r = 0; r < mStimuli.Size(); ++r)
                if (!mStimuli[r].isHandled)
                    mStimuli[write++] = mStimuli[r];
            mStimuli.Resize(write);
        }

        // =====================================================================
        void NkHumanoidBehavior::EvaluateIntentions(nk_float32 pain,
                                                     nk_float32 anxiety,
                                                     nk_float32 fatigue) noexcept {
            GeneratePainResponse   (pain);
            GenerateAnxietyResponse(anxiety);
            GenerateFatigueResponse(fatigue);
            GenerateMicroexpressions(mMicroExprTimer);
            GenerateIdleBehaviors   (mIdleTimer);
        }

        void NkHumanoidBehavior::GeneratePainResponse(nk_float32 pain) noexcept {
            if (pain < 0.1f) return;
            float normPain = pain / 10.f;

            // Pleurs si seuil franchi + personnalité le permet
            float cryThreshold = 1.f - mPersonality.traits.CryProbability();
            if (normPain > cryThreshold && mCryTimer <= 0.f) {
                NkBehaviorIntent cry;
                cry.type      = NkIntentType::Cry;
                cry.priority  = 0.6f * normPain;
                cry.intensity = (normPain - cryThreshold) / (1.f - cryThreshold);
                cry.duration  = 5.f + normPain * 10.f;
                mIntentions.PushBack(cry);
                mCryTimer = cry.duration;
            }

            // Protection (bras croisés) si douleur thoracique ou abdominale
            if (normPain > 0.4f) {
                NkBehaviorIntent guard;
                guard.type      = NkIntentType::Guard;
                guard.priority  = 0.4f * normPain;
                guard.intensity = normPain;
                mIntentions.PushBack(guard);
            }
        }

        void NkHumanoidBehavior::GenerateAnxietyResponse(nk_float32 anxiety) noexcept {
            if (anxiety < 0.2f) return;

            // Regard fuyant si anxiété modérée
            if (anxiety > 0.4f) {
                NkBehaviorIntent gaze;
                gaze.type      = NkIntentType::AvoidEyeContact;
                gaze.priority  = 0.3f + anxiety * 0.3f;
                gaze.intensity = anxiety;
                mIntentions.PushBack(gaze);
            }

            // Chercher du réconfort si anxiété élevée + agréabilité élevée
            if (anxiety > 0.6f && mPersonality.traits.agreeableness > 0.5f) {
                NkBehaviorIntent reassure;
                reassure.type      = NkIntentType::SeekReassurance;
                reassure.priority  = 0.4f;
                reassure.intensity = anxiety;
                mIntentions.PushBack(reassure);
            }
        }

        void NkHumanoidBehavior::GenerateFatigueResponse(nk_float32 fatigue) noexcept {
            (void)fatigue;
            // La fatigue se manifeste surtout dans le BodyController (posture Slumped)
            // et dans la vitesse de parole (SpeechEngine)
            // Ici : réduction de l'expressivité spontanée (idle behaviors moins fréquents)
        }

        void NkHumanoidBehavior::GenerateMicroexpressions(nk_float32 timer) noexcept {
            // Micro-expression involontaire toutes les 4–12 secondes
            float interval = 6.f + (1.f - mPersonality.traits.neuroticism) * 6.f;
            if (timer < interval) return;
            mMicroExprTimer = 0.f;

            // Choisir une micro-expression contextuelle
            NkBehaviorIntent micro;
            micro.type          = NkIntentType::Microexpression;
            micro.isInvoluntary = true;
            micro.duration      = 0.15f + (float)(rand() % 15) * 0.01f; // 150–300ms
            micro.intensity     = 0.3f + mPersonality.traits.neuroticism * 0.4f;

            // Type de micro selon état résiduel
            if (mResidualPain > 0.3f)    micro.data = "pain_flash";
            else if (mResidualAnxiety > 0.3f) micro.data = "worry_flash";
            else                         micro.data = "neutral_twitch";

            micro.priority = 0.7f; // haute priorité (involontaire)
            mIntentions.PushBack(micro);
        }

        void NkHumanoidBehavior::GenerateIdleBehaviors(nk_float32 timer) noexcept {
            // Comportements de repos naturels toutes les 3–8 secondes
            float interval = 3.f + (1.f - mPersonality.traits.extraversion) * 5.f;
            if (timer < interval) return;
            mIdleTimer = 0.f;

            // Saccade du regard
            if (mGazeSaccadeTimer > 2.f) {
                mGazeSaccadeTimer = 0.f;
                NkBehaviorIntent saccade;
                saccade.type      = NkIntentType::AvoidEyeContact;
                saccade.priority  = 0.2f;
                saccade.duration  = 0.5f + (float)(rand() % 100) * 0.01f;
                saccade.intensity = 0.3f;
                mIntentions.PushBack(saccade);
            }
        }

        // =====================================================================
        void NkHumanoidBehavior::FilterByPersonality() noexcept {
            float ampScale = mPersonality.traits.ExpressionAmplitude();

            for (nk_usize i = 0; i < mIntentions.Size(); ++i) {
                auto& intent = mIntentions[i];

                // Les réflexes involontaires passent toujours
                if (intent.isInvoluntary) continue;

                // Stoïcisme : atténue l'expression de la douleur
                if (intent.type == NkIntentType::Wince ||
                    intent.type == NkIntentType::ExpressEmotion) {
                    intent.intensity *= ampScale;

                    // Si le patient ment sur sa douleur (trait stoïque)
                    float understate = mPersonality.traits.PainUnderstatement();
                    if (intent.type == NkIntentType::Wince)
                        intent.intensity *= (1.f - understate * 0.6f);
                }

                // Anxiété de trait : amplifie les réponses de peur
                if (intent.type == NkIntentType::AvoidEyeContact) {
                    intent.intensity *= (0.5f + mPersonality.traits.neuroticism);
                }

                // Introvertis parlent moins
                if (intent.type == NkIntentType::Speak) {
                    if (mPersonality.traits.extraversion < 0.3f)
                        intent.priority *= 0.5f;
                }
            }
        }

        void NkHumanoidBehavior::FilterByRole() noexcept {
            const auto& c = mPersonality.constraints;

            for (nk_isize i = (nk_isize)mIntentions.Size() - 1; i >= 0; --i) {
                auto& intent = mIntentions[i];

                // Rôle ne permet pas les pleurs
                if (!c.canCry && intent.type == NkIntentType::Cry) {
                    mIntentions.EraseAt((nk_usize)i);
                    continue;
                }

                // Rôle ne permet pas la panique
                if (!c.canPanic && intent.type == NkIntentType::ExpressEmotion
                    && intent.data == "panic") {
                    mIntentions.EraseAt((nk_usize)i);
                    continue;
                }

                // Appliquer le scale global du rôle
                intent.intensity *= c.expressionScale;
                intent.intensity  = NkClamp(intent.intensity, 0.f, 1.f);

                // Supprimer si trop faible après filtrage
                if (intent.intensity < 0.02f && !intent.isInvoluntary) {
                    mIntentions.EraseAt((nk_usize)i);
                }
            }
        }

        // =====================================================================
        void NkHumanoidBehavior::EmitBehaviors() noexcept {
            mOutput.faceTargets.Clear();
            mOutput.shouldSpeak   = false;
            mOutput.reachForPain  = false;
            mOutput.crying        = (mCryTimer > 0.f);
            mOutput.cryIntensity  = mCryTimer > 0.f ? NkMin(mCryTimer / 5.f, 1.f) : 0.f;
            mOutput.avoidEyeContact = false;

            // Trier les intentions par priorité décroissante
            for (nk_isize i = 1; i < (nk_isize)mIntentions.Size(); ++i) {
                NkBehaviorIntent key = mIntentions[i];
                nk_isize j = i - 1;
                while (j >= 0 && mIntentions[j].priority < key.priority) {
                    mIntentions[j+1] = mIntentions[j]; --j;
                }
                mIntentions[j+1] = key;
            }

            for (nk_usize i = 0; i < mIntentions.Size(); ++i) {
                const auto& intent = mIntentions[i];

                switch (intent.type) {
                    case NkIntentType::Wince: {
                        // Grimace douleur → AU4+6+7+9
                        auto addAU = [&](nk_uint8 au, nk_float32 w, bool flash) {
                            NkBehaviorOutput::FaceTarget ft;
                            ft.auId      = au;
                            ft.intensity = intent.intensity * w;
                            ft.duration  = intent.duration;
                            ft.isFlash   = flash;
                            mOutput.faceTargets.PushBack(ft);
                        };
                        addAU(4,  0.9f, false);
                        addAU(6,  0.6f, false);
                        addAU(7,  0.7f, false);
                        addAU(9,  0.4f, intent.isInvoluntary);
                        addAU(25, 0.3f, false);
                        break;
                    }
                    case NkIntentType::Microexpression: {
                        NkBehaviorOutput::FaceTarget ft;
                        ft.duration = intent.duration;
                        ft.isFlash  = true;
                        if (intent.data == "shock") {
                            ft.auId = 1; ft.intensity = 0.8f * intent.intensity;
                            mOutput.faceTargets.PushBack(ft);
                            ft.auId = 5; ft.intensity = 0.7f * intent.intensity;
                            mOutput.faceTargets.PushBack(ft);
                        } else if (intent.data == "startle") {
                            ft.auId = 1; ft.intensity = 1.0f;
                            mOutput.faceTargets.PushBack(ft);
                            ft.auId = 2; ft.intensity = 1.0f;
                            mOutput.faceTargets.PushBack(ft);
                            ft.auId = 5; ft.intensity = 0.9f;
                            mOutput.faceTargets.PushBack(ft);
                        } else if (intent.data == "pain_flash") {
                            ft.auId = 4; ft.intensity = 0.5f * intent.intensity;
                            mOutput.faceTargets.PushBack(ft);
                        } else if (intent.data == "relief") {
                            ft.auId = 12; ft.intensity = 0.3f * intent.intensity;
                            mOutput.faceTargets.PushBack(ft);
                        }
                        break;
                    }
                    case NkIntentType::ReachForPain:
                        mOutput.reachForPain = true;
                        break;
                    case NkIntentType::AvoidEyeContact:
                        mOutput.avoidEyeContact = true;
                        // Saccade aléatoire
                        mOutput.gazeYaw   = (float)(rand() % 40 - 20);
                        mOutput.gazePitch = (float)(rand() % 20 - 10);
                        break;
                    case NkIntentType::SeekReassurance:
                        mOutput.avoidEyeContact = false;
                        mOutput.gazeYaw   = 0.f;
                        mOutput.gazePitch = 5.f; // regarder vers le médecin
                        break;
                    case NkIntentType::Cry:
                        // Les pleurs modifient aussi le visage (AU1+4+17)
                        {
                            NkBehaviorOutput::FaceTarget ft;
                            ft.duration = 0.f;
                            ft.isFlash  = false;
                            ft.auId = 1;  ft.intensity = 0.7f * intent.intensity;
                            mOutput.faceTargets.PushBack(ft);
                            ft.auId = 4;  ft.intensity = 0.8f * intent.intensity;
                            mOutput.faceTargets.PushBack(ft);
                            ft.auId = 17; ft.intensity = 0.6f * intent.intensity;
                            mOutput.faceTargets.PushBack(ft);
                        }
                        break;
                    default: break;
                }
            }

            mIntentions.Clear();
        }

        // =====================================================================
        void NkHumanoidBehavior::RememberEvent(const char* key,
                                                nk_float32 valence) noexcept {
            for (nk_uint32 i = 0; i < mMemoryCount; ++i) {
                if (NkStrEqual(mMemories[i].key, key)) {
                    // Mise à jour par moyenne pondérée
                    mMemories[i].valence = NkLerp(mMemories[i].valence, valence, 0.5f);
                    return;
                }
            }
            if (mMemoryCount < kMaxMemories) {
                NkStrNCpy(mMemories[mMemoryCount].key, key, 63);
                mMemories[mMemoryCount].valence = NkClamp(valence, -1.f, 1.f);
                mMemories[mMemoryCount].decay   = 0.01f; // lent
                ++mMemoryCount;
            }
        }

        nk_float32 NkHumanoidBehavior::RecallEvent(const char* key) const noexcept {
            for (nk_uint32 i = 0; i < mMemoryCount; ++i)
                if (NkStrEqual(mMemories[i].key, key)) return mMemories[i].valence;
            return 0.f;
        }

    } // namespace humanoid
} // namespace nkentseu
