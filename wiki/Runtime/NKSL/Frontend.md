# Le frontend du compilateur NkSL

> Couche **Runtime** · NKSL · Transformer du **texte** de shader en **arbre vérifié** : le lexer
> `NkSLLexer` (texte → tokens), le parser `NkSLParser` (tokens → AST), l'analyse sémantique
> `NkSLSemantic` (AST → AST typé et validé), et la table des symboles `NkSLSymbolTable` qui porte
> les portées.

Un shader, avant d'être du SPIR-V, du HLSL ou du MSL, est d'abord **du texte** : `vec3 color = texture(albedo, uv).rgb;`. Pour le moteur, ce texte ne veut rien dire tant qu'on ne l'a pas **découpé**, **structuré**, puis **vérifié**. C'est exactement le travail du *frontend* : trois passes en cascade qui prennent une chaîne de caractères et en font un arbre de syntaxe abstraite (AST) dont on garantit qu'il a un sens — types cohérents, fonctions appelées avec les bons arguments, `break` uniquement dans une boucle, bindings sans collision. Une fois ce frontend franchi, le *backend* (générateurs GLSL/HLSL/MSL, vu ailleurs) n'a plus qu'à traduire un arbre déjà sain. Cette page vous apprend à enchaîner ces passes et à lire ce qu'elles produisent.

C'est le **vrai compilateur NkSL** — le module NKSL, avec sa grammaire complète (annotations `@binding`, `@stage`, `@entry`, types samplers/images, compute). Ce n'est **pas** le transpileur ad-hoc NkSL embarqué dans NKRenderer (un substituteur de texte ligne à ligne) : ici on a un vrai lexer, un vrai parser récursif descendant et une vraie analyse de types à scopes imbriqués.

Tout est **zéro-STL** : les chaînes sont des `NkString`, les listes des `NkVector`, les tables des `NkUnorderedMap`, et toute la mémoire passe par NKMemory. Les objets du frontend (`NkSLLexer`, `NkSLParser`, `NkSLSemantic`, `NkSLSymbolTable`) sont à durée de vie **pile ou membre** standard : on les construit, on les utilise, ils se détruisent en sortie de portée.

- **Namespace** : `nkentseu`
- **Headers** : `#include "NKSL/Frontend/NkSLLexer.h"` · `NkSLParser.h` · `NkSLSemantic.h` · `NkSLSymbolTable.h`

---

## L'analyse lexicale : `NkSLLexer`

La toute première difficulté est bête mais réelle : un fichier shader n'est qu'un long ruban de caractères, espaces et commentaires compris. Avant de raisonner sur la grammaire, il faut le **segmenter** en unités atomiques de sens — les *tokens* : ce `vec3` est un mot-clé de type, ce `color` un identifiant, ce `=` un opérateur d'affectation, ce `0.5` un littéral flottant. C'est le rôle du **lexer** : il avale les caractères, saute les blancs et les commentaires, et recrache des `NkSLToken` un par un.

Chaque `NkSLToken` porte son **genre** (`NkSLTokenKind`, une grande énumération qui couvre tout le langage : littéraux, mots-clés de types, qualificateurs, contrôle de flux, annotations `@`, opérateurs, délimiteurs), son **texte** brut, sa **ligne** et sa **colonne** (pour pointer une erreur exactement), et — pour les littéraux — sa **valeur** rangée dans une union (`intVal`, `uintVal`, `floatVal`, `boolVal`).

```cpp
NkSLLexer lexer("vec3 c = albedo.rgb * 0.5;", "albedo.nksl");
NkSLToken t;
do {
    t = lexer.Next();          // consomme et renvoie le token suivant
    // t.kind == NK_KW_VEC3, puis NK_IDENT ("c"), NK_OP_ASSIGN, ...
} while (t.kind != NkSLTokenKind::NK_END_OF_FILE);
```

L'union de `NkSLToken` est un **piège classique** : un seul de ses membres est valide à la fois. On ne lit `floatVal` que si `kind == NK_LIT_FLOAT`, `boolVal` que si `kind == NK_LIT_BOOL`, etc. Lire `intVal` sur un token flottant ne renvoie pas une conversion : ce sont les mêmes octets réinterprétés, donc une valeur fausse.

Le lexer n'est pas qu'un flux à sens unique : il sait **regarder devant sans consommer**. `Peek()` montre le prochain token en le laissant en place, `PeekAt(offset)` regarde N tokens plus loin (le *lookahead* dont le parser a besoin pour décider, par exemple, si un identifiant suivi d'une parenthèse est un appel de fonction ou un cast). `IsAtEnd()` dit si la source est épuisée, `GetLine()`/`GetColumn()` donnent la position courante, et `GetErrors()` rend les erreurs purement lexicales accumulées (caractère illégal, littéral malformé).

> **En résumé.** `NkSLLexer` transforme le texte source en `NkSLToken` typés (`Next` consomme, `Peek`/`PeekAt` regardent devant). Chaque token porte genre + texte + ligne/colonne + valeur. L'union de valeur se lit **selon le `kind`** uniquement. C'est la première passe : pas de grammaire ici, juste de la segmentation.

---

## L'analyse syntaxique : `NkSLParser`

Une fois le texte en tokens, encore faut-il vérifier qu'ils **s'enchaînent selon la grammaire** du langage — qu'une déclaration de fonction a bien un type de retour, un nom, des parenthèses, un corps ; qu'une expression respecte la précédence des opérateurs. C'est le **parser**, un *récursif descendant* : il lit le flux de tokens et bâtit l'**arbre de syntaxe abstraite**, dont la racine est un `NkSLProgramNode*`.

Le parser ne possède **pas** le lexer : il en garde une **référence**. Le lexer doit donc survivre au moins aussi longtemps que le parser — détruire le lexer puis appeler `Parse()` est un usage-après-libération.

```cpp
NkSLLexer  lexer(source, "shader.nksl");
NkSLParser parser(lexer);              // référence — le lexer doit rester vivant
NkSLProgramNode* ast = parser.Parse(); // construit tout l'arbre

if (parser.HasErrors()) {
    for (const auto& e : parser.GetErrors()) { /* rapporter ligne + message */ }
}
```

`Parse()` consomme tout le programme et renvoie l'arbre. Au passage, il **collecte la taille de groupe de travail compute** : les directives `layout(local_size_x = …, …) in;` sont rangées dans le `NkSLProgramNode` final, prêtes pour le backend compute. Les erreurs et avertissements se relisent par `GetErrors()` / `GetWarnings()`, et `HasErrors()` est le raccourci à tester avant de continuer.

Deux comportements méritent qu'on les connaisse. D'abord la **robustesse** : le parser est borné en profondeur de récursion (`kMaxParseDepth = 512`). Une expression pathologiquement imbriquée (`((((((…))))))`) ne fait pas exploser la pile native — au-delà du seuil, le parser émet une erreur et cesse de descendre. Et après une erreur, il passe en **mode panique** (`Synchronize`) : plutôt que de produire une cascade d'erreurs parasites, il saute jusqu'à un point sûr (fin d'instruction) et reprend, ce qui permet de signaler plusieurs vraies erreurs en une seule compilation.

Ensuite, les **déclarations multiples**. En GLSL on écrit `vec3 a, b, c;`. Plutôt qu'un nœud « groupe » artificiel, le parser renvoie le premier déclarateur et empile les suivants dans une file interne que l'appelant draine — une seule liste de `NkSLVarDeclNode`, sans niveau d'indirection superflu.

> **En résumé.** `NkSLParser` lit les tokens et bâtit l'AST (`Parse()` → `NkSLProgramNode*`). Il **référence** le lexer (durée de vie à garantir), borne sa récursion à 512 (anti stack-overflow), récupère après erreur en mode panique, et collecte la taille de workgroup compute. Vérifiez `HasErrors()` avant d'aller plus loin.

---

## La table des symboles : `NkSLSymbolTable`

Vérifier les types suppose de savoir, à chaque point du code, **quels noms existent et ce qu'ils désignent** : `color` est-il une variable locale, un paramètre, un uniform global ? `clamp` est-il une fonction connue, et avec quelles surcharges ? Une variable d'un bloc `{ }` cesse-t-elle d'exister à l'accolade fermante ? C'est le rôle de la **table des symboles** : un empilement de **portées** (`NkSLScope`) imbriquées, chacune étant un dictionnaire nom → symbole, chaînée à sa portée parente.

Le cœur du dispositif est `NkSLResolvedType`, le **type résolu** après analyse — la forme « comprise » d'un type, par opposition au type tel qu'écrit. Il porte un `base` (le genre fondamental : `NK_FLOAT`, `NK_VEC3`, `NK_MAT4`, un struct…), le nom du struct le cas échéant, une taille de tableau, et des drapeaux (`isConst`, `isRef` pour les paramètres `out`/`inout`, `isUnsized`). Il expose une batterie de **prédicats** (`IsScalar`, `IsVector`, `IsMatrix`, `IsSampler`, `IsNumeric`, `IsFloat`…) et de **fabriques** (`Float()`, `Vec3()`, `Mat4()`…) qui rendent le code de l'analyse lisible.

```cpp
NkSLSymbolTable table;                 // les builtins (fonctions/variables) sont déjà enregistrés
table.PushScope(/*isFunction=*/true, /*isLoop=*/false, NkSLResolvedType::Vec4());

NkSLSymbol sym;
sym.name = "uv";
sym.kind = NkSLSymbolKind::NK_VARIABLE;
sym.type = NkSLResolvedType::Vec2();
table.Define(sym);                     // false si le nom existe déjà dans CE scope

NkSLSymbol* found = table.Resolve("uv");   // remonte vers les parents si besoin
// ... fin de bloc :
table.PopScope();
```

`PushScope` / `PopScope` ouvrent et ferment une portée (un bloc, une fonction, une boucle). `Define` insère un symbole dans la portée **courante** (et renvoie `false` en cas de collision de nom). `Resolve` cherche un nom de la portée courante vers les parentes, en remontant la chaîne — c'est le *shadowing* lexical naturel : une variable locale masque une globale du même nom. `ResolveFunction` va plus loin : à partir d'un nom **et** d'une liste de types d'arguments, elle choisit la bonne **surcharge** (`NkSLFunctionOverload`).

La table connaît aussi le **contexte** : `IsInLoop()` (pour valider `break`/`continue`), `IsInFunction()`, `CurrentReturnType()` (pour vérifier qu'un `return` produit le bon type). Et elle offre les **règles de typage** sous forme de statiques : `IsAssignable`, `IsImplicitlyConvertible`, `BinaryResultType` (le type de `vec3 * float`, par exemple, est `vec3`), `VectorSize`.

Attention à l'**ownership** : la table **possède** ses `NkSLScope*` (les libère au `PopScope`), mais les pointeurs `NkSLSymbol*` / `NkSLScope*` qu'elle vous rend sont **internes et invalidables** — un `PopScope` ou un rehash de la map les périme. On les utilise tout de suite, on ne les conserve pas. Les pointeurs `decl` / `structDecl` / `varDecl` d'un symbole, eux, visent l'**AST** (possédé ailleurs) : la table ne les libère jamais. Et rien n'est thread-safe.

> **En résumé.** `NkSLSymbolTable` empile des portées (`PushScope`/`PopScope`) et résout les noms du courant vers les parents (`Resolve`, `ResolveFunction` pour les surcharges). `NkSLResolvedType` est le type « compris ». La table **possède** ses scopes mais les pointeurs rendus sont **invalidables** — ne les gardez pas. Les `*DeclNode*` pointent vers l'AST, non possédé par la table.

---

## L'analyse sémantique : `NkSLSemantic`

Le parser garantit que le code est **bien formé** ; il ne garantit pas qu'il a un **sens**. `float x = vec3(1.0);` passe la grammaire mais n'a aucun sens — on affecte un vecteur à un scalaire. `texture(2.0, uv)` aussi — le premier argument devrait être un sampler. `break;` hors d'une boucle, un swizzle `.xyzw` sur un `float`, un même binding réutilisé deux fois : autant d'erreurs que **seule la sémantique** peut attraper. `NkSLSemantic` parcourt l'AST, infère et vérifie les types à l'aide de la table des symboles, et rend un verdict.

L'analyse est faite **pour un stage donné** (`NkSLStage`) : un vertex shader et un fragment shader n'ont pas les mêmes builtins ni les mêmes contraintes. On construit donc une `NkSLSemantic` par stage.

```cpp
NkSLSemantic sem(stage);                       // stage = vertex / fragment / compute…
NkSLSemanticResult res = sem.Analyze(ast);     // l'AST n'est pas possédé par l'analyseur

if (!res.success) {
    for (const auto& e : res.errors)   { /* … */ }
    for (const auto& w : res.warnings) { /* … */ }
}
```

`Analyze(program)` renvoie un `NkSLSemanticResult` : un drapeau `success`, plus les vecteurs `errors` et `warnings`. La structure offre `AddError(line, msg)` (qui force `success = false`) et `AddWarning(line, msg)`. Au passage, l'analyse **collecte l'interface du shader** — ce dont le backend et le moteur ont besoin pour brancher les ressources : `GetInputVars()`, `GetOutputVars()`, `GetUniforms()`, `GetBlocks()` (les blocs uniform/buffer), et `GetEntryPoints()` (les fonctions marquées `@entry`).

Deux statiques publiques complètent le tableau. `TypeName(resolvedType)` rend le nom lisible d'un type résolu — précieux pour formuler des messages d'erreur (« attendu `vec3`, reçu `vec2` »). Et surtout `CheckStageInterface(vertexSem, fragmentSem, errors)` réalise la vérification **inter-stages** : elle confronte les *outputs* du vertex aux *inputs* du fragment et signale toute incohérence (un varying produit d'un côté mais pas consommé du même type de l'autre), en accumulant les erreurs dans le vecteur fourni.

> **En résumé.** `NkSLSemantic` analyse l'AST pour un stage (`Analyze` → `NkSLSemanticResult{success, errors, warnings}`) : inférence et vérification de types, validations (swizzle, appels builtins, constructeurs, `break`/`continue`, bindings). Elle **collecte l'interface** (inputs/outputs/uniforms/blocks/entry points) et offre `CheckStageInterface` pour valider vertex ↔ fragment.

---

## Aperçu de l'API

Tous les éléments publics du frontend, regroupés par fichier. Le détail de chacun est dans la « Référence complète ».

### `NkSLLexer.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Énumération | `NkSLTokenKind` | Genre d'un token : littéraux, identifiant, mots-clés de types (scalaires/vecteurs/matrices), samplers, images, qualificateurs de stockage, interpolation, précision, contrôle de flux, struct/layout, compute (`local_size_*`), const, annotations `@`, builtins, opérateurs, délimiteurs, `NK_END_OF_FILE`, `NK_UNKNOWN`. |
| Token | `NkSLToken` | Unité lexicale produite. |
| Champ | `.kind` | Genre (`NkSLTokenKind`). |
| Champ | `.text` | Texte brut du token (`NkString`). |
| Champ | `.line` `.column` | Position source. |
| Champ (union) | `.intVal` `.uintVal` `.floatVal` `.boolVal` | Valeur du littéral — **un seul actif**, selon `kind`. |
| Construction | `NkSLLexer(source, filename = "shader")` | Copie la source ; `filename` pour les erreurs. |
| Lecture | `Next()` | Consomme et renvoie le token suivant. |
| Lecture | `Peek()` | Token suivant sans le consommer. |
| Lecture | `PeekAt(offset)` | Lookahead de N tokens. |
| État | `IsAtEnd()` | Fin de source ? |
| État | `GetLine()` `GetColumn()` | Position courante (1-based). |
| Erreurs | `GetErrors()` | Erreurs lexicales accumulées. |

### `NkSLParser.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkSLParser(lexer&)` | Garde une **référence** au lexer (durée de vie à garantir). |
| Analyse | `Parse()` | Parse le programme, renvoie `NkSLProgramNode*` (avec taille de workgroup compute). |
| Erreurs | `GetErrors()` | Erreurs de syntaxe. |
| Avertissements | `GetWarnings()` | Avertissements. |
| Test | `HasErrors()` | `true` si au moins une erreur. |

### `NkSLSymbolTable.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Type résolu | `NkSLResolvedType` | Type compris après analyse. |
| Champs | `.base` `.typeName` `.arraySize` `.isUnsized` `.isConst` `.isRef` | Genre / nom du struct / taille tableau / drapeaux. |
| Prédicats | `IsVoid` `IsScalar` `IsVector` `IsMatrix` `IsSampler` `IsImage` `IsNumeric` `IsFloat` | Catégorie du type (inline). |
| Méthodes | `ComponentCount()` `ScalarBase()` | Nombre de composantes / type scalaire sous-jacent. |
| Opérateurs | `operator==` `operator!=` | Égalité (base/typeName/arraySize). |
| Fabriques | `Void/Bool/Int/UInt/Float/Vec2/Vec3/Vec4/Mat3/Mat4()` · `FromNode(typeNode)` | Construction directe d'un type résolu. |
| Énumération | `NkSLSymbolKind` | `NK_VARIABLE` `NK_FUNCTION` `NK_STRUCT_TYPE` `NK_PARAMETER` `NK_BUILTIN_VAR` `NK_BUILTIN_FUNC`. |
| Surcharge | `NkSLFunctionOverload` | Une signature de fonction. |
| Champs | `.paramTypes` `.returnType` `.decl` `.isBuiltin` | Paramètres / retour / AST (nullptr si builtin) / builtin ?. |
| Symbole | `NkSLSymbol` | Une entrée de la table. |
| Champs | `.name` `.kind` `.type` `.overloads` `.structDecl` `.varDecl` `.line` | Nom / genre / type / surcharges / AST struct / AST var / ligne. |
| Portée | `NkSLScope` | Une portée lexicale. |
| Champs | `.symbols` `.parent` `.returnType` `.isFunction` `.isLoop` | Dico nom→symbole / parent / retour courant / drapeaux. |
| Méthodes | `Find(name)` `Define(sym)` `Has(name)` | Sur **ce** scope seulement. |
| Table | `NkSLSymbolTable()` | Enregistre les builtins. |
| Portées | `PushScope(isFunction, isLoop, returnType)` `PopScope()` `CurrentScope()` | Empiler / dépiler / scope courant. |
| Résolution | `Define(sym)` `Resolve(name)` `ResolveFunction(name, argTypes)` | Définir / résoudre (chaîné) / résoudre une surcharge. |
| Contexte | `IsInLoop()` `IsInFunction()` `CurrentReturnType()` | Pour valider `break`/`continue`/`return`. |
| Règles statiques | `IsAssignable` `IsImplicitlyConvertible` `BinaryResultType` `VectorSize` | Typage : affectabilité, conversion implicite, type d'une opération binaire, taille d'un vecteur. |

### `NkSLSemantic.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Résultat | `NkSLSemanticResult` | Verdict de l'analyse. |
| Champs | `.success` `.errors` `.warnings` | Réussite / erreurs / avertissements. |
| Méthodes | `AddError(line, msg)` `AddWarning(line, msg)` | Ajout (AddError force `success=false`). |
| Analyseur | `NkSLSemantic(stage)` | Analyse pour un `NkSLStage` (table de symboles propre). |
| Analyse | `Analyze(program)` | Vérifie l'AST, renvoie `NkSLSemanticResult`. |
| Interface | `GetInputVars()` `GetOutputVars()` `GetUniforms()` `GetBlocks()` `GetEntryPoints()` | Collecte des entrées/sorties/uniforms/blocs/points d'entrée. |
| Statiques | `TypeName(type)` `CheckStageInterface(vsSem, fsSem, errors)` | Nom lisible d'un type / vérification vertex ↔ fragment. |

---

## Référence complète

Chaque élément est repris en détail, avec ses usages — un compilateur de shaders sert tout le moteur : rendu et post-process, mais aussi l'UI/2D (shaders d'interface), le compute pour la physique, l'animation (skinning GPU), l'audio (DSP sur GPU), les outils d'éditeur (validation live), et l'IO de pipeline (cache de shaders).

### `NkSLTokenKind` — le vocabulaire du langage

C'est l'énumération qui nomme **chaque catégorie de lexème** que NkSL reconnaît. Elle n'est pas renumérotée explicitement : les valeurs sont implicites (0..N) dans l'ordre de déclaration. Son étendue dessine en creux le langage tout entier :

- **Littéraux** (`NK_LIT_INT`, `NK_LIT_UINT`, `NK_LIT_FLOAT`, `NK_LIT_DOUBLE`, `NK_LIT_BOOL`, `NK_LIT_STRING`) et l'**identifiant** (`NK_IDENT`) — les feuilles de l'arbre.
- **Mots-clés de types** : scalaires (`NK_KW_VOID/BOOL/INT/UINT/FLOAT/DOUBLE`), vecteurs (`NK_KW_BVEC2..4`, `IVEC2..4`, `UVEC2..4`, `VEC2..4`, `DVEC2..4`), matrices carrées et rectangulaires (`NK_KW_MAT2..4`, `MAT2X2..4X4`, `DMAT2..4`). De quoi typer toute la géométrie d'un shader.
- **Samplers et images** : la base (`NK_KW_SAMPLER2D`, `SAMPLER2D_SHADOW`, `SAMPLER2D_ARRAY`, `SAMPLERCUBE`, `SAMPLER3D`, variantes `ISAMPLER2D`/`USAMPLER2D`, images `IMAGE2D`/`IIMAGE2D`/`UIMAGE2D`) **plus** un jeu additionnel très complet (1D, cube array, multisample `SAMPLER2DMS`, toutes les déclinaisons entières/non signées des images 1D/3D/cube/2D-array). Couvre l'échantillonnage classique (rendu), les shadow maps (lookups de profondeur), et les images en lecture/écriture (compute, post-process).
- **Qualificateurs de stockage** (`NK_KW_IN/OUT/INOUT/UNIFORM/BUFFER/PUSH_CONSTANT/SHARED/WORKGROUP`), **interpolation** (`SMOOTH/FLAT/NOPERSPECTIVE`), **précision** (`LOWP/MEDIUMP/HIGHP`) — comment une donnée circule entre stages et comment elle est interpolée.
- **Contrôle de flux** (`IF/ELSE/FOR/WHILE/DO/BREAK/CONTINUE/RETURN/DISCARD/SWITCH/CASE/DEFAULT`), **struct/layout** (`STRUCT/LAYOUT`), **compute** (`LOCAL_SIZE_X/Y/Z`), **const** (`CONST/INVARIANT`).
- **Annotations NkSL** — l'extension au-delà de GLSL : `NK_AT_BINDING` (`@binding(set=0, binding=1)`), `NK_AT_LOCATION` (`@location(0)`), `NK_AT_PUSH_CONSTANT`, `NK_AT_STAGE` (`@stage(vertex)`), `NK_AT_ENTRY`, `NK_AT_BUILTIN` (`@builtin(position)`). C'est par elles que le moteur déclare bindings, locations et points d'entrée de façon portable entre APIs.
- **Builtins** (`BUILTIN_POSITION/FRAGCOORD/FRAGDEPTH/VERTEXID/INSTANCEID/FRONTFACING/LOCALINVID/GLOBALINVID/WORKINVID`) — les variables système des différents stages (position de sortie vertex, coordonnée de fragment, ID d'invocation compute…).
- **Opérateurs** (arithmétiques, bit-à-bit, logiques, comparaisons, affectations composées `+=`…`>>=`, `++`/`--`, `.`/`,`/`;`/`:`/`?`) et **délimiteurs** (`( ) [ ] { }`, `@`, `#`).
- **Spéciaux** : `NK_END_OF_FILE` (sentinelle de fin, à tester pour stopper le flux) et `NK_UNKNOWN` (caractère non reconnu → erreur lexicale).

### `NkSLToken` — l'unité produite

Le token est ce que `Next`/`Peek` renvoient. `kind` le classe, `text` garde son écriture brute (utile pour les identifiants et les messages), `line`/`column` le situent à la source. Pour un littéral, sa valeur déjà décodée vit dans une **union anonyme** : `intVal` (entier signé), `uintVal`, `floatVal` (et double), `boolVal`. L'union est le point sensible : ses membres partagent la mémoire, donc **lire le mauvais membre rend des octets faux**. La règle est mécanique — `NK_LIT_FLOAT` → `floatVal`, `NK_LIT_BOOL` → `boolVal`, `NK_LIT_INT` → `intVal`, etc. — et c'est au consommateur de la respecter.

### `NkSLLexer` — le segmenteur

Construit sur une copie de la source (`NkSLLexer(source, filename)`), il expose un modèle simple et robuste :

- `Next()` est le moteur : il saute blancs et commentaires, lit le prochain lexème et renvoie le token. On boucle dessus jusqu'à `NK_END_OF_FILE`.
- `Peek()` / `PeekAt(offset)` donnent le **lookahead** sans avancer : le parser s'en sert pour trancher des ambiguïtés grammaticales (un `nom(` est-il un appel ou un cast de type ?). En interne, le lexer maintient un petit tampon de tokens déjà lus.
- `IsAtEnd()`, `GetLine()`, `GetColumn()` exposent l'état (ligne/colonne 1-based). `GetErrors()` rend les erreurs purement lexicales (caractère illégal via `NK_UNKNOWN`, nombre malformé).

Dans la pratique du moteur, le lexer sert rarement seul — il alimente le parser. Mais son flux de tokens est aussi la brique d'**outils d'éditeur** : coloration syntaxique, indentation automatique, comptage de tokens, qui n'ont besoin que de la segmentation, pas de l'arbre.

### `NkSLParser` — le constructeur d'arbre

C'est un **récursif descendant** classique, organisé en montée de précédence pour les expressions et en méthodes dédiées par construction (déclarations, statements). On retient quatre points côté API :

- **Référence, pas possession.** `NkSLParser(lexer&)` capture le lexer par référence : on construit toujours les deux ensemble et on les laisse mourir ensemble. Détacher leur durée de vie est un bug.
- **`Parse()`** est l'unique point d'entrée : il rend l'AST racine et, ce faisant, propage la taille de workgroup compute (`layout(local_size_*) in;`) dans le `NkSLProgramNode`. Le moteur s'en sert directement pour dimensionner les *dispatch* compute (physique sur GPU, génération de terrain, simulation de particules).
- **Diagnostic groupé.** `GetErrors()`, `GetWarnings()`, `HasErrors()` exposent un rapport **complet** d'une seule passe, grâce au mode panique : on récupère plusieurs vraies erreurs au lieu de s'arrêter à la première — exactement ce qu'attend une boucle de compilation d'éditeur, où l'on veut tout corriger d'un coup.
- **Bornage.** La profondeur est plafonnée à 512 : une entrée hostile ou générée ne fait pas planter le compilateur, elle produit une erreur propre. C'est une garantie de robustesse pour un pipeline qui compile des shaders venus de fichiers, de l'éditeur, ou d'un cache.

### `NkSLResolvedType` — le type compris

Toute l'analyse sémantique manipule des `NkSLResolvedType`, pas des chaînes. C'est la représentation **normalisée** d'un type : son `base` (genre fondamental), `typeName` (si `base == STRUCT`), `arraySize` (`0` = pas un tableau), et les drapeaux `isConst`, `isRef` (paramètre `out`/`inout`), `isUnsized`.

- **Prédicats** (inline, const) pour classer sans recopier la logique partout : `IsVoid`, `IsScalar` (base dans `[BOOL, DOUBLE]`), `IsVector` (`[IVEC2, DVEC4]`), `IsMatrix` (`[MAT2, DMAT4]`), `IsSampler`/`IsImage` (via les helpers libres `NkSLTypeIsSampler`/`NkSLTypeIsImage`), `IsNumeric` (scalaire ∪ vecteur ∪ matrice), `IsFloat` (float, vecteur float, ou matrice).
- **Méthodes** : `ComponentCount()` rend le nombre de composantes scalaires (1 pour un float, 4 pour un vec4, 16 pour un mat4) — la base pour vérifier un constructeur (`vec3(a, b)` a-t-il assez de composantes ?) ou calculer une taille de layout. `ScalarBase()` rend le scalaire sous-jacent (`vec3` → float, `ivec2` → int) — utile pour les promotions.
- **Égalité** : `operator==` compare `base`, `typeName`, `arraySize` (il **ignore** `isConst`/`isRef`/`isUnsized`) : deux types sont « le même » au sens structurel, indépendamment de leur qualification — ce qu'il faut pour comparer un argument à un paramètre attendu.
- **Fabriques** : `Float()`, `Vec3()`, `Mat4()`, etc., et `FromNode(typeNode)` qui résout un nœud de type de l'AST en `NkSLResolvedType`. Elles rendent le code de vérification déclaratif.

### `NkSLSymbolKind`, `NkSLFunctionOverload`, `NkSLSymbol`, `NkSLScope`

Ces structures portent **ce que la table sait** d'un nom :

- `NkSLSymbolKind` distingue ce qu'un nom **est** : variable, fonction, type struct, paramètre, ou builtin (variable / fonction). Cela conditionne ce qu'on a le droit d'en faire (on n'appelle pas une variable, on ne lit pas une fonction comme une valeur).
- `NkSLFunctionOverload` est **une** signature : `paramTypes`, `returnType`, le pointeur `decl` vers l'AST de la fonction (`nullptr` pour un builtin, qui n'a pas de corps source) et `isBuiltin`. Une même fonction peut avoir plusieurs surcharges (c'est tout l'intérêt de `clamp`, `mix`, `texture` qui acceptent plusieurs types).
- `NkSLSymbol` agrège tout : `name`, `kind`, `type` (pour les variables/paramètres), `overloads` (pour les fonctions), `structDecl`/`varDecl` (pointeurs AST), `line`. C'est l'entrée stockée dans une portée.
- `NkSLScope` est une **portée** : sa map `symbols`, son `parent` (chaînage vers la portée englobante), le `returnType` de la fonction courante, et les drapeaux `isFunction`/`isLoop`. Ses méthodes `Find`/`Define`/`Has` agissent sur **ce seul scope** (sans remonter) — la remontée, c'est `Resolve` au niveau de la table. `Find` rend un pointeur dans la map, donc invalidable par rehash.

### `NkSLSymbolTable` — la résolution de noms

La table orchestre les portées et les règles de typage :

- **Cycle de vie d'une portée** : `PushScope(isFunction, isLoop, returnType)` à l'entrée d'un bloc/fonction/boucle, `PopScope()` à la sortie, `CurrentScope()` pour le scope actif. À la construction, elle enregistre tous les **builtins** (fonctions GLSL et variables système) — d'où le fait que `texture`, `normalize`, `gl_Position` & co. soient résolus sans qu'on les déclare.
- **Définir et résoudre** : `Define(sym)` insère dans le scope courant (false si conflit) ; `Resolve(name)` cherche du courant vers les parents (le *shadowing* lexical) ; `ResolveFunction(name, argTypes)` choisit la surcharge dont les paramètres correspondent aux arguments.
- **Contexte de validation** : `IsInLoop()` (un `break`/`continue` est-il légal ici ?), `IsInFunction()`, `CurrentReturnType()` (un `return` produit-il le bon type ?).
- **Règles de typage statiques** : `IsAssignable(from, to)` et `IsImplicitlyConvertible(from, to)` (peut-on affecter / convertir sans cast ?), `BinaryResultType(op, lhs, rhs)` (le type de `lhs op rhs` — `vec3 * float` → `vec3`), `VectorSize(base)` (composantes d'un type vecteur). Ce sont les briques que la sémantique appelle à chaque expression.

Ces mécaniques servent **tout** code de shader : résoudre une variable d'éclairage en rendu, une fonction de bruit en génération procédurale, un built-in compute (`gl_GlobalInvocationID`) en physique GPU, un swizzle de couleur en UI.

### `NkSLSemanticResult` et `NkSLSemantic` — le verdict

`NkSLSemanticResult` est ce qu'on lit après l'analyse : `success`, `errors`, `warnings`, plus les ajouteurs `AddError`/`AddWarning` (le premier bascule `success` à false). C'est le contrat de sortie du frontend : si `success` est faux, on n'avance pas vers le backend.

`NkSLSemantic(stage)` porte sa propre `NkSLSymbolTable` et analyse pour un stage précis. `Analyze(program)` fait le walk complet de l'AST (sans posséder l'arbre) et renvoie le résultat. Mais son apport va au-delà du oui/non : pendant l'analyse, elle **collecte l'interface** du shader, exactement ce que le moteur doit brancher pour exécuter le shader :

- `GetInputVars()` / `GetOutputVars()` — les varyings et attributs (pour câbler le format de sommets en rendu, ou les varyings entre stages).
- `GetUniforms()` / `GetBlocks()` — uniforms simples et blocs uniform/buffer (pour allouer et lier les UBO/SSBO : matrices de caméra en rendu, paramètres de simulation en compute, données d'instance en animation).
- `GetEntryPoints()` — les fonctions `@entry` (pour savoir quelle fonction lancer par stage).

Les deux statiques publiques outillent le diagnostic et la liaison de pipeline : `TypeName(type)` produit le nom lisible d'un type résolu (messages d'erreur clairs dans l'éditeur), et `CheckStageInterface(vsSem, fsSem, errors)` confronte les sorties d'un vertex aux entrées d'un fragment — la garantie qu'un programme complet (et pas chaque stage isolé) est cohérent avant de construire le PSO.

### Idiomes et pièges transverses

- **Le pipeline frontend complet** : `NkSLLexer lexer(src, file); NkSLParser parser(lexer); NkSLProgramNode* ast = parser.Parse();` puis `NkSLSemantic sem(stage); auto res = sem.Analyze(ast);`. On vérifie `parser.HasErrors()` puis `res.success` — on n'enchaîne sur le backend que si les deux passent.
- **Vérification cross-stage** : on analyse vertex et fragment dans **deux** `NkSLSemantic` distincts, puis on appelle `NkSLSemantic::CheckStageInterface(vsSem, fsSem, errors)` pour valider le raccord.
- **Ownership** : le parser **référence** le lexer (durée de vie à garantir) ; la table **possède** ses `NkSLScope*` mais rend des pointeurs **invalidables** (`PopScope`/rehash) qu'on ne conserve pas ; les `*DeclNode*` rendus par les getters sémantiques et par les symboles pointent vers l'**AST** possédé ailleurs — on ne libère jamais via ces vecteurs/pointeurs.
- **L'union de `NkSLToken`** se lit selon `kind`, jamais autrement.
- **Zéro-STL** : tout est `NkVector` / `NkUnorderedMap` / `NkString`, allocation conforme NKMemory. Les objets du frontend ont une durée de vie pile/membre standard (pas de Create/Destroy explicite dans ces headers).

---

### Exemple

```cpp
#include "NKSL/Frontend/NkSLLexer.h"
#include "NKSL/Frontend/NkSLParser.h"
#include "NKSL/Frontend/NkSLSemantic.h"
using namespace nkentseu;

// 1) Lexer : texte → tokens.
NkSLLexer lexer(vertexSource, "lit.vert.nksl");

// 2) Parser : tokens → AST (référence le lexer, qui doit rester vivant).
NkSLParser parser(lexer);
NkSLProgramNode* ast = parser.Parse();
if (parser.HasErrors()) {
    for (const auto& e : parser.GetErrors()) { /* rapporter ligne + message */ }
    return;
}

// 3) Sémantique : AST → AST typé et validé, pour un stage donné.
NkSLSemantic vsSem(NkSLStage::/* VERTEX */ {});
NkSLSemanticResult res = vsSem.Analyze(ast);
if (!res.success) {
    for (const auto& e : res.errors)   { /* erreurs de type, swizzle, binding… */ }
    return;
}

// L'interface collectée alimente le branchement de pipeline.
const auto& uniforms = vsSem.GetUniforms();      // UBO à lier
const auto& entries  = vsSem.GetEntryPoints();   // fonctions @entry

// 4) Cohérence vertex ↔ fragment.
NkVector<NkSLCompileError> linkErrors;
if (!NkSLSemantic::CheckStageInterface(vsSem, fsSem, linkErrors)) {
    // un output vertex ne correspond pas à l'input fragment attendu
}
```

---

[← Index NKSL](README.md) · [Récap NKSL](../NKSL.md) · [Couche Runtime](../README.md)
