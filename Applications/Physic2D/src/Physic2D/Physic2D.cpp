// =============================================================================
// Physic2D.cpp — etats, entrees, outils
//
// L'ORDRE D'UNE TRAME (celui d'Unkeny.h, et il n'est pas indifferent)
//   1. OnPointer / OnEvent  -> les gestes posent leur INTENTION
//   2. OnTick               -> les outils continus (pinceau, aimant, gomme,
//                              saisie d'un rigide), PUIS scene.Pas(dt)
//   3. OnDraw               -> lit, ne modifie rien
//
// ⚠️ UNITES : tout ce qui touche le monde est en METRES, secondes, kg — la
//   convention de NKPhysics. Les pixels n'existent qu'entre OnPointer et la
//   camera (NkVue2D::EcranVersMonde), jamais plus loin.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Physic2D/Physic2D.h"
#include "Physic2D/Physic2DBanc.h"
#include "NKPhysics/NkParticules2DFabrique.h"

#include "Unkeny/Banc/NkUnkenyBanc.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkPath.h"

#include <chrono>
#include <cstdio>

namespace nkentseu {

	using namespace unkeny;
	using physic2d::NkActeur;
	using physic2d::NkActeurInfo;

	namespace {
		// Rayons d'action des outils, en metres. Ils sont AUSSI ceux du curseur
		// dessine : un cercle qui mentirait sur la portee de l'outil serait pire
		// que pas de cercle.
		constexpr float32 kRayonSaisie = 0.6f;
		constexpr float32 kRayonGomme = 0.5f;
		constexpr float32 kRayonAimant = 3.0f;
		constexpr float32 kRayonExplosion = 2.6f;
		constexpr float32 kVitesseExplosion = 9.f; ///< m/s au centre
		constexpr float32 kAccelAimant = 45.f;	   ///< m/s^2 au centre
		constexpr float32 kRayonObstacle = 0.10f;

		float32 Longueur(const NkVec2f &v) {
			return math::NkSqrt(v.x * v.x + v.y * v.y);
		}

		/// Distance signee d'un point a la forme d'un collisionneur (negatif = dedans).
		float32 DistanceForme(const NkTransform2D &t, const NkCollisionneur2D &c, const NkVec2f &p) {
			// Dans le repere local de la forme.
			const float32 co = math::NkCos(-t.rotation);
			const float32 si = math::NkSin(-t.rotation);
			const NkVec2f d(p.x - t.position.x - c.decalage.x, p.y - t.position.y - c.decalage.y);
			const NkVec2f l(d.x * co - d.y * si, d.x * si + d.y * co);
			switch (c.forme) {
				case NkForme2D::NK_CERCLE:
					return Longueur(l) - c.rayon;
				case NkForme2D::NK_CAPSULE: {
					const float32 x = math::NkClamp(l.x, -c.demiTaille.x, c.demiTaille.x);
					return Longueur(NkVec2f(l.x - x, l.y)) - c.rayon;
				}
				default: {
					const float32 qx = math::NkAbs(l.x) - c.demiTaille.x;
					const float32 qy = math::NkAbs(l.y) - c.demiTaille.y;
					const float32 ex = qx > 0.f ? qx : 0.f;
					const float32 ey = qy > 0.f ? qy : 0.f;
					return Longueur(NkVec2f(ex, ey)) + math::NkMin(math::NkMax(qx, qy), 0.f);
				}
			}
		}
	} // namespace

	Physic2D::Physic2D() {
		Config().title = "Physic2D — Unkeny";
		Config().width = 1600;
		Config().height = 900;
		Config().clearColor = renderer::NkColor2D{21, 21, 23, 255};

		// Le theme : celui d'UE5 — gris neutres tres sombres, un seul bleu
		// d'accent. On COPIE le defaut d'Unkeny et on change des valeurs : pas
		// une couleur ecrite en dur ailleurs dans la demo.
		mTheme.fond = NkColor(21, 21, 23);
		mTheme.panneau = NkColor(36, 36, 38);
		mTheme.panneauActif = NkColor(58, 58, 62);
		mTheme.bord = NkColor(12, 12, 13);
		mTheme.texte = NkColor(214, 214, 218);
		mTheme.texteFaible = NkColor(138, 138, 146);
		mTheme.accent = NkColor(14, 134, 255);
		mTheme.succes = NkColor(98, 196, 84);
		mTheme.alerte = NkColor(226, 72, 60);
		mTheme.or_ = NkColor(240, 178, 50);
		mTheme.arrondi = 0.16f;
		mTheme.epaisseurBord = 1.f;
	}

	NkOptional<int> Physic2D::OnCommandLine(const NkVector<NkString> &args) {
		for (uint32 i = 0; i < args.Size(); ++i) {
			if (args[i] == "--selftest") {
				// Le banc du MOTEUR d'abord, puis celui de l'integration : un echec
				// d'Unkeny se lit ainsi a sa source, pas dans ses consequences.
				const int32 moteur = unkeny::NkUnkenyLancerBanc();
				std::printf("\n");
				const int32 demo = physic2d::NkPhysic2DLancerBanc();
				return NkOptional<int>((moteur != 0 || demo != 0) ? 1 : 0);
			}
			if (args[i].StartsWith("--niveau=")) {
				const NkString v = args[i].SubStr(9);
				int32 n = 0;
				for (const char *c = v.CStr(); *c >= '0' && *c <= '9'; ++c) {
					n = n * 10 + (*c - '0');
				}
				mNiveau = n % physic2d::NK_NB_NIVEAUX;
			}
		}
		return NkOptional<int>();
	}

	bool Physic2D::OnGuiInit() {
		NkSceneConfig cfg;
		cfg.physique = true;
		cfg.particules = true;
		cfg.gravite = NkVec2f(0.f, -9.81f);
		if (!mScene.Init(cfg)) {
			return false;
		}
		// Les textures AVANT le premier niveau : ses acteurs rigides les portent.
		mTextures.Brancher(&renderer::NkCanvasGuiApp::RelaisTeleversement, static_cast<renderer::NkCanvasGuiApp *>(this));
		physic2d::NkCreerTexturesActeurs(mTextures);
		// Le son : facultatif. Sans peripherique, la demo continue en silence.
		if (mSons.Demarrer()) {
			physic2d::NkCreerSonsActeurs(mSons, mSonPose, mSonExplosion, mSonCoupe);
		}
		Charger(mNiveau);
		Planifier();
		Cadrer();
		return true;
	}

	void Physic2D::OnLayout(const renderer::NkLayoutInfo &info) {
		renderer::NkCanvasGuiApp::OnLayout(info); // les polices suivent l'ecran
		Planifier();
		if (!mCameraTouchee) {
			Cadrer();
		} else {
			mScene.Camera().PoserViseur(mPlan.vue);
		}
	}

	// =========================================================================
	// Etats
	// =========================================================================
	void Physic2D::Charger(int32 niveau) {
		AnnulerGeste();
		mNiveau = (niveau % physic2d::NK_NB_NIVEAUX + physic2d::NK_NB_NIVEAUX) % physic2d::NK_NB_NIVEAUX;
		physic2d::NkChargerNiveau(mScene, mNiveau);
		if (physics::NkParticules2D *p = mScene.Particules()) {
			// Un niveau repart des reglages d'usine, gravite comprise.
			p->reglages.gravite = NkVec2f(0.f, -9.81f);
			p->reglages.echelleTemps = 1.f;
			p->reglages.amortAir = 0.05f;
			p->reglages.sousPas = 8;
			if (mScene.MondePhysique() != nullptr) {
				mScene.MondePhysique()->SetGravity(math::NkVec3f(0.f, -9.81f, 0.f));
			}
		}
		mPhoto.valide = false;
		mSelection = 0;
		// Un niveau charge PENDANT le jeu se joue ; charge a l'arret, il attend.
		if (mEtat == NkEtat::NK_JEU) {
			mScene.Photographier(mPhoto);
		}
	}

	void Physic2D::Jouer() {
		if (!mPhoto.valide) {
			mScene.Photographier(mPhoto); // ce que "Arret" rendra
		}
		mEtat = NkEtat::NK_JEU;
	}

	void Physic2D::Pause() {
		if (mEtat == NkEtat::NK_JEU) {
			mEtat = NkEtat::NK_PAUSE;
		}
	}

	void Physic2D::Arreter() {
		AnnulerGeste();
		if (mPhoto.valide) {
			mScene.Restaurer(mPhoto);
			AlignerGravite();
		}
		mPhoto.valide = false;
		mSelection = 0;
		mEtat = NkEtat::NK_EDITION;
	}

	void Physic2D::AlignerGravite() {
		// La photo et le fichier rendent les reglages des PARTICULES, pas ceux du
		// monde rigide : on realigne la gravite, qui doit rester UNE.
		if (mScene.MondePhysique() != nullptr && mScene.Particules() != nullptr) {
			const NkVec2f g = mScene.Particules()->reglages.gravite;
			mScene.MondePhysique()->SetGravity(math::NkVec3f(g.x, g.y, 0.f));
		}
	}

	const char *Physic2D::CheminSauvegarde() {
		if (mChemin.Empty()) {
			// Le dossier de l'application (%APPDATA% sous Windows, ~/.config
			// ailleurs). S'il n'existe pas (plateforme sans HOME), le dossier
			// courant.
			const NkPath base = NkDirectory::GetAppDataDirectory();
			if (!base.ToString().Empty()) {
				const NkPath dossier = base / "Physic2D";
				NkDirectory::CreateRecursive(dossier);
				mChemin = (dossier / "sauvegarde.nkscene").ToString();
			} else {
				mChemin = "physic2d.nkscene";
			}
		}
		return mChemin.CStr();
	}

	void Physic2D::Annoncer(const char *texte, bool erreur) {
		mAnnonce = texte;
		mAnnonceAge = 0.f;
		mAnnonceErreur = erreur;
	}

	void Physic2D::Sauver() {
		AnnulerGeste();
		if (NkSauverSceneFichier(mScene, CheminSauvegarde(), &mTextures)) {
			Annoncer("Scène enregistrée", false);
		} else {
			Annoncer("Enregistrement impossible", true);
		}
	}

	void Physic2D::Ouvrir() {
		AnnulerGeste();
		NkString erreur;
		if (NkChargerSceneFichier(mScene, CheminSauvegarde(), &mTextures, &erreur)) {
			AlignerGravite();
			mPhoto.valide = false;
			mSelection = 0;
			mEtat = NkEtat::NK_PAUSE; // on retrouve la scene figee : a l'utilisateur de relancer
			Annoncer("Scène ouverte — en pause", false);
		} else {
			Annoncer(erreur.Empty() ? "Aucune sauvegarde" : erreur.CStr(), true);
		}
	}

	void Physic2D::UnPas() {
		if (!mPhoto.valide) {
			mScene.Photographier(mPhoto);
		}
		mEtat = NkEtat::NK_PAUSE;
		mScene.Pas(mScene.Config().pasFixe);
	}

	void Physic2D::Commande(int32 cmd) {
		switch (cmd) {
			case CMD_JOUER:
				(mEtat == NkEtat::NK_JEU) ? Pause() : Jouer();
				break;
			case CMD_PAS:
				UnPas();
				break;
			case CMD_ARRET:
				Arreter();
				break;
			case CMD_NIV_PREC:
				Charger(mNiveau - 1);
				break;
			case CMD_NIV_SUIV:
				Charger(mNiveau + 1);
				break;
			case CMD_VUE: {
				const int32 n = static_cast<int32>(NkModeRenduParticules::NK_COUNT);
				mRendu.mode = static_cast<NkModeRenduParticules>((static_cast<int32>(mRendu.mode) + 1) % n);
				break;
			}
			case CMD_GRILLE:
				mGrille = !mGrille;
				break;
			case CMD_CADRER:
				mCameraTouchee = false;
				Cadrer();
				break;
			case CMD_SAUVER:
				Sauver();
				break;
			case CMD_OUVRIR:
				Ouvrir();
				break;
			case CMD_PANNEAU:
				mDetailsOuvert = !mDetailsOuvert;
				Planifier();
				mScene.Camera().PoserViseur(mPlan.vue);
				break;
			default:
				break;
		}
	}

	void Physic2D::Regler(int32 reglage, bool plus) {
		physics::NkParticules2D *p = mScene.Particules();
		if (p == nullptr) {
			return;
		}
		physics::NkReglagesP2D &r = p->reglages;
		const float32 s = plus ? 1.f : -1.f;
		switch (reglage) {
			case REG_TEMPERATURE:
				r.temperature = math::NkClamp(r.temperature + s * 10.f, 0.f, 100.f);
				break;
			case REG_VENT:
				r.vent = math::NkClamp(r.vent + s * 2.f, -20.f, 20.f);
				break;
			case REG_GRAVITE: {
				// La gravite est UNE : celle des rigides suit celle des particules,
				// sinon une caisse tomberait plus vite que le blob qui la porte.
				const float32 g = math::NkClamp(r.gravite.y - s * 1.962f, -29.43f, 9.81f);
				r.gravite.y = math::NkAbs(g) < 0.01f ? 0.f : g;
				if (physics::NkPhysicsWorld *w = mScene.MondePhysique()) {
					w->SetGravity(math::NkVec3f(0.f, r.gravite.y, 0.f));
				}
				break;
			}
			case REG_AIR:
				// ⚠️ PAS de reglage d'echelle de temps ici : NkReglagesP2D en a une,
				// mais elle ne ralentit que les particules. Les rigides couples
				// iraient a vitesse normale et le couplage ne serait plus juste.
				r.amortAir = math::NkClamp(r.amortAir + s * 0.25f, 0.f, 3.f);
				break;
			case REG_SOUSPAS:
				r.sousPas = math::NkClamp(r.sousPas + (plus ? 2 : -2), 2, 20);
				break;
			default:
				break;
		}
	}

	// =========================================================================
	// Trame
	// =========================================================================
	void Physic2D::OnTick(float32 dt) {
		const float32 h = math::NkMin(dt, 0.05f);
		OutilsContinus(h);

		if (mEtat == NkEtat::NK_JEU) {
			const auto t0 = std::chrono::steady_clock::now();
			mScene.Pas(h);
			const int32 n = mScene.DernierNbPas();
			if (n > 0) {
				const float32 ms = static_cast<float32>(
					std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
				mMsParPas = ms / static_cast<float32>(n);
				mMsLisse = mMsLisse <= 0.f ? mMsParPas : mMsLisse + (mMsParPas - mMsLisse) * 0.1f;
			}
		}

		// Ce qui s'efface avec le temps.
		mTraceAge += h;
		mAnnonceAge += h;
		mCoupeAge += h;
		mSons.Avancer(mScene); // les NkSource2D : apres Pas, comme le veut Unkeny
		if (!mGeste && mTraceAge > 0.35f) {
			mTraceN = 0;
		}
		for (NkEclair &e : mEclairs) {
			e.age += h;
		}
		// Un corps selectionne qui a disparu (gomme, coupe totale) : on l'oublie.
		if (mSelection != 0u && (mScene.Particules() == nullptr || mScene.Particules()->IndexCorps(mSelection) < 0)) {
			mSelection = 0u;
		}
	}

	// Le son se coupe quand l'application part en arriere-plan : sinon il
	// continue par-dessus ce que l'utilisateur fait ensuite (NkCanvasApp.h).
	void Physic2D::OnPause() {
		mSons.PoserMuet(true);
	}
	void Physic2D::OnResume() {
		mSons.PoserMuet(false);
	}
	void Physic2D::OnShutdown() {
		mSons.Arreter();
	}

	// =========================================================================
	// Entrees
	// =========================================================================
	bool Physic2D::OnEvent(const NkEvent &e) {
		// Survol : pour le curseur d'outil. Jamais consomme.
		if (const auto *m = e.As<NkMouseMoveEvent>()) {
			const NkVec2f p(static_cast<float32>(m->GetX()), static_cast<float32>(m->GetY()));
			if (mPanSouris) {
				NkVue2D &cam = mScene.Camera();
				const float32 z = cam.Zoom();
				cam.PoserCentre(cam.Centre() + NkVec2f(-(p.x - mSouris.x) / z, (p.y - mSouris.y) / z));
				mCameraTouchee = true;
			}
			mSouris = p;
			return false;
		}
		// Bouton droit ou milieu : deplacer la vue (le gauche reste aux outils).
		if (const auto *b = e.As<NkMouseButtonPressEvent>()) {
			if (b->GetButton() == NkMouseButton::NK_MB_RIGHT || b->GetButton() == NkMouseButton::NK_MB_MIDDLE) {
				const NkVec2f p(static_cast<float32>(b->GetX()), static_cast<float32>(b->GetY()));
				if (NkDansRect(mPlan.vue, p)) {
					mPanSouris = true;
					mSouris = p;
					return true;
				}
			}
			return false;
		}
		if (const auto *b = e.As<NkMouseButtonReleaseEvent>()) {
			if (b->GetButton() == NkMouseButton::NK_MB_RIGHT || b->GetButton() == NkMouseButton::NK_MB_MIDDLE) {
				const bool etait = mPanSouris;
				mPanSouris = false;
				return etait;
			}
			return false;
		}
		if (const auto *w = e.As<NkMouseWheelVerticalEvent>()) {
			const NkVec2f p(static_cast<float32>(w->GetX()), static_cast<float32>(w->GetY()));
			if (NkDansRect(mPlan.vue, p)) {
				Zoomer(p, w->GetDeltaY() > 0.0 ? 1.15f : 1.f / 1.15f);
				return true;
			}
			return false;
		}
		return false;
	}

	void Physic2D::Zoomer(const NkVec2f &ecran, float32 facteur) {
		NkVue2D &cam = mScene.Camera();
		const NkVec2f avant = cam.EcranVersMonde(ecran);
		cam.PoserZoom(math::NkClamp(cam.Zoom() * facteur, 8.f, 800.f));
		const NkVec2f apres = cam.EcranVersMonde(ecran);
		cam.PoserCentre(cam.Centre() + (avant - apres)); // le point sous le curseur ne bouge pas
		mCameraTouchee = true;
	}

	bool Physic2D::OnPointer(const NkPointer &p) {
		const NkVec2f pos(p.x, p.y);

		// --- Deux doigts : pincement. Il passe AVANT tout le reste ------------
		if (p.fromTouch) {
			int32 k = -1;
			for (int32 i = 0; i < 2; ++i) {
				if (mDoigts[i].actif && mDoigts[i].id == p.id) {
					k = i;
				}
			}
			if (p.phase == NkPointerPhase::NK_POINTER_DOWN && k < 0) {
				for (int32 i = 0; i < 2 && k < 0; ++i) {
					if (!mDoigts[i].actif) {
						k = i;
						mDoigts[i].actif = true;
						mDoigts[i].id = p.id;
					}
				}
			}
			if (k >= 0) {
				mDoigts[k].pos = pos;
			}
			const bool deux = mDoigts[0].actif && mDoigts[1].actif;
			if (deux && !mPincement && p.phase == NkPointerPhase::NK_POINTER_DOWN) {
				// Le second doigt ANNULE le geste du premier : un pincement qui
				// commencerait par poser un blob serait une surprise.
				AnnulerGeste();
				mPincement = true;
				mPinceDistance = math::NkMax(Longueur(mDoigts[1].pos - mDoigts[0].pos), 1.f);
				mPinceZoom = mScene.Camera().Zoom();
				mPinceMonde = mScene.Camera().EcranVersMonde((mDoigts[0].pos + mDoigts[1].pos) * 0.5f);
				return true;
			}
			if (p.phase == NkPointerPhase::NK_POINTER_UP || p.phase == NkPointerPhase::NK_POINTER_CANCEL) {
				if (k >= 0) {
					mDoigts[k].actif = false;
				}
				if (mPincement) {
					if (!mDoigts[0].actif && !mDoigts[1].actif) {
						mPincement = false;
					}
					return true;
				}
			}
			if (mPincement) {
				if (deux && p.phase == NkPointerPhase::NK_POINTER_MOVE) {
					NkVue2D &cam = mScene.Camera();
					const float32 d = math::NkMax(Longueur(mDoigts[1].pos - mDoigts[0].pos), 1.f);
					cam.PoserZoom(math::NkClamp(mPinceZoom * d / mPinceDistance, 8.f, 800.f));
					const NkVec2f ici = cam.EcranVersMonde((mDoigts[0].pos + mDoigts[1].pos) * 0.5f);
					cam.PoserCentre(cam.Centre() + (mPinceMonde - ici));
					mCameraTouchee = true;
				}
				return true;
			}
		}

		switch (p.phase) {
			case NkPointerPhase::NK_POINTER_DOWN: {
				mSouris = pos;
				if (const NkCible *c = CibleSous(pos)) {
					switch (c->genre) {
						case NkGenre::NK_COMMANDE:
							Commande(c->valeur);
							break;
						case NkGenre::NK_OUTIL:
							mOutil = static_cast<NkOutil>(c->valeur);
							break;
						case NkGenre::NK_ACTEUR:
							mActeur = static_cast<NkActeur>(c->valeur);
							mOutil = NkOutil::NK_POSER; // choisir un acteur, c'est vouloir le poser
							break;
						case NkGenre::NK_REGLAGE:
							Regler(c->valeur / 2, (c->valeur & 1) != 0);
							break;
					}
					return true;
				}
				// Un clic sur un panneau qui n'est pas un bouton ne traverse pas
				// jusqu'a la scene.
				if (!NkDansRect(mPlan.vue, pos) || (mPlan.details.w > 0.f && NkDansRect(mPlan.details, pos))) {
					return true;
				}
				GesteDebut(pos);
				return true;
			}
			case NkPointerPhase::NK_POINTER_MOVE:
				mSouris = pos;
				if (mGeste) {
					GesteVers(pos);
					return true;
				}
				return false;
			case NkPointerPhase::NK_POINTER_UP:
				if (mGeste) {
					GesteFin(pos);
					return true;
				}
				return false;
			case NkPointerPhase::NK_POINTER_CANCEL:
				AnnulerGeste();
				return true;
			default:
				return false;
		}
	}

	bool Physic2D::OnKeyPress(const NkKeyPressEvent &e) {
		switch (e.GetKey()) {
			case NkKey::NK_SPACE:
				Commande(CMD_JOUER);
				return true;
			case NkKey::NK_P:
				Commande(CMD_PAS);
				return true;
			case NkKey::NK_ESCAPE:
				if (mGeste) {
					AnnulerGeste();
				} else {
					Arreter();
				}
				return true;
			case NkKey::NK_TAB:
				Commande(CMD_NIV_SUIV);
				return true;
			case NkKey::NK_V:
				Commande(CMD_VUE);
				return true;
			case NkKey::NK_G:
				Commande(CMD_GRILLE);
				return true;
			case NkKey::NK_F:
				Commande(CMD_CADRER);
				return true;
			case NkKey::NK_R:
				Charger(mNiveau);
				return true;
			case NkKey::NK_S:
				Sauver();
				return true;
			case NkKey::NK_O:
				Ouvrir();
				return true;
			default:
				break;
		}
		// 1..9 : les outils, dans l'ordre de la palette.
		static const NkKey kChiffres[9] = {NkKey::NK_NUM1, NkKey::NK_NUM2, NkKey::NK_NUM3, NkKey::NK_NUM4, NkKey::NK_NUM5,
										   NkKey::NK_NUM6, NkKey::NK_NUM7, NkKey::NK_NUM8, NkKey::NK_NUM9};
		for (int32 i = 0; i < 9; ++i) {
			if (e.GetKey() == kChiffres[i]) {
				AnnulerGeste();
				mOutil = static_cast<NkOutil>(i);
				return true;
			}
		}
		return false;
	}

	// =========================================================================
	// Outils
	// =========================================================================
	void Physic2D::AnnulerGeste() {
		if (physics::NkParticules2D *p = mScene.Particules()) {
			p->SaisirFin();
		}
		mGeste = false;
		mPinceau = ecs::NkEntityId::Invalid();
		mRigideSaisi = ecs::NkEntityId::Invalid();
		mTraceN = 0;
	}

	ecs::NkEntityId Physic2D::RigideSous(const NkVec2f &m, bool decorAussi) {
		ecs::NkEntityId trouve = ecs::NkEntityId::Invalid();
		float32 meilleur = 0.15f; // tolerance : 15 cm autour de la forme
		mScene.Monde().Query<NkTransform2D, NkCollisionneur2D, NkCorps2D>().ForEach(
			[&](ecs::NkEntityId id, NkTransform2D &t, NkCollisionneur2D &c, NkCorps2D &corps) {
				const physic2d::NkDecor2D *d = mScene.Monde().Get<physic2d::NkDecor2D>(id);
				if (corps.type != NkTypeCorps::NK_DYNAMIQUE && !(decorAussi && d != nullptr && !d->sol)) {
					return; // le sol et les murs ne s'attrapent ni ne se gomment
				}
				const float32 dist = DistanceForme(t, c, m);
				if (dist < meilleur) {
					meilleur = dist;
					trouve = id;
				}
			});
		return trouve;
	}

	void Physic2D::Selectionner(const NkVec2f &m) {
		physics::NkParticules2D *p = mScene.Particules();
		if (p == nullptr) {
			return;
		}
		const int32 i = p->ParticuleProche(m, 0.35f);
		mSelection = i >= 0 ? p->corps[p->particules[static_cast<uint32>(i)].corps].id : 0u;
	}

	void Physic2D::GesteDebut(const NkVec2f &ecran) {
		physics::NkParticules2D *p = mScene.Particules();
		if (p == nullptr) {
			return;
		}
		const NkVec2f m = mScene.Camera().EcranVersMonde(ecran);
		mGeste = true;
		mGesteDebut = m;
		mGestePrec = m;
		mGesteEcranPrec = ecran;
		mTraceN = 0;

		switch (mOutil) {
			case NkOutil::NK_POSER: {
				const physic2d::NkInfoActeur &info = NkActeurInfo(mActeur);
				if (info.pinceau) {
					mPinceau = physic2d::NkOuvrirPinceau(mScene, mActeur);
				} else if (mActeur != NkActeur::NK_PONT) {
					// Le pont se TRACE : il nait au relache. Le reste nait au clic.
					const ecs::NkEntityId e = physic2d::NkPoserActeur(mScene, mActeur, m);
					mSons.JouerA(mSonPose, m, mScene.Camera(), 0.6f);
					if (const NkCorpsMou2D *mou = mScene.Monde().Get<NkCorpsMou2D>(e)) {
						mSelection = mou->corpsId;
					}
					mGeste = false;
				}
				break;
			}
			case NkOutil::NK_SAISIR:
				if (!p->SaisirDebut(m, kRayonSaisie)) {
					mRigideSaisi = RigideSous(m, false);
				} else {
					Selectionner(m);
				}
				break;
			case NkOutil::NK_COUTEAU:
				mTrace[mTraceN++] = m;
				break;
			case NkOutil::NK_EXPLOSION: {
				p->Explosion(m, kRayonExplosion, kVitesseExplosion);
				mSons.JouerA(mSonExplosion, m, mScene.Camera(), 1.f);
				// Les rigides aussi : le souffle ne choisit pas.
				mScene.Monde().Query<NkTransform2D, NkCorps2D>().ForEach(
					[&](ecs::NkEntityId id, NkTransform2D &t, NkCorps2D &c) {
						if (c.type != NkTypeCorps::NK_DYNAMIQUE) {
							return;
						}
						const NkVec2f d = t.position - m;
						const float32 l = Longueur(d);
						if (l >= kRayonExplosion) {
							return;
						}
						const NkVec2f dir = l > 1.0e-5f ? d / l : NkVec2f(0.f, 1.f);
						mScene.PoserVitesse(id, mScene.Vitesse(id) + dir * (kVitesseExplosion * (1.f - l / kRayonExplosion)));
					});
				for (NkEclair &e : mEclairs) {
					if (e.age > 0.5f) {
						e.pos = m;
						e.age = 0.f;
						break;
					}
				}
				mGeste = false;
				break;
			}
			case NkOutil::NK_EPINGLE: {
				const int32 i = p->ParticuleProche(m, 0.3f);
				if (i >= 0) {
					p->BasculerEpingle(static_cast<uint32>(i));
				}
				mGeste = false;
				break;
			}
			default:
				break; // aimant, gomme, obstacle, camera : tout se passe en route
		}
	}

	void Physic2D::GesteVers(const NkVec2f &ecran) {
		physics::NkParticules2D *p = mScene.Particules();
		const NkVec2f m = mScene.Camera().EcranVersMonde(ecran);
		switch (mOutil) {
			case NkOutil::NK_SAISIR:
				if (p != nullptr && !mRigideSaisi.IsValid()) {
					p->SaisirVers(m);
				}
				break;
			case NkOutil::NK_COUTEAU:
				if (p != nullptr && Longueur(m - mGestePrec) > 1.0e-3f) {
					if (p->Couper(mGestePrec, m) > 0u && mCoupeAge > 0.12f) {
						mSons.JouerA(mSonCoupe, m, mScene.Camera(), 0.5f);
						mCoupeAge = 0.f;
					}
					if (mTraceN == kTrace) {
						for (int32 i = 1; i < kTrace; ++i) {
							mTrace[i - 1] = mTrace[i];
						}
						--mTraceN;
					}
					mTrace[mTraceN++] = m;
					mTraceAge = 0.f;
				}
				break;
			case NkOutil::NK_CAMERA: {
				NkVue2D &cam = mScene.Camera();
				const float32 z = cam.Zoom();
				cam.PoserCentre(cam.Centre() +
								NkVec2f(-(ecran.x - mGesteEcranPrec.x) / z, (ecran.y - mGesteEcranPrec.y) / z));
				mCameraTouchee = true;
				break;
			}
			default:
				break;
		}
		mGestePrec = m;
		mGesteEcranPrec = ecran;
	}

	void Physic2D::GesteFin(const NkVec2f &ecran) {
		const NkVec2f m = mScene.Camera().EcranVersMonde(ecran);
		switch (mOutil) {
			case NkOutil::NK_POSER:
				if (mActeur == NkActeur::NK_PONT) {
					// Un clic sans glisser pose un pont de longueur par defaut.
					if (Longueur(m - mGesteDebut) < 0.6f) {
						physic2d::NkPoserActeur(mScene, NkActeur::NK_PONT, mGesteDebut);
					} else {
						physic2d::NkPoserPont(mScene, mGesteDebut, m);
					}
				}
				break;
			case NkOutil::NK_OBSTACLE:
				physic2d::NkPoserObstacle(mScene, mGesteDebut, m, kRayonObstacle);
				break;
			default:
				break;
		}
		if (physics::NkParticules2D *p = mScene.Particules()) {
			p->SaisirFin();
		}
		mGeste = false;
		mPinceau = ecs::NkEntityId::Invalid();
		mRigideSaisi = ecs::NkEntityId::Invalid();
		mTraceAge = 0.f;
	}

	void Physic2D::OutilsContinus(float32 dt) {
		if (!mGeste) {
			return;
		}
		physics::NkParticules2D *p = mScene.Particules();
		if (p == nullptr) {
			return;
		}
		const NkVec2f m = mScene.Camera().EcranVersMonde(mSouris);
		switch (mOutil) {
			case NkOutil::NK_POSER:
				if (mPinceau.IsValid() && mEtat == NkEtat::NK_JEU) {
					// Verser SEULEMENT quand le temps passe : en pause, la matiere
					// s'empilerait au meme endroit, en une boule sans pression.
					if (!physic2d::NkVerser(mScene, mPinceau, m, 3, mGraine)) {
						// Un autre corps a ete cree entre-temps : on rouvre.
						mPinceau = physic2d::NkOuvrirPinceau(mScene, mActeur);
					}
				}
				break;
			case NkOutil::NK_AIMANT:
				p->Aimant(m, kRayonAimant, kAccelAimant, dt);
				mScene.Monde().Query<NkTransform2D, NkCorps2D>().ForEach(
					[&](ecs::NkEntityId id, NkTransform2D &t, NkCorps2D &c) {
						if (c.type != NkTypeCorps::NK_DYNAMIQUE) {
							return;
						}
						const NkVec2f d = m - t.position;
						const float32 l = Longueur(d);
						if (l >= kRayonAimant || l < 1.0e-4f) {
							return;
						}
						mScene.PoserVitesse(id, mScene.Vitesse(id) + d / l * (kAccelAimant * (1.f - l / kRayonAimant) * dt));
					});
				break;
			case NkOutil::NK_GOMME: {
				p->Gommer(m, kRayonGomme);
				// Rigides et obstacles poses : UN par trame, sous le curseur.
				const ecs::NkEntityId e = RigideSous(m, true);
				if (e.IsValid()) {
					mScene.Detruire(e);
				}
				break;
			}
			case NkOutil::NK_SAISIR:
				if (mRigideSaisi.IsValid()) {
					if (!mScene.Monde().IsAlive(mRigideSaisi)) {
						mRigideSaisi = ecs::NkEntityId::Invalid();
						break;
					}
					// Un ressort critique vers le curseur : la vitesse qui rejoint
					// la cible en ~80 ms, bornee pour ne pas percer les murs.
					const NkTransform2D *t = mScene.Monde().Get<NkTransform2D>(mRigideSaisi);
					NkVec2f v = (m - t->position) * 12.f;
					const float32 l = Longueur(v);
					if (l > 25.f) {
						v = v * (25.f / l);
					}
					mScene.PoserVitesse(mRigideSaisi, v);
				}
				break;
			default:
				break;
		}
	}

} // namespace nkentseu
