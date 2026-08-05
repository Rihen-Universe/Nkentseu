# Construire une application, de zéro à l'exécutable

Nous avons vu la pile (chapitre 1), le rendu (chapitre 2) et les widgets
(chapitre 3). Il reste à tout assembler. Ce chapitre construit une application
complète, dans l'ordre, en expliquant à chaque étape *pourquoi* elle est là
et *ce qui casse* si on l'oublie.

À la fin, vous aurez un programme qui compile, se lance, affiche une interface et
répond à la souris et au clavier.

## Vue d'ensemble du squelette

Avant le détail, voici la carte. Une application NKGui sur NkCanvas se construit
en sept étapes, puis boucle.

**`Les sept étapes de l'initialisation`**

```
1. FENETRE          NkWindow + NkWindowConfig
2. CONTEXTE GPU     NkContextDesc  (choix de l'API graphique)
3. CIBLE DE RENDU   NkRenderWindow (possede le cycle de frame)
4. CONTEXTE UI      NkGuiContext::Init + SetCurrentContext
5. BACKEND (PONT)   NkGuiCanvasBackend::Init(target->GetRenderer())
6. POLICE           NkGuiFont::LoadEmbedded  PUIS  backend.UploadFontGray8
7. ENTREE           callbacks NKEvent -> ctx.input

BOUCLE :
   evenements -> [resize] -> ctx.BeginFrame(dt) -> widgets -> ctx.EndFrame()
   -> target->Clear() -> backend.Submit(ctx.dl) -> backend.Submit(ctx.dlOverlay)
   -> target->Display()
```

L'ordre n'est pas indifférent. Le pont a besoin du renderer, donc de la cible.
L'upload de l'atlas a besoin du pont *et* de la police chargée. Les
*callbacks* ont besoin du contexte, puisqu'ils écrivent dedans.

## Étape 1 — la fenêtre

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:243-251`**

```cpp
    NkWindow window;
    NkWindowConfig cfg;
    cfg.title = "NKGui - Demo Phase 2 (Coeur : drawlist + interaction)";
    cfg.width = 1100;
    cfg.height = 800;
    cfg.centered = true;
    cfg.resizable = true;
    if (!window.Create(cfg))
        return -1;
```

Rien de subtil, mais notez le test de retour : `Create` renvoie
`false` si la fenêtre n'a pas pu être créée, et il n'y a pas d'exception à
attraper. Chaque étape de cette initialisation suit le même modèle — retour
booléen ou méthode `IsValid()` — et chacune doit être testée.

Les en-têtes nécessaires :

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:7-11 (extrait)`**

```cpp
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
```

## Étape 2 et 3 — contexte GPU et cible de rendu

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:253-265`**

```cpp
    NkContextDesc desc;
    desc.api = NkGraphicsApi::NK_GFX_API_AUTO;
    if (desc.api == NkGraphicsApi::NK_GFX_API_AUTO) {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
        desc.api = NkGraphicsApi::NK_GFX_API_DX11;
#else
        desc.api = NkGraphicsApi::NK_GFX_API_OPENGL;
#endif
    }
    auto target = memory::NkMakeUnique<NkRenderWindow>(window, desc);
    if (!target || !target->IsValid())
        return -1;
```

Trois observations :

- `NK_GFX_API_AUTO` est résolu ici, à la main, par une directive
  de préprocesseur. On aurait pu utiliser
  `NkContextFactory::CreateWithFallback` et laisser le moteur
  essayer une liste ordonnée ; la démo préfère être explicite.
- La cible est créée par `memory::NkMakeUnique` et vivra jusqu'à la
  fin de `nkmain`. Elle possède le contexte graphique *et* le
  renderer 2D.
- Le double test `!target || !target->IsValid()` couvre les deux
  modes d'échec : l'allocation, et l'initialisation GPU.

Les en-têtes correspondants :

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:12-16 (extrait)`**

```cpp
#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Core/NkGraphicsApi.h"
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKTime/NkClock.h"
#include "NKMemory/NkUniquePtr.h"
```

> **✅ Ce qu'il faut retenir**
>
> Si votre fenêtre reste noire alors que le programme tourne, changez d'API
> graphique avant de chercher dans votre code. Rappel du chapitre 1 : seul OpenGL
> était validé à l'exécution au 30 mai 2026 selon
> `Kernel/Runtime/NKCanvas/ROADMAP.md`, et DX11 comme le rasteriseur
> logiciel affichaient un écran vide. Le journal
> (`logs/app.log`) vous dit quel backend a réellement été retenu.

## Étape 4 — le contexte NKGui

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:266-272`**

```cpp
    auto ctxPtr = memory::NkMakeUnique<NkGuiContext>();
    if (!ctxPtr)
        return -1;
    NkGuiContext &ctx = *ctxPtr;
    ctx.Init(static_cast<int32>(cfg.width), static_cast<int32>(cfg.height));
    ctx.styleFn = &DemoStyle; // hook de style (override de dessin par widget)
    SetCurrentContext(&ctx);
```

Le contexte est volumineux — plus de cent champs et trente-deux listes de dessin
de fenêtres — ce qui explique qu'on l'alloue plutôt que de le poser sur la pile.
On prend ensuite une référence, ce qui rend tout le code des widgets lisible.

`Init(width, height)` pose la taille de vue initiale et prépare les
structures. `SetCurrentContext` installe le contexte courant du fil
d'exécution ; la ligne `ctx.styleFn` est facultative.

C'est ici, juste après `Init`, que vous personnaliserez le thème :

**`Personnalisation du thème — écrit pour ce cours, d'après NkEditorShell.cpp:155-179`**

```cpp
    ctx.Init(1100, 800);
    ctx.theme.bgPrimary = {13, 17, 23, 255};
    ctx.theme.panel     = {22, 27, 34, 255};
    ctx.theme.accent    = {88, 166, 255, 255};
    ctx.theme.rounding  = 6.f;
    SetCurrentContext(&ctx);
```

## Étape 5 — le pont vers NkCanvas

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:274-276`**

```cpp
    renderer::NkGuiCanvasBackend backend;
    if (!backend.Init(target->GetRenderer()))
        return -1;
```

Deux lignes, et c'est tout le mariage entre les deux modules. L'en-tête à inclure
est celui qui vit dans NkCanvas :

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:25`**

```cpp
#include "NKCanvas/UI/NkGuiCanvasBackend.h" // backend NKGui->NKCanvas (lib reutilisable)
```

Rappelez-vous du chapitre 1 : ce fichier est *header-only*. Il n'est compilé
nulle part ailleurs que dans votre propre unité de traduction. C'est pour cela
que votre `.jenga` doit dépendre à la fois de `NKGui` et de
`NKCanvas`.

`backend` est déclaré sur la pile de `nkmain`. Son destructeur
libère les textures qu'il a créées :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/UI/NkGuiCanvasBackend.h:24-32`**

```cpp
~NkGuiCanvasBackend() {
    auto &alloc = nkentseu::memory::NkGetDefaultAllocator();
    for (uint32 i = 0; i < mFonts.Size();  ++i) if (mFonts[i].tex)  alloc.Delete(mFonts[i].tex);
    for (uint32 i = 0; i < mImages.Size(); ++i) if (mImages[i].tex) alloc.Delete(mImages[i].tex);
}
```

> **⚠️ L'ordre de destruction**
>
> Le pont détruit des textures GPU, donc il doit mourir *avant* le renderer
> qui les a créées. Comme `backend` est déclaré *après* `target`
> dans `nkmain`, il est détruit *avant* — les objets locaux se
> détruisent dans l'ordre inverse de leur déclaration. Cet ordre est correct par
> construction ; inverser les deux déclarations produirait une libération de
> textures sur un renderer déjà mort.

## Étape 6 — la police, et l'upload de son atlas

C'est l'étape où l'on se trompe le plus souvent, parce qu'elle est en deux temps
et que rien ne signale l'oubli du second.

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:281-290`**

```cpp
    auto fontPtr = memory::NkMakeUnique<NkGuiFont>();
    if (!fontPtr->LoadEmbedded(NkEmbeddedFontId::DroidSans, 18.f)) {
        fontPtr->LoadEmbedded(NkEmbeddedFontId::ProggyClean, 16.f);
    }
    ctx.font = fontPtr.Get();
    if (fontPtr->Valid()) {
        backend.UploadFontGray8(fontPtr->TexId(), fontPtr->pixels, fontPtr->atlasW, fontPtr->atlasH);
    }
```

### Ce que fait `LoadEmbedded`

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiFont.h:19-40 (abrégé)`**

```cpp
struct NKENTSEU_NKGUI_CLASS_EXPORT NkGuiFont {
        NkFontAtlas atlas;          ///< possède la texture + les glyphes
        NkFont *face = nullptr;     ///< face produite (détenue par l'atlas)
        uint32 texId = 0x4E4B4654u; ///< 'NKFT' — id stable pour le backend
        uint8 *pixels = nullptr;    ///< atlas alpha8 (détenu par l'atlas)
        int32 atlasW = 0;
        int32 atlasH = 0;
        bool dirty = false;         ///< à (ré)uploader côté backend

        NkGuiFont() = default;
        NkGuiFont(const NkGuiFont &) = delete;            // NON COPIABLE
        NkGuiFont &operator=(const NkGuiFont &) = delete;

        bool LoadEmbedded(NkEmbeddedFontId id, float32 sizePx, bool extFallback = true) noexcept;
        bool LoadFromFile(const char *path, float32 sizePx, bool extFallback = true) noexcept;
```

L'appel rasterise les glyphes dans un atlas et laisse `pixels` pointer sur
un bitmap en niveaux de gris (un octet par pixel, l'alpha). **Cet atlas est
en mémoire centrale ; le GPU ne le connaît pas encore.**

Les polices embarquées disponibles
(`Kernel/Runtime/NKFont/src/NKFont/Embedded/NkFontEmbedded.h:49-67`) :
`ProggyClean`, `ProggyTiny` (bitmap monospace),
`DroidSans`, `Karla`, `Roboto` (sans-serif),
`Cousine`, `SourceCodePro` (monospace vectorielles),
`DroidSerif`, `DejaVuSansMono`.

### Ce que fait `UploadFontGray8`

Le pont convertit l'atlas en RGBA — blanc partout, l'alpha portant la forme du
glyphe — et crée la texture GPU :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/UI/NkGuiCanvasBackend.h:47-55`**

```cpp
const usize n = (usize)w * (usize)h;
mExpand.Resize(n * 4u);
uint8 *d = mExpand.Data();
for (usize i = 0; i < n; ++i) {
    d[i*4+0] = 255u; d[i*4+1] = 255u; d[i*4+2] = 255u; d[i*4+3] = gray[i];
}
```

C'est là que le `texId` prend son sens : le pont range la texture dans une
table indexée par cet identifiant, et `Submit` la retrouvera quand une
commande de dessin portera le même. La constante par défaut est
`0x4E4B4654` — les quatre lettres « NKFT ». Elle n'est **jamais
nulle**, car zéro est réservé à la texture blanche interne côté RHI.

> **⚠️ Oublier l'upload : du texte parfaitement invisible**
>
> Si vous chargez la police et posez `ctx.font` mais oubliez
> `UploadFontGray8`, il ne se passe *rien* de visible et
> *aucun* message n'apparaît. Les widgets se dessinent (leurs rectangles sont
> là), le layout est correct — mais chaque commande de texte référence un
> `texId` que le pont ne connaît pas, et elle est silencieusement ignorée.
>
> Symptôme : des boutons vides, des cases à cocher sans étiquette, un panneau au
> titre absent. Diagnostic : vérifiez que `fontPtr->Valid()` est vrai, et
> que l'upload est bien appelé après le chargement.

> **⚠️ La police ne doit jamais mourir avant la boucle**
>
> **`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiContext.h:141-142 (sens)`**
>
> ```cpp
> NkGuiFont *font = nullptr;      ///< pointeur BRUT, NON POSSEDE
> NkGuiFont *codeFont = nullptr;  ///< idem
> ```
>
> `ctx.font = fontPtr.Get()` donne au contexte un pointeur *brut*. Le
> contexte ne possède rien. Si l'objet `NkGuiFont` est détruit — parce qu'il
> était local à une fonction d'initialisation, par exemple — le contexte pointe sur
> de la mémoire libérée et la première mesure de texte plante.
>
> La règle : **l'objet police doit vivre au moins aussi longtemps que la
> boucle**. La démo le tient dans un `NkUniquePtr` déclaré dans
> `nkmain`, à côté de tout le reste.

> **⚠️ Recharger un atlas à une autre taille**
>
> **`Kernel/Runtime/NKCanvas/src/NKCanvas/UI/NkGuiCanvasBackend.h:65-66`**
>
> ```cpp
> // (Re)créer la texture si elle n'existe pas OU si la taille change (rechargement
> // de police à une autre taille = zoom / DPI). Sinon Update() déborderait l'ancienne taille.
> ```
>
> Le pont gère ce cas pour vous. Mais **vous** devez veiller à ne jamais
> recharger une police au milieu d'une image : les commandes déjà émises
> référenceraient un atlas remplacé, et leurs coordonnées de texture ne
> correspondraient plus. Faites-le avant `BeginFrame`, comme la démo le fait
> pour la touche F9.

### Les polices de repli

Pour les alphabets que la police principale ne couvre pas :

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiFont.h:71-78`**

```cpp
// Polices de REPLI EXTERNES (fichiers .ttf charges au runtime) : tout glyphe
// absent des polices principales (Inter/DejaVu) y est cherche. Roles :
//   broad : large couverture (latin etendu, grec, cyrillique, hebreu, arabe, symboles)
//   cjk   : ideogrammes (chinois/japonais/coreen) - volumineux, opt-in
//   emoji : emoji monochrome
// A poser par l'APPLICATION (ex. NKCode, depuis son dossier data/fonts) AVANT
// de charger les polices. Un chemin nullptr/vide = role desactive.
void NkSetFallbackFontPaths(const char *broad, const char *cjk, const char *emoji) noexcept;
```

**Avant** de charger la police, donc. C'est un invariant d'ordre : l'appel
place des chemins dans des variables globales que la construction de l'atlas
consultera.

Pour une police monospace destinée au code, on désactive au contraire les replis
(`extFallback = false`) : « atlas minuscule => reconstruction RAPIDE au
zoom » (`NkGuiFont.h:34-35`).

## Étape 7 — brancher l'entrée

Les *callbacks* écrivent directement dans `ctx.input`. Voici
l'ensemble minimal, puis les compléments.

### Souris et fermeture

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:326-343 (extrait)`**

```cpp
    auto &events = NkEvents();
    bool running = true;
    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent *) { running = false; });

    events.AddEventCallback<NkMouseMoveEvent>([&](NkMouseMoveEvent *e) {
        ctx.input.mousePos = {static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY())};
    });
    events.AddEventCallback<NkMouseButtonPressEvent>([&](NkMouseButtonPressEvent *e) {
        if (e->GetButton() == NkMouseButton::NK_MB_LEFT)  ctx.input.mouseDown[0] = true;
        if (e->GetButton() == NkMouseButton::NK_MB_RIGHT) ctx.input.mouseDown[1] = true;
        ctx.input.ctrlDown  = e->GetModifiers().ctrl;
        ctx.input.shiftDown = e->GetModifiers().shift;
        ctx.input.altDown   = e->GetModifiers().alt;
    });
    events.AddEventCallback<NkMouseButtonReleaseEvent>([&](NkMouseButtonReleaseEvent *e) {
        if (e->GetButton() == NkMouseButton::NK_MB_LEFT)  ctx.input.mouseDown[0] = false;
        if (e->GetButton() == NkMouseButton::NK_MB_RIGHT) ctx.input.mouseDown[1] = false;
    });
```

Indices des boutons : `0` = gauche, `1` = droit, `2` =
milieu.

### La molette

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:344-352 (extrait)`**

```cpp
    events.AddEventCallback<NkMouseWheelVerticalEvent>([&](NkMouseWheelVerticalEvent *e) {
        ctx.input.wheel += static_cast<float32>(e->GetDeltaY()); // >0 = haut ; consommé en EndFrame
        const auto m = e->GetModifiers();                        // modificateurs au moment de la molette
        ctx.input.ctrlDown = m.ctrl; ctx.input.shiftDown = m.shift; ctx.input.altDown = m.alt;
    });
    events.AddEventCallback<NkMouseWheelHorizontalEvent>([&](NkMouseWheelHorizontalEvent *e) {
        ctx.input.wheelH += static_cast<float32>(e->GetDeltaX());
    });
```

Notez le `+=` et non le `=` : plusieurs crans peuvent arriver dans
la même image, ils s'accumulent. `EndFrame` remet le compteur à zéro.
Notez aussi la relecture des modificateurs : sans elle, le Ctrl+molette (zoom)
ne fonctionnerait pas de façon fiable.

### Le double-clic

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:353-359 (extrait)`**

```cpp
    // Double-clic détecté par l'OS : NKEvent l'émet comme événement DÉDIÉ (le 2e
    // clic n'arrive PAS comme un mouseDown normal) → on l'injecte explicitement.
    events.AddEventCallback<NkMouseDoubleClickEvent>([&](NkMouseDoubleClickEvent *e) {
        ctx.input.mousePos = {static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY())};
        if (e->GetButton() == NkMouseButton::NK_MB_LEFT) ctx.input.SetDoubleClick(0);
    });
```

Sans ce *callback*, le second clic d'un double-clic n'existe tout simplement
pas pour NKGui : le système l'a converti en événement dédié. Les renommages en
place et la saisie au clavier dans les `DragFloat`, qui reposent sur le
double-clic, ne fonctionneraient pas.

### Le texte et les touches

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:360-380 (extrait)`**

```cpp
    // Saisie texte + touches d'édition (pour InputText). On pose l'état ENFONCÉ
    // (press/release) pour permettre la répétition au maintien.
    events.AddEventCallback<NkTextInputEvent>([&](NkTextInputEvent *e) { ctx.input.PushChar(e->GetCodepoint()); });

    auto setKey = [&](NkKey k, bool down) {
        switch (k) {
            case NkKey::NK_LEFT:   ctx.input.SetKey(NkGuiKey::Left, down);      break;
            case NkKey::NK_RIGHT:  ctx.input.SetKey(NkGuiKey::Right, down);     break;
            case NkKey::NK_UP:     ctx.input.SetKey(NkGuiKey::Up, down);        break;
            case NkKey::NK_DOWN:   ctx.input.SetKey(NkGuiKey::Down, down);      break;
            case NkKey::NK_HOME:   ctx.input.SetKey(NkGuiKey::Home, down);      break;
            case NkKey::NK_END:    ctx.input.SetKey(NkGuiKey::End, down);       break;
            case NkKey::NK_BACK:   ctx.input.SetKey(NkGuiKey::Backspace, down); break;
            case NkKey::NK_DELETE: ctx.input.SetKey(NkGuiKey::Delete, down);    break;
            case NkKey::NK_ENTER:  ctx.input.SetKey(NkGuiKey::Enter, down);     break;
            case NkKey::NK_ESCAPE: ctx.input.SetKey(NkGuiKey::Escape, down);    break;
            default: break;
        }
    };
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent *e)    { setKey(e->GetKey(), true);  … });
    events.AddEventCallback<NkKeyReleaseEvent>([&](NkKeyReleaseEvent *e){ setKey(e->GetKey(), false); … });
```

Deux points essentiels, déjà annoncés au chapitre 1 :

- on pose l'état **enfoncé/relâché**, pas un « appui » ponctuel. C'est
  ce qui permet à NKGui de calculer les durées et donc la répétition au
  maintien ;
- le **texte** passe par `PushChar` et les **touches** par
  `SetKey`. Ce sont deux canaux séparés.

Il reste à brancher les raccourcis d'édition, sur le modèle du shell de
l'éditeur :

**`Engine/NKEditorKit/src/NKEditorKit/NkEditorShell.cpp:318-326 (extrait)`**

```cpp
if (e->GetModifiers().ctrl) { // raccourcis copier/couper/coller/tout-selectionner
    if      (k == NkKey::NK_C) mUI.input.wantCopy = true;
    else if (k == NkKey::NK_X) mUI.input.wantCut = true;
    else if (k == NkKey::NK_V) mUI.input.wantPaste = true;
    else if (k == NkKey::NK_A) mUI.input.wantSelectAll = true;
```

### Le presse-papiers

NKGui ne connaît pas la fenêtre, donc il ne peut pas accéder au presse-papiers du
système. On lui donne deux pointeurs de fonction
(`ctx.clipboardGetFn`, `ctx.clipboardSetFn`, plus un pointeur
utilisateur `ctx.clipboardUser`) ; c'est ce que fait le shell en
`NkEditorShell.cpp:185-190`. Sans cela, copier-coller dans un champ de
saisie ne fait rien.

## La boucle principale

### Le temps

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:431-436`**

```cpp
    while (running && window.IsOpen()) {
        float32 dt = clock.Tick().delta;
        if (dt <= 0.f)
            dt = 1.f / 60.f;
        if (dt > 0.1f)
            dt = 0.1f;
```

Les deux bornes ont chacune leur raison : `dt <= 0` arrive à la toute
première image (l'horloge n'a pas encore de référence) ; `dt > 0.1` arrive
après un blocage — déplacement de fenêtre, mise en veille, point d'arrêt du
débogueur. Sans la borne haute, tout ce qui dépend du temps ferait un bond : le
curseur de saisie clignoterait plusieurs fois d'un coup, les répétitions au
maintien se déclencheraient en rafale.

### Les événements — avant tout le reste

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:438-442`**

```cpp
        while (NkEvent *ev = NkEvents().PollEvent()) {
            (void)ev;
        }
        if (!running)
            break;
```

Le `if (!running) break;` évite de dessiner une image entière sur une
fenêtre qu'on vient de fermer.

### Le redimensionnement

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:444-458`**

```cpp
        // Synchroniser la SWAPCHAIN à la taille de la FENÊTRE. target->GetSize()
        // renvoie la taille de la swapchain (qui ne change QU'À OnResize) — s'en
        // servir pour détecter le resize était faux : la swapchain restait figée à
        // sa taille de création (rendu basse-résolution étiré). On pilote OnResize
        // depuis la taille fenêtre (inclut la 1re frame → sync initiale).
        const math::NkVec2u wsz = target->GetWindow().GetSize();
        if (wsz.x > 0 && wsz.y > 0 && (wsz.x != lastW || wsz.y != lastH)) {
            target->OnResize(wsz.x, wsz.y);
            lastW = wsz.x;
            lastH = wsz.y;
        }
        const math::NkVec2u sz = target->GetSize(); // = wsz après OnResize
        if (sz.x > 0 && sz.y > 0) {
            ctx.viewW = static_cast<int32>(sz.x);
            ctx.viewH = static_cast<int32>(sz.y);
        }
```

`lastW` et `lastH` sont initialisés à zéro, ce qui garantit que la
première image déclenche une synchronisation. Et les tests `> 0` évitent
de recréer une chaîne d'échange de taille nulle quand la fenêtre est réduite.

> **⚠️ Fenêtre minimisée**
>
> Une fenêtre en cours de réduction glisse un rectangle intermédiaire — environ
> 160 × 28 sous Windows — entre le test « minimisée ? » et la lecture de
> taille. Le shell de l'éditeur documente une garde qui refuse toute taille sous
> 32 pixels :
>
> **`Integrations/NKGui/NkEditorRHIRenderer.h:123-130 (extrait)`**
>
> ```cpp
> // GARDE ANTI-MINIMISATION : une fenetre en cours de reduction
> // glisse son rect placeholder (~160x28 sous Windows) entre le
> // test « minimisee ? » de la boucle principale et la lecture
> // de taille -- la course est reelle (defaut 4.3, reproduite).
> // Sous 32 px, les cibles divisees du rendu (bloom /32) tombent
> // a zero et CreateTexture2D echoue en E_INVALIDARG -> mort a
> // la restauration. On refuse net : la vraie taille arrivera
> // avec le retour de la fenetre.
> ```

### L'image d'interface

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:468-481 (extrait)`**

```cpp
        ctx.BeginFrame(dt);
        NkGuiDrawList &dl = ctx.dl;
        const float32 W = static_cast<float32>(ctx.viewW);
        const float32 H = static_cast<float32>(ctx.viewH);

        // Fond + barre d'en-tête + TITRE (texte NKFont).
        dl.AddRectFilled({0.f, 0.f, W, H}, ctx.theme.bgPrimary);
        dl.AddRectFilled({0.f, 0.f, W, 40.f}, ctx.theme.header);
        dl.AddRectFilled({0.f, 38.f, W, 2.f}, ctx.theme.accent);
        TextAt(ctx, {16.f, 11.f}, "NKGui - Phase 3 : layout + widgets (NKFont)");
```

Le fond est dessiné en premier — tout le reste passera par-dessus. Ici la démo
écrit dans `ctx.dl` directement, ce qui est légitime parce qu'aucune
fenêtre n'est ouverte à ce moment : nous sommes bien sur la couche principale.
Dès qu'on est *à l'intérieur* d'un `Begin`, il faut passer par
`ctx.DL()` (chapitre 3).

Puis les widgets, avec une région de layout ouverte :

**`Squelette de widgets — écrit pour ce cours`**

```cpp
        ctx.BeginLayout({20.f, 60.f, 400.f, H - 80.f});
        Text(ctx, "Bonjour NKGui");
        if (Button(ctx, "Cliquez-moi")) {
            logger.Info("clic !");
        }
        static bool visible = true;
        Checkbox(ctx, "Afficher le panneau", visible);
        static float32 zoom = 1.f;
        SliderFloat(ctx, "Zoom", zoom, 0.5f, 4.f);
```

### Le curseur

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:1020-1041`**

```cpp
        // Curseur souhaité par NKGui → curseur OS (OPTIONNEL : l'app choisit d'appliquer).
        // Ex. DragFloat pose ↔ / ↕ pendant le survol/glisser. SetCursor est persistant.
        {
            NkWindow::NkCursorType c = NkWindow::NkCursorType::Arrow;
            switch (ctx.wantCursor) {
                case NkGuiCursor::Text:
                    c = NkWindow::NkCursorType::TextInput;
                    break;
                case NkGuiCursor::Hand:
                    c = NkWindow::NkCursorType::Hand;
                    break;
                case NkGuiCursor::ResizeEW:
                    c = NkWindow::NkCursorType::ResizeWE;
                    break;
                case NkGuiCursor::ResizeNS:
                    c = NkWindow::NkCursorType::ResizeNS;
                    break;
                default:
                    break;
            }
            window.SetCursor(c);
        }
```

Rappel du chapitre 3 : NKGui *demande* un curseur en posant
`ctx.wantCursor`, il ne le pose pas — il ne connaît pas la fenêtre. Ce
bloc est le traducteur, et il est facultatif : sans lui, tout fonctionne, mais
le pointeur ne change jamais de forme au-dessus d'un champ de saisie ou d'un
séparateur. Notez qu'il est placé *avant* `EndFrame`, parce que
`wantCursor` est réinitialisé par `BeginFrame` et rempli par les
widgets.

### La présentation

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:1043-1048`**

```cpp
        ctx.EndFrame();

        target->Clear();
        backend.Submit(dl, sz.x, sz.y);               // couche principale
        backend.Submit(ctx.dlOverlay, sz.x, sz.y); // couche popups/overlay (au-dessus)
        target->Display(); // Capture d'ecran (F12) : APRES Display() — la frame presentee. Self-capture
```

> **⚠️ DEUX `Submit`, jamais un seul**
>
> C'est le piège le plus coûteux de tout ce cours, parce qu'il est absolument
> silencieux.
>
> NKGui produit **deux** listes de dessin :
>
> - `ctx.dl` — le contenu ordinaire, y compris les fenêtres fusionnées
>   par ordre de profondeur ;
> - `ctx.dlOverlay` — les popups, les menus déroulants, les
>   sous-menus, les combos, les infobulles, et tout ce que vous dessinez
>   entre `PushOverlay` et `PopOverlay`.
>
> Si vous n'appelez `Submit` que sur la première, **tout fonctionne
> sauf les surfaces flottantes**. Le menu s'ouvre — l'état est correct, les clics
> sont capturés, la logique tourne — mais rien ne s'affiche. Vous cliquez dans le
> vide sur des éléments invisibles.
>
> Et il n'y a *aucun* message d'erreur : le framework a fait son travail, le
> pont aussi ; c'est simplement qu'on ne lui a jamais donné la seconde liste.
>
> **L'ordre compte aussi** : `ctx.dl` d'abord, `ctx.dlOverlay`
> ensuite. Inversé, les popups passeraient *sous* le contenu.

Le shell de l'éditeur fait exactement la même chose, à travers son interface de
renderer :

**`Engine/NKEditorKit/src/NKEditorKit/NkEditorShell.cpp:725-732`**

```cpp
mUI.EndFrame();

mWindow.SetCursor(MapCursor(mUI.wantCursor));

mRenderer->BeginFrame();
mRenderer->SubmitDrawList(mUI.dl, sz.x, sz.y);
mRenderer->SubmitDrawList(mUI.dlOverlay, sz.x, sz.y);
mRenderer->EndFrame();
```

### Ce que fait `Submit`, en détail

Pour comprendre ce qui peut mal tourner, voici la traduction complète :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/UI/NkGuiCanvasBackend.h:120-195 (condensé)`**

```cpp
void Submit(const NkGuiDrawList &dl, uint32 fbW, uint32 fbH) {
    if (!mRenderer || dl.vtx.Size() == 0 || dl.idx.Size() == 0) return;

    // 1) conversion des sommets NKGui -> NkVertex2D (couleur dépaquetée)
    mScratch.Resize(dl.vtx.Size());
    for (uint32 i = 0; i < dl.vtx.Size(); ++i) {
        const nkgui::NkGuiVertex &s = dl.vtx[i];
        renderer::NkVertex2D &d = mScratch[i];
        d.x = s.pos.x; d.y = s.pos.y; d.u = s.uv.x; d.v = s.uv.y;
        d.r = (uint8)( s.col        & 0xFF);
        d.g = (uint8)((s.col >>  8) & 0xFF);
        d.b = (uint8)((s.col >> 16) & 0xFF);
        d.a = (uint8)((s.col >> 24) & 0xFF);
    }

    // 2) une passe par commande
    for (uint32 ci = 0; ci < dl.cmds.Size(); ++ci) {
        const nkgui::NkGuiDrawCmd &dc = dl.cmds[ci];
        if (dc.idxCount == 0u) continue;

        // 2a) clip : le rect « infini » (1e9) signifie « pas de clip »
        const bool hasClip = (dc.clipRect.w < 1.0e8f && dc.clipRect.h < 1.0e8f);
        if (hasClip) {
            … clamp a [0, fbW] x [0, fbH] …
            if (x1 <= x0 || y1 <= y0) continue;          // clip vide -> on saute
            mRenderer->SetClip(math::NkRect2i{…});
        }

        // 2b) résolution de la texture : d'abord les atlas de police, sinon les images
        renderer::NkTexture *tex = nullptr;
        if (dc.type == nkgui::NkGuiDrawCmdType::TexturedTriangles) { … }

        // 2c) sous-ensemble de sommets + indices REBASÉS
        uint32 lo = 0xFFFFFFFFu, hi = 0u;
        for (uint32 k = 0; k < dc.idxCount; ++k) { const uint32 v = dl.idx[dc.idxOffset+k];
                                                   if (v<lo) lo=v; if (v>hi) hi=v; }
        mIdxTmp.Resize(dc.idxCount);
        for (uint32 k = 0; k < dc.idxCount; ++k) mIdxTmp[k] = dl.idx[dc.idxOffset+k] - lo;
        mRenderer->DrawVertices(mScratch.Data() + lo, hi - lo + 1u, mIdxTmp.Data(), dc.idxCount, tex);

        if (hasClip) mRenderer->PopClip();
    }
}
```

L'étape (2c) mérite l'attention, parce qu'elle corrige un défaut qui coûtait un
plantage :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/UI/NkGuiCanvasBackend.h:176-178`**

```cpp
// Ne soumet que le SOUS-ENSEMBLE de vertices reference par cette
// commande (indices rebases). Indispensable : passer tout le buffer
// depasse kMaxVertices (65536) des qu'un draw list est gros -> crash.
```

Le pont calcule l'indice minimal et maximal utilisés par la commande, n'envoie
que cette tranche de sommets, et *décale tous les indices* d'autant. Sans
cela, chaque commande enverrait tout le tampon.

*(Le commentaire mentionne 65 536 ; la constante actuelle est
262 144 — voir chapitre 2, section 2.10. Le commentaire garde la trace du
dimensionnement d'origine, et c'est la même famille de bug qui a motivé
l'agrandissement.)*

## Le programme complet

Rassemblons tout dans un fichier minimal mais fonctionnel. Ce squelette est
reconstitué fidèlement depuis `Applications/NKGuiDemo/src/NKGuiDemo/main.cpp`
(lignes 240 à 1067), réduit au strict nécessaire.

**`Squelette complet — reconstitué depuis Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:240-1067`**

```cpp
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Core/NkGraphicsApi.h"
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKTime/NkClock.h"
#include "NKMemory/NkUniquePtr.h"
#include "NKLogger/NkLog.h"
#include "NKGui/NKGui.h"
#include "NKCanvas/UI/NkGuiCanvasBackend.h"

using namespace nkentseu;
using namespace nkentseu::nkgui;
using namespace nkentseu::renderer;

NKENTSEU_DEFINE_APP_DATA(([]() {
    NkAppData d{};
    d.appName    = "MonApp";
    d.appVersion = "0.1.0";
    return d;
})());

int nkmain(const NkEntryState &state) {
    (void)state;

    // ── 1) FENETRE OS (NKWindow) ────────────────────────────────────────────
    NkWindow window;
    NkWindowConfig cfg;
    cfg.title = "Mon app NKGui";
    cfg.width = 1100; cfg.height = 800;
    cfg.centered = true; cfg.resizable = true;
    if (!window.Create(cfg)) return -1;

    // ── 2) CONTEXTE GRAPHIQUE + CIBLE DE RENDU (NKCanvas) ───────────────────
    NkContextDesc desc;
    desc.api = NkGraphicsApi::NK_GFX_API_AUTO;
    if (desc.api == NkGraphicsApi::NK_GFX_API_AUTO) {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
        desc.api = NkGraphicsApi::NK_GFX_API_DX11;
#else
        desc.api = NkGraphicsApi::NK_GFX_API_OPENGL;
#endif
    }
    auto target = memory::NkMakeUnique<NkRenderWindow>(window, desc);
    if (!target || !target->IsValid()) return -1;

    // ── 3) CONTEXTE NKGui ───────────────────────────────────────────────────
    auto ctxPtr = memory::NkMakeUnique<NkGuiContext>();
    if (!ctxPtr) return -1;
    NkGuiContext &ctx = *ctxPtr;
    ctx.Init(static_cast<int32>(cfg.width), static_cast<int32>(cfg.height));
    SetCurrentContext(&ctx);

    // ── 4) BACKEND (NKGui -> NKCanvas) ──────────────────────────────────────
    renderer::NkGuiCanvasBackend backend;
    if (!backend.Init(target->GetRenderer())) return -1;

    // ── 5) POLICE : charger PUIS uploader l'atlas ───────────────────────────
    auto fontPtr = memory::NkMakeUnique<NkGuiFont>();   // DOIT survivre a la boucle
    if (!fontPtr->LoadEmbedded(NkEmbeddedFontId::DroidSans, 18.f)) {
        fontPtr->LoadEmbedded(NkEmbeddedFontId::ProggyClean, 16.f);
    }
    ctx.font = fontPtr.Get();                            // pointeur brut NON possede
    if (fontPtr->Valid()) {
        backend.UploadFontGray8(fontPtr->TexId(), fontPtr->pixels, fontPtr->atlasW, fontPtr->atlasH);
    }

    // ── 6) GLUE D'ENTREE (callbacks NKEvent -> ctx.input) ───────────────────
    auto &events = NkEvents();
    bool running = true;
    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent *) { running = false; });
    events.AddEventCallback<NkMouseMoveEvent>([&](NkMouseMoveEvent *e) {
        ctx.input.mousePos = {static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY())};
    });
    events.AddEventCallback<NkMouseButtonPressEvent>([&](NkMouseButtonPressEvent *e) {
        if (e->GetButton() == NkMouseButton::NK_MB_LEFT) ctx.input.mouseDown[0] = true;
    });
    events.AddEventCallback<NkMouseButtonReleaseEvent>([&](NkMouseButtonReleaseEvent *e) {
        if (e->GetButton() == NkMouseButton::NK_MB_LEFT) ctx.input.mouseDown[0] = false;
    });
    events.AddEventCallback<NkMouseWheelVerticalEvent>([&](NkMouseWheelVerticalEvent *e) {
        ctx.input.wheel += static_cast<float32>(e->GetDeltaY());
    });
    events.AddEventCallback<NkTextInputEvent>([&](NkTextInputEvent *e) {
        ctx.input.PushChar(e->GetCodepoint());
    });

    NkClock clock;
    uint32 lastW = 0, lastH = 0;
    bool showPanel = true;
    float32 zoom = 1.f;
    char nom[64] = "sans titre";

    // ── 7) BOUCLE PRINCIPALE ────────────────────────────────────────────────
    while (running && window.IsOpen()) {
        float32 dt = clock.Tick().delta;
        if (dt <= 0.f)  dt = 1.f / 60.f;
        if (dt >  0.1f) dt = 0.1f;                       // borne anti-saut

        // a) EVENEMENTS — AVANT BeginFrame (BeginFrame appelle input.NewFrame())
        while (NkEvent *ev = NkEvents().PollEvent()) { (void)ev; }
        if (!running) break;

        // b) Synchroniser la SWAPCHAIN sur la taille de la FENETRE
        const math::NkVec2u wsz = target->GetWindow().GetSize();
        if (wsz.x > 0 && wsz.y > 0 && (wsz.x != lastW || wsz.y != lastH)) {
            target->OnResize(wsz.x, wsz.y);
            lastW = wsz.x; lastH = wsz.y;
        }
        const math::NkVec2u sz = target->GetSize();
        if (sz.x > 0 && sz.y > 0) { ctx.viewW = (int32)sz.x; ctx.viewH = (int32)sz.y; }

        // c) DEBUT DE FRAME UI
        ctx.BeginFrame(dt);
        const float32 W = (float32)ctx.viewW;
        const float32 H = (float32)ctx.viewH;
        ctx.dl.AddRectFilled({0.f, 0.f, W, H}, ctx.theme.bgPrimary);

        // d) LES WIDGETS
        if (BeginPanel(ctx, "Reglages", {20.f, 20.f, 340.f, H - 40.f})) {
            Text(ctx, "Bonjour NKGui");
            Separator(ctx);
            Checkbox(ctx, "Afficher l'apercu", showPanel);
            SliderFloat(ctx, "Zoom", zoom, 0.5f, 4.f);
            InputText(ctx, "Nom", nom, sizeof(nom));
            if (Button(ctx, "Reinitialiser")) {
                zoom = 1.f;
                showPanel = true;
            }
            EndPanel(ctx);
        }

        // e) FIN DE FRAME UI
        ctx.EndFrame();

        // f) PRESENTATION : Clear ouvre la frame, Display la ferme et presente
        target->Clear();
        backend.Submit(ctx.dl,        sz.x, sz.y);        // couche principale
        backend.Submit(ctx.dlOverlay, sz.x, sz.y);        // popups/overlay AU-DESSUS
        target->Display();
    }

    SetCurrentContext(nullptr);
    ctx.Shutdown();
    return 0;
}
```

> **✅ Ce qu'il faut retenir**
>
> Deux cents lignes, dont la moitié est du branchement d'entrée. Tout le reste de
> votre application tiendra entre `BeginFrame` et `EndFrame`.

## Ajouter une image

Les icônes et les images suivent exactement le chemin de la police : on charge
des pixels, on les envoie sous un identifiant, on dessine.

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:292-321 (abrégé)`**

```cpp
    uint32 imgTexId = 0u;
    int32 imgW = 0, imgH = 0;
    {
        static const char *kCandidates[] = {
            "Applications/Mou/assets/brand/rihen-logo.png",
            "Resources/Icons/ContentBrowser/FileIcon.png",
            "../../../Applications/Mou/assets/brand/rihen-logo.png",
        };
        NkImage img;
        bool ok = false;
        for (const char *p : kCandidates)
            if (img.Load(p, 4)) { ok = true; break; }
        if (ok && img.Width() > 0 && img.Height() > 0 && img.Pixels()) {
            imgW = img.Width();
            imgH = img.Height();
            static NkVector<uint8> rgba;
            rgba.Resize(static_cast<usize>(imgW) * imgH * 4u);
            for (int32 y = 0; y < imgH; ++y) {
                const uint8 *src = img.RowPtr(y);
                uint8 *dst = rgba.Data() + static_cast<usize>(y) * imgW * 4u;
                for (int32 x = 0; x < imgW * 4; ++x)
                    dst[x] = src[x];
            }
            imgTexId = 0x494D4731u; // 'IMG1'
            backend.UploadImageRGBA(imgTexId, rgba.Data(), imgW, imgH);
            logger.Info("Image chargee : {0}x{1}", imgW, imgH);
        } else {
            logger.Info("Image demo introuvable (cwd) -> section image vide");
        }
    }
```

Puis, dans la boucle, `Image(ctx, imgTexId, 128.f, 128.f)` ou
`ctx.DL().AddImage(imgTexId, rect, {0,0}, {1,1}, tint)`.

Trois choses à noter :

- **La liste de chemins candidats**, avec des chemins relatifs à la
  racine du dépôt *et* des chemins remontants. C'est la parade au
  piège du répertoire courant vu au chapitre 0. Le dépôt formalise cette
  approche : « Candidats RELATIFS AU CWD (dev, lancement depuis la racine
  du repo) PUIS relatifs a l'EXECUTABLE »
  (`Applications/NKCode/src/NKCode/Shell/NkAppFonts.h:18-21`).
- **L'échec est journalisé, pas fatal.** L'application se lance quand
  même, avec une section image vide.
- **Le choix du `texId`**. Ici `0x494D4731`, soit
  « IMG1 ». Il doit être distinct de celui de toute police
  (`0x4E4B4654` pour la première), car le pont résout
  **les atlas de police d'abord, les images ensuite**
  (`NkGuiCanvasBackend.h:161-174`). Le shell de l'éditeur va plus
  loin et réserve des plages entières :

**`Engine/NKEditorKit/src/NKEditorKit/NkEditorShell.h:392`**

```cpp
uint32 mNextTexId = 0x4E4B0100u;  // ids de textures app (logos/icones) — distincts des polices
```

> **✅ Ce qu'il faut retenir**
>
> `texId == 0` est **réservé** : c'est la texture blanche interne côté
> RHI (`Integrations/NKGui/NkGuiRHIBackend.h:40`), et un
> `AddImage` avec `texId == 0` est purement ignoré
> (`NkGuiDrawList.cpp:160-161`). Si votre image ne s'affiche pas, la
> première chose à vérifier est que son identifiant n'est pas resté à zéro parce
> que le chargement a échoué.

## Le fichier de build

Il reste à déclarer la cible pour Jenga. Créez
`Applications/MonApp/MonApp.jenga` :

**`Applications/MonApp/MonApp.jenga — écrit pour ce cours, calqué sur Applications/NKGuiDemo/NKGuiDemo.jenga:41-75`**

```cpp
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""MonApp — application de demonstration NKGui sur NKCanvas."""

from Jenga import *
from jengaconfig import *

with project("MonApp"):
    windowedapp()
    language("C++")
    cppdialect("C++17")
    location(".")

    files(["src/**.cpp"])

    nkentseudependson(
        ["NKGui", "NKCanvas", "NKWindow", "NKEvent", "NKGlad", "NKFont", "NKImage",
         "NKStream", "NKFileSystem", "NKLogger", "NKMath", "NKTime",
         "NKContainers", "NKMemory", "NKCore", "NKPlatform", "NKThreading"],
        extra_includes=["src", "src/MonApp", "%{NKGlad.location}/include"],
    )

    objdir("%{wks.location}/Build/Obj/%{cfg.buildcfg}-%{cfg.system}/%{prj.name}")
    targetdir("%{wks.location}/Build/Bin/%{cfg.buildcfg}-%{cfg.system}/%{prj.name}")

    apppublisher("Rihen Universe")
    appversion("0.1.0")
    licensefile("../../LICENSE")

    with filter("system:Windows"):
        windowedapp()
        usetoolchain(TC_WINDOWS)
        defines(["WIN32_LEAN_AND_MEAN", "_UNICODE", "UNICODE"])
        links(["user32", "gdi32", "opengl32", "dwmapi", "shell32",
               "uuid", "ole32", "dxguid",
               "d3d11", "d3d12", "dxgi", "d3dcompiler"])
```

### Pourquoi chaque dépendance est là

| **Module** | **Pourquoi** |
|---|---|
| `NKGui` | les widgets et le contexte |
| `NKCanvas` | le rendu, la cible, et l'en-tête du pont |
| `NKWindow` | la fenêtre et le point d'entrée `nkmain` |
| `NKEvent` | les événements typés |
| `NKFont` | la rasterisation des glyphes |
| `NKImage` | le décodage des images |
| `NKGlad` | le chargeur de fonctions OpenGL |
| `NKTime` | `NkClock` et le *delta time* |
| `NKLogger` | l'objet `logger` |
| `NKMath`, `NKContainers`, `NKMemory`, `NKCore`, `NKPlatform` | les fondations |
| `NKFileSystem`, `NKStream`, `NKThreading` | tirés par les modules ci-dessus |

> **⚠️ Oublier `NKCanvas` ou `NKGui`**
>
> Le pont `NkGuiCanvasBackend.h` est *header-only*. Il inclut
> `"NKGui/NKGui.h"` et appelle des méthodes de `NkIRenderer2D`. Si
> votre `.jenga` ne déclare qu'un seul des deux modules, l'erreur ne se
> produit pas à la compilation de NKCanvas — qui ne compile pas ce fichier — mais
> *chez vous*, sous forme d'en-têtes introuvables ou de symboles non résolus.

### Enregistrer la cible et compiler

Selon l'organisation du dépôt, la nouvelle cible doit être connue du fichier
racine `Nkentseu.jenga` — vérifiez comment les autres applications y sont
listées avant de lancer la compilation.

**`Compilation et lancement — depuis la racine du dépôt`**

```
cd D:\Projets\2026\Nkentseu\Nkentseu
jenga build --target MonApp --config Release
.\Build\Bin\Release-Windows\MonApp\MonApp.exe
```

Et la configuration de mise au point, qui garde symboles et assertions :

**`Compilation en Debug`**

```
jenga build --target MonApp --config Debug
.\Build\Bin\Debug-Windows\MonApp\MonApp.exe
```

> **✅ Lancer depuis la racine**
>
> Toujours depuis `D:/Projets/2026/Nkentseu/Nkentseu`, pas depuis le dossier
> de l'exécutable. Les chemins de ressources — polices de repli, images, icônes —
> sont écrits relativement à la racine du dépôt pendant le développement. Et le
> journal, `logs/app.log`, est écrit relativement au répertoire courant :
> lancer d'ailleurs le disperse.

## Diagnostiquer les silences

Cette section est un aide-mémoire. Elle liste ce qui, dans une application
NKGui, échoue *sans message d'erreur*.

| **Symptôme** | **Cause la plus probable** |
|---|---|
| Fenêtre noire, l'application tourne | Backend graphique non fonctionnel ; essayer OpenGL, lire `logs/app.log` |
| Widgets visibles, aucun texte | `UploadFontGray8` manquant, ou `fontPtr->Valid()` faux |
| Menus et combos invisibles mais cliquables | Le second `Submit` sur `ctx.dlOverlay` manque |
| Popups sous le contenu | Les deux `Submit` sont dans le mauvais ordre |
| Les clics arrivent avec une image de retard, ou jamais | Les événements sont pompés *après* `BeginFrame` |
| Plus rien ne réagit, l'application tourne | `activeId` figé : un glissement maison n'a pas remis `ctx.activeId = 0` |
| La souris semble gelée après une modale | Un panneau a écrasé `ctx.input.mousePos` |
| Rendu flou et étiré après agrandissement | Le redimensionnement est détecté sur la swapchain au lieu de la fenêtre |
| Pas de barre de défilement, barre horizontale hors écran | `AvailHeight()` utilisé sans intersection avec `CurrentClip()` |
| Image absente | `texId` resté à zéro (chargement échoué) ou en collision avec une plage de police |
| Bords droit et bas des panneaux amincis | Une bordure dessinée à l'`AddLine` au lieu de quatre rectangles internes |
| Texte flou | Position de glyphe non alignée sur la grille de pixels |
| Deux boutons réagissent ensemble | Même libellé, même portée : il manque `PushId`/`PopId` |

## Pour aller plus loin

Ce que ce chapitre n'a pas fait, et que vous savez maintenant aborder seul :

- **Des fenêtres flottantes et du docking.** Ajoutez
  `DockSpaceOverViewport(ctx)` avant vos `Begin`, puis
  déclarez chaque panneau entre `Begin` et `EndWindow`. La
  démo le fait en `main.cpp:888-1017`.
- **Le HiDPI.** `SetUiScale(ctx, 1.5f)` plus un rechargement de
  la police à `18.f * 1.5f`, avant `BeginFrame`
  (`main.cpp:461-466`).
- **La capture d'écran**, après `Display()` :
  `target->Capture("capture.png")`
  (`main.cpp:1049-1061`).
- **La coquille d'éditeur.** Si votre application ressemble à un IDE —
  barre de titre maison, palette de commandes, préférences, panneaux
  ancrables — n'écrivez pas tout cela : `Engine/NKEditorKit` le
  fournit, et vous n'avez plus qu'à écrire des panneaux dérivant de
  `NkEditorPanel`.

## Exercices

> **✏️ 1 — Faire tourner le squelette**
>
> Créez `Applications/MonApp/` avec son `.jenga` et son
> `src/MonApp/main.cpp`, recopiez le squelette de la section 4.9,
> compilez, lancez. Vérifiez que le panneau s'affiche avec son titre, que la case à
> cocher répond et que le *slider* se déplace. Puis, **volontairement**,
> supprimez la ligne `backend.Submit(ctx.dlOverlay, …)`, ajoutez un
> `BeginCombo` au panneau, et constatez par vous-même ce que « échoue sans
> message » veut dire.

> **✏️ 2 — Un vrai clavier**
>
> Complétez le branchement de l'entrée : double-clic, molette horizontale, touches
> d'édition (`setKey`), raccourcis `wantCopy`/`wantCut`/`wantPaste`/`wantSelectAll`, et le presse-papiers via
> `ctx.clipboardGetFn` / `ctx.clipboardSetFn`. Vérifiez ensuite,
> dans un `InputText`, que les flèches déplacent le curseur, que Maj+flèche
> sélectionne, que Ctrl+C et Ctrl+V fonctionnent, et qu'un accent s'écrit
> correctement.

> **✏️ 3 — Une image et son identifiant**
>
> Chargez une image PNG avec `NkImage`, envoyez-la par
> `backend.UploadImageRGBA` sous un identifiant de votre choix, affichez-la
> avec le widget `Image`. Puis attribuez-lui *délibérément* le même
> identifiant que la police (`0x4E4B4654`) et observez ce qui s'affiche.
> Expliquez le résultat à partir de l'ordre de résolution du pont : atlas de
> police d'abord, images ensuite.

> **✏️ 4 — Une application complète**
>
> Écrivez un petit gestionnaire de tâches : une table à trois colonnes (fait,
> titre, priorité), un champ de saisie et un bouton pour ajouter une ligne, une
> case à cocher par ligne, et un bouton « Supprimer les tâches terminées ». Les
> données vivent dans un `NkVector` de votre propre structure. Contraintes :
> un `PushId` par ligne, un identifiant stable pour la table, et
> **aucune** modification du tableau pendant le parcours — levez un drapeau
> et agissez après `EndTable`, conformément au dernier idiome du
> chapitre 3.
