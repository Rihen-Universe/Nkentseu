// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#pragma once
// =============================================================================
// Nogee/Viewport/NogeeViewport3D.h — le pont OPAQUE entre la scene ECS et l'ecran
// =============================================================================
// CE QUE C'EST : la premiere image du moteur dans son propre editeur. Le monde
// ECS de Noge est rendu hors ecran par `renderer::NkRenderer` sur le device DEJA
// monte par l'editeur (`NkEditorRHIRenderer`), puis sa texture est publiee au
// backend NKGui sous `kNogeeViewportTexId` — le `ViewportPanel` n'a plus qu'a la
// poser avec `AddImage`.
//
// POURQUOI CETTE FACADE EST OPAQUE (`void*` partout, aucun type NKRenderer) :
// `NKRenderer` et `NKCanvas` declarent tous les deux `renderer::NkBlendMode` et
// `renderer::NkVertex2D`. Les deux ne peuvent pas se rencontrer dans une meme
// unite de compilation. `NogeeShell.cpp` et `ViewportPanel.cpp` vivent du cote
// NKGui/NKEditorKit ; le cote NKRenderer est enferme dans `NogeeViewport3D.cpp`,
// SEULE unite du projet a inclure NKRenderer. Meme regle, et meme raison, que
// `NkAnimaEditor/AnimBridge.h` et `NK3DModeler/NkDemo3DHost.h` — les deux hotes
// qui, eux, affichent deja une scene 3D dans un panneau d'editeur.
//
// L'ORDRE, ET IL N'EST PAS NEGOCIABLE : le rendu hors ecran se fait dans le
// crochet `SetPreUI` de l'editeur (`NkEditorRHIRenderer.h` l.182), c'est-a-dire
// frame device OUVERTE et passe backbuffer PAS ENCORE commencee — on ne peut pas
// imbriquer une passe de rendu dans une autre.
//
// CE QUE CE PONT NE FAIT PAS : il ne reecrit aucun renderer. La scene est
// alimentee par `NkRenderSystem` (Noge/ECS/Systems), tel quel ; ce fichier ne
// fait que lui donner un renderer, une cible, une camera et une frame.
// =============================================================================

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace noge {

		// Identifiant de la texture d'interface qui porte la vue 3D. Meme espace
		// d'identifiants que les polices et les images du theme : il doit rester
		// unique dans l'application. 4096 est la valeur qu'utilisent deja
		// NK3DModeler et son ancienne vue (NkViewport3D.h l.30) — on garde la
		// convention de la maison plutot que d'en inventer une.
		constexpr uint32 kNogeeViewportTexId = 4096u;

		// ── Montage (a appeler une fois, avant la premiere frame) ─────────────
		// Le device de l'editeur. UNE pile GPU par fenetre : on emprunte le sien,
		// on n'en ouvre pas un second.
		void NogeeViewport3DSetDevice(void *device); // NkIDevice*
		// Le monde ECS a dessiner (ecs::NkWorld*). Sans lui, tout est no-op.
		void NogeeViewport3DBindWorld(void *world);

		// ── LA SELECTION DE L'EDITEUR, ET PAS UNE SECONDE ────────────────────
		// On lie le `NkSelectionManager` QUE L'OUTLINER ET DETAILS LISENT DEJA
		// (NogeeShell.cpp : `sOutliner.Bind(..., &sSel, ...)`,
		// `sDetails.Bind(..., &sSel, ...)`, `sViewport.Bind(..., &sSel, ...)`).
		// Un objet, trois lecteurs : la vue ne PEUT PAS diverger de l'arbre,
		// parce qu'il n'y a rien dont diverger. Se garder ici une copie locale de
		// « l'entite selectionnee » aurait cree exactement la seconde source
		// qu'une selection partagee doit eviter.
		void NogeeViewport3DBindSelection(void *selectionManager); // NkSelectionManager*

		// Force le montage de la pile (renderer + cible + camera) MAINTENANT, au
		// lieu d'attendre la premiere frame. L'hote en a besoin : c'est le seul
		// moyen d'obtenir la poignee du cube avant de creer l'entite TEMOIN.
		bool NogeeViewport3DInit();

		// Vrai quand le renderer, la cible hors ecran et la camera existent.
		bool NogeeViewport3DReady();

		// Poignee GPU de la primitive cube (0 si la pile n'est pas montee). Elle se
		// pose telle quelle dans `NkMeshComponent::meshHandle` : une entite qui la
		// porte n'a AUCUN fichier a charger — le negatif « mesh absent » reste donc
		// un vrai negatif, pas un echec d'import deguise.
		nk_uint64 NogeeViewport3DCubeMeshHandle();

		// ── Par frame ────────────────────────────────────────────────────────
		// Taille de la VUE (le panneau, pas la fenetre). Ne refait la cible que
		// si la taille change vraiment.
		void NogeeViewport3DResize(uint32 w, uint32 h);
		// Rend le monde ECS dans la cible hors ecran, sur le command buffer de
		// l'editeur. Calcule son dt lui-meme.
		void NogeeViewport3DFrame(void *cmd); // NkICommandBuffer*
		// Publie la cible aupres du backend NKGui sous kNogeeViewportTexId.
		void NogeeViewport3DRegisterInto(void *guiBackend); // NkGuiRHIBackend*

		// ── ORIGINE DE LA VUE DANS LA FENETRE ────────────────────────────────
		// Ce que NK3DModeler fait a chaque image (main.cpp l.1426-1433,
		// `Demo3DHostSetView`) et sans quoi le pointage vise un autre pixel que
		// celui qu'on voit : la souris arrive en coordonnees FENETRE, l'image vit
		// dans le rectangle de la vue. Tant que l'ecart n'est pas retranche, un
		// clic « au centre du cube » tombe a cote — d'autant plus loin que les
		// panneaux de gauche sont larges.
		//
		// Le panneau depose son rectangle a chaque image ; le pont le garde pour
		// qui en aura besoin (picking, gizmos, navigation). ⚠️ AUCUNE entree n'est
		// consommee ici : ce lot POSE la traduction, il ne fait pas le pointage.
		void NogeeViewport3DSetView(float32 offX, float32 offY, float32 w, float32 h, bool survol);
		// Souris FENETRE -> souris VUE. Faux si le point tombe hors de la vue.
		bool NogeeViewport3DMouseToView(float32 winX, float32 winY, float32 *outX, float32 *outY);

		// ── LE POINTEUR VISE LE MEME PIXEL QUE L'IMAGE ───────────────────────
		// Les deux sens de la meme transformation, et ils DOIVENT se repondre.
		// Toutes deux travaillent en coordonnees VUE (pixels, origine en haut a
		// gauche de la vue) : c'est `NogeeViewport3DMouseToView` qui amene la
		// souris fenetre jusque-la.
		//
		// ⚠️ Elles lisent les matrices de l'ENTITE CAMERA (`viewProjMatrix`, ecrite
		// par `NkRenderSystem::UpdateActiveCamera`), c'est-a-dire CELLES QUI ONT
		// SERVI A DESSINER l'image. Un rayon calcule depuis un second jeu de
		// matrices « equivalent » serait auto-coherent et pourtant faux a l'ecran :
		// c'est exactement le genre de temoin qui ne temoigne de rien.

		// Rectangle ECRAN de la vue, tel que le panneau l'a MESURE et depose.
		void NogeeViewport3DViewRect(float32 *x, float32 *y, float32 *w, float32 *h);

		// Point MONDE -> pixel de la VUE. Faux si le point est derriere la camera.
		bool NogeeViewport3DProjectToView(const float32 monde[3], float32 *vx, float32 *vy);

		// Pixel de la VUE -> rayon MONDE (origine + direction normalisee).
		// La profondeur NDC choisie est 0, valide dans les DEUX conventions
		// (-1..1 d'OpenGL comme 0..1 de Direct3D) : n'importe quel point du rayon
		// suffit a le definir, puisque son origine est la position de la camera.
		bool NogeeViewport3DRayFromView(float32 vx, float32 vy, float32 origine[3], float32 direction[3]);

		// ── SELECTION : QUEL OBJET SOUS CE PIXEL ─────────────────────────────
		// Parcours ECS sur le CPU, AABB puis triangles, le plus proche gagne.
		// Arbitrage de Rodolf (13/09) : le CPU d'abord — le tampon d'identifiants
		// GPU ajoute une passe et une relecture, donc un cout en images par
		// seconde, pour une exactitude au pixel dont un editeur n'a pas besoin
		// tant que les scenes sont petites. Et c'est REVERSIBLE : cette interface
		// ne changera pas le jour ou le tampon arrivera.
		//
		// Ne sont candidates que les entites REELLEMENT DESSINEES — meme predicat
		// que `NkRenderSystem::SubmitMeshes` : NkTransform + NkMeshComponent +
		// NkMaterialComponent, sans NkInactive, `visible` vrai. On ne selectionne
		// pas ce qu'on ne voit pas.
		//
		// `precision` dit COMMENT la reponse a ete obtenue, parce que les deux ne
		// se valent pas et que l'appelant a le droit de le savoir :
		//     1 = triangle exact       0 = boite englobante seule
		// ⚠️ Un mesh passe par `NkMeshSystem::Import` n'a PAS de copie CPU
		// (`keepCPU` reste faux — NkMeshSystem.h l.96-100) : un asset glisse depuis
		// le Content Browser se selectionne donc A LA BOITE, pas au triangle.
		// C'est mesure, pas suppose.
		//
		// Rend faux si rien n'est touche. `entite` recoit un NkEntityId empaquete
		// (`NkEntityId::Pack()`), 0 si aucune.
		bool NogeeViewport3DPick(float32 vx, float32 vy, nk_uint64 *entite, float32 *distance,
								 int32 *precision);

		// ── Camera d'orbite (critere n2 : la camera commande la vue) ──────────
		void NogeeViewport3DOrbit(float32 dYawDeg, float32 dPitchDeg, float32 dZoom);
		void NogeeViewport3DSetOrbit(float32 yawDeg, float32 pitchDeg);
		void NogeeViewport3DGetOrbit(float32 *yawDeg, float32 *pitchDeg);

		// Controle positif interne : un cube soumis A LA MAIN dans la meme scene,
		// juste apres le pont ECS. Il partage toutes les causes du cube ECS sauf
		// le pont lui-meme : s'il se voit et que le cube ECS ne se voit pas, le
		// defaut est dans le pont ; si aucun des deux ne se voit, il est dans le
		// montage de l'hote. Un viewport uniforme ne designe aucun coupable.
		void NogeeViewport3DControle(bool on);

		// ── L'INSTRUMENT DE MESURE ───────────────────────────────────────────
		// Demande l'ecriture de la N-ieme image RENDUE dans un fichier. Ce n'est
		// PAS une capture d'ecran : la cible hors ecran est relue telle quelle
		// (`NkOffscreenTarget::Capture`), donc le fichier ne contient QUE le
		// viewport — ni fenetre, ni panneaux, ni curseur, ni gigue de mise en
		// page. C'est la seule facon de comparer deux executions pixel a pixel
		// sans mesurer autre chose que la scene.
		void NogeeViewport3DCaptureAt(int32 numeroImage, const char *chemin);

		// ── Temoins numeriques (la sonde les lit, personne ne les devine) ─────
		// Nombre de draw calls que NkRenderSystem a soumis a la derniere frame.
		int32 NogeeViewport3DDrawCount();
		// Nombre de frames rendues depuis le montage.
		int32 NogeeViewport3DFrameCount();

	} // namespace noge
} // namespace nkentseu
