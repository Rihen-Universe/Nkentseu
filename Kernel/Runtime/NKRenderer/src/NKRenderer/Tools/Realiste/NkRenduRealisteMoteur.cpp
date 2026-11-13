// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/Realiste/NkRenduRealisteMoteur.cpp
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// (13/11/2026) L'API du rendu realiste dans le moteur : voir l'en-tete. Chaque calcul est DEPLACE de
// NKCraft/Render/NkRenduRealiste.cpp a l'identique (memes operations, memes types : le temoin au pixel le verifie).
// -----------------------------------------------------------------------------

#include "NKRenderer/Tools/Realiste/NkRenduRealisteMoteur.h"
#include "NKRenderer/Tools/Realiste/NkArgileReglages.h"
#include "NKRenderer/Tools/Environment/NkEnvironmentSystem.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/NkRenderer.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKMath/NkFunctions.h"
#include "NKPlatform/NkEnv.h"

namespace nkentseu {
	namespace renderer {

		NkRrEffetAmbiance NkRrEffetDeLAmbiance(const NkAmbiance &ambiance) {
			// NK_AMBIANCE_MUTATION=1 : la contre-epreuve du banc sans fenetre -- le jour, quelle que soit l'ambiance
			// (NKCraft, lui, refuse deja la pose avant d'arriver ici sous la meme variable)
			static const bool mutation = env::GetEnvVar("NK_AMBIANCE_MUTATION") != nullptr;
			const NkAmbiance &a = mutation ? NkAmbianceDe(0) : ambiance;
			NkRrEffetAmbiance r;
			r.soleil[0] = a.soleilAz;
			r.soleil[1] = a.soleilEl > 0.5f ? a.soleilEl : 0.5f;
			r.soleil[2] = a.soleilForce;
			r.turbidite = a.turbidite;
			r.cielAffiche = a.cielAffiche;
			r.ambianceDesc = a.ambianceDesc;
			if (a.soleil) {
				r.descSoleil = true;
				const float32 ds[6] = {a.soleilAz, a.soleilEl, a.soleilLux, a.soleilCoul[0], a.soleilCoul[1], a.soleilCoul[2]};
				for (int32 k = 0; k < 6; ++k)
					r.descSol[k] = ds[k];
			} else {
				// la nuit : ni soleil ni ciel (banc du 13/11 : le ciel « crepuscule » de la description restait clair
				// derriere la vitre, et l'exposition automatique le brulait en blanc)
				r.descSoleil = false;
				r.sansSoleil = true;
				r.sansCiel = true;
			}
			const char *j = "jour";
			const char *d = a.ambianceDesc ? a.ambianceDesc : "";
			int32 i = 0;
			while (d[i] && d[i] == j[i])
				++i;
			r.sansEmission = d[i] == 0 && j[i] == 0;
			r.expoAuto = true;
			r.cle = a.cle;
			r.contraste = a.contraste;
			r.saturation = a.saturation;
			r.chaleur = a.chaleur;
			r.indexAmbiance = NkAmbianceIndex(a.nom);
			r.sppMinTraceur = a.sppMinTraceur;
			return r;
		}

		void NkRrTransmissionSoleil(float32 elevDeg, float32 turb, float32 *rgb) {
			const float64 el = elevDeg < 0.5f ? 0.5 : (float64)elevDeg;
			const float64 z = 90.0 - el;
			const float64 cz = math::NkCos(z * 0.017453292519943295);
			const float64 m = 1.0 / (cz + 0.50572 * math::NkPow(96.07995 - z, -1.6364));
			float64 beta = 0.04608 * (float64)turb - 0.04586;
			if (beta < 0.0)
				beta = 0.0;
			const float64 lam[3] = {0.680, 0.550, 0.440};
			for (int32 i = 0; i < 3; ++i) {
				const float64 tauR = 0.008735 * math::NkPow(lam[i], -4.08);
				const float64 tauA = beta * math::NkPow(lam[i], -1.3);
				rgb[i] = (float32)math::NkExp(-m * (tauR + tauA));
			}
		}

		NkRrSoleilTempsReel NkRrCalculerSoleil(float32 azimutDeg, float32 hauteurDeg, float32 force, float32 turbidite,
											   bool physique, bool blanc) {
			NkRrSoleilTempsReel s;
			const float32 az = azimutDeg * 0.017453292f, el = hauteurDeg * 0.017453292f;
			const float32 ce = (float32)math::NkCos((float64)el), se = (float32)math::NkSin((float64)el);
			const float32 ca = (float32)math::NkCos((float64)az), sa = (float32)math::NkSin((float64)az);
			s.dir[0] = -ce * sa;
			s.dir[1] = -se;
			s.dir[2] = -ce * ca;
			float32 tr[3], tz[3];
			NkRrTransmissionSoleil(hauteurDeg, turbidite, tr);
			NkRrTransmissionSoleil(90.f, turbidite, tz);
			// couleur = transmission relative a celle du zenith, ramenee a un maximum de 1 ; l'intensite suit la
			// transmission verte (l'oeil, puis l'exposition, s'y font)
			float32 coul[3] = {tr[0] / tz[0], tr[1] / tz[1], tr[2] / tz[2]};
			const float32 mxc = coul[0] > coul[1] ? (coul[0] > coul[2] ? coul[0] : coul[2]) : (coul[1] > coul[2] ? coul[1] : coul[2]);
			for (int32 a = 0; a < 3; ++a)
				coul[a] /= (mxc > 1e-4f ? mxc : 1e-4f);
			// L'INTENSITE : le ciel du moteur est NORMALISE en exposition (coef[0].w, Hosek) -- il ne s'assombrit pas
			// au couchant comme le vrai (mesure du 09/11, `r5`). Par defaut la COULEUR suit la transmission, pas
			// l'intensite ; `physique` : les deux.
			s.intensite = force * (physique ? (tr[1] / tz[1]) : 1.f);
			if (blanc)
				coul[0] = coul[1] = coul[2] = 1.f;
			for (int32 a = 0; a < 3; ++a)
				s.couleur[a] = coul[a];
			return s;
		}

		NkRrNuitTempsReel NkRrCalculerNuit(bool sansCiel, bool prereglee) {
			static const bool mutation = env::GetEnvVar("NK_NUIT_MUTATION") != nullptr;
			NkRrNuitTempsReel n;
			n.cielCache = sansCiel && prereglee;
			n.nuit = sansCiel && prereglee && !mutation;
			n.fondNuit = n.nuit;
			return n;
		}

		NkRrAmbiantTempsReel NkRrCalculerAmbiant(float32 force, bool sansIbl, bool nuit) {
			NkRrAmbiantTempsReel a;
			const float32 f = (sansIbl || nuit) ? 0.f : force;
			a.intensite = f > 2.f ? 2.f : f;
			const float32 fc = f > 2.f ? f / 2.f : 1.f;
			a.teinte[0] = a.teinte[1] = a.teinte[2] = fc;
			return a;
		}

		float32 NkRrCleTempsReel(float32 cleTraceur) {
			return cleTraceur > 0.f ? cleTraceur * kNkArgileCleTempsReel : 0.18f;
		}

		void NkRrSoleilDescription(NkScArchviz &s, const float32 *sol) {
			const float32 az = sol[0] * 0.017453292f, el = sol[1] * 0.017453292f;
			s.soleilDir[0] = math::NkCos(el) * math::NkSin(az);
			s.soleilDir[1] = math::NkSin(el);
			s.soleilDir[2] = math::NkCos(el) * math::NkCos(az);
			s.soleilE = sol[2] * 1e-3f;
			for (int32 q = 0; q < 3; ++q)
				s.soleilCouleur[q] = sol[3 + q];
		}

		float32 NkRrCielPhysique(NkScArchviz &s, float32 turbidite) {
			NkSkyParams sp;
			sp.model = NkSkyModel::NK_SKY_HOSEK;
			sp.sunDirection = {-s.soleilDir[0], -s.soleilDir[1], -s.soleilDir[2]};
			sp.turbidity = turbidite;
			sp.sunIntensity = 1.f;
			NkVector<float32> t;
			NkHosekCielTable(sp, 1.f, kNkRrCielW, kNkRrCielH, t);
			// l'eclairement horizontal, du degrade d'avant et du ciel physique
			float64 eAvant = 0.0, eHosek = 0.0;
			for (int32 j = 0; j < kNkRrCielH / 2; ++j) {
				const float32 th = 3.14159265f * ((float32)j + 0.5f) / (float32)kNkRrCielH;
				const float32 w = math::NkCos(th) * math::NkSin(th);
				const float32 k = math::NkSqrt(math::NkCos(th));
				float32 g[3];
				for (int32 q = 0; q < 3; ++q)
					g[q] = (s.cielHorizon[q] * (1.f - k) + s.cielZenith[q] * k) * s.cielIntensite;
				eAvant += (float64)(w * (0.2126f * g[0] + 0.7152f * g[1] + 0.0722f * g[2])) * (float64)kNkRrCielW;
				for (int32 i = 0; i < kNkRrCielW; ++i) {
					const float32 *p = t.Data() + ((usize)j * (usize)kNkRrCielW + (usize)i) * 3u;
					eHosek += (float64)(w * (0.2126f * p[0] + 0.7152f * p[1] + 0.0722f * p[2]));
				}
			}
			const float32 f = eHosek > 1e-12 ? (float32)(eAvant / eHosek) : 1.f;
			for (usize i = 0; i < t.Size(); ++i)
				t[i] *= f;
			s.cielTable = t;
			s.cielW = kNkRrCielW;
			s.cielH = kNkRrCielH;
			s.cielIntensite = 1.f;
			return f;
		}

		void NkRrPoserAmbianceTraceur(const NkRrEffetAmbiance &fx, NkScArchviz &s, NkTcReglages &r) {
			if (fx.descSoleil) {
				// le soleil de l'ambiance, puis son CIEL : la luminosite vue de l'ambiance (le couchant 0,5) et le ciel
				// physique a sa turbidite autour de ce soleil (au couchant : chaud pres de l'horizon) -- sans lui, un
				// degrade fixe garde la lumiere du jour quand le soleil tombe a 6 000 lux (banc sans fenetre du 13/11 :
				// le couchant sortait PLUS BLEU que le jour, R/B 0,82)
				NkRrSoleilDescription(s, fx.descSol);
				s.cielIntensite *= fx.cielAffiche;
				(void)NkRrCielPhysique(s, fx.turbidite);
			}
			r.sansSoleil = fx.sansSoleil;
			r.sansCiel = fx.sansCiel;
			r.expoAuto = fx.expoAuto;
			r.cleExpo = fx.cle;
			r.fin.contraste = fx.contraste;
			r.fin.saturation = fx.saturation;
			r.fin.chaleur = fx.chaleur;
			if (fx.sppMinTraceur > r.spp)
				r.spp = fx.sppMinTraceur;
		}

		void NkRrAjusterSceneSondes(NkScArchviz &s, bool soleilBlanc, bool sansSoleil, bool sansCiel) {
			if (soleilBlanc) // (12/11) argile, jour neutre : le soleil blanc
				s.soleilCouleur[0] = s.soleilCouleur[1] = s.soleilCouleur[2] = 1.f;
			// (13/11) la NUIT (ambiance prereglee : sans soleil, sans ciel) : les sondes ne cuisent ni l'un ni l'autre
			// (le banc des ambiances : le sejour de nuit du temps reel restait eclaire comme le jour)
			if (sansSoleil)
				s.soleilE = 0.f;
			if (sansCiel)
				s.cielIntensite = 0.f;
		}

		bool NkRrMonterSondes(NkRenderer *r, const NkSdGrille &g, const NkSdVolume &v, float32 mode, NkRrSondesGPU &gpu) {
			NkIDevice *dev = r ? r->GetDevice() : nullptr;
			if (!dev || !r->GetRender3D())
				return false;
			if (!gpu.tex.IsValid() || gpu.W != v.W || gpu.H != v.H || gpu.D != v.D) {
				if (gpu.tex.IsValid())
					dev->DestroyTexture(gpu.tex);
				NkTextureDesc td = NkTextureDesc::Tex3D((uint32)v.W, (uint32)v.H, (uint32)v.D, NkGPUFormat::NK_RGBA16_FLOAT);
				td.debugName = "SondesIrradiance";
				gpu.tex = dev->CreateTexture(td);
				gpu.W = v.W;
				gpu.H = v.H;
				gpu.D = v.D;
			}
			if (!gpu.samp.IsValid())
				gpu.samp = dev->CreateSampler(NkSamplerDesc::Clamp());
			if (!gpu.tex.IsValid() || !gpu.samp.IsValid() || !dev->WriteTexture(gpu.tex, v.texels.Data()))
				return false;
			r->GetRender3D()->SetSondes(
				gpu.tex, gpu.samp, math::NkVec4f{g.origine[0], g.origine[1], g.origine[2], g.pas},
				math::NkVec4f{g.origine[0] + g.pas * (float32)(g.nx - 1), g.origine[1] + g.pas * (float32)(g.ny - 1),
							  g.origine[2] + g.pas * (float32)(g.nz - 1), mode});
			return true;
		}

	} // namespace renderer
} // namespace nkentseu
