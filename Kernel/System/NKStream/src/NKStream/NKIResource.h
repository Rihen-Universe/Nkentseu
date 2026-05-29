#pragma once
// =============================================================================
// NKIResource.h — Interface commune des ressources média (CPU)
//
// Contrat de chargement/sauvegarde partagé par toutes les ressources média
// décodables côté CPU : NkImage (PNG/JPG/HDR/...), NkFont (TTF/OTF),
// NkAudioSample (WAV/MP3/OGG/FLAC), etc. Strictement CPU : décode un
// fichier/mémoire/flux vers des données en RAM, et inversement pour Save.
//
// Les ressources GPU (NkTexture de NKCanvas, textures de NKRHI / NKRenderer)
// N'IMPLÉMENTENT PAS cette interface : chaque couche de rendu a son propre
// "device" de création (NkIRenderer2D pour NKCanvas, NkIDevice pour NKRHI,
// son propre renderer pour NKRenderer). Elles CONSOMMENT une NKIResource CPU
// (ex. NkImage), puis uploadent avec leur device propre. Voir l'analyse
// d'architecture du 2026-05-28 : pas d'interface GPU commune cross-couche.
//
// Emplacement : NKStream, car LoadFromStream/SaveToStream manipulent NkStream
// et que NKStream est bas dans la stack (System), accessible par NKImage,
// NKFont, NKCanvas, NKAudio sans nouvelle dépendance lourde. Distinct de
// NKSerialization (qui sérialise des OBJETS structurés via NkISerializable,
// pas des formats média via codecs).
// =============================================================================

#include "NKCore/NkTypes.h"

namespace nkentseu {

    class NkStream; // NKStream/NkStream.h — flux binaire/fichier/mémoire

    // -------------------------------------------------------------------------
    // NKIResource — ressource média chargeable/sauvegardable (CPU)
    // -------------------------------------------------------------------------
    class NKIResource {
        public:
            virtual ~NKIResource() = default;

            // ── Chargement (obligatoire) ─────────────────────────────────────
            virtual bool LoadFromFile  (const char* path)             = 0;
            virtual bool LoadFromMemory(const void* data, usize size) = 0;
            virtual bool LoadFromStream(NkStream& stream)             = 0;

            // ── Sauvegarde (optionnelle : retourne false si le codec ne sait
            //    pas écrire ce format). SaveToMemory alloue `out` via
            //    nkentseu::memory::NkAlloc : libérer avec
            //    nkentseu::memory::NkFree (JAMAIS std::free / delete[] —
            //    heap corruption Windows c0000374 sinon).
            virtual bool SaveToFile  (const char* path)         const { (void)path; return false; }
            virtual bool SaveToMemory(uint8*& out, usize& size) const { (void)out; (void)size; return false; }
            virtual bool SaveToStream(NkStream& stream)         const { (void)stream; return false; }

            // ── État ──────────────────────────────────────────────────────────
            virtual bool IsValid() const = 0;
            virtual void Unload()        = 0;
    };

} // namespace nkentseu
