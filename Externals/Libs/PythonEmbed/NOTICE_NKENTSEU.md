# NOTICE — Repackaging Nkentseu / Jenga

Ce depot est un **repackaging** de **CPython 3.12.7** (distribution "embeddable"
officielle + fichiers SDK `include/`/`libs/`) pour l'embarquement in-process
de Jenga dans NKCode (moteur **Nkentseu**, https://github.com/Rihen-Universe/Nkentseu).

- `runtime/` : package embeddable officiel Windows x64
  (https://www.python.org/ftp/python/3.12.7/python-3.12.7-embed-amd64.zip) —
  ce qui est COPIE a cote de NKCode.exe a l'installation (`tools/python-embed/`).
- `sdk/include/` + `sdk/libs/` : fichiers de developpement (`Python.h`,
  `python312.lib`) necessaires pour COMPILER/LIER NkEmbeddedJenga — jamais
  distribues avec NKCode, uniquement utilises au moment du build.
- Version bundlee : voir `VERSION` a cote de ce fichier — a mettre a jour ICI
  (independamment d'une nouvelle release de NKCode) pour livrer un correctif
  Python sans reinstaller tout NKCode.
- Licence amont : **PSF License** (Python Software Foundation). Upstream :
  https://www.python.org/downloads/

Aucune revendication de propriete sur le code amont.
