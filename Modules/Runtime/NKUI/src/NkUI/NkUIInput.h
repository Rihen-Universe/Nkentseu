#pragma once

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Frame input state abstraction for keyboard/mouse/text input.
 * Main data: Button/key transitions, mouse wheel, typed chars.
 * Change this file when: Input events mapping or capture semantics evolve.
 */
/**
 * @File    NkUIInput.h
 * @Brief   Abstraction input clavier/souris/touch — platform-agnostic.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  L'utilisateur remplit NkUIInputState à chaque frame depuis son
 *  propre gestionnaire d'événements (NKEvent, SDL, GLFW, Win32…).
 *  NkUI utilise les types de NKEvent (NkKey, NkMouseButton).
 *
 *  Workflow par frame :
 *    state.BeginFrame();               // remet les deltas à zéro
 *    state.SetMousePos(x, y);          // depuis l'événement platform
 *    state.SetMouseButton(0, pressed); // bouton gauche
 *    state.AddInputChar(codepoint);    // caractère tapé
 *    state.SetKey(NkKey::NK_TAB, true);   // touche enfoncée
 *    ctx.Feed(state);                  // transmis au contexte UI
 */
#include "NKUI/NkUIExport.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"

namespace nkentseu {
    namespace nkui {

        // ─────────────────────────────────────────────────────────────────────────────
        //  Codes de touches — on utilise directement nkentseu::NkKey (NKEvent)
        // ─────────────────────────────────────────────────────────────────────────────
        using NkKey         = nkentseu::NkKey;
        using NkMouseButton = nkentseu::NkMouseButton;

        // ─────────────────────────────────────────────────────────────────────────────
        //  État input d'une frame
        // ─────────────────────────────────────────────────────────────────────────────
        struct NKUI_API NkUIInputState {
            // Souris
            NkVec2  mousePos       = {};
            NkVec2  mousePosLast   = {};
            NkVec2  mouseDelta     = {};
            float32 mouseWheel     = 0.f;
            float32 mouseWheelH    = 0.f;
            bool    mouseDown[5]   = {};   // boutons 0-4
            bool    mouseClicked[5]= {};   // click cette frame
            bool    mouseReleased[5]={};
            bool    mouseDblClick[5]={};
            float32 mouseDownDuration[5]={};

            // Clavier
            bool    keyDown[static_cast<int32>(NkKey::NK_KEY_MAX)] = {};
            bool    keyPressed[static_cast<int32>(NkKey::NK_KEY_MAX)]={};
            bool    keyReleased[static_cast<int32>(NkKey::NK_KEY_MAX)]={};
            bool    ctrl=false, shift=false, alt=false;

            // Texte saisi (codepoints UTF-32)
            uint32  inputChars[32]  = {};
            int32   numInputChars   = 0;

            // Touch (mobile/tablette)
            struct Touch { NkVec2 pos; bool down; };
            Touch   touches[10]     = {};
            int32   numTouches      = 0;

            // Delta temps
            float32 dt              = 0.016f; // secondes depuis la dernière frame

            // ── Méthodes helper ──────────────────────────────────────────────────────
            void BeginFrame() noexcept {
                mouseDelta={mousePos.x-mousePosLast.x, mousePos.y-mousePosLast.y};
                for(int32 i=0;i<5;++i){mouseClicked[i]=false;mouseReleased[i]=false;mouseDblClick[i]=false;}
                for(int32 i=0;i<(int32)NkKey::NK_KEY_MAX;++i){keyPressed[i]=false;keyReleased[i]=false;}
                numInputChars=0;
                mouseWheel=0.f; mouseWheelH=0.f;
            }
            void SetMousePos(float32 x,float32 y) noexcept {
                mousePosLast=mousePos; mousePos={x,y};
                mouseDelta={x-mousePosLast.x,y-mousePosLast.y};
            }
            void SetMouseButton(int32 btn,bool down) noexcept {
                if(btn<0||btn>=5) return;
                if(down&&!mouseDown[btn]) mouseClicked[btn]=true;
                if(!down&&mouseDown[btn]) mouseReleased[btn]=true;
                mouseDown[btn]=down;
            }
            void AddMouseWheel(float32 dy,float32 dx=0) noexcept {mouseWheel+=dy;mouseWheelH+=dx;}
            void SetKey(NkKey key,bool down) noexcept {
                const int32 k=static_cast<int32>(key);
                if(k<=0||k>=(int32)NkKey::NK_KEY_MAX) return;
                if(down&&!keyDown[k]) keyPressed[k]=true;
                if(!down&&keyDown[k]) keyReleased[k]=true;
                keyDown[k]=down;
                if(key==NkKey::NK_LCTRL ||key==NkKey::NK_RCTRL)  ctrl=down;
                if(key==NkKey::NK_LSHIFT||key==NkKey::NK_RSHIFT) shift=down;
                if(key==NkKey::NK_LALT  ||key==NkKey::NK_RALT)   alt=down;
            }
            void AddInputChar(uint32 c) noexcept {
                if(numInputChars<32) inputChars[numInputChars++]=c;
            }
            bool IsKeyDown(NkKey k)     const noexcept{return keyDown[static_cast<int32>(k)];}
            bool IsKeyPressed(NkKey k)  const noexcept{return keyPressed[static_cast<int32>(k)];}
            bool IsMouseDown(int32 b=0) const noexcept{return b<5&&mouseDown[b];}
            bool IsMouseClicked(int32 b=0)  const noexcept{return b<5&&mouseClicked[b];}
            bool IsMouseReleased(int32 b=0) const noexcept{return b<5&&mouseReleased[b];}
        };
    }
} // namespace nkentseu
