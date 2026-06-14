# NKCanvas — Heap corruption c0000374 au shutdown (mismatch d'allocateur du renderer 2D)

- **Catégorie** : NKCanvas (gestion mémoire / cycle de vie)
- **Sévérité** : critique (crash systématique à la fermeture, tous backends)
- **Date** : 2026-06-04
- **Statut** : **résolu**

## Symptôme

À la **fermeture** de Pong (après migration sur NKCanvas), crash
`0xC0000374` (*STATUS_HEAP_CORRUPTION*) — **sur les 3 backends testés**
(OpenGL, DX11, DX12). L'app tourne parfaitement, l'image s'affiche, puis **au
shutdown** elle plante. En `gdb` :

```
Thread 1 received signal SIGSEGV
#9  nkentseu::memory::NkFreeAligned (ptr=0x...)            NkPlatform.cpp:1090
#10 (anonymous namespace)::FreeAlignedByMalloc (ptr=0x...) NkAllocator.cpp:197
#11 nkentseu::memory::NkMallocAllocator::Deallocate        NkAllocator.cpp:335
#12 nkentseu::memory::NkAllocator::Delete<NkIRenderer2D>   NkAllocator.h:759
#13 NkDefaultDelete<NkIRenderer2D>::operator()             NkUniquePtr.h:67
#14 NkUniquePtr<NkIRenderer2D>::Reset                      NkUniquePtr.h:234
#15 NkRenderWindow::~NkRenderWindow                        NkRenderWindow.cpp:116
#17 PongApp::Shutdown                                      PongApp.cpp:159
```

Le crash est **backend-agnostique** et toujours au **même endroit** : la
libération du `NkIRenderer2D` détenu par le `NkRenderWindow`.

## Contexte

- Introduit pendant la migration **Pong → NKCanvas** (juin 2026).
- `NkRenderWindow` possède le renderer 2D via
  `memory::NkUniquePtr<NkIRenderer2D> mRenderer`, créé par
  `NkRenderer2DFactory::CreateUnique(ctx)`.

## Pistes explorées (et écartées)

Important de documenter les fausses pistes — un `c0000374` ne pointe PAS le
coupable, il explose juste au prochain accès au tas.

1. ❌ **Overrun du buffer de l'atlas de police / des textures** : écarté — le
   Page Heap était *clean* au chargement (aucune faute pendant le run).
2. ❌ **Double-free de `NkTexture`** (157 frames de l'intro Rihen) : les
   `DeleteGLTexture` au changement de scène étaient tous uniques et propres.
3. ❌ **`NkTexture` move qui ne transfère pas `mCPUPixels`** : vrai bug (data-loss)
   mais **pas** le corrupteur (corrigé séparément, cf. plus bas).
4. ✅ **Page Heap (Full)** a tranché : le crash reste **exactement au même free**
   (le renderer), confirmant un free *invalide* et non un overrun antérieur.

### La technique qui a résolu : Full Page Heap

`gflags` n'étant pas toujours présent, on active le **Full Page Heap** via le
registre (PowerShell **admin**), ce qui fait crasher *au moment exact* du free
fautif :

```powershell
$k = "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\Pong.exe"
New-Item -Path $k -Force | Out-Null
New-ItemProperty -Path $k -Name GlobalFlag    -Value 0x02000000 -PropertyType DWord -Force | Out-Null  # hpa
New-ItemProperty -Path $k -Name PageHeapFlags -Value 0x3        -PropertyType DWord -Force | Out-Null
```
Puis `gdb --batch -ex run -ex "bt full" --args Pong.exe`. **Désactiver** ensuite
(sinon l'app est très lente / gloutonne en RAM) en **supprimant** ces valeurs.

## Cause racine

**Mismatch d'allocateur** entre l'allocation et la libération du renderer.

- `NkRenderer2DFactory::Create()` allouait le renderer avec **`new`** :
  ```cpp
  r2d = new NkOpenGLRenderer2D();   // operator new == malloc
  ```
- mais `CreateUnique()` l'enveloppe dans un `NkUniquePtr<NkIRenderer2D>` dont le
  deleter (`NkDefaultDelete`) libère via **l'allocateur NKMemory** →
  `NkMallocAllocator::Deallocate` → `NkFreeAligned` → **`_aligned_free`**.

`malloc` et `_aligned_malloc` utilisent une **comptabilité de tas différente**
(l'aligné stocke un pointeur de base décalé avant le bloc). Libérer un pointeur
`malloc` avec `_aligned_free` lit un « base pointer » bidon → **corruption du
tas → `c0000374`**.

C'est silencieux jusqu'au shutdown car le bloc fautif n'est libéré qu'à la
destruction du `NkRenderWindow`.

## Solution

**Allouer ET libérer via le même allocateur NKMemory.** Dans
`NkRenderer2DFactory::Create()` :

```cpp
auto& alloc = ::nkentseu::memory::NkGetDefaultAllocator();   // celui qu'utilise NkUniquePtr
...
r2d = alloc.New<NkOpenGLRenderer2D>();   // au lieu de `new`
...
if (!r2d->Initialize(ctx)) { alloc.Delete(r2d); return nullptr; }   // au lieu de `delete`
```

`alloc.New<T>()` (= `_aligned_malloc` + placement new) correspond exactement à
`NkDefaultDelete` → `alloc.Delete<T>()` (= dtor + `_aligned_free`). Plus aucun
mismatch.

Fichier : `Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Core/NkRenderer2DFactory.cpp`.

## Vérification

Reconstruire, **Page Heap actif**, lancer sous `gdb`, jouer, fermer :

```
[Inferior 1 (process ...) exited normally]
```
→ **plus aucun `SIGSEGV` / `c0000374`** au shutdown. Confirmé sur OpenGL.

## Notes / pièges (règle générale)

- **NE JAMAIS mélanger** `new`/`delete` (CRT/malloc) avec les allocateurs
  NKMemory (`NkAlloc`/`NkFree`/`_aligned_*`). Un objet alloué par un allocateur
  doit être libéré par **le même**. Vaut aussi pour les buffers retournés par les
  codecs (`NkXxxCodec::Encode` → `NkFree`, jamais `delete[]`/`std::free`).
- Pour tout objet destiné à un `NkUniquePtr`, préférer
  `memory::NkMakeUnique<T>()` (alloc + deleter cohérents par construction) plutôt
  qu'un `new` suivi d'un wrap manuel.
- Un `c0000374` indique l'endroit du **prochain** accès au tas, pas le coupable.
  Le **Full Page Heap** est l'outil décisif pour localiser le free/écriture exact.

## Liens

- `Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Core/NkRenderer2DFactory.cpp` (Create / CreateUnique)
- `Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Targets/NkRenderWindow.cpp:116` (~NkRenderWindow → mRenderer.Reset())
- `Kernel/Foundation/NKMemory/src/NKMemory/NkUniquePtr.h` (NkDefaultDelete, NkMakeUnique, NkGetDefaultAllocator)
- Bug connexe : [nktexture-move-mcpupixels.md](nktexture-move-mcpupixels.md)
