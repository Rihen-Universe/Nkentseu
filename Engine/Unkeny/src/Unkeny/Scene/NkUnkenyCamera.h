// =============================================================================
// NkUnkenyCamera.h — la VUE 2D, et les DEUX espaces qu'elle relie
//
// ⚠️ POURQUOI « VUE » ET NON « CAMERA » — mesure du 2026-09-01
//   `nkentseu::NkCamera2D` EXISTE DEJA, dans NKCamera : c'est une POSE de camera
//   virtuelle (position + rotation) pilotee par l'IMU, pour l'AR. Ce fichier
//   decrit tout autre chose : ce qu'on VOIT d'une scene et ou on le dessine.
//   Deux types du meme nom dans le meme depot, c'est le piege des deux
//   `NkShaderStage` que ce depot a deja paye : non qualifie, le second gagne,
//   et l'erreur ne sort pas dans le fichier fautif.
//   Le nom `NkVue2D` supprime la question au lieu de la documenter.
//
//   Le vocabulaire d'Unkeny, pour qu'il ne se re-melange pas :
//     NkVue2D                 ce qu'on voit d'une scene, et ou (Unkeny)
//     nkentseu::NkCamera2D    pose d'une camera virtuelle AR (NKCamera)
//     NkCameraSystem          la camera REELLE de l'appareil (NKCamera)
//
// ⚠️ LE PIEGE QUE CE FICHIER EXISTE POUR EVITER
//   Il y a deux reperes, et ils se ressemblent trait pour trait :
//     MONDE  ce que la scene decrit — metres, cases, ce qu'on veut
//     ECRAN  des pixels, origine en haut a gauche
//   Ce depot a deja paye une journee entiere sur ce seul point : un clic force
//   au pixel exact d'un objet ne le selectionnait jamais, parce qu'on donnait
//   des pixels de FENETRE a un code qui attendait des pixels de VISEUR. La
//   difference ne se voit pas : les deux sont des entiers, dans le meme ordre
//   de grandeur, et les deux « marchent » a l'ecran.
//
//   La regle qui en decoule, et elle vaut pour tout le moteur :
//     TOUTE conversion entre les deux passe par ICI, jamais a la main.
//   Un `pos.x * zoom + offset` ecrit ailleurs est un second convertisseur qui
//   divergera au premier changement de camera.
//
// LA CONVENTION D'AXES, ecrite parce qu'elle ne se devine pas
//   Le monde a Y VERS LE HAUT (convention mathematique : une gravite negative
//   fait tomber). L'ecran a Y vers le BAS. La conversion INVERSE donc Y — c'est
//   la seule chose surprenante de ce fichier, et c'est volontaire : forcer le
//   monde a avoir Y vers le bas rendrait toute la physique contre-intuitive.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKGui/Core/NkGuiTypes.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace unkeny {

		using math::NkVec2f;
		using nkgui::NkRect;

		class NkVue2D {
			public:
				/// La zone de l'ECRAN dans laquelle la camera dessine. En pixels.
				/// C'est elle qui rend le viseur d'un editeur possible : la camera
				/// ne suppose pas qu'elle occupe toute la fenetre.
				void PoserViseur(const NkRect &viseur) noexcept {
					mViseur = viseur;
				}
				const NkRect &Viseur() const noexcept {
					return mViseur;
				}

				/// Le point du MONDE qui se trouve au centre du viseur.
				void PoserCentre(const NkVec2f &centre) noexcept {
					mCentre = centre;
				}
				const NkVec2f &Centre() const noexcept {
					return mCentre;
				}

				/// Pixels par unite de monde. 1 unite = `mZoom` pixels.
				/// ⚠️ Jamais zero ni negatif : une camera a zoom nul divise par
				/// zero dans le sens inverse, et le defaut sort en NaN a l'autre
				/// bout de la chaine, tres loin d'ici.
				void PoserZoom(float32 pixelsParUnite) noexcept {
					mZoom = pixelsParUnite > 0.0001f ? pixelsParUnite : 0.0001f;
				}
				float32 Zoom() const noexcept {
					return mZoom;
				}

				void PoserRotation(float32 radians) noexcept {
					mRotation = radians;
				}
				float32 Rotation() const noexcept {
					return mRotation;
				}

				/// Cadre le monde pour qu'une zone donnee tienne ENTIEREMENT dans
				/// le viseur. Utile a l'ouverture d'un niveau ou d'un plateau.
				void Cadrer(const NkVec2f &centreMonde, const NkVec2f &tailleMonde) noexcept {
					mCentre = centreMonde;
					if (tailleMonde.x <= 0.f || tailleMonde.y <= 0.f) {
						return;
					}
					// On prend le zoom le PLUS PETIT des deux : c'est celui qui
					// fait tenir la zone. Prendre le plus grand la ferait deborder.
					const float32 zx = mViseur.w / tailleMonde.x;
					const float32 zy = mViseur.h / tailleMonde.y;
					PoserZoom(zx < zy ? zx : zy);
				}

				// --- LES DEUX SEULES CONVERSIONS DU MOTEUR --------------------

				NkVec2f MondeVersEcran(const NkVec2f &monde) const noexcept {
					const float32 dx = monde.x - mCentre.x;
					const float32 dy = monde.y - mCentre.y;
					float32 rx = dx, ry = dy;
					if (mRotation != 0.f) {
						const float32 c = math::NkCos(-mRotation);
						const float32 s = math::NkSin(-mRotation);
						rx = dx * c - dy * s;
						ry = dx * s + dy * c;
					}
					// ⚠️ Y S'INVERSE ICI, et seulement ici : le monde a Y vers le
					// haut, l'ecran vers le bas.
					return NkVec2f(mViseur.x + mViseur.w * 0.5f + rx * mZoom,
								   mViseur.y + mViseur.h * 0.5f - ry * mZoom);
				}

				NkVec2f EcranVersMonde(const NkVec2f &ecran) const noexcept {
					const float32 px = (ecran.x - (mViseur.x + mViseur.w * 0.5f)) / mZoom;
					const float32 py = -(ecran.y - (mViseur.y + mViseur.h * 0.5f)) / mZoom;
					float32 rx = px, ry = py;
					if (mRotation != 0.f) {
						const float32 c = math::NkCos(mRotation);
						const float32 s = math::NkSin(mRotation);
						rx = px * c - py * s;
						ry = px * s + py * c;
					}
					return NkVec2f(mCentre.x + rx, mCentre.y + ry);
				}

				/// Une LONGUEUR du monde en pixels. Distincte d'un point : une
				/// longueur ne subit ni le centrage ni l'inversion de Y. Les
				/// confondre donne des rayons qui bougent quand la camera se
				/// deplace.
				float32 LongueurVersEcran(float32 monde) const noexcept {
					return monde * mZoom;
				}

				/// Le rectangle du MONDE actuellement visible. Sert a ne dessiner
				/// que ce qui l'est (culling) — et a un editeur pour savoir quoi
				/// montrer dans sa minicarte.
				NkRect ZoneVisible() const noexcept {
					const float32 w = mViseur.w / mZoom;
					const float32 h = mViseur.h / mZoom;
					return NkRect{mCentre.x - w * 0.5f, mCentre.y - h * 0.5f, w, h};
				}

			private:
				NkRect mViseur{0.f, 0.f, 1.f, 1.f};
				NkVec2f mCentre{0.f, 0.f};
				float32 mZoom = 32.f; ///< 32 px par unite : une valeur qui se voit
				float32 mRotation = 0.f;
		};

	} // namespace unkeny
} // namespace nkentseu
