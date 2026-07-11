# GitHub — « claude Claude » persistant dans l'encart Contributors (cache de la sidebar)

- **Catégorie** : GitHub / dépôt
- **Sévérité** : mineur techniquement, **important pour l'image** (perception investisseurs/partenaires)
- **Date** : 2026-07-11 (résolu ; problème traîné depuis la purge du 2026-07-09)
- **Statut** : **RÉSOLU** sur `Rihen-Universe/Nkentseu` **et** `Rihen-Universe/Jenga`

## Symptômes

- L'encart **« Contributors »** de la page d'accueil du dépôt GitHub affiche **« claude Claude »**
  en 3ᵉ contributeur, à côté de `LeTeguis` et `RihenUniverse`.
- Persiste **plus de 24 h** après avoir purgé l'historique, malgré des `Ctrl+F5` répétés.
- **Pourtant la donnée est propre** : `git log` (toutes branches) = **0** trailer
  `Co-Authored-By: Claude` et **0** commit dont l'auteur soit Claude ; l'API
  `repos/<org>/<repo>/contributors?anon=1` et `.../stats/contributors` ne renvoient **que**
  `LeTeguis` + `RihenUniverse` (**aucun Claude**).

## Diagnostic

1. **La donnée ≠ l'affichage.** GitHub calcule deux choses différentes :
   - Les **API** `contributors` / `stats/contributors` → comptent les **auteurs** des commits de la
     branche par défaut. Elles étaient déjà **propres**.
   - L'**encart HTML « Contributors »** de la page d'accueil → un **fragment mis en cache**, dérivé du
     graphe des contributeurs (auteurs **ET** co-auteurs `Co-Authored-By`). Ce fragment **traîne** derrière
     la donnée : il gardait l'entrée « Claude » créée par les anciens commits (d'avant la purge) avec le
     trailer `Co-Authored-By: Claude <noreply@anthropic.com>`.
2. **Pourquoi le cache ne se rafraîchissait pas** : le graphe des contributeurs et son fragment ont un
   **TTL long** côté GitHub et ne se recalculent pas à chaque `git push`. Rien dans la donnée n'était en
   cause — il fallait **forcer** une invalidation/recalcul.
3. **Ce qui n'était PAS la cause** : les ~35 vieilles **Pull Requests** gardent des refs immuables
   (`refs/pull/*`) avec les anciens commits Claude, mais elles **n'alimentent pas** l'encart de la page
   d'accueil (qui se calcule sur la branche par défaut). Supprimer/recréer le dépôt aurait marché mais
   aurait **perdu stars/forks/issues/PRs** — écarté.

## Cause racine

Le trailer `Co-Authored-By: Claude` (ajouté automatiquement par Claude Code) s'était retrouvé dans
l'historique. Une fois **purgé** de la branche par défaut, seule restait une **entrée en cache** dans le
fragment « Contributors » de GitHub, sans mécanisme de rafraîchissement automatique rapide.

## Résolution (non destructive — préserve stars/forks/PRs)

1. **Purge à la source** (faite le 2026-07-09) : réécriture de l'historique via `git filter-repo` pour
   retirer tous les trailers `Co-Authored-By: Claude` + désactivation **globale** de l'attribution dans
   `~/.claude/settings.json` (`includeCoAuthoredBy: false`, `attribution.commit/pr: ""`) → **plus aucun**
   nouveau commit/PR ne réintroduit Claude, sur toutes les sessions du profil.
2. **Forcer le recalcul du graphe** (le déblocage réel) :
   - Appeler `GET repos/<org>/<repo>/stats/contributors` (réponse `202 Accepted` = GitHub recalcule en
     tâche de fond, puis `200`).
   - **Astuce du renommage de branche** : renommer la branche par défaut puis la remettre, ce qui
     invalide le fragment mis en cache :
     ```
     gh api --method POST repos/<org>/<repo>/branches/main/rename     -f new_name=main-tmp
     gh api --method POST repos/<org>/<repo>/branches/main-tmp/rename  -f new_name=main
     gh api --method PATCH repos/<org>/<repo> -f default_branch=main   # si le pointeur reste sur main-tmp
     ```
3. **Attendre** que le fragment se régénère (de quelques minutes à ~24 h selon le TTL).

**Résultat** : « claude Claude » a **disparu** de l'encart Contributors de `Nkentseu`, puis la **même
procédure** a été appliquée à `Jenga` (`Rihen-Universe/Jenga`).

## Pièges rencontrés

- **Protection de branche bloque le renommage retour.** Sur `Nkentseu`, `main-tmp → main` a échoué en
  **HTTP 422** (« delete the branch protection rule for 'main' »). Il a fallu **retirer temporairement la
  protection** (`DELETE .../branches/main/protection`), renommer, puis la re-créer au besoin. Sur `Jenga`,
  la protection (permissive, 0 review requise) **n'a pas bloqué** le renommage → aucune manip de protection
  nécessaire.
- **Le pointeur `default_branch` reste sur `main-tmp`** juste après le renommage retour (délai de
  propagation) → forcer avec `PATCH ... -f default_branch=main`.
- **Ne PAS confondre** la donnée (déjà propre : API `contributors`/`stats`) et l'affichage (cache). Vérifier
  la donnée d'abord évite de « réparer » un faux problème (ex. supprimer le dépôt inutilement).

## Prévention

- `includeCoAuthoredBy: false` + `attribution` vides dans `~/.claude/settings.json` (global) — déjà en place.
- Les scripts `gitcommit.sh`/`gitpr.sh` du dépôt ne mettent **aucune** mention Claude.
- En cas de récidive de l'encart : relancer l'astuce du renommage + `stats/contributors`, puis patienter ;
  ne recourir au **delete+recreate** (perte stars/forks/PRs) qu'en dernier recours explicitement validé.
