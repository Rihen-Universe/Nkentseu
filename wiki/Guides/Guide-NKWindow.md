# Créer une fenêtre avec NKWindow

**AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
**Écrit le 26/09/2026. Public : quelqu'un qui découvre Nkentseu et veut ouvrir sa première fenêtre.**

> ⚠️ **Tout ce qui suit a été relevé dans le code, pas écrit de mémoire.** Chaque nom de
> fonction, chaque champ, chaque valeur par défaut vient d'un fichier que vous pouvez
> ouvrir. Les chemins sont donnés pour que vous puissiez vérifier — et si le code dit
> autre chose que ce document, **c'est le code qui a raison**, et cela mérite un signalement.

---

## 1. La plus petite application qui tienne debout

```cpp
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"

using namespace nkentseu;

NKENTSEU_DEFINE_APP_DATA(([]() {
    NkAppData d{};
    d.appName    = "MonApp";
    d.appVersion = "0.1.0";
    return d;
})());

int nkmain(const NkEntryState &state) {
    (void)state;

    NkWindow window;
    NkWindowConfig cfg;
    cfg.title  = "Ma premiere fenetre";
    cfg.width  = 1280;
    cfg.height = 720;

    if (!window.Create(cfg))
        return -1;

    while (window.IsOpen()) {
        while (NkEvent *ev = NkEvents().PollEvent()) {
            (void)ev;   // on traitera les événements au chapitre suivant
        }
    }
    return 0;
}
```

### Ce que chaque morceau fait, et pourquoi il existe

**`nkmain`, pas `main`.** Nkentseu vise huit plateformes, et chacune a sa propre façon de
démarrer un programme — Windows veut `WinMain`, Android veut une activité, iOS veut un
délégué. `NKMain.h` écrit le vrai point d'entrée pour vous et appelle **votre** `nkmain`.
Vous écrivez une seule fonction, elle marche partout.

**`NKENTSEU_DEFINE_APP_DATA`** déclare l'identité de votre application — son nom, sa
version. Certaines plateformes en ont besoin **avant** que la première ligne de code ne
tourne : c'est pourquoi c'est une macro au niveau du fichier et non un appel dans `nkmain`.

**`NkEntryState`** porte ce que la plateforme a transmis au démarrage — les arguments de
ligne de commande, notamment. Vous pouvez l'ignorer au début : `(void)state;`.

⚠️ **Un contrat écrit et non évident** : `argv[0]` n'est **pas** un argument utilisateur,
c'est le nom du programme. Vingt-et-un sites du dépôt le sautent, et rien ne le disait —
le dépôt a une note à ce sujet. Si vous lisez les arguments, commencez à l'indice 1, ou
employez `UserArgs()` qui le fait pour vous.

---

## 2. `NkWindowConfig` — les réglages, un par un

Fichier : `Kernel/Runtime/NKWindow/src/NKWindow/Core/NkWindowConfig.h`, ligne 116.

### ⚠️ Le contrat des tailles, à lire avant tout le reste

```cpp
int32  x = 100;        // coin haut-gauche de la FENETRE (cadre compris)
int32  y = 100;
uint32 width  = 1280;  // taille de la zone CLIENT (la surface dessinable)
uint32 height = 720;
```

**`x`/`y` concernent la fenêtre entière, `width`/`height` concernent l'intérieur.**
Ce n'est pas un caprice : quand vous demandez 1280×720, vous voulez 1280×720 **pixels à
dessiner**, pas 1280×720 moins la barre de titre. Mélanger les deux donne une fenêtre plus
petite que demandée — c'est un défaut classique, et il a été payé ici.

### Position et taille

| champ | défaut | ce qu'il fait |
|---|---|---|
| `x`, `y` | 100, 100 | position du coin haut-gauche de la fenêtre |
| `width`, `height` | 1280, 720 | taille de la **zone client** |
| `minWidth`, `minHeight` | 160, 90 | l'utilisateur ne peut pas réduire en deçà |
| `maxWidth`, `maxHeight` | 0xFFFF | ni agrandir au-delà |
| `centered` | `true` | centre la fenêtre et **ignore `x`/`y`** |

### Comportement

| champ | défaut | ce qu'il fait |
|---|---|---|
| `resizable` | `true` | l'utilisateur peut redimensionner |
| `movable` | `true` | l'utilisateur peut déplacer |
| `closable` | `true` | le bouton de fermeture agit |
| `minimizable` | `true` | le bouton de réduction agit |
| `maximizable` | `true` | le bouton d'agrandissement agit |
| `canFullscreen` | `true` | le plein écran est autorisé |
| `fullscreen` | `false` | démarre en plein écran |
| `modal` | `false` | bloque les autres fenêtres de l'application |
| `vsync` | `true` | synchronise sur le rafraîchissement de l'écran |
| `dropEnabled` | `false` | accepte le glisser-déposer de fichiers |
| `screenOrientation` | `AUTO` | mobile : orientation imposée |

### Apparence

| champ | défaut | ce qu'il fait |
|---|---|---|
| `frame` | `true` | cadre et barre de titre du système |
| `hasShadow` | `true` | ombre portée |
| `transparent` | `false` | fond transparent (nécessite un compositeur) |
| `visible` | `true` | visible dès la création |
| `bgColor` | `0x141414FF` | couleur de fond, en `RRGGBBAA` |

### Fenêtres discrètes — trois réglages qui se ressemblent et ne font pas la même chose

| champ | défaut | ce qu'il fait |
|---|---|---|
| `alwaysOnTop` | `false` | reste au-dessus des autres fenêtres |
| `clickThrough` | `false` | **la souris traverse** la fenêtre et atteint ce qu'il y a dessous |
| `opacity` | `1.0f` | opacité globale, de 0 à 1 |
| `noActivate` | `false` | **ne prend jamais le focus** |

⚠️ **`clickThrough` et `noActivate` ne sont pas la même chose**, et l'en-tête le dit
explicitement :

- `clickThrough` laisse la souris **traverser** — vos clics vont à la fenêtre du dessous ;
- `noActivate` **garde les clics qui la visent** et refuse seulement de **voler le focus**.

Le second existe pour les fenêtres de mesure : une fenêtre qui passe au premier plan reçoit
les frappes de l'utilisateur, et l'on croit alors mesurer le produit alors qu'on mesure
l'utilisateur. Deux faux défauts ont été diagnostiqués ainsi en une nuit.

📌 **Ces trois réglages existent aussi à l'exécution** (`SetAlwaysOnTop`, `SetOpacity`,
`SetClickThrough`). Les poser dans la configuration **évite le clignotement** d'une fenêtre
créée « normale » puis corrigée une image plus tard.

### Identité

| champ | défaut |
|---|---|
| `title` | `"NkWindow"` — le texte de la barre de titre |
| `name` | `"NkApp"` — le nom interne, employé par certaines plateformes |
| `iconPath` | vide — chemin d'une icône |

---

## 3. 🔴 CE QUE VOUS DEVEZ SAVOIR AVANT DE DÉBOGUER

**Tous les réglages n'agissent pas sur toutes les plateformes.** Ce n'est pas un défaut
caché : c'est **mesuré et écrit**.

👉 **`wiki/Runtime/NKWindow/Proprietes-par-dorsal.md`** — un tableau de chaque propriété
contre chaque dorsal, avec quatre verdicts : *agit* · *ignorée en silence* · *refus nommé* ·
*sans objet sur cette plateforme*.

**Lisez-le avant de chercher l'erreur chez vous.** Un utilisateur a passé du temps à
chercher pourquoi `resizable = false` ne faisait rien : la réponse était que le dorsal
Windows ne lisait pas ce champ. Ce n'était pas son code.

⚠️ **Et une honnêteté sur ce tableau** : il est **relevé sur treize dorsaux, exécuté sur
un** (Windows). Les autres lignes viennent de la lecture du code. C'est utile, ce n'est pas
une preuve d'exécution.

**Depuis le 25/09**, ce qui ne peut pas agir **le dit** : un refus nommé au journal, une
fois par propriété et par plateforme, au lieu d'un silence.

---

## 4. Agir sur la fenêtre après sa création

Fichier : `Core/NkWindow.h`.

### Cycle de vie
```cpp
bool Create(const NkWindowConfig &config);
void Close();
bool IsOpen() const;
bool IsValid() const;
NkWindowId GetId() const;
```

### Titre, taille, position
```cpp
NkString GetTitle() const;
void     SetTitle(const NkString &title);
void     SetSize(uint32 width, uint32 height);
void     SetPosition(int32 x, int32 y);
float32  GetDpiScale() const;          // mise à l'échelle de l'écran
NkWindowConfig GetConfig() const;      // relire la configuration effective
```

⚠️ **Un piège mesuré dans ce dépôt** : `SetSize(GetSize())` **n'est pas l'identité**. La
fenêtre grandissait de quelques pixels à chaque lancement, parce qu'une taille *client*
relue puis reposée comme taille *fenêtre* accumule l'épaisseur du cadre. Si vous
sauvegardez et restaurez une taille, **vérifiez laquelle des deux vous manipulez**.

### État
```cpp
void Minimize();
void Maximize();
void Restore();
bool IsMaximized() const;
bool IsMinimized() const;
void SetVisible(bool visible);
void SetFullscreen(bool fullscreen);
void SetDecorated(bool decorated);
bool IsDecorated() const;
```

⚠️ **`IsMaximized` et `IsMinimized` ne renseignent PAS sur le plein écran.** Une fenêtre en
plein écran n'est ni maximisée ni minimisée. Le piège est assez naturel pour qu'un
recensement automatique s'y soit laissé prendre.

### Barre de titre personnalisée
```cpp
void BeginDragMove();                  // déplacement natif
void BeginResize(NkResizeEdge edge);   // redimensionnement par un bord
```
Ces deux-là passent la main au système : votre barre de titre dessinée devient alors aussi
fluide qu'une vraie.

### Presse-papiers
```cpp
void     SetClipboardText(const NkString &text);
NkString GetClipboardText() const;
bool     SetClipboardImage(const NkClipboardImage &image);
bool     GetClipboardImage(NkClipboardImage &out) const;
bool     HasClipboardImage() const;
```

### Écrans
```cpp
NkVector<NkDisplayInfo> EnumerateMonitors() const;
uint32                  GetMonitorCount() const;
```

---

## 5. La souris — et le piège qui a fait trébucher trois personnes

### Écrire (agir sur la souris) : dans **NKWindow**
```cpp
void SetMousePosition(uint32 x, uint32 y);
bool SetMousePositionClient(int32 x, int32 y);
void ShowMouse(bool show);
void CaptureMouse(bool capture);
void SetCursor(NkCursorType cursor);
void ClipMouseToClient(bool clip);
```

⚠️ **`SetMousePosition` et `SetMousePositionClient` n'ont pas le même contrat**, et l'en-tête
l'explique : sur Windows, `SetMousePosition` appelle `SetCursorPos`, donc des coordonnées
**écran** ; XCB et XLib font autrement. **`SetMousePositionClient` est celle dont le
contrat ne varie pas** — préférez-la.

### Lire (savoir où est la souris) : dans **NKEvent**

```cpp
#include "NKEvent/NkEventDispatcher.h"

int32 x = NkInput.MouseX();
int32 y = NkInput.MouseY();
```

**C'est le piège le plus fréquent.** L'écriture est dans NKWindow parce qu'elle *agit sur
la fenêtre* ; la lecture est dans NKEvent avec le reste de l'état d'entrée. Trois
utilisateurs ont cherché le lecteur là où se trouve l'écrivain.

**Depuis le 26/09, chaque en-tête renvoie vers l'autre.** Le détail est dans le guide
NKEvent.

### 🔴 `SetCursor` est PERSISTANT — la cause d'un « ça ne marche pas » très fréquent

```cpp
enum class NkCursorType {
    Arrow, TextInput, Hand,
    ResizeNS, ResizeWE, ResizeNWSE, ResizeNESW
};
```

> **Appelez `SetCursor` à CHAQUE image**, avec la forme voulue pour la zone survolée.
> Appelé une seule fois, le système remet la flèche **au premier mouvement de souris**.

Ce n'est pas un défaut : Windows envoie un message de curseur à chaque déplacement, et la
dernière réponse gagne. La méthode **mémorise**, seul le rappel **agit**.

⚠️ **Et `SetCursor` n'existe que sur Windows.** Sur X11, Wayland et macOS, elle n'est pas
implémentée — ces appels écrivent aujourd'hui un **refus nommé** au journal, au lieu de ne
rien faire en silence. Le tableau complet est dans
**`wiki/Runtime/NKWindow/Curseur-par-dorsal.md`**.

**Une démo existe pour le voir** : `NkDemoCurseur` — quatre zones, quatre formes, et une
case « rappeler à chaque image » qu'on peut décocher pour reproduire le défaut à volonté.

---

## 6. Les pièges à connaître, en résumé

| piège | ce qu'il faut savoir |
|---|---|
| **Un réglage qui ne fait rien** | consultez la table par dorsal **avant** de chercher chez vous |
| **`SetCursor` sans effet** | il est persistant : rappelez-le chaque image |
| **Pas de getter pour la souris** | il est dans NKEvent, pas NKWindow |
| **`SetSize(GetSize())` fait grandir** | taille client contre taille fenêtre |
| **`IsMaximized` pour le plein écran** | ce n'est pas la même chose |
| **`clickThrough` au lieu de `noActivate`** | traverser ≠ ne pas voler le focus |
| **`argv[0]`** | c'est le nom du programme, pas un argument |

---

## 7. Où lire la suite

| fichier | ce qu'il contient |
|---|---|
| `Core/NkWindowConfig.h` | les réglages, chacun commenté |
| `Core/NkWindow.h` | toutes les méthodes, avec leur portée par plateforme |
| `Core/NkEntry.h`, `NKMain.h` | le point d'entrée multiplateforme |
| `wiki/Runtime/NKWindow/Proprietes-par-dorsal.md` | ce qui agit, par dorsal |
| `wiki/Runtime/NKWindow/Curseur-par-dorsal.md` | idem pour le curseur |
| `Applications/ImGuiRef/src/ImGuiRef/main.cpp` | un exemple complet et court |
| `Applications/Mou/src/Mou/main.cpp` | le squelette minimal, 31 lignes |

⚠️ **Les en-têtes de ce dépôt ne sont pas de la documentation générée** : ils expliquent
**pourquoi** une chose est ainsi, et souvent **quel défaut a été payé** pour l'apprendre.
Ils valent la lecture.

---

## 8. Si quelque chose ne marche pas

1. **Lisez `logs/app.log`.** Beaucoup de refus s'y écrivent avec leur raison — et depuis
   le 25/09, les erreurs remontent aussi **à l'écran** dans les applications qui ont branché
   le puits d'affichage.
2. **Consultez la table par dorsal.** La moitié des « ça ne marche pas » signalés étaient
   des propriétés non implémentées sur la plateforme visée.
3. **Signalez-le.** Les trois derniers défauts corrigés dans NKWindow ont été trouvés par
   des utilisateurs, pas par l'équipe. Un « c'est normal que… ? » vaut mieux qu'une heure
   perdue.
