#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiFont.h
// @Brief   Police NKGui — wrapper mince sur NKFont (atlas + glyphes). Phase 3.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// Charge une police (embarquée ou fichier), construit l'atlas, expose la face
// NKFont + l'atlas alpha8 à uploader par le backend (texId stable).
// -----------------------------------------------------------------------------

#include "NKGui/NkGuiExport.h"
#include "NKGui/Core/NkGuiTypes.h"
#include "NKFont/NkFont.h"
#include "NKFont/Embedded/NkFontEmbedded.h"

namespace nkentseu {
	namespace nkgui {

		struct NKENTSEU_NKGUI_CLASS_EXPORT NkGuiFont {
				NkFontAtlas atlas;			///< possède la texture + les glyphes
				NkFont *face = nullptr;		///< face produite (détenue par l'atlas)
				uint32 texId = 0x4E4B4654u; ///< 'NKFT' — id stable pour le backend
				uint8 *pixels = nullptr;	///< atlas alpha8 (détenu par l'atlas)
				int32 atlasW = 0;
				int32 atlasH = 0;
				bool dirty = false; ///< à (ré)uploader côté backend

				// (26/10) LES IDEOGRAMMES A LA DEMANDE : ce que la police a charge (pour se refaire a l'identique,
				// ideogrammes compris), et l'etat de son repli CJK
				bool depuisFichier = false;
				NkEmbeddedFontId idCharge = NkEmbeddedFontId::Inter;
				char cheminCharge[600] = {};
				float32 tailleChargee = 0.f;
				bool replisExternes = false;
				bool cjkCharge = false;	 ///< l'atlas porte les ideogrammes
				bool cjkDemande = false; ///< un texte affiche en a demande (NkGuiPolicesSignalerTexte)

				NkGuiFont() = default;
				~NkGuiFont();
				NkGuiFont(const NkGuiFont &) = delete;
				NkGuiFont &operator=(const NkGuiFont &) = delete;

				// Charge une police embarquée à la taille donnée + construit l'atlas.
				// extFallback=false : n'incorpore AUCUN repli externe (broad/CJK/emoji = plusieurs
				// milliers de glyphes) — pour les polices MONOSPACE (code) : atlas minuscule =>
				// reconstruction RAPIDE au zoom (l'interface, elle, garde les replis complets).
				bool LoadEmbedded(NkEmbeddedFontId id, float32 sizePx, bool extFallback = true) noexcept;

				// Charge une police depuis un fichier TTF/OTF (ex. police système Windows).
				// Rechargeable : réinitialise l'atlas avant. Conserve `texId` (ré-upload).
				bool LoadFromFile(const char *path, float32 sizePx, bool extFallback = true) noexcept;

				const NkFont *Face() const noexcept {
					return face;
				}

				uint32 TexId() const noexcept {
					return texId;
				}

				bool Valid() const noexcept {
					return face != nullptr;
				}

				float32 Ascent() const noexcept {
					return face ? face->ascent : 0.f;
				}

				float32 Descent() const noexcept {
					return face ? face->descent : 0.f;
				}

				float32 LineHeight() const noexcept {
					return face ? face->lineAdvance : 0.f;
				}

				float32 MeasureWidth(const char *s) const noexcept {
					return (face && s) ? face->CalcTextSizeX(s) : 0.f;
				}

				/// Mesure la sous-chaine [s, end) — meme moteur, `CalcTextSizeX`
				/// accepte deja une borne. Sert a la convention `##id` des
				/// libelles : on mesure la partie AFFICHEE, jamais l'identifiant.
				float32 MeasureWidth(const char *s, const char *end) const noexcept {
					return (face && s) ? face->CalcTextSizeX(s, end) : 0.f;
				}
		};

		// Polices de REPLI EXTERNES (fichiers .ttf charges au runtime) : tout glyphe
		// absent des polices principales (Inter/DejaVu) y est cherche. Roles :
		//   broad : large couverture (latin etendu, grec, cyrillique, hebreu, arabe, symboles)
		//   cjk   : ideogrammes (中文/日本語/한국어) — volumineux, opt-in
		//   emoji : emoji monochrome
		// A poser par l'APPLICATION (ex. NKCode, depuis son dossier data/fonts) AVANT
		// de charger les polices. Un chemin nullptr/vide = role desactive.
		NKENTSEU_NKGUI_API void NkSetFallbackFontPaths(const char *broad, const char *cjk, const char *emoji) noexcept;

		// ── (26/10) LES IDEOGRAMMES (CJK) A LA DEMANDE ─────────────────────────────
		// Mesure du 26/10 (NKCraft, tests/sonde_demarrage.py) : l'atlas d'Inter avec le repli CJK coutait ~570 ms
		// avant la premiere image, pour des ideogrammes que l'interface affiche rarement (un nom de fichier chinois).
		// Desormais, une police a replis externes se construit SANS les ideogrammes ; le premier texte AFFICHE qui en
		// contient (NkGuiDrawList, NkGuiPolicesSignalerTexte) les demande, et l'atlas se refait ENTRE deux images
		// (NkGuiPolicesAppliquerDemandes, appele en debut d'image par le rendu) : le texte s'affiche juste des
		// l'image suivante. Le texte latin est rendu a l'identique (memes glyphes, meme rasterisation).
		// `NK_POLICE_CJK=immediat` : l'ancien chemin (les ideogrammes des le chargement).
		NKENTSEU_NKGUI_API void NkSetCjkALaDemande(bool aLaDemande) noexcept;
		NKENTSEU_NKGUI_API bool NkCjkALaDemande() noexcept;
		/// Un texte va etre affiche avec `face` : s'il porte un ideogramme absent de la face, la police qui la possede
		/// le demande. Cout : un passage sur les octets du texte, rien sans octet >= 0xE2.
		NKENTSEU_NKGUI_API void NkGuiPolicesSignalerTexte(const NkFont *face, const char *debut, const char *fin) noexcept;
		/// Refait les polices qui ont demande les ideogrammes ; `televerser` renvoie chaque atlas refait au GPU (meme
		/// texId). Rend le nombre de polices refaites. A appeler hors d'une image (aucune face n'est en usage).
		using NkGuiTeleverserPolice = bool (*)(uint32 texId, const uint8 *pixels, int32 w, int32 h, void *user);
		NKENTSEU_NKGUI_API uint32 NkGuiPolicesAppliquerDemandes(NkGuiTeleverserPolice televerser, void *user) noexcept;

	} // namespace nkgui
} // namespace nkentseu
