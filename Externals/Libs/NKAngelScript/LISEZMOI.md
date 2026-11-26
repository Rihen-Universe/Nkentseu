# NKAngelScript : AngelScript 2.39.0, bibliothèque TIERCE, copiée sans modification

Emprunt noté dans `D:\Projets\2026\Nkentseu\Tests\agents_14-10\EMPRUNTS_A_REMPLACER.md` (règle de Rodolf du
18/11/2026 : on utilise l'existant en attendant le nôtre). Ouvert le 25/11/2026 par l'agent scripts (étape E4 de
`Tests\agents_14-10\scripts\CONCEPTION_SCRIPTS.md`), avec l'accord du coordinateur.

## Provenance
- Bibliothèque : AngelCode Scripting Library, Andreas Jönsson, version 2.39.0 (`ANGELSCRIPT_VERSION_STRING`,
  `sdk/angelscript/include/angelscript.h:62`).
- Archive : `https://www.angelcode.com/angelscript/sdk/files/angelscript_2.39.0.zip`, téléchargée le 25/11/2026,
  2 487 715 octets, SHA-256 `f029bea4e52c360c750d3a805b80f9317d177241fadbc8f93845e38355d11287`.
- Copié : `sdk/angelscript/include/` et `sdk/angelscript/source/` de l'archive, À L'IDENTIQUE (`diff -r` vide le
  25/11), et `sdk/docs/manual/doc_license.html`. Rien d'autre (ni add_on, ni samples, ni projets de construction).
- Toute modification future d'un fichier de `sdk/` doit être MARQUÉE comme telle (clause 2 de la licence).

## Licence
zlib : `LICENCE_zlib.txt` (le texte de l'en-tête de `angelscript.h`, recopié tel quel) et
`sdk/docs/manual/doc_license.html`. Usage commercial libre ; ne pas prétendre l'avoir écrite ; sources modifiées
marquées ; la mention reste dans les sources. Une citation dans la documentation du produit est appréciée.

## Construction (NKAngelScript.jenga)
- Cible à part, bibliothèque statique ; les avertissements de ce code tiers sont coupés pour CETTE cible seulement.
- `AS_MAX_PORTABILITY` partout : seuls les appels GÉNÉRIQUES (`asCALL_GENERIC`) existent, aucune convention d'appel
  native, aucun fichier assembleur. C'est le chemin qu'Emscripten impose de toute façon (`as_config.h:389`) ; un
  seul chemin, testé une fois, pour Windows, Android, iOS et le web. Le natif x64 MinGW (`AS_X64_MINGW`,
  `as_config.h:931`, `as_callfunc_x64_mingw.cpp`) n'est ni compilé ni éprouvé : il ne viendra qu'après mesure.
- NOTRE code (le banc, `tests/`) : zéro avertissement, aucun std::. L'hôte (ancien `pont/`) vit dans Noge depuis le
  25/11/2026 au soir : `Engine/Noge/src/Noge/ECS/Scripting/AngelScript/` (déplacé, pas réécrit).
