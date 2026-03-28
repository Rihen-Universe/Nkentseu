#pragma once

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Animation helper declarations and value smoothing interfaces.
 * Main data: Animation utility API used by widgets/window effects.
 * Change this file when: You introduce a new animation primitive or API.
 */
/**
 * @File    NkUIAnimation.h
 * @Brief   Système d'animations — easing, transitions, keyframes.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  NkUIAnimator gère un pool de NkUITween — chaque tween interpole
 *  une valeur float32 de start vers end sur une durée donnée avec
 *  une fonction d'easing choisie.
 *
 *  Usage :
 *    // One-shot : démarre une animation, retourne la valeur courante
 *    float32 v = animator.Play("btn_hover", 0.f, 1.f, 0.15f, NkEase::NK_OUT_QUAD);
 *
 *    // Valeur courante d'une animation existante
 *    float32 v = animator.Get("btn_hover");
 *
 *    // Arrêt immédiat
 *    animator.Stop("btn_hover");
 *
 *  Effets prêts à l'emploi :
 *    Shake     — oscillation rapide (erreur, alerte)
 *    Pulse     — battement (attirer l'attention)
 *    Bounce    — rebond à l'atterrissage
 *    FadeIn/Out— fondu entrée/sortie
 *    SlideIn   — glissement depuis un bord
 */
#include "NkUI/NkUIExport.h"
namespace nkentseu {
    namespace nkui {
        // ─────────────────────────────────────────────────────────────────────────────
        //  Fonctions d'easing
        // ─────────────────────────────────────────────────────────────────────────────
        enum class NkEase : uint8 {
            NK_LINEAR=0,
            NK_IN_QUAD, NK_OUT_QUAD, NK_IN_OUT_QUAD,
            NK_IN_CUBIC, NK_OUT_CUBIC, NK_IN_OUT_CUBIC,
            NK_IN_QUART, NK_OUT_QUART, NK_IN_OUT_QUART,
            NK_IN_SINE, NK_OUT_SINE, NK_IN_OUT_SINE,
            NK_IN_EXPO, NK_OUT_EXPO, NK_IN_OUT_EXPO,
            NK_IN_ELASTIC, NK_OUT_ELASTIC, NK_IN_OUT_ELASTIC,
            NK_IN_BOUNCE, NK_OUT_BOUNCE, NK_IN_OUT_BOUNCE,
            NK_IN_BACK, NK_OUT_BACK, NK_IN_OUT_BACK,
            NK_COUNT
        };

        struct NKUI_API NkUIEasing {
            /// Applique une fonction d'easing à t ∈ [0,1] → retourne une valeur transformée
            static float32 Apply(NkEase ease, float32 t) noexcept;

            // Fonctions individuelles (accessibles directement)
            static float32 Linear   (float32 t) noexcept { return t; }
            static float32 InQuad   (float32 t) noexcept { return t*t; }
            static float32 OutQuad  (float32 t) noexcept { return t*(2-t); }
            static float32 InOutQuad(float32 t) noexcept { return t<0.5f?2*t*t:(-2*t*t+4*t-1); }
            static float32 InCubic  (float32 t) noexcept { return t*t*t; }
            static float32 OutCubic (float32 t) noexcept { const float32 s=t-1; return s*s*s+1; }
            static float32 InOutCubic(float32 t)noexcept { return t<0.5f?4*t*t*t:(t-1)*(2*t-2)*(2*t-2)+1; }
            static float32 OutElastic(float32 t)noexcept {
                if(t<=0)return 0;if(t>=1)return 1;
                return ::powf(2,-10*t)*::sinf((t*10-0.75f)*2*NKUI_PI/3)+1;}
            static float32 OutBounce(float32 t) noexcept {
                const float32 n=7.5625f, d=2.75f;
                if(t<1/d)       return n*t*t;
                if(t<2/d)       { const float32 t2=t-1.5f/d;   return n*t2*t2+0.75f; }
                if(t<2.5f/d)    { const float32 t2=t-2.25f/d;  return n*t2*t2+0.9375f; }
                const float32 t2=t-2.625f/d; return n*t2*t2+0.984375f; }
            static float32 OutBack  (float32 t) noexcept {
                const float32 c=1.70158f,c2=c+1;
                return 1+c2*(t-1)*(t-1)*(t-1)+c*(t-1)*(t-1); }
        };

        // ─────────────────────────────────────────────────────────────────────────────
        //  NkUITween — une animation en cours
        // ─────────────────────────────────────────────────────────────────────────────
        struct NkUITween {
            NkUIID   id       = 0;
            float32  start    = 0.f;
            float32  end      = 1.f;
            float32  duration = 0.2f;
            float32  elapsed  = 0.f;
            NkEase   ease     = NkEase::NK_OUT_QUAD;
            bool     looping  = false;
            bool     pingPong = false;
            bool     active   = false;
            float32  value    = 0.f;  // valeur courante calculée

            void Step(float32 dt) noexcept {
                if(!active) return;
                elapsed+=dt;
                float32 t=duration>0?elapsed/duration:1.f;
                if(t>1.f){
                    if(looping){ elapsed=::fmodf(elapsed,duration); t=elapsed/duration; }
                    else if(pingPong&&(static_cast<int32>(elapsed/duration)%2==1)){
                        t=1.f-(t-1.f); elapsed=::fmodf(elapsed,duration);
                    } else { t=1.f; active=!looping&&!pingPong; }
                }
                value=start+(end-start)*NkUIEasing::Apply(ease,t);
            }

            bool IsDone() const noexcept { return !active&&!looping&&!pingPong; }
        };

        // ─────────────────────────────────────────────────────────────────────────────
        //  NkUIAnimator — pool de tweens
        // ─────────────────────────────────────────────────────────────────────────────
        struct NKUI_API NkUIAnimator {
            static constexpr int32 MAX_TWEENS = 256;
            NkUITween tweens[MAX_TWEENS] = {};
            int32     numTweens          = 0;

            /// Met à jour tous les tweens actifs
            void Update(float32 dt) noexcept {
                for(int32 i=0;i<numTweens;++i)
                    if(tweens[i].active) tweens[i].Step(dt);
            }

            /// Démarre ou redémarre une animation. Retourne la valeur courante.
            float32 Play(NkUIID id,float32 from,float32 to,float32 duration,
                        NkEase ease=NkEase::NK_OUT_QUAD,bool loop=false,bool pingPong=false) noexcept
            {
                NkUITween* t=Find(id);
                if(!t){ if(numTweens>=MAX_TWEENS) return to; t=&tweens[numTweens++]; }
                t->id=id; t->start=from; t->end=to; t->duration=duration;
                t->elapsed=0; t->ease=ease; t->active=true;
                t->looping=loop; t->pingPong=pingPong; t->value=from;
                return from;
            }

            /// Continue vers une nouvelle cible depuis la position courante
            float32 Toward(NkUIID id,float32 target,float32 duration,
                            NkEase ease=NkEase::NK_OUT_QUAD) noexcept
            {
                NkUITween* t=Find(id);
                const float32 cur=t?t->value:target;
                return Play(id,cur,target,duration,ease);
            }

            /// Valeur courante (0 si non trouvé)
            float32 Get(NkUIID id) const noexcept {
                for(int32 i=0;i<numTweens;++i) if(tweens[i].id==id) return tweens[i].value;
                return 0.f;
            }
            float32 Get(NkUIID id,float32 def) const noexcept {
                for(int32 i=0;i<numTweens;++i) if(tweens[i].id==id) return tweens[i].value;
                return def;
            }

            void Stop(NkUIID id) noexcept {
                NkUITween* t=Find(id); if(t){t->active=false;}
            }
            void StopAll() noexcept { for(int32 i=0;i<numTweens;++i) tweens[i].active=false; }

            bool IsPlaying(NkUIID id) const noexcept {
                for(int32 i=0;i<numTweens;++i) if(tweens[i].id==id) return tweens[i].active;
                return false;
            }

            // ── Effets prêts à l'emploi ───────────────────────────────────────────────

            /// Effet shake horizontal (erreur, alerte)
            NkVec2 Shake(NkUIID id,float32 intensity=5.f,float32 duration=0.4f) noexcept {
                if(!IsPlaying(id)) Play(id,0.f,1.f,duration,NkEase::NK_OUT_QUAD);
                const float32 t=Get(id); if(t<=0) return {};
                const float32 decay=1.f-Get(id);
                const float32 freq=20.f;
                return {intensity*decay*::sinf(t*freq*NKUI_PI),
                        intensity*0.3f*decay*::sinf(t*freq*1.3f*NKUI_PI)};
            }

            /// Effet pulse (attirer l'attention) — retourne un scale [0.95, 1.05]
            float32 Pulse(NkUIID id,float32 amplitude=0.05f,float32 period=0.8f) noexcept {
                if(!IsPlaying(id)) Play(id,0.f,1.f,period,NkEase::NK_LINEAR,true);
                const float32 t=Get(id);
                return 1.f+amplitude*::sinf(t*2*NKUI_PI);
            }

            /// FadeIn : retourne opacité 0→1
            float32 FadeIn(NkUIID id,float32 duration=0.2f) noexcept {
                if(!IsPlaying(id)) Play(id,0.f,1.f,duration,NkEase::NK_OUT_QUAD);
                return Get(id);
            }

            /// FadeOut : retourne opacité 1→0
            float32 FadeOut(NkUIID id,float32 duration=0.2f) noexcept {
                if(!IsPlaying(id)) Play(id,1.f,0.f,duration,NkEase::NK_IN_QUAD);
                return Get(id,1.f);
            }

            /// SlideIn depuis le haut — retourne offset Y (de -h → 0)
            float32 SlideInFromTop(NkUIID id,float32 h,float32 duration=0.25f) noexcept {
                if(!IsPlaying(id)) Play(id,-h,0.f,duration,NkEase::NK_OUT_BACK);
                return Get(id);
            }

            /// Bounce à l'atterrissage — retourne offset Y (de -h → 0)
            float32 Bounce(NkUIID id,float32 h=20.f,float32 duration=0.4f) noexcept {
                if(!IsPlaying(id)) Play(id,-h,0.f,duration,NkEase::NK_OUT_BOUNCE);
                return Get(id);
            }

        private:
            NkUITween* Find(NkUIID id) noexcept {
                for(int32 i=0;i<numTweens;++i) if(tweens[i].id==id) return &tweens[i];
                return nullptr;
            }
        };
    }
} // namespace nkentseu
