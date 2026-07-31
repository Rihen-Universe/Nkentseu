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

				// -1 = AUCUNE selection. C'est un etat legitime : celui ou les commandes
				// de scene s'appliquent, et non une valeur sentinelle d'erreur.
				int32 selectedObject = 1;
				int32 selectedAsset = 0;  ///< carte du navigateur
				int32 selectedFolder = 1; ///< dossier du navigateur
				int32 matcap = 0;		  ///< matcap actif, mode edition seulement
				int32 viewLayout = 0;	  ///< disposition des vues (menu de gauche)
				int32 selShape = 0;		  ///< rectangle / cercle / lasso
				int32 modOpenCat = 0;	  ///< categorie de modificateurs survolee
				NkRect modAnchor{};		  ///< ou ancrer la liste a deux niveaux
				int32 activeTab = 0;
				int32 activeFilter = 4; ///< pastille « Tout »

				// ── ETATS DES LISTES DEROULANTES ────────────────────────────────
				// Un indice par combo. Les libelles vivent dans les tables de
				// NkModelerScreens : ici on ne garde que le CHOIX, qui est ce qui
				// appartient a la session.
				int32 projection = 0; ///< 0 perspective, 1..6 vues orthographiques
				int32 shading = 0;	  ///< eclaire / non eclaire / fil de fer / rendu
				// Masque de surimpressions : grille, repere, contours, gizmos, normales,
				// statistiques, filaire, origines. Grille + repere + contours + gizmos
				// par defaut -- ce qu on veut voir en ouvrant, sans le bruit du reste.
				uint32 overlayMask = 0x0Fu;
// Sections DEROULEES du panneau Details, un bit par section (maillage,
				// modificateurs, materiaux, sous-maillages). Les quatre ouvertes au
				// depart : un panneau qui s'ouvre tout replie oblige a quatre clics
				// avant de montrer quoi que ce soit.
				uint32 detailOpen = 0x0Fu;
				int32 materialSlot = 0; ///< emplacement de materiau courant
				// PANNEAUX VISIBLES. Ils ont une taille minimale (cf. NkLayout) : on ne
				// les retrecit donc pas jusqu'a disparition, on les MASQUE franchement.
				// Un panneau reduit a trois pixels n'est ni utilisable ni refermable.
				bool showLeft = true;
				bool showRight = true;
				bool showBrowser = true;
				// Fraction horizontale du clic dans la barre de titre, retenue au moment
				// du clic pour replacer la fenetre sous le curseur quand on la tire
				// depuis l'etat maximise.
				float32 dragFracX = 0.5f;
				int32 solidLight = 0; ///< eclairage du mode solide : studio / matcap / plat
				int32 selectMode = 2; ///< sommet / arete / face
				int32 addKind = 0;	  ///< primitive a ajouter
				int32 modKind = 0;	  ///< modificateur a ajouter
				int32 orientation = 0; ///< repere : monde / local / normal / vue
				int32 camSpeed = 2;	   ///< vitesse de deplacement de la camera

				// Sections repliables. Une seule pour l'instant ; il y en aura une par
				// section de panneau, et c'est deja la bonne forme.
				bool showTransform = true;

				// Valeurs de transformation. Elles vivent ici et non dans la peinture :
				// le glissement les modifie, elles doivent survivre a la frame.
				float32 pos[3] = {0.f, 0.f, 0.f};
				float32 rot[3] = {0.f, 0.f, 0.f};
				float32 scl[3] = {1.f, 1.f, 1.f};

				// Dossiers deplies du navigateur.
				bool folderOpen[8] = {true, true, false, false, false, false, false, false};

				// Scenes. UNE seule par defaut -- ouvrir sur deux scenes vides ferait
				// croire que l'une d'elles contient quelque chose.
				char sceneNames[8][32] = {"Scene", "", "", "", "", "", "", ""};
				int32 sceneCount = 1;

				// Noms modifiables de la hierarchie et des dossiers.
				char objectNames[8][32] = {"Scene", "Cube", "Sphere", "Groupe", "Roue", "Axe", "", ""};
				char folderNames[8][32] = {"MonProjet", "Maillages", "Animations", "Materiaux",
										   "Textures", "", "", ""};

				// Etats par objet de la hierarchie. Tableau fixe tant que la scene est
				// simulee ; il deviendra une propriete de l'objet reel.
				bool visible[8] = {true, true, true, true, false, true, true, true};
				bool locked[8] = {false, false, true, false, false, false, false, false};

				// Defilements. Etats de SESSION : ils doivent survivre a la frame.
				float32 scrollHier = 0.f, scrollProps = 0.f, scrollDetails = 0.f;
				float32 scrollTree = 0.f, scrollAssets = 0.f;
				// Defilements HORIZONTAUX : les noms longs debordent des qu'un panneau
				// est retreci, et sans eux la fin du nom est simplement perdue.
				float32 scrollHierH = 0.f, scrollPropsH = 0.f, scrollDetailsH = 0.f;
				float32 scrollTreeH = 0.f, scrollAssetsH = 0.f;

				// Menus ouverts. -1 = aucun. L'indice designe l'entree de la barre.
				int32 openMenu = -1;
				int32 hoverMenuItem = -1;
				int32 openSubMenu = -1; ///< sous-menu deploye dans le menu courant

				// ── PROPORTIONS AJUSTABLES ──────────────────────────────────────
				// Les separateurs modifient ces FRACTIONS et non des pixels : a la
				// prochaine ouverture, la disposition se retrouve identique quelle que
				// soit la taille de fenetre. En pixels, une fenetre plus petite
				// ecraserait les panneaux ; en fractions, ils suivent.
				float32 leftFrac = 0.16f;
				float32 rightFrac = 0.29f;
				float32 browserFrac = 0.22f;
				float32 propsFrac = 0.45f; ///< part des proprietes dans la colonne de droite

				// Separateur en cours de glissement. -1 = aucun. On MEMORISE lequel :
				// sans cela, un glissement rapide qui sort du rectangle du separateur
				// le lacherait en pleine course.
				int32 dragSplitter = -1;
				float32 dragStart = 0.f;   ///< position souris au debut du glissement
				float32 dragStartFrac = 0.f;

				// ── DOCUMENT ────────────────────────────────────────────────────
				// `dirty` decide s'il faut demander confirmation a la fermeture. Il
				// passe a vrai des qu'une action modifie la scene.
				bool dirty = true;
				bool askClose = false; ///< boite de confirmation affichee
				bool maximized = false;
				// Consommes par la boucle APRES la frame : BeginDragMove et Maximize
				// bloquent (boucle modale de l'OS), les appeler pendant la peinture
				// reentrerait dans la frame.
				bool wantDragMove = false;
				bool wantMinimize = false;
				bool wantMaxRestore = false;

				bool running = true;
		};

		// Forme de curseur demandee pour la frame. L'application la reporte a la
		// fenetre APRES la peinture : une zone dessinee tard peut ainsi corriger la
		// demande d'une zone dessinee tot, exactement comme pour le survol.
		enum class NkCursorWant : uint8 { Arrow = 0, Hand, ResizeWE, ResizeNS };

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
					mCursor = NkCursorWant::Arrow;
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
				// Bouton ENFONCE, independamment de la zone survolee : c'est ce qu'il
				// faut pour poursuivre un glissement quand la souris a quitte la zone.
				bool MouseDown() const {
					return mDown;
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

				// Curseur voulu. La DERNIERE demande gagne, meme regle que le survol :
				// c'est ce qui est peint par-dessus qui commande.
				void WantCursor(NkCursorWant c) {
					mCursor = c;
				}
				NkCursorWant Cursor() const {
					return mCursor;
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
				NkCursorWant mCursor = NkCursorWant::Arrow;
		};

	} // namespace nk3d
} // namespace nkentseu
