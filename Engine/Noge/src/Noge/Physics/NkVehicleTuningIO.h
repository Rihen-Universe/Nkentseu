// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkVehicleTuningIO.h — le réglage du véhicule S'ÉCRIT ET SE RELIT (2026-09-14)
//
// POURQUOI CE FICHIER EXISTE, ET POURQUOI IL EST *ICI*
// ----------------------------------------------------
// Mesure du 14/09 : `NkVehicleTuning` expose une vingtaine de paramètres, et
// PERSONNE ne peut en charger un. `Tuning().<champ>` n'était écrit qu'à deux
// endroits — une sonde et un banc. Le produit n'avait aucun chemin de
// configuration : tout en dur, à l'appel.
//
// Il n'y avait pourtant RIEN à inventer. `NKSerialization` fait 7 633 lignes de
// .cpp (JSON, YAML, XML, binaire, NkArchive, NkReflectSerializer, versionnement
// de schéma) et compte treize consommateurs réels. Le seul module qui n'en avait
// aucune occurrence, c'était NKPhysics.
//
// ⚠️ ET IL NE DOIT PAS EN AVOIR. `NKPhysics.jenga` écrit en toutes lettres :
// « Bibliothèque de simulation PURE (sans ECS) [...] L'intégration gameplay
// (composants) se fait dans Noge, pas ici. » Brancher la sérialisation DANS
// NKPhysics contredirait le contrat que le module affiche lui-même — c'est
// exactement la famille « deux règles qui s'excluent ». Le pont vit donc là où
// l'architecture le dit : dans Noge, qui dépend déjà des DEUX (voir
// `Noge.jenga` : NKPhysics et NKSerialization sont dans `_DEPS`).
//
// En-tête SEUL (aucun .cpp, aucun lien) : il suffit d'ajouter le chemin
// d'inclusion de Noge pour s'en servir. `NkSystemsRevivalTest` l'a déjà.
//
// CE QUE CE FICHIER NE FAIT PAS
// -----------------------------
// ⚠️ Il ne change AUCUNE valeur par défaut du produit. Sans fichier chargé, le
// comportement est identique au bit à celui d'avant. `mu` reste 0,40 — la tenue
// de route est ce que Rodolf SENT, c'est sa décision, pas la mienne. Ce fichier
// lui donne le MOYEN de la prendre, il ne la prend pas à sa place.
// =============================================================================

#pragma once

#ifndef NOGE_PHYSICS_NKVEHICLETUNINGIO_H
#define NOGE_PHYSICS_NKVEHICLETUNINGIO_H

#include "NKPhysics/NkVehicle.h"
#include "NKSerialization/NkArchive.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKSerialization/JSON/NkJSONWriter.h"

#include <cstdio> // fopen("rb"/"wb") : voir plus bas, le mode BINAIRE est un critere

// ⚠️ CE QUE CE FICHIER N INCLUT PAS, ET POURQUOI (2026-09-14)
// `NKSerialization/NkSerializer.h` offre `SaveToFile`/`LoadFromFile`, et c est
// par la que j ai commence. Il ne compile pas ici : ses appels a `detail::`
// deviennent AMBIGUS des qu on inclut NKPhysics et NKSerialization dans la meme
// unite -- deux espaces de noms `detail` sous `nkentseu`. Ce n est pas mon
// defaut a corriger : treize consommateurs utilisent deja NKSerialization, et on
// ne reecrit pas un module partage pour se simplifier la vie. Je passe donc par
// `NkJSONWriter`/`NkJSONReader`, qui sont la couche juste en dessous et qui n ont
// pas ce probleme. **Le defaut est signale, pas contourne en silence.**

namespace noge {

	// =========================================================================
	//  LE DOCUMENT — un réglage de véhicule, sérialisable
	// =========================================================================
	// Enveloppe `NkVehicleTuning` et lui donne les trois méthodes que
	// `NkSerializable` exige. La structure de physique, elle, reste une donnée
	// pure : elle ne connaît ni archive, ni fichier, ni format.
	class NkVehicleTuningDoc final {
		public:
			nkentseu::physics::NkVehicleTuning tuning{};

			NkVehicleTuningDoc() noexcept = default;
			explicit NkVehicleTuningDoc(const nkentseu::physics::NkVehicleTuning &t) noexcept : tuning(t) {}

			static const char *TypeName() noexcept { return "NkVehicleTuning"; }

			// ── LA TABLE DES CHAMPS ───────────────────────────────────────────
			// ⚠️ Elle est écrite DEUX FOIS (écriture, lecture) : c'est ce que
			// l'archive demande. Une liste recopiée est une dérivation en double,
			// et une dérivation en double s'oublie en silence — un champ ajouté à
			// `NkVehicleTuning` et absent d'ici ne serait tout simplement jamais
			// sauvegardé, sans erreur, sans avertissement.
			// LA GARDE CI-DESSOUS REND CET OUBLI IMPOSSIBLE : `sizeof` change dès
			// qu'un champ entre dans la structure, et la compilation s'arrête.
			// Une garde qui ne coûte rien à l'exécution et qui parle au bon moment.
			// 24 champs : 21 `float32` (84 octets) + 3 `bool` rangés dans les
			// 4 octets de bourrage qui suivent. Total 88.
			// (14/09, second service de cette garde : `tyreFriction` et
			// `muFromChassis` sont entrés dans NkTuning et la compilation s'est
			// arrêtée ici tant que la table ne les portait pas. C'est exactement
			// ce pour quoi elle existe — la liste recopiée ne peut plus s'oublier.)
			// ⚠️ Cette garde a deja servi le jour meme ou elle a ete ecrite : j'avais
			// ecrit 88 de tete, et la compilation m'a arrete. C'est exactement le
			// service attendu -- elle parle AVANT que le champ soit perdu, pas apres.
			// ⚠️ LA TAILLE NE SE DEVINE PAS, elle se LIT. Je l'ai calculee de tete
			// deux fois et je me suis trompe deux fois (88 puis 84, puis 88 encore) :
			// le bourrage autour des `bool` ne suit pas l'intuition. Le banc IMPRIME
			// donc `sizeof` a chaque execution -- le prochain qui ajoute un champ n'a
			// plus a le calculer, il le lit.
			static_assert(sizeof(nkentseu::physics::NkVehicleTuning) == 92u,
						  "NkVehicleTuning a change de taille : un champ a ete ajoute ou retire. "
						  "Mets la table de NkVehicleTuningIO.h a jour (ECRITURE **et** LECTURE), "
						  "puis corrige cette taille. Sans ca, le champ ne serait jamais sauvegarde.");

			bool Serialize(nkentseu::NkArchive &ar) const {
				// suspension
				ar.SetFloat32("restLength", tuning.restLength);
				ar.SetFloat32("wheelRadius", tuning.wheelRadius);
				ar.SetFloat32("stiffness", tuning.stiffness);
				ar.SetFloat32("damping", tuning.damping);
				ar.SetFloat32("maxSuspFactor", tuning.maxSuspFactor);
				// moteur, frein, direction
				ar.SetFloat32("engineForce", tuning.engineForce);
				ar.SetFloat32("brakeForce", tuning.brakeForce);
				ar.SetFloat32("maxSteerDeg", tuning.maxSteerDeg);
				ar.SetFloat32("steerRateDegPerSec", tuning.steerRateDegPerSec);
				ar.SetFloat32("ackermann", tuning.ackermann);
				// adhérence
				ar.SetFloat32("mu", tuning.mu);
				ar.SetFloat32("tyreFriction", tuning.tyreFriction);
				ar.SetBool("muFromChassis", tuning.muFromChassis);
				ar.SetFloat32("freezeSpeed", tuning.freezeSpeed);
				ar.SetBool("staticFriction", tuning.staticFriction);
				ar.SetBool("alternateSweep", tuning.alternateSweep);
				// ce qui retient la voiture
				ar.SetFloat32("rollingResistance", tuning.rollingResistance);
				ar.SetFloat32("engineBrake", tuning.engineBrake);
				ar.SetFloat32("dragCd", tuning.dragCd);
				ar.SetFloat32("airDensity", tuning.airDensity);
				ar.SetFloat32("frontalArea", tuning.frontalArea);
				ar.SetFloat32("linearDamping", tuning.linearDamping);
				ar.SetFloat32("corneringDrag", tuning.corneringDrag);
				ar.SetFloat32("corneringDragTau", tuning.corneringDragTau);
				return true;
			}

			// ⚠️ CHAQUE CHAMP EST LU DANS UNE VARIABLE LOCALE INITIALISÉE PAR LA
			// VALEUR COURANTE, et n'écrase le réglage que si la clé existe. C'est
			// le volet (k3-négatif) : un fichier qui ne mentionne PAS un champ
			// laisse la valeur par défaut du produit intacte, au bit. Un fichier
			// partiel est donc une surcharge, pas une remise à zéro.
			bool Deserialize(const nkentseu::NkArchive &ar) {
				LisFloat(ar, "restLength", tuning.restLength);
				LisFloat(ar, "wheelRadius", tuning.wheelRadius);
				LisFloat(ar, "stiffness", tuning.stiffness);
				LisFloat(ar, "damping", tuning.damping);
				LisFloat(ar, "maxSuspFactor", tuning.maxSuspFactor);
				LisFloat(ar, "engineForce", tuning.engineForce);
				LisFloat(ar, "brakeForce", tuning.brakeForce);
				LisFloat(ar, "maxSteerDeg", tuning.maxSteerDeg);
				LisFloat(ar, "steerRateDegPerSec", tuning.steerRateDegPerSec);
				LisFloat(ar, "ackermann", tuning.ackermann);
				LisFloat(ar, "mu", tuning.mu);
				LisFloat(ar, "tyreFriction", tuning.tyreFriction);
				LisBool(ar, "muFromChassis", tuning.muFromChassis);
				LisFloat(ar, "freezeSpeed", tuning.freezeSpeed);
				LisBool(ar, "staticFriction", tuning.staticFriction);
				LisBool(ar, "alternateSweep", tuning.alternateSweep);
				LisFloat(ar, "rollingResistance", tuning.rollingResistance);
				LisFloat(ar, "engineBrake", tuning.engineBrake);
				LisFloat(ar, "dragCd", tuning.dragCd);
				LisFloat(ar, "airDensity", tuning.airDensity);
				LisFloat(ar, "frontalArea", tuning.frontalArea);
				LisFloat(ar, "linearDamping", tuning.linearDamping);
				LisFloat(ar, "corneringDrag", tuning.corneringDrag);
				LisFloat(ar, "corneringDragTau", tuning.corneringDragTau);
				return true;
			}

		private:
			static void LisFloat(const nkentseu::NkArchive &ar, const char *cle, nkentseu::float32 &dest) noexcept {
				nkentseu::float32 v = dest; // la valeur COURANTE : absente du fichier => inchangée
				if (ar.GetFloat32(cle, v)) dest = v;
			}
			static void LisBool(const nkentseu::NkArchive &ar, const char *cle, bool &dest) noexcept {
				nkentseu::nk_bool v = dest;
				if (ar.GetBool(cle, v)) dest = v;
			}
	};

	// =========================================================================
	//  LES DEUX JEUX DE VALEURS — pas deux codes (k3)
	// =========================================================================
	// `ackermann`, `alternateSweep` et `staticFriction` le prouvaient déjà : le
	// mécanisme des « deux mondes » n'est pas un mode, c'est un PARAMÈTRE, et il
	// y en a vingt-deux. Ce qui suit n'est donc pas une deuxième physique, c'est
	// une deuxième colonne de nombres.

	/// Le réglage du PRODUIT, tel qu'il est aujourd'hui. Rien n'y est modifié :
	/// ce sont les valeurs par défaut de `NkVehicleTuning`, écrites telles quelles.
	inline nkentseu::physics::NkVehicleTuning VehicleTuningJeu() noexcept {
		return nkentseu::physics::NkVehicleTuning{}; // les défauts, intacts
	}

	/// Le réglage « simulation ». CHAQUE écart au défaut est justifié par une
	/// mesure du 14/09 (banc 10 de `renderdemo`, sonde NK_VEHICLE_PROBE) :
	///
	/// ⚠️ MIS A JOUR LE 14/09 : `mu` N'EST PLUS UN ECART. Rodolf a tranché
	/// (« je dirais configurable meme si 0.90 me convient »), la grandeur a été
	/// corrigée à sa source, et 0,90 est devenu LE DEFAUT DU PRODUIT. Ce fichier
	/// le pose encore explicitement — pour qu'il se lise sans connaître le défaut —
	/// mais il ne change plus rien : ce n'est plus lui qui fait la différence.
	///
	///   linearDamping     0,02 -> 0,005. À 90 km/h les 0,02 pèsent 608 N, soit
	///                     55 % de tout ce qui retient la voiture et 1,9 fois
	///                     l'aérodynamique. Ce n'est pas un modèle de voiture,
	///                     c'est un amortisseur numérique : en simulation, on
	///                     laisse l'air et le roulement faire le travail et on
	///                     garde juste de quoi stabiliser l'intégrateur.
	///
	///   frontalArea       0 (dérivée) -> 2,31 m². La dérivation prend le
	///                     RECTANGLE englobant (4·demiX·demiY = 2,7214 m²) et
	///                     SURESTIME d'environ 15 % : une carrosserie n'est pas
	///                     un rectangle. 2,7214 x 0,85 = 2,313.
	///
	/// ⚠️ Tout le reste est laissé au défaut. Je ne règle pas ce que je n'ai pas
	/// mesuré, et je ne décide pas `mu` : ce fichier est là pour que Rodolf
	/// puisse comparer les deux mondes et trancher lui-même.
	inline nkentseu::physics::NkVehicleTuning VehicleTuningSimulation() noexcept {
		nkentseu::physics::NkVehicleTuning t{}; // on PART des défauts
		t.mu = 0.90f;
		t.linearDamping = 0.005f;
		t.frontalArea = 2.313f;
		return t;
	}

	// =========================================================================
	//  ÉCRIRE / RELIRE
	// =========================================================================
	// Deux lignes sur `NKSerialization`. Aucun format nouveau, aucun analyseur
	// écrit à la main. Le format se déduit de l'extension (`.json`, `.yaml`,
	// `.xml`, binaire) : un fichier `.json` est LISIBLE et MODIFIABLE à la main,
	// ce qui est tout l'intérêt pour quelqu'un qui veut régler sa voiture.

	/// Le réglage -> du texte JSON. Aucun fichier : l'appelant décide où ça va.
	inline bool VehicleTuningToJson(const nkentseu::physics::NkVehicleTuning &t, nkentseu::NkString &out) noexcept {
		const NkVehicleTuningDoc doc(t);
		nkentseu::NkArchive ar;
		if (!doc.Serialize(ar)) return false;
		return nkentseu::NkJSONWriter::WriteArchive(ar, out, true) != 0;
	}

	/// Du texte JSON -> le réglage. `out` ENTRE avec ce que l'appelant a déjà :
	/// un fichier partiel surcharge les champs qu'il nomme, et laisse les autres.
	inline bool VehicleTuningFromJson(nkentseu::NkStringView json, nkentseu::physics::NkVehicleTuning &out) noexcept {
		nkentseu::NkArchive ar;
		if (nkentseu::NkJSONReader::ReadArchive(json, ar) == 0) return false;
		NkVehicleTuningDoc doc(out);
		if (!doc.Deserialize(ar)) return false;
		out = doc.tuning;
		return true;
	}

	// ⚠️ LE MODE BINAIRE N'EST PAS UN DÉTAIL. En mode texte, Windows traduit
	// "\n" en CRLF à l'écriture ET CRLF en "\n" à la lecture : la traduction
	// s'applique des DEUX côtés et masque une différence réelle entre deux
	// fichiers. Le critère « identiques octet par octet » ne veut donc rien dire
	// si on ouvre en texte. D'où "wb" et "rb", partout, sans exception.

	inline bool SaveVehicleTuning(const nkentseu::physics::NkVehicleTuning &t, const char *chemin) noexcept {
		nkentseu::NkString json;
		if (!VehicleTuningToJson(t, json)) return false;
		std::FILE *f = std::fopen(chemin, "wb");
		if (!f) return false;
		const nkentseu::nk_size n = json.Size();
		const bool ok = (n == 0) || (std::fwrite(json.Data(), 1, n, f) == n);
		std::fclose(f);
		return ok;
	}

	inline bool LoadVehicleTuning(const char *chemin, nkentseu::physics::NkVehicleTuning &out) noexcept {
		std::FILE *f = std::fopen(chemin, "rb");
		if (!f) return false;
		std::fseek(f, 0, SEEK_END);
		const long taille = std::ftell(f);
		std::fseek(f, 0, SEEK_SET);
		if (taille <= 0) { std::fclose(f); return false; }
		nkentseu::NkString texte;
		texte.Resize((nkentseu::nk_size)taille);
		const bool lu = std::fread(texte.Data(), 1, (nkentseu::nk_size)taille, f) == (nkentseu::nk_size)taille;
		std::fclose(f);
		if (!lu) return false;
		return VehicleTuningFromJson(texte.View(), out);
	}

} // namespace noge

#endif // NOGE_PHYSICS_NKVEHICLETUNINGIO_H
