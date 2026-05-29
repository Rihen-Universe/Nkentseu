# NKReflection — Roadmap

État actuel (mai 2026) : Module fondationnel **utilisable mais minimaliste**.
L'infrastructure runtime existe (`NkType`, `NkProperty`, `NkMethod`, `NkClass`,
`NkRegistry` singleton) avec gestion type-safe via `NkFunction`
(constructor/destructor/getter/setter/invoker encapsulés). Les macros
`NKENTSEU_REFLECT_CLASS`, `NKENTSEU_PROPERTY`, `NKENTSEU_REGISTER_CLASS` sont
exposées via `NkRegistry.h`. **Stockage des propriétés/méthodes dans un tableau
statique de capacité fixe 64**. Tests smoke uniquement (categorie + lookup).
Pas encore intégré au flux `InspectorPanel` de Unkeny ni câblé pour la
sérialisation.

---

## Synthèse

| Phase / Composant | Statut | Effort | Priorité |
|-------------------|--------|--------|----------|
| NkType (NkTypeCategory, GetName, GetSize, typeid) | Livré | — | — |
| NkProperty (offset direct + getter/setter via NkFunction, flags) | Livré | — | — |
| NkMethod (invoker via NkFunction<void*(void*, void**)>, flags) | Livré | — | — |
| NkClass (héritage, ctor/dtor via NkFunction, props + methods) | Livré | — | — |
| NkRegistry (singleton Meyer, FindClass, FindType, GetClass/Type<T>) | Livré | — | — |
| Macros `NKENTSEU_REFLECT_CLASS`, `NKENTSEU_PROPERTY`, `NKENTSEU_REGISTER_CLASS` | Livré | — | — |
| Fonctions utilitaires (Initialize, Shutdown, DumpRegistry, ValidateRegistry) | Livré | — | — |
| Capacité fixe 64 props/méthodes (pas d'allocation dynamique) | Livré | — | Refacto Moyenne |
| Tests (TypeCategory, Registry GetType<T>) | Partiel | M | Haute |
| Enums réflechies (NkEnumDescriptor avec name<→value) | TODO | M | Haute |
| Containers réflechis (NkVector<T>, NkHashMap<K,V> introspectables) | TODO | L | Haute |
| Attributes/metadata custom (`[Range(0,100)]`, `[Tooltip("...")]`) | TODO | M | Haute (Inspector) |
| Thread-safety en écriture (RegisterType/RegisterClass) | TODO | S | Moyenne |
| Sérialisation via réflexion (NkSerializeReflect<T>) | TODO | L | Haute |
| InspectorPanel binding (édition live des props via NkProperty) | TODO | L | Haute |
| Polymorphisme/casting réflexif (Cast<T>, IsKindOf) | Partiel | M | Moyenne |
| Code generation (clang AST parser → enregistrement auto sans macros) | TODO | XL | Basse |
| Reflection dans plugins .dll/.so chargés à chaud | TODO | M | Moyenne |

Légende : Livré · Partiel · En cours · TODO · Abandonné

---

## Livré

### Cœur
- [NkType](src/NKReflection/NkType.h) :
  - `NkTypeCategory` énuméré (NK_VOID, NK_BOOL, NK_INT8..NK_INT64,
    NK_UINT8..NK_UINT64, NK_FLOAT32/64, NK_STRING, NK_CLASS, NK_ENUM, ...).
  - `GetName()`, `GetSize()`, `GetCategory()`, comparaison via typeid.
  - Template `NkTypeOf<T>()` pour accès direct à la metadata d'un type C++.
  - Pas de dépendance STL hors `<typeinfo>`.

- [NkProperty](src/NKReflection/NkProperty.h) :
  - Représente une propriété d'instance avec nom, type, offset ou
    getter/setter via `NkFunction<void*(void*)>`.
  - Flags : `NK_READ_ONLY`, `NK_WRITE_ONLY`, `NK_STATIC`, `NK_PCONST`,
    `NK_TRANSIENT` (skip serialization).
  - `MakeFromMember()` helper template pour binding automatique via `NkBind`.

- [NkMethod](src/NKReflection/NkMethod.h) :
  - Signature : nom, type de retour, liste paramètres, flags
    (`NK_STATIC`, `NK_CONST`, `NK_VIRTUAL`, `NK_NOEXCEPT`).
  - Invocation indirecte via `NkFunction<void*(void*, void**)>` (this +
    args packed).
  - `MakeFromCallable()` helper via `NkBind`/`NkPartial`.

- [NkClass](src/NKReflection/NkClass.h) :
  - Méta : nom, taille, classe de base, vecteurs statiques de props +
    methods (capacité 64 chacun).
  - Ctor/dtor via `NkFunction<void*(void)>` / `NkFunction<void(void*)>`.
  - `HasConstructor`, `CreateInstance`, `HasDestructor`, `DestroyInstance`.
  - `GetPropertyAt(i)`, `GetProperty(name)`, idem pour methods.
  - `MakeFromClassType<T>()` factory template.

- [NkRegistry](src/NKReflection/NkRegistry.h) :
  - Singleton Meyer thread-safe en init.
  - `RegisterType`, `RegisterClass`, `FindType(name)`, `FindClass(name)`,
    `GetType<T>()` (création lazy), `GetClass<T>()`.
  - Callback `OnRegisterCallback(name, isClass)`.

### Macros publiques
- `NKENTSEU_REFLECT_CLASS(ClassName)` — déclare la méta dans la classe.
- `NKENTSEU_PROPERTY(Type, name)` — déclare et expose un champ.
- `NKENTSEU_REGISTER_CLASS(ClassName)` — enregistrement statique au startup
  (dans `.cpp`).

### Fonctions utilitaires de haut niveau
[NkReflection.h](src/NKReflection/NkReflection.h) :
- `Initialize()` / `Shutdown()` optionnels.
- `CreateInstance<T>()` / `CreateInstanceByName(name)`.
- `DestroyInstance<T>(ptr)` / `DestroyInstanceByName(name, ptr)`.
- `GetRegisteredClassCount()`, `GetRegisteredTypeCount()`.
- `DumpRegistry()`, `ValidateRegistry()`.
- Alias namespace `nkentseu::refl = nkentseu::reflection`.

### Tests
- [test_smoke.cpp](tests/test_smoke.cpp) :
  - `NkTypeOf<nk_int32>().GetCategory() == NK_INT32`, idem float32.
  - `GetSize()` matches `sizeof`.
  - `NkRegistry::Get().GetType<nk_uint64>()` non null.

---

## En cours / TODO immédiat

### Tests à étendre — Haute priorité
Le test_smoke ne couvre que `NkType`. Il manque :
- Enregistrement d'une classe complète avec props + méthodes via macros, puis
  lookup et invocation.
- `CreateInstance<T>` puis `DestroyInstance<T>` cycle.
- Lecture/écriture d'une propriété via `NkProperty::GetValue<T>(instance)` et
  `SetValue<T>(instance, value)`.
- Invocation d'une méthode avec arguments packed.
- Validation de l'héritage (base class lookup).
- Thread-safety lecture/écriture (test concurrentiel).

### Polymorphisme et casting
Aujourd'hui `NkClass::GetBaseClass()` existe mais pas d'utilitaire
`Cast<T>(NkClass*, void*)` pour caster un `void*` vers un type dérivé en
validant la chaîne d'héritage. Indispensable pour InspectorPanel quand
l'utilisateur sélectionne une entité.

### Capacité fixe 64
Limite codée en dur dans `NkClass`. Cela bloquera dès qu'une classe a > 64
propriétés (cas réaliste pour un component complexe). Options :
1. Augmenter à 128/256 statiques.
2. Passer à `NkVector<NkProperty>` (allocation dynamique, mais cohérent avec
   le reste de NKContainers).
3. Hybride : SBO 16 + heap fallback (style `NkSmallVector`).

---

## À venir / À ajouter (futur proche)

### Enums réflechies — pivot InspectorPanel
Aujourd'hui pas de support natif des enums :
- `NkEnumDescriptor` listant les paires `(name, value)`.
- Macro `NKENTSEU_REFLECT_ENUM(EnumType, V1, V2, V3)`.
- API `NkEnumToString(value)` / `NkEnumFromString(name)`.
- Usage InspectorPanel : combo-box des valeurs autorisées au lieu d'un champ
  numérique brut.

### Attributes / metadata custom
Pour piloter l'InspectorPanel :
- `[Range(0.f, 1.f)]` → slider.
- `[Tooltip("...")]` → infobulle.
- `[Hidden]` → ne pas afficher.
- `[Category("Transform")]` → group dans l'inspecteur.
- `[Serialize(false)]` → équivalent NK_TRANSIENT.
- API runtime : `NkProperty::GetAttribute<T>()`, `HasAttribute<T>()`.

### Containers réflechis
Le système actuel ne sait pas itérer dynamiquement sur un `NkVector<T>` ou
`NkHashMap<K, V>` membre. Cas critique pour sérialisation et UI :
- `NkContainerTrait<T>` ou polymorphisme implicite via `NkType`
  (`IsContainer()`, `GetElementType()`, `GetCount(instance)`,
  `GetAt(instance, i)`, `PushBack(instance, value)`).

### Sérialisation via réflexion
Intégration avec NKSerialization (priorité haute pour PV3DE et Unkeny) :
- `NkSerializeReflect<T>(writer, instance)` itère sur les `NkProperty` et
  appelle le sérialiseur générique selon `NkTypeCategory`.
- `NkDeserializeReflect<T>(reader, instance)` symétrique.
- Skip `NK_TRANSIENT`.
- Format JSON `.nkscene` / `.nkproj` automatiquement piloté par les classes
  réfléchies.

### InspectorPanel binding — pilier Unkeny
Cf. `ARCHITECTURE.md §4.2 InspectorPanel` : la réflexion est la clé.
- Itération `NkClass::GetPropertyAt(i)` sur le component sélectionné.
- Widget UI dérivé du `NkTypeCategory` + attributes (cf. ci-dessus).
- Édition live : `NkProperty::SetValue<T>(...)` met à jour l'entité ECS.
- Undo/redo via `CommandHistory` (cf. ARCHITECTURE.md §4.3) → chaque mutation
  réflexive devient une command.

### Thread-safety en écriture
Aujourd'hui : "synchronisation externe requise" pour
`RegisterType`/`RegisterClass`. À durcir avec un mutex interne, ou par
construction garantir que tous les enregistrements ont lieu avant le
`OnInit()` de l'application.

### Plugin reflection
Quand `PluginSystem` chargera des `.dll`/`.so` (cf. ARCHITECTURE.md §4.3), le
NkRegistry devra :
- Recevoir les classes du plugin lors du `dlopen`.
- Les retirer proprement au `dlclose` (sans dangling pointers vers du code
  unloaded).
- API `NkRegistry::UnregisterAllFromModule(handle)`.

### Code generation (long terme)
Aujourd'hui les macros sont intrusives. Alternative :
- Parser clang AST → générer automatiquement `Register_XXX.cpp` au build.
- Évite l'oubli des macros et permet d'annoter via commentaires/attributs C++
  standard (`[[nk::reflect]]`).

---

## Bugs / quirks connus
- Capacité 64 props/méthodes par classe : limite implicite, pas de message
  d'erreur si dépassement (silencieusement ignoré ou crash).
- `Initialize()` est documenté optionnel mais `Shutdown()` doit être appelé
  avant arrêt de tous threads — non vérifié en runtime.
- L'alias `nkentseu::refl = reflection` est défini, mais le code utilise
  largement `nkentseu::reflection::` directement.

---

## Dépendances
- **Couches en dessous (utilisées)** : NKCore (Types, Traits, Builtin,
  Assert), NKContainers (NkFunction, NkBind, NkPartial).
- **Modules au-dessus qui en dépendent** :
  - **NKSerialization** (priorité 1) : besoin de la réflexion pour itérer sur
    les fields et sérialiser/désérialiser automatiquement
    (cf. `Native/NkReflect.h` qui est probablement le pont).
  - **Unkeny / InspectorPanel** (cf. ARCHITECTURE.md §4.2) : édition live des
    composants via les `NkProperty`.
  - **NKScene** (ECS) : enregistrement des components → introspection
    automatique sans plumbing manuel.
  - **NKScript** (futur) : binding C++ ↔ Lua via les métadonnées.
  - **PluginSystem** Unkeny : chargement de classes additionnelles à chaud.
