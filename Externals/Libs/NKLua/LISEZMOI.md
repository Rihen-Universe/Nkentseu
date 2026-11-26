# NKLua : Lua 5.5.1, bibliothèque TIERCE, copiée sans modification

Emprunt noté dans `D:\Projets\2026\Nkentseu\Tests\agents_14-10\EMPRUNTS_A_REMPLACER.md` (règle de Rodolf du
18/11/2026 : on utilise l'existant en attendant le nôtre). Ouvert le 25/11/2026 au soir par l'agent scripts-2 (ordre de
Rodolf : AngelScript, Lua, puis PocketPy ; Lua est du C pur, sans la réserve « MSVC seulement » de PocketPy).

## Provenance
- Bibliothèque : Lua 5.5.1, Lua.org, PUC-Rio (`LUA_VERSION_MAJOR` 5, `LUA_VERSION_MINOR` 5, release 1, `sdk/lua/src/lua.h`).
- Archive : `lua-5.5.1.tar.gz` (lua.org), téléchargée le 25/11/2026 par l'agent scripts dans un dossier neuf
  (`Tests\agents_14-10\scripts\tiers\`), SHA-256 `1c4b4068d67061f2a2231ad2b5422e77acea1487ea9890f6320af614f4373dce`.
- Copié : `src/` de l'archive À L'IDENTIQUE dans `sdk/lua/src/` (`diff -r` vide le 25/11), `README` et
  `doc/readme.html`. Rien d'autre.
- Toute modification future d'un fichier de `sdk/` doit être MARQUÉE comme telle.

## Licence
MIT : `LICENCE_MIT.txt` (le texte de la fin de `lua.h`, lignes 523-543, recopié tel quel). Usage commercial libre ;
la mention de copyright et la permission restent dans toutes les copies.

## Construction (NKLua.jenga)
- Bibliothèque statique en C, sans `lua.c` ni `luac.c` (les interpréteurs autonomes) ; avertissements de ce code
  tiers coupés pour CETTE cible seulement ; `LUA_USE_POSIX` hors Windows.
- Lua calcule en double (`lua_Number`) ; nos composants sont en float32 : les bancs comparent avec une tolérance,
  jamais à l'octet (CONCEPTION_SCRIPTS.md, risque 4).
- L'hôte (notre code : zéro avertissement, aucun std::) vit dans Noge : `Engine/Noge/src/Noge/ECS/Scripting/Lua/`.
  Ses bancs : `Engine/Noge/tests/test_lua_*.cpp`.
