# NKCode ↔ Toolchain Apple (zig) — détection & intégration UI

> **Public : l'agent/dev qui travaille sur NKCode.**
> Ce document explique **comment NKCode doit détecter et exposer** la compilation
> croisée Apple (iOS / tvOS / watchOS / macOS) réalisée **depuis Windows** via
> **zig**, et comment lancer un build depuis l'interface. Tout le mécanisme
> sous-jacent existe déjà dans Jenga ; NKCode n'a qu'à **détecter**, **afficher**
> et **invoquer**.

---

## 1. En une phrase

La cross-compilation Apple est **opt-in** dans Jenga : on ajoute le flag
`--ios-backend=zig` (ou `--macos-backend=zig`) à `jenga build`, et Jenga route vers
`IosZigBuilder` / `MacosZigBuilder`. **Les builders natifs (Xcode/clang) restent
intacts.** NKCode doit donc, dans son sélecteur de cible, proposer les cibles Apple
« zig », **activées seulement si la toolchain est détectée**, et passer le bon flag
+ les bonnes variables d'environnement au process de build.

> Important : `NKCode.jenga` **ne déclare aucun `usetoolchain` zig** — c'est **voulu**.
> Le choix zig se fait **au moment du build** (flag CLI), pas dans le `.jenga`. NKCode
> ne modifie donc pas le `.jenga` ; il ajoute le flag à la commande.

---

## 2. Ce qu'il faut détecter (5 composants)

| # | Composant | Rôle | Où (défaut) | Env override |
|---|-----------|------|-------------|--------------|
| 1 | **zig 0.13.0** | compilateur (macOS **et** iOS) | `C:\apple-sdks\zigldl\zig-windows-x86_64-0.13.0\zig.exe` | `ZIG_MACOS` (aussi valable iOS) |
| 2 | **SDK iOS complet** | headers + `.tbd` de link | `C:\apple-sdks\iPhoneOS12.2.sdk` | `IOS_SDK` |
| 3 | **ld.lld (Mach-O)** | linkeur iOS (zig ne linke pas `.ios`) | NDK : `…\toolchains\llvm\prebuilt\windows-x86_64\bin\ld.lld.exe` | `LD64` |
| 4 | **libc++ iOS retaguée** | ABI libc++ iOS (fix A3) | `C:\apple-sdks\libcxx-ios\libc++.a` | `IOS_LIBCPP_DIR` |
| 5 | **rcodesign** *(optionnel)* | signature `.app`/`.ipa` | `C:\apple-sdks\tools\apple-codesign-*\rcodesign.exe` | `RCODESIGN` |

- **macOS** n'a besoin que de #1 + un SDK macOS (`MACOS_SDK`, défaut `C:\apple-sdks\MacOSX11.3.sdk`) — il se **compile ET linke** avec le driver zig (pas de ld.lld).
- **iOS/tvOS/watchOS** ont besoin de #1..#4 (link via ld.lld). #5 seulement pour signer.

> ⚠️ Un SDK iOS **complet** est requis (présence de `usr/lib/libSystem.tbd`,
> `usr/lib/libc++.tbd`, `usr/lib/libobjc.tbd`). Les SDK « headers-only » **ne
> linkent pas** — la détection doit vérifier ces `.tbd`, pas juste le dossier.

---

## 3. Algorithme de détection (à implémenter dans NKCode)

```text
DetectAppleZig():
  zig      = env(ZIG_MACOS)  or  "C:\apple-sdks\zigldl\zig-windows-x86_64-0.13.0\zig.exe"
  ok_zig   = exists(zig) and run(zig, "version") starts_with "0.13"

  ios_sdk  = env(IOS_SDK)   or  first_glob("C:\apple-sdks\iPhoneOS*.sdk")
  ok_iosdk = exists(ios_sdk + "\usr\lib\libSystem.tbd")           # SDK COMPLET
  ld64     = env(LD64)      or  find_ndk_ldlld()                  # cf §4
  ok_ld    = exists(ld64)
  libcpp   = env(IOS_LIBCPP_DIR) or "C:\apple-sdks\libcxx-ios"
  ok_libcpp= exists(libcpp + "\libc++.a")

  mac_sdk  = env(MACOS_SDK) or "C:\apple-sdks\MacOSX11.3.sdk"
  ok_mac   = ok_zig and exists(mac_sdk + "\usr\lib\libSystem.tbd")

  rcodesign= env(RCODESIGN) or first_glob("C:\apple-sdks\tools\apple-codesign-*\rcodesign.exe")
  ok_sign  = exists(rcodesign)

  return {
    iOS_available   : ok_zig and ok_iosdk and ok_ld and ok_libcpp,
    macOS_available : ok_mac,
    signing_available: ok_sign,
    paths: { zig, ios_sdk, ld64, libcpp, mac_sdk, rcodesign }
  }
```

`find_ndk_ldlld()` : chercher `ld.lld.exe` sous
`%ANDROID_NDK_HOME%\toolchains\llvm\prebuilt\windows-x86_64\bin\`, sinon sous
`C:\Android\ndk\*\toolchains\llvm\prebuilt\windows-x86_64\bin\`.

**Recommandé (le plus simple)** : NKCode n'a **pas besoin de réimplémenter** la
détection — le script Jenga fournit un mode sonde **sans téléchargement** :

```bat
python Jenga/scripts/setup_apple_toolchain.py --check --json
```

- Émet un **JSON parsable** avec, pour chacun des 5 composants, `{path, ok}` + les
  booléens `ios_available`, `macos_available`, `signing_available`.
- **Code de sortie** : `0` si iOS **ou** macOS est prêt, `1` sinon.
- `--check` seul (sans `--json`) : résumé lisible. `--compile-check` : ajoute un vrai
  compile+link (preuve, plus lent).

Exemple de sortie JSON (extrait) :
```json
{ "zig": {"path": "C:\\apple-sdks\\...\\zig.exe", "ok": true, "version": "0.13.0"},
  "ios_sdk": {"present": true, "complete": true, "ok": true},
  "ld64": {"path": "C:\\Android\\ndk\\...\\ld.lld.exe", "ok": true},
  "ios_available": true, "macos_available": true, "signing_available": true }
```

NKCode appelle ce script au démarrage (ou sur « Re-détecter »), parse le JSON, et
active/grise les cibles Apple. Le pseudo-code ci-dessous reste utile si NKCode veut
détecter **sans** dépendre du script.

---

## 4. Surfaçage dans l'interface

1. **Sélecteur de plateforme/cible** : ajouter les entrées `iOS`, `tvOS`,
   `watchOS`, `macOS` avec un badge **« zig cross »**.
   - **Activées** si `DetectAppleZig()` renvoie disponible pour cette famille.
   - **Grisées** sinon, avec une action **« Installer la toolchain Apple »** qui
     lance `setup_apple_toolchain.py` (afficher la sortie dans le panneau Output).
2. **Panneau Réglages → Apple** : afficher les 5 chemins détectés (✓/✗ chacun) et
   un bouton **« Re-détecter »** + **« Installer/Réparer »**.
3. **Case « Signer (ad-hoc) »** dans la barre de build iOS, activée si
   `signing_available`. Optionnel : champ `.p12` + mot de passe + provisioning
   pour la signature device.

---

## 5. Invocation du build (ce que NKCode exécute)

NKCode a déjà l'intégration Jenga (build/run). Pour une cible Apple zig, il **ajoute
le flag** et **positionne les variables d'environnement** du process de build :

```bat
:: iOS (compile + link + bundle .app/.ipa, signé ad-hoc si IOS_SIGN=adhoc)
set ZIG_MACOS=C:\apple-sdks\zigldl\zig-windows-x86_64-0.13.0\zig.exe
set IOS_SDK=C:\apple-sdks\iPhoneOS12.2.sdk
set IOS_LIBCPP_DIR=C:\apple-sdks\libcxx-ios
set RCODESIGN=C:\apple-sdks\tools\apple-codesign-0.27.0-x86_64-pc-windows-msvc\rcodesign.exe
set IOS_SIGN=adhoc
set PATH=C:\apple-sdks\bin;%PATH%
jenga build --platform iOS --ios-backend=zig --target NKCode --config Debug
```

```bat
:: macOS (compile + link + .app, natif intact via clang si on N'ajoute PAS le flag)
set ZIG_MACOS=C:\apple-sdks\zigldl\zig-windows-x86_64-0.13.0\zig.exe
set MACOS_SDK=C:\apple-sdks\MacOSX11.3.sdk
jenga build --platform macOS --macos-backend=zig --target NKCode --config Debug
```

Variantes de flag : `--ios-backend=zig`, `--apple-backend=zig` (couvre iOS/tvOS/watchOS),
`--macos-backend=zig`. Sans flag → builders natifs Xcode/clang (inchangés).

### Signature (opt-in, gérée par le builder)
| Env | Effet |
|-----|-------|
| `IOS_SIGN=adhoc` | signature ad-hoc (dev/local, pas d'install device) |
| `IOS_SIGN=<cert.p12>` + `IOS_SIGN_PASS=…` | certificat Apple |
| `IOS_PROVISION=<profile.mobileprovision>` | profil pour install device |
| *(absent)* | `.app` non signé |

Sortie attendue en fin de build : `[ios-zig] signé (ad-hoc) : NKCode.app`, artefacts
sous `Build\Bin\<Config>-iOS\NKCode\NKCode.{app,ipa}`.

---

## 6. Détails que NKCode doit connaître

- **Un seul binaire zig** sert macOS **et** iOS (variable `ZIG_MACOS`, nom historique).
- La cible affiche `iOS x86_64` dans l'en-tête Jenga mais le `IosZigBuilder` **force
  arm64** au link (les objets `.o` sont bien arm64). Ne pas s'en inquiéter.
- **Exécution/test** = matériel Apple ou simulateur (Mac). Depuis Windows on
  **produit** l'`.ipa` ; l'installation sur iPhone se fait via **Sideloadly/AltStore**
  (signe avec l'Apple ID de l'utilisateur) ou `ideviceinstaller` (avec `.p12`+profil).
- `rcodesign verify` affiche un **warning CMS connu** (bug de l'outil) — ce **n'est
  pas** un échec. Vérifier avec `rcodesign print-signature-info <app>/<exe>`.
- Le dossier racine de toute la toolchain est **`C:\apple-sdks\`** (sauf `ld.lld`
  qui vient du **NDK Android**).

---

## 7. Références Jenga (pour aller plus loin)

- Wiki : `Jenga/Docs/wiki/Compilation-Apple-depuis-Windows.md` (procédure complète + signature).
- Installateur/vérif : `Jenga/scripts/setup_apple_toolchain.py`.
- Reproduction du fix ABI libc++ iOS : `Jenga/scripts/make_ios_libcxx.py`.
- Builders : `Jenga/Core/Builders/IosZig.py`, `MacosZig.py` ; routage : `Jenga/Commands/Build.py`.
- Toolchains enregistrées : `zig-ios-arm64`, `zig-tvos-arm64`, `zig-watchos-arm64`,
  `zig-macos-arm64/x86_64` (`Jenga/Core/Toolchains.py`).

---

**TL;DR pour NKCode** : détecter les 5 composants (§2/§3) → activer les cibles Apple
« zig » dans le sélecteur (§4) → au build, ajouter `--ios-backend=zig` + env (§5).
Rien à changer dans `NKCode.jenga`.
