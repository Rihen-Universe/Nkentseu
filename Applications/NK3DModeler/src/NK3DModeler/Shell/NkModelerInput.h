#pragma once
// =============================================================================
// NkModelerInput.h — l'etat de l'application et le survol/clic sur les zones.
//
// POURQUOI UNE COUCHE A PART PLUTOT QUE DES `if` DANS LE DESSIN
//   Peindre et reagir sont deux besoins differents. Si chaque fonction de
//   peinture testait elle-meme la souris, il faudrait la lui passer partout, et
//   surtout : la ZONE cliquable finirait par diverger de la zone DESSINEE des la
//   premiere retouche de mise en page. On declare donc les zones UNE fois, au
//   moment ou on les dessine, et on interroge ensuite.
//
// LE PRINCIPE : REGISTRE DE ZONES.
//   Chaque frame, la peinture enregistre ses rectangles sensibles avec une CLE.
//   La couche d'interaction dit ensuite « quelle zone est sous la souris »,
//   « laquelle vient d'etre cliquee ». Zone dessinee et zone cliquable sont donc
//   le MEME rectangle par construction -- impossible qu'elles se decalent.
//
//   C'est le principe de l'interface immediate, et il evite le defaut classique
//   du bouton qui repond a cote parce qu'on a bouge le dessin sans bouger le
//   test.
// =============================================================================

#include "NKGui/Core/NkGuiContext.h"
#include "NKEditorKit/NkShortcutTable.h"

namespace nkentseu {
	namespace nk3d {

		using nkgui::NkRect;

		// ── MODES ───────────────────────────────────────────────────────────────
		// L'ordre est celui du deroulant. Sculpt 2.5D et Sculpt sont DEUX modes et
		// non un seul avec une option : ils n'ont ni les memes outils, ni le meme
		// resultat (le premier travaille en espace ecran, le second sur la
		// geometrie reelle).
		enum class NkMode : uint8 { Object = 0, Edit, Sculpt25D, Sculpt, Texturing, Count };

		inline const char *NkModeName(NkMode m) {
			static const char *const kNames[] = {"Objet", "Edition", "Sculpt 2.5D", "Sculpt",
												 "Texturing"};
			return (uint8)m < (uint8)NkMode::Count ? kNames[(uint8)m] : "?";
		}

		// Sous-mode de selection, valable uniquement hors mode objet.
		enum class NkSubMode : uint8 { Vertex = 0, Edge, Face };

		// Outil actif : « que fait mon clic ? ». Un seul a la fois.
		enum class NkTool : uint8 { Select = 0, Cursor, Move, Rotate, Scale };

		// ── ETAT DE SESSION ─────────────────────────────────────────────────────
		struct NkModelerState {
				NkMode mode = NkMode::Object;
				NkSubMode subMode = NkSubMode::Face;
				NkTool tool = NkTool::Move;

				// Aimantation : une bascule PAR transformation (cf. UI_SPEC / Unreal).
				bool snapGrid = true, snapAngle = true, snapScale = false;

				int32 selectedObject = 1; ///< ligne de la hierarchie
				int32 selectedAsset = 0;  ///< carte du navigateur
				int32 activeTab = 0;
				int32 activeFilter = 4; ///< pastille « Tout »

				// Etats par objet de la hierarchie. Tableau fixe tant que la scene est
				// simulee ; il deviendra une propriete de l'objet reel.
				bool visible[8] = {true, true, true, true, false, true, true, true};
				bool locked[8] = {false, false, true, false, false, false, false, false};

				// Defilements. Etats de SESSION : ils doivent survivre a la frame.
				float32 scrollHier = 0.f, scrollProps = 0.f, scrollDetails = 0.f;
				float32 scrollTree = 0.f, scrollAssets = 0.f;

				// Menus ouverts. -1 = aucun. L'indice designe l'entree de la barre.
				int32 openMenu = -1;

				bool running = true;
		};

		// ── REGISTRE DE ZONES ───────────────────────────────────────────────────
		// Cle STABLE en texte plutot qu'un identifiant numerique : on lit
		// « hier.row.2 » dans un journal, pas « 4711 ». Le cout d'une comparaison de
		// chaine est negligeable devant le nombre de zones d'un ecran.
		class NkHitRegistry {
			public:
				static const uint32 kMax = 256;

				void Begin(const nkgui::NkGuiInput &in) {
					mCount = 0;
					mMouse = in.mousePos;
					mDown = in.mouseDown[0];
					mClicked = in.mouseClicked[0];
					mRightClicked = in.mouseClicked[1];
					mWheel = in.wheel;
					mHover[0] = 0;
				}

				// Declare une zone sensible. Renvoie true si la souris est dessus --
				// ce qui permet d'ecrire directement `if (Add(...)) dessineSurvol();`.
				bool Add(const char *key, const NkRect &r) {
					if (mCount >= kMax || !key)
						return false;
					Copy(mKeys[mCount], key);
					mRects[mCount] = r;
					mCount++;
					const bool over = Contains(r, mMouse);
					// LA DERNIERE ZONE AJOUTEE GAGNE. C'est ce qu'il faut : on peint du
					// fond vers le dessus, donc la derniere declaree est celle qui est
					// VISIBLEMENT au-dessus. Garder la premiere ferait repondre le
					// panneau a la place du bouton pose dessus.
					if (over)
						Copy(mHover, key);
					return over;
				}

				bool IsHovered(const char *key) const {
					return Eq(mHover, key);
				}
				const char *Hovered() const {
					return mHover;
				}
				bool Clicked(const char *key) const {
					return mClicked && Eq(mHover, key);
				}
				bool RightClicked(const char *key) const {
					return mRightClicked && Eq(mHover, key);
				}
				bool Down(const char *key) const {
					return mDown && Eq(mHover, key);
				}
				bool AnyClick() const {
					return mClicked;
				}

				// Molette appliquee a la zone survolee. Le decalage est BORNE ici et non
				// chez l'appelant : sans borne, on peut faire defiler un panneau court
				// dans le vide et croire qu'il est vide.
				bool Wheel(const char *key, float32 &offset, float32 contentH, float32 viewH) const {
					if (mWheel == 0.f || !Eq(mHover, key))
						return false;
					const float32 maxOff = contentH > viewH ? contentH - viewH : 0.f;
					offset -= mWheel * 40.f;
					if (offset < 0.f)
						offset = 0.f;
					if (offset > maxOff)
						offset = maxOff;
					return true;
				}

				nkgui::NkVec2 Mouse() const {
					return mMouse;
				}

				static bool Contains(const NkRect &r, const nkgui::NkVec2 &p) {
					return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
				}

			private:
				static void Copy(char *dst, const char *src) {
					uint32 i = 0;
					for (; src[i] && i < 47; ++i)
						dst[i] = src[i];
					dst[i] = 0;
				}
				static bool Eq(const char *a, const char *b) {
					if (!a || !b)
						return false;
					while (*a && *b) {
						if (*a != *b)
							return false;
						++a;
						++b;
					}
					return *a == *b;
				}

				char mKeys[kMax][48] = {};
				NkRect mRects[kMax] = {};
				uint32 mCount = 0;
				char mHover[48] = {};
				nkgui::NkVec2 mMouse{0.f, 0.f};
				bool mDown = false, mClicked = false, mRightClicked = false;
				float32 mWheel = 0.f;
		};

	} // namespace nk3d
} // namespace nkentseu
