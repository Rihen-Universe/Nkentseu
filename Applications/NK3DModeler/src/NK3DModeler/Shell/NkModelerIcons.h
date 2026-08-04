#pragma once
// =============================================================================
// NkModelerIcons.h â€” les icones de l'interface.
//
// SOURCE : les SVG de vscode-codicons, deposes dans data/icons/. Le SVG plutot
// que le PNG parce qu'un rasteriseur existe deja (NkSVGCodec) : on decode a la
// taille VOULUE, donc net a tous les facteurs d'echelle. Un PNG de 16 px
// remonte a 24 baverait, et il faudrait livrer trois tailles de chaque icone.
//
// ON RASTERISE A 2x PUIS ON REDUIT D'UN CRAN. Decoder directement a 16 px donne
// un trait crenele : le rasteriseur n'a pas assez de surface pour moyenner. En
// decodant a 32 puis en reduisant, chaque pixel final resume 2x2 -> trait net.
// C'est la meme lecon que NKCode avait tiree pour ses PNG.
//
// LES ICONES SONT MONOCHROMES, teintees a l'affichage par le ROLE DE THEME de
// leur contexte. Une icone livree en gris et affichee telle quelle serait
// invisible en theme clair -- exactement le defaut que le systeme de roles
// existe pour empecher.
// =============================================================================

#include "NKImage/NKImage.h"
#include "NKEditorKit/NkIEditorRenderer.h"
#include "NKFileSystem/NkFile.h"
#include "NKContainers/String/NkFormat.h"

namespace nkentseu {
	namespace nk3d {

		// Identifiants STABLES. L'ordre n'a pas d'importance ici (rien n'est
		// serialise), mais les noms si : ils disent l'USAGE, pas le dessin. Le jour
		// ou Â« enregistrer Â» change d'icone, seule la table ci-dessous bouge.
		enum class NkIcon : uint16 {
			None = 0,
			Save,
			Gear,
			Add,
			Edit,
			Search,
			Folder,
			FolderOpen,
			ChevronDown,
			ChevronRight,
			ChevronLeft,
			Eye,
			Globe,
			Layers,
			Dot,	  ///< sous-mode SOMMET
			Square,	  ///< sous-mode FACE
			Move,
			Rotate,
			Scale,
			Cursor,
			Terminal,
			Drawer,
			Journal,
			WinClose,
			WinMax,
			WinMin,
			Trash,
			Mesh,	  ///< objet maillage dans la hierarchie
			Circle,	  ///< objet sphere
			Ruler,
			ArrowLeft,
			ArrowRight,
			Import,
			Refresh,
			Lock,
			Unlock,
			EyeClosed,
			Zoom,
			Pan,
			Camera,
			Ortho,
			Gizmo,
			Overlay,
			Light,
			Menu,
			WinRestore,
			ChevronUp,
			Check,
			Persp,
			OrthoView,
			SnapGrid,
			SnapAngle,
			SnapScale,
			// L'AIMANT et l'EDITION PROPORTIONNELLE ont leur propre dessin :
			// emprunter un quadrillage ou un cercle nu n'apprenait rien a qui
			// regarde la barre (regle de Rihen -- une icone doit DECRIRE).
			Magnet,
			Proportional,
			ArrowUp,
			ArrowDown,
			ViewFront,
			ViewBack,
			Material,
			Wireframe,
			Matcap,
			SelRect,
			SelCircle,
			SelLasso,
			// Modes d'affichage de la demo portee (normales / uv / occlusion) et
			// divers du cablage de la vue. Chaque BOUTON de la vue doit porter un
			// dessin unique -- deux boutons identiques obligent a lire l'info-bulle.
			ViewNormals,
			ViewUV,
			ViewAO,
			Picker, ///< pot de peinture : couleur personnalisee
			Speed,  ///< vitesse de camera
			// Icones DEDIEES du menu Ajouter (regle de Rihen).
			SphereUV,
			IcoSphere,
			Torus,
			Cylinder,
			Cone,
			Capsule,
			Plane3D,
			CircleEdge,
			Metaball,
			SurfacePatch,
			CurveBezier,
			Text3D,
			EmptyAxes,
			ImageRef,
			Cube3D,
			// Icones du PANNEAU DE PROPRIETES dessine par Rihen sur Banani. Elles
			// suivent la nomenclature Lucide de la maquette, pour que le dessin a
			// l'ecran soit celui qu'il a choisi.
			Sun,			  ///< pastille « Rendu »
			SlidersH,		  ///< pastille « Modificateur »
			Link2,			  ///< troisieme icone d'une ligne de transformation
			SquareCheck,	  ///< 1re action d'un element de liste (assigner)
			PlusCircle,		  ///< 3e action (ajouter)
			MinusCircle,	  ///< 4e action (retirer)
			Tag,			  ///< marqueur d'un attribut
			Pipette,		  ///< DESIGNER un objet dans la vue (« Picker » est le
							  ///< pot de peinture : deux gestes differents, deux dessins)
			Count
		};

		class NkModelerIcons {
			public:
				// Charge tout. `dirs` est essaye dans l'ordre : le premier ou le
				// fichier existe gagne, comme pour les themes et les icones de NKCode.
				bool Load(editorkit::NkIEditorRenderer &renderer, uint32 firstTexId, int32 sizePx = 16) {
					// Table usage -> fichier. Certains usages partagent un dessin : le
					// curseur de selection et la cible, par exemple. Les separer ici
					// permet de les dissocier plus tard sans toucher au rendu.
					struct Def {
							NkIcon id;
							const char *file;
					};
					static const Def kDefs[] = {
						{NkIcon::Save, "save"},
						{NkIcon::Gear, "gear"},
						{NkIcon::Add, "add"},
						{NkIcon::Edit, "edit"},
						{NkIcon::Search, "search"},
						{NkIcon::Folder, "folder"},
						{NkIcon::FolderOpen, "folder-opened"},
						{NkIcon::ChevronDown, "chevron-down"},
						{NkIcon::ChevronRight, "chevron-right"},
						{NkIcon::ChevronLeft, "chevron-left"},
						{NkIcon::Eye, "eye"},
						{NkIcon::Globe, "globe"},
						{NkIcon::Layers, "layers"},
						{NkIcon::Dot, "record-small"},
						{NkIcon::Square, "primitive-square"},
						{NkIcon::Move, "move"},
						{NkIcon::Rotate, "refresh"},
						{NkIcon::Scale, "screen-normal"},
						{NkIcon::Cursor, "target"},
						{NkIcon::Terminal, "terminal"},
						{NkIcon::Drawer, "split-horizontal"},
						{NkIcon::Journal, "output"},
						{NkIcon::WinClose, "chrome-close"},
						{NkIcon::WinMax, "chrome-maximize"},
						{NkIcon::WinMin, "chrome-minimize"},
						{NkIcon::Trash, "trash"},
						{NkIcon::Mesh, "package"},
						{NkIcon::Circle, "circle-filled"},
						{NkIcon::Ruler, "symbol-ruler"},
						{NkIcon::ArrowLeft, "arrow-left"},
						{NkIcon::ArrowRight, "arrow-right"},
						{NkIcon::Import, "desktop-download"},
						{NkIcon::Refresh, "refresh"},
						{NkIcon::Lock, "lock"},
						{NkIcon::Unlock, "unlock"},
						{NkIcon::EyeClosed, "eye-closed"},
						{NkIcon::Zoom, "zoom-in"},
						{NkIcon::Pan, "hand"},
						{NkIcon::SphereUV, "uv-sphere"},
						{NkIcon::IcoSphere, "ico-sphere"},
						{NkIcon::Torus, "torus"},
						{NkIcon::Cylinder, "cylinder"},
						{NkIcon::Cone, "cone"},
						{NkIcon::Capsule, "capsule"},
						{NkIcon::Plane3D, "plane-3d"},
						{NkIcon::CircleEdge, "circle-edge"},
						{NkIcon::Metaball, "metaball"},
						{NkIcon::SurfacePatch, "surface-patch"},
						{NkIcon::CurveBezier, "curve-bezier"},
						{NkIcon::Text3D, "text-3d"},
						{NkIcon::EmptyAxes, "empty-axes"},
						{NkIcon::ImageRef, "image-ref"},
						{NkIcon::Cube3D, "cube-3d"},
					// Panneau de proprietes (maquette Banani, nomenclature Lucide).
					{NkIcon::Sun, "sun"},
					{NkIcon::SlidersH, "sliders-horizontal"},
					{NkIcon::Link2, "link-2"},
					{NkIcon::SquareCheck, "square-check"},
					{NkIcon::PlusCircle, "plus-circle"},
					{NkIcon::MinusCircle, "minus-circle"},
					{NkIcon::Tag, "tag"},
					{NkIcon::Pipette, "pipette"},
						{NkIcon::Camera, "device-camera"},
						{NkIcon::Ortho, "ortho-cube"},
						{NkIcon::Gizmo, "location"},
						{NkIcon::Overlay, "circle-filled"},
						{NkIcon::Light, "lightbulb"},
						{NkIcon::Menu, "menu"},
						{NkIcon::WinRestore, "chrome-restore"},
						{NkIcon::ChevronUp, "chevron-up"},
						{NkIcon::Check, "check"},
						// Projection : la CAMERA pour la perspective (on voit depuis un point),
						// la GRILLE pour l'orthographique (aucun point de fuite). Deux dessins
						// distincts, sinon rien ne dit laquelle est active.
						{NkIcon::Persp, "device-camera"},
						{NkIcon::OrthoView, "symbol-numeric"},
						// AIMANTATION : dessins DISTINCTS de ceux des outils de
						// transformation. Reutiliser l'icone de rotation pour l'aimantation
						// angulaire faisait croire a deux boutons de rotation cote a cote --
						// c'est le defaut signale par Rihen. Une aimantation dit Â« sur quoi
						// je retombe Â», pas Â« ce que je fais Â».
						{NkIcon::SnapGrid, "table"},		// quadrillage
						{NkIcon::SnapAngle, "compass"},	// rapporteur
						{NkIcon::SnapScale, "law"},		// balance : proportions
						// Dessins FAITS POUR EUX (data/icons/) : un aimant en fer a
						// cheval dit « ca s'accroche », des anneaux de plus en plus
						// fins autour d'un point disent « l'influence decroit ».
						{NkIcon::Magnet, "magnet"},
						{NkIcon::Proportional, "proportional"},
						// Vues d'axe : chaque direction a SON icone. Six entrees portant le
						// meme dessin obligeraient a lire le libelle pour les distinguer, ce
						// qui annule l'interet d'une liste a icones.
						{NkIcon::ArrowUp, "arrow-circle-up"},
						{NkIcon::ArrowDown, "arrow-circle-down"},
						{NkIcon::ViewFront, "arrow-circle-right"},
						{NkIcon::ViewBack, "arrow-circle-left"},
						// OMBRAGE : quatre dessins DISTINCTS. Solide et Materiau partageaient
						// le meme disque plein -- on ne pouvait donc pas savoir lequel etait
						// actif en regardant le bouton, ce qui vide de son sens une barre a
						// icones seules. Blender les distingue de la meme facon : sphere nue
						// pour le solide, sphere COLOREE pour le materiau.
						{NkIcon::Material, "symbol-color"},
						{NkIcon::Wireframe, "list-tree"},
						{NkIcon::Matcap, "color-mode"},
						// Formes de selection.
						{NkIcon::SelRect, "chrome-maximize"},
						{NkIcon::SelCircle, "circle-filled"},
						{NkIcon::SelLasso, "edit"},
						{NkIcon::ViewNormals, "milestone"},
						{NkIcon::ViewUV, "symbol-method"},
						{NkIcon::ViewAO, "book"},
						{NkIcon::Picker, "paintcan"},
						{NkIcon::Speed, "symbol-operator"},
					};

					const NkString exeDir = NkPath::GetExecutableDirectory().ToString();
					const NkString exeIcons = exeDir.Empty() ? NkString("data/icons/") : (exeDir + "/data/icons/");
					const char *dirs[] = {"data/icons/", "Applications/NK3DModeler/data/icons/",
										  exeIcons.CStr()};

					uint32 ok = 0;
					const int32 n = (int32)(sizeof(kDefs) / sizeof(kDefs[0]));
					for (int32 i = 0; i < n; ++i) {
						const uint16 slot = (uint16)kDefs[i].id;
						mTex[slot] = 0;
						for (int32 d = 0; d < 3; ++d) {
							const NkString path = NkPrintf("%s%s.svg", dirs[d], kDefs[i].file);
							if (!NkFile::Exists(path.CStr()))
								continue;
							// 2x puis reduction d'un cran -> trait net (cf. l'en-tete).
							NkImage *big = NkSVGCodec::DecodeFromFile(path.CStr(), sizePx * 2, sizePx * 2);
							if (!big || !big->IsValid()) {
								if (big)
									big->Free();
								continue;
							}
							NkImage *small = big->Resize(sizePx, sizePx);
							NkImage *use = (small && small->IsValid()) ? small : big;

							// â”€â”€ LA CORRECTION QUI FAIT TOUT â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
							// Les codicons sont dessines en NOIR. La teinte du rendu
							// MULTIPLIE l'echantillon : noir x blanc = noir. Les icones
							// sortaient donc NOIRES sur fond sombre quelle que soit la
							// couleur demandee -- et le systeme de roles ne servait a
							// rien pour elles.
							//
							// On les convertit en MASQUE : RGB force a blanc, ALPHA
							// conserve (c'est lui qui porte la forme). La teinte reprend
							// alors tout son sens, exactement comme pour un glyphe de
							// police -- et les icones basculeront toutes seules en
							// theme clair.
							if (uint8 *px = use->Pixels()) {
								const int32 n = use->Width() * use->Height();
								for (int32 k = 0; k < n; ++k) {
									px[k * 4 + 0] = 255;
									px[k * 4 + 1] = 255;
									px[k * 4 + 2] = 255;
								}
							}

							const uint32 texId = firstTexId + (uint32)slot;
							if (renderer.UploadImageRGBA(texId, use->Pixels(), use->Width(), use->Height())) {
								mTex[slot] = texId;
								ok++;
							}
							if (small)
								small->Free();
							big->Free();
							break;
						}
					}
					mLoaded = ok;
					mSize = sizePx;
					return ok > 0;
				}

				// 0 si l'icone manque. L'appelant dessine alors SANS icone plutot que
				// de planter ou d'afficher un carre : une icone absente doit degrader
				// l'interface, pas la casser.
				uint32 Tex(NkIcon i) const {
					return (uint16)i < (uint16)NkIcon::Count ? mTex[(uint16)i] : 0u;
				}
				int32 Size() const {
					return mSize;
				}
				uint32 LoadedCount() const {
					return mLoaded;
				}

			private:
				uint32 mTex[(uint16)NkIcon::Count] = {};
				uint32 mLoaded = 0;
				int32 mSize = 16;
		};

	} // namespace nk3d
} // namespace nkentseu
