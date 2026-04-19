#pragma once
// =============================================================================
// Nkentseu/Modeling/NkMeshModifier.h
// =============================================================================
// Stack de modificateurs non-destructifs pour NkEditableMesh.
//
// DESIGN :
//   • Chaque modificateur prend un NkEditableMesh en entrée et produit un
//     NkEditableMesh modifié en sortie (fonctionnel, pas in-place)
//   • La stack est réévaluée à chaque changement d'un modificateur ou du mesh
//   • Le mesh original (base mesh) n'est jamais modifié
//   • Comparable aux modificateurs Blender (Object Mode modifier stack)
//
// USAGE :
//   NkModifierStack stack;
//   stack.Add<NkBevelModifier>().width = 0.1f;
//   stack.Add<NkMirrorModifier>().axis = NkMirrorModifier::Axis::X;
//   stack.Add<NkSubdivModifier>().levels = 2;
//
//   NkEditableMesh result = stack.Apply(baseMesh);
//   renderer::NkMeshDesc gpuDesc;
//   result.ToMeshDesc(gpuDesc);
// =============================================================================

#include "NkEditableMesh.h"

namespace nkentseu {

    // =========================================================================
    // NkMeshModifier — classe de base abstraite
    // =========================================================================
    class NkMeshModifier {
        public:
            static constexpr uint32 kMaxNameLen = 64u;
            char name[kMaxNameLen] = {};

            bool enabled = true;    ///< Actif ou bypassé
            bool showInViewport = true;
            bool showInRender   = true;

            virtual ~NkMeshModifier() noexcept = default;

            /**
             * @brief Applique le modificateur sur le mesh d'entrée.
             * @param input  Mesh source (non modifié).
             * @param output Mesh résultant (peut être une copie modifiée de input).
             */
            virtual void Apply(const NkEditableMesh& input,
                               NkEditableMesh& output) const noexcept = 0;

            [[nodiscard]] virtual const char* GetTypeName() const noexcept = 0;
    };

    // =========================================================================
    // Modificateurs built-in
    // =========================================================================

    // ── Bevel ─────────────────────────────────────────────────────────────────
    class NkBevelModifier : public NkMeshModifier {
        public:
            float32 width    = 0.1f;
            uint32  segments = 2;
            float32 profile  = 0.5f;  ///< Profil du biseau [0=concave, 1=convexe]
            bool    onlyVerts = false; ///< Bevel sur vertices uniquement

            NkBevelModifier() noexcept {
                std::strncpy(name, "Bevel", kMaxNameLen - 1);
            }

            void Apply(const NkEditableMesh& in,
                       NkEditableMesh& out) const noexcept override;

            const char* GetTypeName() const noexcept override { return "Bevel"; }
    };

    // ── Mirror ────────────────────────────────────────────────────────────────
    class NkMirrorModifier : public NkMeshModifier {
        public:
            enum class Axis : uint8 { X = 1, Y = 2, Z = 4 };

            Axis    axis            = Axis::X;
            bool    mergeMirror     = true;    ///< Fusionne les vertices à la jointure
            float32 mergeThreshold  = 0.001f;
            bool    flipNormals     = false;
            bool    mirrorUV        = false;

            NkMirrorModifier() noexcept {
                std::strncpy(name, "Mirror", kMaxNameLen - 1);
            }

            void Apply(const NkEditableMesh& in,
                       NkEditableMesh& out) const noexcept override;

            const char* GetTypeName() const noexcept override { return "Mirror"; }
    };

    // ── Subdivide ─────────────────────────────────────────────────────────────
    class NkSubdivModifier : public NkMeshModifier {
        public:
            uint32 levels       = 1;
            bool   catmullClark = true;   ///< Catmull-Clark ou simple midpoint
            bool   useCrease    = true;   ///< Respecte les edges sharp pour créases

            NkSubdivModifier() noexcept {
                std::strncpy(name, "Subdivide", kMaxNameLen - 1);
            }

            void Apply(const NkEditableMesh& in,
                       NkEditableMesh& out) const noexcept override;

            const char* GetTypeName() const noexcept override { return "Subdivide"; }
    };

    // ── Solidify ──────────────────────────────────────────────────────────────
    class NkSolidifyModifier : public NkMeshModifier {
        public:
            float32 thickness       = 0.01f;
            float32 offset          = -1.f;  ///< [-1=intérieur, +1=extérieur]
            bool    fillRim         = true;   ///< Fermer les bords
            bool    flipNormals     = false;

            NkSolidifyModifier() noexcept {
                std::strncpy(name, "Solidify", kMaxNameLen - 1);
            }

            void Apply(const NkEditableMesh& in,
                       NkEditableMesh& out) const noexcept override;

            const char* GetTypeName() const noexcept override { return "Solidify"; }
    };

    // ── Array ─────────────────────────────────────────────────────────────────
    class NkArrayModifier : public NkMeshModifier {
        public:
            uint32  count       = 2;
            NkVec3f offset      = {1, 0, 0};  ///< Offset relatif par copie
            bool    mergeEnds   = false;       ///< Fusionne la première et la dernière copie

            NkArrayModifier() noexcept {
                std::strncpy(name, "Array", kMaxNameLen - 1);
            }

            void Apply(const NkEditableMesh& in,
                       NkEditableMesh& out) const noexcept override;

            const char* GetTypeName() const noexcept override { return "Array"; }
    };

    // ── Decimate ──────────────────────────────────────────────────────────────
    class NkDecimateModifier : public NkMeshModifier {
        public:
            float32 ratio       = 0.5f;  ///< Ratio de faces conservées [0..1]
            bool    planar      = false;  ///< Décimation planaire (face-coplanaires)
            float32 angleLimit  = 5.f;   ///< Angle limite pour planaire (degrés)

            NkDecimateModifier() noexcept {
                std::strncpy(name, "Decimate", kMaxNameLen - 1);
            }

            void Apply(const NkEditableMesh& in,
                       NkEditableMesh& out) const noexcept override;

            const char* GetTypeName() const noexcept override { return "Decimate"; }
    };

    // ── Smooth ────────────────────────────────────────────────────────────────
    class NkSmoothModifier : public NkMeshModifier {
        public:
            uint32  iterations  = 5;
            float32 factor      = 0.5f;  ///< Force du lissage [0..1]
            bool    onlyX       = false;
            bool    onlyY       = false;
            bool    onlyZ       = false;

            NkSmoothModifier() noexcept {
                std::strncpy(name, "Smooth", kMaxNameLen - 1);
            }

            void Apply(const NkEditableMesh& in,
                       NkEditableMesh& out) const noexcept override;

            const char* GetTypeName() const noexcept override { return "Smooth"; }
    };

    // =========================================================================
    // NkModifierStack — stack ordonnée de modificateurs
    // =========================================================================
    class NkModifierStack {
        public:
            NkModifierStack()  noexcept = default;
            ~NkModifierStack() noexcept { Clear(); }

            NkModifierStack(const NkModifierStack&) = delete;
            NkModifierStack& operator=(const NkModifierStack&) = delete;

            // ── Gestion de la stack ───────────────────────────────────────

            /**
             * @brief Ajoute un modificateur à la fin de la stack.
             * @return Référence au modificateur créé pour configuration.
             */
            template<typename T, typename... Args>
            T& Add(Args&&... args) noexcept {
                static_assert(std::is_base_of_v<NkMeshModifier, T>);
                auto* mod = new T(std::forward<Args>(args)...);
                mModifiers.PushBack(mod);
                mDirty = true;
                return *mod;
            }

            /**
             * @brief Supprime le modificateur à l'index donné.
             */
            void Remove(uint32 index) noexcept;

            /**
             * @brief Déplace un modificateur (changement d'ordre dans la stack).
             */
            void MoveUp  (uint32 index) noexcept;
            void MoveDown(uint32 index) noexcept;

            /**
             * @brief Vide la stack complètement.
             */
            void Clear() noexcept {
                for (nk_usize i = 0; i < mModifiers.Size(); ++i)
                    delete mModifiers[i];
                mModifiers.Clear();
                mDirty = true;
            }

            // ── Application ──────────────────────────────────────────────

            /**
             * @brief Applique toute la stack sur le mesh de base.
             * @param baseMesh Mesh original (jamais modifié).
             * @return Mesh résultant après application de tous les modificateurs.
             *
             * @note Si aucun modificateur n'est actif, retourne un clone de baseMesh.
             */
            [[nodiscard]] NkEditableMesh Apply(const NkEditableMesh& baseMesh) const noexcept;

            /**
             * @brief Applique jusqu'à un modificateur donné (pour preview partielle).
             */
            [[nodiscard]] NkEditableMesh ApplyUpTo(const NkEditableMesh& baseMesh,
                                                    uint32 upToIndex) const noexcept;

            /**
             * @brief "Applique" définitivement la stack : fusionne dans le base mesh
             *        et vide la stack.
             */
            void ApplyAll(NkEditableMesh& baseMesh) noexcept;

            // ── Accès ─────────────────────────────────────────────────────
            [[nodiscard]] uint32 Count() const noexcept {
                return static_cast<uint32>(mModifiers.Size());
            }

            [[nodiscard]] NkMeshModifier* Get(uint32 i) noexcept {
                return i < mModifiers.Size() ? mModifiers[i] : nullptr;
            }

            [[nodiscard]] const NkMeshModifier* Get(uint32 i) const noexcept {
                return i < mModifiers.Size() ? mModifiers[i] : nullptr;
            }

            void MarkDirty() noexcept { mDirty = true; }
            [[nodiscard]] bool IsDirty() const noexcept { return mDirty; }

        private:
            NkVector<NkMeshModifier*> mModifiers;
            mutable bool              mDirty = true;
    };

} // namespace nkentseu
