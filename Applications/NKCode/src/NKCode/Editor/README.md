# Editor — l'éditeur de code texte

> ⚠️ Squelette : pas encore de code. Voir l'[architecture](../../../ARCHITECTURE.md) et la
> [roadmap](../../../ROADMAP.md).

## Rôle
`Editor` est le **cœur « VSCode »** de NKCode : un éditeur de code texte performant. Il gère un
**tampon de texte** (les lignes du fichier), le **curseur** et la **sélection** (jusqu'au
multi-curseurs), l'**undo/redo**, et le rendu **monospace** via NKFont avec une **gouttière**
(numéros de ligne). Sa fonction la plus visible est la **coloration syntaxique** : un *tokenizer*
découpe le texte, un *thème* associe une couleur à chaque catégorie, et le rendu colore chaque
token (`RenderText`).

Il fournit aussi le confort attendu : recherche/remplacement, repli de code, minimap, et — plus
tard — la **complétion** et les **diagnostics** (via les services de langage). Il s'ouvre/sauve via
NKFileSystem et s'affiche dans un panneau du Shell (avec onglets).

## Responsabilités
- Tampon de texte + curseur/sélection (multi-curseurs) + undo/redo.
- Rendu monospace + gouttière + survol/sélection ; défilement.
- **Coloration syntaxique** (tokenizer + thème) ; recherche/remplacement.
- Ouvrir/sauver (NKFileSystem) ; onglets ; (plus tard) complétion/LSP, diagnostics.

## Dépend de
**NKFont**, **NKUI**/**NKCanvas**, NKEvent, NKFileSystem, NKContainers (tampon).

[Architecture](../../../ARCHITECTURE.md) · [Roadmap](../../../ROADMAP.md)
