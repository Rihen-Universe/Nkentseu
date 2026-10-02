// -----------------------------------------------------------------------------
// @File    NKScenaModele.cpp
// @Brief   Le modele de NKScena (voir NKScenaModele.h).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NKScena/NKScenaModele.h"
#include "NKScena/NKScenaPont.h"

#include "NKFileSystem/NkDirectory.h"
#include "Noge/ECS/Components/Core/NkTag.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/Rendering/NkRenderComponents.h"
#include "Noge/Sequencer/NkSequencer.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace nkscena {

		using editorkit::NkTimelineTrack;
		using editorkit::NkTimelineValueKind;

		namespace {
			/// La priorite que prend la camera du plan quand la vue regarde par elle :
			/// au-dessus de la camera d'edition (1000) et de toute camera de scene.
			constexpr int32 kPrioritePlan = 100000;

			void PreparerDossier(const char *chemin) {
				char dossier[512];
				std::snprintf(dossier, sizeof(dossier), "%s", chemin != nullptr ? chemin : "");
				char *fin = nullptr;
				for (char *c = dossier; *c != '\0'; ++c) {
					if (*c == '/' || *c == '\\') {
						fin = c;
					}
				}
				if (fin != nullptr) {
					*fin = '\0';
					if (!NkDirectory::Exists(dossier)) {
						(void)NkDirectory::CreateRecursive(dossier);
					}
				}
			}

			int32 IndicePropriete(const NkString &p) {
				if (p == NkString(kNkScenaPosition)) {
					return 0;
				}
				if (p == NkString(kNkScenaRotation)) {
					return 1;
				}
				if (p == NkString(kNkScenaEchelle)) {
					return 2;
				}
				return -1;
			}
		} // namespace

		// =====================================================================
		NkScenaModele::NkScenaModele() : mSeq(new NkSequence()), mReste(new NkSequence()) {
			frise.fps = 24.f;
			frise.duration = 5.f;
			frise.rootLabel = nom;
			frise.FrameAll();
		}

		NkScenaModele::~NkScenaModele() {
			delete mSeq;
			delete mReste;
		}

		void NkScenaModele::Brancher(ecs::NkWorld *monde, NkPhysicsSystem *physique) noexcept {
			scene.Brancher(monde, physique);
		}

		ecs::NkEntityId NkScenaModele::Entite(const char *nomEntite) {
			ecs::NkEntityId r = ecs::NkEntityId::Invalid();
			if (!Pret() || nomEntite == nullptr || nomEntite[0] == '\0') {
				return r;
			}
			scene.Monde().Query<const ecs::NkName>().ForEach([&](ecs::NkEntityId id, const ecs::NkName &n) {
				if (!r.IsValid() && std::strcmp(n.value, nomEntite) == 0) {
					r = id;
				}
			});
			return r;
		}

		// =====================================================================
		// LA SCENE
		// =====================================================================
		bool NkScenaModele::SceneDemo() {
			if (!Pret()) {
				return false;
			}
			scene.NouvelleScene();
			scene.chemin = kSceneDemo;
			const bool ok = scene.Enregistrer();
			mCameraForcee = ecs::NkEntityId::Invalid();
			mRevision = ~0u;
			(void)Synchroniser();
			mDernierTemps = -1.f;
			return ok;
		}

		bool NkScenaModele::OuvrirScene(const char *fichier) {
			if (!Pret() || fichier == nullptr) {
				return false;
			}
			if (!scene.Ouvrir(fichier)) {
				return false;
			}
			// Les entites ont ete RECREEES : la camera forcee n'existe plus, les
			// pistes se relient de nouveau par leur nom.
			mCameraForcee = ecs::NkEntityId::Invalid();
			mRevision = ~0u;
			(void)Synchroniser();
			mDernierTemps = -1.f;
			if (mPerdues > 0u) {
				Annoncer(NkString::Format("%u piste(s) ou plan(s) nomment une entité absente de cette scène : ils n'animent rien",
										  static_cast<unsigned>(mPerdues))
							 .CStr(),
						 2);
			}
			return true;
		}

		// =====================================================================
		// LA SEQUENCE
		// =====================================================================
		void NkScenaModele::NouvelleSequence(float32 duree, float32 ips) {
			frise = editorkit::NkTimelineModel{};
			frise.fps = ips > 0.f ? ips : 24.f;
			frise.duration = duree > 1e-3f ? duree : 5.f;
			nom = "Séquence 1";
			frise.rootLabel = nom;
			frise.FrameAll();
			*mReste = NkSequence{};
			chemin = kSequenceDefaut;
			mRevision = ~0u;
			(void)Synchroniser();
			modifie = false;
			choix = NkScenaChoix::Sequence;
			mDernierTemps = -1.f;
		}

		bool NkScenaModele::Synchroniser() {
			const bool force = mRevision == ~0u;
			if (!force && frise.revision == mRevision) {
				return false;
			}
			if (!force) {
				modifie = true;
			}
			mRevision = frise.revision;
			NkScenaSequenceDepuisFrise(frise, mReste, *mSeq);
			mSeq->name = nom;
			mSeq->scene = scene.chemin;
			// La sortie : ce que le fichier d'origine portait (format, flou...), plus
			// ce que NKScena regle (taille, dossier, prefixe) et la plage de la frise.
			const float32 debut = mSeq->renderOutput.startTime;
			const float32 fin = mSeq->renderOutput.endTime;
			mSeq->renderOutput = mReste->renderOutput;
			mSeq->renderOutput.startTime = debut;
			mSeq->renderOutput.endTime = fin;
			mSeq->renderOutput.width = sortie.largeur;
			mSeq->renderOutput.height = sortie.hauteur;
			mSeq->renderOutput.fps = mSeq->fps;
			mSeq->renderOutput.outputDirectory = sortie.dossier;
			mSeq->renderOutput.filePrefix = sortie.prefixe;
			mPerdues = Pret() ? mSeq->BindByName(scene.Monde()) : 0u;
			mDernierTemps = -1.f;
			return true;
		}

		bool NkScenaModele::Enregistrer(const char *fichier) {
			if (fichier != nullptr && fichier[0] != '\0') {
				chemin = fichier;
			}
			mRevision = ~0u;
			(void)Synchroniser();
			PreparerDossier(chemin.CStr());
			const bool ok = mSeq->SaveToFile(chemin.CStr());
			if (ok) {
				modifie = false;
				Annoncer(NkString::Format("Séquence enregistrée : %s (scène : %s)", chemin.CStr(), scene.chemin.CStr()).CStr(), 1);
			} else {
				Annoncer(NkString::Format("ÉCHEC de l'enregistrement de %s : %s", chemin.CStr(), NkSequenceDernierRefus()).CStr(), 3);
			}
			return ok;
		}

		bool NkScenaModele::Ouvrir(const char *fichier) {
			if (fichier == nullptr || fichier[0] == '\0') {
				return false;
			}
			// Lire D'ABORD : un fichier illisible laisse tout tel quel.
			NkSequence lue;
			if (!lue.LoadFromFile(fichier)) {
				Annoncer(NkString::Format("Séquence illisible : %s — %s (rien n'a changé)", fichier, NkSequenceDernierRefus()).CStr(),
						 3);
				return false;
			}
			if (!lue.scene.Empty() && lue.scene != scene.chemin) {
				if (!OuvrirScene(lue.scene.CStr())) {
					Annoncer(NkString::Format("La scène de la séquence (%s) ne s'ouvre pas : la séquence s'applique à la scène ouverte",
											  lue.scene.CStr())
								 .CStr(),
							 2);
				}
			} else if (lue.scene.Empty()) {
				Annoncer("Cette séquence ne nomme pas de scène (format v1) : elle s'applique à la scène ouverte", 2);
			}
			nom = lue.name.Empty() ? NkString("Séquence") : lue.name;
			sortie.largeur = lue.renderOutput.width > 0u ? lue.renderOutput.width : sortie.largeur;
			sortie.hauteur = lue.renderOutput.height > 0u ? lue.renderOutput.height : sortie.hauteur;
			sortie.dossier = lue.renderOutput.outputDirectory;
			sortie.prefixe = lue.renderOutput.filePrefix;
			*mReste = NkSequence{};
			mReste->renderOutput = lue.renderOutput;
			NkScenaFriseDepuisSequence(lue, frise, mReste);
			frise.rootLabel = nom;
			frise.cursor = frise.PlayStart();
			frise.playing = false;
			frise.FrameAll();
			chemin = fichier;
			mRevision = ~0u;
			(void)Synchroniser();
			modifie = false;
			choix = NkScenaChoix::Sequence;
			Annoncer(NkString::Format("Séquence ouverte : %s — %u piste(s), %u plan(s)%s", fichier,
									  static_cast<unsigned>(mSeq->tracks.Size()),
									  static_cast<unsigned>(mSeq->cameraTrack.shots.Size()),
									  mPerdues > 0u ? " — des cibles manquent dans la scène" : "")
						 .CStr(),
					 mPerdues > 0u ? 2 : 1);
			return true;
		}

		// =====================================================================
		// LES PISTES ET LES CLES
		// =====================================================================
		nk_uint64 NkScenaModele::NouvelId() const noexcept {
			nk_uint64 m = 0;
			for (uint32 i = 0; i < (uint32)frise.tracks.Size(); ++i) {
				m = frise.tracks[i].id > m ? frise.tracks[i].id : m;
			}
			return m + 1u;
		}

		bool NkScenaModele::APisteTransform(const char *entite) const {
			for (uint32 i = 0; i < (uint32)frise.tracks.Size(); ++i) {
				const NkTimelineTrack &t = frise.tracks[i];
				if (t.object == NkString(entite) && IndicePropriete(t.property) >= 0) {
					return true;
				}
			}
			return false;
		}

		bool NkScenaModele::AjouterPisteTransform(const char *entite) {
			if (!Entite(entite).IsValid()) {
				Annoncer(NkString::Format("Pas d'entité « %s » dans la scène", entite != nullptr ? entite : "").CStr(), 2);
				return false;
			}
			if (APisteTransform(entite)) {
				Annoncer(NkString::Format("« %s » a déjà sa piste de transformation", entite).CStr(), 2);
				return false;
			}
			// UN SEUL geste a annuler pour les trois pistes (AddTrack en empilerait trois).
			frise.PushUndo();
			const char *const kProps[3] = {kNkScenaPosition, kNkScenaRotation, kNkScenaEchelle};
			nk_uint64 premiere = 0;
			for (int32 k = 0; k < 3; ++k) {
				NkTimelineTrack t;
				t.id = NouvelId();
				t.object = entite;
				t.property = kProps[k];
				t.label = kProps[k];
				t.kind = NkTimelineValueKind::Nombre;
				t.channels = 3;
				frise.tracks.PushBack(t);
				premiere = premiere == 0 ? t.id : premiere;
			}
			frise.activeTrack = premiere;
			frise.Touch();
			choix = NkScenaChoix::Piste;
			Annoncer(NkString::Format("Piste de transformation : %s (position, rotation, échelle)", entite).CStr());
			return true;
		}

		nk_uint64 NkScenaModele::PistePlans() const {
			for (uint32 i = 0; i < (uint32)frise.tracks.Size(); ++i) {
				const NkTimelineTrack &t = frise.tracks[i];
				if (t.kind == NkTimelineValueKind::Clips && t.property == NkString(kNkScenaPlans)) {
					return t.id;
				}
			}
			return 0;
		}

		nk_uint64 NkScenaModele::AjouterPistePlans() {
			const nk_uint64 deja = PistePlans();
			if (deja != 0) {
				return deja;
			}
			frise.PushUndo();
			NkTimelineTrack t;
			t.id = NouvelId();
			t.property = kNkScenaPlans;
			t.label = kNkScenaPlans;
			t.kind = NkTimelineValueKind::Clips;
			t.channels = 1;
			t.showCurve = false;
			// En TETE, comme la piste « Camera Cuts » du Sequencer d'UE5.
			frise.tracks.Insert(frise.tracks.Begin(), t);
			frise.activeTrack = t.id;
			frise.Touch();
			choix = NkScenaChoix::Piste;
			Annoncer("Piste des plans caméra ajoutée : « + » sur la piste pose un plan");
			return t.id;
		}

		nk_uint64 NkScenaModele::AjouterPlan(const char *camera, float32 debut, float32 duree) {
			const ecs::NkEntityId e = camera != nullptr ? Entite(camera) : ecs::NkEntityId::Invalid();
			if (!e.IsValid() || !scene.Monde().Has<ecs::NkCameraComponent>(e)) {
				Annoncer(NkString::Format("Pas de caméra « %s » dans la scène", camera != nullptr ? camera : "").CStr(), 2);
				return 0;
			}
			const nk_uint64 piste = AjouterPistePlans();
			duree = duree > frise.FrameDuration() ? duree : frise.FrameDuration();
			const nk_uint64 clip = frise.AddClip(piste, NkString(camera), debut, duree, duree);
			if (clip != 0) {
				if (editorkit::NkTimelineClip *c = frise.Clip(piste, clip)) {
					c->loop = false;
				}
				if (debut + duree > frise.duration) {
					frise.duration = debut + duree;
				}
				frise.activeClip = clip;
				frise.Touch();
				choix = NkScenaChoix::Plan;
				Annoncer(NkString::Format("Plan : %s, de %.2f s à %.2f s", camera, static_cast<double>(debut),
										  static_cast<double>(debut + duree))
							 .CStr());
			}
			return clip;
		}

		bool NkScenaModele::LireVivant(const NkTimelineTrack &piste, float32 out[4]) {
			out[0] = out[1] = out[2] = out[3] = 0.f;
			const int32 k = IndicePropriete(piste.property);
			if (k < 0) {
				return false;
			}
			const ecs::NkEntityId e = Entite(piste.object.CStr());
			const ecs::NkTransform *tf = e.IsValid() ? scene.Monde().Get<ecs::NkTransform>(e) : nullptr;
			if (tf == nullptr) {
				return false;
			}
			if (k == 0) {
				out[0] = tf->localPosition.x;
				out[1] = tf->localPosition.y;
				out[2] = tf->localPosition.z;
			} else if (k == 1) {
				// Tangage, lacet, roulis en degres, dans la convention des canaux de
				// rotation du sequenceur (lacet puis tangage puis roulis).
				NkSequenceDegreesFromRotation(tf->localRotation, out[0], out[1], out[2]);
			} else {
				out[0] = tf->localScale.x;
				out[1] = tf->localScale.y;
				out[2] = tf->localScale.z;
			}
			return true;
		}

		void NkScenaModele::Derouler(const NkTimelineTrack &piste, float32 t, float32 v[4]) const {
			// Un angle relu vit dans ]-180, 180] : poser 190 degres apres 170 ferait
			// tourner la courbe de 340 degres a l'envers. La valeur posee est donc
			// celle, a 360 degres pres, la plus PROCHE de la courbe a cet instant
			// (le « derouler » des courbes de rotation d'UE5 et de Blender).
			if (IndicePropriete(piste.property) != 1 || piste.keys.Empty()) {
				return;
			}
			float32 courbe[4];
			if (!NkScenaEvaluerPiste(frise, piste, t, courbe)) {
				return;
			}
			for (uint32 c = 0; c < 3; ++c) {
				while (v[c] - courbe[c] > 180.f) {
					v[c] -= 360.f;
				}
				while (courbe[c] - v[c] > 180.f) {
					v[c] += 360.f;
				}
			}
		}

		bool NkScenaModele::PoserCle(nk_uint64 piste, float32 t) {
			const NkTimelineTrack *tr = frise.Track(piste);
			if (tr == nullptr || tr->kind == NkTimelineValueKind::Clips || tr->locked) {
				return false;
			}
			float32 v[4];
			if (!LireVivant(*tr, v)) {
				return false;
			}
			Derouler(*tr, t, v);
			return frise.SetKey(piste, t, v) >= 0;
		}

		bool NkScenaModele::PoserCles(const char *entite, float32 t) {
			if (!APisteTransform(entite)) {
				return false;
			}
			frise.PushUndo();
			bool une = false;
			for (uint32 i = 0; i < (uint32)frise.tracks.Size(); ++i) {
				const NkTimelineTrack &tr = frise.tracks[i];
				if (tr.object != NkString(entite) || IndicePropriete(tr.property) < 0 || tr.locked) {
					continue;
				}
				float32 v[4];
				if (LireVivant(tr, v)) {
					Derouler(tr, t, v);
					une = frise.SetKey(tr.id, t, v, 255, false) >= 0 || une;
				}
			}
			return une;
		}

		void NkScenaModele::Cameras(NkVector<NkString> &noms) {
			noms.Clear();
			if (!Pret()) {
				return;
			}
			ecs::NkWorld &w = scene.Monde();
			w.Query<const ecs::NkCameraComponent>().ForEach([&](ecs::NkEntityId id, const ecs::NkCameraComponent &) {
				const ecs::NkName *n = w.Get<ecs::NkName>(id);
				if (n != nullptr && !(n->value[0] == '_' && n->value[1] == '_')) {
					noms.PushBack(NkString(n->value));
				}
			});
		}

		uint32 NkScenaModele::NombreCles() const noexcept {
			uint32 n = 0;
			for (uint32 i = 0; i < (uint32)frise.tracks.Size(); ++i) {
				n += (uint32)frise.tracks[i].keys.Size();
			}
			return n;
		}

		// =====================================================================
		// LE TEMPS
		// =====================================================================
		void NkScenaModele::Evaluer(float32 t) {
			(void)Synchroniser();
			if (!Pret()) {
				return;
			}
			if (!frise.AnySolo()) {
				mSeq->Evaluate(t, scene.Monde());
			} else {
				// SOLO : seules les pistes « solo » jouent. Une COPIE, muette ailleurs
				// (la sequence qui s'enregistre ne porte pas cet etat d'interface).
				NkSequence copie = *mSeq;
				for (uint32 i = 0; i < (uint32)copie.tracks.Size(); ++i) {
					NkTrack &tr = copie.tracks[i];
					for (uint32 c = 0; c < (uint32)tr.channels.Size(); ++c) {
						const NkString canal(tr.channels[c].propertyName);
						for (uint32 j = 0; j < (uint32)frise.tracks.Size(); ++j) {
							const NkTimelineTrack &ft = frise.tracks[j];
							const char *prefixe = NkScenaPrefixeCanal(ft.property);
							if (ft.object == tr.entityName && prefixe != nullptr && canal.StartsWith(prefixe) &&
								!frise.TrackActive(ft)) {
								tr.channels[c].muted = true;
							}
						}
					}
				}
				const nk_uint64 plans = PistePlans();
				if (const NkTimelineTrack *ft = frise.Track(plans)) {
					copie.cameraTrack.muted = copie.cameraTrack.muted || !frise.TrackActive(*ft);
				}
				copie.Evaluate(t, scene.Monde());
			}
			mDernierTemps = t;
			AppliquerCameras(t);
		}

		bool NkScenaModele::Image(float32 dt) {
			const bool refaite = Synchroniser();
			const bool bouge = frise.Advance(dt);
			const float32 t = frise.cursor;
			const bool aPoser = refaite || bouge || t != mDernierTemps;
			if (aPoser) {
				Evaluer(t);
			} else {
				AppliquerCameras(t);
			}
			return aPoser;
		}

		ecs::NkEntityId NkScenaModele::CameraDuPlan(float32 t) const {
			const ecs::NkEntityId e = mSeq->GetActiveCameraAt(t);
			if (!e.IsValid() || !scene.Pret() || !const_cast<NkScenaModele *>(this)->scene.Monde().IsAlive(e) ||
				!const_cast<NkScenaModele *>(this)->scene.Monde().Has<ecs::NkCameraComponent>(e)) {
				return ecs::NkEntityId::Invalid();
			}
			return e;
		}

		NkString NkScenaModele::PlanA(float32 t) const {
			const NkCameraShot *p = mSeq->cameraTrack.GetActiveShot(t);
			return p != nullptr ? p->cameraName : NkString();
		}

		void NkScenaModele::AppliquerCameras(float32 t) {
			if (!Pret()) {
				return;
			}
			ecs::NkWorld &w = scene.Monde();
			const ecs::NkEntityId voulue = vueCamera ? CameraDuPlan(t) : ecs::NkEntityId::Invalid();
			if (voulue == mCameraForcee) {
				return;
			}
			// La camera d'avant retrouve SA priorite (la scene n'est pas modifiee).
			if (mCameraForcee.IsValid() && w.IsAlive(mCameraForcee)) {
				if (ecs::NkCameraComponent *c = w.Get<ecs::NkCameraComponent>(mCameraForcee)) {
					c->priority = mPrioriteAvant;
				}
			}
			mCameraForcee = ecs::NkEntityId::Invalid();
			if (voulue.IsValid()) {
				if (ecs::NkCameraComponent *c = w.Get<ecs::NkCameraComponent>(voulue)) {
					mPrioriteAvant = c->priority;
					c->priority = kPrioritePlan;
					mCameraForcee = voulue;
				}
			}
		}

		// =====================================================================
		// L'EXEMPLE
		// =====================================================================
		bool NkScenaModele::Exemple(float32 duree) {
			if (!Pret()) {
				return false;
			}
			(void)SceneDemo();
			NouvelleSequence(duree, 24.f);
			nom = "Exemple — le joueur traverse";
			frise.rootLabel = nom;
			chemin = "Build/NKScena/exemple.nkseq";
			ecs::NkWorld &w = scene.Monde();
			const ecs::NkEntityId joueur = Entite("Joueur");
			const ecs::NkEntityId camera = Entite("Caméra");
			ecs::NkTransform *tj = joueur.IsValid() ? w.Get<ecs::NkTransform>(joueur) : nullptr;
			ecs::NkTransform *tc = camera.IsValid() ? w.Get<ecs::NkTransform>(camera) : nullptr;
			if (tj == nullptr || tc == nullptr) {
				Annoncer("Exemple : la scène de NogeDemo n'a pas son joueur ou sa caméra", 3);
				return false;
			}
			(void)AjouterPlan("Caméra", 0.f, duree);
			(void)AjouterPisteTransform("Joueur");
			(void)AjouterPisteTransform("Caméra");
			// Le joueur traverse en tournant d'un demi-tour.
			tj->localPosition = math::NkVec3f{-3.f, 0.5f, 0.f};
			tj->localRotation = NkSequenceRotationFromDegrees(0.f, 0.f, 0.f);
			(void)PoserCles("Joueur", 0.f);
			tj->localPosition = math::NkVec3f{3.f, 0.5f, 1.5f};
			tj->localRotation = NkSequenceRotationFromDegrees(0.f, 90.f, 0.f);
			(void)PoserCles("Joueur", duree);
			// La camera avance vers le sol et pivote (un travelling avant).
			tc->localPosition = math::NkVec3f{7.f, 4.5f, 9.f};
			tc->localRotation = NkSequenceRotationFromDegrees(-20.f, 35.f, 0.f);
			(void)PoserCles("Caméra", 0.f);
			tc->localPosition = math::NkVec3f{3.f, 2.2f, 6.5f};
			tc->localRotation = NkSequenceRotationFromDegrees(-12.f, 22.f, 0.f);
			(void)PoserCles("Caméra", duree);
			frise.undoStack.Clear();
			frise.redoStack.Clear();
			frise.cursor = 0.f;
			frise.FrameAll();
			mRevision = ~0u;
			(void)Synchroniser();
			Evaluer(0.f);
			modifie = false;
			choix = NkScenaChoix::Sequence;
			Annoncer("Exemple : le joueur traverse, la caméra avance — Espace pour jouer");
			return true;
		}

	} // namespace nkscena
} // namespace nkentseu
