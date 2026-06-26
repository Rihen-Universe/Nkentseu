# NKEditorKit — Architecture

## Intention

Construire **une seule fois** la coquille d'un éditeur (fenêtre, docking, panneaux,
menus, palette de commandes, thème) et la partager entre tous les éditeurs
Nkentseu. Aujourd'hui elle alimente **NKCode** (IDE / éditeur de code) ; elle est
conçue pour accueillir **Nogee** (éditeur de moteur de jeu) plus tard, qui pourra
migrer ses panneaux dessus sans réécrire sa plomberie.

Principe directeur : **assembler, ne pas réinventer.** NKUI fournit déjà un système
de docking complet (`NkUIDockManager` : arbre de splits binaires, drag & drop,
onglets, surbrillance de bord, sérialisation de layout), un gestionnaire de
fenêtres (`NkUIWindowManager`), et tous les widgets. NKEditorKit se contente de les
**orchestrer** dans une boucle d'application d'édition.

## Position dans les couches

```
Foundation → System → Runtime ─┬─ NKCanvas (2D)  ─┐
                               ├─ NKUI (docking)  ─┼─→ NKEditorKit ─→ NKCode (IDE)
                               └─ NKFont/NKImage  ─┘                └→ Nogee (futur)
                               (NKRenderer 3D ──→ Noge : indépendant, non requis ici)
```

NKEditorKit vit dans `Engine/` aux côtés de Noge, mais **ne dépend pas** de Noge
ni de NKRenderer : c'est un outil 2D bâti sur NKCanvas/NKUI. Un éditeur de code
n'embarque pas le moteur 3D.

## Composants

### `NkEditorShell` — la coquille (propriétaire de tout)

Possède et orchestre, sur la pile de l'application **(à allouer sur le tas)** :

- `NkWindow` + `NkRenderWindow` (NKCanvas) — fenêtre + contexte GPU + renderer 2D ;
- `NkUIContext`, `NkUIWindowManager`, `NkUIDockManager`, `NkUILayoutStack` — l'état NKUI ;
- `NkUICanvasBackend` — pont NKUI → NKCanvas (rend les draw lists) ;
- la **boucle principale** : `events → BeginFrame → menubar → docking → panneaux → palette → EndFrame → Submit → Display`.

> **Allocation tas obligatoire.** Les gestionnaires NKUI sont de gros objets par
> valeur (fenêtres × 64, nœuds de dock × 128…), soit > 1 Mo. Créer un
> `NkEditorShell` sur la **pile** déclenche un *stack overflow*. Toujours passer par
> `memory::NkMakeUnique<NkEditorShell>()` (NKMemory).

La boucle reproduit **fidèlement** le pipeline NKUI validé (démo `Sandbox/Base04`
`NkUIDemo`, et l'app `Nkoung`) : la barre de menus est dessinée sur la couche
*overlay*, le dock sur la couche *windows*, chaque panneau est soit **ancré**
(`NkUIDockManager::BeginDocked/EndDocked`) soit **flottant**
(`NkUIWindow::Begin/End`) — le shell gère la bascule selon l'état `isDocked`.

### `NkEditorPanel` — un panneau

Classe de base abstraite : un titre, un drapeau d'ouverture (piloté par le menu
« Affichage »), un côté de docking par défaut, et `OnUI(NkEditorFrameContext&)`
à implémenter. Le shell crée/dessine la fenêtre ; le panneau ne fait que **remplir
son contenu**. Pas de dépendance NKReflection : le contenu est dessiné
impérativement (les panneaux génériques pilotés par réflexion viendront plus tard).

### `NkEditorFrameContext` — le contexte de frame

Agrège l'état NKUI d'une frame (contexte, gestionnaires, layout, police, draw list)
et expose des **helpers** (`Text`, `Button`, `Checkbox`, `SliderFloat`,
`LabelValue`, `Separator`) qui encapsulent les appels NKUI verbeux. C'est la couche
d'**ergonomie** : un panneau écrit `ec.Button("OK")` au lieu de
`nkui::NkUI::Button(ctx, ls, dl, font, "OK")`. Pour les besoins avancés, les
objets NKUI restent accessibles (`ec.Ui()`, `ec.Draw()`, `ec.Layout()`).

### `NkEditorCommand` + palette de commandes

Modèle « tout est une commande nommée » (façon VSCode). Une commande = un libellé,
un raccourci affiché, un callback `void(*)(void*)` + contexte. La **palette
(Ctrl+P)** est un overlay listant les commandes (clic ou flèches + Entrée pour
exécuter). C'est le **point d'extension** futur : `NKCode/Extensions` enregistrera
ses commandes (et ses panneaux) via la même API → chargement de paquets externes
« façon VSCode ».

## Boucle de rendu (résumé)

```
mClock.Tick()                         # delta time
mUIInput.BeginFrame() ; poll events   # souris/clavier → état NKUI + pointeur unifié
mUI.BeginFrame(input, dt)
  ├─ bg layer : fond
  ├─ BuildMenuBar()                    # Fichier / Affichage / Fenêtre (+ app)
  ├─ dock.SetViewport / BeginFrame / Render
  ├─ DrawPanels()                      # chaque panneau : docké ou flottant → OnUI()
  ├─ BootstrapDocking()                # 1re frame : ancre les panneaux (centre puis côtés)
  └─ DrawCommandPalette()              # overlay si Ctrl+P
mUI.EndFrame()
backend.Submit(mUI) ; renderTarget.Display()
```

## Décisions & dette assumée

- **Disposition par défaut** : LEFT et BOTTOM se fractionnent proprement ; le split
  RIGHT du `NkUIDockManager` reste à affiner → on regroupe le centre en onglets en
  attendant. Réarrangeable par glisser-déposer. À terme : constructeur d'arbre de
  dock dédié ou layout JSON par défaut chargé via `LoadLayout`.
- **Réflexion différée** : inspecteur générique, éditeur de nœuds sérialisable et
  UI-builder attendront la maturité de **NKReflection ↔ NKSerialization** (clé de
  voûte commune éditeur + assets, cf. `ECOSYSTEM.md`). NKEditorKit livre d'abord la
  moitié *sans réflexion* (coquille, docking, commandes), immédiatement utile à NKCode.
- **Mémoire** : exclusivement NKMemory (`NkMakeUnique`, `NkUniquePtr`) — jamais
  `new`/`delete`. Le shell se possède via `NkUniquePtr` ; il ne possède pas les
  panneaux (durée de vie garantie par l'appelant).
