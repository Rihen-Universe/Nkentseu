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
			// ── COMPRESSION : `AUTO` PAR DEFAUT (2026-09-05) ─────────────────
			// 🔴 Le defaut etait « aucune », et Rodolf a teste : il n'a rien vu.
			// Normal — c'est exactement la condition ou j'avais moi-meme mesure un
			// gain NUL (1 682 ms contre 1 691). **Le defaut doit etre le mode qui
			// gagne.**
			//
			// `AUTO` ne compresse pas aveuglement : il REGARDE l'image et decide.
			// Ce que la mesure du jour a tranche (`NkTexBake --perte` sur les onze
			// cartes du depot) :
			//   - alpha UTILE          -> BRUT (BC1 n'a pas d'alpha ; il la mangerait
			//                            en silence — `awesomeface.png` est dans ce cas)
			//   - carte de NORMALES    -> BRUT (ecart max 155 et 79 niveaux mesures ;
			//                            BC5 est fait pour elles, il n'est pas livre)
			//   - tout le reste        -> BC1 (ecart 8 a 93, PSNR 26,8 a 46,7 dB)
			//
			// `NKTEXFMT_INCONNU` reste disponible et veut dire « brut, je l'ai
			// choisi » — c'est ce que pose `NK_TEX_FORMAT=raw`.
			nk_uint32 compression = NKTEXFMT_AUTO;
	};

	// =========================================================================
	// NkTextureOven
	// =========================================================================
	class NkTextureOven {
		public:
			// Ce que le dernier `Cuire` a decide, en clair. Existe pour que le
			// moteur puisse l'IMPRIMER : une decision automatique qu'on ne voit pas
			// est une decision qu'on ne peut pas contredire — et c'est exactement ce
			// qui a fait que Rodolf n'a rien vu le 2026-09-05.
			static NkString &DerniereDecision() noexcept {
				static NkString s;
				return s;
			}

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

			// ── RECONNAITRE UNE CARTE DE NORMALES PAR SON CONTENU ────────────
			// 🔴 Pas par son NOM : « nrm », « _n », « Normal », « bump » sont des
			// conventions d'artiste, et rien n'oblige personne a les suivre. Le
			// contenu ne ment pas : une normale tangentielle encode (x, y, z) avec
			// z > 0, donc **B proche de 255 partout** et **R, G autour de 128**. Une
			// texture de couleur n'a aucune raison d'avoir cette forme.
			//
			// Pourquoi ça compte : mesure du 2026-09-05 sur les cartes du depot,
			// `NkTexBake --perte` —
			//   backpack/normal.png : PSNR 28,99 dB, **ecart max 155 sur 255**
			//   rusted_iron/normal  : PSNR 31,10 dB, ecart max 79
			// contre 37,9 dB / 93 pour un albedo et 40,3 dB / 41 pour une occlusion.
			// BC1 interpole sur UN axe de couleur ; les normales varient sur trois
			// axes independants, et un ecart de 155 niveaux est une direction
			// franchement fausse — ça se voit sur l'eclairage, pas sur les pixels.
			// BC5 est fait pour elles ; il n'est pas livre. Donc : brut.
			//
			// Echantillonne au plus 4096 pixels repartis : lire une 4K entiere pour
			// une statistique ne changerait pas la reponse et couterait 67 Mo de
			// parcours.
			[[nodiscard]] static bool RessembleAUneNormale(const NkImage &img) noexcept {
				if (!img.IsValid())
					return false;
				const int32 ch = ChannelsOf(img.Format());
				if (ch < 3)
					return false; // une carte a 1 ou 2 canaux n'est pas une normale
				if (img.Format() != NkImagePixelFormat::NK_RGB24 && img.Format() != NkImagePixelFormat::NK_RGBA32)
					return false;

				const int32 pas = (img.Width() * img.Height() > 4096) ? int32(img.Width() / 64 + 1) : 1;
				nk_uint64 n = 0, bleuHaut = 0, rgCentre = 0;
				for (int32 y = 0; y < img.Height(); y += pas) {
					const uint8 *r = img.RowPtr(y);
					for (int32 x = 0; x < img.Width(); x += pas) {
						const uint8 *p = r + usize(x) * usize(ch);
						if (p[2] >= 200u)
							++bleuHaut;
						if (p[0] >= 78u && p[0] <= 178u && p[1] >= 78u && p[1] <= 178u)
							++rgCentre;
						++n;
					}
				}
				if (n == 0)
					return false;
				// Les deux conditions ENSEMBLE : un ciel bleu a du bleu haut mais pas
				// de R,G centres ; une photo grise a des R,G centres mais pas de bleu
				// haut. Il faut les deux pour etre une normale.
				return (bleuHaut * 100u / n) >= 70u && (rgCentre * 100u / n) >= 70u;
			}

			// Que faire de cette image ? Rend le code de compression a appliquer, et
			// remplit `raison` avec ce qui a decide — un choix silencieux est un choix
			// qu'on ne peut pas contredire.
			[[nodiscard]] static nk_uint32 ChoisirCompression(const NkImage &img, bool srgb,
															 const char **raison) noexcept {
				const char *r = nullptr;
				nk_uint32 code = NKTEXFMT_INCONNU;

				if (AAlphaUtile(img)) {
					r = "alpha utile : BC1 n'en a pas, il la mangerait en silence";
				} else if (RessembleAUneNormale(img)) {
					r = "carte de normales : BC1 y laisse jusqu'a 155 niveaux d'ecart (mesure) — BC5 est fait pour "
						"elles, il n'est pas livre";
				} else if (img.Format() != NkImagePixelFormat::NK_RGB24 &&
						   img.Format() != NkImagePixelFormat::NK_RGBA32 &&
						   img.Format() != NkImagePixelFormat::NK_GRAY8 &&
						   img.Format() != NkImagePixelFormat::NK_GRAY_A16) {
					r = "format non 8 bits par canal (HDR) : BC1 ne s'y applique pas";
				} else {
					code = srgb ? nk_uint32(NKTEXFMT_BC1_RGB_SRGB) : nk_uint32(NKTEXFMT_BC1_RGB_UNORM);
					r = "BC1";
				}
				if (raison)
					*raison = r;
				return code;
			}

			// L'alpha porte-t-il une information ? Un alpha entierement opaque n'en
			// porte pas — le jeter ne perd rien. Un seul pixel translucide suffit a
			// dire le contraire.
			[[nodiscard]] static bool AAlphaUtile(const NkImage &img) noexcept {
				if (!img.IsValid())
					return false;
				const int32 ch = ChannelsOf(img.Format());
				if (ch != 4 && ch != 2)
					return false;
				const int32 iA = ch - 1;
				for (int32 y = 0; y < img.Height(); ++y) {
					const uint8 *r = img.RowPtr(y);
					for (int32 x = 0; x < img.Width(); ++x)
						if (r[usize(x) * usize(ch) + usize(iA)] != 255u)
							return true;
				}
				return false;
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
				// `AUTO` se resout ICI, sur l'image reelle. La decision est rendue
				// par `derniereDecision` : le moteur l'imprime, l'outil aussi.
				nk_uint32 demande = reglages.compression;
				if (demande == NKTEXFMT_AUTO) {
					const char *raison = nullptr;
					demande = ChoisirCompression(source, reglages.sRGB, &raison);
					DerniereDecision() = raison ? raison : "";
				} else {
					DerniereDecision() = (demande == NKTEXFMT_INCONNU) ? "brut demande" : "compression demandee";
				}
				if (demande != NKTEXFMT_INCONNU) {
					const char *refus = RaisonRefusCompression(demande, cible);
					if (refus)
						return Refus(err, refus);
					// L'encodeur BC1 veut du RGBA8 serre ; `cible` garantit deja
					// RGBA32 (le RGB24 a ete converti en 1.).
					for (nk_size i = 0; i < largeurs.Size(); ++i) {
						NkVector<nk_uint8> blocs;
						NkBlockCompress::EncoderBC1(octets[i].Data(), largeurs[i], hauteurs[i], blocs);
						octets[i] = std::move(blocs);
					}
					codeFinal = demande;
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
