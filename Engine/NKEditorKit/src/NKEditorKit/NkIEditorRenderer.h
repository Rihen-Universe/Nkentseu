#pragma once
// -----------------------------------------------------------------------------
// @File    NkIEditorRenderer.h
// @Brief   Interface du backend de RENDU de la coquille d'editeur.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// NkEditorShell delegue TOUT le rendu (creation du contexte GPU, frame, soumission
// des draw-lists NKGui, upload des atlas de police/images) a un NkIEditorRenderer.
// Ainsi la coquille (menus / docking / palette / panneaux) est INDEPENDANTE du
// systeme de rendu :
//   - IDE (NKCode)            -> impl NKCanvas (NkEditorCanvasRenderer, defaut),
//   - app anim / moteur de jeu-> impl NKRHI/NKRenderer (fournie par l'app).
//
// L'interface n'expose QUE des types NKGui / NKWindow purs : NKEditorKit reste
// « 2D pur » (aucune dependance NKRHI ni NKRenderer ; l'impl NKRHI vit AILLEURS).
// -----------------------------------------------------------------------------

#include "NKCore/NkConfig.h" // NKENTSEU_PLATFORM_* : lus par NkEditorGfxApiSupported
#include "NKEditorKit/NkEditorExport.h"
#include "NKGui/Core/NkGuiDrawList.h"
#include "NKMath/NKMath.h" // math::NkVec2u

namespace nkentseu {

	class NkWindow; // forward (NKWindow)

	namespace editorkit {

		// ── LE CHOIX D'API GRAPHIQUE ────────────────────────────────────────────
		// Choix d'API graphique NEUTRE (decouple de NKCanvas ET NKRHI : leurs enums
		// NkGraphicsApi se dupliquent dans le namespace nkentseu et ne peuvent
		// cohabiter dans un meme TU). Chaque impl mappe vers son propre enum.
		//
		// ⚠️ ELLE PORTE LES CINQ APIs DE LA DIRECTIVE DE RODOLF (2026-08-18) :
		//    « pour toutes nos applications, on doit pouvoir choisir le backend
		//    graphique entre ceux disponibles : OpenGL, Vulkan, DX11, DX12 et
		//    Metal. » `Metal` a ete ajoute le 2026-08-18 ; son absence rendait la
		//    directive intenable et deux agents l'avaient signalee sans pouvoir y
		//    toucher. `Software` s'ajoute a la liste : c'est un backend du depot,
		//    pas une demande de la directive.
		//
		// ⚠️ INSERE AU MILIEU, ET C'EST MESURE, PAS SUPPOSE : rien ne serialise
		//    cette enumeration (aucun `Save`/`Load`, trois sites d'affectation, tous
		//    en code). Elle n'est donc PAS append-only, contrairement a `NkRole`
		//    qui est ecrit dans un fichier de theme. `Metal` est donc range la ou
		//    la directive le nomme, apres DX12, et non jete a la fin.
		enum class NkEditorGfxApi : uint8 { Auto = 0, OpenGL, Vulkan, DX11, DX12, Metal, Software };

		// ── LE NOM TEXTUEL ──────────────────────────────────────────────────────
		// UN SEUL VOCABULAIRE POUR TOUT LE DEPOT. La regle 1 de la directive
		// demande « le meme nom d'option et la meme variable d'environnement dans
		// toutes les applications, pas un vocabulaire par application ». Mesure du
		// 18/08 : quatre applications, quatre vocabulaires — `--gfx=` chez
		// NKUIDesign, `-b<backend>` chez NkAnimaEditor, rien chez NK3DModeler ni
		// chez Nogee. Le vocabulaire vit donc ICI, avec l'enumeration qu'il nomme.
		//
		// `Auto` n'est pas une API : c'est une DELEGATION. Elle a quand meme un nom,
		// parce qu'elle est une valeur d'option legitime.
		inline const char *NkEditorGfxApiName(NkEditorGfxApi api) {
			switch (api) {
				case NkEditorGfxApi::OpenGL:
					return "opengl";
				case NkEditorGfxApi::Vulkan:
					return "vulkan";
				case NkEditorGfxApi::DX11:
					return "dx11";
				case NkEditorGfxApi::DX12:
					return "dx12";
				case NkEditorGfxApi::Metal:
					return "metal";
				case NkEditorGfxApi::Software:
					return "software";
				default:
					return "auto";
			}
		}

		/// Liste des valeurs acceptees, pour un message d'aide ou d'erreur. Une
		/// seule chaine, au meme endroit que la table : ainsi elle ne peut pas
		/// vieillir a part.
		inline const char *NkEditorGfxApiChoices() {
			return "auto|opengl|vulkan|dx11|dx12|metal|software";
		}

		// ── L'ANALYSE ───────────────────────────────────────────────────────────
		// Rend `false` sur une valeur inconnue, et alors `out` N'EST PAS TOUCHE.
		// ⚠️ Ce detail est la regle 3 de la directive en miniature : une valeur
		//    inconnue qui ramenerait silencieusement `out` a `Auto` ferait croire a
		//    l'appelant qu'il a obtenu ce qu'il demandait. On refuse, on ne
		//    remplace pas. Tenu par l'essai 4d du banc NKEditorKitTest.
		inline bool NkEditorGfxApiFromName(const char *name, NkEditorGfxApi &out) {
			if (!name || !*name)
				return false;
			// Comparaison locale : cet en-tete reste sans dependance de chaine.
			struct Local {
					static bool Eq(const char *a, const char *b) {
						for (; *a && *b; ++a, ++b)
							if (*a != *b)
								return false;
						return *a == *b;
					}
			};
			static const NkEditorGfxApi kAll[] = {
				NkEditorGfxApi::Auto,  NkEditorGfxApi::OpenGL, NkEditorGfxApi::Vulkan,
				NkEditorGfxApi::DX11,  NkEditorGfxApi::DX12,   NkEditorGfxApi::Metal,
				NkEditorGfxApi::Software,
			};
			for (uint32 i = 0; i < sizeof(kAll) / sizeof(kAll[0]); ++i)
				if (Local::Eq(name, NkEditorGfxApiName(kAll[i]))) {
					out = kAll[i];
					return true;
				}
			return false;
		}

		// ── LA DISPONIBILITE, ET SA RAISON ──────────────────────────────────────
		// ⚠️ C'EST LA MOITIE QUI COMPTE. Ajouter `Metal` a l'enumeration sans ceci
		//    aurait produit exactement ce que la directive interdit : sur Windows,
		//    `Metal` serait tombe dans le `default:` des deux implantations et
		//    aurait lance DX11 EN SILENCE. Une entree qui retombe sans le dire est
		//    pire que son absence — l'absence force a chercher, la presence
		//    dispense de verifier.
		//
		// `outReason` recoit, en cas de refus, la phrase a journaliser. Elle n'est
		// PAS ecrite par l'appelant : une raison recopiee dans quatre applications
		// finit par diverger dans trois d'entre elles.
		//
		// Ce qu'elle NE dit PAS, et il faut le savoir pour ne pas s'y fier a tort :
		// elle repond « cette plateforme porte-t-elle cette API », pas « cette
		// machine a-t-elle le pilote ». Un poste Windows sans SDK Vulkan passera
		// ici et echouera a la creation du contexte — c'est le role de
		// `NkIEditorRenderer::Init` de le dire, pas le sien.
		inline bool NkEditorGfxApiSupported(NkEditorGfxApi api, const char **outReason = nullptr) {
			if (outReason)
				*outReason = "";
			switch (api) {
				case NkEditorGfxApi::Metal:
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
					return true;
#else
					if (outReason)
						*outReason = "Metal n'existe que sur les plateformes Apple (macOS, iOS) ; "
									 "lancer une autre API a sa place serait un repli silencieux";
					return false;
#endif
				case NkEditorGfxApi::DX11:
				case NkEditorGfxApi::DX12:
#if defined(NKENTSEU_PLATFORM_WINDOWS)
					return true;
#else
					if (outReason)
						*outReason = "Direct3D n'existe que sur Windows ; lancer une autre API a sa "
									 "place serait un repli silencieux";
					return false;
#endif
				default:
					// Auto, OpenGL, Vulkan, Software : portes partout ou le depot
					// construit. Leur echec eventuel est un echec de PILOTE, pas de
					// plateforme, et il se dit a la creation du contexte.
					return true;
			}
		}

		// Backend de rendu de la coquille. Possede le contexte GPU + le backend de
		// draw-lists NKGui. La FENETRE reste possedee par le shell (passee a Init).
		class NKEDITORKIT_API NkIEditorRenderer {
			public:
				virtual ~NkIEditorRenderer() = default;

				// Cree le contexte GPU lie a `window` + le backend de draw-lists.
				// `api` = API demandee (Auto -> choix par defaut de l'impl).
				virtual bool Init(NkWindow &window, NkEditorGfxApi api) = 0;
				virtual void Shutdown() = 0;
				virtual bool IsValid() const = 0;

				// Taille du framebuffer courant (px).
				virtual math::NkVec2u Size() const = 0;
				virtual void OnResize(uint32 width, uint32 height) = 0;

				// Cycle de frame : BeginFrame (begin + clear) -> SubmitDrawList(s) -> EndFrame (present).
				virtual void BeginFrame() = 0;
				virtual void SubmitDrawList(const nkgui::NkGuiDrawList &dl, uint32 fbW, uint32 fbH) = 0;
				virtual void EndFrame() = 0;

				// Upload des textures referencees par texId dans les draw-lists.
				// Gray8 = atlas de police (etendu en RGBA blanc + alpha) ; RGBA8 = image.
				virtual bool UploadFontGray8(uint32 texId, const uint8 *pixels, int32 w, int32 h) = 0;
				virtual bool UploadImageRGBA(uint32 texId, const uint8 *pixels, int32 w, int32 h) = 0;
		};

	} // namespace editorkit
} // namespace nkentseu
