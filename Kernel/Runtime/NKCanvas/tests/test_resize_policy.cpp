// =============================================================================
// test_resize_policy.cpp
//
// Que devient l'image quand la fenetre change de taille ? Avant le 2026-09-05,
// NKCanvas repondait DEUX choses selon un critere que personne ne pouvait
// deviner : la vue par defaut suivait la fenetre, donc on voyait plus de monde ;
// une vue posee par SetView restait intacte, donc la meme scene se retrouvait
// etiree. Aucun des deux comportements n'etait annonce, et rien ne posait de
// bandes noires.
//
// Pire, « suis-je sur la vue par defaut ? » se decidait en comparant les champs
// en EGALITE FLOTTANTE EXACTE. Deux pieges silencieux en decoulaient :
//   - SetView(GetDefaultView()), geste parfaitement raisonnable, n'etait pas vu
//     comme une vue custom ;
//   - une camera valant par hasard la vue par defaut, ce qui arrive tout le
//     temps au demarrage, se faisait confisquer au premier redimensionnement.
//
// Ces tests fixent le contrat des six politiques, et celui du drapeau.
// Ils sont purement geometriques : aucun contexte graphique n'est necessaire.
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKCanvas/Renderer/Batch/NkBatchRenderer2D.h"

#include <cstring>

using namespace nkentseu;
using namespace nkentseu::renderer;

namespace {

	// Le plus petit renderer concret possible : NkBatchRenderer2D ne laisse
	// abstraits que la soumission de geometrie et l'envoi de la projection, et
	// toute la logique de vue et de viewport est dans la classe de base.
	class RendererEssai final : public NkBatchRenderer2D {
		public:
			bool Initialize(NkIGraphicsContext *) override {
				return true;
			}
			void Shutdown() override {
			}
			bool IsValid() const override {
				return true;
			}
			void Clear(const NkColor2D &) override {
			}

			uint32 projections = 0; ///< combien de fois la projection a ete envoyee

		protected:
			void BeginBackend() override {
			}
			void EndBackend() override {
			}
			void SubmitBatches(const NkBatchGroup *, uint32, const NkVertex2D *, uint32, const uint32 *,
							   uint32) override {
			}
			void UploadProjection(const float32[16]) override {
				++projections;
			}
	};

	// Reference 400x300 dans une fenetre 800x400 : le rapport ne correspond pas,
	// donc chaque politique donne un resultat different et reconnaissable.
	const float32 REF_L = 400.f;
	const float32 REF_H = 300.f;

	RendererEssai *Preparer(RendererEssai &r) {
		r.OnResize(800, 400);
		return &r;
	}

} // namespace

// ── Le drapeau de vue custom ────────────────────────────────────────────────

TEST_CASE(NKCanvasResizePolicy, VueParDefautSuitLaFenetre) {
	RendererEssai r;
	Preparer(r);
	ASSERT_FALSE(r.IsViewCustom());

	r.OnResize(1000, 500);
	ASSERT_EQUAL(1000.f, r.GetView().size.x);
	ASSERT_EQUAL(500.f, r.GetView().size.y);
}

TEST_CASE(NKCanvasResizePolicy, SetViewDeLaVueParDefautCompteCommeCustom) {
	// C'est le premier piege : ce geste ne comptait pas, parce que les champs
	// etaient egaux au flottant pres.
	RendererEssai r;
	Preparer(r);
	r.SetView(r.GetDefaultView());
	ASSERT_TRUE(r.IsViewCustom());
}

TEST_CASE(NKCanvasResizePolicy, CameraEgaleParHasardALaVueParDefautNestPasConfisquee) {
	// Second piege : une camera qui vaut par hasard la vue par defaut. Elle
	// existe au demarrage de presque tous les jeux, et elle etait recalee.
	RendererEssai r;
	Preparer(r);

	NkView2D camera;
	camera.center = {400.f, 200.f}; // exactement le centre de la fenetre
	camera.size = {800.f, 400.f};	// exactement sa taille
	camera.rotation = 0.f;
	r.SetView(camera);
	ASSERT_TRUE(r.IsViewCustom());

	// La politique par defaut fait suivre la TAILLE, mais le centre reste celui
	// de l'utilisateur : sa camera n'est pas remplacee par la vue par defaut.
	r.SetView(NkView2D{{120.f, 90.f}, {800.f, 400.f}, 0.f});
	r.OnResize(1000, 500);
	ASSERT_EQUAL(120.f, r.GetView().center.x);
	ASSERT_EQUAL(90.f, r.GetView().center.y);
}

TEST_CASE(NKCanvasResizePolicy, ResetViewRendLaMainAuSuiviAutomatique) {
	RendererEssai r;
	Preparer(r);
	r.SetView(NkView2D{{10.f, 20.f}, {100.f, 50.f}, 0.f});
	ASSERT_TRUE(r.IsViewCustom());

	r.ResetView();
	ASSERT_FALSE(r.IsViewCustom());
	ASSERT_EQUAL(800.f, r.GetView().size.x);
	ASSERT_EQUAL(400.f, r.GetView().size.y);
}

// ── Les six politiques ──────────────────────────────────────────────────────

TEST_CASE(NKCanvasResizePolicy, SuivreLaFenetre) {
	RendererEssai r;
	Preparer(r);
	r.SetResizePolicy(NkResizePolicy::NK_FOLLOW_WINDOW, NkVec2f{REF_L, REF_H});

	// Un pixel reste un pixel : la vue vaut la fenetre, le viewport est plein.
	ASSERT_EQUAL(800.f, r.GetView().size.x);
	ASSERT_EQUAL(400.f, r.GetView().size.y);
	ASSERT_EQUAL(0, r.GetViewport().left);
	ASSERT_EQUAL(800, r.GetViewport().width);
	ASSERT_EQUAL(400, r.GetViewport().height);
}

TEST_CASE(NKCanvasResizePolicy, Etirer) {
	RendererEssai r;
	Preparer(r);
	r.SetResizePolicy(NkResizePolicy::NK_STRETCH, NkVec2f{REF_L, REF_H});

	// La vue garde la taille de reference, le viewport prend toute la fenetre :
	// c'est ce couple qui produit la deformation.
	ASSERT_EQUAL(REF_L, r.GetView().size.x);
	ASSERT_EQUAL(REF_H, r.GetView().size.y);
	ASSERT_EQUAL(REF_L * 0.5f, r.GetView().center.x);
	ASSERT_EQUAL(REF_H * 0.5f, r.GetView().center.y);
	ASSERT_EQUAL(800, r.GetViewport().width);
	ASSERT_EQUAL(400, r.GetViewport().height);
}

TEST_CASE(NKCanvasResizePolicy, AjusterAvecBandes) {
	RendererEssai r;
	Preparer(r);
	r.SetResizePolicy(NkResizePolicy::NK_FIT_LETTERBOX, NkVec2f{REF_L, REF_H});

	// echelle = min(800/400, 400/300) = 1.333 ; 400*1.333 = 533, 300*1.333 = 400.
	ASSERT_EQUAL(533, r.GetViewport().width);
	ASSERT_EQUAL(400, r.GetViewport().height);
	ASSERT_EQUAL(133, r.GetViewport().left); // (800 - 533) / 2
	ASSERT_EQUAL(0, r.GetViewport().top);
	// Toute la reference reste visible.
	ASSERT_EQUAL(REF_L, r.GetView().size.x);
	ASSERT_EQUAL(REF_H, r.GetView().size.y);
}

TEST_CASE(NKCanvasResizePolicy, AjusterEnRognant) {
	RendererEssai r;
	Preparer(r);
	r.SetResizePolicy(NkResizePolicy::NK_FIT_CROP, NkVec2f{REF_L, REF_H});

	// Fenetre en 2:1, reference en 4:3 : on garde la largeur et l'on rogne en
	// hauteur. 400 / 2 = 200 lignes de monde visibles sur 300.
	ASSERT_EQUAL(REF_L, r.GetView().size.x);
	ASSERT_EQUAL(200.f, r.GetView().size.y);
	// Le viewport, lui, prend toute la fenetre : rien n'est laisse vide.
	ASSERT_EQUAL(800, r.GetViewport().width);
	ASSERT_EQUAL(400, r.GetViewport().height);
}

TEST_CASE(NKCanvasResizePolicy, EchelleEntiere) {
	RendererEssai r;
	Preparer(r);
	r.SetResizePolicy(NkResizePolicy::NK_INTEGER_SCALE, NkVec2f{REF_L, REF_H});

	// 1.333 arrondi a 1 : la reference occupe exactement 400x300 pixels.
	ASSERT_EQUAL(400, r.GetViewport().width);
	ASSERT_EQUAL(300, r.GetViewport().height);
	ASSERT_EQUAL(200, r.GetViewport().left); // (800 - 400) / 2
	ASSERT_EQUAL(50, r.GetViewport().top);	 // (400 - 300) / 2
}

TEST_CASE(NKCanvasResizePolicy, EchelleEntiereDeuxFois) {
	RendererEssai r;
	r.OnResize(1600, 900);
	r.SetResizePolicy(NkResizePolicy::NK_INTEGER_SCALE, NkVec2f{REF_L, REF_H});

	// min(1600/400, 900/300) = min(4, 3) = 3.
	ASSERT_EQUAL(1200, r.GetViewport().width);
	ASSERT_EQUAL(900, r.GetViewport().height);
	ASSERT_EQUAL(200, r.GetViewport().left);
	ASSERT_EQUAL(0, r.GetViewport().top);
}

TEST_CASE(NKCanvasResizePolicy, EchelleEntiereSousLaReferenceRetombeSurLAjustement) {
	// La fenetre est plus petite que la reference : l'entier vaudrait zero, et
	// l'image disparaitrait. On garde alors l'ajustement exact.
	RendererEssai r;
	r.OnResize(200, 150);
	r.SetResizePolicy(NkResizePolicy::NK_INTEGER_SCALE, NkVec2f{REF_L, REF_H});

	ASSERT_EQUAL(200, r.GetViewport().width);
	ASSERT_EQUAL(150, r.GetViewport().height);
}

TEST_CASE(NKCanvasResizePolicy, ManuelNeToucheARien) {
	RendererEssai r;
	Preparer(r);
	const NkView2D camera{{111.f, 222.f}, {333.f, 444.f}, 0.5f};
	r.SetView(camera);
	r.SetResizePolicy(NkResizePolicy::NK_MANUAL, NkVec2f{REF_L, REF_H});

	r.OnResize(1234, 567);
	ASSERT_EQUAL(111.f, r.GetView().center.x);
	ASSERT_EQUAL(333.f, r.GetView().size.x);
	ASSERT_EQUAL(444.f, r.GetView().size.y);
	// Le viewport n'est pas mis a jour non plus : c'est le contrat, et c'est le
	// prix du controle total.
	ASSERT_EQUAL(800, r.GetViewport().width);
}

// ── Ce qui doit rester vrai quelle que soit la politique ────────────────────

TEST_CASE(NKCanvasResizePolicy, LaVueParDefautDitToujoursLaVeriteSurLEcran) {
	// Meme sous une politique d'ajustement, GetDefaultView() doit rester
	// utilisable pour dessiner une interface en coordonnees ecran.
	RendererEssai r;
	Preparer(r);
	r.SetResizePolicy(NkResizePolicy::NK_FIT_LETTERBOX, NkVec2f{REF_L, REF_H});
	r.OnResize(1024, 768);

	ASSERT_EQUAL(1024.f, r.GetDefaultView().size.x);
	ASSERT_EQUAL(768.f, r.GetDefaultView().size.y);
	ASSERT_EQUAL(512.f, r.GetDefaultView().center.x);
	ASSERT_EQUAL(384.f, r.GetDefaultView().center.y);
}

TEST_CASE(NKCanvasResizePolicy, SansReferenceUnAjustementRetombeSurLeSuivi) {
	// Une politique d'ajustement sans taille de reference n'a aucun sens. Plutot
	// que de dessiner n'importe quoi, on suit la fenetre.
	RendererEssai r;
	Preparer(r);
	r.SetResizePolicy(NkResizePolicy::NK_FIT_LETTERBOX, NkVec2f{0.f, 0.f});

	// La reference retombe sur la vue courante, donc l'ajustement est exact et
	// le viewport reste plein-cadre.
	ASSERT_EQUAL(800, r.GetViewport().width);
	ASSERT_EQUAL(400, r.GetViewport().height);
}

TEST_CASE(NKCanvasResizePolicy, LaPolitiqueSAppliqueSansAttendreUnRedimensionnement) {
	// Sans application immediate, regler la politique ne se verrait qu'au premier
	// redimensionnement, et un jeu lance en plein ecran ne la verrait jamais.
	RendererEssai r;
	Preparer(r);
	ASSERT_EQUAL(800, r.GetViewport().width);

	r.SetResizePolicy(NkResizePolicy::NK_INTEGER_SCALE, NkVec2f{REF_L, REF_H});
	ASSERT_EQUAL(400, r.GetViewport().width); // change tout de suite
}

TEST_CASE(NKCanvasResizePolicy, LeNomDeChaquePolitiqueEstLisible) {
	ASSERT_TRUE(std::strcmp("FollowWindow", NkResizePolicyName(NkResizePolicy::NK_FOLLOW_WINDOW)) == 0);
	ASSERT_TRUE(std::strcmp("Stretch", NkResizePolicyName(NkResizePolicy::NK_STRETCH)) == 0);
	ASSERT_TRUE(std::strcmp("FitLetterbox", NkResizePolicyName(NkResizePolicy::NK_FIT_LETTERBOX)) == 0);
	ASSERT_TRUE(std::strcmp("FitCrop", NkResizePolicyName(NkResizePolicy::NK_FIT_CROP)) == 0);
	ASSERT_TRUE(std::strcmp("IntegerScale", NkResizePolicyName(NkResizePolicy::NK_INTEGER_SCALE)) == 0);
	ASSERT_TRUE(std::strcmp("Manual", NkResizePolicyName(NkResizePolicy::NK_MANUAL)) == 0);
}
