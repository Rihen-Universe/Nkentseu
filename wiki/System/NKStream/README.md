# NKStream — documentation détaillée

Le module **NKStream**, partie par partie. Pour une vue d'ensemble et un guide « par où
commencer », voir le récap : [../NKStream.md](../NKStream.md).

Chaque page suit la même structure : un **tutoriel** narratif, un **aperçu** tabulaire de
toute l'API, puis une **référence-cours** où chaque élément est expliqué avec son contrat, sa
complexité et ses pièges réels.

| Page | Ce qu'on y apprend | Headers |
|------|--------------------|---------|
| [Streams.md](Streams.md) | L'interface de flux abstraite et ses trois implémentations : `ReadRaw`/`WriteRaw` et `Read<T>`/`Write<T>`, `Seek`/`Tell`/`Size`/`IsEOF`, encodage, modes d'ouverture ; flux mémoire auto-extensible ; flux fichier (via `NkFile`, BOM) ; flux console (stdin/stdout/stderr, UTF-8/UTF-16). | `NkStream.h`, `NkBinaryStream.h`, `NkFileStream.h`, `NkConsoleStream.h` |
| [Resource.md](Resource.md) | L'interface ressource CPU `NKIResource` : `LoadFromFile`/`Memory`/`Stream`, `IsValid`/`Unload`, `SaveTo*` (optionnels) et le contrat d'ownership `NkAlloc`/`NkFree`. Implémentée par NkImage, NkFont, NkAudioSample. | `NKIResource.h` |

[← Récap NKStream](../NKStream.md) · [← Couche System](../README.md)
