// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKImage/Core/NkTextureOven.h — LE FOUR
// =============================================================================
// Une image decodee (PNG, JPEG, TGA, HDR…) -> le payload d'un actif `.nktex` :
// pixels DEJA dans la disposition que le GPU accepte, mipmaps precalculees.
//
// ── POURQUOI CE FICHIER EST UN EN-TETE SEUL, ET OU IL VIT ────────────────────
// Le four est un PONT entre deux modules : `NKImage` sait decoder et reduire,
// `NKSerialization` sait ecrire un actif. Aucun des deux ne doit dependre de
// l'autre pour ça — NKSerialization tirerait douze codecs d'image, NKImage
// tirerait la reflexion. Le pont est donc un en-tete seul, range du cote des
// pixels, et c'est SON UTILISATEUR (l'outil de cuisson, le banc, l'editeur) qui
// met les deux modules sur son chemin d'inclusion. Rien n'est compile dans
// `NKImage.lib` : la dependance n'existe que chez celui qui inclut ce fichier.
//
// ⚠️ Une seule copie de cette logique. Le banc et l'outil de production doivent
// eprouver le MEME chemin — un four ecrit deux fois diverge au premier correctif.
//
// ── CE QUE LE FOUR CHANGE, ET POURQUOI ───────────────────────────────────────
// Une image a 3 canaux (RGB24) N'EST PAS televersable : `NkGPUFormat` n'a aucun
// format RGB8, et `NkFormatBytesPerPixel` rendrait 0. C'est d'ailleurs ce que
// fait deja `NkTextureLibrary::LoadWithNKImage` a chaque chargement — il
// convertit en RGBA8 dense. Le four fait donc la meme conversion UNE FOIS, a la
// cuisson, au lieu de la refaire a chaque lancement. C'est exactement la raison
// d'etre d'un format d'actif : le travail se paie au four, pas au demarrage.
// =============================================================================
#pragma once

#ifndef NKENTSEU_IMAGE_CORE_NKTEXTUREOVEN_H
#define NKENTSEU_IMAGE_CORE_NKTEXTUREOVEN_H

#include "NKImage/Core/NkBlockCompress.h"
#include "NKImage/Core/NkImage.h"
#include "NKSerialization/Asset/NkTextureAssetFormat.h"

#include <cstdio>
#include <cstring>
#include <utility>

namespace nkentseu {

	// =========================================================================
	// Reglages de cuisson
	// =========================================================================
	// 🔴 CES DEFAUTS SONT CEUX DE `NkLoadOptions` (NKRenderer), ET CE N'EST PAS
	// UNE COINCIDENCE. L'empreinte du cache porte ces quatre champs : si le four
	// pre-cuit avec `filterMode = LINEAR` pendant que le moteur cherche sous
	// `ANISO`, la pre-cuisson n'est jamais trouvee — **et rien ne le signale**,
	// parce que chacun cherche un fichier que l'autre n'ecrit pas. Le symptome
	// serait « la pre-cuisson ne sert a rien », jamais une erreur.
	//
	// Un banc garde cet accord (`NKRenderer_TextureAsset_Tests`, temoin
	// « accord four/moteur ») : il compare champ par champ ce que
	// `NkTextureLibrary::ReglagesDepuisOptions(NkLoadOptions{})` produit avec
	// `NkTexOvenReglages{}`. Toucher a l'un sans l'autre le fait rougir.
	struct NkTexOvenReglages {
			// Vrai pour une carte de COULEUR (albedo, emission) — l'echantillonnage
			// devra deliner. Faux pour une donnee (normale, rugosite, metallique,
			// occlusion) : la deliner l'abimerait.
			bool sRGB = true;                            // NkLoadOptions::srgb
			bool genererMips = true;                     // NkLoadOptions::genMipmaps
			nk_uint32 addressMode = NKTEXADDR_REPEAT;    // NkLoadOptions::useClampEdge == false
			nk_uint32 filterMode = NKTEXFILTER_ANISO;    // NkLoadOptions::useAnisotropic == true

			// ── COMPRESSION PAR BLOCS ────────────────────────────────────────
			// `NKTEXFMT_INCONNU` (le defaut) = pas de compression, pixels bruts.
			// Sinon, le code du format vise — seul `NKTEXFMT_BC1_RGB_UNORM` /
			// `_SRGB` est livre aujourd'hui.
			//
			// ⚠️ CE CHAMP N'ENTRE PAS DANS LES DEFAUTS PARTAGES AVEC
			// `NkLoadOptions` : le moteur ne compresse jamais tout seul. BC1 est
			// AVEC PERTE et SANS ALPHA — l'appliquer d'office abimerait les cartes
			// de normales (les artefacts de bloc s'y voient comme des facettes) et
			// mangerait l'alpha des textures qui en ont un. Ça se demande.
			nk_uint32 compression = NKTEXFMT_INCONNU;
	};

	// =========================================================================
	// NkTextureOven
	// =========================================================================
	class NkTextureOven {
		public:
			// Le format de pixel que le GPU acceptera, pour une image donnee.
			// Rend NKTEXFMT_INCONNU si l'image doit d'abord etre convertie.
			[[nodiscard]] static nk_uint32 CodeFormat(NkImagePixelFormat f, bool srgb) noexcept {
				switch (f) {
					case NkImagePixelFormat::NK_GRAY8:
						return NKTEXFMT_R8_UNORM;
					case NkImagePixelFormat::NK_GRAY_A16:
						return NKTEXFMT_RG8_UNORM;
					case NkImagePixelFormat::NK_RGBA32:
						return srgb ? nk_uint32(NKTEXFMT_RGBA8_SRGB) : nk_uint32(NKTEXFMT_RGBA8_UNORM);
					case NkImagePixelFormat::NK_RGBA128F:
						return NKTEXFMT_RGBA32_FLOAT;
					case NkImagePixelFormat::NK_RGB96F:
						return NKTEXFMT_RGB32_FLOAT;
					default:
						// NK_RGB24 tombe ici EXPRES : il faut le convertir en
						// RGBA32 avant, le GPU n'a pas de format a 3 octets.
						return NKTEXFMT_INCONNU;
				}
			}

			// Le format vers lequel convertir une image que le GPU refuserait.
			// Rend le format d'entree si aucune conversion n'est necessaire.
			[[nodiscard]] static NkImagePixelFormat FormatTeleversable(NkImagePixelFormat f) noexcept {
				switch (f) {
					case NkImagePixelFormat::NK_RGB24:
						return NkImagePixelFormat::NK_RGBA32; // pas de RGB8 cote GPU
					case NkImagePixelFormat::NK_RGB96F:
						return NkImagePixelFormat::NK_RGB96F; // NK_RGB32_FLOAT existe
					default:
						return f;
				}
			}

			// Recopie les lignes d'une image SERREES : `NkImage` aligne son pas de
			// ligne sur 4 octets, le fichier porte un pas explicite et sans trou.
			static void SerrerLignes(const NkImage &img, NkVector<nk_uint8> &out) noexcept {
				const nk_size pas = nk_size(img.Width()) * nk_size(BytesPerPixelOf(img.Format()));
				out.Clear();
				out.Resize(pas * nk_size(img.Height()));
				for (int32 y = 0; y < img.Height(); ++y)
					memcpy(out.Data() + nk_size(y) * pas, img.RowPtr(y), pas);
			}

			// Le format demande est-il livre ? Rend une raison si non — un refus
			// muet ferait croire a une compression appliquee.
			[[nodiscard]] static const char *RaisonRefusCompression(nk_uint32 code, NkImagePixelFormat fmt) noexcept {
				if (code == NKTEXFMT_INCONNU)
					return nullptr; // pas de compression demandee
				if (code != nk_uint32(NKTEXFMT_BC1_RGB_UNORM) && code != nk_uint32(NKTEXFMT_BC1_RGB_SRGB))
					return "seul BC1 est livre : BC7, ETC2 et ASTC sont nommes, pas faits";
				if (fmt != NkImagePixelFormat::NK_RGBA32 && fmt != NkImagePixelFormat::NK_RGB24)
					return "BC1 ne prend que des images 8 bits par canal (ni HDR, ni gris)";
				return nullptr;
			}

			// ── LE FOUR ──────────────────────────────────────────────────────
			// `source` : une image decodee. `out` : le payload d'un `.nktex`.
			// L'appelant l'ecrit ensuite dans un actif par `NkAssetIO::Write`.
			[[nodiscard]] static nk_bool Cuire(const NkImage &source, const NkTexOvenReglages &reglages,
											   NkVector<nk_uint8> &out, NkString *err = nullptr) noexcept {
				out.Clear();
				if (!source.IsValid())
					return Refus(err, "image source invalide");

				// 1. Amener l'image dans un format que le GPU accepte.
				const NkImagePixelFormat cible = FormatTeleversable(source.Format());
				NkImage travail = (cible == source.Format()) ? source.Copy() : source.Convert(cible);
				if (!travail.IsValid())
					return Refus(err, "conversion vers un format televersable impossible");

				const nk_uint32 code = CodeFormat(travail.Format(), reglages.sRGB);
				if (code == NKTEXFMT_INCONNU)
					return Refus(err, "format de pixel non cuisinable");
				const nk_uint32 bpp = nk_uint32(BytesPerPixelOf(travail.Format()));

				// 2. La chaine de niveaux. Les octets sont ranges d'abord, en
				//    entier, parce que les descripteurs pointeront dedans : le
				//    tampon ne doit plus bouger ensuite.
				const nk_uint32 nMax = reglages.genererMips
										   ? NkTexturePayload::CompteMipsComplet(nk_uint32(travail.Width()),
																				 nk_uint32(travail.Height()))
										   : 1u;
				NkVector<NkVector<nk_uint8>> octets;
				octets.Resize(nk_size(nMax));
				NkVector<nk_uint32> largeurs, hauteurs;

				{
					NkImage courant = std::move(travail);
					for (nk_uint32 i = 0; i < nMax; ++i) {
						SerrerLignes(courant, octets[i]);
						largeurs.PushBack(nk_uint32(courant.Width()));
						hauteurs.PushBack(nk_uint32(courant.Height()));
						if (i + 1u == nMax)
							break;
						NkImage suivant = courant.ReduceHalf();
						if (!suivant.IsValid())
							break;
						courant = std::move(suivant);
					}
				}

				// 2bis. COMPRESSION PAR BLOCS, si elle est demandee.
				// Chaque niveau est compresse separement : un mip n'est pas un
				// morceau du precedent, c'est une image a lui.
				nk_uint32 codeFinal = code;
				nk_uint32 bppFinal = bpp;
				bool compresse = false;
				if (reglages.compression != NKTEXFMT_INCONNU) {
					const char *refus = RaisonRefusCompression(reglages.compression, cible);
					if (refus)
						return Refus(err, refus);
					// L'encodeur BC1 veut du RGBA8 serre ; `cible` garantit deja
					// RGBA32 (le RGB24 a ete converti en 1.).
					for (nk_size i = 0; i < largeurs.Size(); ++i) {
						NkVector<nk_uint8> blocs;
						NkBlockCompress::EncoderBC1(octets[i].Data(), largeurs[i], hauteurs[i], blocs);
						octets[i] = std::move(blocs);
					}
					codeFinal = reglages.compression;
					bppFinal = 0u; // un format par blocs n'a pas d'octet-par-pixel
					compresse = true;
				}

				// 3. Assembler et encoder.
				const nk_size nNiveaux = largeurs.Size();
				NkTexCuisson cuisson;
				cuisson.formatCode = codeFinal;
				cuisson.width = largeurs[0];
				cuisson.height = hauteurs[0];
				cuisson.depth = 1u;
				cuisson.arrayLayers = 1u;
				cuisson.flags = (reglages.sRGB && NkTexFormatEstSrgb(codeFinal) ? nk_uint32(NKTEXFLAG_SRGB) : 0u) |
								(nNiveaux > 1u ? nk_uint32(NKTEXFLAG_MIPS_PRECALCULES) : 0u);
				cuisson.addressMode = reglages.addressMode;
				cuisson.filterMode = reglages.filterMode;
				cuisson.rowAlignment = 1u;
				for (nk_size i = 0; i < nNiveaux; ++i) {
					NkTexNiveauSource n;
					n.width = largeurs[i];
					n.height = hauteurs[i];
					n.depth = 1u;
					// Pas de ligne : une rangee de BLOCS quand c'est compresse.
					// Zero serait accepte par l'encodeur (il recalculerait
					// largeur x octets-par-pixel = 0) et produirait un actif vide.
					n.rowPitch = compresse ? nk_uint32(((largeurs[i] + 3u) / 4u) * NkBlockCompress::kOctetsParBlocBC1)
										   : (largeurs[i] * bppFinal);
					n.data = octets[i].Data();
					n.size = nk_uint32(octets[i].Size());
					cuisson.levels.PushBack(n);
				}
				return NkTexturePayload::Encode(cuisson, out, err);
			}

			// Depuis des pixels DEJA decodes — le chemin de la cuisson paresseuse.
			// Sans lui, un premier chargement decoderait deux fois : une pour
			// televerser, une pour cuire.
			//
			// `pixels` n'est ni copie ni possede : `NkImage::Wrap` en fait une vue.
			[[nodiscard]] static nk_bool CuireDepuisPixels(const nk_uint8 *pixels, nk_uint32 largeur, nk_uint32 hauteur,
															   NkImagePixelFormat format, nk_uint32 pasDeLigne,
															   const NkTexOvenReglages &reglages, NkVector<nk_uint8> &out,
															   NkString *err = nullptr) noexcept {
				if (!pixels || largeur == 0u || hauteur == 0u)
					return Refus(err, "pixels absents ou dimensions nulles");
				NkImage vue = NkImage::Wrap(const_cast<nk_uint8 *>(pixels), int32(largeur), int32(hauteur), format,
											int32(pasDeLigne));
				if (!vue.IsValid())
					return Refus(err, "vue sur les pixels invalide");
				return Cuire(vue, reglages, out, err);
			}

			// Commodite : depuis un fichier image sur disque.
			[[nodiscard]] static nk_bool CuireFichier(const char *cheminImage, const NkTexOvenReglages &reglages,
													  NkVector<nk_uint8> &out, NkString *err = nullptr) noexcept {
				NkImage img;
				if (!cheminImage || !img.Load(cheminImage, 0))
					return Refus(err, "image source illisible");
				return Cuire(img, reglages, out, err);
			}

		private:
			static nk_bool Refus(NkString *err, const char *raison) noexcept {
				if (err)
					*err = NkString(raison);
				return false;
			}
	};

	// =========================================================================
	// NkTexCacheNommage — OU vit un actif cuit, et sous quel nom
	//
	// 🔴 Ce nommage est partage par le FOUR (outil de ligne de commande, qui
	// pre-cuit une distribution) et par le MOTEUR (qui cuit paresseusement au
	// premier chargement). **Deux copies de ce calcul divergeraient au premier
	// champ ajoute**, et le symptome serait « la pre-cuisson ne sert jamais » —
	// sans erreur nulle part, parce que chacun chercherait un fichier que
	// l'autre n'ecrit pas. Une seule definition, donc, et elle est ici.
	//
	// L'empreinte porte le CONTENU de la source, pas sa date : le depot a paye
	// « un cache qui compare les dates ne voit pas un fichier restaure »
	// (2026-08-22). Elle porte aussi les OPTIONS de cuisson et la VERSION du
	// payload — changer l'une ou l'autre rend les actifs precedents introuvables
	// au lieu de les servir a tort.
	// =========================================================================
	class NkTexCacheNommage {
		public:
			static const char *Racine() noexcept {
				return "Build/Cache/Assets";
			}

			// Rend 0 si la source est illisible : l'appelant se passe alors du
			// cache, il n'invente pas une empreinte.
			[[nodiscard]] static nk_uint64 Empreinte(const char *cheminSource,
													 const NkTexOvenReglages &reglages) noexcept {
				if (!cheminSource)
					return 0u;
				std::FILE *f = std::fopen(cheminSource, "rb");
				if (!f)
					return 0u;

				constexpr nk_uint64 kBase = 14695981039346656037ULL;
				constexpr nk_uint64 kPrime = 1099511628211ULL;
				auto melanger = [](nk_uint64 h, const void *d, nk_size n) noexcept {
					const nk_uint8 *p = static_cast<const nk_uint8 *>(d);
					for (nk_size i = 0; i < n; ++i)
						h = (h ^ p[i]) * kPrime;
					return h;
				};

				nk_uint64 h = kBase;
				nk_uint64 total = 0u;
				// Par blocs : une texture 8K en RGBA depasse la centaine de Mo,
				// on ne la met pas entiere en memoire juste pour la hacher.
				nk_uint8 tampon[64u * 1024u];
				for (;;) {
					const nk_size lu = std::fread(tampon, 1, sizeof(tampon), f);
					if (lu == 0u)
						break;
					h = melanger(h, tampon, lu);
					total += lu;
				}
				const bool abime = (std::ferror(f) != 0);
				std::fclose(f);
				if (abime || total == 0u)
					return 0u;

				// La longueur, pour qu'aucune collision de suffixe ne rapproche
				// deux contenus de tailles differentes.
				h = melanger(h, &total, sizeof(total));

				const nk_uint8 opts[4] = {nk_uint8(reglages.sRGB ? 1u : 0u),
										  nk_uint8(reglages.genererMips ? 1u : 0u),
										  nk_uint8(reglages.addressMode & 0xFFu),
										  nk_uint8(reglages.filterMode & 0xFFu)};
				h = melanger(h, opts, sizeof(opts));
				// La compression aussi : un actif BC1 et un actif brut de la meme
				// source sont deux fichiers, pas un seul qui changerait de sens.
				const nk_uint32 comp = reglages.compression;
				h = melanger(h, &comp, sizeof(comp));

				const nk_uint32 v = kNkTexPayloadVersion;
				h = melanger(h, &v, sizeof(v));

				return h ? h : 1u; // 0 est reserve a « pas d'empreinte »
			}

			[[nodiscard]] static NkString Chemin(nk_uint64 empreinte) noexcept {
				return NkString::Fmtf("%s/%016llX.nktex", Racine(), (unsigned long long)empreinte);
			}
	};

} // namespace nkentseu

#endif // NKENTSEU_IMAGE_CORE_NKTEXTUREOVEN_H
