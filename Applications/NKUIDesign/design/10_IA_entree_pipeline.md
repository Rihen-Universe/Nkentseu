# Document 10 — L'IA de design entre dans NkUIDesign : l'état du pipeline

> Ouvert le **2026-08-30**, sur la demande de Rodolf : *« commencer, si possible,
> à faire que l'IA de design soit prête à entrer dans NkUIDesign. »*
> Chantier tenu à côté du chantier toile, territoires disjoints : ce document,
> `DesignAI.h`, `DesignAIRecette.h`, `ia_pont.py`. Rien d'autre n'a été touché.

---

## 1. Le pipeline, et ce qui en est prouvé

```
description en français
        │
        ▼
backend (FICHIER d'abord — déterministe, sans clé ni réseau)
        │   nkuidesign_prompt.txt écrit → nkuidesign_reponse.txt relu
        ▼
VALIDATION — refus MOTIVÉ (6 verdicts), document intact octet pour octet
        │   structure lisible · composants du registre (`CanGraft`) · REJEU
        ▼
INSERTION par `GraftFrom` — la MÊME porte que la main, nœuds normaux du modèle
        │
        ▼
badge de provenance en métadonnée (`author = ia`, `verified` après rejeu)
        │   retiré à l'affichage dès la première édition manuelle (`corrected`)
        ▼
ANNULABLE d'un seul geste (`Retract` — le sous-arbre entier, deux gardes)
```

**La preuve de recette existe et passe : `RunRecetteIA()` — 18/18** (mesuré le
30/08, exécutable autonome lié contre les bibliothèques du dépôt). La requête
de référence est celle convenue — *« un écran de connexion : un titre, deux
champs avec libellés, un bouton »* — par le backend fichier avec de **vrais
fichiers**, et chaque maillon est un essai : prompt écrit sur disque, réponse
relue, posée, visible par introspection de l'arbre (labels + enfants), mise en
page complète (9/9 rectangles), badge sur la greffe et rien d'autre, provenance
qui survit à l'aller-retour, retrait ENTIER (document identique octet pour
octet), retrait rejoué refusé, édition manuelle qui éteint le badge, et
l'aperçu (`Propose`/`Commit`/`Discard`) qui ne touche au document qu'au Commit.

La sonde existante reste verte après ces changements : **133/133**.

## 2. Ce qui a été ajouté le 30/08 (et ce qui ne l'a pas été)

### Ajouté — dans `DesignAI.h`

- **`Propose` / `CommitProposal` / `DiscardProposal`** — la garantie 1 de la
  spec §6.2 (« rien n'est écrit tant que l'utilisateur n'a pas validé »),
  tenue par construction : la proposition validée et rejouée vit DE CÔTÉ,
  le document n'est touché qu'au Commit. `Ask` (demander = poser) reste
  intact — la sonde et le panneau actuels en dépendent.
- **`Retract`** — l'annulation d'un geste. Il n'existe aujourd'hui **aucun
  historique de document** dans l'application (mesuré : zéro occurrence
  d'Undo/Historique dans Document.h, Panels.h, Canvas.h, main.cpp) ; le
  retrait passe par `RemoveSubtree` avec deux gardes : le nœud visé est bien
  d'origine IA, et le sous-arbre a exactement la taille posée. Un index
  périmé ou un sous-arbre modifié structurellement → refus, document intact.
- La validation est factorisée (`ValidateReply`) : l'aperçu et la pose jugent
  une réponse par le même code — pas deux vérités.

### Ajouté — nouveaux fichiers

- **`DesignAIRecette.h`** — la preuve de recette ci-dessus, à brancher sur
  `--recette-ia` (point d'intégration en §4).
- **`ia_pont.py`** — le backend modèle externe par la prise FICHIER : lit le
  prompt, appelle `ollama` ou `claude` en ligne de commande, écrit la réponse.
  Aucun code réseau, ni ici ni dans l'application.

### PAS ajouté, et pourquoi

- **Pas de backend réseau C++.** PV3DE a des backends Claude/Ollama qui
  marchent, sur des sockets écrites à la main que leur propre en-tête demande
  de remplacer par un client HTTP partagé. Les recopier serait une troisième
  copie dans une application — le manque est au canal, la remontée en module
  est un chantier de kit.
- **Pas Ilyana.** Décision actée (CAPTURE_MONTAGE §4) : sa carte est prise
  par l'entraînement, et coupler une capacité neuve à un modèle en formation
  les fait échouer ensemble. Le jour venu, elle se branche sur la même prise
  fichier — le pipeline ne change pas.
- **Pas de validation `NkGuiValidate` au fil de l'insertion.** Le modèle du
  document est le format d'assemblage `nkuidoc` (dont les clés visent
  `.nkgui` v0.2) ; `NkGuiValidate` valide des archives `.nkgui`, un autre
  étage (export/roundtrip). La porte d'insertion a sa validation équivalente
  et motivée. Brancher `NkGuiValidate` ici exigerait un convertisseur
  nkuidoc→nkgui qui n'existe pas — le créer serait un format parallèle de
  plus, pas un de moins.

## 3. Deux découvertes de mesure, à ne pas perdre

1. **Le vocabulaire du format est anglais** (`column`, `fixed`, `expand`) et
   **le lecteur ignore en silence une valeur inconnue** : une réponse écrite
   avec « colonne »/« fixe » se pose quand même, rejeu compris, mais
   l'agencement reste `none` et les enfants n'ont aucun rectangle. L'essai 2b
   de la recette est le témoin de ce piège. À noter : `ProbeValidReply()`
   dans `Probe.h` écrit `agencement = ligne` — jamais analysé, même défaut
   silencieux (signalé au canal, `Probe.h` n'est pas mon territoire).
2. **Le registre vivant n'a aucun composant de widget** — seulement
   `content_browser` et `tree_view`. L'écran de connexion est donc exprimé en
   cadres libellés : la recette prouve le pipeline et la structure, pas une
   palette. Le jour où `bouton`/`champ`/`libellé` seront déclarés, seule la
   réponse de référence change.

## 4. Points d'intégration (hors territoire, donnés au canal — Q31)

1. `main.cpp` : brancher `--recette-ia` sur `RunRecetteIA()` à côté de
   `--probe`, plus la ligne d'aide.
2. `Panels.h:853` : afficher le badge « [ia] » seulement si
   `author == IA && !corrected` — la donnée de provenance reste entière pour
   le corpus, l'affichage suit la spec §6.2.
3. `AIPanel` : passer de `Ask` à `Propose` + « Appliquer » / « Rejeter », et
   « Retirer » après un Commit — quand le chantier toile aura rendu la main.

## 5. Rejouer les preuves

```
# la recette (une fois --recette-ia branché dans main.cpp) :
NKUIDesign.exe --recette-ia        # rapport : nkuidesign_recette_ia.txt

# le pont modèle externe, à la main :
#   1. dans NkUIDesign : « Demander à l'IA »  (écrit nkuidesign_prompt.txt)
python ia_pont.py --backend ollama --modele qwen2.5:7b-instruct
#   2. recliquer : l'application vérifie et pose — ou refuse en disant pourquoi
```
