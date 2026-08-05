# Avant-propos

## À qui s'adresse ce cours

Ce cours s'adresse à une personne qui *sait déjà programmer en C++* et qui
ne connaît *rien* au moteur Nkentseu. C'est une distinction importante :
nous n'expliquerons pas ce qu'est un pointeur, une référence, une lambda ou un
constructeur. En revanche, nous expliquerons *tout* du moteur — y compris
les choses qui paraissent évidentes une fois qu'on les a comprises, et
particulièrement celles-là, parce que ce sont elles qui font perdre une soirée
quand personne ne les a écrites.

Ce que vous devez savoir avant d'ouvrir ce document :

- du C++ moderne, niveau C++17 : `auto`, lambdas, références,
  `constexpr`, `noexcept`, énumérations fortement typées
  (`enum class`) ;
- la notion de compilation séparée : en-têtes, unités de traduction,
  édition de liens ;
- ce qu'est un pointeur non possédant (*non-owning*) par opposition à
  un pointeur possédant. Ce cours en parlera souvent : le moteur en est
  rempli et c'est la première source de plantages au démarrage ;
- quelques notions vagues de rendu : un sommet (*vertex*), un
  triangle, une texture, une matrice de projection. Vous n'avez pas besoin
  de savoir écrire un *shader* : nous n'en écrirons aucun.

Ce que vous n'avez *pas* besoin de savoir : OpenGL, Vulkan, Direct3D,
Metal. Tout le cours se tient au-dessus de ces API ; le moteur les cache, et
c'est précisément son intérêt.

### Ce que ce cours couvre, et ce qu'il ne couvre pas

Les cinq premiers chapitres — ceux que vous tenez — forment un parcours complet
et fermé : à la fin du chapitre 4, vous savez écrire, compiler et lancer une
application graphique interactive complète avec le moteur. Le parcours est le
suivant :

1. **Chapitre 0** (celui-ci) : le dépôt, les règles du projet, la
   compilation.
2. **Chapitre 1** : la pile logicielle complète, de la fenêtre du
   système d'exploitation jusqu'aux boutons.
3. **Chapitre 2** : **NkCanvas**, le moteur de rendu 2D. Comment
   on dessine des triangles, et ce que le module ne sait *pas* faire.
4. **Chapitre 3** : **NKGui**, le framework d'interface. C'est le
   cœur du cours et le chapitre le plus long.
5. **Chapitre 4** : construire une application finie, du fichier de
   build jusqu'à l'exécutable qui s'affiche.

Ne sont *pas* couverts ici : le rendu 3D (NKRenderer, NKRHI), l'audio
(NKAudio), la vidéo (NKMedia), le réseau (NKNetwork), l'intelligence
artificielle (Kernel/AI). Certains de ces modules font l'objet de chapitres
ultérieurs de ce même cours.

> **✅ Un avertissement d'honnêteté**
>
> Ce cours dit aussi ce qui *ne marche pas*. Le moteur est un projet vivant :
> certains backends graphiques sont validés à l'exécution, d'autres non ; certaines
> fonctions déclarées retournent une valeur neutre. Chaque fois que c'est le cas,
> il est écrit noir sur blanc, avec le fichier et la ligne. Un cours qui promet un
> moteur parfait vous ferait perdre plus de temps qu'il ne vous en ferait gagner.

## Comment lire les exemples

Tous les blocs de code de ce cours portent leur **provenance** en titre,
sous la forme `chemin/vers/fichier.cpp:ligne`. Ce n'est pas de la
coquetterie : cela signifie que vous pouvez ouvrir le fichier et vérifier. Les
extraits sont pris tels quels dans le dépôt, parfois abrégés (les coupures sont
signalées par des points de suspension ou un commentaire). Quand un exemple est
*écrit pour le cours* et n'existe pas tel quel dans le dépôt, c'est dit
explicitement.

Trois encadrés reviennent régulièrement :

- **Ce qu'il faut retenir** — la phrase à emporter, si vous ne
  deviez retenir qu'une chose de la section ;
- **Le piège** — un comportement qui coûte une soirée. Presque tous
  ceux de ce cours sont issus de bugs réellement rencontrés dans le dépôt
  et documentés en commentaire dans les sources ;
- **Exercice** — à faire, pas à lire.

Le dépôt de référence est `D:/Projets/2026/Nkentseu/Nkentseu`. Tous les
chemins de ce cours sont relatifs à cette racine.

## Comment le dépôt est organisé

Le moteur est découpé en **modules**. Un module est un dossier autonome
avec ses sources, son fichier de build (`.jenga`) et, souvent, sa
documentation (`README.md`, `ARCHITECTURE.md`,
`ROADMAP.md`, `USAGE.md`). Les modules sont rangés par
**couche**, et le principe fondamental est énoncé dans
`ARCHITECTURE.md` : *chaque couche ne connaît que les couches situées
en dessous d'elle*.

### Kernel/Foundation — les briques de base

| **Module** | **Rôle** |
|---|---|
| `NKCore` | Types primitifs (`int32`, `float32`, `usize`…), macros, asserts |
| `NKMath` | `NkVec2/3/4`, `NkMat4`, `NkQuat`, `NkColor`, `NkRect` |
| `NKMemory` | Allocateurs, `NkUniquePtr` — *la* porte d'entrée de toute allocation |
| `NKContainers` | `NkVector`, `NkString`, `NkHashMap` |
| `NKPlatform` | Détection de plateforme à la compilation, macros `NKENTSEU_PLATFORM_*` |

Ces cinq modules n'ont *aucune* dépendance externe. Tout le reste du moteur
repose dessus.

### Kernel/System — les services système

`NKFileSystem` (chemins, fichiers), `NKLogger` (journalisation),
`NKStream` (lecture/écriture binaire et texte), `NKThreading`
(fils d'exécution, mutex, atomiques), `NKTime` (horloges, *delta
time*), `NKNetwork`, `NKReflection`, `NKSerialization`.

### Kernel/Runtime — les modules du moteur

C'est là que vivent les deux vedettes de ce cours :

- `Kernel/Runtime/NKCanvas` — le rendu 2D (chapitre 2) ;
- `Kernel/Runtime/NKGui` — le framework d'interface (chapitre 3).

et leurs voisins immédiats : `NKWindow` (la fenêtre native),
`NKEvent` (les événements typés), `NKFont` (rasterisation de
polices), `NKImage` (décodage PNG/JPG/SVG…), `NKRHI`
(abstraction GPU bas niveau), `NKRenderer` (rendu 3D), `NKAudio`,
`NKMedia`, `NKSL` (langage de *shaders*), `NKUI`
(l'ancien module d'interface, remplacé par NKGui).

### Engine et Integrations

`Engine/NKEditorKit` est une *coquille* d'application d'édition :
fenêtre sans décoration, barre de titre maison, docking, palette de commandes,
préférences. C'est ce qui permet à l'IDE `NKCode` de n'écrire que ses
panneaux. Ce cours ne s'en sert pas — nous construirons l'application à la main
au chapitre 4 — mais il faut savoir qu'il existe, parce que la plupart des
exemples réels du dépôt passent par lui.

`Integrations/NKGui` contient le pont alternatif entre NKGui et NKRHI
(`NkGuiRHIBackend`), pour les applications qui font de la 3D. Nous y
reviendrons au chapitre 1.

### Applications et Resources

`Applications/` contient toutes les applications et démos, une par
dossier. Trois nous serviront de référence tout au long du cours :

- `Applications/NKGuiDemo` — la démo de référence de NKGui
  (environ 1 067 lignes dans un seul `main.cpp`). Elle prouve le
  pipeline complet et couvre presque toute l'API. C'est le fichier à ouvrir
  à côté de ce cours ;
- `Applications/Sandbox/src/DemoNkentseu/NkCanvas/NkCanvasDemo.cpp`
  — la démo NkCanvas *seul*, sans interface, à laquelle nous
  consacrerons la fin du chapitre 2 ;
- `Applications/NKCode` — l'IDE complet, l'application la plus
  grosse du dépôt. Elle sert d'étude de cas.

`Resources/` contient les données partagées (polices, textures, icônes,
modèles, shaders). `Documentation/` et `Guides/` contiennent la
documentation écrite. `Build/` reçoit tout ce que la compilation produit,
et `logs/` les journaux d'exécution.

> **✅ Ce qu'il faut retenir**
>
> Le dépôt suit une règle simple : **un dossier = un module = un fichier
> `.jenga`**. Pour comprendre ce dont un module dépend, il suffit d'ouvrir
> son `.jenga` et de lire l'appel à `nkentseudependson`. Nous le
> ferons souvent : c'est le moyen le plus fiable de savoir qui connaît qui.

## Les outils de base du moteur

Avant d'écrire du code Nkentseu, il faut connaître la poignée d'outils
qu'on retrouve dans chaque fichier du dépôt. Ils viennent tous de
`Kernel/Foundation` et de `Kernel/System`, et ils reviendront à
toutes les pages de ce cours.

### Les types primitifs

Le moteur nomme explicitement la taille de ses entiers, dans
`NKCore` : `int8`, `int16`, `int32`,
`int64`, `uint8`, `uint16`, `uint32`,
`uint64`, `float32`, `float64`, et `usize` pour une
taille en octets. Ils sont disponibles dans l'espace de noms
`nkentseu`. Une largeur de fenêtre est un `uint32`, une
coordonnée un `float32`, un compteur de boucle un `int32` : on
lit la taille dans le nom, sans avoir à connaître la plateforme.

### Chaînes et conteneurs — `NKContainers`

| **Type** | **Rôle** |
|---|---|
| `NkString` | chaîne de caractères, `CStr()` donne le `const char *` |
| `NkStringView` | vue non possédante sur une chaîne |
| `NkVector<T>` | tableau dynamique — le conteneur le plus utilisé du moteur |
| `NkHashMap` | table associative |

Les méthodes suivent la convention de nommage du moteur, en
`PascalCase` : `Size()`, `Data()`, `PushBack()`,
`PopBack()`, `Resize()`, `Reserve()`, `Clear()`,
`Erase()`, `Begin()`, `End()`. L'accès indexé s'écrit
comme partout, avec les crochets. Un parcours typique, tel qu'on en croise des
centaines dans le dépôt :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/UI/NkGuiCanvasBackend.h:122-125 (extrait)`**

```cpp
mScratch.Resize(dl.vtx.Size());
for (uint32 i = 0; i < dl.vtx.Size(); ++i) {
    const nkgui::NkGuiVertex &s = dl.vtx[i];
    renderer::NkVertex2D &d = mScratch[i];
```

Deux réflexes visibles ici et à reprendre : on dimensionne d'abord
(`Resize`), on remplit ensuite ; et on prend une *référence* sur
l'élément plutôt qu'une copie.

Pour construire une chaîne à partir de valeurs, le moteur fournit
`NkPrintf`, qui renvoie une `NkString` :

**`Applications/NKCode/src/NKCode/Editor/NkCodeEditor.h:4585-4586 (extrait)`**

```cpp
const NkString nb = NkPrintf("%d", i + 1);
const float32 nw = ctx.font->MeasureWidth(nb.CStr());
```

### Mathématiques — `NKMath`

Tout vit dans l'espace de noms `nkentseu::math` :
`NkVec2f`, `NkVec2i`, `NkVec2u`, `NkVec3f`,
`NkVec4f` pour les vecteurs ; `NkMat4f` pour les matrices ;
`NkQuat` pour les quaternions ; `NkRect2f`, `NkRect2i` pour
les rectangles ; `NkColor` pour les couleurs ; et les fonctions
trigonométriques `NkCos`, `NkSin`, etc. Les vecteurs se
construisent en agrégat, ce qui donne un code très compact :

**`Applications/Sandbox/src/DemoNkentseu/NkCanvas/NkCanvasDemo.cpp:90-92`**

```cpp
    math::NkVec2f ball{220.f, 200.f};
    math::NkVec2f vel{260.f, 215.f}; // pixels / seconde
    const float32 R = 42.f;
```

### Fichiers et chemins — `NKFileSystem`

`NkPath` manipule les chemins, `NkFile` lit et écrit. Deux appels
que ce cours croisera : `NkPath::GetExecutableDirectory()`, qui donne le
dossier de l'exécutable, et `NkFile::ReadAllBytes`, qui charge un fichier
entier — c'est ainsi que `NkFont` lit une fonte
(`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Resources/NkFont.cpp:65-81`).

### Mémoire — `NKMemory`

Toute la mémoire du moteur passe par `NKMemory`, dans l'espace de noms
`nkentseu::memory`. Deux outils suffisent pour ce cours :

- `memory::NkMakeUnique<T>(args…)` crée un objet possédé par un
  `NkUniquePtr<T>` : il se libère tout seul en fin de portée ;
- `memory::NkGetDefaultAllocator()` donne l'allocateur par défaut,
  dont les méthodes `New<T>(args…)` et `Delete(ptr)`
  servent quand on gère la durée de vie soi-même.

Il existe aussi des allocateurs spécialisés — linéaire pour les données « par
image », *pool* pour des objets identiques, arène pour un niveau de jeu —
détaillés dans `Guides/01-NKMemory.md`.

La règle qui va avec, et qui n'admet pas d'exception : **un objet obtenu
d'un allocateur se rend au même allocateur**. Le dépôt en porte la cicatrice :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Core/NkRenderer2DFactory.cpp:41-46`**

```cpp
// IMPORTANT : on alloue via l'allocateur NKMemory (NkGetDefaultAllocator)
// et NON via `new`. Raison : le NkUniquePtr<NkIRenderer2D> (CreateUnique)
// desalloue via NkDefaultDelete -> ce MEME allocateur (-> _aligned_free).
// Allouer avec `new` (malloc) puis liberer avec _aligned_free = mismatch
// d'allocateur -> heap corruption c0000374 au free du renderer (shutdown).
// cf BugReports/NKCanvas/heap-c0000374-renderer-alloc-mismatch.md.
```

> **⚠️ Le mélange d'allocateurs**
>
> Rendre à un allocateur un bloc qu'il n'a pas donné produit une
> **corruption de tas**. Sous Windows, cela se manifeste par un plantage
> `0xc0000374` — et, ce qui rend le bug diabolique, ce plantage survient
> *à la fermeture de l'application*, très loin de la ligne fautive. Le
> fichier `Kernel/Runtime/NKCanvas/src/NKCanvas/Factory/NkContextFactory.h`
> le dit en une ligne (`:39`) : « RÈGLE : ne JAMAIS faire `delete`
> (objet alloue par NKMemory) ». Un objet créé par une *factory* du moteur se
> détruit par la méthode `Destroy` correspondante, et par elle seule.

### Les habitudes à prendre

Voici la forme que prend tout cela dans le code que vous écrirez au chapitre 4 :

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:262-269 (extrait)`**

```cpp
auto target = memory::NkMakeUnique<NkRenderWindow>(window, desc);
if (!target || !target->IsValid())
    return -1;

auto ctxPtr = memory::NkMakeUnique<NkGuiContext>();
if (!ctxPtr)
    return -1;
NkGuiContext &ctx = *ctxPtr;
```

Trois habitudes à prendre immédiatement :

- on crée les objets à durée de vie longue avec
  `memory::NkMakeUnique<T>` : ils se libèrent seuls, au bon
  endroit ;
- on teste toujours le résultat. `NkMakeUnique` peut retourner un
  pointeur nul ; les constructeurs du moteur ne lancent pas d'exception —
  ils posent un état « invalide » qu'on interroge par `IsValid()` ;
- quand un objet est possédé par un `NkUniquePtr`, on passe partout
  ailleurs un *pointeur brut non possédant* obtenu par
  `.Get()`. Il faut alors garantir la durée de vie à la main.
  Nous verrons au chapitre 4 que c'est exactement le cas de la police.

> **✅ Ce qu'il faut retenir**
>
> `NkString` et `NkVector` pour les données, `math::Nk*` pour
> la géométrie, `NkFileSystem` pour les fichiers, `NKMemory` pour la
> mémoire : ce sont les outils du moteur, et ils suffisent. Les noms de méthodes
> sont en `PascalCase` et les tailles d'entiers sont écrites dans le nom du
> type.

## Compiler : le système Jenga

Le moteur n'utilise ni CMake, ni Meson, ni Visual Studio directement. Il utilise
**Jenga**, un système de build maison écrit en Python. Chaque module porte
un fichier `<NomDuModule>.jenga` qui est, littéralement, un script Python.

### À quoi ressemble un fichier `.jenga`

**`Applications/NKGuiDemo/NKGuiDemo.jenga:41-58 (extrait)`**

```cpp
with project("NKGuiDemo"):
    windowedapp()
    language("C++")
    cppdialect("C++17")
    location(".")

    files(["src/**.cpp"])

    nkentseudependson(
        ["NKGui", "NKReflection", "NKCanvas", "NKWindow", "NKEvent", "NKGlad",
         "NKFont", "NKImage", "NKStream", "NKFileSystem", "NKLogger", "NKMath",
         "NKTime", "NKContainers", "NKMemory", "NKCore", "NKPlatform", "NKThreading"],
        extra_includes=["src", "src/NKGuiDemo", "%{NKGlad.location}/include"],
    )

    objdir("%{wks.location}/Build/Obj/%{cfg.buildcfg}-%{cfg.system}/%{prj.name}")
    targetdir("%{wks.location}/Build/Bin/%{cfg.buildcfg}-%{cfg.system}/%{prj.name}")
```

Les quatre appels à connaître :

- `project("Nom")` — ouvre la déclaration d'une cible. Le nom est
  celui qu'on passera à `--target` ;
- `windowedapp()` — le type de cible : application fenêtrée. Une
  bibliothèque déclarerait `staticlib()` ;
- `files([...])` — les sources à compiler. Notez le motif
  `"src/**.cpp"` : *seuls les `.cpp`*. Un module
  entièrement en `.h` avec des fonctions `inline` ne compile
  rien du tout — c'est le cas du pont NKGui → NkCanvas, et
  c'est délibéré (chapitre 1) ;
- `nkentseudependson([...])` — la liste des modules dont on dépend.
  C'est *la* ligne à lire pour savoir qui connaît qui.

### Les deux commandes que vous taperez

**`Commande — à taper depuis la racine du dépôt`**

```
jenga build --target NKGuiDemo --config Release
jenga build --target NKGuiDemo --config Debug
```

L'exécutable produit atterrit à un emplacement déterminé par le
`targetdir` ci-dessus :

**`Chemin de l'exécutable produit (Windows)`**

```
Build\Bin\Release-Windows\NKGuiDemo\NKGuiDemo.exe
Build\Bin\Debug-Windows\NKGuiDemo\NKGuiDemo.exe
```

La configuration `Debug` garde les symboles et les assertions ;
`Release` optimise. Un réflexe du dépôt, énoncé dans les règles de
travail du projet : quand on livre un lot, on construit **les deux**
configurations, parce qu'un bug qui n'existe qu'en `Release` (ordre
d'initialisation, dépassement de tampon masqué) est le pire de tous.

> **⚠️ Lancer depuis la racine du dépôt**
>
> Beaucoup d'applications du dépôt cherchent leurs ressources par des chemins
> *relatifs au répertoire courant* : `Resources/Fonts/…`,
> `Applications/NKCode/data/textures/…`. Lancer l'exécutable depuis son
> propre dossier (`Build/Bin/…`) donne alors une fenêtre sans polices,
> sans icônes, parfois entièrement noire — et *aucun* message d'erreur
> explicite.
>
> La parade adoptée par le dépôt est de chercher dans plusieurs dossiers
> candidats, dont celui de l'exécutable, comme le documente
> `Applications/NKCode/src/NKCode/Shell/NkAppFonts.h:18-21` : « Candidats
> RELATIFS AU CWD (dev, lancement depuis la racine du repo) PUIS relatifs à
> l'EXECUTABLE ». Tant que vous êtes en développement : **lancez depuis la
> racine du dépôt**.

### Où sont les journaux

Le moteur journalise via `NKLogger`. La trace d'exécution est écrite dans
`logs/app.log`, *relativement au répertoire courant* — donc, si vous
lancez depuis la racine, dans `logs/app.log` à la racine du dépôt. La
trace précédente est conservée sous `logs/app.prev.log`.

C'est le premier endroit à regarder quand une application se lance et n'affiche
rien : le choix du backend graphique, les échecs de création de contexte, les
polices introuvables y sont écrits. Dans votre propre code, la journalisation
s'écrit ainsi :

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:319 et 1055 (extraits)`**

```cpp
logger.Info("Image chargee : {0}x{1}", imgW, imgH);
logger.Error("Capture ecran ECHEC ({0})", path);
```

(l'objet global `logger` vient de `NKLogger/NkLog.h` ;
il existe aussi des variantes `Infof`/`Warnf`/`Errorf` au
format `printf`).

## Le point d'entrée : `nkmain`, pas `main`

Dernière particularité à connaître avant d'écrire la moindre ligne : le moteur
fournit lui-même le `main` natif de chaque plateforme (`WinMain`
sous Windows, `main` ailleurs, un point d'entrée Java/JNI sur Android).
Votre programme, lui, déclare :

**`Applications/Sandbox/src/DemoNkentseu/NkCanvas/NkCanvasDemo.cpp:29-33 et 57`**

```cpp
NKENTSEU_DEFINE_APP_DATA(([]() {
    NkAppData d{};
    d.appName = "NkCanvas Demo";
    d.appVersion = "1.0.0";
    return d;
})());

int nkmain(const NkEntryState &state) {
```

Il faut pour cela inclure `NKWindow/NKMain.h`. Le paramètre
`state` porte notamment les arguments de la ligne de commande, sous forme
d'un `NkVector<NkString>` :

**`Applications/Sandbox/src/DemoNkentseu/NkCanvas/NkCanvasDemo.cpp:36-40 (extrait)`**

```cpp
static NkGraphicsApi ParseBackend(const NkVector<NkString> &args) {
    for (usize i = 1; i < args.Size(); ++i) {
        const NkString &a = args[i];
        if (a == "--backend=vulkan" || a == "-bvk")
            return NkGraphicsApi::NK_GFX_API_VULKAN;
```

> **✅ Ce qu'il faut retenir**
>
> Trois choses à retenir de ce chapitre, et rien d'autre :
>
> 1. Les outils du moteur : `NkString`, `NkVector`,
>    `math::Nk*`, `NkFileSystem`, et
>    `memory::NkMakeUnique` pour la mémoire.
> 2. `jenga build --target <Cible> --config Release` depuis la
>    racine, exécutable dans
>    `Build/Bin/Release-Windows/<Cible>/<Cible>.exe`, lancé
>    **depuis la racine**.
> 3. Le point d'entrée s'appelle `nkmain(const NkEntryState &)`, et
>    les journaux sont dans `logs/app.log`.

## Exercices

> **✏️ 1 — Reconnaître la carte**
>
> Sans compiler quoi que ce soit, ouvrez `Kernel/Runtime/NKGui/NKGui.jenga`
> et `Kernel/Runtime/NKCanvas/NKCanvas.jenga`. Relevez la liste passée à
> `nkentseudependson` dans chacun. Question : **NKGui dépend-il de
> NKCanvas ?** Notez votre réponse ; le chapitre 1 y revient et en tire toute une
> architecture.

> **✏️ 2 — Le premier build**
>
> Construisez et lancez la démo de référence :
>
> ```
> cd D:\Projets\2026\Nkentseu\Nkentseu
> jenga build --target NKGuiDemo --config Release
> .\Build\Bin\Release-Windows\NKGuiDemo\NKGuiDemo.exe
> ```
>
> Puis relancez-la depuis *son propre dossier* (`cd Build\Bin\Release-Windows\NKGuiDemo`
> puis `.\NKGuiDemo.exe`) et comparez : qu'est-ce qui change à
> l'écran ? Ouvrez ensuite `logs/app.log` et retrouvez la ligne qui indique
> le backend graphique retenu.

> **✏️ 3 — Chasse aux allocations**
>
> Cherchez dans `Kernel/Runtime/NKCanvas/src/` toutes les occurrences de
> `NkGetDefaultAllocator`. Pour chacune, identifiez qui libère l'objet et
> par quel appel. Vous devriez tomber sur le destructeur du pont NKGui, que le
> chapitre 1 détaille :
> `Kernel/Runtime/NKCanvas/src/NKCanvas/UI/NkGuiCanvasBackend.h:24-32`.
