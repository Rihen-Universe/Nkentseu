# NKReflection — Roadmap

État actuel (2026-07-19, audit de maturité) : **MATURE — chantier principal terminé (P1→P5)**.
Audit 2026-07-19 : le module (9 420 LOC, 20 fichiers) et son pont NKECS compilent ;
les consommateurs réels sont branchés (NKEditorKit `NkEditorInspector` +
NKGuiDemo via `EnumerateEditableProperties`, NkReflectSerializer côté
NKSerialization, NkEntitySerialization côté NKECS). Les suites de tests
P2/P3/P4/P5 vivent chez les consommateurs (`NKSerialization/tests/test_reflect_*`,
`NKECS/tests/test_entity_serialization`) — exécution actuellement désactivée par
la politique workspace (`disableunittestexecution`), dernière exécution verte
2026-07-12. Il ne reste que des TODO de confort (voir tableau).
NKReflection est la **source de vérité runtime UNIQUE** de la réflexion
(décision d'architecture 2026-06 : NKECS/Reflect et NKSerialization/Native
sont des adaptateurs au-dessus, pas des systèmes concurrents). Le module
fournit tout ce qu'attendent ses 5 consommateurs (sérialisation auto,
inspecteur d'éditeur, Editor Kit, Blueprint, NKSerialization) : lire/écrire
toute propriété **sans connaître son type à la compilation** (NkReflectVariant),
sérialisation automatique via NkArchive, enums/conteneurs/métadonnées
d'édition, réflexion auto des composants ECS, invocation de méthodes avec
arguments. ~220 assertions de test réparties dans les suites P2/P3/P4/P5.

---

## Synthèse

| Phase / Composant | Statut | Effort | Priorité |
|-------------------|--------|--------|----------|
| Cœur : NkType / NkProperty / NkMethod / NkClass / NkRegistry | Livré | — | — |
| Macros `NKENTSEU_REFLECT_CLASS` / `NKENTSEU_PROPERTY` (+`_FLAGS`, auto-register) | Livré | — | — |
| P1 — `NkReflectVariant` (valeur type-erased, SBO 32o) + `Get/SetValueGeneric` | Livré | — | — |
| P1 — Capacité dynamique props/méthodes (NkVector, fin du fixe-64 silencieux) | Livré | — | — |
| P2 — `NkReflectSerializer` (NKSerialization) : (dé)sérialisation auto via NkArchive | Livré | — | — |
| P3 — Métadonnées d'édition (NkReflectMeta 64-bit, range/tooltip/category) | Livré | — | — |
| P3 — Enums réfléchies (`NkEnumDescriptor`, `NKENTSEU_REFLECT_ENUM`, nom symbolique sérialisé) | Livré | — | — |
| P3 — Conteneurs (`NkContainerTrait` NkVector<T> + SetArray/GetArray) | Livré | — | — |
| P4 — Pont NKECS (`NkReflectBridge`, `NkRegisterComponentReflection<T>`) | Livré | — | — |
| P4/P5 — Sérialisation ENTITÉ/MONDE (`NkEntitySerialization` : masque → composants) | Livré | — | — |
| P5 — API inspecteur (`NkInspector` : EnumerateEditableProperties, Set/GetPropertyByName) | Livré | — | — |
| P5 — `NkMethod` invocation 0-4 args type-erased (`InvokeVariant`) | Livré | — | — |
| P5 — Réflexion math opt-in (`NkMathReflect` : ~29 types Vec/Quat/Mat/Color/Rect) | Livré | — | — |
| Conteneurs d'OBJETS réfléchis (SetObjectArray/GetObjectArray) | Livré | — | — |
| Thread-safety écriture registre (NkMutex interne, lectures lock-free) | Livré | — | — |
| Tests (P2 20 + P3 45 + P4 42 + P5 117 assertions + smoke + sandbox) | Livré | — | — |
| NkProperty mode getter/setter indirect SANS instance capturée | TODO | M | Moyenne |
| Identité de type portable (typeid().name() est mangled/compilo-dépendant) | TODO | M | Basse |
| Reflection plugins .dll/.so (UnregisterAllFromModule au dlclose) | TODO | M | Basse |
| Code generation (clang AST → enregistrement auto sans macros) | TODO | XL | Basse |

Légende : Livré · Partiel · En cours · TODO · Abandonné

---

## Livré

### Cœur (inchangé depuis mai 2026, consolidé par P1)
- [NkType](src/NKReflection/NkType.h) : `NkTypeCategory` (NK_VOID…NK_STRING,
  NK_CLASS, NK_ENUM), `GetName/GetSize/GetCategory`, `NkTypeOf<T>()`.
- [NkProperty](src/NKReflection/NkProperty.h) : offset direct ou getter/setter
  `NkFunction`, flags (`NK_READ_ONLY`, `NK_TRANSIENT`…), `MakeFromMember()`.
- [NkMethod](src/NKReflection/NkMethod.h) : invocation indirecte type-erased.
- [NkClass](src/NKReflection/NkClass.h) : héritage, ctor/dtor, props+méthodes
  **en NkVector** (capacité dynamique, dédup par nom — fini le fixe-64).
- [NkRegistry](src/NKReflection/NkRegistry.h) : singleton, Find/GetType/Class,
  **écritures protégées par NkMutex interne**, lectures lock-free.

### P1 — valeur type-erased
- `NkReflectVariant` : SBO 32 octets + heap NkAlloc + chemin NkString ;
  coercions numériques (`ToInt64/ToFloat64/ToBool`), enums.
- `NkProperty::GetValueGeneric/SetValueGeneric` : lecture/écriture SANS
  template, dispatch par NkTypeCategory — le goulot des 5 consommateurs.
- Macro `NKENTSEU_PROPERTY` auto-register (registrar inline static, sûr
  multi-TU). Fix bug préexistant NKENTSEU_REGISTER_CLASS (NK_CONCAT).

### P2 — sérialisation automatique
- `NkReflectSerializer` (dans NKSerialization) : `SerializeReflected/
  DeserializeReflected(cls, instance, NkArchive&)` + `SerializeObject<T>`.
  Itère l'héritage, primitifs/NkString/objets imbriqués récursifs, exclut
  NK_TRANSIENT, ne désérialise pas NK_READ_ONLY/NK_STATIC. → save/load
  automatique `.nkscene`/`.nkproj` sans Serialize() manuel. Tests 20/20.

### P3 — enums, conteneurs, méta d'édition
- `NkReflectMeta.h` : drapeaux 64-bit portés de NKECS/Reflect (Range/Tooltip/
  HideInEditor/Serialize/BlueprintReadWrite/ColorPicker…) + NkEditMeta.
- `NkEnumDescriptor.h` + `NKENTSEU_REFLECT_ENUM` (sérialise le nom symbolique).
- `NkContainerTrait.h` : NkVector<T> réfléchi + SetArray/GetArray ; puis
  **conteneurs d'objets réfléchis** (SetObjectArray, test dédié).
- Auto-link `NkTypeOf<T>().SetClass` dans NKENTSEU_REFLECT_CLASS. Tests 45/45.

### P4 — pont NKECS
- `NkReflectBridge.h` (NKECS) : `NkRegisterComponentReflection<T>()` génère un
  NkClass depuis les NkFieldInfo du composant (metaFlags 1:1) ; hooks
  serialize/deserialize de ComponentMeta branchés sur P2.
- `NkJsonSerialization.h` réparé (includes Noge/nlohmann orphelins retirés).
- Puis **`NkEntitySerialization.h`** : SerializeEntity/DeserializeEntity
  (parcours du NkComponentMask de l'archétype, ajout type-erased par
  ComponentId) + SerializeWorld/DeserializeWorld — base des sauvegardes de
  scène. Deps NKECS→NKReflection/NKSerialization reportées dans
  `config/modules.jenga`. Tests 42/42.

### P5 — inspecteur, méthodes, math
- `NkInspector.h/.cpp` : NkEditableProperty + EnumerateEditableProperties +
  Set/GetPropertyByName (live-edit type-erased, respecte readOnly/hidden).
- `NkMethod` : `MakeFromMember` 0-4 args + `InvokeVariant` type-erased
  (base de l'appel Blueprint) + GetParameterCount/Type/IsConst.
- `NkMathReflect.h` (opt-in) : Vec2/3/4 (f/d/i/u), Quat, Mat2/3/4, Color,
  Rect2, NkAngle, NkEulerAngle → sérialisés comme sous-objets (round-trip
  JSON profond). Tests 117 assertions.

### Qualité de vie
- 2026-07-12 : `NKENTSEU_PROPERTY`/`_FLAGS` se terminent en `public:` (elles
  commençaient par `public:` mais retombaient en `private:` — les membres
  déclarés après la macro devenaient privés silencieusement).

---

## En cours / TODO

*(Le chantier principal est bouclé — le travail continue chez les
consommateurs : Editor Kit / inspecteur, Blueprint. Reste ici, par ordre
d'utilité :)*

- **NkProperty getter/setter indirect** : le mode indirect capture une
  instance concrète (cassé pour la réflexion de type) — à re-concevoir
  (paire de NkFunction prenant `void* instance`).
- **Identité de type portable** : `typeid().name()` est mangled et dépend du
  compilateur — gêne la sérialisation cross-toolchain ; passer à un nom
  canonique (macro ou hash stable).
- **Plugins** : retirer proprement les classes d'un module déchargé
  (`NkRegistry::UnregisterAllFromModule`).
- **Codegen clang** (long terme) : enregistrement auto sans macros.

---

## Bugs / quirks connus
- `typeid().name()` mangled (voir TODO identité portable).
- `Initialize()` optionnel mais `Shutdown()` doit précéder l'arrêt des
  threads — non vérifié au runtime.
- ~~Capacité fixe 64 props/méthodes (débordement silencieux)~~ corrigé P1.
- ~~Macro NKENTSEU_PROPERTY finit en `private:`~~ corrigé 2026-07-12.

---

## Dépendances
- **Couches en dessous** : NKCore (Types, Traits, Assert), NKContainers
  (NkFunction, NkVector, NkString), NKThreading (NkMutex du registre).
- **Modules au-dessus qui en dépendent** :
  - **NKSerialization** : `NkReflectSerializer` (P2) — livré.
  - **NKECS** : `NkReflectBridge` + `NkEntitySerialization` (P4) — livré.
  - **Editor Kit / Nogee InspectorPanel** : `NkInspector` (P5) — API prête,
    câblage UI côté éditeur.
  - **Blueprint/NKCode** : `NkMethod::InvokeVariant` — API prête.
