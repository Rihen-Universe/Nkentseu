// =============================================================================
// Physic2D.cpp — cycle de vie, entrees, simulation, outils
//
// Le dessin des panneaux vit dans Physic2DPanneaux.cpp : ce fichier-ci
// DECIDE (quel outil, quel etat, quel pas), l'autre MONTRE.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Physic2D/Editeur/Physic2D.h"
#include "Physic2D/Physique/NkPhysBanc.h"

#include "NKEvent/NkMouseEvent.h"
#include "NKLogger/NkLog.h"

#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace nkentseu {

	using namespace physic2d;

	namespace {
		constexpr float32 kPasFixe = 1.f / 60.f;
		constexpr uint32 kTexPolice = 0x50324430u; // 'P2D0'
	} // namespace

	// =========================================================================
	// Construction
	// =========================================================================
	Physic2D::Physic2D() {
		Config().title = "Physic2D — Simulation physique 2D";
		Config().width = 1600;
		Config().height = 900;
		Config().clearColor = renderer::NkColor2D{10, 10, 10, 255};
		mJournal.Reserve(320);
	}

	NkOptional<int> Physic2D::OnCommandLine(const NkVector<NkString> &args) {
		for (uint32 i = 0; i < args.Size(); ++i) {
			if (args[i] == "--selftest") {
				return NkOptional<int>(NkPhysLancerBanc());
			}
			if (args[i].StartsWith("--niveau=")) {
				const NkString v = args[i].SubStr(9);
				mNiveau = v.Size() > 0 ? static_cast<int32>(v[0] - '0') : 0;
				mNiveau = mNiveau < 0 || mNiveau >= NK_NB_NIVEAUX ? 0 : mNiveau;
			}
		}
		return NkOptional<int>();
	}

	bool Physic2D::OnInit() {
		if (!mBackend.Init(Target().GetRenderer())) {
			logger.Error("[Physic2D] le dorsal NKGui -> NKCanvas n'a pas pu s'initialiser");
			return false;
		}
		mEchelle = NkClampf(Layout().density, 1.f, 3.f);
		ChargerPolices();
		const renderer::NkLayoutInfo &l = Layout();
		Disposer(static_cast<float32>(l.width), static_cast<float32>(l.height));
		Journal(Niveau::INFO, "LogInit", "Physic2D démarre sur NKCanvas + NKGui");
		Journal(Niveau::INFO, "LogInit", "Moteur : XPBD en sous-pas, fluides de Clavet, appariement de forme");
		ChargerNiveau(mNiveau);
		Recadrer();
		Jouer();
		Journal(Niveau::SUCCES, "LogAide", "Choisissez un acteur à gauche puis cliquez dans la vue. F1 : aide.");
		mPret = true;
		return true;
	}

	void Physic2D::OnLayout(const renderer::NkLayoutInfo &layout) {
		mEchelle = NkClampf(layout.density, 1.f, 3.f);
		if (mPret && std::fabs(mEchelle - mEchellePolices) > 0.01f) {
			ChargerPolices();
		}
		Disposer(static_cast<float32>(layout.width), static_cast<float32>(layout.height));
	}

	void Physic2D::OnPause() {
		// Une fenetre qui perd le focus ne recevra jamais le "relache" : sans
		// ceci, un outil resterait enfonce au retour.
		for (int32 i = 0; i < 3; ++i) {
			mEntree.bas[i] = false;
		}
		if (mOutilEnCours) {
			FinOutil(mDernierMonde);
			mOutilEnCours = false;
		}
		mPanoramique = false;
	}

	void Physic2D::ChargerPolices() {
		const float32 E = mEchelle;
		mTaillePolice = std::floor(13.f * E + 0.5f);
		mTailleGrande = std::floor(17.f * E + 0.5f);
		mTailleMono = std::floor(12.f * E + 0.5f);
		mPolice.texId = kTexPolice;
		mPoliceGrande.texId = kTexPolice + 1u;
		mPoliceMono.texId = kTexPolice + 2u;
		// Roboto est la police d'UE5. Les replis garantissent qu'on ECRIT
		// toujours quelque chose, meme sur une compilation sans elle.
		const bool ok = mPolice.LoadEmbedded(NkEmbeddedFontId::Roboto, mTaillePolice) ||
						mPolice.LoadEmbedded(NkEmbeddedFontId::Inter, mTaillePolice) ||
						mPolice.LoadEmbedded(NkEmbeddedFontId::DroidSans, mTaillePolice);
		if (!mPoliceGrande.LoadEmbedded(NkEmbeddedFontId::Roboto, mTailleGrande)) {
			mPoliceGrande.LoadEmbedded(NkEmbeddedFontId::DroidSans, mTailleGrande);
		}
		if (!mPoliceMono.LoadEmbedded(NkEmbeddedFontId::Cousine, mTailleMono)) {
			mPoliceMono.LoadEmbedded(NkEmbeddedFontId::DejaVuSansMono, mTailleMono);
		}
		nkgui::NkGuiFont *polices[3] = {&mPolice, &mPoliceGrande, &mPoliceMono};
		for (int32 i = 0; i < 3; ++i) {
			if (polices[i]->pixels != nullptr && polices[i]->atlasW > 0 && polices[i]->atlasH > 0) {
				mBackend.UploadFontGray8(polices[i]->TexId(), polices[i]->pixels, polices[i]->atlasW, polices[i]->atlasH);
			}
		}
		if (!ok) {
			logger.Warn("[Physic2D] aucune police chargee : l'interface sera sans texte");
		}
		mEchellePolices = mEchelle;
	}

	void Physic2D::Disposer(float32 w, float32 h) {
		const float32 E = mEchelle;
		const float32 g = 2.f * E;
		Disposition &d = mDisp;
		d.menu = NkRect{0.f, 0.f, w, 28.f * E};
		d.outils = NkRect{0.f, d.menu.h, w, 44.f * E};
		const float32 haut = d.menu.h + d.outils.h + g;
		const float32 basH = mMontrerBas ? NkMinf(200.f * E, h * 0.28f) : 0.f;
		float32 gw = mMontrerGauche ? 256.f * E : 0.f;
		float32 dw = mMontrerDroite ? 336.f * E : 0.f;
		// Sur un petit ecran, la VUE passe avant les panneaux.
		if (w - gw - dw < 380.f * E) {
			gw = 0.f;
		}
		if (w - gw - dw < 380.f * E) {
			dw = 0.f;
		}
		const float32 mainH = h - haut - (basH > 0.f ? basH + g : 0.f) - g;
		d.gauche = NkRect{g, haut, gw > 0.f ? gw : 0.f, mainH};
		d.droite = NkRect{w - dw - g, haut, dw, mainH};
		d.outliner = NkRect{d.droite.x, haut, dw, std::floor(mainH * 0.36f)};
		d.details = NkRect{d.droite.x, haut + d.outliner.h + g, dw, mainH - d.outliner.h - g};
		const float32 vx = gw > 0.f ? d.gauche.x + gw + g : g;
		const float32 vw = (dw > 0.f ? d.droite.x - g : w - g) - vx;
		d.vue = NkRect{vx, haut, vw, mainH};
		d.bas = NkRect{g, h - basH - g, w - 2.f * g, basH};
		mCam.vue = d.vue;
	}

	void Physic2D::Journal(Niveau n, const char *categorie, const char *fmt, ...) {
		LigneJournal l;
		std::snprintf(l.categorie, sizeof(l.categorie), "%s", categorie);
		va_list args;
		va_start(args, fmt);
		std::vsnprintf(l.texte, sizeof(l.texte), fmt, args);
		va_end(args);
		l.niveau = n;
		l.temps = mTemps;
		if (mJournal.Size() >= 300) {
			mJournal.Erase(mJournal.Begin());
		}
		mJournal.PushBack(l);
		mDefilJournal = 0.f; // on suit la fin, comme la console d'UE
	}

	// =========================================================================
	// Simulation : Jouer / Pause / Pas / Arreter
	// =========================================================================
	void Physic2D::Jouer() {
		if (mEtat == Etat::EDITION) {
			mPhoto = mMonde;
			mPhotoValide = true;
			Journal(Niveau::SUCCES, "LogPIE", "Début de la simulation (Arrêter restaurera le niveau)");
		} else if (mEtat == Etat::PAUSE) {
			Journal(Niveau::INFO, "LogPIE", "Reprise");
		}
		mEtat = Etat::JEU;
		mAccumulateur = 0.f;
	}

	void Physic2D::MettreEnPause() {
		if (mEtat == Etat::JEU) {
			mEtat = Etat::PAUSE;
			Journal(Niveau::INFO, "LogPIE", "Pause (N : avancer d'un pas)");
		}
	}

	void Physic2D::Arreter() {
		if (mEtat == Etat::EDITION) {
			return;
		}
		mMonde.SaisirFin();
		if (mPhotoValide) {
			mMonde = mPhoto;
			mPhotoValide = false;
		}
		mEtat = Etat::EDITION;
		mRupturesVues = mMonde.rupturesTotal;
		if (mMonde.IndexCorps(mSelection) < 0) {
			mSelection = 0;
		}
		Journal(Niveau::AVERT, "LogPIE", "Fin de la simulation : niveau restauré tel qu'avant Jouer");
	}

	void Physic2D::UnPas() {
		if (mEtat == Etat::EDITION) {
			mPhoto = mMonde;
			mPhotoValide = true;
		}
		mEtat = Etat::PAUSE;
		const auto t0 = std::chrono::steady_clock::now();
		mMonde.Pas(kPasFixe);
		mMsPhysique = static_cast<float32>(
			std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
	}

	void Physic2D::ChargerNiveau(int32 i) {
		mMonde.SaisirFin();
		NkChargerNiveau(mMonde, i);
		mNiveau = i;
		mSelection = 0;
		mRupturesVues = 0;
		mOutilEnCours = false;
		mCorpsPinceau = -1;
		if (mEtat != Etat::EDITION) {
			mPhoto = mMonde;
			mPhotoValide = true;
		} else {
			mPhotoValide = false;
		}
		Journal(Niveau::SUCCES, "LogNiveau", "Niveau \"%s\" chargé : %u corps, %u particules", NkNiveauNom(i),
				static_cast<unsigned>(mMonde.corps.Size()), static_cast<unsigned>(mMonde.particules.Size()));
	}

	void Physic2D::Simuler(float32 dt) {
		float32 ms = 0.f;
		if (mEtat == Etat::JEU) {
			mAccumulateur += dt;
			int32 n = 0;
			const auto t0 = std::chrono::steady_clock::now();
			while (mAccumulateur >= kPasFixe && n < 3) {
				mMonde.Pas(kPasFixe);
				mAccumulateur -= kPasFixe;
				++n;
			}
			// Trop lent pour tenir le temps reel : on RALENTIT plutot que
			// d'empiler un retard qui figerait l'application.
			if (n == 3) {
				mAccumulateur = 0.f;
			}
			ms = static_cast<float32>(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
			mMsPhysique = mMsPhysique * 0.9f + ms * 0.1f;
		}

		// Les ruptures sont annoncees par paquets, une fois par seconde.
		mAttenteRuptures -= dt;
		if (mAttenteRuptures <= 0.f) {
			mAttenteRuptures = 1.f;
			if (mMonde.rupturesTotal > mRupturesVues) {
				const uint32 n = mMonde.rupturesTotal - mRupturesVues;
				Journal(n > 50 ? Niveau::AVERT : Niveau::INFO, "LogPhysique", "%u liaison%s rompue%s", n, n > 1 ? "s" : "",
						n > 1 ? "s" : "");
			}
			mRupturesVues = mMonde.rupturesTotal;
		}

		mHistPhys[mHistIndex] = mEtat == Etat::JEU ? ms : 0.f;
		mHistImage[mHistIndex] = dt * 1000.f;
		mHistEnergie[mHistIndex] = mMonde.stats.energieCinetique;
		mHistIndex = (mHistIndex + 1) % kHist;
	}

	// =========================================================================
	// Entrees
	// =========================================================================
	bool Physic2D::OnEvent(const NkEvent &event) {
		if (const auto *e = event.As<NkMouseMoveEvent>()) {
			mEntree.x = static_cast<float32>(e->GetX());
			mEntree.y = static_cast<float32>(e->GetY());
			return true;
		}
		if (const auto *e = event.As<NkMouseButtonPressEvent>()) {
			const int32 b = e->GetButton() == NkMouseButton::NK_MB_LEFT	   ? 0
							: e->GetButton() == NkMouseButton::NK_MB_RIGHT ? 1
							: e->GetButton() == NkMouseButton::NK_MB_MIDDLE ? 2
																			: -1;
			mEntree.x = static_cast<float32>(e->GetX());
			mEntree.y = static_cast<float32>(e->GetY());
			if (b >= 0) {
				mEntree.bas[b] = true;
				mEntree.presse[b] = true;
			}
			return true;
		}
		if (const auto *e = event.As<NkMouseButtonReleaseEvent>()) {
			const int32 b = e->GetButton() == NkMouseButton::NK_MB_LEFT	   ? 0
							: e->GetButton() == NkMouseButton::NK_MB_RIGHT ? 1
							: e->GetButton() == NkMouseButton::NK_MB_MIDDLE ? 2
																			: -1;
			mEntree.x = static_cast<float32>(e->GetX());
			mEntree.y = static_cast<float32>(e->GetY());
			if (b >= 0) {
				mEntree.bas[b] = false;
				mEntree.relache[b] = true;
			}
			return true;
		}
		if (const auto *e = event.As<NkMouseWheelVerticalEvent>()) {
			mEntree.molette += static_cast<float32>(e->GetDeltaY());
			return true;
		}
		if (const auto *e = event.As<NkKeyPressEvent>()) {
			mEntree.ctrl = e->GetModifiers().ctrl;
			mEntree.shift = e->GetModifiers().shift || e->GetKey() == NkKey::NK_LSHIFT || e->GetKey() == NkKey::NK_RSHIFT;
			return false; // la coquille en fera un OnKeyPress
		}
		if (const auto *e = event.As<NkKeyReleaseEvent>()) {
			mEntree.ctrl = e->GetModifiers().ctrl;
			mEntree.shift = e->GetModifiers().shift;
			if (e->GetKey() == NkKey::NK_LSHIFT || e->GetKey() == NkKey::NK_RSHIFT) {
				mEntree.shift = false;
			}
			if (e->GetKey() == NkKey::NK_LCTRL || e->GetKey() == NkKey::NK_RCTRL) {
				mEntree.ctrl = false;
			}
			return false;
		}
		return false;
	}

	bool Physic2D::OnPointer(const NkPointer &p) {
		if (!p.fromTouch) {
			return false; // la souris est deja lue par OnEvent
		}
		mEntree.x = p.x;
		mEntree.y = p.y;
		switch (p.phase) {
			case NkPointerPhase::NK_POINTER_DOWN:
				mEntree.bas[0] = true;
				mEntree.presse[0] = true;
				break;
			case NkPointerPhase::NK_POINTER_UP:
			case NkPointerPhase::NK_POINTER_CANCEL:
				mEntree.bas[0] = false;
				mEntree.relache[0] = true;
				break;
			default:
				break;
		}
		return true;
	}

	bool Physic2D::OnKeyPress(const NkKeyPressEvent &e) {
		const NkKey k = e.GetKey();
		switch (k) {
			case NkKey::NK_Q:
				ChoisirOutil(Outil::SELECTION);
				return true;
			case NkKey::NK_W:
				ChoisirOutil(Outil::SAISIR);
				return true;
			case NkKey::NK_E:
				ChoisirOutil(Outil::COUPER);
				return true;
			case NkKey::NK_R:
				ChoisirOutil(Outil::EXPLOSION);
				return true;
			case NkKey::NK_T:
				ChoisirOutil(Outil::AIMANT);
				return true;
			case NkKey::NK_Y:
				ChoisirOutil(Outil::OBSTACLE);
				return true;
			case NkKey::NK_G:
				ChoisirOutil(Outil::GOMME);
				return true;
			case NkKey::NK_P:
				ChoisirOutil(Outil::EPINGLE);
				return true;
			case NkKey::NK_NUM1:
			case NkKey::NK_NUM2:
			case NkKey::NK_NUM3:
			case NkKey::NK_NUM4:
			case NkKey::NK_NUM5:
			case NkKey::NK_NUM6:
			case NkKey::NK_NUM7:
			case NkKey::NK_NUM8:
			case NkKey::NK_NUM9:
				ChoisirActeur(static_cast<NkActeur>(static_cast<int32>(k) - static_cast<int32>(NkKey::NK_NUM1)));
				return true;
			case NkKey::NK_NUM0:
				ChoisirActeur(NkActeur::NK_A_ATOMES_DISQUE);
				return true;
			case NkKey::NK_SPACE:
				if (mEtat == Etat::JEU) {
					MettreEnPause();
				} else {
					Jouer();
				}
				return true;
			case NkKey::NK_N:
				UnPas();
				return true;
			case NkKey::NK_F5:
				Arreter();
				return true;
			case NkKey::NK_DELETE:
			case NkKey::NK_BACK:
				SupprimerSelection();
				return true;
			case NkKey::NK_F:
				Focaliser();
				return true;
			case NkKey::NK_H:
			case NkKey::NK_HOME:
				Recadrer();
				return true;
			case NkKey::NK_V:
				mOptions.mode = static_cast<NkModeVue>((static_cast<int32>(mOptions.mode) + 1) % static_cast<int32>(NkModeVue::NK_COUNT));
				return true;
			case NkKey::NK_F1:
				mMontrerBas = true;
				mOngletBas = 2;
				Disposer(static_cast<float32>(Layout().width), static_cast<float32>(Layout().height));
				return true;
			case NkKey::NK_ESCAPE:
				if (mPopup != Popup::AUCUN) {
					mPopup = Popup::AUCUN;
					return true;
				}
				if (mOutil == Outil::PLACER) {
					ChoisirOutil(Outil::SELECTION);
					return true;
				}
				if (mSelection != 0) {
					mSelection = 0;
					return true;
				}
				return false; // Android : le retour non reclame quitte
			default:
				return false;
		}
	}

	// =========================================================================
	// Trame
	// =========================================================================
	void Physic2D::OnUpdate(float32 dt) {
		mTemps += dt;
		mOptions.temps = mTemps;
		if (dt > 0.f) {
			mFps = mFps * 0.95f + (1.f / dt) * 0.05f;
		}
		mPopupAuDebut = mPopup != Popup::AUCUN;
		if (mPret && mDisp.vue.w > 0.f) {
			InteragirVue(dt);
		}
		Simuler(dt);
		mFlashExplosion = NkMaxf(0.f, mFlashExplosion - dt * 3.f);
	}

	void Physic2D::OnRender(renderer::NkRenderWindow &target) {
		const math::NkVec2u taille = target.GetSize();
		const float32 w = static_cast<float32>(taille.x);
		const float32 h = static_cast<float32>(taille.y);
		if (w < 2.f || h < 2.f) {
			return;
		}
		if (w != mDisp.menu.w) {
			Disposer(w, h);
		}
		mDl.Reset();
		mUi.Debut(&mDl, &mPolice, mTaillePolice, &mPoliceGrande, mTailleGrande, &mPoliceMono, mTailleMono, &mEntree,
				  mEchelle, NkMaxf(0.001f, 1.f / NkMaxf(mFps, 1.f)));
		if (mPopup != Popup::AUCUN) {
			mUi.DefinirModale(RectPopup());
		}
		mDl.AddRectFilled(NkRect{0.f, 0.f, w, h}, ue5::kFond);

		DessinerVue();
		DessinerMenu();
		DessinerBarreOutils();
		if (mDisp.gauche.w > 0.f) {
			DessinerPlacerActeurs();
		}
		if (mDisp.droite.w > 0.f) {
			DessinerOutliner();
			DessinerDetails();
		}
		if (mDisp.bas.h > 0.f) {
			DessinerBas();
		}
		DessinerPopup();
		mUi.Fin();

		mBackend.Submit(mDl, taille.x, taille.y);
		mEntree.FinDeTrame();
	}

	// =========================================================================
	// La vue : camera et outils
	// =========================================================================
	bool Physic2D::SourisDansVue() const {
		if (!mEntree.Dans(mDisp.vue)) {
			return false;
		}
		// La barre de vue (en haut a gauche) et l'indicateur (en haut a droite)
		// sont des boutons : un clic sur eux n'est pas un clic dans le monde.
		const float32 E = mEchelle;
		const NkRect barre{mDisp.vue.x, mDisp.vue.y, 420.f * E, 34.f * E};
		return !mEntree.Dans(barre);
	}

	void Physic2D::InteragirVue(float32 dt) {
		const bool libre = !mPopupAuDebut && mUi.Actif() == 0;
		const bool dansVue = libre && SourisDansVue();
		const NkV2 w = mCam.Monde(mEntree.x, mEntree.y);
		if (dt > 0.f) {
			const NkV2 v = (w - mDernierMonde) / dt;
			mVitesseSouris = mVitesseSouris * 0.6f + v * 0.4f;
		}

		// Zoom a la molette, centre sur le curseur.
		if (dansVue && mEntree.molette != 0.f) {
			const NkV2 avant = mCam.Monde(mEntree.x, mEntree.y);
			mCam.zoom = NkClampf(mCam.zoom * std::pow(1.18f, mEntree.molette), 0.05f, 8.f);
			const NkV2 apres = mCam.Monde(mEntree.x, mEntree.y);
			mCam.centre += avant - apres;
		}

		// Panoramique : bouton droit ou milieu (comme la vue orthographique d'UE).
		if (dansVue && (mEntree.presse[1] || mEntree.presse[2])) {
			mPanoramique = true;
			mPanoX = mEntree.x;
			mPanoY = mEntree.y;
		}
		if (mPanoramique) {
			if (mEntree.bas[1] || mEntree.bas[2]) {
				mCam.centre.x -= (mEntree.x - mPanoX) / mCam.zoom;
				mCam.centre.y += (mEntree.y - mPanoY) / mCam.zoom;
				mPanoX = mEntree.x;
				mPanoY = mEntree.y;
			} else {
				mPanoramique = false;
			}
		}

		const NkV2 wApresCam = mCam.Monde(mEntree.x, mEntree.y);
		if (dansVue && mEntree.presse[0] && !mOutilEnCours) {
			mOutilEnCours = true;
			mDernierMonde = wApresCam;
			DebutOutil(wApresCam);
		}
		if (mOutilEnCours && mEntree.bas[0]) {
			ContinuerOutil(wApresCam, dt);
		}
		if (mOutilEnCours && !mEntree.bas[0]) {
			ContinuerOutil(wApresCam, dt);
			FinOutil(wApresCam);
			mOutilEnCours = false;
		}
		mDernierMonde = wApresCam;
	}

	void Physic2D::DebutOutil(const NkV2 &w) {
		const float32 px = 1.f / mCam.zoom; // un pixel, en centimetres
		mDepartOutil = w;
		mNbTraine = 0;
		switch (mOutil) {
			case Outil::SELECTION: {
				const int32 k = mMonde.ParticuleProche(w, 14.f * px);
				Selectionner(k >= 0 ? mMonde.corps[mMonde.particules[static_cast<uint32>(k)].corps].id : 0u);
				break;
			}
			case Outil::SAISIR:
				mMonde.SaisirDebut(w, NkMaxf(8.f, 22.f * px));
				break;
			case Outil::COUPER:
				mTraineCouteau[0] = w;
				mNbTraine = 1;
				break;
			case Outil::EXPLOSION:
				mMonde.Explosion(w, 220.f, 2600.f);
				mFlashExplosion = 1.f;
				mPosExplosion = w;
				Journal(Niveau::INFO, "LogPhysique", "Explosion en (%.0f, %.0f)", static_cast<double>(w.x), static_cast<double>(w.y));
				break;
			case Outil::GOMME:
				mMonde.Gommer(w, 22.f * px);
				break;
			case Outil::EPINGLE: {
				const int32 k = mMonde.ParticuleProche(w, 18.f * px);
				if (k >= 0) {
					mMonde.BasculerEpingle(static_cast<uint32>(k));
					Journal(Niveau::INFO, "LogPhysique", "Particule %d %s", k,
							mMonde.particules[static_cast<uint32>(k)].epingle ? "épinglée" : "libérée");
				}
				break;
			}
			case Outil::PLACER: {
				const NkInfoActeur &info = NkActeurInfo(mActeur);
				if (mActeur == NkActeur::NK_A_PONT) {
					break; // le pont se trace : on attend le relache
				}
				if (info.pinceau) {
					mCorpsPinceau = NkOuvrirPinceau(mMonde, mActeur);
					if (mCorpsPinceau >= 0) {
						NkVerser(mMonde, static_cast<uint32>(mCorpsPinceau), w, 20.f, 6, mAlea);
					}
					break;
				}
				const int32 c = NkCreerActeur(mMonde, mActeur, w);
				if (c < 0) {
					break;
				}
				{
					Selectionner(mMonde.corps[static_cast<uint32>(c)].id);
					Journal(Niveau::INFO, "LogActeur", "%s placé (%u particules)", mMonde.corps[static_cast<uint32>(c)].nom,
							static_cast<unsigned>(mMonde.corps[static_cast<uint32>(c)].nombre));
				}
				break;
			}
			default:
				break;
		}
	}

	void Physic2D::ContinuerOutil(const NkV2 &w, float32 dt) {
		const float32 px = 1.f / mCam.zoom;
		switch (mOutil) {
			case Outil::SELECTION: {
				const int32 ci = mMonde.IndexCorps(mSelection);
				if (ci >= 0) {
					const NkV2 d = w - mDernierMonde;
					if (NkLen2(d) > 0.f) {
						mMonde.Translater(static_cast<uint32>(ci), d);
						mMonde.AppliquerVitesse(static_cast<uint32>(ci), NkV2());
					}
				}
				break;
			}
			case Outil::SAISIR:
				mMonde.SaisirVers(w);
				break;
			case Outil::COUPER: {
				mMonde.Couper(mDernierMonde, w);
				if (mNbTraine < 24) {
					mTraineCouteau[mNbTraine++] = w;
				} else {
					for (int32 i = 1; i < 24; ++i) {
						mTraineCouteau[i - 1] = mTraineCouteau[i];
					}
					mTraineCouteau[23] = w;
				}
				break;
			}
			case Outil::AIMANT:
				mMonde.Aimant(w, 260.f, mEntree.shift ? -5200.f : 5200.f, dt * mMonde.reglages.echelleTemps);
				break;
			case Outil::GOMME:
				mMonde.Gommer(w, 22.f * px);
				break;
			case Outil::PLACER:
				if (mCorpsPinceau >= 0) {
					// Le pinceau prolonge SON corps tant qu'il est le dernier ; si un
					// autre s'est intercale, on en ouvre un nouveau (contiguite).
					const uint32 c = static_cast<uint32>(mCorpsPinceau);
					const bool valide = c + 1u == mMonde.corps.Size() &&
										mMonde.corps[c].debut + mMonde.corps[c].nombre == mMonde.particules.Size();
					if (!valide) {
						mCorpsPinceau = NkOuvrirPinceau(mMonde, mActeur);
					}
					if (mCorpsPinceau >= 0) {
						NkVerser(mMonde, static_cast<uint32>(mCorpsPinceau), w, NkMaxf(14.f, 20.f), 3, mAlea);
					}
				}
				break;
			default:
				break;
		}
	}

	void Physic2D::FinOutil(const NkV2 &w) {
		switch (mOutil) {
			case Outil::SELECTION: {
				const int32 ci = mMonde.IndexCorps(mSelection);
				if (ci >= 0 && NkLen(w - mDepartOutil) > 2.f) {
					// On LANCE ce qu'on deplace : la vitesse de la souris passe au corps.
					NkV2 v = mVitesseSouris * 0.7f;
					const float32 l = NkLen(v);
					if (l > 2500.f) {
						v *= 2500.f / l;
					}
					mMonde.AppliquerVitesse(static_cast<uint32>(ci), v);
				}
				break;
			}
			case Outil::SAISIR:
				mMonde.SaisirFin();
				break;
			case Outil::COUPER:
				mNbTraine = 0;
				break;
			case Outil::OBSTACLE: {
				NkObstacle o;
				o.a = mDepartOutil;
				o.b = w;
				o.rayon = 10.f;
				if (NkLen(w - mDepartOutil) < 8.f) {
					o.b = o.a;
					o.rayon = 24.f;
				}
				mMonde.obstacles.PushBack(o);
				Journal(Niveau::INFO, "LogActeur", "Obstacle statique ajouté (%.0f cm)", static_cast<double>(NkLen(o.b - o.a)));
				break;
			}
			case Outil::PLACER:
				if (mActeur == NkActeur::NK_A_PONT) {
					const int32 c = NkLen(w - mDepartOutil) > 60.f ? NkCreerPont(mMonde, mDepartOutil, w)
																   : NkCreerActeur(mMonde, NkActeur::NK_A_PONT, w);
					if (c >= 0) {
						Selectionner(mMonde.corps[static_cast<uint32>(c)].id);
						Journal(Niveau::INFO, "LogActeur", "%s placé (%u planches)", mMonde.corps[static_cast<uint32>(c)].nom,
								static_cast<unsigned>(mMonde.corps[static_cast<uint32>(c)].nombre));
					}
				} else if (mCorpsPinceau >= 0 && mCorpsPinceau < static_cast<int32>(mMonde.corps.Size())) {
					const NkCorps &c = mMonde.corps[static_cast<uint32>(mCorpsPinceau)];
					Journal(Niveau::INFO, "LogActeur", "%s versé (%u particules)", c.nom, static_cast<unsigned>(c.nombre));
				}
				mCorpsPinceau = -1;
				break;
			default:
				break;
		}
	}

	void Physic2D::ChoisirOutil(Outil o) {
		if (mOutilEnCours) {
			FinOutil(mDernierMonde);
			mOutilEnCours = false;
		}
		mOutil = o;
	}

	void Physic2D::ChoisirActeur(NkActeur a) {
		if (static_cast<int32>(a) >= static_cast<int32>(NkActeur::NK_A_COUNT)) {
			return;
		}
		ChoisirOutil(Outil::PLACER);
		mActeur = a;
	}

	void Physic2D::Selectionner(uint32 id) {
		mSelection = id;
	}

	void Physic2D::SupprimerSelection() {
		const int32 ci = mMonde.IndexCorps(mSelection);
		if (ci < 0) {
			return;
		}
		char nom[40];
		std::snprintf(nom, sizeof(nom), "%s", mMonde.corps[static_cast<uint32>(ci)].nom);
		mMonde.SupprimerCorps(static_cast<uint32>(ci));
		mSelection = 0;
		Journal(Niveau::AVERT, "LogActeur", "%s supprimé", nom);
	}

	void Physic2D::Focaliser() {
		const int32 ci = mMonde.IndexCorps(mSelection);
		if (ci < 0) {
			Recadrer();
			return;
		}
		NkV2 mn, mx;
		mMonde.BoiteCorps(static_cast<uint32>(ci), mn, mx);
		mCam.centre = (mn + mx) * 0.5f;
		const float32 bw = NkMaxf(mx.x - mn.x, 40.f);
		const float32 bh = NkMaxf(mx.y - mn.y, 40.f);
		mCam.zoom = NkClampf(NkMinf(mCam.vue.w / (bw * 3.f), mCam.vue.h / (bh * 3.f)), 0.05f, 8.f);
	}

	void Physic2D::Recadrer() {
		const float32 L = mMonde.reglages.largeur;
		const float32 H = mMonde.reglages.hauteur;
		mCam.centre = NkV2(0.f, H * 0.5f - 20.f);
		mCam.zoom = NkClampf(NkMinf(mCam.vue.w / (L * 1.04f), mCam.vue.h / (H * 1.1f)), 0.05f, 8.f);
	}

} // namespace nkentseu
