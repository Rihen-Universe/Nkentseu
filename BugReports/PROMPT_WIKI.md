# Prompt — Écrire le wiki Nkentseu (à coller dans Claude lancé DEPUIS ce dossier)

> Lancer `claude` depuis `D:\Projets\2026\Nkentseu\Nkentseu` (cwd = Nkentseu) → les sous-agents
> ont alors accès au repo et peuvent paralléliser. Puis coller le bloc ci-dessous.

---

Tu travailles dans le dépôt **Nkentseu** (`D:\Projets\2026\Nkentseu\Nkentseu`). Objectif :
**rédiger le wiki complet du moteur**, dans le dossier `wiki/`, **couche par couche**, en
**français**. Tu DOIS utiliser des **sous-agents (Task/Agent)** pour lire les headers et rédiger en
parallèle — ton cwd te donne accès au repo, donc la délégation marche.

## Structure imposée
```
wiki/
  README.md                      # index général (existe déjà)
  Foundation/
    README.md                    # overview de la couche (existe)
    <Module>.md                  # RÉCAP du module (intro narrative + « Par où commencer » + index headers)
    <Module>/                    # DOSSIER : 1 fichier détaillé PAR groupe logique de headers
      README.md                  # index des pages (tableau « ce qu'on y apprend »)
      <Partie>.md                # ex. Sequential.md, Associative.md, Strings.md…
```

## FORMAT VALIDÉ — 4 sections par fichier détaillé (NE PAS dévier)
1. **Tutoriel narratif (style SFML, https://www.sfml-dev.org/tutorials/3.1/)** : LA PROSE MÈNE. On
   part d'un **problème** (« où ranger une suite d'éléments ? »), on amène la solution, on explique
   le **pourquoi**, on clarifie par contrastes (« ce n'est PAS… »). Exemples SUIVIS d'explication.
   Un encadré `> **En résumé.**` par section.
2. **`## Aperçu de l'API`** : un (ou des) **TABLEAU** `| Catégorie | Élément | Rôle |` listant
   **TOUS** les éléments publics (types, construction, opérateurs, méthodes, fonctions, alias).
3. **`## Référence complète`** : un **VÉRITABLE COURS**. Chaque élément non trivial décrit À FOND —
   complexité/formule **+ cas d'usage dans PLUSIEURS DOMAINES** (rendu, ECS, physique/collision,
   animation, gameplay/IA, audio, UI/2D, IO, GPU…), en prose avec puces par domaine. PAS de tableaux
   ici (ils sont dans l'Aperçu). Trivial = bref ; important = à fond. Accessible aux novices.
4. **`### Exemple`** récapitulatif + liens de navigation en pied de page.

Pour CHAQUE module, soigner AUSSI : (a) le **récap `<Module>.md`** = intro + un guide
« **Par où commencer** » (tableau besoin → page) + aperçu des familles + index des headers (3
colonnes : header | contenu | documenté dans) ; (b) l'**index `<Module>/README.md`** = tableau des
pages avec « ce qu'on y apprend ».

## RÈGLE DURE — lire d'abord, ne RIEN inventer
Avant d'écrire un fichier, **lire EXHAUSTIVEMENT** : (1) les headers concernés sous
`Kernel/Foundation/<Module>/src/<Module>/…`, (2) le **`Readme.md`** du module, (3) les **fonctions
d'exemple in-header** (souvent en fin de fichier). Documenter UNIQUEMENT l'API réelle (la lecture
corrige aussi les noms). Le wiki est du markdown GitHub → **les accents sont OK** (français correct).

## GABARITS DE RÉFÉRENCE (lis-les AVANT d'écrire — reproduis EXACTEMENT ce niveau)
- `wiki/Foundation/NKMath/Vectors.md` (le `Dot` y a 5 puces de domaines)
- `wiki/Foundation/NKContainers/Sequential.md`
- Récap/README modèles : `wiki/Foundation/NKMath.md` + `wiki/Foundation/NKMath/README.md`

## UTILISER LES SOUS-AGENTS (obligatoire, pour aller vite)
Pour chaque fichier détaillé :
1. **Sous-agent LECTEUR** : « Lis EXHAUSTIVEMENT ces headers + le Readme du module + les exemples
   in-header. Renvoie un résumé structuré de TOUTE l'API publique : types/alias, signatures
   complètes, complexités, invariants, idiomes d'usage, namespace, header d'include. N'invente
   rien. »
2. **Sous-agent RÉDACTEUR** (ou toi) : à partir du résumé + du gabarit (`Vectors.md`/`Sequential.md`),
   écris le fichier au format 4 sections. Il ne doit utiliser QUE l'API extraite par le lecteur.
Tu peux **paralléliser plusieurs modules à la fois**. Si l'outil **Workflow** est dispo, orchestre
en pipeline (lecture → rédaction → vérification) ; sinon, des appels Agent successifs/parallèles.
**Vérification** : après rédaction, un sous-agent relit le header et confirme zéro API inventée.

## ÉTAT (déjà fait / à faire) — MAJ 2026-06-14
- ✅ **Foundation : 100 % TERMINÉ** au format final (vérifié) : **NKMath**, **NKContainers** (13
  fichiers + récap + README), **NKMemory** (8, refait), **NKCore** (9, refait), **NKPlatform** (5).
  NE PAS réécrire Foundation.
- ⏳ **System : à faire** (seul `wiki/System/README.md` existe). 8 modules sous `Kernel/System/` :
  **NKFileSystem, NKLogger, NKNetwork, NKReflection, NKSerialization, NKStream, NKThreading, NKTime**.
  Pour chacun : récap `wiki/System/<Module>.md` + dossier `wiki/System/<Module>/` (fichiers détaillés
  par groupe de headers) + `README.md`. Même méthode (sous-agent lecteur → rédacteur → vérif).
- ⏳ Ensuite **Runtime** (`Kernel/Runtime/*` : NKCanvas, NKUI, NKAudio, NKRHI, NKFont, NKImage,
  NKWindow, NKEvent, NKCamera, …) puis **Engine**. (Les couches `Kernel/AI` et `Kernel/Bare` ont
  déjà leurs propres ARCHITECTURE/README/ROADMAP — pas de wiki au format cours pour elles sauf
  demande.)
- Mettre à jour `wiki/README.md` (index général) au fil des couches ajoutées.

## Conventions & garde-fous
- Français, format 4 sections, gabarits respectés. Mets à jour la mémoire `project_nkentseu_wiki.md`
  après chaque module fini (avancement).
- **Git** : scripts en place (`gitcommit.sh`, `gitpush.sh dev "msg"`, `gitpr.sh`). Identité commits =
  LeTeguis, **zéro mention Claude**, ne **pas** pousser sans accord explicite de l'utilisateur.
- Ne touche pas au code moteur ni à l'index git (WIP utilisateur) ; tu n'écris que sous `wiki/`.

Foundation étant terminé, **commence par la couche System** : crée `wiki/System/NKTime.md` + le
dossier `wiki/System/NKTime/` (lis d'abord `Kernel/System/NKTime/src/**` + le Readme + les exemples
via un sous-agent lecteur), puis enchaîne NKStream, NKFileSystem, NKThreading, NKLogger,
NKSerialization, NKReflection, NKNetwork. Ensuite la couche Runtime. Fais un point après chaque module.
