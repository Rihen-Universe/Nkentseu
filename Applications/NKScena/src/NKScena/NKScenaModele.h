#pragma once
// -----------------------------------------------------------------------------
// @File    NKScenaModele.h
// @Brief   LE MODELE de NKScena : la scene (empruntee a Nogee, NogeeModele), LA
//          FRISE partagee ou l'on pose pistes, cles et plans camera, la SEQUENCE
//          de Noge qu'elle devient (ce qui joue et s'enregistre en .nkseq), le
//          temps, la camera de plan -- et TOUTES les actions : aucune n'est
//          ecrite dans un dessin de panneau (la regle d'UnkenyEditor,
//          NkEditeurActions.h), le banc (`--selftest`) les eprouve sans fenetre.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// CE QUE NKSCENA N'EDITE PAS (ROADMAP.md §2) : ni maillage, ni squelette, ni
// la scene elle-meme -- elle l'OUVRE (.nkscene3d, ou l'ancien .nkscene de
// Nogee et NogeDemo), pose le temps dessus, et n'enregistre que la SEQUENCE.
// La seule scene qu'elle ecrit est celle de demonstration (la scene de
// NogeDemo, posee par NogeeModele::NouvelleScene), pour que la sequence ait un
// fichier vers lequel pointer.
//
// LE TEMPS : la frise porte le curseur, la lecture, la boucle et la plage
// (NkTimelineModel::Advance). A chaque image, l'application appelle
// `Synchroniser` (la sequence suit la frise) puis `Evaluer(curseur)` quand
// quelque chose a change : les transformations des entites sont ecrites par
// NkSequence::Evaluate, la camera du plan actif prend la main si la vue
// regarde « par la camera du plan ».
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKEditorKit/Components/NkTimelineModel.h"
#include "Nogee/Editeur/NogeeModele.h"

namespace nkentseu {

	class NkSequence;
	class NkPhysicsSystem;

	namespace nkscena {

		/// Ce que les Details montrent (la derniere chose choisie).
		enum class NkScenaChoix : uint8 { Rien = 0, Entite, Piste, Cle, Plan, Sequence };

		/// Les reglages de la sortie (NkRenderOutput de la sequence).
		struct NkScenaSortie {
				uint32 largeur = 960;
				uint32 hauteur = 540;
				NkString dossier = "Build/NKScena/Rendu";
				NkString prefixe = "image";
		};

		class NkScenaModele {
			public:
				/// La scene de demonstration, ecrite au premier lancement.
				static constexpr const char *kSceneDemo = "Build/NKScena/nogedemo.nkscene3d";
				static constexpr const char *kSequenceDefaut = "Build/NKScena/sequence.nkseq";

				NkScenaModele();
				~NkScenaModele();
				NkScenaModele(const NkScenaModele &) = delete;
				NkScenaModele &operator=(const NkScenaModele &) = delete;

				// ── La scene (le modele de Nogee, emprunte) ───────────────────
				nogee::NogeeModele scene;
				void Brancher(ecs::NkWorld *monde, NkPhysicsSystem *physique) noexcept;
				bool Pret() const noexcept {
					return scene.Pret();
				}
				/// La scene de NogeDemo (sol, joueur, balle, camera, soleil), ecrite
				/// dans kSceneDemo pour que la sequence puisse la nommer.
				bool SceneDemo();
				/// Ouvre une scene Noge (JSON de NkSceneSerializer) ; les pistes se
				/// RELIENT a ses entites par leur nom.
				bool OuvrirScene(const char *fichier);
				const NkString &CheminScene() const noexcept {
					return scene.chemin;
				}
				/// L'entite de la scene qui porte ce nom (Invalid sinon).
				ecs::NkEntityId Entite(const char *nom);

				// ── La sequence ───────────────────────────────────────────────
				editorkit::NkTimelineModel frise;
				NkString nom = "Séquence 1";
				NkString chemin = kSequenceDefaut;
				bool modifie = false;
				NkScenaSortie sortie;
				/// Une sequence vide (la frise vide, `duree` secondes a `ips`).
				void NouvelleSequence(float32 duree = 5.f, float32 ips = 24.f);
				/// Enregistre la sequence (.nkseq, format v2 : elle POINTE vers sa scene).
				bool Enregistrer(const char *fichier = nullptr);
				/// Ouvre une sequence, PUIS la scene qu'elle nomme, et relie ses pistes.
				bool Ouvrir(const char *fichier);
				/// La sequence de Noge telle qu'elle joue (refaite depuis la frise).
				const NkSequence &Sequence() const noexcept {
					return *mSeq;
				}
				/// Combien de pistes ou de plans nomment une entite ABSENTE de la scene.
				uint32 CiblesPerdues() const noexcept {
					return mPerdues;
				}

				// ── Les pistes et les cles ────────────────────────────────────
				/// Les trois pistes d'une entite (Position, Rotation, Échelle). Faux si
				/// l'entite n'existe pas ou a deja ses pistes.
				bool AjouterPisteTransform(const char *entite);
				bool APisteTransform(const char *entite) const;
				/// La piste des plans camera (une seule). Rend son identite.
				nk_uint64 AjouterPistePlans();
				nk_uint64 PistePlans() const;
				/// Un plan : la camera `camera` filme de `debut` a `debut + duree`.
				nk_uint64 AjouterPlan(const char *camera, float32 debut, float32 duree);
				/// Une cle a `t` sur les trois pistes de l'entite, de sa pose VIVANTE.
				bool PoserCles(const char *entite, float32 t);
				/// Une cle a `t` sur une piste, de la valeur vivante de sa propriete.
				bool PoserCle(nk_uint64 piste, float32 t);
				/// La valeur VIVANTE de la propriete d'une piste (le crochet `readLive`).
				bool LireVivant(const editorkit::NkTimelineTrack &piste, float32 sortie[4]);
				/// Les cameras de la scene (les noms proposes pour un plan).
				void Cameras(NkVector<NkString> &noms);
				uint32 NombreCles() const noexcept;

				// ── Le temps ──────────────────────────────────────────────────
				/// La sequence suit la frise (refaite si la frise a change, ou si la
				/// scene a ete rechargee). Rend vrai si elle a ete refaite.
				bool Synchroniser();
				/// Pose la scene a l'instant `t` (les pistes, puis la camera du plan).
				void Evaluer(float32 t);
				/// Le pas d'une image : lecture, synchronisation, evaluation si quelque
				/// chose a change. Rend vrai si la scene a ete posee.
				bool Image(float32 dt);
				/// La vue regarde PAR LA CAMERA DU PLAN actif (sinon la vue libre).
				bool vueCamera = false;
				/// La camera du plan actif a `t` (Invalid s'il n'y en a pas).
				ecs::NkEntityId CameraDuPlan(float32 t) const;
				/// Le nom du plan actif a `t` (vide s'il n'y en a pas).
				NkString PlanA(float32 t) const;

				// ── Ce que les Details montrent ───────────────────────────────
				NkScenaChoix choix = NkScenaChoix::Rien;

				/// La sequence de DEMONSTRATION (celle des captures et du banc) : la
				/// scene de NogeDemo, le joueur qui traverse, la camera qui avance,
				/// un plan camera de bout en bout. `duree` secondes.
				bool Exemple(float32 duree = 4.f);

				/// Annonce au journal (le canal de la scene, partage).
				void Annoncer(const char *texte, uint8 niveau = 0) {
					scene.Annoncer(texte, niveau);
				}

			private:
				nk_uint64 NouvelId() const noexcept;
				void Derouler(const editorkit::NkTimelineTrack &piste, float32 t, float32 v[4]) const;
				void AppliquerCameras(float32 t);

				NkSequence *mSeq = nullptr;	  ///< ce qui joue (refait depuis la frise)
				NkSequence *mReste = nullptr; ///< ce que la frise ne montre pas, et la sortie d'origine
				uint32 mRevision = ~0u;
				uint32 mPerdues = 0;
				float32 mDernierTemps = -1.f;
				ecs::NkEntityId mCameraForcee = ecs::NkEntityId::Invalid();
				int32 mPrioriteAvant = 0;
				bool mVueCameraAvant = false;
		};

	} // namespace nkscena
} // namespace nkentseu
