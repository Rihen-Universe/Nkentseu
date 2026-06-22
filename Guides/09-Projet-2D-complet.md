# Projet 2D complet — assembler tout Nkentseu (capstone)

> [← Retour à l'index](README.md)
>
> Dans ce dernier guide, on construit **un petit jeu 2D de A à Z** en assemblant les
> briques vues précédemment : [NKWindow](02-NKWindow.md) (fenêtre + boucle),
> [NKEvent](03-NKEvent.md) (entrées), [NKCanvas](05-NKCanvas.md) (cible de rendu),
> [NKUI](07-NKUI.md) (dessin 2D + texte), [NKImage](04-NKImage.md) (textures),
> [NKAudio](06-NKAudio.md) (sons) et [NKMemory](01-NKMemory.md) (mémoire).
>
> Le jeu : **« Attrape »** — un panier en bas de l'écran qu'on déplace pour attraper
> des objets qui tombent. Score affiché, son à chaque attrapé, musique de fond.

C'est exactement le squelette utilisé par les vrais jeux du dépôt (Mú, Pong) : une
fenêtre, une cible de rendu, et un **draw list NKUI** comme couche de dessin 2D
immédiate (rectangles, images, texte). C'est le chemin le plus court vers un jeu
jouable et multi-plateforme.

---

## 1. L'ossature

On choisit le modèle **direct** (le plus proche de SFML) : on gère nous-mêmes la boucle
dans `nkmain`. On encapsule tout dans une petite classe `Jeu`.

```cpp
// main.cpp
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "Jeu.h"

using namespace nkentseu;

NKENTSEU_DEFINE_APP_DATA(([]() {
    NkAppData d{};
    d.appName    = "Attrape";
    d.appVersion = "1.0.0";
    return d;
})());

int nkmain(const NkEntryState& state) {
    Jeu jeu;
    if (!jeu.Initialize()) return -1;
    return jeu.Run();
}
```

---

## 2. Mise en place : fenêtre + rendu + UI + audio

`Initialize()` crée tout dans l'ordre des couches : fenêtre → cible de rendu →
contexte UI + backend → audio.

```cpp
// Jeu.h (extrait)
#include "NKWindow/NKWindow.h"
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Core/NkGraphicsApi.h"
#include "NKUI/NKUI.h"
#include "NKCanvas/UI/NkUICanvasBackend.h"
#include "NKAudio/NKAudio.h"
#include "NKTime/NkClock.h"

using namespace nkentseu;

class Jeu {
public:
    bool Initialize();
    int  Run();
    ~Jeu();
private:
    void Update(float32 dt);
    void Render();

    NkWindow                         mWindow;
    renderer::NkRenderWindow*        mRT       = nullptr;
    nkui::NkUIContext*               mUI       = nullptr;
    renderer::NkUICanvasBackend*     mUIBack   = nullptr;
    nkui::NkUIFont*                  mFont     = nullptr;
    nkui::NkUIInputState             mUIInput;
    NkClock                          mClock;

    // état du jeu (voir §4)
    float32 mPanierX = 600.f;
    // ...
};
```

```cpp
// Jeu.cpp — Initialize()
bool Jeu::Initialize() {
    // 1) Fenêtre (NKWindow)
    NkWindowConfig cfg;
    cfg.title  = "Attrape !";
    cfg.width  = 1280;
    cfg.height = 720;
    cfg.centered = true;
    if (!mWindow.Create(cfg)) return false;

    // 2) Cible de rendu 2D (NKCanvas) — backend AUTO (DX11 sur Windows, GL ailleurs)
    NkContextDesc desc;
    desc.api = NkGraphicsApi::NK_GFX_API_AUTO;
    mRT = memory::NkGetDefaultAllocator().New<renderer::NkRenderWindow>(mWindow, desc);
    if (!mRT || !mRT->IsValid()) return false;

    // 3) UI immédiate (NKUI) + backend qui dessine via le renderer 2D
    nkui::NkUIFontConfig fc;
    fc.yAxisUp = false;
    fc.enableAtlas = true;
    fc.defaultFontSize = 28.f;
    mUI = memory::NkGetDefaultAllocator().New<nkui::NkUIContext>();
    mUI->Init((int32)cfg.width, (int32)cfg.height, fc);

    mUIBack = memory::NkGetDefaultAllocator().New<renderer::NkUICanvasBackend>();
    mUIBack->Init(mRT->GetRenderer());

    int32 fid = mUI->fontManager.LoadEmbedded(nkui::NkEmbeddedFontId::DroidSans, 48.f);
    mFont = mUI->fontManager.Get(fid < 0 ? 0 : (uint32)fid);

    // 4) Audio (NKAudio) — échoue proprement s'il n'y a pas de carte son
    audio::AudioEngine::Instance().Initialize();

    return true;
}
```

> Remarque mémoire : on alloue les sous-systèmes avec
> `memory::NkGetDefaultAllocator().New<T>()` — **jamais `new`** (voir
> [NKMemory](01-NKMemory.md)). On les libère avec `.Delete(...)` dans le destructeur.

```cpp
Jeu::~Jeu() {
    audio::AudioEngine::Instance().Shutdown();
    memory::NkGetDefaultAllocator().Delete(mUIBack);
    memory::NkGetDefaultAllocator().Delete(mUI);
    memory::NkGetDefaultAllocator().Delete(mRT);
    mWindow.Close();
}
```

---

## 3. La boucle principale

Le cœur de tout jeu : delta-time, événements, update, rendu. (Pour la mise en pause
sur perte de focus, voir l'astuce en fin de guide.)

```cpp
int Jeu::Run() {
    while (mWindow.IsOpen()) {
        const float32 dt = mClock.Tick().delta;     // secondes depuis la frame précédente

        // a) Entrées (NKEvent) -> on alimente l'input UI au passage
        mUIInput.BeginFrame();
        while (NkEvent* ev = NkEvents().PollEvent()) {
            if (auto* m = ev->As<NkMouseMoveEvent>()) {
                mPanierX = (float32)m->GetX();                 // le panier suit la souris
                mUIInput.SetMousePos((float32)m->GetX(), (float32)m->GetY());
            }
            else if (ev->Is<NkWindowCloseEvent>()) {
                // la fenêtre se ferme : IsOpen() repassera à false
            }
        }

        // b) Logique
        Update(dt);

        // c) Rendu
        Render();
    }
    return 0;
}
```

---

## 4. La logique du jeu

Un objet qui tombe, un panier, un score. Tout en coordonnées écran (pixels).

```cpp
// dans Jeu (membres)
float32 mObjetX = 400.f, mObjetY = -60.f;   // position de l'objet qui tombe
float32 mVitesse = 320.f;                    // px/s
int32   mScore   = 0;
uint32  mRng     = 1234u;

static const float32 PANIER_W = 160.f, PANIER_H = 40.f;
static const float32 OBJET_SZ = 56.f;

float32 Rand01() {   // PRNG simple, déterministe
    mRng = mRng * 1664525u + 1013904223u;
    return (float32)((mRng >> 8) & 0xFFFFFF) / 16777216.f;
}

void Jeu::Update(float32 dt) {
    const float32 H = (float32)mWindow.GetSize().y;
    const float32 W = (float32)mWindow.GetSize().x;

    mObjetY += mVitesse * dt;                 // chute

    const float32 panierY = H - PANIER_H - 24.f;
    // Attrapé ? (l'objet touche le niveau du panier ET est aligné)
    if (mObjetY + OBJET_SZ >= panierY &&
        mObjetX + OBJET_SZ > mPanierX - PANIER_W * 0.5f &&
        mObjetX < mPanierX + PANIER_W * 0.5f) {
        ++mScore;
        JouerBip(880.f);                       // son aigu = réussite (voir §6)
        Respawn(W);
    }
    else if (mObjetY > H) {                     // raté
        JouerBip(180.f);                        // son grave = raté
        Respawn(W);
    }
}

void Jeu::Respawn(float32 W) {
    mObjetX = Rand01() * (W - OBJET_SZ);
    mObjetY = -OBJET_SZ;
    mVitesse += 8.f;                            // ça accélère, c'est plus fun
}
```

---

## 5. Le rendu (NKCanvas + NKUI)

On efface l'écran, on remplit le **draw list** NKUI (formes + texte), puis on
`Submit` (NKUI → renderer 2D) et on présente la frame.

```cpp
void Jeu::Render() {
    const math::NkVec2u sz = mRT->GetSize();
    const float32 W = (float32)sz.x, H = (float32)sz.y;

    // 1) Effacer le fond (couleur RGBA 0-255)
    mRT->Clear(math::NkColor{ 30, 34, 48, 255 });

    // 2) Construire l'UI/2D de la frame
    mUIInput.dt = 0.f;                          // (le dt UI ne sert qu'aux animations UI)
    mUI->BeginFrame(mUIInput, 0.f);
    nkui::NkUIDrawList& dl = *mUI->dl;

    // l'objet qui tombe (carré orange arrondi)
    dl.AddRectFilled(math::NkFloatRect{ mObjetX, mObjetY, OBJET_SZ, OBJET_SZ },
                     math::NkColor{ 255, 170, 60, 255 }, 12.f, 12.f);

    // le panier (rectangle vert)
    const float32 px = mPanierX - PANIER_W * 0.5f;
    const float32 py = H - PANIER_H - 24.f;
    dl.AddRectFilled(math::NkFloatRect{ px, py, PANIER_W, PANIER_H },
                     math::NkColor{ 80, 200, 120, 255 }, 10.f, 10.f);

    // le score (texte) — RAPPEL : RenderText attend une BASELINE en y
    if (mFont) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "Score : %d", mScore);
        const float32 topY = 24.f;
        mFont->RenderText(dl, math::NkVec2f{ 24.f, topY + mFont->metrics.ascender },
                          buf, math::NkColor{ 255, 255, 255, 255 });
    }

    mUI->EndFrame();

    // 3) Envoyer la draw list au GPU (upload auto des atlas de police)
    mUIBack->Submit(*mUI, sz.x, sz.y);

    // 4) Présenter la frame à l'écran
    mRT->Display();                            // (selon la version : Present())
}
```

> Pourquoi NKUI pour dessiner le jeu ? Parce que le draw list immédiat
> (`AddRectFilled`, `AddImage`, `RenderText`) est le moyen le plus simple et le plus
> portable de poser des formes/textures/texte 2D. Pour des sprites avec
> transformations riches, formes vectorielles ou render-to-texture, utilise plutôt les
> objets de [NKCanvas](05-NKCanvas.md) (`NkSprite`, `NkRectangleShape`, `NkText`,
> `NkVertexArray`) directement sur la `NkRenderWindow`.

### 5.1 Variante : un vrai sprite texturé

Pour remplacer le carré orange par une image (une pomme, par ex.) :

```cpp
// chargement (une fois, dans Initialize) — voir guides NKImage + NKCanvas
NkImage img;
img.Load("assets/pomme.png", 4);
uint32 texPomme = mUIBack->UploadTextureRGBA8(img.Pixels(), img.Width(), img.Height());
// (ou passe par NkTexture si tu dessines via NkSprite sur la NkRenderWindow)

// rendu
dl.AddImage(texPomme, math::NkFloatRect{ mObjetX, mObjetY, OBJET_SZ, OBJET_SZ },
            math::NkVec2f{0,0}, math::NkVec2f{1,1}, math::NkColor{255,255,255,255});
```

Voir [NKImage](04-NKImage.md) (chargement, SVG multi-DPI) et la section textures de
[NKCanvas](05-NKCanvas.md).

---

## 6. Le son (NKAudio)

On génère les bips **en code** (pas de fichiers nécessaires) avec `AudioGenerator`, et
on les garde vivants tant qu'ils jouent. Une vraie banque de sons + musique est décrite
dans [NKAudio](06-NKAudio.md).

```cpp
// membres : on garde les samples vivants (sinon coupure pendant la lecture)
audio::AudioSample mBip;

void Jeu::JouerBip(float32 frequence) {
    using namespace audio;
    // libère le précédent puis génère un nouveau ton court
    if (mBip.IsValid()) AudioLoader::Free(mBip);
    mBip = AudioGenerator::GenerateTone(frequence, 0.10f, WaveformType::SINE, 44100, 0.5f);
    AdsrEnvelope env; env.attackTime = 0.005f; env.decayTime = 0.04f;
    env.sustainLevel = 0.3f; env.releaseTime = 0.05f;
    AudioGenerator::ApplyEnvelope(mBip, env);

    VoiceParams p; p.volume = 0.8f; p.bus = "SFX";
    AudioEngine::Instance().Play(mBip, p);
}
```

> Pour une **musique de fond** en boucle : charge un `.mp3/.ogg` avec
> `AudioLoader::Load`, garde le `AudioSample` membre vivant, et joue-le avec
> `VoiceParams{ looping = true, bus = "Music" }`. Détails et volume dans
> [NKAudio](06-NKAudio.md).

---

## 7. Un bouton « Rejouer » (NKUI)

Au game-over (ou en pause), un bouton suffit. Voici le motif complet de bouton enfant
(rect + label centré + détection de clic via l'input), tiré de [NKUI](07-NKUI.md) :

```cpp
bool Bouton(nkui::NkUIDrawList& dl, nkui::NkUIFont* font, const nkui::NkUIInputState& in,
            float32 x, float32 y, float32 w, float32 h, const char* label) {
    const float32 mx = in.MouseX(), my = in.MouseY();
    const bool survol = (mx >= x && mx < x+w && my >= y && my < y+h);
    dl.AddRectFilled(math::NkFloatRect{x,y,w,h},
                     survol ? math::NkColor{255,200,60,255} : math::NkColor{240,240,240,255}, 16.f, 16.f);
    if (font && label) {
        const float32 tw = font->MeasureWidth(label);
        font->RenderText(dl, math::NkVec2f{ x + (w-tw)*0.5f, y + (h - font->metrics.lineHeight)*0.5f + font->metrics.ascender },
                         label, math::NkColor{40,40,40,255});
    }
    return survol && in.IsMouseReleased(0);    // true au relâché sur le bouton
}
```

---

## 8. Mettre le jeu en pause quand la fenêtre perd le focus

Bonne pratique (et attendue sur mobile) : quand la fenêtre n'a plus le focus, on **fige
le jeu et l'audio**. On écoute `NkWindowFocusLost/GainedEvent` (voir [NKEvent](03-NKEvent.md)) :

```cpp
bool mPaused = false;
// abonnement (dans Initialize) :
NkEvents().AddEventCallback<NkWindowFocusLostEvent>([this](NkWindowFocusLostEvent*){
    mPaused = true;
    audio::AudioEngine::Instance().SetMasterVolume(0.f);   // ou Pause des voix
});
NkEvents().AddEventCallback<NkWindowFocusGainedEvent>([this](NkWindowFocusGainedEvent*){
    mPaused = false;
    audio::AudioEngine::Instance().SetMasterVolume(1.f);
});

// dans Run(), après le poll :
if (mPaused) { NkChrono::Sleep((int64)16); continue; }     // on fige + on relâche le CPU
```

---

## 9. Compiler

Le `*.jenga` du projet déclare les modules utilisés :

```python
with project("Attrape"):
    kind("app")
    files(["src/**.cpp"])
    nkentseudependson(
        ["NKWindow", "NKEvent", "NKCanvas", "NKUI", "NKImage", "NKAudio", "NKFont",
         "NKMemory", "NKCore", "NKMath", "NKTime", "NKLogger"],
        extra_includes=["src"],
    )
    # Liens audio par plateforme (voir guide NKAudio) :
    #   Windows : avrt, winmm   |   Android : OpenSLES, dl   |   Linux : asound
```

```
jenga build --target Attrape --config Debug
jenga build --target Attrape --platform android --config Release
```

---

## 10. Récap & pistes

Tu as un jeu 2D complet et multi-plateforme : fenêtre, boucle, entrées, rendu de
formes/textures/texte, son, score, pause sur focus. Les briques :

| Besoin | Module | Guide |
|--------|--------|-------|
| Fenêtre + boucle + entrée | NKWindow / NKEvent | [02](02-NKWindow.md) · [03](03-NKEvent.md) |
| Dessin 2D (cible + sprites/formes/texte) | NKCanvas | [05](05-NKCanvas.md) |
| Dessin immédiat + UI + texte | NKUI | [07](07-NKUI.md) |
| Textures / images | NKImage | [04](04-NKImage.md) |
| Son & musique | NKAudio | [06](06-NKAudio.md) |
| Mémoire (jamais `new`) | NKMemory | [01](01-NKMemory.md) |
| Multijoueur | NKNetwork | [08](08-NKNetwork.md) |

Pistes pour aller plus loin : plusieurs objets simultanés (un `NkVector`), des sprites
animés, des particules à l'attrapé, un menu et un écran game-over (scènes), des niveaux
(JSON via NKSerialization), un mode 2 joueurs en réseau ([NKNetwork](08-NKNetwork.md)).

[← Retour à l'index](README.md)
