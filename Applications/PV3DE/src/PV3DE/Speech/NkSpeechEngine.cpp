#include "NkSpeechEngine.h"
#include "NKMath/NKMath.h"
#include "NKLogger/NkLog.h"
#include <string.h>

namespace nkentseu {
    namespace pv3de {

        // =====================================================================
        // NkVisemeMapper
        // =====================================================================

        // Règles graphème → visème (ordre : plus long d'abord)
        const NkVisemeMapper::PhonemeRule NkVisemeMapper::kRules[] = {
            {"ou",  NkViseme::OO}, {"oi",  NkViseme::OO},
            {"au",  NkViseme::AO}, {"eau", NkViseme::OH},
            {"ch",  NkViseme::SH}, {"ph",  NkViseme::FF},
            {"th",  NkViseme::TH}, {"gn",  NkViseme::NN},
            {"eu",  NkViseme::UH}, {"ai",  NkViseme::AE},
            {nullptr, NkViseme::Silence}
        };

        NkViseme NkVisemeMapper::CharToViseme(char c) {
            switch (c | 0x20) { // lowercase
                case 'a': return NkViseme::AA;
                case 'e': return NkViseme::EH;
                case 'i': case 'y': return NkViseme::IH;
                case 'o': return NkViseme::OH;
                case 'u': return NkViseme::OO;
                case 'f': case 'v': return NkViseme::FF;
                case 'm': case 'b': case 'p': return NkViseme::NN;
                case 'n': return NkViseme::NN;
                case 'w': return NkViseme::WW;
                case 'l': case 'd': case 't': case 's':
                case 'z': case 'r': return NkViseme::TH;
                case 'k': case 'g': case 'x': case 'q': return NkViseme::AE;
                case ' ': case ',': case '.': case '!':
                case '?': case ';': return NkViseme::Silence;
                default: return NkViseme::AX;
            }
        }

        NkVector<NkVisemeFrame> NkVisemeMapper::Map(const NkString& text,
                                                     nk_float32 durationMs) {
            NkVector<NkVisemeFrame> frames;
            if (text.IsEmpty() || durationMs <= 0.f) return frames;

            const char* s   = text.CStr();
            nk_usize    len = text.Length();
            nk_float32  msPerChar = durationMs / (nk_float32)NkMax(len, (nk_usize)1);
            nk_float32  t   = 0.f;

            for (nk_usize i = 0; i < len; ) {
                // Test des digraphes
                bool matched = false;
                for (nk_uint32 r = 0; kRules[r].grapheme; ++r) {
                    nk_usize rlen = strlen(kRules[r].grapheme);
                    if (i + rlen <= len &&
                        strncasecmp(s + i, kRules[r].grapheme, rlen) == 0) {
                        NkVisemeFrame f;
                        f.timeMs    = t;
                        f.viseme    = kRules[r].viseme;
                        f.intensity = (kRules[r].viseme == NkViseme::Silence) ? 0.f : 1.f;
                        frames.PushBack(f);
                        t += msPerChar * (nk_float32)rlen;
                        i += rlen;
                        matched = true;
                        break;
                    }
                }
                if (!matched) {
                    NkVisemeFrame f;
                    f.timeMs    = t;
                    f.viseme    = CharToViseme(s[i]);
                    f.intensity = (f.viseme == NkViseme::Silence) ? 0.f : 1.f;
                    frames.PushBack(f);
                    t += msPerChar;
                    ++i;
                }
            }

            // Frame de fermeture
            NkVisemeFrame end; end.timeMs = durationMs;
            end.viseme = NkViseme::Silence; end.intensity = 0.f;
            frames.PushBack(end);
            return frames;
        }

        // =====================================================================
        // NkLipSyncController
        // =====================================================================

        void NkLipSyncController::Play(const NkVector<NkVisemeFrame>& frames,
                                       nk_float32 startMs) {
            mFrames    = frames;
            mCurrentMs = startMs;
            mFrameIdx  = 0;
            mPlaying   = !frames.IsEmpty();
            mBlendT    = 1.f;
            mCurrentViseme  = NkViseme::Silence;
            mPreviousViseme = NkViseme::Silence;
        }

        void NkLipSyncController::Stop() {
            mPlaying = false;
            mFrameIdx = 0;
        }

        void NkLipSyncController::Update(nk_float32 dt, NkFaceController& fc) {
            if (!mPlaying || mFrames.IsEmpty()) return;

            mCurrentMs += dt * 1000.f; // secondes → ms

            // Avancer dans les frames
            while (mFrameIdx + 1 < mFrames.Size() &&
                   mCurrentMs >= mFrames[mFrameIdx + 1].timeMs) {
                ++mFrameIdx;
                mPreviousViseme = mCurrentViseme;
                mCurrentViseme  = mFrames[mFrameIdx].viseme;
                mBlendT = 0.f;
            }

            if (mFrameIdx >= mFrames.Size()) {
                mPlaying = false;
                ApplyViseme(NkViseme::Silence, 0.f, fc);
                return;
            }

            // Blend entre visème courant et précédent
            mBlendT = NkMin(mBlendT + dt * 8.f, 1.f); // ~125ms de transition
            nk_float32 intensity = mFrames[mFrameIdx].intensity *
                                   NkSmoothStep(0.f, 1.f, mBlendT);
            ApplyViseme(mCurrentViseme, intensity, fc);
        }

        void NkLipSyncController::ApplyViseme(NkViseme v, nk_float32 intensity,
                                               NkFaceController& fc) {
            // Reset AUs bouche
            fc.SetAUTarget(NkActionUnitId::AU25, 0.f);
            fc.SetAUTarget(NkActionUnitId::AU26, 0.f);
            fc.SetAUTarget(NkActionUnitId::AU18, 0.f);
            fc.SetAUTarget(NkActionUnitId::AU20, 0.f);

            switch (v) {
                case NkViseme::AA: case NkViseme::AO:
                    fc.SetAUTarget(NkActionUnitId::AU25, 0.7f * intensity);
                    fc.SetAUTarget(NkActionUnitId::AU26, 0.5f * intensity);
                    break;
                case NkViseme::EE:
                    fc.SetAUTarget(NkActionUnitId::AU25, 0.3f * intensity);
                    fc.SetAUTarget(NkActionUnitId::AU20, 0.6f * intensity);
                    break;
                case NkViseme::OO: case NkViseme::WW:
                    fc.SetAUTarget(NkActionUnitId::AU18, 0.7f * intensity);
                    fc.SetAUTarget(NkActionUnitId::AU26, 0.3f * intensity);
                    break;
                case NkViseme::AE: case NkViseme::EH:
                    fc.SetAUTarget(NkActionUnitId::AU25, 0.4f * intensity);
                    fc.SetAUTarget(NkActionUnitId::AU26, 0.2f * intensity);
                    break;
                case NkViseme::FF:
                    fc.SetAUTarget(NkActionUnitId::AU25, 0.2f * intensity);
                    break;
                case NkViseme::NN: case NkViseme::TH:
                    fc.SetAUTarget(NkActionUnitId::AU25, 0.15f * intensity);
                    break;
                case NkViseme::Silence: default:
                    break;
            }
        }

        // =====================================================================
        // NkSpeechEngine
        // =====================================================================

        void NkSpeechEngine::Init() {
            logger.Infof("[NkSpeechEngine] Init\n");
        }

        void NkSpeechEngine::Speak(const NkSpeechRequest& req, NkFaceController& fc) {
            if (req.text.IsEmpty()) return;

            // Estimation durée : ~150ms/syllabe, ~2.5 syllabes/mot
            nk_usize words = 1;
            const char* s = req.text.CStr();
            for (nk_usize i = 0; s[i]; ++i) if (s[i] == ' ') ++words;
            nk_float32 dur = (nk_float32)words * 2.5f * 150.f / req.rateMult;

            auto frames = NkVisemeMapper::Map(req.text, dur);
            mLipSync.Play(frames);

            mCurrentPitch  = req.pitchMult;
            mCurrentRate   = req.rateMult;
            mCurrentBreath = req.breathAmt;

            logger.Infof("[NkSpeechEngine] Speak: \"{}\" ({:.0f}ms)\n",
                         req.text.CStr(), dur);
        }

        void NkSpeechEngine::Interrupt(NkFaceController& fc) {
            mLipSync.Stop();
            fc.SetAUTarget(NkActionUnitId::AU25, 0.f);
            fc.SetAUTarget(NkActionUnitId::AU26, 0.f);
        }

        void NkSpeechEngine::Update(nk_float32 dt, NkFaceController& fc) {
            mLipSync.Update(dt, fc);
        }

        NkString NkSpeechEngine::GetResponse(const NkString& question,
                                             nk_float32 pain,
                                             nk_float32 breath) {
            // Base de réponses simple — sera remplacée par BDD JSON Phase 6
            const char* q = question.CStr();

            if (strstr(q, "douleur") || strstr(q, "mal")) {
                if (pain >= 7.f)
                    return "J'ai très mal... ici, dans la poitrine... ça irradie dans le bras.";
                if (pain >= 4.f)
                    return "Oui, j'ai une douleur, dans la poitrine... plutôt forte.";
                return "Un peu... une gêne dans la poitrine.";
            }
            if (strstr(q, "respir") || strstr(q, "souffle")) {
                if (breath >= 0.6f)
                    return "J'ai... du mal à respirer. Même au repos...";
                if (breath >= 0.3f)
                    return "Ça me gêne un peu quand je fais des efforts.";
                return "Non, ça va pour la respiration.";
            }
            if (strstr(q, "depuis") || strstr(q, "quand")) {
                return "Depuis ce matin... vers huit heures peut-être.";
            }
            if (strstr(q, "antécédent") || strstr(q, "maladie")) {
                return "J'ai de l'hypertension... et du cholestérol, je prends des médicaments.";
            }
            return "Je ne sais pas trop...";
        }

    } // namespace pv3de
} // namespace nkentseu
