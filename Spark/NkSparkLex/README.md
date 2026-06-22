# NkSparkLex

Analyse lexicale : transforme un source `.spark` en flux de **tokens**.

## Rôle

- Découpe le texte en tokens : identifiants, mots-clés, littéraux (entiers déc/hex/bin,
  flottants, chaînes, caractères), opérateurs, ponctuation.
- Conserve la **position** de chaque token (SourceSpan) pour les diagnostics.
- Gère commentaires (`//`, `/* */`), espaces, fins de ligne.
- Tolérant aux erreurs : émet un diagnostic et continue (récupération), pour rapporter
  plusieurs erreurs en une passe.

## Notes embarqué

- Littéraux entiers avec suffixes de largeur (`0xFFu8`, `1024u32`) utiles pour MMIO.
- Pas de préprocesseur C ; les imports/modules sont gérés au niveau Parse/Sema.

## Dépendances

- NkSparkCore (tokens, spans, diagnostics).
