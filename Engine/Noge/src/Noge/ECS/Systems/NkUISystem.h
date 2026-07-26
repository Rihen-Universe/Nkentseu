#pragma once
// =============================================================================
// Noge/ECS/Systems/NkUISystem.h — système ECS UI in-game (HUD), CPU-only
// =============================================================================
// Premier SYSTÈME réel derrière ECS/Components/UI/NkUIComponent.h (jusqu'ici
// données seules, aucun consommateur — cf. Engine/Noge/ROADMAP.md, Phase G2
// item G2.1 / pilier 6).
//
// ARCHITECTURE — système DÉCOUPLÉ du backend de dessin (décision Rihen,
// 2026-07-24) : NKCanvas n'est PAS un client de NKRHI (backends/devices
// autonomes, cf. NKCanvas/Renderer/Resources/NkShader.h : « PAS DE DEPENDANCE
// A NKRHI »). Dans une fenêtre de JEU possédée par NKRHI/NKRenderer, le HUD
// de production ne pourra donc PAS dessiner via NKCanvas (deux devices sur la
// même fenêtre = conflit de swapchain) — il passera par le chemin RHI. D'où
// la structure en 3 étages :
//   1. `NkUISystem` (ce fichier) : parcourt les entités UI, fait le LAYOUT
//      (anchor/pivot/anchoredPos/sizeDelta -> rect écran) et produit une
//      LISTE DE PRIMITIVES ABSTRAITE (`NkUIDrawCmd` : rectangles colorés +
//      lignes de texte) — zéro dépendance à un moteur de dessin.
//   2. `NkUIDrawBackend` : interface minimale consommée par le replay
//      (Begin/DrawRect/DrawTextLine + métriques texte pour l'alignement).
//   3. `NkUISoftwareCanvasBackend` : implémentation d'AUJOURD'HUI, 100 %
//      headless — délègue aux primitives RÉELLES du backend **Software** de
//      NKCanvas (`NkSoftwareFramebuffer`, header-inline, rendu en mémoire
//      CPU : FillRect/DrawPoint/Clear/GetPixel) et au rasterizer CPU réel de
//      NKFont (atlas alpha8, police embarquée ProggyClean). AUCUNE fenêtre,
//      AUCUN device GPU (contrainte matérielle : extinctions thermiques sur
//      pics GPU). La CIBLE DE PRODUCTION (fenêtre de jeu) est un backend
//      posé sur `renderer::NkRender2D` (Kernel/Runtime/NKRenderer/Tools/
//      Render2D — rendu 2D RÉEL déjà livré sur le device NKRHI, même device
//      que la scène 3D donc zéro conflit de swapchain) : les commandes
//      ci-dessous (rects pleins/bordures = 4 rects, texte à la baseline)
//      mappent trivialement sur son API (FillRect/DrawRect/DrawImage/tint) ;
//      il implémentera la même interface sans toucher au système.
//
// POURQUOI PAS `NkSoftwareContext` (le contexte Software complet de
// NKCanvas) : son Initialize() exige une NkWindow native valide
// (GetSurfaceDesc + presenter HWND/DIB) — pas headless. `NkSoftwareFramebuffer`
// est exactement le buffer dans lequel ce contexte dessine, utilisé en direct.
//
// LIMITES ASSUMÉES (jalon "HUD simple", documentées honnêtement) :
//   - Pas de hiérarchie de rects (chaque NkRectTransform est ancré au canvas
//     entier), pas de localScale/localRotation, un seul canvas pris en compte.
//   - NkUIImage.textureId/9-slice/Tiled/Radial ignorés -> rect de couleur
//     unie (type Filled supporté en horizontal/vertical pour les jauges).
//   - Texte : taille native de l'atlas (ProggyClean 13 px bitmap) —
//     NkUIText.fontSize/bold/italic/outline/shadow ignorés ; une seule ligne,
//     troncature simple au bord droit du rect.
//   - NkUIProgressBar.showLabel/labelFormat non rendus ; boutons/sliders/
//     toggles : aucune entrée souris/clavier branchée dans ce jalon.
//   - Ordre de dessin : ordre d'itération des archétypes ; au sein d'une
//     entité : Panel < Image < ProgressBar < Texte ; pas de z-order par
//     widget (seul NkCanvas.sortingOrder existe côté données).
// =============================================================================

#include "NKECS/System/NkSystem.h"
#include "NKECS/World/NkWorld.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
// NkRenderComponents.h DOIT précéder NkUIComponent.h : il définit ecs::NkColor4
// et ecs::NkBlendMode que NkUIComponent.h référence sans les redéclarer (même
// ordre que l'agrégat Noge/Nkentseu.h).
#include "Noge/ECS/Components/Rendering/NkRenderComponents.h"
#include "Noge/ECS/Components/UI/NkUIComponent.h"
#include "NKCanvas/Backend/Software/NkSoftwareContext.h" // NkSoftwareFramebuffer (CPU, header-inline)
#include "NKFont/NkFont.h"
#include "NKFont/Embedded/NkFontEmbedded.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {

	// =========================================================================
	// NkUIDrawCmd — primitive abstraite produite par NkUISystem
	// =========================================================================
	struct NkUIDrawCmd {
			enum class Kind : uint8 { Rect = 0, Text = 1 };

			Kind kind = Kind::Rect;
			// Rect : (x, y, w, h) = rectangle en pixels framebuffer.
			// Text : x = bord gauche, y = BASELINE, w = clip droit (maxX), h = 0.
			float32 x = 0.f, y = 0.f, w = 0.f, h = 0.f;
			ecs::NkColor4 color = {};
			uint32 textOffset = 0u; // Kind::Text : offset dans l'arène texte du système
	};

	// =========================================================================
	// NkUIDrawBackend — interface de dessin consommée par le replay
	// =========================================================================
	class NkUIDrawBackend {
		public:
			virtual ~NkUIDrawBackend() = default;

			[[nodiscard]] virtual uint32 Width() const noexcept = 0;
			[[nodiscard]] virtual uint32 Height() const noexcept = 0;

			/// Début de frame HUD (efface avec la couleur donnée, RGBA 0..255).
			virtual void Begin(const uint8 clearColor[4]) noexcept = 0;

			/// Rectangle plein (pixels framebuffer, couleur float 0..1).
			virtual void DrawRect(float32 x, float32 y, float32 w, float32 h, const ecs::NkColor4 &c) noexcept = 0;

			/// Ligne de texte à la baseline (troncature à maxX). No-op si texte
			/// non supporté par ce backend.
			virtual void DrawTextLine(float32 x, float32 baselineY, const char *text, const ecs::NkColor4 &c,
									  float32 maxX) noexcept = 0;

			// Métriques texte (utilisées par le LAYOUT du système pour les
			// alignements H/V). Un backend sans texte renvoie false/0.
			[[nodiscard]] virtual bool TextSupported() const noexcept = 0;
			[[nodiscard]] virtual float32 TextWidth(const char *text) const noexcept = 0;
			[[nodiscard]] virtual float32 TextLineHeight() const noexcept = 0;
			[[nodiscard]] virtual float32 TextAscent() const noexcept = 0;
	};

	// =========================================================================
	// NkUISoftwareCanvasBackend — backend headless NKCanvas-Software + NKFont
	// =========================================================================
	class NkUISoftwareCanvasBackend final : public NkUIDrawBackend {
		public:
			NkUISoftwareCanvasBackend() noexcept = default;
			~NkUISoftwareCanvasBackend() noexcept override;

			NkUISoftwareCanvasBackend(const NkUISoftwareCanvasBackend &) = delete;
			NkUISoftwareCanvasBackend &operator=(const NkUISoftwareCanvasBackend &) = delete;

			/// Alloue le framebuffer CPU et (optionnel) l'atlas NKFont embarqué.
			bool Init(uint32 width, uint32 height, bool enableText) noexcept;
			void Shutdown() noexcept;

			// ── NkUIDrawBackend ──────────────────────────────────────────────
			[[nodiscard]] uint32 Width() const noexcept override {
				return mFB.width;
			}

			[[nodiscard]] uint32 Height() const noexcept override {
				return mFB.height;
			}

			void Begin(const uint8 clearColor[4]) noexcept override;
			void DrawRect(float32 x, float32 y, float32 w, float32 h, const ecs::NkColor4 &c) noexcept override;
			void DrawTextLine(float32 x, float32 baselineY, const char *text, const ecs::NkColor4 &c,
							  float32 maxX) noexcept override;

			[[nodiscard]] bool TextSupported() const noexcept override {
				return mTextReady;
			}

			[[nodiscard]] float32 TextWidth(const char *text) const noexcept override;
			[[nodiscard]] float32 TextLineHeight() const noexcept override;
			[[nodiscard]] float32 TextAscent() const noexcept override;

			// ── Inspection headless (assertions de pixels, capture) ──────────
			[[nodiscard]] NkSoftwareFramebuffer &Framebuffer() noexcept {
				return mFB;
			}

			/// Couleur logique RGBA du pixel (x,y) — le swizzle octet-natif
			/// (BGRA Windows / RGBA ailleurs) est résolu par NkSWPixel::LoadPixel.
			[[nodiscard]] math::NkColor GetPixel(uint32 x, uint32 y) noexcept {
				return mFB.GetPixel(x, y);
			}

			/// Sauvegarde le framebuffer en PNG (NKImage, codec réel).
			bool SavePNG(const char *path) const noexcept;

		private:
			NkSoftwareFramebuffer mFB; // framebuffer CPU NKCanvas (backend Software)
			NkFontAtlas mAtlas;		   // atlas CPU NKFont (possède mFont)
			NkFont *mFont = nullptr;   // police embarquée (non possédée, vit dans mAtlas)
			const nkft_uint8 *mAtlasPixels = nullptr; // texels alpha8 (possédés par mAtlas)
			nkft_int32 mAtlasW = 0;
			nkft_int32 mAtlasH = 0;
			bool mReady = false;
			bool mTextReady = false;
	};

	// =========================================================================
	// NkUISystem — le système ECS (layout + liste de primitives + replay)
	// =========================================================================
	class NkUISystem final : public ecs::NkSystem {
		public:
			NkUISystem() noexcept = default;
			~NkUISystem() noexcept override;

			NkUISystem(const NkUISystem &) = delete;
			NkUISystem &operator=(const NkUISystem &) = delete;

			/**
			 * @brief Commodité headless : crée et possède un
			 *        NkUISoftwareCanvasBackend (framebuffer CPU + texte NKFont).
			 *
			 * 100 % headless : aucune fenêtre, aucun device GPU. Si la police
			 * embarquée n'est pas disponible dans ce build, le HUD reste
			 * fonctionnel (rects/panels/barres) et TextReady() renvoie false.
			 */
			bool Init(uint32 width, uint32 height, bool enableText = true) noexcept;

			/**
			 * @brief Injecte un backend de dessin EXTERNE (non possédé) — le
			 *        chemin prévu pour un futur NkUIRHIBackend en fenêtre de jeu.
			 *        Libère l'éventuel backend Software possédé.
			 */
			void SetBackend(NkUIDrawBackend *backend) noexcept;

			/// Libère le backend possédé (idempotent).
			void Shutdown() noexcept;

			[[nodiscard]] ecs::NkSystemDesc Describe() const override {
				return ecs::NkSystemDesc{}
					.Writes<ecs::NkRectTransform>() // rect calculé écrit (rôle layout)
					.Writes<ecs::NkUIProgressBar>() // displayedValue animé
					.Reads<ecs::NkCanvas>()
					.Reads<ecs::NkUIPanel>()
					.Reads<ecs::NkUIImage>()
					.Reads<ecs::NkUIText>()
					.InGroup(ecs::NkSystemGroup::Render)
					.Sequential()
					.Named("NkUISystem");
			}

			/// Layout -> liste de primitives -> replay sur le backend courant.
			void Execute(ecs::NkWorld &world, float32 dt) override;

			/// Couleur de fond du HUD (Begin/Clear en début d'Execute).
			void SetClearColor(uint8 r, uint8 g, uint8 b, uint8 a = 255u) noexcept {
				mClear[0] = r;
				mClear[1] = g;
				mClear[2] = b;
				mClear[3] = a;
			}

			[[nodiscard]] bool IsReady() const noexcept {
				return mBackend != nullptr;
			}

			/// true si le backend courant rend réellement le texte.
			[[nodiscard]] bool TextReady() const noexcept {
				return mBackend && mBackend->TextSupported();
			}

			/// Backend de dessin courant (possédé ou injecté).
			[[nodiscard]] NkUIDrawBackend *Backend() noexcept {
				return mBackend;
			}

			/// Backend Software possédé (pixels/PNG) — nullptr si un backend
			/// externe a été injecté via SetBackend().
			[[nodiscard]] NkUISoftwareCanvasBackend *Software() noexcept {
				return mOwnedSoftware;
			}

			// ── Liste de primitives de la DERNIÈRE frame (inspection/tests) ──
			[[nodiscard]] const NkVector<NkUIDrawCmd> &Commands() const noexcept {
				return mCmds;
			}

			/// Texte d'une commande Kind::Text (pointeur dans l'arène interne,
			/// valide jusqu'au prochain Execute()).
			[[nodiscard]] const char *CommandText(const NkUIDrawCmd &cmd) const noexcept {
				return (cmd.kind == NkUIDrawCmd::Kind::Text && cmd.textOffset < mTextArena.Size())
						   ? mTextArena.Data() + cmd.textOffset
						   : "";
			}

		private:
			// Layout : anchor/pivot/anchoredPos/sizeDelta -> rect (pixels de
			// référence canvas). min==max -> point d'ancrage + taille fixe ;
			// min!=max -> étirement entre les deux ancres (+ sizeDelta en marge).
			static void ComputeRect(const ecs::NkRectTransform &rt, float32 refW, float32 refH, float32 &outX,
									float32 &outY, float32 &outW, float32 &outH) noexcept;

			void PushRect(float32 x, float32 y, float32 w, float32 h, const ecs::NkColor4 &c) noexcept;
			// Cadre d'épaisseur `t` = 4 commandes Rect.
			void PushStroke(float32 x, float32 y, float32 w, float32 h, float32 t, const ecs::NkColor4 &c) noexcept;
			void PushText(float32 x, float32 baselineY, const char *text, const ecs::NkColor4 &c,
						  float32 maxX) noexcept;

			NkUIDrawBackend *mBackend = nullptr;			  // backend courant (jamais nul si IsReady())
			NkUISoftwareCanvasBackend *mOwnedSoftware = nullptr; // possédé (NKMemory) si Init() a été utilisé
			NkVector<NkUIDrawCmd> mCmds;					  // primitives de la frame
			NkVector<char> mTextArena;						  // stockage des textes (offsets)
			uint8 mClear[4] = {0u, 0u, 0u, 255u};
	};

} // namespace nkentseu
