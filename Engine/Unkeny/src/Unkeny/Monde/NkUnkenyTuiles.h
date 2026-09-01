// =============================================================================
// NkUnkenyTuiles.h — la carte de tuiles (RPG, plateforme, roguelike)
//
// A QUOI SERT CE FICHIER
//   Un decor 2D fait de cases : le sol d'un RPG, les plateformes d'un jeu de
//   plateforme, le donjon d'un roguelike. C'est la structure qui porte 95 % des
//   entites visibles d'un jeu 2D — et la raison pour laquelle on ne les met PAS
//   dans l'ECS.
//
// ⚠️ POURQUOI UNE TUILE N'EST PAS UNE ENTITE
//   Une carte de 200x200 fait quarante mille cases. En faire quarante mille
//   entites ECS coute quarante mille transforms, quarante mille sprites, et un
//   parcours de requete a chaque trame — pour un decor qui ne bouge pas.
//   Une carte est donc un TABLEAU, dessine directement. Les entites restent
//   pour ce qui vit : le joueur, les ennemis, les objets ramassables.
//   C'est la meme decision que tous les moteurs 2D prennent, et c'est celle
//   qu'on regrette de ne pas avoir prise quand la carte grandit.
//
// ⚠️ ET LES COLLISIONS DE LA CARTE NE SONT PAS QUARANTE MILLE CORPS
//   Un corps physique par tuile ecraserait le solveur. La carte expose une
//   requete — « qu'y a-t-il en (x, y) ? » — et un jeu de plateforme s'en sert
//   directement. Pour un vrai contact physique, on construit des BANDES : les
//   tuiles pleines contigues d'une meme ligne deviennent UN corps statique.
//   `ConstruireCorpsStatiques` fait cette fusion.
//
// LE REPERE
//   Case (0,0) en HAUT A GAUCHE de la carte, comme un tableau. La conversion
//   vers le monde tient compte de l'axe Y inverse — une carte qu'on lit de haut
//   en bas dans un fichier doit s'afficher de haut en bas a l'ecran.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - une couche de plus (decor, objets, collisions) -> une NkCoucheTuiles
//   - un format de fichier                           -> a cote, pas ici : la
//     carte est une donnee, sa lecture est un service
// =============================================================================
#pragma once

#include "NKContainers/Sequential/NkVector.h"
#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace unkeny {

		using math::NkVec2f;

		/// Une tuile vide. 0 est reserve : une carte fraichement allouee est
		/// donc vide, et non remplie de la premiere tuile de l'atlas.
		static const uint16 NK_TUILE_VIDE = 0;

		/// Ce qu'une tuile fait au jeu. Le rendu ne s'en sert pas ; la
		/// navigation et la collision, si.
		enum class NkNatureTuile : uint8 {
			NK_TRAVERSABLE = 0, ///< herbe, sol, decor
			NK_SOLIDE,			///< mur : on ne passe pas
			NK_PLATEFORME,		///< on la traverse par en dessous, on s'y pose par au-dessus
			NK_MORTELLE,		///< pics, vide
			NK_EAU				///< ralentit, ou nage
		};

		/// Une couche de la carte. Une carte en a plusieurs : fond, decor,
		/// devant. Elles se dessinent dans l'ordre, chacune avec sa parallaxe.
		struct NkCoucheTuiles {
				NkVector<uint16> tuiles; ///< largeur * hauteur, indices d'atlas
				int32 couche = 0;		 ///< ordre de dessin, comme un sprite

				/// Vitesse de defilement par rapport a la camera. 1 = colle au
				/// monde ; 0,5 = deux fois plus lent (fond lointain) ; 0 = fixe.
				/// C'est ce qui donne la profondeur a un decor 2D, et ca ne coute
				/// qu'un facteur.
				float32 parallaxe = 1.f;
				bool visible = true;
				uint32 teinte = 0xFFFFFFFFu;
		};

		class NkCarteTuiles {
			public:
				/// `tailleTuile` est en unites de MONDE, pas en pixels : une carte
				/// ne doit pas changer de taille quand on zoome.
				bool Creer(int32 largeur, int32 hauteur, float32 tailleTuile);
				void Liberer();

				int32 Largeur() const noexcept {
					return mLargeur;
				}
				int32 Hauteur() const noexcept {
					return mHauteur;
				}
				float32 TailleTuile() const noexcept {
					return mTaille;
				}
				bool EstValide() const noexcept {
					return mLargeur > 0 && mHauteur > 0;
				}

				/// Ajoute une couche vide. Rend son index, ou -1 si la carte n'est
				/// pas creee — un refus se DIT.
				int32 AjouterCouche(int32 ordre = 0, float32 parallaxe = 1.f);

				int32 NbCouches() const noexcept {
					return static_cast<int32>(mCouches.Size());
				}
				NkCoucheTuiles *Couche(int32 i) noexcept;
				const NkCoucheTuiles *Couche(int32 i) const noexcept;

				uint16 Tuile(int32 couche, int32 x, int32 y) const noexcept;
				void PoserTuile(int32 couche, int32 x, int32 y, uint16 tuile) noexcept;

				/// La nature d'une tuile de l'atlas. C'est une propriete de
				/// l'ATLAS, pas de la carte : la meme tuile est solide partout.
				void PoserNature(uint16 tuile, NkNatureTuile nature);
				NkNatureTuile Nature(uint16 tuile) const noexcept;

				/// La nature au point du MONDE donne, en consultant la couche de
				/// collision. C'est la requete qu'un jeu de plateforme appelle.
				NkNatureTuile NatureAu(const NkVec2f &monde, int32 coucheCollision) const noexcept;

				bool EstSolideAu(const NkVec2f &monde, int32 coucheCollision) const noexcept {
					const NkNatureTuile n = NatureAu(monde, coucheCollision);
					return n == NkNatureTuile::NK_SOLIDE;
				}

				// --- Conversions carte <-> monde ------------------------------
				// ⚠️ Case (0,0) en HAUT A GAUCHE, et le monde a Y vers le HAUT :
				// la ligne 0 est donc la plus HAUTE en monde. Confondre les deux
				// retourne la carte, et c'est le genre de defaut qu'on cherche
				// dans le rendu alors qu'il est dans la conversion.
				NkVec2f CaseVersMonde(int32 x, int32 y) const noexcept {
					return NkVec2f((static_cast<float32>(x) + 0.5f) * mTaille,
								   -(static_cast<float32>(y) + 0.5f) * mTaille);
				}
				bool MondeVersCase(const NkVec2f &monde, int32 &x, int32 &y) const noexcept {
					if (mTaille <= 0.f) {
						return false;
					}
					x = static_cast<int32>(math::NkFloor(monde.x / mTaille));
					y = static_cast<int32>(math::NkFloor(-monde.y / mTaille));
					return x >= 0 && x < mLargeur && y >= 0 && y < mHauteur;
				}

				/// Taille de la carte entiere, en monde. Sert a cadrer la vue.
				NkVec2f TailleMonde() const noexcept {
					return NkVec2f(static_cast<float32>(mLargeur) * mTaille,
								   static_cast<float32>(mHauteur) * mTaille);
				}

			private:
				int32 mLargeur = 0;
				int32 mHauteur = 0;
				float32 mTaille = 1.f;
				NkVector<NkCoucheTuiles> mCouches;
				/// Nature par index de tuile. Indexee directement : un tableau
				/// vaut mieux qu'une table de hachage pour quelques centaines de
				/// tuiles consultees des milliers de fois par trame.
				NkVector<NkNatureTuile> mNatures;
		};

	} // namespace unkeny
} // namespace nkentseu
