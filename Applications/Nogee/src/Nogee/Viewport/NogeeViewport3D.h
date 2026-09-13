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
