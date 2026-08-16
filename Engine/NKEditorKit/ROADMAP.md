# NKEditorKit — ROADMAP / journal des décisions

> Créé le 2026-08-17 par l'agent Noge, à l'occasion du correctif d'occlusion de
> la palette de commandes. Le kit n'avait pas de ROADMAP : `ARCHITECTURE.md`
> décrit la structure, `README.md` l'usage, et rien ne consignait les **décisions
> et leurs conséquences pour les applications consommatrices**.

## 🔎 Les consommateurs de ce module (à connaître avant d'y toucher)

`NkEditorShell` est **partagé**. Includes actifs mesurés le 2026-08-17 :

| application | fichiers incluant NKEditorKit |
|---|---|
| **NKCode** | 25 |
| **ConquerorLab** | 8 |
| **NK3DModeler** | 7 |
| **NkAnimaEditor** | 2 |
| NKEditorKitDemo | 2 |
| NKEditMeshHarness | 1 |
| **Nogee** | chemin optionnel `--ui=rhi` (depuis le 2026-08-17) |

**Toute modification du shell arrive chez ces applications sans qu'elles l'aient
demandée.** C'est la raison d'être de ce fichier.

---

## 2026-08-17 — La palette Ctrl+P déclare enfin son occlusion (couche 50)

### Ce qui était faux

`DrawCommandPalette` peignait un **voile plein écran** (`kBackdrop`, l. 2093)
sans jamais appeler `PushOcclusion` ni `NkInputLayerScope`. Le voile était donc
**purement visuel** : un widget d'un panneau ancré (couche 0) restait
**survolable et cliquable dessous**, parce que `ItemHoverable` consulte
`PointReachable` et que rien ne lui avait déclaré cette surface.

**L'asymétrie qui l'a fait voir** — dans le **même kit** :

| surface | déclaration |
|---|---|
| `NkEditorContextMenu.h:165` | `PushOcclusion(box, 50)` ✅ |
| `NkEditorModal.h:192` | `PushOcclusion(box, 100)` ✅ |
| `NkFilePicker.h:539` | `PushOcclusion(plein écran, 100)` ✅ |
| **palette de commandes** | **rien** ❌ |

### Le correctif

Deux lignes, purement additives, sur le patron exact des trois voisines :

```cpp
mUI.PushOcclusion({0.f, 0.f, W, H}, 50);
NkGuiContext::NkInputLayerScope _paletteLayer(mUI, 50);
```

Couche **50** = « menus/palettes/popovers » selon la légende de
`NkGuiContext.h`.

### La mesure, et son témoin

Banc : `Nogee --ui=rhi --occlusion-test` (sonde interrogeant `ItemHoverable`
depuis un **panneau ancré réel**).

```
                        AVANT        APRÈS
palette FERMÉE            1            1     <- le témoin sait toujours dire OUI
palette OUVERTE           1            0     <- le clic ne traverse plus
occlCount, palette ouverte 0            1  (layer=50)
```

⚠️ **Le témoin est ce qui rend ce tableau lisible.** Une première sonde, posée
dans le hook `SetOverlay`, rendait 0 sous le voile — « ça bloque bien ». Rejouée
**palette fermée**, elle rendait 0 **aussi** : elle mesurait le clip de
l'overlay, pas la palette. Il a fallu la refaire depuis un panneau ancré pour
qu'un 0 veuille dire quelque chose.

### ⚠️ CE QUE CE CORRECTIF CHANGE POUR LES QUATRE APPLICATIONS

**Un clic qui passait à travers le voile ne passe plus.** C'est la correction
d'un défaut, mais c'est **un changement de comportement** : une application qui
s'appuyait, sciemment ou non, sur ce clic traversant verra un widget cesser de
répondre pendant que la palette est ouverte.

**Le sens de la panne justifie de l'avoir livré** : le défaut corrigé était
**silencieux** (un clic atteint un widget censé être couvert, personne n'est
prévenu) ; la panne éventuelle est **bruyante** (un clic ne répond pas, visible
au premier essai). On échange un défaut muet contre une panne qui se signale.

⚠️ **Mesuré dans Nogee UNIQUEMENT.** NKCode, ConquerorLab, NK3DModeler et
NkAnimaEditor **n'ont pas été relancés**. Si l'une d'elles se comporte
autrement, ce paragraphe est le point de départ de l'enquête, pas une surprise.

### 🔬 Ce dont le correctif NE dépend PAS — vérifié, parce que l'inverse a été avancé

Il a été suggéré que ce correctif serait sans effet dans **ConquerorLab**, parce
que `ConquerorLab/main.cpp:219` appelle `SetMaskBodyOnPopup(false)`.
**Mesure du code : c'est faux, et il vaut mieux que ce soit écrit ici.**

Le shell a **deux mécanismes d'étanchéité distincts** :

| mécanisme | où | désactivable par l'app ? |
|---|---|---|
| **blanchiment de l'entrée du corps** — `modal` ⇒ `mUI.input.mousePos = {-100000,-100000}` + boutons effacés | `NkEditorShell.cpp:693-707` | **oui, partiellement** : `mMaskBodyOnPopup` ne neutralise que le terme `overPopup` |
| **routeur d'occlusion** — `PushOcclusion` ⇒ `PointReachable`, **première porte** de `ItemHoverable` | `NkGuiContext.cpp:491-497` | **non** |

`SetMaskBodyOnPopup(false)` n'agit que sur `overPopup`, donc uniquement sur le
**premier** mécanisme. **Le correctif de la palette passe par le second, que
rien côté application ne désactive.** Il fonctionne donc dans ConquerorLab.

> 🎯 **À retenir avant de retirer cette ligne un jour** : si elle semble sans
> effet quelque part, ce n'est pas `SetMaskBodyOnPopup` qu'il faut regarder,
> c'est `curInputLayer` — `PointReachable` ne bloque que les couches
> **strictement supérieures** à celle en cours de dessin.

---

## 2026-08-17 — La fenêtre Préférences n'a PAS ce défaut (rétractation)

**Écrit parce qu'une conclusion fausse a failli produire un correctif inutile
dans un module partagé.**

`DrawPreferences` peint elle aussi un voile plein écran sans `PushOcclusion` —
la ressemblance avec la palette est frappante, et une première mesure a bien
affiché « le clic traverse le voile ».

**Elle était fausse, et c'est l'instrument qui mentait.** L'expression de
`NkEditorShell.cpp:693` :

```cpp
const bool modal = mShowPrefs || mUI.appModal || overPopup || mCtxOpen;
```

**`mShowPrefs` y figure ; la palette, non.** Quand les Préférences sont
ouvertes, le shell **blanchit déjà l'entrée du corps** (`mousePos` à
`-100000`) : les panneaux sont étanches. La sonde, elle, **forçait**
`input.mousePos` avant d'appeler `ItemHoverable` — elle défaisait donc
exactement la protection qu'elle prétendait mesurer.

**Contrôle qui a tranché** : relever la souris **reçue** par le panneau *avant*
tout forçage.

```
palette ouverte      : souris reçue = normale   -> la fuite était RÉELLE
Préférences ouvertes : souris reçue = -100000   -> VERDICT NUL
```

**Aucune ligne n'a été ajoutée à `DrawPreferences`.** Les deux surfaces se
ressemblent à l'œil et sont protégées par deux mécanismes différents : la
palette par aucun (jusqu'à aujourd'hui), les Préférences par le blanchiment
d'entrée.

> 🎯 **La leçon, pour la prochaine surface flottante ajoutée au shell** :
> demander **par lequel des deux mécanismes** elle est étanche. Ne pas déclarer
> d'occlusion est légitime *si* la surface figure dans l'expression `modal` ;
> sinon, il faut la déclarer. **Ce qu'il ne faut pas, c'est ni l'un ni l'autre —
> c'était le cas de la palette.**
