// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkRenderStyle.cpp -- voir l'en-tête. Les valeurs sont écrites ICI, une fois,
// et chacune dit pourquoi.
// =============================================================================
#include "NKRenderer/Materials/NkRenderStyle.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"

namespace nkentseu {
	namespace renderer {

		void NkStyledMaterial::ApplyTo(NkDrawCall3D &dc) const {
			if (instance)
				dc.material = instance->GetHandle();
			dc.tint = tint;
			dc.metallic = metallic;
			dc.roughness = roughness;
			dc.subsurface = subsurface;
			dc.subsurfaceColor = subsurfaceColor;
		}

		const char *NkRenderStyleName(NkRenderStyle s) {
			switch (s) {
				case NkRenderStyle::NK_STYLE_REALISTE:
					return "realiste";
				case NkRenderStyle::NK_STYLE_CARTOON:
					return "cartoon";
				case NkRenderStyle::NK_STYLE_RIHEN:
					return "rihen";
			}
			return "?";
		}

		void NkApplyRenderStyle(NkPostConfig &pp, NkRenderStyle s) {
			switch (s) {
				case NkRenderStyle::NK_STYLE_REALISTE:
					// L'image telle que la lumière la fait : aucune retouche.
					pp.bloom = true;
					pp.ssao = true;
					pp.colorGrading = false;
					pp.saturation = 1.f;
					break;
				case NkRenderStyle::NK_STYLE_CARTOON:
					// Aplats : pas d'occlusion d'écran (elle salirait les aplats
					// d'un dégradé) ni de halo ; couleurs franches.
					pp.bloom = false;
					pp.ssao = false;
					pp.colorGrading = true;
					// Le tonemap ACES desature les teintes franches (son pied
					// releve les canaux faibles) : 1,3 rend au cel sa couleur.
					pp.saturation = 1.3f;
					break;
				case NkRenderStyle::NK_STYLE_RIHEN:
					// Les matières du réaliste, une image un peu plus vive.
					pp.bloom = true;
					pp.ssao = true;
					pp.colorGrading = true;
					pp.saturation = 1.08f;
					break;
			}
		}

		namespace {

			float32 RugositeParDefaut(NkMatiere m) {
				switch (m) {
					case NkMatiere::NK_MATIERE_PLASTIQUE:
						return 0.45f;
					case NkMatiere::NK_MATIERE_METAL:
						return 0.3f;
					case NkMatiere::NK_MATIERE_PEAU:
						return 0.5f;
					case NkMatiere::NK_MATIERE_TISSU:
						return 0.75f;
				}
				return 0.5f;
			}

			// Matière RÉALISTE : le modèle physique de chaque matière.
			NkStyledMaterial Realiste(NkMaterialSystem *mats, const NkStyledMaterialDesc &d) {
				NkStyledMaterial r;
				const float32 rug = d.rugosite >= 0.f ? d.rugosite : RugositeParDefaut(d.matiere);
				r.roughness = rug;
				if (d.matiere == NkMatiere::NK_MATIERE_TISSU) {
					// Le tissu lit SA rugosité et son duvet dans l'instance.
					r.instance = mats->CreateInstance(mats->DefaultCloth());
					if (r.instance) {
						r.instance->SetAlbedo(d.couleur);
						r.instance->SetRoughness(rug);
						r.instance->SetSheen(0.3f);
					}
					return r;
				}
				// Plastique, métal, peau : le PBR. La peau d'un maillage STATIQUE
				// passe par le PBR et sa diffusion (le pipeline Skin ne dessine que
				// le chemin skinné) ; soumise par SubmitSkinned, la même instance
				// sert au nuanceur de peau, qui lit ces paramètres dans le matériau.
				r.instance = mats->CreateInstance(mats->DefaultPBR());
				if (r.instance) {
					r.instance->SetAlbedo(d.couleur);
					r.instance->SetRoughness(rug);
				}
				// LA COULEUR DU PBR EST LA TEINTE D'OBJET : pbr.frag prend
				// texture x vColor (= couleur de sommet x uObj.tint) et NE LIT PAS
				// uMat.albedo (mesure : une instance PBR « rouge » sortait blanche
				// sur la premiere planche). Le tissu, lui, multiplie les deux
				// (texture x uMat.albedo x vColor) : sa teinte reste a 1.
				r.tint = d.couleur;
				if (d.matiere == NkMatiere::NK_MATIERE_METAL) {
					r.metallic = 1.f;
					if (r.instance)
						r.instance->SetMetallic(1.f);
				} else if (d.matiere == NkMatiere::NK_MATIERE_PEAU) {
					// Diffusion modérée, teinte rouge du sang sous la peau.
					r.subsurface = 0.35f;
					r.subsurfaceColor = {1.f, 0.45f, 0.3f};
					if (r.instance)
						r.instance->SetSubsurface(r.subsurface, r.subsurfaceColor);
				}
				return r;
			}

			// Matière CARTOON : le Toon, réglé par matière.
			NkStyledMaterial Cartoon(NkMaterialSystem *mats, const NkStyledMaterialDesc &d) {
				NkStyledMaterial r;
				r.roughness = d.rugosite >= 0.f ? d.rugosite : RugositeParDefaut(d.matiere);
				r.instance = mats->CreateInstance(mats->DefaultToon());
				NkMaterialInstance *t = r.instance;
				if (!t)
					return r;
				t->SetAlbedo(d.couleur);
				// Deux tons, bord net mais pas crénelé (0,02 de N.L).
				t->SetToonThreshold(0.3f);
				t->SetToonSmooth(0.02f);
				t->SetToonBands(2.f);
				// Contour noir légèrement bleuté (un noir pur « troue » l'image).
				t->SetHullOutline(1.6f, {0.05f, 0.04f, 0.07f});
				// L'ombre du Toon MULTIPLIE l'albédo : une ombre froide pour les
				// matières inertes, chaude pour la peau (le sang sous la peau).
				switch (d.matiere) {
					case NkMatiere::NK_MATIERE_PLASTIQUE:
						t->SetToonShadowColor({0.45f, 0.42f, 0.62f});
						t->SetSpecHardness(64.f);
						t->SetRim(0.25f, {1.f, 1.f, 1.f});
						break;
					case NkMatiere::NK_MATIERE_METAL:
						// Reflet large et teinté par la couleur (metallic du Toon).
						t->SetToonShadowColor({0.35f, 0.33f, 0.45f});
						t->SetSpecHardness(20.f);
						t->SetMetallic(1.f);
						t->SetRim(0.35f, d.couleur);
						r.metallic = 1.f;
						break;
					case NkMatiere::NK_MATIERE_PEAU:
						t->SetToonShadowColor({0.72f, 0.45f, 0.45f});
						t->SetSpecHardness(160.f);
						t->SetRim(0.15f, {1.f, 0.85f, 0.75f});
						break;
					case NkMatiere::NK_MATIERE_TISSU:
						// Un tissu ne brille pas : reflet réduit à un point.
						t->SetToonShadowColor({0.5f, 0.47f, 0.62f});
						t->SetSpecHardness(2048.f);
						t->SetRim(0.f, {1.f, 1.f, 1.f});
						break;
				}
				return r;
			}

		} // namespace

		NkStyledMaterial NkCreateStyledMaterial(NkMaterialSystem *mats, NkRenderStyle s,
												const NkStyledMaterialDesc &d) {
			if (!mats)
				return {};
			switch (s) {
				case NkRenderStyle::NK_STYLE_REALISTE:
					return Realiste(mats, d);
				case NkRenderStyle::NK_STYLE_CARTOON:
					return Cartoon(mats, d);
				case NkRenderStyle::NK_STYLE_RIHEN: {
					// Matière réaliste, silhouette dessinée : contour fin de la
					// teinte de l'objet assombrie (pas un noir de bande dessinée).
					NkStyledMaterial r = Realiste(mats, d);
					if (r.instance)
						r.instance->SetHullOutline(1.2f, {d.couleur.x * 0.25f, d.couleur.y * 0.25f,
														  d.couleur.z * 0.25f});
					return r;
				}
			}
			return {};
		}

	} // namespace renderer
} // namespace nkentseu
