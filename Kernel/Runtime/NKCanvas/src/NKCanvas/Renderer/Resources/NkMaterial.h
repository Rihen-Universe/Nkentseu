#pragma once
// =============================================================================
// NkMaterial.h — Materiau 2D : shader + parametres preconfigures (style Godot)
//
// Pourquoi ?
//   NkShader expose une API GPU brute (compile + set uniformes). Un material
//   encapsule un usage concret : « ce shader avec ces parametres pour ce
//   genre d'effet ». Permet de cree des bibliotheques d'effets reutilisables :
//
//     - **Toon 2D** : sprite avec quantization de couleur (3-5 niveaux),
//       outline detection par sobel, ramp via texture LUT.
//     - **Lumiere 2D** : circular lighting / spot avec attenuation distance.
//     - **Feu** : noise UV + gradient + glow additif (blend NK_ADD).
//     - **Eau** : ripple/wave UV (sin(uv.x + t) sur amplitude reglable).
//     - **Distortion** : heat-haze, refraction sample-back.
//     - **Color grading** : LUT 1D/3D pour cinematique.
//     - **Particles glow** : additive blend + radial gradient.
//
//   L'app instancie le material une fois (chargement + compile + uniforms
//   defauts), puis l'attache aux draws via target.Draw(drawable, material.States()).
//
// MODELE
//   - 1 NkShader (compile a partir de sources que l'app fournit ou que la
//     bibliotheque NkMaterial::* fournit en preset).
//   - N uniformes (nom string + valeur). Ecriture differee : on les stocke
//     dans un side-buffer, l'app appelle Apply() pour les uploader sur le
//     shader avant un draw. Ou on appelle Apply() implicite quand le material
//     est utilise via target.Draw(drawable, mat.States()).
//   - Etats par defaut : blendMode, texture courante.
//
// USAGE
//   // ── Setup (au chargement de la scene) ──
//   NkShader fireShader;
//   fireShader.SetSourceGLSL(kFire_VS, kFire_FS);   // fournis par le preset
//   fireShader.Compile(*target.GetRenderer());
//
//   NkMaterial fireMat;
//   fireMat.SetShader(&fireShader);
//   fireMat.SetBlendMode(NkBlendMode::NK_ADD);
//   fireMat.SetTexture("uNoise", &noiseTex);
//   fireMat.SetVec3("uColorHot",  1.0f, 0.7f, 0.2f);
//   fireMat.SetVec3("uColorCool", 1.0f, 0.0f, 0.0f);
//
//   // ── Frame loop ──
//   fireMat.SetFloat("uTime", time);
//   target.Draw(flameSprite, fireMat.States());
//
// PHILOSOPHIE
//   NkMaterial est un *value object* qui *contient* les valeurs d'uniformes
//   et *reference* un shader externe. La duree de vie du shader doit etre >=
//   celle du material. Plusieurs materials peuvent partager un meme shader
//   (variants : feu rouge vs bleu — meme shader, params differents).
//
// LIMITES (premiere passe)
//   - Pas de hot-reload : si tu modifies le source shader, tu dois recompiler
//     manuellement (NkShader::Compile a nouveau).
//   - Pas de uniform buffer : chaque uniform est setke par appel GL/DX
//     individuel via NkShader. Sur un nombre eleve d'uniformes / draws,
//     considerer un Uniform Buffer Object explicite (future v2).
//   - L'application des uniforms est faite lazyement quand States() est
//     appele (cf Apply()). Si plusieurs materials avec le meme shader sont
//     utilises sur la meme frame, le dernier Apply gagne (les uniforms sont
//     des etats GPU globaux par programme).
// =============================================================================

#include "NKCanvas/Renderer/Core/NkRenderStates.h"
#include "NKCanvas/Renderer/Resources/NkShader.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
    namespace renderer {

        class NkTexture;

        class NkMaterial {
            public:
                NkMaterial() noexcept = default;
                ~NkMaterial() noexcept = default;

                // Copiable (que des handles non-owning et POD). Si necessaire,
                // expliciter copy/move plus tard.
                NkMaterial(const NkMaterial&)            = default;
                NkMaterial(NkMaterial&&) noexcept        = default;
                NkMaterial& operator=(const NkMaterial&) = default;
                NkMaterial& operator=(NkMaterial&&) noexcept = default;

                // ── Shader rattache ────────────────────────────────────────────
                void           SetShader(NkShader* s) noexcept { mShader = s; }
                NkShader*      GetShader()       noexcept       { return mShader; }
                const NkShader* GetShader() const noexcept       { return mShader; }

                // ── Etats GPU par defaut ───────────────────────────────────────
                void        SetBlendMode(NkBlendMode m) noexcept { mBlendMode = m; }
                NkBlendMode GetBlendMode() const noexcept        { return mBlendMode; }

                void               SetMainTexture(const NkTexture* tex) noexcept { mMainTexture = tex; }
                const NkTexture*   GetMainTexture() const noexcept                { return mMainTexture; }

                // ── Uniformes scalaires/vectoriels (cumules, Apply() les pose) ─
                void SetFloat(const char* name, float32 v)               { Push(Uniform::Float, name, &v, 1); }
                void SetVec2 (const char* name, float32 x, float32 y)    { float32 d[2]{x,y}; Push(Uniform::Vec2, name, d, 2); }
                void SetVec3 (const char* name, float32 x, float32 y, float32 z) { float32 d[3]{x,y,z}; Push(Uniform::Vec3, name, d, 3); }
                void SetVec4 (const char* name, float32 x, float32 y, float32 z, float32 w) { float32 d[4]{x,y,z,w}; Push(Uniform::Vec4, name, d, 4); }
                void SetColor(const char* name, const NkColor2D& c)      {
                    const float32 inv = 1.f / 255.f;
                    SetVec4(name, c.r * inv, c.g * inv, c.b * inv, c.a * inv);
                }
                void SetMat4 (const char* name, const float32* mat16)    { Push(Uniform::Mat4, name, mat16, 16); }

                // ── Uniforme texture (slot indexe) ─────────────────────────────
                void SetTexture(const char* name, const NkTexture* tex, uint32 slot = 0) {
                    TextureBinding tb;
                    tb.name = name;
                    tb.texture = tex;
                    tb.slot = slot;
                    // Remplace ou ajoute.
                    for (auto& b : mTextures) {
                        if (b.name == name) { b = tb; return; }
                    }
                    mTextures.PushBack(tb);
                }

                // ── Application des uniformes au shader (avant draw) ───────────
                /// Upload tous les uniformes accumules sur le NkShader rattache.
                /// Appele automatiquement par States() — l'app peut aussi
                /// l'invoquer manuellement (ex. apres avoir change un uniform
                /// dans la frame loop sans repasser par States()).
                void Apply() const {
                    if (!mShader || !mShader->IsValid()) return;
                    for (const auto& u : mUniforms) {
                        const char* n = u.name.CStr();
                        switch (u.kind) {
                            case Uniform::Float: mShader->SetFloat(n, u.data[0]); break;
                            case Uniform::Vec2:  mShader->SetVec2 (n, NkVec2f{u.data[0], u.data[1]}); break;
                            case Uniform::Vec3:  mShader->SetVec3 (n, u.data[0], u.data[1], u.data[2]); break;
                            case Uniform::Vec4:  mShader->SetVec4 (n, u.data[0], u.data[1], u.data[2], u.data[3]); break;
                            case Uniform::Mat4:  mShader->SetMat4 (n, u.data); break;
                        }
                    }
                    for (const auto& tb : mTextures) {
                        mShader->SetTexture(tb.name.CStr(), tb.texture, tb.slot);
                    }
                }

                /// Retourne un NkRenderStates pret a passer a target.Draw(drawable, mat.States()).
                /// L'Apply() est invoque implicitement (uniformes uploades juste avant).
                NkRenderStates States() const {
                    Apply();
                    NkRenderStates s;
                    s.blendMode = mBlendMode;
                    s.texture   = mMainTexture;
                    s.shader    = mShader;
                    // transform reste identite (le drawable composera son propre transform).
                    return s;
                }

            private:
                struct Uniform {
                    enum Kind : uint8 { Float, Vec2, Vec3, Vec4, Mat4 };
                    Kind     kind = Float;
                    NkString name;
                    float32  data[16]{}; // suffisant pour Mat4 ; Float/Vec utilisent les premiers
                };

                struct TextureBinding {
                    NkString         name;
                    const NkTexture* texture = nullptr;
                    uint32           slot    = 0;
                };

                void Push(Uniform::Kind k, const char* name, const float32* src, uint32 count) {
                    // Remplace si meme nom + kind, sinon append.
                    for (auto& u : mUniforms) {
                        if (u.kind == k && u.name == name) {
                            for (uint32 i = 0; i < count && i < 16; ++i) u.data[i] = src[i];
                            return;
                        }
                    }
                    Uniform u;
                    u.kind = k;
                    u.name = name;
                    for (uint32 i = 0; i < count && i < 16; ++i) u.data[i] = src[i];
                    mUniforms.PushBack(u);
                }

                NkShader*               mShader{nullptr};         ///< non-owning
                const NkTexture*        mMainTexture{nullptr};    ///< non-owning, slot 0
                NkBlendMode             mBlendMode{NkBlendMode::NK_ALPHA};
                NkVector<Uniform>       mUniforms;
                NkVector<TextureBinding> mTextures;
        };

    } // namespace renderer
} // namespace nkentseu
