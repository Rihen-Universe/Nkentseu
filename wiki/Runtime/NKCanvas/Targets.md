# Les cibles de rendu

> Couche **Runtime** · NKCanvas · *Où* le dessin atterrit : la surface abstraite `NkRenderTarget`,
> la fenêtre `NkRenderWindow`, la texture offscreen `NkRenderTexture`, leur table de dispatch GPU
> `NkRenderTextureBackend`, et le pont d'interface `NkUICanvasBackend`.

Quand on dessine quelque chose, la première question n'est pas *quoi* mais ***où***. Un sprite, un
maillage 2D, une interface — il leur faut une **surface** sur laquelle se poser : l'écran, ou bien
une texture cachée qu'on ré-affichera plus tard. C'est tout le rôle de la famille « Targets ». Elle
calque la trinité de SFML — `sf::RenderTarget`, `sf::RenderWindow`, `sf::RenderTexture` — pour qu'on
écrive le **même code de dessin** quel que soit l'endroit où il finit. Le principe directeur tient
en une phrase : **on dessine sur une cible, pas sur une fenêtre.** Une fois ce réflexe pris, rendre
vers l'écran ou vers une mini-carte devient le même geste.

La famille s'articule autour d'une **classe abstraite**, `NkRenderTarget`, dont héritent deux
surfaces concrètes : `NkRenderWindow` (le *back buffer* d'une fenêtre) et `NkRenderTexture` (un
*framebuffer* offscreen, une texture). À côté vit `NkRenderTextureBackend`, la petite table de
fonctions qui branche le offscreen sur chaque API GPU, et `NkUICanvasBackend`, le pont qui fait
rendre une interface NKUI à travers le renderer 2D de NKCanvas.

- **Namespace** : `nkentseu::renderer` (sauf `NkWindow` / `NkIGraphicsContext`, dans `nkentseu`, et
  `NkUIContext`, dans `nkentseu::nkui`)
- **Headers** : `NKCanvas/Renderer/Targets/NkRenderTarget.h`,
  `NKCanvas/Renderer/Targets/NkRenderWindow.h`, `NKCanvas/Renderer/Targets/NkRenderTexture.h`,
  `NKCanvas/Renderer/Targets/NkRenderTextureBackend.h`, `NKCanvas/UI/NkUICanvasBackend.h`

---

## La surface abstraite : `NkRenderTarget`

C'est le contrat que partagent toutes les cibles. `NkRenderTarget` ne sait pas où elle dessine — sur
un écran ou dans une texture, peu lui importe : elle expose un vocabulaire commun (vider, dessiner,
présenter, régler la caméra) que les deux dérivées concrétisent chacune à leur façon. Cette
abstraction est ce qui permet d'écrire une fonction `void DessinerLaScene(NkRenderTarget& cible)` et
de l'appeler indifféremment sur la fenêtre ou sur une texture offscreen — c'est l'équivalent exact
de `sf::RenderTarget`.

Une frame suit toujours le même rituel : on **vide** la surface (`Clear`), on **dessine** dessus
(`Draw`), puis on **présente** (`Display`). `Clear(color)` remplit le framebuffer d'une couleur
(noir par défaut) ; `Display()` finalise — *swap* des buffers pour une fenêtre, finalisation du FBO
pour une texture. Oublier `Display()` à la fin de frame, et rien n'apparaît.

Le dessin se fait par `Draw`, qui existe en plusieurs surcharges visant deux mondes. Le monde
**moderne** repose sur `NkDrawable` : `Draw(const NkDrawable&, states)` est non-virtuel et délègue
simplement à `drawable.Draw(*this, states)` — c'est le *drawable* qui se charge de rappeler
`target.Draw(vertices, …)`. La forme `Draw(const NkVertexArray&, states)` est un raccourci du même
acabit. Tout converge vers le **point d'entrée bas niveau**, l'unique virtuelle pure
`Draw(const NkVertex*, count, primitive, states)` : c'est elle que chaque dérivée implémente, c'est
elle qui parle vraiment au backend. Une dernière surcharge, `Draw(const NkIDrawable2D&)`, n'existe
que pour la **compatibilité** avec l'ancienne interface, et disparaîtra après la migration de
`NkSprite`/`NkText`.

Vient ensuite la **caméra 2D** : `SetView`/`GetView` règlent et relisent le `NkView2D` courant
(la portion du monde regardée), `GetDefaultView()` rend la vue par défaut (celle qui colle un pixel
écran sur une unité monde). Le **viewport** (`SetViewport`/`GetViewport`) délimite, en pixels, la
*sous-région* de la cible où ce view est projeté — la base du *split-screen* ou des mini-cartes. Et
parce que view et viewport déforment la correspondance écran↔monde, `MapPixelToCoords` /
`MapCoordsToPixel` traduisent dans les deux sens : indispensable pour savoir *quelle case de la
grille* le clic souris vient de toucher.

Enfin, l'accès au renderer sous-jacent existe sous deux formes : `GetRenderer()` renvoie le
`NkIRenderer2D*` **bas niveau**, tandis que `GetRenderer2D()` renvoie une **façade**
`NkRenderer2D&` plus conviviale, façon SFML, qui redirige correctement vers `*this`.

Ce n'est **pas** un contexte GPU ni un renderer : `NkRenderTarget` ne possède ni l'un ni l'autre,
elle ne fait que **les exposer**. C'est une surface, pas un moteur.

> **En résumé.** `NkRenderTarget` = la surface abstraite façon `sf::RenderTarget`. Rituel
> `Clear → Draw → Display`. Toutes les `Draw` convergent vers l'unique virtuelle pure
> `Draw(vertices, count, primitive, states)`. View + viewport définissent *quoi* et *où* projeter ;
> `MapPixelToCoords`/`MapCoordsToPixel` font le pont écran↔monde. Écrivez votre code de dessin
> contre cette base, jamais contre une cible précise.

---

## La cible écran : `NkRenderWindow`

`NkRenderWindow` est la cible « normale » : elle rend dans le *back buffer* d'une `NkWindow`. C'est
l'équivalent direct de `sf::RenderWindow`. Sous le capot, elle assemble trois pièces : une
`NkWindow&` (la fenêtre native), un `NkIGraphicsContext*` (le contexte GPU, créé via
`NkContextFactory`) et un `NkIRenderer2D*` (le renderer 2D, créé via `NkRenderer2DFactory`).

Deux constructeurs, deux régimes de **propriété**. Le **chemin normal**,
`NkRenderWindow(window, desc)`, crée lui-même contexte et renderer à partir d'un `NkContextDesc` :
NKCanvas **possède** alors le contexte et le détruira (`mOwnsContext = true`). La **variante
avancée**, `NkRenderWindow(window, externalContext)`, accepte un contexte que l'utilisateur a déjà
`Initialize()` — NKCanvas se contente d'empiler un Renderer2D par-dessus et **ne possède pas** le
contexte (`mOwnsContext = false`) ; à l'appelant de le *Shutdown*/détruire, mais **après** le
`NkRenderWindow`. `IsValid()` confirme que les deux pièces ont bien été créées. La classe est
**non-copiable** (une fenêtre ne se duplique pas).

Au-delà des overrides hérités de `NkRenderTarget` (tous présents : `Clear`, `Display`, le couple
view, le couple viewport, `GetSize`, le `Draw` bas niveau, le mapping pixel↔monde, les accesseurs
renderer), `NkRenderWindow` ajoute des **accès avancés** inline : `GetContext()` rend le
`NkIGraphicsContext*`, `GetWindow()` la `NkWindow&`.

Le point délicat est la **recréation de la swapchain**. Quand la fenêtre change de taille ou de DPI,
le contexte GPU doit reconstruire ses buffers — sinon l'image est étirée, ou le programme crashe.
Deux méthodes branchent ça sur les événements fenêtre : `OnResize(width, height)`, à appeler depuis
le handler `NkWindowResizeEvent` avec des dimensions en **pixels physiques** (post-DPI), et
`OnDpiChange()`, à appeler depuis `NkWindowDpiEvent`, qui recalcule la taille physique via
`NkWindow::GetSize()` puis délègue à `OnResize`. La règle d'or : **les deux événements doivent mener
à la même routine de recréation** ; ne câbler que l'un des deux laisse des bugs DPI rampants.

Ce n'est **pas** une fenêtre : `NkRenderWindow` ne crée pas la `NkWindow`, elle la *référence*. La
fenêtre vit ailleurs (cf. NKWindow) ; la cible ne fait que dessiner dedans.

> **En résumé.** `NkRenderWindow` = la cible écran (`sf::RenderWindow`), fenêtre + contexte +
> renderer. Ctor `desc` → NKCanvas possède le contexte ; ctor `externalContext` → l'appelant le
> possède (le détruire **après**). Non-copiable. **Branchez impérativement** `OnResize`
> (`NkWindowResizeEvent`, pixels physiques) **et** `OnDpiChange` (`NkWindowDpiEvent`) sur la même
> routine, sinon resize/DPI cassés.

---

## La cible offscreen : `NkRenderTexture`

`NkRenderTexture` rend dans une **texture** au lieu de l'écran. C'est l'équivalent de
`sf::RenderTexture`, et c'est la clé de tout ce qui « rend deux fois » : on dessine une scène dans un
*framebuffer* offscreen, puis on **réutilise le résultat comme texture** — pour un post-traitement,
une mini-carte, un portail, une vignette d'aperçu dans l'éditeur. Concrètement, elle crée un FBO et
une texture couleur attachée via `NkRenderTextureBackend`, le dispatch cross-API ; après `Display()`,
la texture est échantillonnable par son identifiant GPU.

> Note d'implémentation : à ce jour, seul OpenGL est livré (FBO + texture `GL_RGBA8`) ;
> Software/Vulkan/DX11/DX12/Metal sont des stubs et leur `Create` renvoie `false`. C'est l'état des
> backends, pas une limite de l'API publique.

Elle suit un cycle de vie **Create/Destroy** propre — conforme à la règle dure NKMemory (toute
classe avec `Create` a un `Destroy`). `Create(renderer, width, height)` fabrique le framebuffer
offscreen via le backend actif ; il **exige un renderer déjà initialisé**, car c'est l'`Initialize`
de ce renderer qui a installé le `NkRenderTextureBackend`. `Destroy()` libère le framebuffer et est
**idempotent** (l'appeler deux fois ne fait pas de mal — le destructeur l'appelle d'ailleurs).
`IsValid()` teste la présence d'un handle, `GetHandle()` le rend, et `GetColorTextureGPUId()` donne
l'**identifiant GPU de la texture couleur** — celui qu'on passe à `NkTexture::SetGPUId` ou à un
shader user qui veut la *sampler*. La classe est **non-copiable**.

Tous les overrides de `NkRenderTarget` sont présents (`Clear`, `Display`, view, viewport, `GetSize`
qui rend `{width, height}`, le `Draw` bas niveau, le mapping, les accesseurs renderer). Différence
**essentielle de propriété** avec la fenêtre : `NkRenderTexture` **ne possède pas** son renderer —
son `mRenderer` est une simple **référence non-propriétaire** vers le renderer principal. La texture
emprunte le moteur de la cible écran, elle ne le duplique pas.

Ce n'est **pas** un `NkTexture` : `NkRenderTexture` est une *cible* sur laquelle on dessine ; le
résultat, lui, s'expose comme texture via `GetColorTextureGPUId()`. Et `Create` **peut échouer**
(retour `false` si le backend ne supporte pas le offscreen) — il faut toujours vérifier.

> **En résumé.** `NkRenderTexture` = rendu offscreen vers texture (`sf::RenderTexture`), pour
> post-process / mini-carte / portail / aperçu éditeur. `Create(renderer, w, h)` exige un renderer
> **déjà initialisé** et peut renvoyer `false` ; `Destroy` idempotent ; renderer **non-possédé**.
> Le résultat se *sample* via `GetColorTextureGPUId()`. Non-copiable.

---

## Le dispatch GPU : `NkRenderTextureBackend`

Pour qu'une `NkRenderTexture` fonctionne sur n'importe quelle API graphique, il faut un point de
branchement uniforme : `NkRenderTextureBackend` est cette **table de cinq pointeurs de fonction** —
exactement le même patron que `NkTextureBackend` ou `NkShaderBackend`. Chaque renderer enregistre sa
table à son `Initialize()` ; `NkRenderTexture` n'appelle plus que ces callbacks, sans rien savoir de
GL, Vulkan ou DX.

Les cinq champs (tous initialisés à `nullptr`) couvrent le cycle d'un FBO : `Create(w, h)` renvoie un
handle `uint32 > 0` si OK (0 sinon) — handle interne au backend (FBO GL, index `VkFramebuffer`, slot
RTV DX11…) ; `Destroy(handle)` le libère ; `Bind(handle)` redirige les draws suivants vers le
offscreen (en sauvegardant le framebuffer courant pour pouvoir le restaurer) ; `Unbind()` restaure le
framebuffer principal (le *back buffer*) ; `GetColorTextureGPUId(handle)` rend l'identifiant
*samplable* par `NkTexture` (nom de texture GL, slot SRV…) **sans re-upload**.

Deux fonctions libres l'enregistrent et le neutralisent. `NkRenderTextureSetBackend(backend)` installe
la table active — appelée par chaque backend renderer en fin d'`Initialize`. Et
`NkRenderTextureInstallUnsupportedBackend(name)` pose un **stub** pour les renderers sans support FBO :
son `Create` renvoie 0, donc `NkRenderTexture::Create` renverra proprement `false` plutôt que de
crasher.

L'idiome à retenir : convention `Create → handle > 0`, et les paires `Bind`/`Unbind` (l'ancien FBO
restauré par `Unbind`). Ce n'est **pas** une API utilisateur : on n'appelle jamais ces fonctions à la
main dans du code applicatif — c'est de la plomberie que les backends remplissent eux-mêmes.

> **En résumé.** `NkRenderTextureBackend` = table de 5 fn-ptrs (`Create`/`Destroy`/`Bind`/`Unbind`/
> `GetColorTextureGPUId`) que chaque renderer enregistre via `NkRenderTextureSetBackend`. Convention
> handle > 0, paires Bind/Unbind, `GetColorTextureGPUId` rend l'id samplable sans re-upload. Stub via
> `NkRenderTextureInstallUnsupportedBackend`. Plomberie interne, pas une API user.

---

## Le pont interface : `NkUICanvasBackend`

`NkUICanvasBackend` fait dessiner une **interface NKUI** à travers le renderer 2D de NKCanvas. NKUI
produit, en mode *immediate*, des *draw-lists* (listes de triangles, de clips et de textures) ;
ce pont les traduit en appels au `NkIRenderer2D`. C'est l'équivalent de `NkUINKRHIBackend`, mais sans
la plomberie GPU bas niveau : `NkRenderer2D` absorbe déjà pipeline, buffers, clip et textures. Le pont
**possède** les textures qu'il a uploadées (atlas de police, images), indexées par le `texId` NKUI.

Atout architectural : le **header n'inclut pas NKUI** (il *forward-declare* `nkui::NkUIContext`), donc
il reste utilisable par n'importe quel consommateur sans lier NKUI ; seul le `.cpp` dépend du module.
Mieux, ce `.cpp` ne compile que si `NK_CANVAS_WITH_NKUI=1` (flag env `NK_CANVAS_NKUI`, cf.
`NKCanvas.jenga`) — l'intégration UI est donc **optionnelle à la compilation**.

Le cycle est **Init/Destroy** (toujours conforme à la règle NKMemory). `Init(renderer)` reçoit le
`NkIRenderer2D` actif (celui d'une `NkRenderWindow` ou un `NkRenderer2D`), à appeler une fois ;
`Destroy()` libère tout, y compris les textures possédées. Le cœur est
`Submit(ctx, fbW, fbH)` : il rend **toutes les couches** du contexte NKUI, et **doit être appelé
entre le `Begin()` et le `End()` de la cible** — `fbW`/`fbH` (taille framebuffer) servent au calcul
du clip. Pour alimenter les textures, deux uploads : `UploadTextureRGBA8(texId, data, w, h)` pour des
données RGBA (`w*h*4` octets) et `UploadTextureGray8(texId, data, w, h)` pour du niveau de gris
(`w*h` octets, étendu en RGBA blanc + alpha) — c'est ce dernier qui sert l'**atlas de police** NKUI.
`HasTexture(texId)` interroge la présence d'une texture. La classe est **non-copiable**.

Pièges à connaître : le `texId` doit être **non nul** ; `Submit` **hors** d'une paire `Begin`/`End`
ne dessine rien ; et le pont **possède** ses `NkTexture*` (libérées par `Destroy`) — ne pas tenter de
les détruire soi-même.

> **En résumé.** `NkUICanvasBackend` = pont NKUI → NKCanvas (`NkIRenderer2D`), façon
> `NkUINKRHIBackend` sans plomberie GPU. Header sans dépendance NKUI (forward-declare) ; `.cpp`
> optionnel (`NK_CANVAS_WITH_NKUI`). Cycle `Init`/`Destroy` ; `Submit` **entre `Begin`/`End`** de la
> cible ; uploads RGBA8 (images) et Gray8 (atlas police), `texId != 0` ; possède ses textures.
> Non-copiable.

---

## Aperçu de l'API

La liste de **tous** les éléments publics, détaillés ensuite dans la « Référence complète ».

### `NkRenderTarget` — cible de rendu abstraite

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Destruction | `virtual ~NkRenderTarget()` | Destructeur virtuel (base polymorphe). |
| Frame | `Clear(color = Black)` (pure) | Vide le framebuffer avec une couleur. |
| Frame | `Display()` (pure) | Présente le rendu (swap / finalise). À appeler en fin de frame. |
| Caméra | `SetView(view)` / `GetView()` (pures) | Règle / relit le `NkView2D` courant. |
| Caméra | `GetDefaultView()` (pure) | Vue par défaut (1 pixel ↔ 1 unité). |
| Viewport | `SetViewport(rect)` / `GetViewport()` (pures) | Sous-région pixel où projeter le view. |
| Dimensions | `GetSize()` (pure) | Taille de la cible en pixels (`NkVec2u`). |
| Dessin | `Draw(drawable, states)` | Délègue à `drawable.Draw(*this, states)` (inline). |
| Dessin | `Draw(vertexArray, states)` | Raccourci tableau de vertices. |
| Dessin | `Draw(vertices, count, primitive, states)` (pure) | **Point d'entrée bas niveau** vers le backend. |
| Dessin (compat) | `Draw(const NkIDrawable2D&)` | Ancien style, à retirer après migration. |
| Mapping | `MapPixelToCoords(pixel)` (pure) | Pixel écran → coordonnée monde. |
| Mapping | `MapCoordsToPixel(point)` (pure) | Coordonnée monde → pixel écran. |
| Renderer | `GetRenderer()` (pure, 2 surcharges) | Accès `NkIRenderer2D*` bas niveau. |
| Renderer | `GetRenderer2D()` (pure, 2 surcharges) | Façade `NkRenderer2D&` style SFML. |

### `NkRenderWindow : NkRenderTarget` — cible écran

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkRenderWindow(window, desc)` | NKCanvas crée + possède le contexte. |
| Construction | `NkRenderWindow(window, externalContext)` | Contexte fourni, **non possédé**. |
| Construction | copie `= delete` | Non-copiable. |
| Destruction | `~NkRenderWindow()` | Détruit renderer puis contexte (si possédé). |
| Validité | `IsValid()` | Contexte + renderer créés ? |
| Overrides | `Clear` `Display` `SetView`/`GetView` `GetDefaultView` `SetViewport`/`GetViewport` `GetSize` `Draw(vertices…)` `MapPixelToCoords` `MapCoordsToPixel` | Implémentations de `NkRenderTarget`. |
| Overrides | `using NkRenderTarget::Draw;` | Réintroduit les surcharges masquées. |
| Overrides | `GetRenderer()` / `GetRenderer2D()` | Renderer bas niveau / façade (inline). |
| Accès | `GetContext()` (2 surcharges) | `NkIGraphicsContext*` sous-jacent. |
| Accès | `GetWindow()` (2 surcharges) | `NkWindow&` référencée. |
| Resize | `OnResize(width, height)` | Recrée la swapchain (pixels physiques). |
| Resize | `OnDpiChange()` | Recalcule la taille puis délègue à `OnResize`. |

### `NkRenderTexture : NkRenderTarget` — cible offscreen

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkRenderTexture()` | Constructeur par défaut. |
| Construction | copie `= delete` | Non-copiable. |
| Destruction | `~NkRenderTexture()` | Appelle `Destroy()` (inline). |
| Cycle de vie | `Create(renderer, w, h)` | Crée le FBO offscreen ; peut renvoyer `false`. |
| Cycle de vie | `Destroy()` | Libère le FBO ; **idempotent**. |
| Cycle de vie | `IsValid()` | Handle valide ? (inline) |
| Cycle de vie | `GetHandle()` | Handle interne (inline). |
| Cycle de vie | `GetColorTextureGPUId()` | Id GPU de la texture couleur (à sampler). |
| Overrides | `Clear` `Display` `SetView`/`GetView` `GetDefaultView` `SetViewport`/`GetViewport` `GetSize` `Draw(vertices…)` `MapPixelToCoords` `MapCoordsToPixel` | Implémentations de `NkRenderTarget`. |
| Overrides | `using NkRenderTarget::Draw;` | Réintroduit les surcharges masquées. |
| Overrides | `GetRenderer()` / `GetRenderer2D()` | Renderer **non-possédé** / façade (inline). |

### `NkRenderTextureBackend` — table de dispatch GPU

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Champ | `Create(w, h) → uint32` | Crée le FBO ; handle > 0 si OK, 0 sinon. |
| Champ | `Destroy(handle)` | Libère le FBO. |
| Champ | `Bind(handle)` | Redirige les draws vers le offscreen. |
| Champ | `Unbind()` | Restaure le framebuffer principal. |
| Champ | `GetColorTextureGPUId(handle) → uint32` | Id samplable par `NkTexture`. |
| Libre | `NkRenderTextureSetBackend(backend)` | Enregistre la table active. |
| Libre | `NkRenderTextureInstallUnsupportedBackend(name)` | Stub (Create → 0). |

### `NkUICanvasBackend` — pont NKUI → NKCanvas

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkUICanvasBackend()` | Constructeur par défaut. |
| Construction | copie `= delete` | Non-copiable. |
| Destruction | `~NkUICanvasBackend()` | Appelle `Destroy()` (inline). |
| Cycle de vie | `Init(renderer)` | Lie le `NkIRenderer2D` actif. À appeler une fois. |
| Cycle de vie | `Destroy()` | Libère tout (dont les textures possédées). |
| Rendu | `Submit(ctx, fbW, fbH)` | Rend les couches NKUI, **entre `Begin`/`End`**. |
| Textures | `UploadTextureRGBA8(texId, data, w, h)` | Upload RGBA8 (`w*h*4`). |
| Textures | `UploadTextureGray8(texId, data, w, h)` | Upload Gray8 (`w*h`, atlas police). |
| Textures | `HasTexture(texId)` | La texture est-elle connue ? |

---

## Référence complète

Chaque élément est repris ici à fond, avec ses usages dans les différents domaines du temps réel —
rendu, ECS, gameplay/IA, UI, outils/éditeur, IO.

### La hiérarchie et le piège du *name-hiding*

Tout part de `NkRenderTarget`, abstraite, dont dérivent `NkRenderWindow` (back buffer fenêtre) et
`NkRenderTexture` (FBO offscreen). On programme **contre la base** : une fonction qui prend
`NkRenderTarget&` accepte indifféremment l'écran ou une texture — c'est le polymorphisme qui rend le
code de dessin réutilisable.

Un piège C++ guette toute la famille : dès qu'une dérivée **override** une seule surcharge de `Draw`
(en l'occurrence `Draw(vertices, count, primitive, states)`), elle **masque** toutes les autres
`Draw` héritées (`Draw(drawable)`, `Draw(vertexArray)`…). La parade est systématique :
`using NkRenderTarget::Draw;` réintroduit les surcharges masquées. Les **deux** dérivées le font ;
si vous écrivez une nouvelle cible, ne l'oubliez pas, sous peine de voir disparaître `target.Draw(monSprite)`.

### `NkRenderTarget` — le rituel de frame

Le contrat de frame est invariant : **`Clear` puis des `Draw` puis `Display`**. `Clear(color)` est la
remise à zéro — un fond, un ciel, un noir d'éditeur. `Display()` est la présentation, qu'on **oublie
trop facilement** : sans elle, on a tout dessiné… dans le vide.

Le système de `Draw` mérite qu'on en saisisse l'architecture, car elle structure tout le module :

- **`Draw(const NkDrawable&, states)`** — la voie noble. Non-virtuelle, elle se borne à appeler
  `drawable.Draw(*this, states)` : c'est l'objet dessinable (un sprite, une forme, un texte) qui
  *sait* se décomposer en vertices et rappelle ensuite `target.Draw(vertices…)`. Inversion de
  contrôle propre, façon SFML.
- **`Draw(const NkVertexArray&, states)`** — le raccourci : on tient déjà un tableau de sommets prêt,
  on le soumet directement.
- **`Draw(const NkVertex*, count, primitive, states)`** — la **seule virtuelle pure** de dessin, le
  goulot par lequel tout passe vers le backend. C'est là qu'on choisit la primitive (triangles,
  lignes…) et qu'on applique l'état (blend, texture, transform). Tout ce qui précède finit ici.
- **`Draw(const NkIDrawable2D&)`** — vestige de l'ancienne interface, gardé le temps de migrer
  `NkSprite`/`NkText` ; à ignorer dans tout code neuf.

Côté caméra, `SetView`/`GetView` pilotent le `NkView2D` — la portion du monde visible, son zoom, son
décalage. C'est ce qui réalise un **scroll** de niveau, un **zoom** d'éditeur, un **suivi** de
personnage. `GetDefaultView()` redonne la vue neutre (utile pour dessiner l'UI *par-dessus* une scène
qui, elle, a sa propre view). Le **viewport** sert à découper la cible : deux viewports moitié-écran =
**split-screen**, un petit viewport dans un coin = **mini-carte**, des viewports juxtaposés = panneaux
d'éditeur.

Les deux mappings sont l'outil quotidien du **gameplay** et des **outils** : `MapPixelToCoords`
convertit un clic souris (pixels) en position monde — *quelle tuile, quelle entité ai-je cliquée ?* —
et `MapCoordsToPixel` fait l'inverse pour, par exemple, ancrer un label d'UI au-dessus d'une entité du
monde. Sans eux, dès qu'une view zoome ou défile, le pointeur et le monde ne sont plus alignés.

Enfin, deux niveaux d'accès au moteur : `GetRenderer()` pour le `NkIRenderer2D*` bas niveau (quand on
a besoin de fonctionnalités fines), `GetRenderer2D()` pour la façade `NkRenderer2D&` agréable, qui
**redirige correctement vers la cible courante** — c'est elle qu'on utilise pour les dessins
immédiats de confort.

### `NkRenderWindow` — la cible écran et sa propriété

C'est la cible de tous les jours. Au-delà du rituel hérité, deux thèmes lui sont propres : la
**propriété du contexte** et la **recréation de swapchain**.

La **propriété** se décide au constructeur. Avec `(window, desc)`, NKCanvas crée et possède le
contexte GPU (`mOwnsContext = true`) et l'ordre de destruction est garanti (renderer puis contexte).
Avec `(window, externalContext)`, vous gardez la main : vous fournissez un contexte déjà
`Initialize()`, NKCanvas pose juste un renderer dessus, et **c'est à vous** de détruire le contexte —
mais **après** le `NkRenderWindow`, jamais avant. Ce second mode sert quand plusieurs cibles partagent
un contexte, ou quand un sous-système GPU possède déjà le sien. Domaines concernés :

- **Outils / éditeur** — un contexte partagé entre la fenêtre principale et plusieurs panneaux ;
  variante `externalContext` pour ne pas multiplier les contextes.
- **Rendu / gameplay** — le cas standard reste le ctor `desc`, simple et sûr.
- **IO / cycle de vie** — `IsValid()` à vérifier juste après construction, avant la première frame.

La **recréation de swapchain** est le point où les bugs DPI se nichent. `OnResize(w, h)` veut des
**pixels physiques** (après application du facteur DPI) ; on l'appelle dans le handler de
`NkWindowResizeEvent`. `OnDpiChange()`, branché sur `NkWindowDpiEvent`, relit la taille via la fenêtre
et retombe sur `OnResize`. La discipline : **une seule routine de recréation**, atteinte par les deux
chemins. Un testeur en 150 % d'échelle Windows révèle immédiatement un câblage incomplet.

`GetContext()` et `GetWindow()` ouvrent l'accès direct quand on a besoin de descendre d'un cran (lire
l'état GPU, interroger la fenêtre native). À manier avec parcimonie : la plupart du code applicatif n'a
besoin que des cinq verbes de `NkRenderTarget`.

### `NkRenderTexture` — rendre dans une texture

Le offscreen débloque toute une famille d'effets qui supposent de **rendre une première passe, puis la
réutiliser** :

- **Rendu / post-traitement** — dessiner la scène dans la texture, puis la ré-afficher plein écran à
  travers un shader (flou, bloom, colorimétrie, vignette).
- **UI / 2D** — une **mini-carte** : rendre le niveau vu de haut dans une petite texture, puis la
  coller dans un coin de l'HUD. Ou une **vignette d'aperçu** d'asset.
- **Gameplay** — un **portail**, un **miroir**, un écran de caméra de surveillance : la vue d'ailleurs
  est rendue offscreen, puis plaquée sur une surface du monde.
- **Outils / éditeur** — les **thumbnails** d'un navigateur d'assets, l'aperçu d'un matériau dans
  l'inspecteur : on rend l'objet isolé dans une texture qu'on affiche dans le panneau.

Le contrat est `Create` → utiliser comme cible → `GetColorTextureGPUId()` pour récupérer le résultat
samplable → `Destroy`. `Create(renderer, w, h)` **exige un renderer déjà initialisé** : c'est lui qui,
à son `Initialize`, a installé le `NkRenderTextureBackend` ; sans ça, pas de backend, et `Create`
renvoie `false`. Toujours **vérifier ce retour** (sur les backends encore stub, il sera `false`).
`Destroy()` est **idempotent** — on peut l'appeler sans risque, le destructeur le fait de toute façon.

Point de propriété crucial : le renderer est **non-possédé** (`mRenderer` réfère le renderer
principal). La `NkRenderTexture` *emprunte* le moteur de la cible écran ; elle ne le crée ni ne le
détruit. C'est cohérent : on a un seul renderer GPU, et plusieurs cibles qui s'en servent.

### `NkRenderTextureBackend` — la plomberie cross-API

Cette structure est le **trait d'union** entre `NkRenderTexture` (qui ignore tout du GPU) et chaque
backend graphique (qui en connaît tous les détails). Cinq fonctions, et une convention stricte :

- **`Create(w, h)`** renvoie un handle `uint32`, **strictement positif** en cas de succès, **0** en
  cas d'échec — ce 0 est ce qui fait remonter `false` jusqu'à `NkRenderTexture::Create`. Le handle est
  *opaque* : FBO côté GL, index de `VkFramebuffer`, slot RTV côté DX — le code commun n'en sait rien.
- **`Bind(handle)` / `Unbind()`** vont **par paires** : `Bind` détourne les draws vers le offscreen
  *après avoir sauvegardé* le framebuffer courant, `Unbind` restaure ce dernier. Oublier le `Unbind`,
  c'est continuer à dessiner dans la texture alors qu'on croyait revenir à l'écran.
- **`Destroy(handle)`** libère les ressources GPU du FBO.
- **`GetColorTextureGPUId(handle)`** rend l'identifiant que `NkTexture` peut *sampler* directement —
  **sans re-upload**, puisque la texture vit déjà sur le GPU.

L'installation passe par deux fonctions libres. `NkRenderTextureSetBackend(backend)` est appelée par
**chaque backend renderer** en fin d'`Initialize` : c'est le moment où la table devient active. Pour un
backend sans support FBO, `NkRenderTextureInstallUnsupportedBackend(name)` pose un stub dont le
`Create` renvoie 0 — l'application reste **robuste** (un `Create` qui échoue proprement plutôt qu'un
appel à `nullptr`). C'est de la plomberie : du code applicatif n'appelle jamais ces fonctions, seuls
les implémenteurs de backend les touchent.

### `NkUICanvasBackend` — rendre une interface

Ce pont relie deux mondes : l'**immediate-mode** de NKUI (qui empile des draw-lists chaque frame) et
le **renderer 2D** de NKCanvas (qui sait dessiner des triangles texturés avec clip). Il est le pendant
de `NkUINKRHIBackend`, en plus léger : là où le backend NKRHI gère lui-même pipeline et buffers, ici
`NkRenderer2D` s'en charge déjà.

- **UI / 2D** — l'usage central : afficher menus, HUD, inspecteur, console — toute interface NKUI —
  dans une `NkRenderWindow` ou une `NkRenderTexture`. `Submit(ctx, fbW, fbH)` parcourt les couches du
  contexte et les rend ; il **doit s'intercaler entre `Begin()` et `End()`** de la cible, et reçoit la
  taille du framebuffer pour calculer correctement les **clips** (régions de découpe des panneaux).
- **Outils / éditeur** — exactement le canal d'une interface d'éditeur posée sur le viewport de rendu.
- **IO / ressources** — les uploads de textures. `UploadTextureRGBA8` (données `w*h*4`) sert les
  **images** de l'UI ; `UploadTextureGray8` (données `w*h`, étendues en RGBA blanc + alpha) sert
  l'**atlas de police** — c'est ainsi que le texte NKUI arrive sur le GPU. `HasTexture(texId)` évite un
  ré-upload inutile.

Deux propriétés structurent son emploi. D'abord le **découplage de compilation** : le header ne
*connaît* pas NKUI (il forward-declare `NkUIContext`), si bien qu'un module peut le référencer sans
lier NKUI ; et le `.cpp` n'est compilé que sous `NK_CANVAS_WITH_NKUI=1` — l'UI est une **option**, pas
une dépendance forcée. Ensuite l'**ownership** : le pont **possède** les `NkTexture*` de sa table
interne (clé `texId` → texture), libérées par `Destroy()`. On ne détruit donc pas ces textures
soi-même ; on appelle `Destroy()` et on laisse le pont nettoyer.

Pièges récurrents : un `texId` à **0** est invalide (réservé) ; un `Submit` **hors** d'une paire
`Begin`/`End` ne produit rien ; et `Init` doit recevoir un `NkIRenderer2D` **déjà actif** (typiquement
`renderWindow.GetRenderer()`).

### Les idiomes transverses

- **Programmer contre `NkRenderTarget`** — vos fonctions de dessin prennent `NkRenderTarget&`, jamais
  une cible concrète : le même code rend à l'écran et offscreen.
- **`using NkRenderTarget::Draw;`** — obligatoire dans toute dérivée qui override `Draw`, sinon les
  autres surcharges disparaissent.
- **Ownership explicite** — `NkRenderWindow` possède son contexte *selon le ctor* (et toujours son
  renderer) ; `NkRenderTexture` ne possède **pas** son renderer ; `NkUICanvasBackend` possède ses
  textures. Connaître qui possède quoi, c'est éviter les double-free.
- **Create/Destroy** — `NkRenderTexture` (`Destroy` idempotent) et `NkUICanvasBackend`
  (`Init`/`Destroy`) respectent la règle dure NKMemory : toute classe avec `Create` a son `Destroy`.
- **Non-copiables** — les trois cibles à état GPU (`NkRenderWindow`, `NkRenderTexture`,
  `NkUICanvasBackend`) interdisent la copie ; on les passe par référence.
- **Dispatch par table** — `NkRenderTextureBackend` (5 fn-ptrs, enregistrée par chaque renderer) est
  le même patron que les autres backends GPU du module ; stub propre via
  `NkRenderTextureInstallUnsupportedBackend`.

---

### Exemple

```cpp
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCanvas/Renderer/Targets/NkRenderTexture.h"
#include "NKCanvas/UI/NkUICanvasBackend.h"
using namespace nkentseu::renderer;

// 1) Cible écran : NKCanvas possède le contexte (chemin normal).
NkRenderWindow window(nativeWindow, contextDesc);
if (!window.IsValid()) return;

// Brancher resize + DPI sur la MÊME routine de recréation.
// onWindowResize  -> window.OnResize(physW, physH);  (pixels physiques)
// onWindowDpi     -> window.OnDpiChange();

// 2) Cible offscreen : exige un renderer déjà initialisé, peut échouer.
NkRenderTexture rt;
if (rt.Create(*window.GetRenderer(), 256, 256)) {
    rt.Clear(NkColor2D::Black);
    // ... dessiner la mini-carte dans rt ...
    rt.Display();
    uint32 gpuId = rt.GetColorTextureGPUId();   // samplable comme une texture
    // -> à coller dans l'HUD via NkTexture::SetGPUId(gpuId)
}

// 3) Interface NKUI rendue à travers le renderer 2D de NKCanvas.
NkUICanvasBackend ui;
ui.Init(window.GetRenderer());                  // une seule fois

// Rituel de frame :
window.Clear(NkColor2D::Black);
// ... dessiner la scène (window.Draw(...)) ...
// window.Begin(); ui.Submit(uiContext, fbW, fbH); window.End();  // Submit ENTRE Begin/End
window.Display();

rt.Destroy();    // idempotent
ui.Destroy();    // libère les textures possédées
```

---

[← Index NKCanvas](README.md) · [Récap NKCanvas](../NKCanvas.md) · [Couche Runtime](../README.md)
