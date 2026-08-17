#pragma once
// =============================================================================
// NkcTotemLibrary — les totems en IMAGES, deposees dans un dossier.
//
// LE PRINCIPE, LE MEME QUE POUR LES GRILLES
// -----------------------------------------
// « Une regle est du code, un plateau est une donnee. » Un totem est une donnee
// aussi : un artiste doit pouvoir le changer sans compiler, sans ouvrir l'atelier,
// sans demander a personne. Il depose des fichiers, il sauvegarde, ca apparait.
//
//     travail/totems/
//         Guerrier/          <- UN DOSSIER = UN TOTEM, son nom est le nom du totem
//             n0.png             niveau 0, image fixe
//             n1.png             niveau 1
//             n2.png  n3.png  n4.png
//         Esprit/
//             n0_000.png         niveau 0, ANIMATION : suffixe numerique
//             n0_001.png
//             n0_002.png
//             n1.png             niveau 1, fixe — on peut melanger
//         Pierre/
//             base.png           AUCUN niveau : cette image sert a tous
//
// LES TROIS REGLES DE NOMMAGE, ET RIEN D'AUTRE
//   nN.png          image du niveau N (0 a 4)
//   nN_KKK.png      Kieme image de l'animation du niveau N
//   base(.png|_KKK) s'applique a TOUS les niveaux — c'est le cas « pas de niveau »
//
// Un niveau sans image retombe sur `base`, et `base` absent retombe sur le disque
// colore d'origine. A AUCUN MOMENT une image manquante ne fait disparaitre un
// totem : le jeu doit rester jouable pendant qu'on dessine.
//
// LA MISE A L'ECHELLE EST AUTOMATIQUE
// -----------------------------------
// L'image est ajustee a la cellule, en conservant ses proportions. L'artiste
// travaille a la taille qui l'arrange ; c'est l'atelier qui adapte, parce que la
// taille d'une cellule depend du plateau, de la fenetre et du zoom — trois choses
// qu'aucun fichier PNG ne peut connaitre.
//
// CE QU'ON NE FAIT PAS, ET POURQUOI
// ---------------------------------
// Pas d'atlas. Un atlas demande un outil de packing, un format de metadonnees, et
// une etape de build : exactement les trois choses qu'on veut eviter a quelqu'un
// dont le metier est de dessiner. Une texture par image coute plus de draw calls ;
// sur quelques dizaines de totems a l'ecran, cela ne se mesure pas.
// =============================================================================

#include "ConquerorLab/NkcWinClean.h"

#include "Conqueror/ConquerorRulesABI.h"

#include "NKEditorKit/NkEditorShell.h"
#include "NKImage/NKImage.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace conqueror {

		// Les identifiants de texture sont alloues PAR LE SHELL (UploadRGBA), pas
		// par nous : se reserver une plage « assez haute pour ne gener personne »
		// est le genre de convention qui tient jusqu'au jour ou elle ne tient plus,
		// et l'echec serait alors une police remplacee par un totem.

		/// Images par niveau et par totem. Cinq niveaux (REGLES §5), plus une
		/// entree « base » qui sert de repli.
		inline constexpr int32 kTotemLevels	   = 5;
		inline constexpr int32 kTotemMaxFrames = 64;

		/// Une suite d'images — une seule si l'objet est fixe.
		struct NkcTotemAnim {
				uint32	tex[kTotemMaxFrames] = {};
				uint16	w[kTotemMaxFrames]	 = {};
				uint16	h[kTotemMaxFrames]	 = {};
				int32	count				 = 0;

				bool Valid() const noexcept { return count > 0; }
		};

		struct NkcTotem {
				NkString	 name;					 ///< le nom du DOSSIER
				NkcTotemAnim levels[kTotemLevels];	 ///< vide si le niveau n'a rien
				NkcTotemAnim base;					 ///< repli, « pas de niveau »

				/// L'animation a utiliser pour ce niveau : le niveau, sinon la base,
				/// sinon rien — et « rien » veut dire « dessine le disque colore ».
				const NkcTotemAnim *For(int32 level) const noexcept {
					if (level >= 0 && level < kTotemLevels && levels[level].Valid())
						return &levels[level];
					if (base.Valid()) return &base;
					return nullptr;
				}

				bool Valid() const noexcept {
					if (base.Valid()) return true;
					for (int32 i = 0; i < kTotemLevels; ++i)
						if (levels[i].Valid()) return true;
					return false;
				}
		};

		// ---------------------------------------------------------------------
		class NkcTotemLibrary {
			public:
				/// `dir` : travail/totems. Cree s'il manque — un dossier absent
				/// n'apprend a personne ou deposer ses images.
				void Init(const NkString &dir) noexcept {
					mDir = dir;
					NkDirectory::CreateRecursive(mDir.CStr());
				}

				const NkString			   &Dir() const noexcept { return mDir; }
				const NkVector<NkcTotem>   &Totems() const noexcept { return mTotems; }

				/// (Re)lit le dossier et televerse les textures.
				///
				/// APPELEE UNE FOIS, PAS A CHAQUE IMAGE : lire un dossier et decoder
				/// des PNG soixante fois par seconde figerait l'atelier. Le panneau
				/// Joueurs expose un bouton « Recharger » — un artiste qui vient de
				/// modifier un fichier sait qu'il l'a modifie.
				void Reload(editorkit::NkEditorShell &shell) noexcept {
					mTotems.Clear();
					if (mDir.Empty() || !NkDirectory::Exists(mDir.CStr())) return;

					NkVector<NkString> dirs = NkDirectory::GetDirectories(mDir.CStr());
					for (usize i = 0; i < dirs.Size(); ++i) {
						NkcTotem t;
						t.name = dirs[i];

						NkString sub = mDir;
						sub += "/";
						sub += dirs[i];

						LoadSet(shell, sub, "base", t.base);
						for (int32 lv = 0; lv < kTotemLevels; ++lv) {
							char prefix[8];
							std::snprintf(prefix, sizeof(prefix), "n%d", lv);
							LoadSet(shell, sub, prefix, t.levels[lv]);
						}

						if (t.Valid()) {
							mTotems.PushBack(t);
							logger.Infof("[lab] totem « %s » charge", t.name.CStr());
						} else {
							logger.Infof("[lab] dossier de totem sans image exploitable : %s",
										 t.name.CStr());
						}
					}
				}

				/// Le totem `idx`, ou nullptr — l'appelant retombe alors sur le
				/// disque colore.
				const NkcTotem *At(int32 idx) const noexcept {
					if (idx < 0 || static_cast<usize>(idx) >= mTotems.Size()) return nullptr;
					return &mTotems[static_cast<usize>(idx)];
				}

				int32 IndexOf(const char *name) const noexcept {
					if (!name) return -1;
					for (usize i = 0; i < mTotems.Size(); ++i)
						if (mTotems[i].name == name) return static_cast<int32>(i);
					return -1;
				}

			private:
				/// Charge `prefix.png` (fixe) OU la suite `prefix_000.png`,
				/// `prefix_001.png`... (animation). Les deux formes ne se melangent
				/// pas : une suite l'emporte, parce qu'un artiste qui a numerote ses
				/// images voulait une animation.
				void LoadSet(editorkit::NkEditorShell &shell, const NkString &dir,
							 const char *prefix, NkcTotemAnim &out) noexcept {
					out.count = 0;

					// 1. la suite numerotee
					for (int32 k = 0; k < kTotemMaxFrames; ++k) {
						char file[64];
						std::snprintf(file, sizeof(file), "%s_%03d.png", prefix, k);
						NkString path = dir;
						path += "/";
						path += file;
						if (!NkFile::Exists(path.CStr())) break;
						if (!Upload(shell, path, out)) break;
					}
					if (out.count > 0) return;

					// 2. l'image seule
					NkString path = dir;
					path += "/";
					path += prefix;
					path += ".png";
					if (NkFile::Exists(path.CStr())) Upload(shell, path, out);
				}

				bool Upload(editorkit::NkEditorShell &shell, const NkString &path,
							NkcTotemAnim &out) noexcept {
					if (out.count >= kTotemMaxFrames) return false;
					NkImage img;
					// 4 canaux : on impose le RGBA parce que le televersement l'attend,
					// et parce qu'un totem sans canal alpha serait un carre sur la
					// cellule au lieu d'une silhouette.
					if (!img.Load(path.CStr(), 4) || !img.IsValid()) {
						logger.Infof("[lab] image illisible : %s", path.CStr());
						return false;
					}
					const uint32 tex = shell.UploadRGBA(
						reinterpret_cast<const uint8 *>(img.Pixels()), img.Width(), img.Height());
					if (tex == 0) return false;
					out.tex[out.count] = tex;
					out.w[out.count]   = static_cast<uint16>(img.Width());
					out.h[out.count]   = static_cast<uint16>(img.Height());
					++out.count;
					return true;
				}

			private:
				NkString		   mDir;
				NkVector<NkcTotem> mTotems;
		};

	} // namespace conqueror
} // namespace nkentseu
