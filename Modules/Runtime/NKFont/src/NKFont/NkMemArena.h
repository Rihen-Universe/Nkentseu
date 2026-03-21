#pragma once
/**
 * @File    NkMemArena.h
 * @Brief   Allocateur linéaire (arena) — zéro malloc dans le hot path.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  Un arena est un bloc mémoire contigu pré-alloué. Les allocations avancent
 *  un curseur ; le tout est libéré en une seule opération (Reset).
 *  Aucune libération individuelle n'est possible.
 *
 *  Deux modes de construction :
 *    - Owning  : l'arena alloue et libère son buffer (via malloc/free).
 *    - View    : l'arena opère sur un buffer externe (pile, static, mappé).
 *
 *  NkScratchArena : RAII scope guard — sauvegarde/restaure le curseur.
 *  Permet l'allocation temporaire dans un sous-scope sans Reset global.
 *
 *  Thread-safety : AUCUNE. Un arena par thread (pattern recommandé).
 *
 * @Usage
 * @code
 *   // Arena owning (heap)
 *   NkMemArena arena;
 *   arena.Init(4 * 1024 * 1024);  // 4 MiB
 *
 *   float* pts  = arena.Alloc<float>(1024);
 *   uint8* bits = arena.Alloc<uint8>(512 * 512);
 *
 *   {
 *       NkScratchArena scratch(arena);   // sauvegarde curseur
 *       int32* tmp = arena.Alloc<int32>(256);
 *       // ... travail temporaire ...
 *   }  // curseur restauré ici
 *
 *   arena.Reset();  // repart de zéro, rien n'est libéré
 *   arena.Destroy();
 * @endcode
 */

#include "NKFont/NkFontExport.h"
#include "NKCore/NkTypes.h"
#include <cstdlib>   // malloc / free — seule dépendance C autorisée
#include <cstring>   // memset

namespace nkentseu {

    // =========================================================================
    //  NkMemArena
    // =========================================================================

    class NKENTSEU_FONT_API NkMemArena {
    public:

        // ── Constructeurs / destructeur ───────────────────────────────────────

        NkMemArena() noexcept = default;
        ~NkMemArena() noexcept { Destroy(); }

        NkMemArena(const NkMemArena&)            = delete;
        NkMemArena& operator=(const NkMemArena&) = delete;

        NkMemArena(NkMemArena&& o) noexcept
            : mBase(o.mBase), mSize(o.mSize)
            , mOffset(o.mOffset), mOwning(o.mOwning)
        {
            o.mBase  = nullptr;
            o.mSize  = 0;
            o.mOffset = 0;
            o.mOwning = false;
        }

        NkMemArena& operator=(NkMemArena&& o) noexcept {
            if (this != &o) {
                Destroy();
                mBase    = o.mBase;
                mSize    = o.mSize;
                mOffset  = o.mOffset;
                mOwning  = o.mOwning;
                o.mBase  = nullptr;
                o.mSize  = 0;
                o.mOffset = 0;
                o.mOwning = false;
            }
            return *this;
        }

        // ── Initialisation ────────────────────────────────────────────────────

        /**
         * @Brief Alloue un nouveau buffer (mode owning).
         * @param bytes    Taille totale en octets.
         * @param zeroInit Mettre à zéro le buffer à l'initialisation.
         * @return true si succès, false si malloc échoue.
         */
        bool Init(usize bytes, bool zeroInit = false) noexcept {
            Destroy();
            mBase = static_cast<uint8*>(::malloc(bytes));
            if (!mBase) return false;
            mSize   = bytes;
            mOffset = 0;
            mOwning = true;
            if (zeroInit) ::memset(mBase, 0, bytes);
            return true;
        }

        /**
         * @Brief Opère sur un buffer externe (mode view — pas de ownership).
         * @param buffer Pointeur vers le buffer externe.
         * @param bytes  Taille du buffer en octets.
         */
        void InitView(void* buffer, usize bytes) noexcept {
            Destroy();
            mBase   = static_cast<uint8*>(buffer);
            mSize   = bytes;
            mOffset = 0;
            mOwning = false;
        }

        /// Libère le buffer si owning, réinitialise tous les membres.
        void Destroy() noexcept {
            if (mOwning && mBase) ::free(mBase);
            mBase   = nullptr;
            mSize   = 0;
            mOffset = 0;
            mOwning = false;
        }

        // ── Allocation ────────────────────────────────────────────────────────

        /**
         * @Brief Alloue `bytes` octets alignés sur `align`.
         * @param bytes  Taille demandée.
         * @param align  Alignement (doit être une puissance de 2, défaut : 8).
         * @return Pointeur vers la zone allouée, nullptr si arena épuisé.
         * @note  Zéro-initialise le bloc alloué (comportement intentionnel
         *        pour éviter les glyphes corrompus par des pixels non initialisés).
         */
        NKFONT_NODISCARD void* AllocRaw(usize bytes, usize align = 8) noexcept {
            // Alignement du curseur courant
            const usize aligned = AlignUp(mOffset, align);
            if (aligned + bytes > mSize) return nullptr;
            void* ptr    = mBase + aligned;
            mOffset      = aligned + bytes;
            ::memset(ptr, 0, bytes);
            return ptr;
        }

        /**
         * @Brief Alloue un tableau de `count` objets de type T.
         * @note  Appelle uniquement AllocRaw — pas de constructeurs.
         *        T doit être trivially constructible (types POD).
         */
        template<typename T>
        NKFONT_NODISCARD T* Alloc(usize count = 1, usize align = alignof(T)) noexcept {
            return static_cast<T*>(AllocRaw(sizeof(T) * count, align));
        }

        /**
         * @Brief Alloue et copie `src` (count éléments de type T).
         */
        template<typename T>
        NKFONT_NODISCARD T* AllocCopy(const T* src, usize count) noexcept {
            T* dst = Alloc<T>(count);
            if (dst && src) ::memcpy(dst, src, sizeof(T) * count);
            return dst;
        }

        // ── Gestion du curseur ────────────────────────────────────────────────

        /// Remet le curseur à zéro sans libérer la mémoire.
        void Reset() noexcept { mOffset = 0; }

        /// Sauvegarde la position courante du curseur.
        NKFONT_NODISCARD usize SaveCursor() const noexcept { return mOffset; }

        /// Restaure une position sauvegardée (rollback partiel).
        void RestoreCursor(usize savedOffset) noexcept {
            if (savedOffset <= mSize) mOffset = savedOffset;
        }

        // ── Accesseurs ────────────────────────────────────────────────────────

        NKFONT_NODISCARD uint8* Base()      const noexcept { return mBase;           }
        NKFONT_NODISCARD usize  Size()      const noexcept { return mSize;           }
        NKFONT_NODISCARD usize  Used()      const noexcept { return mOffset;         }
        NKFONT_NODISCARD usize  Remaining() const noexcept { return mSize - mOffset; }
        NKFONT_NODISCARD bool   IsValid()   const noexcept { return mBase != nullptr; }
        NKFONT_NODISCARD bool   IsOwning()  const noexcept { return mOwning;         }

        /// Pourcentage d'utilisation [0.0, 1.0].
        NKFONT_NODISCARD float UsageRatio() const noexcept {
            return mSize > 0
                ? static_cast<float>(mOffset) / static_cast<float>(mSize)
                : 0.f;
        }

    private:
        uint8* mBase    = nullptr;
        usize  mSize    = 0;
        usize  mOffset  = 0;
        bool   mOwning  = false;

        /// Alignement vers le haut (align doit être une puissance de 2).
        static constexpr usize AlignUp(usize value, usize align) noexcept {
            return (value + align - 1) & ~(align - 1);
        }
    };

    // =========================================================================
    //  NkScratchArena — RAII scope guard
    // =========================================================================
    /**
     * @Brief Sauvegarde le curseur d'un NkMemArena à la construction,
     *        le restaure à la destruction.
     *
     * @Usage
     * @code
     *   {
     *       NkScratchArena scratch(arena);
     *       int32* tmp = arena.Alloc<int32>(1024);
     *       // ...
     *   }  // arena.RestoreCursor() appelé ici
     * @endcode
     */
    class NKENTSEU_FONT_API NkScratchArena {
    public:
        explicit NkScratchArena(NkMemArena& arena) noexcept
            : mArena(arena), mSaved(arena.SaveCursor())
        {}

        ~NkScratchArena() noexcept {
            mArena.RestoreCursor(mSaved);
        }

        NkScratchArena(const NkScratchArena&)            = delete;
        NkScratchArena& operator=(const NkScratchArena&) = delete;

        /// Accès direct à l'arena sous-jacent.
        NkMemArena& Arena() noexcept { return mArena; }

    private:
        NkMemArena& mArena;
        usize       mSaved;
    };

    // =========================================================================
    //  NkArenaPool — pool d'arenas pré-dimensionnés par usage
    // =========================================================================
    /**
     * @Brief Ensemble de 3 arenas avec des rôles distincts :
     *    - Permanent : données de la face (tables parsées, métriques) — jamais resetté
     *    - Frame     : données de rendu d'une frame — resetté chaque frame
     *    - Scratch   : calculs temporaires (rasterizer, hinter) — resetté par opération
     *
     * @Usage
     * @code
     *   NkArenaPool pool;
     *   pool.Init(
     *       8  * 1024 * 1024,   // 8  MiB permanent
     *       4  * 1024 * 1024,   // 4  MiB frame
     *       2  * 1024 * 1024    // 2  MiB scratch
     *   );
     * @endcode
     */
    struct NKENTSEU_FONT_API NkArenaPool {
        NkMemArena permanent;
        NkMemArena frame;
        NkMemArena scratch;

        bool Init(usize permanentBytes, usize frameBytes, usize scratchBytes) noexcept {
            return permanent.Init(permanentBytes)
                && frame.Init(frameBytes)
                && scratch.Init(scratchBytes);
        }

        void Destroy() noexcept {
            permanent.Destroy();
            frame.Destroy();
            scratch.Destroy();
        }

        /// Appelé à chaque début de frame.
        void BeginFrame() noexcept { frame.Reset(); }

        /// Appelé à chaque début d'opération de rendu d'un glyphe.
        void BeginGlyph() noexcept { scratch.Reset(); }
    };

} // namespace nkentseu
