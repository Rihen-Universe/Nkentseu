#pragma once
// -----------------------------------------------------------------------------
// @File    NogeeModele.h
// @Brief   LE MODELE de l'editeur Nogee : la scene du moteur (le monde ECS de
//          NkEngineLayer), la selection, l'etat de jeu, la camera d'edition,
//          les drapeaux d'editeur (oeil, cadenas), l'historique, le fichier, et
//          TOUTES les actions -- aucune n'est ecrite dans un dessin de panneau
//          (la regle d'UnkenyEditor, NkEditeurActions.h).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// CE QUE NOGEE SAIT POSER (« Placer des acteurs », sa touche) : ce que Noge
// rend DEJA, sans rien inventer -- les primitives du NkMeshSystem (cube,
// sphere, cylindre, plan, capsule, cone ; NkRenderSystem::ResolvePrimitive),
// la camera (NkCameraComponent), les lumieres directionnelle, ponctuelle et
// projecteur (NkLightComponent), et un corps rigide (NkRigidbody3D +
// NkCollider3D, NkPhysicsSystem) pour ce qui tombe.
// -----------------------------------------------------------------------------

#include "NKECS/World/NkWorld.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKMath/NKMath.h"
#include "NKSerialization/NkSerializer.h"

namespace nkentseu {

	class NkPhysicsSystem;

	namespace nogee {

		enum class NogeeEtatJeu : uint8 { Edition = 0, Jeu, Pause };
		enum class NogeeOutil : uint8 { Selection = 0, Deplacer, Tourner, Echelle, Count };
		const char *NogeeNomOutil(NogeeOutil o) noexcept;

		/// Ce que pose un element du catalogue.
		enum class NogeeGenre : uint8 {
			ActeurVide = 0,
			Cube,
			Sphere,
			Cylindre,
			Plan,
			Capsule,
			Cone,
			Camera,
			LumiereDirectionnelle,
			LumierePonctuelle,
			LumiereProjecteur,
			CubePhysique,
			SpherePhysique,
			Sol,
			Count
		};

		/// Un element de « Placer des acteurs ». `onglets` : bits NkFamilleBitOnglet.
		struct NogeeElement {
				const char *nom;
				const char *aide;
				NogeeGenre genre;
				uint32 onglets;
		};
		const NogeeElement *NogeeCatalogue(int32 &nombre) noexcept;

		/// La nature d'une entite (l'icone de l'Outliner, la ligne « type »).
		enum class NogeeNature : uint8 { Entite = 0, Maillage, Rigide, Decor, Lumiere, Camera };

		/// La camera d'edition tourne autour d'un pivot (Unreal : clic droit + souris).
		struct NogeeOrbite {
				float32 lacetDeg = 35.f;
				float32 tangageDeg = 26.f;
				float32 distance = 14.f;
				math::NkVec3f pivot{0.f, 0.5f, 0.f};
		};

		class NogeeModele {
			public:
				static constexpr const char *kNomCameraEditeur = "__CameraEditeur";

				/// Le monde vient de NkEngineLayer ; la physique aussi (pour rendre les
				/// corps quand une scene est videe ou rechargee).
				void Brancher(ecs::NkWorld *monde, NkPhysicsSystem *physique) noexcept;
				ecs::NkWorld &Monde() noexcept {
					return *mMonde;
				}
				bool Pret() const noexcept {
					return mMonde != nullptr;
				}

				// ── La scene ──────────────────────────────────────────────────
				/// La scene de NogeDemo (le premier resultat de Noge) : un sol, un cube
				/// rouge, une balle jaune posee dessus, une camera, un soleil.
				void NouvelleScene();
				void Vider();
				ecs::NkEntityId Poser(NogeeGenre g, const math::NkVec3f &position, const char *nom = nullptr);
				/// L'element `k` du catalogue au point `position`, choisi, annonce, annulable.
				ecs::NkEntityId PoserElement(int32 k, const math::NkVec3f &position);
				void Supprimer(ecs::NkEntityId id);
				ecs::NkEntityId Dupliquer(ecs::NkEntityId id);
				void Renommer(ecs::NkEntityId id, const char *nom);
				/// Rattache `enfant` a `parent` (Invalid = le detacher), sans le deplacer.
				void Rattacher(ecs::NkEntityId enfant, ecs::NkEntityId parent);
				ecs::NkEntityId Parent(ecs::NkEntityId id) const noexcept;
				/// Les entites de la scene (la camera d'edition exclue), dans un ordre
				/// STABLE : celui de la trame d'avant, les nouvelles a la fin.
				void Entites(NkVector<ecs::NkEntityId> &sortie);
				const char *Nom(ecs::NkEntityId id) const noexcept;
				NogeeNature Nature(ecs::NkEntityId id) const noexcept;
				const char *Type(ecs::NkEntityId id) const noexcept;
				bool EstEditeur(ecs::NkEntityId id) const noexcept;
				/// La boite (monde) d'une entite : la primitive unite par sa matrice
				/// monde ; une entite sans maillage : un cube de 0,5 m autour d'elle.
				bool Boite(ecs::NkEntityId id, math::NkVec3f &mini, math::NkVec3f &maxi) const noexcept;

				// ── La selection ──────────────────────────────────────────────
				ecs::NkEntityId selection = ecs::NkEntityId::Invalid();
				bool aSelection = false;
				void Choisir(ecs::NkEntityId id) noexcept;
				void Deselectionner() noexcept;
				bool SelectionValide() const noexcept;

				// ── L'actif (la case des Details), l'oeil, le cadenas ─────────
				bool EstActive(ecs::NkEntityId id) const noexcept;
				void Activer(ecs::NkEntityId id, bool actif);
				bool EstCache(ecs::NkEntityId id) const noexcept;
				void BasculerCache(ecs::NkEntityId id);
				bool EstVerrouille(ecs::NkEntityId id) const noexcept;
				void BasculerVerrou(ecs::NkEntityId id);

				// ── Jouer ─────────────────────────────────────────────────────
				NogeeEtatJeu etat = NogeeEtatJeu::Edition;
				void Jouer();
				void Pause();
				void Arreter();
				void Pas();
				/// La physique avance-t-elle a cette image ? (consomme un « pas »)
				bool PhysiqueAvance() noexcept;

				// ── La camera d'edition ───────────────────────────────────────
				NogeeOrbite orbite;
				ecs::NkEntityId cameraEditeur = ecs::NkEntityId::Invalid();
				/// Pose la camera d'edition d'apres l'orbite (matrice monde comprise :
				/// le systeme de transforms est deja passe pour cette image).
				void PoserCameraEditeur();
				/// Cadre l'entite (F, double-clic dans l'Outliner).
				void Cadrer(ecs::NkEntityId id);

				// ── L'outil et l'accrochage ───────────────────────────────────
				NogeeOutil outil = NogeeOutil::Selection;
				bool repereLocal = false;
				bool accrocheGrille = true;
				float32 pasGrille = 0.25f;
				bool accrocheAngle = true;
				float32 pasAngle = 15.f;
				bool accrocheEchelle = false;
				float32 pasEchelle = 0.1f;
				bool voirGrille = true;

				// ── Le fichier ────────────────────────────────────────────────
				NkString chemin = "Build/Nogee/scene.nkscene";
				bool modifie = false;
				bool Enregistrer();
				bool Ouvrir(const char *fichier);

				// ── L'historique (Ctrl+Z / Ctrl+Y) : des instantanes de la scene ──
				/// A appeler AVANT une modification.
				void Retenir();
				bool Annuler();
				bool Refaire();
				bool PeutAnnuler() const noexcept {
					return !mAvant.Empty();
				}
				bool PeutRefaire() const noexcept {
					return !mApres.Empty();
				}

				// ── Les annonces ──────────────────────────────────────────────
				NkString message;
				float32 messageAge = 99.f;
				/// Ce qui attend d'entrer au journal (niveau : 0 info, 1 succes,
				/// 2 avertissement, 3 erreur), vide par l'interface.
				NkVector<NkString> aJournaliser;
				NkVector<uint8> niveaux;
				void Annoncer(const char *texte, uint8 niveau = 0);

			private:
				void CreerCameraEditeur();
				void DetruireCameraEditeur();
				bool Instantane(NkArchive &a);
				bool Restaurer(const NkArchive &a);
				void AssurerHierarchie(ecs::NkEntityId id);
				NkString NomLibre(const char *base);

				ecs::NkWorld *mMonde = nullptr;
				NkPhysicsSystem *mPhysique = nullptr;
				NkVector<ecs::NkEntityId> mOrdre;
				NkVector<ecs::NkEntityId> mVerrous;
				NkVector<NkArchive> mAvant;
				NkVector<NkArchive> mApres;
				NkArchive mAvantJeu;
				bool mAvantJeuPris = false;
				bool mPasDemande = false;
		};

	} // namespace nogee
} // namespace nkentseu
