// -----------------------------------------------------------------------------
// FICHIER: Unkeny/Script/NkUnkenyScripts.cpp
// DESCRIPTION: Le registre des scripts, l'hote qui les fait tourner, et LA
//              table C (NkUnkHoteV1) que Blueprint et C++ appellent.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Unkeny/Script/NkUnkenyScripts.h"

#include "NKMemory/NKMemory.h"
#include "NKSerialization/Asset/NkAssetMetadata.h"
#include "Unkeny/Anim/NkUnkenyAnimateur.h"
#include "Unkeny/Anim/NkUnkenySpriteAnim.h"
#include "Unkeny/Entree/NkUnkenyActions.h"
#include "Unkeny/Entree/NkUnkenyActionsStandard.h"
#include "Unkeny/Entree/NkUnkenyLiaisons.h"
#include "Unkeny/Scene/NkUnkenyScene.h"
#include "Unkeny/Son/NkUnkenySon.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace unkeny {

		// =====================================================================
		// Le registre
		// =====================================================================
		NkScripts2D::~NkScripts2D() {
			Vider();
		}

		void NkScripts2D::Vider() {
			for (uint32 i = 0; i < mDefs.Size(); ++i) {
				if (mDefs[i].programme != nullptr) {
					memory::NkGetDefaultAllocator().Delete(mDefs[i].programme);
					mDefs[i].programme = nullptr;
				}
			}
			mDefs.Clear();
			++mGeneration;
		}

		uint32 NkScripts2D::Trouver(const char *nom) const noexcept {
			for (uint32 i = 0; nom != nullptr && i < mDefs.Size(); ++i) {
				if (std::strcmp(mDefs[i].nom.CStr(), nom) == 0) {
					return i + 1u;
				}
			}
			return 0u;
		}

		const NkDefinitionScript *NkScripts2D::Definition(uint32 id) const noexcept {
			return id >= 1u && id <= mDefs.Size() ? &mDefs[id - 1u] : nullptr;
		}

		uint32 NkScripts2D::Assurer(const char *nom, NkGenreScript genre) {
			uint32 id = Trouver(nom);
			if (id == 0u) {
				NkDefinitionScript d;
				d.nom = nom;
				d.genre = genre;
				d.version = 1u;
				mDefs.PushBack(d);
				id = static_cast<uint32>(mDefs.Size());
			} else {
				NkDefinitionScript &d = mDefs[id - 1u];
				d.genre = genre;
				++d.version;
			}
			++mGeneration;
			return id;
		}

		bool NkScripts2D::EnregistrerModuleCpp(const NkUnkModuleV1 *module, NkString *erreur, const NkUnkModuleV1 *ancien) {
			if (module == nullptr) {
				if (erreur != nullptr) {
					*erreur = "module C++ nul";
				}
				return false;
			}
			if (module->abiMajeure != NK_UNK_ABI_MAJEURE) {
				if (erreur != nullptr) {
					*erreur = NkString::Format("module C++ d'ABI %u.%u, l'hote parle la %u.%u : refuse",
											   static_cast<unsigned>(module->abiMajeure), static_cast<unsigned>(module->abiMineure),
											   static_cast<unsigned>(NK_UNK_ABI_MAJEURE), static_cast<unsigned>(NK_UNK_ABI_MINEURE));
				}
				return false;
			}
			for (uint32 i = 0; i < module->nbClasses; ++i) {
				const NkUnkClasseV1 *c = module->classes[i];
				if (c == nullptr || c->nom == nullptr) {
					continue;
				}
				const NkString nom = NkString(NK_SCRIPT_PREFIXE_CPP) + c->nom;
				const uint32 id = Assurer(nom.CStr(), NkGenreScript::NK_CPP);
				mDefs[id - 1u].classe = c;
				mDefs[id - 1u].erreur.Clear();
			}
			// Les classes de l'ANCIEN module absentes du nouveau : disparues.
			for (uint32 i = 0; ancien != nullptr && i < ancien->nbClasses; ++i) {
				const NkUnkClasseV1 *c = ancien->classes[i];
				if (c == nullptr || c->nom == nullptr) {
					continue;
				}
				bool garde = false;
				for (uint32 k = 0; k < module->nbClasses && !garde; ++k) {
					garde = module->classes[k] != nullptr && module->classes[k]->nom != nullptr &&
							std::strcmp(module->classes[k]->nom, c->nom) == 0;
				}
				if (!garde) {
					const NkString nom = NkString(NK_SCRIPT_PREFIXE_CPP) + c->nom;
					const uint32 id = Assurer(nom.CStr(), NkGenreScript::NK_CPP);
					mDefs[id - 1u].classe = nullptr;
					mDefs[id - 1u].erreur = NkString::Format("la classe %s a disparu du module", c->nom);
				}
			}
			return true;
		}

		uint32 NkScripts2D::EnregistrerBlueprint(const char *nom, const NkModuleBp &module, NkString *erreur) {
			if (nom == nullptr || nom[0] == '\0') {
				return 0u;
			}
			NkProgrammeBp *p = memory::NkGetDefaultAllocator().New<NkProgrammeBp>();
			p->module = module;
			NkRefusBp refus;
			const bool ok = NkPreparerProgrammeBp(*p, refus);
			const uint32 id = Assurer(nom, NkGenreScript::NK_BLUEPRINT);
			NkDefinitionScript &d = mDefs[id - 1u];
			if (d.programme != nullptr) {
				memory::NkGetDefaultAllocator().Delete(d.programme);
			}
			d.programme = p;
			d.erreur.Clear();
			if (!ok) {
				d.erreur = NkString::Format("module refuse : %s", refus.raison.CStr());
				if (refus.fonction >= 0) {
					d.erreur += NkString::Format(" (fonction %d, pc %d, noeud %u)", refus.fonction, refus.pc,
												 static_cast<unsigned>(refus.noeud));
				}
				if (erreur != nullptr) {
					*erreur = d.erreur;
				}
			}
			return id;
		}

		uint32 NkScripts2D::ChargerBlueprintOctets(const char *nom, const uint8 *octets, usize taille, NkString *erreur) {
			NkModuleBp module;
			bool aModule = false;
			NkString err;
			if (!NkLireChargeBp(octets, taille, nullptr, &module, &aModule, &err) || !aModule) {
				const uint32 id = Assurer(nom, NkGenreScript::NK_BLUEPRINT);
				NkDefinitionScript &d = mDefs[id - 1u];
				if (d.programme != nullptr) {
					memory::NkGetDefaultAllocator().Delete(d.programme);
					d.programme = nullptr;
				}
				d.erreur = aModule || !err.Empty() ? err : NkString("Blueprint jamais compile (pas de section MODL)");
				if (erreur != nullptr) {
					*erreur = d.erreur;
				}
				return id;
			}
			return EnregistrerBlueprint(nom, module, erreur);
		}

		uint32 NkScripts2D::ChargerBlueprint(const char *nom, const char *chemin, NkString *erreur) {
			NkAssetMetadata meta;
			NkVector<nk_uint8> charge;
			NkString err;
			if (!NkAssetIO::ReadFull(chemin, meta, charge, &err)) {
				const uint32 id = Assurer(nom, NkGenreScript::NK_BLUEPRINT);
				mDefs[id - 1u].erreur = NkString::Format("%s illisible : %s", chemin, err.CStr());
				if (erreur != nullptr) {
					*erreur = mDefs[id - 1u].erreur;
				}
				return id;
			}
			return ChargerBlueprintOctets(nom, charge.Data(), charge.Size(), erreur);
		}

		// =====================================================================
		// La table C : chaque fonction verifie l'entite, puis agit
		// =====================================================================
		namespace {
			NkHoteScripts2D &H(void *ctx) {
				return *static_cast<NkHoteScripts2D *>(ctx);
			}
			ecs::NkEntityId Id(NkUnkEntite e) {
				return e.pack != 0u ? ecs::NkEntityId::Unpack(e.pack) : ecs::NkEntityId::Invalid();
			}
			/// L'entite vivante, ou Invalid() (et un refus compte).
			ecs::NkEntityId Vive(void *ctx, NkUnkEntite e) {
				NkHoteScripts2D &h = H(ctx);
				const ecs::NkEntityId id = Id(e);
				if (h.Scene() == nullptr || !id.IsValid() || !h.Scene()->Monde().IsAlive(id)) {
					h.CompterRefus();
					return ecs::NkEntityId::Invalid();
				}
				return id;
			}
			bool Fini(float32 v) {
				return v == v && v < 1e30f && v > -1e30f;
			}
			int32 Refus(void *ctx) {
				H(ctx).CompterRefus();
				return 0;
			}

			void TAfficher(void *ctx, NkUnkEntite soi, const char *texte) {
				NkHoteScripts2D &h = H(ctx);
				const char *nom = "?";
				const ecs::NkEntityId id = Id(soi);
				if (h.Scene() != nullptr && id.IsValid() && h.Scene()->Monde().IsAlive(id)) {
					if (const NkEtiquette *e = h.Scene()->Monde().Get<NkEtiquette>(id)) {
						nom = e->nom;
					}
				}
				char ligne[320];
				std::snprintf(ligne, sizeof(ligne), "[%s] %s", nom, texte != nullptr ? texte : "");
				h.Ecrire(ligne, false);
			}
			float TTemps(void *ctx) {
				return H(ctx).Temps();
			}
			int32_t TVivante(void *ctx, NkUnkEntite e) {
				NkHoteScripts2D &h = H(ctx);
				const ecs::NkEntityId id = Id(e);
				return h.Scene() != nullptr && id.IsValid() && h.Scene()->Monde().IsAlive(id) ? 1 : 0;
			}
			int32_t TDetruire(void *ctx, NkUnkEntite e) {
				const ecs::NkEntityId id = Vive(ctx, e);
				if (!id.IsValid()) {
					return 0;
				}
				H(ctx).Scene()->Detruire(id);
				return 1;
			}
			int32_t TParNom(void *ctx, const char *nom, NkUnkEntite *sortie) {
				NkHoteScripts2D &h = H(ctx);
				if (sortie != nullptr) {
					sortie->pack = 0u;
				}
				if (h.Scene() == nullptr || nom == nullptr || nom[0] == '\0') {
					return Refus(ctx);
				}
				ecs::NkEntityId trouve = ecs::NkEntityId::Invalid();
				h.Scene()->Monde().Query<NkEtiquette>().ForEach([&](ecs::NkEntityId id, NkEtiquette &e) {
					if (!trouve.IsValid() && std::strcmp(e.nom, nom) == 0) {
						trouve = id;
					}
				});
				if (!trouve.IsValid()) {
					return Refus(ctx);
				}
				if (sortie != nullptr) {
					sortie->pack = trouve.Pack();
				}
				return 1;
			}
			int32_t TNomEst(void *ctx, NkUnkEntite e, const char *nom) {
				const ecs::NkEntityId id = Vive(ctx, e);
				if (!id.IsValid() || nom == nullptr) {
					return 0;
				}
				const NkEtiquette *et = H(ctx).Scene()->Monde().Get<NkEtiquette>(id);
				return et != nullptr && std::strcmp(et->nom, nom) == 0 ? 1 : 0;
			}
			int32_t TNom(void *ctx, NkUnkEntite e, char *tampon, uint32_t taille) {
				const ecs::NkEntityId id = Vive(ctx, e);
				if (!id.IsValid() || tampon == nullptr || taille == 0u) {
					return 0;
				}
				const NkEtiquette *et = H(ctx).Scene()->Monde().Get<NkEtiquette>(id);
				std::snprintf(tampon, taille, "%s", et != nullptr ? et->nom : "");
				return 1;
			}
			int32_t TActiver(void *ctx, NkUnkEntite e, int32_t actif) {
				const ecs::NkEntityId id = Vive(ctx, e);
				return id.IsValid() && H(ctx).Scene()->Activer(id, actif != 0) ? 1 : 0;
			}
			int32_t TPosition(void *ctx, NkUnkEntite e, float *x, float *y) {
				const ecs::NkEntityId id = Vive(ctx, e);
				const NkTransform2D *t = id.IsValid() ? H(ctx).Scene()->Monde().Get<NkTransform2D>(id) : nullptr;
				if (t == nullptr) {
					return 0;
				}
				if (x != nullptr) {
					*x = t->position.x;
				}
				if (y != nullptr) {
					*y = t->position.y;
				}
				return 1;
			}
			int32_t TTeleporter(void *ctx, NkUnkEntite e, float x, float y) {
				const ecs::NkEntityId id = Vive(ctx, e);
				if (!id.IsValid() || !Fini(x) || !Fini(y)) {
					return id.IsValid() ? Refus(ctx) : 0;
				}
				H(ctx).Scene()->TeleporterEntite(id, NkVec2f(x, y));
				return 1;
			}
			int32_t TRotation(void *ctx, NkUnkEntite e, float *r) {
				const ecs::NkEntityId id = Vive(ctx, e);
				const NkTransform2D *t = id.IsValid() ? H(ctx).Scene()->Monde().Get<NkTransform2D>(id) : nullptr;
				if (t == nullptr) {
					return 0;
				}
				if (r != nullptr) {
					*r = t->rotation;
				}
				return 1;
			}
			int32_t TPoserRotation(void *ctx, NkUnkEntite e, float r) {
				const ecs::NkEntityId id = Vive(ctx, e);
				if (!id.IsValid() || !Fini(r)) {
					return 0;
				}
				// ⚠️ Un corps rigide est mene par le solveur : sa rotation ne s'ecrit
				//    pas dans le transform (elle serait ecrasee au pas suivant).
				if (H(ctx).Scene()->Monde().Has<NkCorps2D>(id)) {
					return Refus(ctx);
				}
				NkTransform2D *t = H(ctx).Scene()->Monde().Get<NkTransform2D>(id);
				if (t == nullptr) {
					return 0;
				}
				t->rotation = r;
				return 1;
			}
			int32_t TVitesse(void *ctx, NkUnkEntite e, float *vx, float *vy) {
				const ecs::NkEntityId id = Vive(ctx, e);
				if (!id.IsValid()) {
					return 0;
				}
				const NkVec2f v = H(ctx).Scene()->Vitesse(id);
				if (vx != nullptr) {
					*vx = v.x;
				}
				if (vy != nullptr) {
					*vy = v.y;
				}
				return 1;
			}
			int32_t TPoserVitesse(void *ctx, NkUnkEntite e, float vx, float vy) {
				const ecs::NkEntityId id = Vive(ctx, e);
				if (!id.IsValid() || !Fini(vx) || !Fini(vy)) {
					return id.IsValid() ? Refus(ctx) : 0;
				}
				H(ctx).Scene()->PoserVitesse(id, NkVec2f(vx, vy));
				return 1;
			}
			int32_t TImpulsion(void *ctx, NkUnkEntite e, float ix, float iy) {
				const ecs::NkEntityId id = Vive(ctx, e);
				if (!id.IsValid()) {
					return 0;
				}
				return H(ctx).Scene()->AppliquerImpulsion(id, NkVec2f(ix, iy)) ? 1 : Refus(ctx);
			}
			int32_t TForce(void *ctx, NkUnkEntite e, float fx, float fy) {
				const ecs::NkEntityId id = Vive(ctx, e);
				if (!id.IsValid()) {
					return 0;
				}
				return H(ctx).Scene()->AppliquerForce(id, NkVec2f(fx, fy)) ? 1 : Refus(ctx);
			}
			int32_t TCouleur(void *ctx, NkUnkEntite e, uint32_t rgba) {
				const ecs::NkEntityId id = Vive(ctx, e);
				NkSprite2D *s = id.IsValid() ? H(ctx).Scene()->Monde().Get<NkSprite2D>(id) : nullptr;
				if (s == nullptr) {
					return 0;
				}
				s->couleur = rgba;
				return 1;
			}
			int32_t TVisible(void *ctx, NkUnkEntite e, int32_t v) {
				const ecs::NkEntityId id = Vive(ctx, e);
				NkSprite2D *s = id.IsValid() ? H(ctx).Scene()->Monde().Get<NkSprite2D>(id) : nullptr;
				if (s == nullptr) {
					return 0;
				}
				s->visible = v != 0;
				return 1;
			}
			int32_t TJouerClip(void *ctx, NkUnkEntite e, int32_t clip) {
				const ecs::NkEntityId id = Vive(ctx, e);
				NkAnimSprite2D *a = id.IsValid() ? H(ctx).Scene()->Monde().Get<NkAnimSprite2D>(id) : nullptr;
				if (a == nullptr || clip < 0 || clip >= static_cast<int32>(a->nbClips)) {
					return 0;
				}
				a->Jouer(static_cast<uint8>(clip));
				return 1;
			}
			int32_t TAnimParametre(void *ctx, NkUnkEntite e, const char *nom, float v) {
				const ecs::NkEntityId id = Vive(ctx, e);
				NkAnimateur2D *a = id.IsValid() ? H(ctx).Scene()->Monde().Get<NkAnimateur2D>(id) : nullptr;
				return a != nullptr && nom != nullptr && Fini(v) && a->Poser(nom, v) ? 1 : 0;
			}
			int32_t TAnimDeclencher(void *ctx, NkUnkEntite e, const char *nom) {
				const ecs::NkEntityId id = Vive(ctx, e);
				NkAnimateur2D *a = id.IsValid() ? H(ctx).Scene()->Monde().Get<NkAnimateur2D>(id) : nullptr;
				return a != nullptr && nom != nullptr && a->Declencher(nom) ? 1 : 0;
			}
			int32_t TJouerEffet(void *ctx, NkUnkEntite e) {
				const ecs::NkEntityId id = Vive(ctx, e);
				NkEmetteur2D *m = id.IsValid() ? H(ctx).Scene()->Monde().Get<NkEmetteur2D>(id) : nullptr;
				if (m == nullptr) {
					return 0;
				}
				m->actif = true;
				H(ctx).Scene()->Effets().Rejouer(id.Pack()); // l'horloge renait, rafale comprise
				return 1;
			}
			int32_t TArreterEffet(void *ctx, NkUnkEntite e) {
				const ecs::NkEntityId id = Vive(ctx, e);
				NkEmetteur2D *m = id.IsValid() ? H(ctx).Scene()->Monde().Get<NkEmetteur2D>(id) : nullptr;
				if (m == nullptr) {
					return 0;
				}
				m->actif = false; // les particules deja nees finissent leur vie
				return 1;
			}
			int32_t TActionParNom(void *ctx, const char *nom, int32_t *indice) {
				NkHoteScripts2D &h = H(ctx);
				const int32 i = h.Liaisons() != nullptr ? NkIndiceAction(*h.Liaisons(), nom) : NkActionStandardParNom(nom);
				if (indice != nullptr) {
					*indice = i;
				}
				return i >= 0 && i < NK_UNKENY_ACTIONS_MAX ? 1 : 0;
			}
			float TValeurAction(void *ctx, int32_t i) {
				NkHoteScripts2D &h = H(ctx);
				return h.Actions() != nullptr && i >= 0 && i < NK_UNKENY_ACTIONS_MAX ? h.Actions()->Valeur(i) : 0.f;
			}
			int32_t TActionEnfoncee(void *ctx, int32_t i) {
				NkHoteScripts2D &h = H(ctx);
				return h.Actions() != nullptr && i >= 0 && i < NK_UNKENY_ACTIONS_MAX && h.Actions()->Enfoncee(i) ? 1 : 0;
			}
			int32_t TJouerSon(void *ctx, const char *nom, float volume) {
				NkHoteScripts2D &h = H(ctx);
				if (h.Sons() == nullptr || nom == nullptr || !Fini(volume)) {
					return Refus(ctx);
				}
				const uint32 son = h.Sons()->Trouver(nom);
				if (son == 0u) {
					return Refus(ctx);
				}
				h.Sons()->Jouer(son, volume);
				return 1;
			}
			/// La valeur PAR DEFAUT d'une variable declaree par la classe C++ du
			/// script `k` de `id` (la variable n'est pas encore dans le composant).
			bool DefautCpp(NkHoteScripts2D &h, const NkScript2D &s, uint32 k, const char *nom, float *x, float *y) {
				if (h.Registre() == nullptr || k >= s.nombre) {
					return false;
				}
				const NkDefinitionScript *d = h.Registre()->Definition(h.Registre()->Trouver(s.refs[k]));
				if (d == nullptr || d->classe == nullptr) {
					return false;
				}
				for (uint32 i = 0; i < d->classe->nbVariables; ++i) {
					if (std::strcmp(d->classe->variables[i].nom, nom) == 0) {
						*x = d->classe->variables[i].x;
						*y = d->classe->variables[i].y;
						return true;
					}
				}
				return false;
			}
			int32_t TLireVariable(void *ctx, NkUnkEntite soi, uint32_t k, const char *nom, float *x, float *y) {
				const ecs::NkEntityId id = Vive(ctx, soi);
				const NkScript2D *s = id.IsValid() ? H(ctx).Scene()->Monde().Get<NkScript2D>(id) : nullptr;
				if (s == nullptr || nom == nullptr) {
					return 0;
				}
				float vx = 0.f, vy = 0.f;
				if (const NkVarScript *v = NkScriptVariable(*s, k, nom)) {
					vx = v->valeur.x;
					vy = v->valeur.y;
				} else if (!DefautCpp(H(ctx), *s, k, nom, &vx, &vy)) {
					return 0;
				}
				if (x != nullptr) {
					*x = vx;
				}
				if (y != nullptr) {
					*y = vy;
				}
				return 1;
			}
			int32_t TEcrireVariable(void *ctx, NkUnkEntite soi, uint32_t k, const char *nom, float x, float y) {
				const ecs::NkEntityId id = Vive(ctx, soi);
				NkScript2D *s = id.IsValid() ? H(ctx).Scene()->Monde().Get<NkScript2D>(id) : nullptr;
				if (s == nullptr || nom == nullptr || !Fini(x) || !Fini(y)) {
					return 0;
				}
				const NkVarScript *v = NkScriptVariable(*s, k, nom);
				const NkTypeVarScript t = v != nullptr ? static_cast<NkTypeVarScript>(v->type)
													   : (y != 0.f ? NkTypeVarScript::NK_VEC2 : NkTypeVarScript::NK_REEL);
				return NkScriptPoserVariable(*s, k, nom, t, NkVec2f(x, y)) ? 1 : 0;
			}

		} // namespace

		// =====================================================================
		// L'hote
		// =====================================================================
		namespace {
			void SystemePasFixe(NkScene &, float32 dt, void *donnees) {
				static_cast<NkHoteScripts2D *>(donnees)->Passage(NkPhaseSysteme::NK_PAS_FIXE, dt);
			}
			void SystemeTrame(NkScene &, float32 dt, void *donnees) {
				static_cast<NkHoteScripts2D *>(donnees)->Passage(NkPhaseSysteme::NK_TRAME, dt);
			}
		} // namespace

		NkHoteScripts2D::NkHoteScripts2D() {
			std::memset(&mTable, 0, sizeof(mTable));
			mTable.taille = static_cast<uint32_t>(sizeof(NkUnkHoteV1));
			mTable.abiMajeure = NK_UNK_ABI_MAJEURE;
			mTable.abiMineure = NK_UNK_ABI_MINEURE;
			mTable.ctx = this;
			mTable.Afficher = &TAfficher;
			mTable.Temps = &TTemps;
			mTable.Vivante = &TVivante;
			mTable.Detruire = &TDetruire;
			mTable.ParNom = &TParNom;
			mTable.NomEst = &TNomEst;
			mTable.Nom = &TNom;
			mTable.Activer = &TActiver;
			mTable.Position = &TPosition;
			mTable.Teleporter = &TTeleporter;
			mTable.Rotation = &TRotation;
			mTable.PoserRotation = &TPoserRotation;
			mTable.Vitesse = &TVitesse;
			mTable.PoserVitesse = &TPoserVitesse;
			mTable.AppliquerImpulsion = &TImpulsion;
			mTable.AppliquerForce = &TForce;
			mTable.PoserCouleur = &TCouleur;
			mTable.PoserVisible = &TVisible;
			mTable.JouerClip = &TJouerClip;
			mTable.AnimParametre = &TAnimParametre;
			mTable.AnimDeclencher = &TAnimDeclencher;
			mTable.JouerEffet = &TJouerEffet;
			mTable.ArreterEffet = &TArreterEffet;
			mTable.ActionParNom = &TActionParNom;
			mTable.ValeurAction = &TValeurAction;
			mTable.ActionEnfoncee = &TActionEnfoncee;
			mTable.JouerSon = &TJouerSon;
			mTable.LireVariable = &TLireVariable;
			mTable.EcrireVariable = &TEcrireVariable;
		}

		NkHoteScripts2D::~NkHoteScripts2D() {
			Arreter();
			Debrancher();
		}

		bool NkHoteScripts2D::Brancher(NkScene &scene, NkScripts2D &scripts) {
			if (mScene == &scene) {
				mScripts = &scripts;
				return true;
			}
			if (mScene != nullptr) {
				return false;
			}
			mScene = &scene;
			mScripts = &scripts;
			mSysPasFixe = scene.AjouterSysteme("scripts.pas_fixe", NkPhaseSysteme::NK_PAS_FIXE, &SystemePasFixe, this, -100);
			mSysTrame = scene.AjouterSysteme("scripts.trame", NkPhaseSysteme::NK_TRAME, &SystemeTrame, this, -100);
			return true;
		}

		void NkHoteScripts2D::Debrancher() {
			if (mScene != nullptr) {
				mScene->RetirerSysteme(mSysPasFixe);
				mScene->RetirerSysteme(mSysTrame);
			}
			mScene = nullptr;
			mSysPasFixe = mSysTrame = 0u;
		}

		void NkHoteScripts2D::Ecrire(const char *ligne, bool faute) {
			NkLigneScript l;
			l.texte = ligne;
			l.faute = faute;
			// Borne : un script qui affiche a chaque image ne doit pas manger la memoire.
			if (mJournal.Size() >= 512u) {
				mJournal.Erase(mJournal.Begin());
			}
			mJournal.PushBack(l);
			if (mSortie != nullptr) {
				mSortie(mSortieDonnees, ligne, faute);
			}
		}

		void NkHoteScripts2D::Liberer(NkInstanceScript &inst) {
			if (inst.cpp != nullptr && inst.classe != nullptr && inst.classe->Detruire != nullptr) {
				inst.classe->Detruire(inst.cpp);
			}
			inst = NkInstanceScript();
		}

		void NkHoteScripts2D::Arreter() {
			for (uint32 i = 0; i < mFiches.Size(); ++i) {
				for (uint32 k = 0; k < NK_UNKENY_SCRIPTS_MAX; ++k) {
					Liberer(mFiches[i].inst[k]);
				}
			}
			mFiches.Clear();
			mIndex.Clear();
			mFile.Clear();
			mTemps = 0.f;
			mFautes = 0u;
			mRefus = 0u;
		}

		uint32 NkHoteScripts2D::NbInstances() const noexcept {
			uint32 n = 0;
			for (uint32 i = 0; i < mFiches.Size(); ++i) {
				for (uint32 k = 0; k < NK_UNKENY_SCRIPTS_MAX; ++k) {
					n += mFiches[i].inst[k].script != 0u ? 1u : 0u;
				}
			}
			return n;
		}

		void *NkHoteScripts2D::InstanceCpp(ecs::NkEntityId id, uint32 k) const noexcept {
			const uint32 *i = mIndex.Find(id.Pack());
			return i != nullptr && k < NK_UNKENY_SCRIPTS_MAX ? mFiches[*i].inst[k].cpp : nullptr;
		}

		bool NkHoteScripts2D::EnFaute(ecs::NkEntityId id, uint32 k) const noexcept {
			const uint32 *i = mIndex.Find(id.Pack());
			return i != nullptr && k < NK_UNKENY_SCRIPTS_MAX && mFiches[*i].inst[k].faute;
		}

		NkHoteScripts2D::NkEntiteScripts *NkHoteScripts2D::Fiche(uint64 pack, bool creer) {
			if (const uint32 *i = mIndex.Find(pack)) {
				return &mFiches[*i];
			}
			if (!creer) {
				return nullptr;
			}
			NkEntiteScripts f;
			f.pack = pack;
			mFiches.PushBack(f);
			mIndex.Insert(pack, static_cast<uint32>(mFiches.Size() - 1u));
			return &mFiches.Back();
		}

		void NkHoteScripts2D::Purger() {
			ecs::NkWorld &w = mScene->Monde();
			uint32 i = 0;
			while (i < mFiches.Size()) {
				const ecs::NkEntityId id = ecs::NkEntityId::Unpack(mFiches[i].pack);
				if (w.IsAlive(id) && w.Has<NkScript2D>(id)) {
					++i;
					continue;
				}
				for (uint32 k = 0; k < NK_UNKENY_SCRIPTS_MAX; ++k) {
					Liberer(mFiches[i].inst[k]);
				}
				mIndex.Erase(mFiches[i].pack);
				const uint32 dernier = static_cast<uint32>(mFiches.Size() - 1u);
				if (i != dernier) {
					mFiches[i] = mFiches[dernier];
					mIndex.Insert(mFiches[i].pack, i);
				}
				mFiches.PopBack();
			}
		}

		const char *NkHoteScripts2D::NomAction(int32 i) const noexcept {
			const char *n = mLiaisons != nullptr ? mLiaisons->NomAction(i) : nullptr;
			if (n == nullptr || n[0] == '\0') {
				n = NkNomActionStandard(i);
			}
			return n != nullptr ? n : "";
		}

		void NkHoteScripts2D::Faute(ecs::NkEntityId id, NkInstanceScript &inst, const char *script, const char *raison) {
			inst.faute = true;
			++mFautes;
			const NkEtiquette *e = mScene != nullptr ? mScene->Monde().Get<NkEtiquette>(id) : nullptr;
			char ligne[512];
			std::snprintf(ligne, sizeof(ligne), "[script] %s sur « %s » : %s -- instance desactivee", script,
						  e != nullptr ? e->nom : "?", raison);
			Ecrire(ligne, true);
		}

		void NkHoteScripts2D::LierInstance(ecs::NkEntityId id, NkScript2D &s, uint32 k, NkInstanceScript &inst) {
			// Le nom a change (les Details ont remplace le script) : nouvelle instance.
			if (inst.script != 0u && std::strcmp(inst.ref, s.refs[k]) != 0) {
				Liberer(inst);
			}
			if (inst.script == 0u) {
				if (inst.generation == mScripts->Generation() && std::strcmp(inst.ref, s.refs[k]) == 0) {
					return; // deja cherche, toujours inconnu : rien de neuf au registre
				}
				std::memcpy(inst.ref, s.refs[k], NK_UNKENY_SCRIPT_NOM_MAX);
				inst.generation = mScripts->Generation();
				const uint32 sid = mScripts->Trouver(s.refs[k]);
				const NkDefinitionScript *d = mScripts->Definition(sid);
				if (d == nullptr) {
					if (!inst.signale) {
						inst.signale = true;
						char ligne[256];
						std::snprintf(ligne, sizeof(ligne), "[script] « %s » inconnu de ce jeu (ni Blueprint charge, ni classe C++)",
									  s.refs[k]);
						Ecrire(ligne, true);
					}
					return;
				}
				inst.script = sid;
				inst.version = d->version;
				inst.signale = false;
				Migrer(id, k, inst, *d);
				return;
			}
			const NkDefinitionScript *d = mScripts->Definition(inst.script);
			if (d != nullptr && d->version != inst.version) {
				Migrer(id, k, inst, *d);
			}
		}

		void NkHoteScripts2D::Migrer(ecs::NkEntityId id, uint32 k, NkInstanceScript &inst, const NkDefinitionScript &def) {
			// L'etat PRIVE, par l'ANCIEN code (Sauver), puis l'ancienne instance
			// detruite par son propre code.
			uint8 tampon[4096];
			uint32 n = 0u;
			const bool recharge = inst.debut && inst.version != def.version;
			if (inst.cpp != nullptr && inst.classe != nullptr) {
				if (inst.classe->Sauver != nullptr) {
					n = inst.classe->Sauver(inst.cpp, tampon, sizeof(tampon));
				}
				if (inst.classe->Detruire != nullptr) {
					inst.classe->Detruire(inst.cpp);
				}
			}
			inst.cpp = nullptr;
			inst.classe = nullptr;
			inst.version = def.version;
			inst.faute = false;
			inst.actions.Clear();
			NkScript2D *s = mScene->Monde().Get<NkScript2D>(id);
			if (!def.erreur.Empty() || (def.genre == NkGenreScript::NK_CPP && def.classe == nullptr) ||
				(def.genre == NkGenreScript::NK_BLUEPRINT && (def.programme == nullptr || !def.programme->pret))) {
				if (!inst.signale) {
					inst.signale = true;
					char ligne[512];
					std::snprintf(ligne, sizeof(ligne), "[script] %s ne peut pas tourner : %s", def.nom.CStr(),
								  def.erreur.Empty() ? "definition vide" : def.erreur.CStr());
					Ecrire(ligne, true);
				}
				return;
			}
			inst.signale = false;
			if (def.genre == NkGenreScript::NK_CPP) {
				inst.classe = def.classe;
				inst.cpp = def.classe->Creer != nullptr ? def.classe->Creer() : nullptr;
				if (inst.cpp == nullptr) {
					Faute(id, inst, def.nom.CStr(), "la classe n'a pas pu etre creee");
					return;
				}
				if (n > 0u && def.classe->Relire != nullptr) {
					def.classe->Relire(inst.cpp, tampon, n);
				}
				// Les variables EXPOSEES absentes du composant prennent leur defaut :
				// les Details les montrent, le fichier les garde.
				for (uint32 v = 0; s != nullptr && v < def.classe->nbVariables; ++v) {
					const NkUnkVariableV1 &x = def.classe->variables[v];
					if (NkScriptVariable(*s, k, x.nom) == nullptr) {
						NkScriptPoserVariable(*s, k, x.nom, static_cast<NkTypeVarScript>(x.type), NkVec2f(x.x, x.y));
					}
				}
			} else {
				const NkModuleBp &m = def.programme->module;
				for (uint32 v = 0; s != nullptr && v < m.variables.Size(); ++v) {
					const NkVariableBp &x = m.variables[v];
					if (NkScriptVariable(*s, k, x.nom.CStr()) == nullptr) {
						NkVec2f val(x.defaut.x, x.defaut.y);
						NkTypeVarScript t = NkTypeVarScript::NK_REEL;
						if (x.type == NkTypeBp::NK_ENTIER) {
							t = NkTypeVarScript::NK_ENTIER;
							val = NkVec2f(static_cast<float32>(x.defaut.i), 0.f);
						} else if (x.type == NkTypeBp::NK_BOOLEEN) {
							t = NkTypeVarScript::NK_BOOLEEN;
							val = NkVec2f(x.defaut.i != 0 ? 1.f : 0.f, 0.f);
						} else if (x.type == NkTypeBp::NK_VEC2) {
							t = NkTypeVarScript::NK_VEC2;
						} else if (x.type == NkTypeBp::NK_COULEUR) {
							t = NkTypeVarScript::NK_COULEUR;
							val = NkCouleurVersVar(static_cast<uint32>(x.defaut.i));
						} else if (x.type == NkTypeBp::NK_TEXTE || x.type == NkTypeBp::NK_ENTITE) {
							// Le texte par defaut est une constante du module ; une
							// entite part de « aucune ».
							const int32 c = x.defaut.i;
							const char *texte = x.type == NkTypeBp::NK_TEXTE && c >= 0 && static_cast<uint32>(c) < m.constantes.Size()
													? m.constantes[static_cast<uint32>(c)].texte.CStr()
													: "";
							NkScriptPoserTexte(*s, k, x.nom.CStr(),
											   x.type == NkTypeBp::NK_TEXTE ? NkTypeVarScript::NK_TEXTE : NkTypeVarScript::NK_ENTITE, texte);
							continue;
						}
						NkScriptPoserVariable(*s, k, x.nom.CStr(), t, val);
					}
				}
				for (uint32 e = 0; e < m.entrees.Size(); ++e) {
					int32 a = -1;
					if (!m.entrees[e].parametre.Empty()) {
						a = mLiaisons != nullptr ? NkIndiceAction(*mLiaisons, m.entrees[e].parametre.CStr())
												 : NkActionStandardParNom(m.entrees[e].parametre.CStr());
					}
					inst.actions.PushBack(a);
				}
			}
			if (recharge) {
				NkUnkEvenementV1 ev;
				std::memset(&ev, 0, sizeof(ev));
				ev.genre = NK_UNK_EV_RECHARGE;
				ev.emplacement = k;
				ev.soi.pack = id.Pack();
				ev.nomAction = "";
				Envoyer(id, k, inst, ev);
			}
		}

		uint32 NkHoteScripts2D::MigrerInstances() {
			if (mScene == nullptr || mScripts == nullptr) {
				return 0u;
			}
			uint32 n = 0;
			for (uint32 i = 0; i < mFiches.Size(); ++i) {
				const ecs::NkEntityId id = ecs::NkEntityId::Unpack(mFiches[i].pack);
				if (!mScene->Monde().IsAlive(id)) {
					continue;
				}
				for (uint32 k = 0; k < NK_UNKENY_SCRIPTS_MAX; ++k) {
					NkInstanceScript &inst = mFiches[i].inst[k];
					const NkDefinitionScript *d = mScripts->Definition(inst.script);
					if (inst.script != 0u && d != nullptr && d->version != inst.version) {
						Migrer(id, k, inst, *d);
						++n;
					}
				}
			}
			return n;
		}

		void NkHoteScripts2D::Lier(ecs::NkEntityId id, NkEntiteScripts &f) {
			NkScript2D *s = mScene->Monde().Get<NkScript2D>(id);
			if (s == nullptr) {
				return;
			}
			const uint32 n = s->nombre < NK_UNKENY_SCRIPTS_MAX ? s->nombre : NK_UNKENY_SCRIPTS_MAX;
			for (uint32 k = 0; k < NK_UNKENY_SCRIPTS_MAX; ++k) {
				if (k >= n || s->refs[k][0] == '\0') {
					if (f.inst[k].script != 0u || f.inst[k].ref[0] != '\0') {
						Liberer(f.inst[k]);
					}
					continue;
				}
				LierInstance(id, *s, k, f.inst[k]);
				// Migrer a pu toucher au composant (variables) : on le relit.
				s = mScene->Monde().Get<NkScript2D>(id);
				if (s == nullptr) {
					return;
				}
			}
		}

		void NkHoteScripts2D::ExecuterBp(ecs::NkEntityId id, uint32 k, NkInstanceScript &inst, const NkDefinitionScript &def,
										 const NkUnkEvenementV1 &ev) {
			const NkProgrammeBp &p = *def.programme;
			const NkModuleBp &m = p.module;
			for (uint32 e = 0; e < m.entrees.Size() && !inst.faute; ++e) {
				const NkEntreeBp &entree = m.entrees[e];
				if (entree.genre != ev.genre) {
					continue;
				}
				if ((ev.genre == NK_UNK_EV_ACTION_PRESSEE || ev.genre == NK_UNK_EV_ACTION_RELACHEE) &&
					(e >= inst.actions.Size() || inst.actions[e] != ev.action)) {
					continue;
				}
				// Un REPARTITEUR : son nom, et ses parametres (nombre ET types).
				const NkDiffusionBp *diff = nullptr;
				if (ev.genre == NK_UNK_EV_PERSONNALISE) {
					if (mDiffusion == nullptr || ev.nomAction == nullptr || std::strcmp(entree.parametre.CStr(), ev.nomAction) != 0) {
						continue;
					}
					const NkFonctionBp &fn = m.fonctions[entree.fonction];
					bool egal = fn.params.Size() == mDiffusion->n;
					for (uint32 i = 0; egal && i < mDiffusion->n; ++i) {
						egal = fn.params[i] == mDiffusion->types[i];
					}
					if (!egal) {
						char ligne[256];
						std::snprintf(ligne, sizeof(ligne), "[script] %s : le repartiteur « %s » arrive avec d'autres parametres -- ignore",
									  def.nom.CStr(), ev.nomAction);
						Ecrire(ligne, true);
						continue;
					}
					diff = mDiffusion;
				}
				// Les variables : du composant vers le cadre (par NOM), puis retour.
				// ⚠️ Pas de pointeur garde sur le composant pendant l'appel : un natif
				//    (Activer) peut changer l'archetype de l'entite et le deplacer.
				const uint32 nv = static_cast<uint32>(m.variables.Size());
				if (mVariables.Size() < nv) {
					mVariables.Resize(nv);
				}
				mTextes.Clear();
				if (const NkScript2D *s = mScene->Monde().Get<NkScript2D>(id)) {
					for (uint32 v = 0; v < nv; ++v) {
						const NkVariableBp &dv = m.variables[v];
						const NkVarScript *x = NkScriptVariable(*s, k, dv.nom.CStr());
						NkValeurBp val = dv.defaut;
						if (x != nullptr) {
							val = NkValeurBp();
							switch (dv.type) {
								case NkTypeBp::NK_ENTIER:
								case NkTypeBp::NK_BOOLEEN:
									val.i = static_cast<int32>(x->valeur.x);
									val.x = x->valeur.x;
									break;
								case NkTypeBp::NK_COULEUR:
									val.i = static_cast<int32>(NkCouleurDeVar(x->valeur));
									break;
								case NkTypeBp::NK_TEXTE:
									mTextes.PushBack(NkString(x->texte));
									val.i = -static_cast<int32>(mTextes.Size());
									break;
								case NkTypeBp::NK_ENTITE: {
									NkUnkEntite t{0u};
									if (x->texte[0] != '\0' && mTable.ParNom != nullptr) {
										// Un nom qui ne designe plus rien : « aucune » (pas un refus).
										const uint32 refus = mRefus;
										mTable.ParNom(this, x->texte, &t);
										mRefus = refus;
									}
									val.e = t.pack;
									break;
								}
								default:
									val.x = x->valeur.x;
									val.y = x->valeur.y;
									val.i = static_cast<int32>(x->valeur.x);
									break;
							}
						}
						mVariables[v] = val;
					}
				}
				NkValeurBp arguments[8];
				if (diff != nullptr) {
					for (uint32 i = 0; i < diff->n; ++i) {
						arguments[i] = diff->args[i];
						if (diff->types[i] == NkTypeBp::NK_TEXTE) {
							// Un texte venu d'un AUTRE module : fabrique ici.
							mTextes.PushBack(diff->textes[i]);
							arguments[i].i = -static_cast<int32>(mTextes.Size());
						}
					}
				}
				NkCadreBp c;
				c.hote = &mTable;
				c.soi.pack = id.Pack();
				c.ev = &ev;
				c.variables = mVariables.Data();
				c.registres = &mRegistres;
				c.textes = &mTextes;
				c.diffuser = &NkHoteScripts2D::DiffuserBp;
				c.diffuserDonnees = this;
				if (diff != nullptr) {
					c.arguments = arguments;
					c.nbArguments = diff->n;
				}
				if (mTraceActive) {
					mVus.Clear();
					c.trace = &NkHoteScripts2D::TracerBp;
					c.traceDonnees = this;
				}
				const NkModuleBp *avant = mModuleCourant;
				mModuleCourant = &m;
				const uint64 soiAvant = mSoiCourant;
				mSoiCourant = id.Pack();
				NkFauteBp faute;
				const bool ok = NkExecuterBp(p, entree.fonction, c, faute);
				mModuleCourant = avant;
				mSoiCourant = soiAvant;
				mRefus += c.refus;
				if (mScene->Monde().IsAlive(id)) {
					if (NkScript2D *s = mScene->Monde().Get<NkScript2D>(id)) {
						for (uint32 v = 0; v < nv; ++v) {
							const NkVariableBp &x = m.variables[v];
							const NkValeurBp &val = mVariables[v];
							switch (x.type) {
								case NkTypeBp::NK_ENTIER:
									NkScriptPoserVariable(*s, k, x.nom.CStr(), NkTypeVarScript::NK_ENTIER, NkVec2f(static_cast<float32>(val.i), 0.f));
									break;
								case NkTypeBp::NK_BOOLEEN:
									NkScriptPoserVariable(*s, k, x.nom.CStr(), NkTypeVarScript::NK_BOOLEEN, NkVec2f(val.i != 0 ? 1.f : 0.f, 0.f));
									break;
								case NkTypeBp::NK_VEC2:
									NkScriptPoserVariable(*s, k, x.nom.CStr(), NkTypeVarScript::NK_VEC2, NkVec2f(val.x, val.y));
									break;
								case NkTypeBp::NK_COULEUR:
									NkScriptPoserVariable(*s, k, x.nom.CStr(), NkTypeVarScript::NK_COULEUR,
														  NkCouleurVersVar(static_cast<uint32>(val.i)));
									break;
								case NkTypeBp::NK_TEXTE: {
									const char *t = "";
									if (val.i >= 0 && static_cast<uint32>(val.i) < m.constantes.Size()) {
										t = m.constantes[static_cast<uint32>(val.i)].texte.CStr();
									} else if (val.i < 0) {
										const int64 d = -static_cast<int64>(val.i) - 1;
										t = d < static_cast<int64>(mTextes.Size()) ? mTextes[static_cast<uint32>(d)].CStr() : "";
									}
									NkScriptPoserTexte(*s, k, x.nom.CStr(), NkTypeVarScript::NK_TEXTE, t);
									break;
								}
								case NkTypeBp::NK_ENTITE: {
									char nom[NK_UNKENY_VAR_TEXTE_MAX] = {};
									const ecs::NkEntityId e2 = val.e != 0u ? ecs::NkEntityId::Unpack(val.e) : ecs::NkEntityId::Invalid();
									if (e2.IsValid() && mScene->Monde().IsAlive(e2)) {
										if (const NkEtiquette *et = mScene->Monde().Get<NkEtiquette>(e2)) {
											std::snprintf(nom, sizeof(nom), "%s", et->nom);
										}
									}
									s = mScene->Monde().Get<NkScript2D>(id);
									if (s != nullptr) {
										NkScriptPoserTexte(*s, k, x.nom.CStr(), NkTypeVarScript::NK_ENTITE, nom);
									}
									break;
								}
								default:
									NkScriptPoserVariable(*s, k, x.nom.CStr(), NkTypeVarScript::NK_REEL, NkVec2f(val.x, 0.f));
									break;
							}
							if (s == nullptr) {
								break;
							}
						}
					}
				}
				if (mTraceActive) {
					// Les noeuds VUS pendant cet evenement : chacun compte une fois.
					NkTraceBp *t = nullptr;
					for (uint32 i = 0; i < mTraces.Size() && t == nullptr; ++i) {
						if (mTraces[i].script == def.nom) {
							t = &mTraces[i];
						}
					}
					if (t == nullptr) {
						NkTraceBp neuve;
						neuve.script = def.nom;
						mTraces.PushBack(neuve);
						t = &mTraces.Back();
					}
					++t->evenements;
					for (uint32 i = 0; i < mVus.Size(); ++i) {
						bool trouve = false;
						for (uint32 q = 0; q < t->noeuds.Size() && !trouve; ++q) {
							if (t->noeuds[q] == mVus[i]) {
								++t->comptes[q];
								trouve = true;
							}
						}
						if (!trouve) {
							t->noeuds.PushBack(mVus[i]);
							t->comptes.PushBack(1u);
						}
					}
					NkPassageBp pa;
					pa.temps = mTemps;
					pa.genre = ev.genre;
					pa.parametre = entree.parametre;
					if (const NkEtiquette *et = mScene->Monde().IsAlive(id) ? mScene->Monde().Get<NkEtiquette>(id) : nullptr) {
						pa.entite = et->nom;
					}
					pa.fonction = entree.fonction;
					pa.premier = mVus.Empty() ? 0u : mVus[0];
					pa.dernier = mVus.Empty() ? 0u : mVus.Back();
					pa.nbNoeuds = static_cast<uint32>(mVus.Size());
					pa.faute = !ok;
					if (t->passages.Size() >= 64u) {
						t->passages.Erase(t->passages.Begin());
					}
					t->passages.PushBack(pa);
				}
				if (!ok) {
					char raison[384];
					std::snprintf(raison, sizeof(raison), "%s (evenement %u, fonction %u, pc %u, noeud %u)", faute.raison.CStr(),
								  static_cast<unsigned>(ev.genre), static_cast<unsigned>(faute.fonction),
								  static_cast<unsigned>(faute.pc), static_cast<unsigned>(faute.noeud));
					Faute(id, inst, def.nom.CStr(), raison);
				}
			}
		}

		void NkHoteScripts2D::TracerBp(void *donnees, uint32 fonction, uint32 noeud) {
			(void)fonction;
			NkHoteScripts2D &h = *static_cast<NkHoteScripts2D *>(donnees);
			for (uint32 i = 0; i < h.mVus.Size(); ++i) {
				if (h.mVus[i] == noeud) {
					// Deja vu : il redevient le DERNIER (l'ordre du passage).
					h.mVus.Erase(h.mVus.Begin() + i);
					break;
				}
			}
			h.mVus.PushBack(noeud);
		}

		const NkTraceBp *NkHoteScripts2D::Trace(const char *nom) const noexcept {
			for (uint32 i = 0; nom != nullptr && i < mTraces.Size(); ++i) {
				if (std::strcmp(mTraces[i].script.CStr(), nom) == 0) {
					return &mTraces[i];
				}
			}
			return nullptr;
		}

		bool NkHoteScripts2D::DiffuserBp(void *donnees, NkUnkEntite cible, const char *nom, const NkValeurBp *args, uint32 n,
										 const NkTypeBp *types) {
			NkHoteScripts2D &h = *static_cast<NkHoteScripts2D *>(donnees);
			// La source : l'entite dont le script s'execute (son cadre la connait ;
			// la file la garde pour « autre »).
			NkUnkEntite source{h.mSoiCourant};
			return h.Diffuser(cible, source, nom, args, n, types, h.mModuleCourant);
		}

		bool NkHoteScripts2D::Diffuser(NkUnkEntite cible, NkUnkEntite source, const char *nom, const NkValeurBp *args, uint32 n,
									   const NkTypeBp *types, const NkModuleBp *module) {
			if (mScene == nullptr || nom == nullptr || n > 8u || cible.pack == 0u) {
				return false;
			}
			const ecs::NkEntityId id = ecs::NkEntityId::Unpack(cible.pack);
			if (!mScene->Monde().IsAlive(id)) {
				return false;
			}
			// Une file BORNEE : deux repartiteurs qui s'appellent l'un l'autre ne
			// mangent pas la memoire.
			if (mFile.Size() >= 256u) {
				Ecrire("[script] trop de repartiteurs en attente (256) : appel ignore", true);
				return false;
			}
			NkDiffusionBp d;
			d.cible = cible.pack;
			d.source = source.pack;
			d.nom = nom;
			d.n = n;
			for (uint32 i = 0; i < n; ++i) {
				d.args[i] = args[i];
				d.types[i] = types != nullptr ? types[i] : NkTypeBp::NK_REEL;
				if (d.types[i] == NkTypeBp::NK_TEXTE) {
					// Le texte, RESOLU ici : son indice ne vaut que dans SON module.
					const int32 k = args[i].i;
					if (k >= 0 && module != nullptr && static_cast<uint32>(k) < module->constantes.Size()) {
						d.textes[i] = module->constantes[static_cast<uint32>(k)].texte;
					} else if (k < 0) {
						const int64 q = -static_cast<int64>(k) - 1;
						d.textes[i] = q < static_cast<int64>(mTextes.Size()) ? mTextes[static_cast<uint32>(q)] : NkString();
					}
				}
			}
			mFile.PushBack(d);
			return true;
		}

		void NkHoteScripts2D::LivrerDiffusions() {
			if (mScene == nullptr) {
				mFile.Clear();
				return;
			}
			uint32 livrees = 0u;
			while (!mFile.Empty()) {
				if (++livrees > 256u) {
					Ecrire("[script] repartiteurs qui s'appellent sans fin : file videe", true);
					mFile.Clear();
					break;
				}
				const NkDiffusionBp d = mFile[0];
				mFile.Erase(mFile.Begin());
				const ecs::NkEntityId id = ecs::NkEntityId::Unpack(d.cible);
				if (!mScene->Monde().IsAlive(id) || !mScene->Monde().Has<NkScript2D>(id)) {
					continue;
				}
				NkUnkEvenementV1 ev;
				std::memset(&ev, 0, sizeof(ev));
				ev.genre = NK_UNK_EV_PERSONNALISE;
				ev.nomAction = d.nom.CStr();
				ev.autre.pack = d.source;
				for (uint32 i = 0; i < d.n; ++i) {
					if (d.types[i] == NkTypeBp::NK_REEL) {
						ev.valeur = d.args[i].x;
						break;
					}
				}
				const NkDiffusionBp *avant = mDiffusion;
				mDiffusion = &d;
				Livrer(id, ev);
				mDiffusion = avant;
			}
		}
		void NkHoteScripts2D::Envoyer(ecs::NkEntityId id, uint32 k, NkInstanceScript &inst, const NkUnkEvenementV1 &ev) {
			if (inst.script == 0u || inst.faute || !mScene->Monde().IsAlive(id)) {
				return;
			}
			const NkDefinitionScript *d = mScripts->Definition(inst.script);
			if (d == nullptr || d->version != inst.version) {
				return; // en attente de migration
			}
			if (d->genre == NkGenreScript::NK_CPP) {
				if (inst.cpp == nullptr || inst.classe == nullptr) {
					return;
				}
				if (inst.classe->Evenement(inst.cpp, &mTable, &ev) == 0) {
					Faute(id, inst, d->nom.CStr(), "exception levee par le script (rattrapee a la frontiere)");
				}
				return;
			}
			if (d->programme != nullptr && d->programme->pret) {
				ExecuterBp(id, k, inst, *d, ev);
			}
		}

		void NkHoteScripts2D::Livrer(ecs::NkEntityId id, NkUnkEvenementV1 ev) {
			ev.soi.pack = id.Pack();
			for (uint32 k = 0; k < NK_UNKENY_SCRIPTS_MAX; ++k) {
				if (!mScene->Monde().IsAlive(id) || !mScene->EstActive(id)) {
					return;
				}
				const NkScript2D *s = mScene->Monde().Get<NkScript2D>(id);
				if (s == nullptr || k >= s->nombre) {
					return;
				}
				if (!s->actifs[k]) {
					continue;
				}
				NkEntiteScripts *f = Fiche(id.Pack(), false);
				if (f == nullptr) {
					return;
				}
				ev.emplacement = k;
				// Copie : Envoyer peut faire grandir mFiches (un script qui cree).
				NkInstanceScript &inst = f->inst[k];
				if (inst.script == 0u || !inst.debut) {
					continue; // pas encore demarre : son Debut viendra d'abord
				}
				Envoyer(id, k, inst, ev);
			}
		}

		void NkHoteScripts2D::Passage(NkPhaseSysteme phase, float32 dt) {
			if (mScene == nullptr || mScripts == nullptr) {
				return;
			}
			ecs::NkWorld &w = mScene->Monde();
			// 1. LE RELEVE, puis plus aucune requete pendant les appels.
			mListe.Clear();
			w.Query<NkScript2D>().ForEach([this](ecs::NkEntityId id, NkScript2D &) {
				mListe.PushBack(id);
			});
			Purger();
			NkUnkEvenementV1 ev;
			std::memset(&ev, 0, sizeof(ev));
			ev.nomAction = "";

			// 2. DEBUT des nouveaux, avant tout autre evenement, quelle que soit la phase.
			for (uint32 i = 0; i < mListe.Size(); ++i) {
				const ecs::NkEntityId id = mListe[i];
				if (!w.IsAlive(id) || !mScene->EstActive(id)) {
					continue;
				}
				NkEntiteScripts *f = Fiche(id.Pack(), true);
				Lier(id, *f);
				for (uint32 k = 0; k < NK_UNKENY_SCRIPTS_MAX; ++k) {
					f = Fiche(id.Pack(), false); // un Debut peut creer des entites
					if (f == nullptr || !w.IsAlive(id)) {
						break;
					}
					const NkScript2D *s = w.Get<NkScript2D>(id);
					if (s == nullptr || k >= s->nombre) {
						break;
					}
					NkInstanceScript &inst = f->inst[k];
					if (inst.script == 0u || inst.debut || !s->actifs[k]) {
						continue;
					}
					inst.debut = true;
					NkUnkEvenementV1 d = ev;
					d.genre = NK_UNK_EV_DEBUT;
					d.emplacement = k;
					d.soi.pack = id.Pack();
					Envoyer(id, k, inst, d);
				}
			}

			if (phase == NkPhaseSysteme::NK_PAS_FIXE) {
				ev.genre = NK_UNK_EV_PAS_FIXE;
				ev.dt = dt;
				for (uint32 i = 0; i < mListe.Size(); ++i) {
					Livrer(mListe[i], ev);
				}
				LivrerDiffusions(); // les repartiteurs appeles pendant ce pas
				return;
			}

			mTemps += dt;
			// 3. CONTACTS et ZONES du dernier Pas, recus par les DEUX entites.
			const NkVector<NkContact2D> &contacts = mScene->Contacts();
			for (uint32 i = 0; i < contacts.Size(); ++i) {
				const NkContact2D c = contacts[i];
				NkUnkEvenementV1 e = ev;
				const bool debut = c.phase == NkPhaseContact::NK_DEBUT;
				e.genre = c.declencheur ? (debut ? NK_UNK_EV_ZONE_ENTREE : NK_UNK_EV_ZONE_SORTIE)
										: (debut ? NK_UNK_EV_CONTACT_DEBUT : NK_UNK_EV_CONTACT_FIN);
				if (w.IsAlive(c.a) && w.Has<NkScript2D>(c.a)) {
					e.autre.pack = c.b.Pack();
					e.soiEstLaZone = c.declencheur ? 1 : 0;
					Livrer(c.a, e);
				}
				if (w.IsAlive(c.b) && w.Has<NkScript2D>(c.b)) {
					e.autre.pack = c.a.Pack();
					e.soiEstLaZone = 0;
					Livrer(c.b, e);
				}
			}
			// 4. ACTIONS : pressee / relachee a CETTE trame (jamais une touche).
			if (mActions != nullptr) {
				for (int32 a = 0; a < NK_UNKENY_ACTIONS_MAX; ++a) {
					const bool presse = mActions->VientDEtrePressee(a);
					const bool relache = mActions->VientDEtreRelachee(a);
					if (!presse && !relache) {
						continue;
					}
					NkUnkEvenementV1 e = ev;
					e.genre = presse ? NK_UNK_EV_ACTION_PRESSEE : NK_UNK_EV_ACTION_RELACHEE;
					e.action = a;
					e.valeur = mActions->Valeur(a);
					e.nomAction = NomAction(a);
					for (uint32 i = 0; i < mListe.Size(); ++i) {
						Livrer(mListe[i], e);
					}
				}
			}
			// 5. TICK.
			ev.genre = NK_UNK_EV_TICK;
			ev.dt = dt;
			for (uint32 i = 0; i < mListe.Size(); ++i) {
				Livrer(mListe[i], ev);
			}
			// 6. Les REPARTITEURS appeles pendant la trame (une file : jamais une
			//    reentree dans la machine).
			LivrerDiffusions();
		}

	} // namespace unkeny
} // namespace nkentseu
