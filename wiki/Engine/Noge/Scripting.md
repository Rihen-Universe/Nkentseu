# Le scripting

> Couche **Engine** · Noge · Donner un **comportement** aux entités ECS : la classe de base
> native C++ `NkScriptComponent`, le composant porteur `NkScriptHost`, les trois systèmes
> d'exécution, et les passerelles vers les langages externes (DLL hot-reload, C# Mono, Python).

Une fois qu'on a des entités et des composants (les données), il manque le **verbe** : ce qui se
passe à chaque frame. Une porte qui s'ouvre, un ennemi qui patrouille, une caméra qui suit le
joueur, un effet qui pulse — tout cela, c'est du *comportement*, et c'est le rôle du scripting.
L'approche de Noge est celle qu'on connaît des moteurs grand public (le `MonoBehaviour` d'Unity) :
on dérive une classe, on remplit des *hooks* de cycle de vie (`OnStart`, `OnUpdate`…), et le moteur
les appelle au bon moment. La question n'est jamais « comment coder un comportement » mais « dans
**quel langage** je l'écris » — du C++ natif pour la boucle chaude, ou un langage de script
(DLL/C#/Python) pour itérer vite et recharger à chaud.

Avant tout, **un avertissement de cap.** Cette famille est aujourd'hui **header-only et en grande
partie SPEC / stub** : toute la logique vit *inline* dans les `.h`, il n'y a **aucun `.cpp`** dans
le dossier, et plusieurs fonctions clés (hot-reload de DLL, horodatages, hot-reload C#/Python) sont
des stubs explicites (`return 0; // stub`). Les backends C#/Python ne font quoi que ce soit que si
leur macro est définie ; sinon ils tombent en *no-op*. Ce qui marche réellement aujourd'hui, c'est
le **chemin natif C++** (dériver `NkScriptComponent`, attacher via `NkScriptHost::Attach`, exécuter
via les trois systèmes). Le reste est une charpente à finir — la page le signale à chaque endroit
concerné, ainsi que plusieurs incohérences de code à corriger.

- **Namespace** : `nkentseu::ecs`
- **Headers réels** :
  - `Noge/ECS/Scripting/NkScriptComponent.h`
  - `Noge/ECS/Scripting/NkScriptSystem.h`
  - `Noge/ECS/Scripting/NkScriptBridge.h`
  - `Noge/ECS/Scripting/CSharp/NkScriptCSharp.h`
  - `Noge/ECS/Scripting/Python/NkScriptPython.h`

---

## Le comportement natif : `NkScriptComponent`

C'est le **cœur** du système, et le seul chemin pleinement opérationnel. On dérive
`NkScriptComponent`, classe abstraite calquée sur `MonoBehaviour`, et on redéfinit les *hooks* qui
nous intéressent. Le moteur les appelle dans un ordre fixe au fil de la vie de l'entité :
`OnAwake`, puis `OnStart` (une seule fois, au premier passage), puis `OnUpdate` à chaque frame,
`OnLateUpdate` après tous les `OnUpdate`, et `OnFixedUpdate` au rythme fixe de la physique ;
`OnEnable`/`OnDisable` encadrent l'activation, `OnDestroy` clôt la vie de l'objet. Chaque hook reçoit
le **monde** (`NkWorld&`) et l'**entité porteuse** (`NkEntityId self`), de sorte qu'un script peut
lire et modifier ses propres composants ou ceux des autres.

Tous les hooks sont `virtual ... noexcept {}` — vides par défaut, on ne redéfinit que ce qu'on
utilise. **Une seule méthode est obligatoire** : `GetTypeName()`, *pure virtual*, qui renvoie le nom
du type (`const char*`). C'est ce nom qui sert d'identité au script dans le registre et la
sérialisation. Deux autres virtuels, `Serialize(char*, uint32)` et `Deserialize(const char*)`,
permettent à un script de sauver/restaurer son état (vides par défaut).

```cpp
class DoorOpener final : public NkScriptComponent {
public:
    const char* GetTypeName() const noexcept override { return "DoorOpener"; }

    void OnUpdate(NkWorld& world, NkEntityId self, float32 dt) noexcept override {
        if (mOpening) mAngle += 90.f * dt;   // ouvre la porte de 90°/s
    }
private:
    bool    mOpening = false;
    float32 mAngle   = 0.f;
};
```

Côté état, `SetEnabled(bool)` / `IsEnabled()` activent ou coupent le script, et `HasStarted()` dit
si `OnStart` a déjà eu lieu. Les membres `protected` `mEnabled` et `mStarted` portent cet état ;
`NkScriptSystem` est déclaré `friend` pour pouvoir poser `mStarted = true` au bon moment.

Ce n'est **pas** un composant ECS à part entière : on n'attache pas un `NkScriptComponent`
directement à une entité. Il vit **à l'intérieur** d'un `NkScriptHost` (ci-dessous), lui-même
enregistré comme composant. Et ce n'est **pas** non plus le lieu des collisions : la classe de base
n'a **aucun** hook `OnCollision*` / `OnTrigger*` — ces hooks n'existent que côté DLL
(`NkScriptDLLBase`) et dans la table de fonctions C (`NkScriptVTable`). C'est une source de bugs
réelle décrite plus bas.

> **En résumé.** `NkScriptComponent` = la classe de base style MonoBehaviour. On redéfinit les hooks
> utiles (`OnStart`/`OnUpdate`/…), on **doit** implémenter `GetTypeName()`. Aucun hook collision dans
> la base, et un script ne vit jamais seul : il s'attache à un `NkScriptHost`.

---

## Le porteur et le registre : `NkScriptHost`, `NkScriptRegistry`

Sur une entité, c'est le **`NkScriptHost`** qui est le vrai composant ECS (déclaré via
`NK_COMPONENT(NkScriptHost)`). Il agit comme un **conteneur de scripts** : jusqu'à
`kMaxScripts = 32` instances de `NkScriptComponent` cohabitent sur une même entité, ce qui permet de
composer un comportement à partir de plusieurs petits scripts. On attache un script avec le template
`Attach<T>(args…)` : il construit l'instance, la range dans le tableau interne, renvoie le pointeur
brut (ou `nullptr` si plein) et arme `pendingStart = true` pour que `OnStart` se déclenche au
prochain tour. On retrouve un script déjà attaché avec `Get<T>()` (premier castable en `T` via
`dynamic_cast`), on teste sa présence avec `Has<T>()`, et on active/coupe tous les scripts d'un coup
avec `SetAllEnabled(bool)`.

```cpp
NkScriptHost& host = world.Add<NkScriptHost>(entity);
host.Attach<DoorOpener>();          // construit + arme OnStart
host.Attach<HoverSound>();          // plusieurs scripts sur la même entité
if (auto* d = host.Get<DoorOpener>()) d->SetEnabled(true);
```

Le compteur de scripts s'appelle `count` (et non `scriptCount`). **Ce détail compte** : trois
fonctions libres d'attache (`AttachDLLScript`, `AttachCSharpScript`, `AttachPythonScript`) écrivent
`host.scriptCount`, qui **n'existe pas** — elles ne compilent donc pas en l'état contre ce
`NkScriptHost` (piège #1, signalé plus bas). Les systèmes d'exécution, eux, bouclent correctement
sur `count`.

À côté du porteur, **`NkScriptRegistry`** est un **singleton** (`NkScriptRegistry::Global()`) qui
permet d'instancier un script **par son nom**, en chaîne de caractères. C'est indispensable pour
l'éditeur et le chargement JSON : on lit `"DoorOpener"` dans une scène, on appelle
`Instantiate("DoorOpener")`, on obtient l'instance — sans connaître le type à la compilation. On
peuple ce registre soit explicitement avec `Register<T>("Nom")`, soit, plus commodément, avec la
macro **`NK_REGISTER_SCRIPT(Type)`** à poser au scope fichier : elle crée un objet statique anonyme
qui s'enregistre tout seul au chargement du module. Le registre a une capacité fixe
(`kMaxEntries = 512`) et reste **silencieux** s'il est plein ou si un nom est introuvable
(`Instantiate` renvoie alors `nullptr`).

> **En résumé.** `NkScriptHost` = le composant ECS qui porte jusqu'à 32 scripts ; `Attach<T>` pour
> ajouter, `Get<T>`/`Has<T>` pour retrouver. Le compteur réel est `count`. `NkScriptRegistry` +
> `NK_REGISTER_SCRIPT` permettent l'instanciation **par nom** (éditeur, JSON).

---

## L'exécution : `NkScriptSystem`, `NkScriptLateSystem`, `NkScriptFixedSystem`

Attacher un script ne suffit pas — il faut que **quelqu'un appelle ses hooks**. C'est le rôle de
trois systèmes ECS (`final`, héritant de `NkSystem`), à brancher dans le scheduler. Chacun
parcourt les entités portant un `NkScriptHost` (et non marquées `NkInactive`) via la requête
`world.Query<NkScriptHost>().Without<NkInactive>().ForEach(...)`, et appelle un hook précis :

- **`NkScriptSystem`** — groupe `NkSystemGroup::Update`. C'est le pilier. Il gère d'abord la phase
  **OnStart** : si `host.pendingStart`, il appelle `OnStart` sur chaque script `IsEnabled()` qui
  n'a pas encore démarré (`!HasStarted()`), pose `mStarted`/`started` et retombe `pendingStart` à
  `false`. Puis il appelle **OnUpdate** sur tout script `IsEnabled() && HasStarted()`.
- **`NkScriptLateSystem`** — groupe `NkSystemGroup::PostUpdate`. Appelle **OnLateUpdate** (utile
  pour ce qui doit se faire *après* toutes les mises à jour : une caméra qui suit, du rattrapage).
- **`NkScriptFixedSystem`** — groupe `NkSystemGroup::FixedUpdate`, signature
  `Execute(NkWorld&, float32 fixedDt)`. Appelle **OnFixedUpdate** au pas de temps fixe (physique).

Les trois sont déclarés `Sequential`, écrivent `NkScriptHost` (`Writes<NkScriptHost>()`) et portent
un nom (`"NkScriptSystem"`, etc.), via le builder fluent `NkSystemDesc{}.Writes<T>().Sequential()
.InGroup(...).Named(...)`. Tous bouclent sur `host.count`, cohérent avec la déclaration du host.

Un point d'attention : ces systèmes n'appellent **que** `OnStart`, `OnUpdate`, `OnLateUpdate` et
`OnFixedUpdate`. **`OnAwake`, `OnEnable`, `OnDisable` et `OnDestroy` ne sont jamais déclenchés** par
le câblage fourni (piège #8). Si votre logique d'éveil ou de nettoyage dépend de ces hooks, sachez
qu'ils ne seront pas appelés tant que ce n'est pas branché.

> **En résumé.** Trois systèmes pilotent les scripts : `NkScriptSystem` (Start + Update),
> `NkScriptLateSystem` (LateUpdate), `NkScriptFixedSystem` (FixedUpdate). Branchez-les dans le
> scheduler. `OnAwake`/`OnEnable`/`OnDisable`/`OnDestroy` ne sont **pas** câblés.

---

## Les scripts externes : DLL, C#, Python

Au-delà du C++ natif, trois passerelles visent à écrire des comportements dans un langage qu'on
**recompile et recharge sans relancer le moteur**. C'est le bon réflexe d'itération rapide — mais
c'est aussi la partie la **moins finie** : le hot-reload DLL est non fonctionnel (horodatages stub),
les backends C# et Python ne s'activent que si leur macro est présente, et plusieurs fonctions
internes sont des stubs. Chacune adapte un script externe en `NkScriptComponent` (via un *adapter*)
pour qu'il s'exécute dans les mêmes systèmes que le natif.

- **DLL** (`NkScriptBridge.h`) — le script est compilé en bibliothèque dynamique (`.dll`/`.so`/
  `.dylib`) exposant une **ABI C stable**. On dérive `NkScriptDLLBase`, on termine par la macro
  `NK_EXPORT_DLL_SCRIPT(MaClasse)`, et côté moteur `NkScriptLoader::Global().LoadDLL(...)` charge la
  lib puis `AttachDLLScript(host, "MaClasse")` l'instancie. C'est le seul backend qui définit des
  hooks de collision/trigger.
- **C#** (`CSharp/NkScriptCSharp.h`) — passerelle Mono/CoreCLR, gardée par `NKECS_MONO_AVAILABLE`.
  Côté C#, on dérive une classe `NkScript` avec des méthodes `OnStart(ref NkContext)` /
  `OnUpdate(ref NkContext, float)` ; côté moteur, `NkCSharpBridge::Global().Init()` →
  `LoadAssembly(...)` → `AttachCSharpScript(host, "Ns.Classe")`. Sans Mono, **tout est no-op**.
- **Python** (`Python/NkScriptPython.h`) — passerelle CPython 3.10+, gardée par
  `NKECS_PYTHON_AVAILABLE`. Côté Python, on dérive `NkScript` avec des méthodes snake_case
  (`on_start(self, ctx)`, `on_update(self, ctx, dt)`…) ; côté moteur,
  `NkPythonBridge::Global().Init()` → `LoadScript("Scripts/X.py")` →
  `AttachPythonScript(host, "ClasseX")`. Sans CPython, **tout est no-op**.

> **En résumé.** Trois backends externes adaptent un script en `NkScriptComponent` : DLL (ABI C,
> hot-reload *visé* mais non fonctionnel), C# (Mono, *no-op* sans `NKECS_MONO_AVAILABLE`), Python
> (CPython, *no-op* sans `NKECS_PYTHON_AVAILABLE`). Charpente largement SPEC.

---

## Aperçu de l'API

La liste de **tous** les éléments publics, regroupés par fichier. Le statut (réel / stub / gardé par
macro) est rappelé. Chaque élément est détaillé dans la « Référence complète ».

### `NkScriptComponent.h` — natif C++

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Base | `class NkScriptComponent` (abstraite) | Classe de base style MonoBehaviour. |
| Cycle de vie | `OnAwake` `OnStart` `OnEnable` `OnDisable` | Hooks d'éveil/démarrage/activation (virtuels, vides par défaut). |
| Cycle de vie | `OnUpdate(…,dt)` `OnLateUpdate(…,dt)` `OnFixedUpdate(…,fixedDt)` | Mises à jour par frame / après update / pas fixe. |
| Cycle de vie | `OnDestroy` | Fin de vie. |
| État | `SetEnabled` `IsEnabled` `HasStarted` | Activer/couper, savoir si `OnStart` a eu lieu. |
| Identité | `GetTypeName()` **(pure virtual)** | Nom du type — **obligatoire à overrider**. |
| Persistance | `Serialize(char*,uint32)` `Deserialize(const char*)` | Sauver/restaurer l'état (vides par défaut). |
| Porteur | `struct NkScriptHost` (`NK_COMPONENT`) | Composant ECS qui porte jusqu'à 32 scripts. |
| Porteur | `Attach<T>(args…)` `Get<T>()` `Has<T>()` `SetAllEnabled` | Ajouter / retrouver / tester / (dés)activer en bloc. |
| Porteur | `scripts[32]` · `count` · `pendingStart` · `started` | Tableau fixe, compteur (**`count`**), drapeaux. |
| Registre | `class NkScriptRegistry` (singleton) | Instanciation **par nom** (éditeur/JSON). |
| Registre | `Global()` · `static Register<T>(name)` · `Instantiate(name)` · `Count()` | Accès singleton / enregistrer (**méthode `static`**) / créer par nom / compter. |
| Macro | `NK_REGISTER_SCRIPT(Type)` | Auto-enregistre un type au chargement. |

### `NkScriptSystem.h` — exécution

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Système | `class NkScriptSystem` (`Update`) | Phase OnStart **+** OnUpdate. |
| Système | `class NkScriptLateSystem` (`PostUpdate`) | OnLateUpdate. |
| Système | `class NkScriptFixedSystem` (`FixedUpdate`) | OnFixedUpdate (`Execute(world, fixedDt)`). |

### `NkScriptBridge.h` — DLL (ABI C)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Plateforme | `NKECS_DLL_EXT` · `NkDLLHandle` · `NKECS_LOAD/CLOSE/FIND` · `NKECS_EXPORT` | Extension, handle, charge/ferme/résout, export. |
| ABI C | `struct NkScriptContext` | Monde + entité + `dt`/`fixedDt` passés à la DLL. |
| ABI C | `struct NkScriptDLLInfo` | Nom/version/auteur/thread-safe. |
| ABI C | `struct NkScriptVTable` | ~16 pointeurs de fonction C (lifecycle + collision/trigger + champs JSON). |
| ABI C | `struct NkScriptDLLFactory` | Info + `Create`/`Destroy` + vtable. |
| ABI C | `NkScript_GetInfoFn` · `NkScript_GetFactoryFn` | Signatures des points d'entrée. |
| Adapter | `class NkDLLScriptAdapter` (`: NkScriptComponent`) | Adapte une factory DLL en script natif. |
| Adapter | `CaptureState` `RestoreState` | État pour hot-reload (= Serialize/Deserialize). |
| Loader | `struct NkLoadedDLL` | Une DLL chargée (path/name/handle/factory). |
| Loader | `class NkScriptLoader` (singleton) | Charge/décharge/hot-reload les DLL. |
| Loader | `LoadDLL` `UnloadDLL` `CreateScript` `DLLCount` `GetDLLName` | API principale du loader. |
| Loader | `LoadDirectory` · `HotReload` · `ReloadDLL` | **STUB / non fonctionnels** (horodatages = 0). |
| Côté DLL | `class NkScriptDLLBase` | Base optionnelle « sans monde explicite » + hooks collision/trigger. |
| Macro | `NK_EXPORT_DLL_SCRIPT(Class)` | Génère `extern "C" nkecs_get_factory()` (5 hooks seulement). |
| Libre | `AttachDLLScript(host, name)` | ⚠️ écrit `host.scriptCount` (mismatch — ne compile pas). |

### `CSharp/NkScriptCSharp.h` — Mono (gardé `NKECS_MONO_AVAILABLE`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Interop | `struct alignas(8) NkCSContext` | Struct blittable P/Invoke (monde, entité, transform, dirty). |
| Internals | `namespace CSharpInternals` (`extern "C"`) | Fonctions exposées à C# via `mono_add_internal_call`. |
| Bridge | `class NkCSharpBridge` (singleton) | Init/Shutdown Mono, charge assemblies, crée scripts. |
| Bridge | `Init` `Shutdown` `IsInitialized` `LoadAssembly` `CreateCSharpScript` | API principale. |
| Bridge | `ReloadAssembly` · `HotReload` | Pas de vrai hot-reload (re-load / **stub**). |
| Adapter | `class NkCSharpScriptAdapter` (`: NkScriptComponent`) | Appelle les méthodes Mono. |
| Libre | `AttachCSharpScript(host, name)` | ⚠️ écrit `host.scriptCount` (même mismatch). |

### `Python/NkScriptPython.h` — CPython (gardé `NKECS_PYTHON_AVAILABLE`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Interop | `struct NkPyContext` | Monde + entité + `dt` (API Python réelle = commentaires). |
| Bridge | `class NkPythonBridge` (singleton) | Init/Shutdown CPython, charge scripts, crée instances. |
| Bridge | `Init` `Shutdown` `IsInitialized` `LoadScript` `CreatePythonScript` | API principale. |
| Bridge | `ReloadScript` · `HotReload` | **TODO / stub**. |
| Adapter | `class NkPythonScriptAdapter` (`: NkScriptComponent`) | Appelle les méthodes snake_case Python. |
| Libre | `AttachPythonScript(host, name)` | ⚠️ écrit `host.scriptCount` (même mismatch). |

---

## Référence complète

Chaque élément est repris ici en détail, avec ses cas d'usage dans les différents domaines du moteur
(gameplay/IA, rendu, animation, physique, audio, UI/éditeur, IO, scène). Le **statut** (réel,
stub, gardé par macro) est rappelé partout où il importe.

### `NkScriptComponent` à fond

C'est la seule pièce **pleinement opérationnelle** de la famille. Le contrat est simple : dériver,
redéfinir les hooks utiles, et **obligatoirement** `GetTypeName()` (pure virtual — sans elle, la
classe reste abstraite et ne compile pas). Tous les autres hooks sont vides par défaut, donc on ne
paie que ce qu'on écrit. Comme chaque hook reçoit `NkWorld&` et `NkEntityId self`, un script est un
acteur de plein droit dans l'ECS : il interroge ses propres composants, en ajoute, émet des
événements, agit sur d'autres entités.

L'ordre des hooks dessine la **boucle de jeu** vue côté gameplay :

- **`OnStart`** — initialisation paresseuse, **une seule fois**, au premier tour où le script est
  actif. *Gameplay/IA* : capturer les références aux autres composants, choisir un état initial de
  FSM. *Rendu* : récupérer un handle de matériau. *Audio* : armer une source. *Scène* : lire les
  paramètres posés par l'éditeur.
- **`OnUpdate(dt)`** — le battement de cœur, chaque frame. *Gameplay* : déplacer, viser, décrémenter
  des minuteries (toujours en multipliant par `dt` pour rester indépendant du framerate). *IA* :
  faire avancer un arbre de comportement. *UI/éditeur* : animer un widget. *Animation* : faire
  progresser une interpolation.
- **`OnLateUpdate(dt)`** — *après* tous les `OnUpdate`. *Caméra* : un suiveur qui se cale sur la
  position **finale** de sa cible (sinon il a une frame de retard). *Rendu* : poser un *billboard*
  une fois le monde figé.
- **`OnFixedUpdate(fixedDt)`** — au pas fixe de la physique. *Physique* : appliquer une force, une
  impulsion, intégrer une vitesse de façon déterministe. C'est ici, pas dans `OnUpdate`, que va la
  logique sensible à la stabilité numérique.
- **`OnAwake` / `OnEnable` / `OnDisable` / `OnDestroy`** — éveil, (dés)activation, destruction.
  **Attention** : ces quatre hooks **ne sont pas appelés** par les systèmes fournis (voir piège #8).
  N'y mettez pas, pour l'instant, de logique critique d'initialisation ou de nettoyage.

`SetEnabled`/`IsEnabled` permettent de couper temporairement un script sans le détacher (une tourelle
endormie, un dialogue en pause), et `HasStarted()` distingue « jamais démarré » de « actif ».
`Serialize`/`Deserialize` (vides par défaut) servent à persister l'état d'un script dans une scène
`.nkscene` ou à le transférer lors d'un futur hot-reload — à remplir manuellement par chaque script.

Ce n'est **pas** un composant qu'on attache directement : on passe toujours par `NkScriptHost`. Et
ce n'est **pas** l'endroit des collisions — la base n'a aucun hook collision/trigger.

### `NkScriptHost` à fond

Le host est le **composant ECS réel** (via `NK_COMPONENT`), et un **conteneur** : un tableau fixe de
32 `std::shared_ptr<NkScriptComponent>` plus un compteur `count`, un drapeau `pendingStart` (un
script vient d'être attaché, il faut le démarrer) et `started`. Le format fixe (pas d'allocation
dynamique du tableau lui-même) le rend prévisible côté mémoire, au prix d'un plafond de 32 scripts
par entité — largement suffisant pour composer un comportement.

- **`Attach<T>(args…)`** — construit `T` (via `make_shared`), le range, renvoie le pointeur brut
  (ou `nullptr` si plein), arme `pendingStart`. *Scène* : c'est l'appel que fait le chargeur de
  niveau pour chaque script déclaré sur une entité. *Gameplay* : composer un acteur à partir de
  briques (`Attach<Health>(); Attach<Patrol>(); Attach<Loot>();`).
- **`Get<T>()`** — renvoie le premier script castable en `T` (`dynamic_cast`), sinon `nullptr`.
  *Gameplay* : un script qui en pilote un autre sur la même entité.
- **`Has<T>()`** — test de présence sans cast utilisable.
- **`SetAllEnabled(bool)`** — coupe/réactive tous les scripts d'un coup. *Éditeur* : geler une
  entité en mode édition ; *gameplay* : suspendre une entité hors-écran.

Le compteur réel est **`count`** — pas `scriptCount`. Retenez-le : c'est le nom sur lequel bouclent
correctement les trois systèmes d'exécution, alors que les trois fonctions libres `Attach*Script`
se trompent de nom (piège #1).

### `NkScriptRegistry` et `NK_REGISTER_SCRIPT` à fond

Le registre résout le problème du **chargement par données** : une scène ou un projet JSON ne
connaît que des **noms** de scripts, pas des types C++. `Register<T>("Nom")` associe un nom à une
fabrique (`FactoryFn = std::function<std::shared_ptr<NkScriptComponent>()>`), et
`Instantiate("Nom")` produit une instance fraîche (`nullptr` si le nom est inconnu). `Count()` donne
le nombre d'entrées. La capacité est fixe (`kMaxEntries = 512`) et le registre est **silencieux**
quand il déborde — à surveiller si un projet dépasse ce plafond.

La macro **`NK_REGISTER_SCRIPT(Type)`** automatise l'enregistrement : posée au scope fichier, elle
crée un objet statique anonyme (`_sNkScriptReg_##Type`) dont le constructeur appelle directement
`NkScriptRegistry::Register<Type>(#Type)` (méthode `static`, sans passer par `Global()`) au
chargement du module. C'est l'idiome standard :

- **Éditeur/UI** : peupler une liste déroulante « Ajouter un script » à partir des noms enregistrés.
- **Scène/IO** : sérialiser `"DoorOpener"` dans le `.nkscene`, le ré-instancier au chargement sans
  toucher au code de chargement.
- **Gameplay** : *spawn* piloté par données (une table d'ennemis qui nomme leurs scripts).

### `NkScriptSystem` / `NkScriptLateSystem` / `NkScriptFixedSystem` à fond

Les trois systèmes sont le **moteur d'exécution**. Ils sont `final`, dérivent `NkSystem`, et se
décrivent au scheduler via le builder `NkSystemDesc{}` fluent : `.Writes<NkScriptHost>()` (ils
modifient les hosts), `.Sequential()` (pas de parallélisme entre eux — un script peut toucher
n'importe quoi), `.InGroup(NkSystemGroup::...)` (la phase) et `.Named(...)`. Les groupes utilisés
sont `Update`, `PostUpdate` et `FixedUpdate`.

- **`NkScriptSystem`** (groupe `Update`) — le seul qui gère **deux** phases. D'abord le démarrage :
  si `host.pendingStart`, il appelle `OnStart` sur chaque script actif non démarré, pose
  `mStarted`/`started`, retombe `pendingStart`. Ensuite la mise à jour : `OnUpdate` sur tout script
  actif **et** démarré. Ce découplage garantit qu'un script attaché en cours de frame démarre avant
  de recevoir son premier `OnUpdate`.
- **`NkScriptLateSystem`** (groupe `PostUpdate`) — `OnLateUpdate` seulement. À placer après toute la
  logique de jeu : caméras, rattrapage, *billboards*, IK de suivi.
- **`NkScriptFixedSystem`** (groupe `FixedUpdate`, `Execute(world, fixedDt)`) — `OnFixedUpdate` au
  pas fixe. Toute la logique physique sensible au déterminisme.

Tous itèrent `world.Query<NkScriptHost>().Without<NkInactive>().ForEach(...)` et bouclent sur
`host.count`. **`OnAwake`/`OnEnable`/`OnDisable`/`OnDestroy` ne sont câblés nulle part ici** : si
votre design en dépend, c'est à ajouter (piège #8). Les enums et le builder (`NkSystemGroup`,
`NkSystemDesc`, `NkInactive`) sont déclarés ailleurs dans l'ECS — ce module ne fait que les
consommer.

### `NkScriptBridge.h` (DLL) à fond — **largement SPEC**

L'idée : compiler des scripts en bibliothèques dynamiques pour les recharger sans relancer le
moteur. Le module fournit les **couches plateforme** (`NKECS_DLL_EXT` = `.dll`/`.dylib`/`.so` ;
`NkDLLHandle` = `HMODULE`/`void*` ; `NKECS_LOAD_DLL`/`NKECS_CLOSE_DLL`/`NKECS_FIND_SYMBOL` mappés sur
`LoadLibraryA`/`GetProcAddress` ou `dlopen`/`dlsym`). À noter une fragilité : `NKECS_EXPORT` est
défini **après** son usage dans `NK_EXPORT_DLL_SCRIPT` — ça ne marche que parce que la macro est
expansée plus tard (ordre à durcir).

**L'ABI C** repose sur des structs *plain* : `NkScriptContext` (monde, entité empaquetée, `dt`,
`fixedDt`), `NkScriptDLLInfo` (nom/version/auteur/thread-safe), `NkScriptVTable` (≈16 pointeurs de
fonction : tout le cycle de vie **plus** `OnCollisionEnter/Exit`, `OnTriggerEnter/Exit`, et la
réflexion de champs `GetFieldsJSON`/`SetFieldFromJSON`), et `NkScriptDLLFactory` (info + `Create`/
`Destroy` + vtable). Le loader résout **un seul symbole : `"nkecs_get_factory"`** — attention,
l'en-tête du fichier documente encore `nkecs_create`/`nkecs_info`, **obsolètes** (piège #7).

- **`NkDLLScriptAdapter`** (dérive `NkScriptComponent`) — fait le pont : son constructeur appelle
  `Create()`, copie `info.name` dans `mTypeName` (rendu par `GetTypeName`), et chaque hook redéfini
  appelle le pointeur vtable correspondant (s'il est non-null) via un `NkScriptContext` bâti par
  `MakeCtx` (qui met `dt` **et** `fixedDt` à la même valeur). `CaptureState`/`RestoreState`
  (= Serialize/Deserialize) visent le transfert d'état au hot-reload.
- **`NkLoadedDLL`** — une DLL chargée (path/name/handle/factory/loadTime), avec `IsValid()` et
  `Unload()`.
- **`NkScriptLoader`** (singleton) — `LoadDLL(path)` (capacité `kMaxDLLs = 256`, résout
  `nkecs_get_factory`, échoue proprement), `UnloadDLL(name)`, `CreateScript(name)`,
  `DLLCount`/`GetDLLName`. **Mais** : `LoadDirectory` est un **stub** (`(void)dir;`), et surtout
  `GetTimestamp()` et `GetFileModTime()` renvoient `0; // stub` — si bien que `HotReload` (qui teste
  `mtime > loadTime`, soit `0 > 0`, toujours faux) **ne se déclenchera jamais**. `ReloadDLL` a
  pourtant une logique complète de capture/restauration d'état (via `dynamic_cast<NkDLLScriptAdapter*>`
  et `host.scripts[s]`), mais elle reste inatteignable tant que l'horodatage est stub. **Statut :
  SPEC / non fonctionnel** (piège #2).
- **`NkScriptDLLBase`** (côté DLL) — base optionnelle « sans monde explicite » : les hooks prennent
  juste `float32` (et `NkEntityId` pour collision/trigger), le contexte étant injecté par
  `_SetContext` et relu via `GetWorld()`/`GetEntityId()`/`GetDt()`. Elle offre des helpers
  composants templatés (`GetComponent`/`AddComponent`/`RemoveComponent`/`HasComponent`/`Emit`) — donc
  un script DLL peut faire la même chose qu'un natif. **C'est le seul backend avec des hooks
  collision/trigger réels.**
- **`NK_EXPORT_DLL_SCRIPT(Class)`** — génère `extern "C" NkScriptDLLFactory nkecs_get_factory()`.
  **Incomplète** : elle ne câble que `OnStart`, `OnUpdate`, `OnDestroy`, `Serialize`, `Deserialize`
  (5 sur ≈16). `OnFixedUpdate`/`OnLateUpdate`/`OnEnable`/`OnDisable`/collision/trigger/champs JSON
  restent `nullptr`, donc non exposés (piège #3). Elle utilise `new`/`delete` (pas NKMemory —
  piège #6).
- **`AttachDLLScript(host, name)`** — fonction libre passant par `NkScriptLoader::CreateScript`.
  ⚠️ Elle écrit `host.scriptCount`/`host.scriptCount++` → **ne compile pas** contre le `NkScriptHost`
  réel (`count`) — piège #1.

### `CSharp/NkScriptCSharp.h` (Mono) à fond — **gardé par macro**

Passerelle vers Mono/CoreCLR, **active uniquement si `NKECS_MONO_AVAILABLE`** ; sinon les types Mono
sont aliasés `void` et toutes les méthodes renvoient `false`/`nullptr` (no-op complet, piège #5).
Plusieurs `TODO` subsistent.

- **`NkCSContext`** (`alignas(8)`) — struct **blittable** pour le P/Invoke, miroir d'une `NkContext`
  côté C# : `worldPtr`, `entityPack`, `dt`/`fixedDt`, la transform (`pos/rot/scl`, `rotW`/`scl`=1),
  une sortie `outPos*`, et `dirtyFlags` (bit 0 = position modifiée). C'est le canal d'échange : le C#
  lit/écrit la transform, le C++ réapplique si le bit *dirty* est posé.
- **`CSharpInternals`** (`extern "C"`) — l'API « moteur » offerte au C# via `mono_add_internal_call`,
  toutes prenant `void* worldPtr` re-casté en `NkWorld*` : `NkCS_CreateEntity` (crée + `NkName`/
  `NkTransform`/`NkTag`), `NkCS_DestroyEntity` (`DestroyDeferred`), `NkCS_IsAlive`,
  `NkCS_GetTransform`, `NkCS_SetPosition` (pose `dirty`), `NkCS_Translate`, `NkCS_Rotate`,
  `NkCS_LookAt`, `NkCS_GetParent`. Deux sont **explicitement des stubs** : `NkCS_EmitPlayerDamaged`
  (exemple à généraliser) et `NkCS_Log` (`NkLog` commenté). *Gameplay/scène* : c'est cette surface
  qui permet à un script C# de manipuler le monde.
- **`NkCSharpBridge`** (singleton) — `Init(runtimeDllPath)` (`mono_jit_init` + `RegisterInternals` ;
  `false` sans Mono), `Shutdown`, `IsInitialized`, `LoadAssembly(path)` (capacité
  `kMaxAssemblies = 32`), `CreateCSharpScript("Ns.Classe")` (split sur le dernier `.`,
  `mono_class_from_name`, crée l'adapter). `ReloadAssembly` **n'est pas** un vrai hot-reload (Mono ne
  le permet pas — simple re-load), et `HotReload` est un **stub**.
- **`NkCSharpScriptAdapter`** (dérive `NkScriptComponent`) — pré-résout les méthodes Mono au
  constructeur et les invoque via `InvokeVoid` (qui remplit un `NkCSContext` depuis le `NkTransform`,
  appelle `mono_runtime_invoke`, journalise les exceptions, réapplique `outPos` si *dirty*).
  ⚠️ Il **override `OnCollisionEnter(NkWorld&, NkEntityId, NkEntityId)`** — méthode **absente** de
  `NkScriptComponent` (la base n'a aucun hook collision) → `override` invalide (piège #4). Son corps
  d'invocation collision est de toute façon vide, et `Serialize`/`Deserialize` sont des TODO vides.
- **`AttachCSharpScript(host, name)`** — même mismatch `host.scriptCount` que la DLL (piège #1).

### `Python/NkScriptPython.h` (CPython) à fond — **gardé par macro**

Passerelle vers CPython 3.10+, **active uniquement si `NKECS_PYTHON_AVAILABLE`** (inclut aussi
`NkScriptBridge.h`) ; sinon no-op (piège #5). Plusieurs simplifications et `TODO`.

- **`NkPyContext`** — `NkWorld* world`, `NkEntityId entity` (= `Invalid()`), `dt`. L'API « Python »
  riche (`get_transform`/`set_position`/`translate`/`get_component`…) n'est **documentée qu'en
  commentaires** — non implémentée côté C++.
- **`NkPythonBridge`** (singleton) — `Init(pythonHome)` (`Py_Initialize` + `RegisterModule`, ajoute
  `Scripts`/`.` au path ; `Py_SetPythonHome` **commenté**), `Shutdown` (`Py_Finalize`),
  `IsInitialized`, `LoadScript(path)` (capacité `kMaxScripts = 128`, exécute dans `__main__` — via
  **`std::fopen` direct**, pas NKFileSystem, piège #6), `CreatePythonScript(className)`
  (`PyObject_GetAttrString` sur `__main__`, instancie, crée l'adapter). `ReloadScript` est un
  **TODO** (pas de `importlib.reload` ; il rappelle juste `LoadScript`) et `HotReload` un **stub**.
  `RegisterModule` injecte un module `nkecs` par `PyRun_SimpleString` (« approche simple » — en prod
  il faudrait `PyModule_Create`) ; ce module fournit `NkVec2`, `NkVec3`, `NkColor4` (avec statics
  `white/red/green/blue/yellow/from_hex`) et la classe de base **`NkScript`** avec tous les hooks
  snake_case (`on_start`/`on_update`/`on_fixed_update`/`on_late_update`/`on_destroy`/`on_enable`/
  `on_disable`/`on_collision_enter`/`on_collision_exit`/`on_trigger_enter`/`on_trigger_exit`/
  `serialize`/`deserialize`).
- **`NkPythonScriptAdapter`** (dérive `NkScriptComponent`) — mappe les hooks C++ sur les noms
  snake_case via `CallMethod` (qui construit un **`dict` simplifié** `{entity_id, dt}` — pas un vrai
  `NkPyContext`, c'est noté en commentaire). `Serialize` passe par `serialize()`+`repr` Python,
  `Deserialize` par `ast.literal_eval`. Même remarque que C# : il **override `OnCollisionEnter(...)`
  absent de la base** (piège #4).
- **`AttachPythonScript(host, name)`** — même mismatch `host.scriptCount` (piège #1).

### Idiomes d'usage par backend

- **Natif** — dériver `NkScriptComponent`, overrider `GetTypeName` (**obligatoire**) + les hooks,
  poser `NK_REGISTER_SCRIPT(MonType)` au scope fichier, attacher via `host.Attach<MonType>()` (ou
  `NkScriptRegistry::Global().Instantiate("MonType")` pour le chemin par données), puis brancher les
  trois systèmes dans le scheduler. **C'est le chemin recommandé et opérationnel.**
- **DLL** — script héritant de `NkScriptDLLBase`, terminer par `NK_EXPORT_DLL_SCRIPT(MaClasse)`,
  compiler en `.dll`, côté moteur `NkScriptLoader::Global().LoadDLL(...)` puis attacher (sous réserve
  du mismatch `count`/`scriptCount`). Hot-reload **non fonctionnel** en l'état.
- **C#** — `NkCSharpBridge::Global().Init(...)` → `LoadAssembly(...)` → attacher ; côté C# dériver
  `NkScript`, `OnStart(ref NkContext)`/`OnUpdate(ref NkContext, float)`. **Inactif sans Mono.**
- **Python** — `NkPythonBridge::Global().Init()` → `LoadScript("Scripts/X.py")` → attacher ; côté
  Python dériver `NkScript`, `on_start(self, ctx)` etc. **Inactif sans CPython.**

### Pièges et statut à connaître

- **#1 — `count` vs `scriptCount`.** `NkScriptHost` déclare `count`, mais `AttachDLLScript`,
  `AttachCSharpScript` et `AttachPythonScript` écrivent `host.scriptCount` (et `host.scriptCount++`).
  En l'état, ces trois fonctions libres **ne compilent pas**. Les trois systèmes d'exécution, eux,
  bouclent correctement sur `count`. À corriger avant tout usage des backends externes.
- **#2 — hot-reload DLL non fonctionnel.** `GetTimestamp()` et `GetFileModTime()` renvoient `0`
  (stubs), donc `mtime > loadTime` est toujours faux → `HotReload` ne se déclenche jamais.
  `LoadDirectory` est aussi un stub. La logique de `ReloadDLL` existe mais reste inatteignable.
- **#3 — `NK_EXPORT_DLL_SCRIPT` incomplète.** Elle ne câble que 5 hooks (Start/Update/Destroy/
  Serialize/Deserialize) sur les ≈16 de `NkScriptVTable`. FixedUpdate/LateUpdate/Enable/Disable/
  collision/trigger/champs JSON restent `nullptr`.
- **#4 — `OnCollisionEnter` override fantôme.** Les adapters C# et Python *override*
  `OnCollisionEnter(NkWorld&, NkEntityId, NkEntityId)`, **absent de `NkScriptComponent`** (la base
  n'a aucun hook collision/trigger). C'est une erreur de compilation potentielle. Les hooks collision
  n'existent réellement que dans `NkScriptDLLBase` et `NkScriptVTable`.
- **#5 — C#/Python = no-op sans macro.** `NKECS_MONO_AVAILABLE` / `NKECS_PYTHON_AVAILABLE` sont
  requises ; sinon tout renvoie `false`/`nullptr`.
- **#6 — allocations hors NKMemory.** `NK_EXPORT_DLL_SCRIPT` utilise `new`/`delete`, le Python
  `LoadScript` utilise `std::fopen`, les adapters utilisent `std::make_shared`/`std::vector` — à
  contre-courant de la règle dure NKMemory du projet. À harmoniser si ces fichiers passent en
  production.
- **#7 — en-tête `NkScriptBridge.h` obsolète.** Il documente l'ABI `nkecs_create`/`nkecs_destroy`/
  `nkecs_script_info`, alors que le code résout réellement le seul symbole `nkecs_get_factory`.
- **#8 — hooks non câblés.** `OnAwake`, `OnEnable`, `OnDisable` et `OnDestroy` ne sont appelés par
  **aucun** des systèmes fournis ; seuls Start/Update/Late/Fixed le sont.

---

### Exemple

```cpp
#include "Noge/ECS/Scripting/NkScriptComponent.h"
#include "Noge/ECS/Scripting/NkScriptSystem.h"
using namespace nkentseu::ecs;

// 1) Un comportement natif : une tourelle qui pivote vers le joueur.
class Turret final : public NkScriptComponent {
public:
    const char* GetTypeName() const noexcept override { return "Turret"; }

    void OnStart(NkWorld& world, NkEntityId self) noexcept override {
        mCooldown = 0.f;                         // une seule fois
    }
    void OnUpdate(NkWorld& world, NkEntityId self, float32 dt) noexcept override {
        if (!IsEnabled()) return;
        mCooldown -= dt;                         // dt → indépendant du framerate
        if (mCooldown <= 0.f) { /* tirer */ mCooldown = 1.5f; }
    }
private:
    float32 mCooldown = 0.f;
};
NK_REGISTER_SCRIPT(Turret);                       // instanciable par nom (éditeur/JSON)

// 2) Attacher sur une entité, puis brancher les systèmes dans le scheduler.
NkScriptHost& host = world.Add<NkScriptHost>(entity);
host.Attach<Turret>();                            // arme OnStart au prochain tour

scheduler.Add<NkScriptSystem>();      // OnStart + OnUpdate   (groupe Update)
scheduler.Add<NkScriptLateSystem>();  // OnLateUpdate         (groupe PostUpdate)
scheduler.Add<NkScriptFixedSystem>(); // OnFixedUpdate        (groupe FixedUpdate)

// 3) Chemin par données : instancier depuis un nom lu dans une scène.
if (auto inst = NkScriptRegistry::Global().Instantiate("Turret")) {
    // ... rangé dans un NkScriptHost par le chargeur de scène.
}
```

---

[← Index Noge](README.md) · [Récap Noge](../Noge.md) · [Couche Engine](../README.md)
