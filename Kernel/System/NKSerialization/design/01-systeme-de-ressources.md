# NKSerialization — le système de ressources de la famille

### Document 01 — charger, lire et écrire une ressource, partout de la même façon

> Rihen, 01/10/2026 : *« soit on a un système de ressources défini, soit on doit en
> définir un, pour charger et lire des ressources, ou écrire »*.
>
> Réponse : **on en a la moitié, inspirée d'Unreal ; on la termine, on n'en écrit
> pas une autre.** Règle du dépôt : avant d'écrire un mécanisme, chercher qui le
> porte, en commençant par la couche du dessous.

## 1. Ce qui existe (vérifié le 01/10, `src/NKSerialization/Asset/`)

| brique | rôle | état |
|---|---|---|
| `NkAssetId` | identifiant unique de 128 bits par asset | ✅ |
| `NkAssetPath` | chemin logique `/Game/Meshes/Cube` | ✅ |
| `NkAssetType` | 20 natures, numéros jamais réutilisés (Scene 18, SaveGame 19…) | ✅ |
| `NkAssetMetadata`, `NkAssetDependency` | métadonnées et dépendances | ✅ |
| format `.nkasset` | en-tête 32 octets + métadonnées NkNative avec CRC + données brutes | ✅ |
| `NkAssetIO` | lire / écrire un `.nkasset` | ✅ |
| `NkAssetRegistry` | registre : parcourir des dossiers, retrouver un asset | ✅ |
| `NkAssetImporter`, `NkAssetCooker` | importer une source, cuire par plateforme (PC, console, mobile, Web) | ✅ |
| **`NkAssetManager.h`** | **le gestionnaire à l'exécution** | 🔴 **fichier vide (0 ligne)** |

Conséquence : chaque moteur charge à sa façon (NKRenderer a `NkTextureAsset` et
`NkMaterialLibrary`, Unkeny n'utilise les métadonnées que pour la livraison, Noge,
NKCraft et PV3DE ont leurs chemins propres).

## 2. Ce qu'il faut définir et écrire

1. **Un système de fichiers virtuel** : des **points de montage** (contenu du
   projet, contenu du moteur, paquets `.nkpak`) ; le **même chemin logique** marche
   dans l'éditeur (fichiers séparés) et dans le jeu livré (paquet, APK, données
   préchargées du Web) ; un montage peut être en lecture seule.
2. **Des chargeurs par type, enregistrés par leur module** : NKImage pour les
   textures, NKAudio pour les sons, NKFont pour les polices, Unkeny pour les scènes
   et les prefabs, NKAnima pour les clips et les contrôleurs, PV3DE pour les cas…
   Le gestionnaire ne dépend d'**aucun** de ces modules : ils s'inscrivent auprès
   de lui (même principe que la réflexion de NKECS).
3. **Le gestionnaire** (`NkAssetManager`) :
   - charger par identifiant ou par chemin, et rendre une **poignée typée** ;
   - **cache** et **comptage de références** ; libération quand plus personne ne
     tient la poignée ;
   - chargement **synchrone** et **asynchrone** (ordonnanceur existant), avec les
     **dépendances** chargées d'abord ;
   - **rechargement à chaud** dans l'éditeur (fichier modifié → ressource
     rechargée, poignées toujours valides) ;
   - **écriture** : enregistrer une ressource modifiée de façon **atomique**
     (fichier temporaire puis renommage), avec la **version de schéma**
     (`NkSchemaVersioning` existe) ; jamais d'écrasement silencieux ;
   - erreurs **dites** : fichier absent, corrompu (CRC), version trop récente,
     chargeur manquant — et une ressource de remplacement visible plutôt qu'un
     plantage.
4. **Un seul chemin pour tous** : Unkeny, Noge, les éditeurs (le Content Browser
   lit le registre), PV3DE, et les jeux (Le Dernier Feu) passent par lui. Les
   chemins actuels restent en place et migrent un par un, sans rien casser.

## 3. Méthode

- D'abord le gestionnaire et le système de fichiers virtuel avec deux chargeurs
  (texture, scène Unkeny), éprouvés par un banc : chargement, cache, références,
  asynchrone, rechargement à chaud, écriture atomique, fichier corrompu, paquet
  monté — chacun avec sa contre-épreuve.
- Puis la migration d'Unkeny (éditeur, joueur, livraison qui écrira les paquets),
  puis Noge, puis le reste.
