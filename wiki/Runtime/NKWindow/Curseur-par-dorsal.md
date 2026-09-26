# Le curseur : ce que chaque dorsal tient vraiment

**AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**

> Couche **Runtime** · NKWindow · La table de vérité **méthode curseur × dorsal**.
>
> Elle existe parce qu'un utilisateur a rapporté que « **les méthodes pour curseur ne fonctionnent
> pas** ». Ce document est ce qui lui manquait : il aurait su en dix secondes **ce qui agit chez
> lui**, et ce qui ne peut pas agir.
>
> Même règle que pour `NkWindowConfig` : **une méthode qui s'accepte sans agir est pire qu'une
> méthode absente.** L'absence se voit à la compilation ; le silence se paie en heures perdues.

## Comment lire la table

| Marque | Sens |
|---|---|
| **agit** | le dorsal fait l'appel natif et produit l'effet promis |
| **mémorise** | l'état est rangé dans la structure, **et quelqu'un le lit** plus tard pour produire l'effet |
| **silence** | accepté, rien ne se passe, **aucun message** — le défaut que ce document existe pour tuer |
| **autre effet** | le dorsal fait quelque chose, mais **pas ce que le nom promet** |
| **s/o** | sans objet : la plateforme n'a pas de curseur (mobile, web tactile, console) |

---

## La table

| | Win32 | XLib | XCB | Wayland | Cocoa | Android · UIKit · HarmonyOS · UWP · Xbox · Noop | Emscripten |
|---|---|---|---|---|---|---|---|
| **`SetCursor`** (forme) | **agit** | ⚠️ **silence** | ⚠️ **silence** | ⚠️ **silence** | ⚠️ **silence** | s/o | s/o |
| **`ShowMouse`** | **agit** | **agit** | **agit** | **mémorise** (lu par l'EventSystem) | **agit** | silence | **agit** |
| **`CaptureMouse`** | **agit** | ⚠️ **silence** | ⚠️ **silence** | ⚠️ **silence** (rangé, jamais lu) | **autre effet** | silence | **agit** |
| **`ClipMouseToClient`** | **agit** | **agit** | **agit** | ⚠️ **silence** (rangé, jamais lu) | **autre effet** | silence | **agit** |
| **`SetMousePosition`** | **agit** (écran) | **agit** | **agit** | **agit** | **agit** | silence | silence |
| **`SetMousePositionClient`** | **agit** | **agit** | **agit** | **agit** | **agit** | rend `false` | **agit** |

### ⚠️ Les quatre cases qui expliquent la plainte

**1. `SetCursor` n'existe QUE sur Windows.** Le fichier `Core/NkWindowCursor.cpp` est bâti ainsi :

```cpp
#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(UWP) && !defined(Xbox)
    … implémentation complète …
#else
    void NkWindow::SetCursor(NkCursorType) {
        // Plateformes sans curseur souris (Android, iOS, Web tactile, headless).
    }
#endif
```

Le commentaire du `#else` nomme « Android, iOS, Web tactile, headless ». **Mais la condition attrape
tout ce qui n'est pas Windows** — donc **X11, Wayland et macOS**, qui ont pourtant un curseur et
savent le changer. *Un `#else` ne dit pas ce qu'il contient : il contient tout le reste.*

L'en-tête annonce d'ailleurs « No-op sur mobile/web (sans curseur) » : **c'est faux**, et c'est cette
phrase que l'utilisateur a lue.

**2. `CaptureMouse` est vide sur XLib et XCB** — corps entièrement vide, pas même un `(void)capture;`.
Alors que `ShowMouse` et `ClipMouseToClient` y sont, eux, implémentés. **Ne traitez donc pas les
méthodes curseur en bloc** : sur X11, deux marchent et une est muette.

**3. Sur Wayland, `CaptureMouse` et `ClipMouseToClient` rangent un booléen que personne ne lit.**
`mMouseCaptured` est écrit en deux endroits et lu **nulle part** — le commentaire du code le dit
lui-même : « pour usage futur ». `mMouseHidden`, lui, **est** lu par l'EventSystem : c'est pourquoi
`ShowMouse` agit et les deux autres non.

**4. Sur Cocoa, `CaptureMouse` et `ClipMouseToClient` appellent la même fonction** —
`CGAssociateMouseAndMouseCursorPosition`. Elle **découple** le curseur du mouvement physique ; elle
ne **confine** pas le curseur à la zone client. Le nom promet un rectangle, l'appel donne autre
chose.

---

## `SetCursor` est PERSISTANT — et c'est la cause la plus probable sur Windows

```cpp
// Persistant : à rappeler chaque frame avec le curseur voulu (sinon, sur
// certaines plateformes, le système le réinitialise à la flèche).
void SetCursor(NkCursorType cursor);
```

Sur Win32, le système envoie `WM_SETCURSOR` **à chaque mouvement de souris** et réinitialise la forme.
NKWindow mémorise le curseur voulu (`mData.mClientCursor`) et le réapplique dans ce message — mais
**seulement quand le curseur est dans la zone cliente** (`LOWORD(lp) == HTCLIENT`).

**Ce que ça veut dire pour qui l'utilise :**

- appeler `SetCursor` **une seule fois** au démarrage : la forme tient tant que la souris ne quitte
  pas la fenêtre, puis revient à la flèche → **on conclut que « ça ne marche pas »** ;
- l'appel immédiat est **volontairement conditionné** au survol de notre fenêtre : sans cette garde,
  une application en arrière-plan écrasait en continu le curseur de la fenêtre au premier plan
  (clignotement constaté).

> **Si vous appelez `SetCursor` alors que la souris n'est pas au-dessus de votre fenêtre, rien ne
> change à l'écran — et c'est voulu.** La forme est mémorisée et s'appliquera au premier
> `WM_SETCURSOR`.

---

## Ce qu'il faut faire, selon ce qu'on observe

| ce que vous voyez | ce que c'est | ce qu'il faut faire |
|---|---|---|
| la forme revient à la flèche dès qu'on bouge | contrat **persistant** | rappeler `SetCursor` **dans la boucle**, avec la forme voulue pour la zone survolée |
| rien ne change, jamais, sur **Linux ou macOS** | `SetCursor` n'y est **pas implémenté** | rien à faire côté appelant — c'est chez nous |
| `CaptureMouse` sans effet sur **X11 ou Wayland** | **silence** | idem |
| `ClipMouseToClient` sans effet sur **Wayland** | **silence** | idem |
| le curseur change dans le client mais pas sur la bordure | `HTCLIENT` seulement | normal : hors zone cliente, le système reprend la main (flèches de redimensionnement) |

---

## Ce que ce document engage

**Tout ce qui est marqué ⚠️ *silence* doit devenir un *refus nommé*** — un message au journal, une
fois par méthode et par plateforme, disant ce qui a été demandé et ne sera pas livré. C'est la règle
déjà appliquée au presse-papiers image, au codec WebP et aux propriétés de fenêtre.

Le refus ne se déclenche **que si l'appelant a demandé quelque chose** : une application qui
n'appelle rien ne doit voir aucun avertissement.
