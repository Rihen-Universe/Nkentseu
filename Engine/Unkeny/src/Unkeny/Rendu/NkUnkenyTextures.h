// =============================================================================
// NkUnkenyTextures.h — les images des sprites
//
// A QUOI SERT CE FICHIER
//   Charger une image (PNG, JPEG, BMP, TGA… tout ce que NKImage decode, SVG
//   compris), la convertir en RGBA, lui donner un identifiant de texture et la
//   TELEVERSER vers le rendu. Un NkSprite2D qui porte cet identifiant dans
//   `texId` est alors dessine texture par NkDessinerScene.
//
// POURQUOI IL EXISTE — mesure du 2026-09-29
//   NkSprite2D a toujours eu `texId`, `uv0` et `uv1`. NkDessinerScene ne les
//   lisait pas : tout sprite etait un aplat de couleur. Et le seul chargeur
//   d'images du depot pour NKCanvas vivait DANS un jeu (Mou/Assets/MouAssets),
//   donc hors de portee des autres.
//
// ⚠️ UNKENY NE VOIT PAS LE RENDU — il recoit un TELEVERSEUR
//   Le backend graphique appartient a la coquille (NkCanvasGuiApp, membre
//   prive). Unkeny ne le touche pas : l'application branche une fonction.
//       mTextures.Brancher(&NkCanvasGuiApp::RelaisTeleversement,
//                          static_cast<NkCanvasGuiApp *>(this));
//   Consequence utile : on peut CHARGER avant d'avoir un rendu (dans un banc,
//   sans GPU). Les images attendent, et partent au branchement.
//
// ⚠️ LES PIXELS SONT GARDES EN MEMOIRE
//   Sur Android et sur le Web, le contexte graphique se PERD (mise en arriere-
//   plan, onglet recharge) et toutes les textures avec. Reteleverser() les
//   renvoie sans relire le disque. Le prix : une copie RGBA par image. Pour un
//   jeu 2D, c'est le bon compromis ; une application qui manipule des
//   centaines de grandes images le reglera par `garderPixels`.
//
// LES CHEMINS
//   Relatifs au dossier des ressources, comme dans Mou : "assets/" devant sur
//   ordinateur, rien sur Android (AAssetManager lit deja dans l'APK).
//   PoserRacine() change ce prefixe.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#pragma once

#include "NKContainers/Sequential/NkVector.h"
#include "NKCore/NkTypes.h"
#include "Unkeny/Scene/NkUnkenyComposants.h"

namespace nkentseu {
	namespace unkeny {

		class NkTextures2D {
			public:
				/// Envoie une image RGBA (4 octets par pixel, lignes jointives)
				/// au rendu sous `texId`. Rend false si le rendu l'a refusee.
				using NkTeleverseur = bool (*)(void *contexte, uint32 texId, const uint8 *rgba, int32 w, int32 h);

				/// Le premier identifiant distribue : 'UK' dans les deux octets
				/// hauts, loin des polices (0x4E4B46..) et du logo (0x4E4B4C47,
				/// 0x4E4B524D) de NkCanvasGuiApp. Deux textures qui partagent un
				/// identifiant s'ecrasent en silence cote backend.
				static constexpr uint32 kPremierId = 0x554B0001u;

				NkTextures2D();

				/// Branche le rendu. Les images deja chargees partent maintenant.
				void Brancher(NkTeleverseur televerseur, void *contexte);

				/// Prefixe des chemins relatifs (defaut : "assets/", "" sur Android).
				void PoserRacine(const char *racine);

				/// Charge un fichier. Le MEME chemin rend le MEME identifiant : un
				/// niveau qui demande cent fois "caisse.png" ne charge qu'une image.
				/// 0 = introuvable ou illisible (et c'est ecrit dans le journal).
				/// `largeurSvg`/`hauteurSvg` : taille de rasterisation d'un SVG.
				uint32 Charger(const char *chemin, int32 largeurSvg = 256, int32 hauteurSvg = 256);

				/// Decode une image deja en memoire (fichier embarque, telechargement).
				uint32 ChargerMemoire(const uint8 *donnees, usize taille, const char *nom);

				/// Enregistre des pixels RGBA fabriques par le programme (damier,
				/// degrade, rendu hors ecran). `nom` peut etre nul.
				uint32 Creer(const uint8 *rgba, int32 w, int32 h, const char *nom);

				/// Remplace les pixels d'une texture existante (meme taille ou non).
				bool Remplacer(uint32 id, const uint8 *rgba, int32 w, int32 h);

				bool Taille(uint32 id, int32 &w, int32 &h) const noexcept;
				const char *Nom(uint32 id) const noexcept;
				uint32 Trouver(const char *nom) const noexcept;
				const uint8 *Pixels(uint32 id) const noexcept; ///< nul si non gardes
				uint32 Nombre() const noexcept {
					return static_cast<uint32>(mEntrees.Size());
				}
				/// Combien attendent encore leur televersement (pas de rendu branche,
				/// ou rendu qui a refuse).
				uint32 EnAttente() const noexcept;

				/// Renvoie toutes les images au rendu : apres une perte de contexte.
				/// Rend le nombre d'images acceptees.
				uint32 Reteleverser();

				/// false : les pixels sont liberes des qu'une image est televersee.
				/// Plus de Reteleverser() possible pour elles.
				bool garderPixels = true;

				// --- Aides de sprite ----------------------------------------------
				/// Un sprite qui montre toute l'image, a `pixelsParUnite` pixels
				/// d'image par unite de monde (100 = une image de 100 px fait 1 m).
				NkSprite2D Sprite(uint32 id, float32 pixelsParUnite = 100.f) const noexcept;

				/// La region d'une case d'atlas en grille (colonnes x lignes), en UV.
				/// `index` part du coin haut-gauche et avance ligne par ligne —
				/// le meme ordre que NkAnimSprite2D::RegionUV.
				/// ⚠️ Le filtre est BILINEAIRE : au bord d'une case, la case voisine
				/// deteint d'un demi-pixel (mesure dans le banc, t6). Un atlas se
				/// dessine avec 1 ou 2 pixels de marge autour de chaque image.
				static void RegionGrille(int32 colonnes, int32 lignes, int32 index, math::NkVec2f &uv0,
										 math::NkVec2f &uv1) noexcept;

			private:
				struct NkEntree {
						uint32 id = 0;
						char nom[160] = {};
						int32 w = 0;
						int32 h = 0;
						NkVector<uint8> rgba;
						bool televersee = false;
				};
				uint32 Enregistrer(const uint8 *rgba, int32 w, int32 h, int32 stride, const char *nom);
				bool Envoyer(NkEntree &e);
				NkEntree *Entree(uint32 id) noexcept;
				const NkEntree *Entree(uint32 id) const noexcept;

				NkVector<NkEntree> mEntrees;
				NkTeleverseur mTeleverseur = nullptr;
				void *mContexte = nullptr;
				uint32 mProchainId = kPremierId;
				char mRacine[128] = {};
		};

	} // namespace unkeny
} // namespace nkentseu
