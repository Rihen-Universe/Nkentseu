# NKPocketPy : PocketPy 2.2.0, bibliothèque TIERCE, copiée sans modification

Emprunt noté dans `D:\Projets\2026\Nkentseu\Tests\agents_14-10\EMPRUNTS_A_REMPLACER.md` (règle de Rodolf du
18/11/2026 : on utilise l'existant en attendant le nôtre). Ouvert le 25/11/2026 au soir par l'agent scripts-2 (ordre de
Rodolf : AngelScript, Lua, puis PocketPy ; « PocketPy partout ; CPython embarqué en option, sur ordinateur »).

## Provenance
- Bibliothèque : PocketPy 2.2.0, blueloveTH (`PK_VERSION` dans `sdk/pocketpy/include/pocketpy/config.h`).
- Archive : `pocketpy-v2.2.0.zip`, téléchargée le 25/11/2026 par l'agent scripts dans un dossier neuf
  (`Tests\agents_14-10\scripts\tiers\`), SHA-256 `bc418838794bfe1a94d44bf23051d0728723f03c1e3ac2c4e50635168c17b391`.
- Copié À L'IDENTIQUE (`diff -r` vide le 25/11) : `src/`, `include/` (sans `include/pybind11/`, l'enveloppe C++ du
  projet, qui n'est pas prise), `LICENSE`, `README.md`. Les sous-modules `3rd/` (lz4, c-periphery, cute_png, msgpack)
  ne sont PAS pris : leurs modules sont éteints, leurs licences ne sont pas lues.
- Toute modification future d'un fichier de `sdk/` doit être MARQUÉE comme telle.

## Licence
MIT : `LICENCE_MIT.txt` (copie de `LICENSE`, « Copyright (c) 2026 blueloveTH »). Usage commercial libre ; la mention
de copyright et la permission restent dans toutes les copies.

## Construction (NKPocketPy.jenga)
- Bibliothèque statique en C11 ; avertissements de ce code tiers coupés pour CETTE cible seulement.
- `PK_ENABLE_OS=0`, `PK_ENABLE_THREADS=0`, `PK_ENABLE_DLL=0`, `PK_ENABLE_DETERMINISM=0`.
- Le README (ligne 55) ne garantit sous Windows que MSVC. MESURÉ le 25/11/2026 à 23 h 27 : les 84 fichiers se
  compilent avec notre chaîne clang (cible MinGW), en Debug. Le fonctionnement est prouvé par les bancs de Noge.
- PocketPy a UNE machine globale (jusqu'à 16, `py_switchvm`) : l'hôte en prend une par instance et la rend.
- L'hôte (notre code : zéro avertissement, aucun std::) vit dans Noge : `Engine/Noge/src/Noge/ECS/Scripting/Python/`.
