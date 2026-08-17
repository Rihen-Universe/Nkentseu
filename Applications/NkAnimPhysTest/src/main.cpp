// =============================================================================
// NkAnimPhysTest — tests headless de la couche physique d'animation (NkAnima M3).
// AUCUN device GPU : pur CPU, sûr à lancer même pendant un entraînement GPU.
// Sortie via printf (sortie directe console, comme NKMeshAITest).
// =============================================================================
#include "NKAnimPhysics/NkPoseMass.h"
#include "NKAnimPhysics/NkBalance.h"
#include "NKAnimPhysics/NkContactDetector.h"
#include "NKAnimPhysics/NkPoseBalancer.h"
#include "NKAnimPhysics/NkAutoPose.h"
#include "NKAnimation/NkMotionPath.h"
#include "NKAnimPhysics/NkClipBalancePass.h"
#include "NKAnimation/NkAnimRetarget.h"
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKRenderer/Mesh/NkGLTFAnimBake.h"
#include "NKRenderer/Tools/Director/NkRoleContext.h"
#include "NKAudio/NkAudioCapture.h"
#include "NKAudio/NkDenoiser.h"

#include <cstdio>
#include <cmath>

using namespace nkentseu;

namespace {
	void Report(const char *tag, const char *desc, bool ok, int &nbOk, int &nbTotal) {
		++nbTotal;
		if (ok)
			++nbOk;
		printf("[ %s ] %s : %s\n", ok ? "OK " : "FAIL", tag, desc);
	}

	// ── Témoin du CÂBLAGE éditeur (pas de la brique) ─────────────────────────
	// Le câblage de NkAnimaEditor affiche le COM dans le viewport. Une physique
	// qu'on affiche se valide à l'oeil, et l'oeil est un MAUVAIS témoin : un COM
	// placé au mauvais endroit RESSEMBLE à un COM. Ce témoin est donc NUMÉRIQUE et
	// headless, et il rejoue la chaîne exacte appelée par AnimBridge :
	//     SetUniform(n)  ->  ComputeCOMFromPositions(positions monde, n)
	//
	// La pose est ASYMÉTRIQUE sur les TROIS axes, à dessein : sur une pose
	// symétrique, une permutation d'axes ou une matrice transposée donnerait le
	// MÊME résultat et le témoin ne prouverait rien — c'est le piège du témoin qui
	// passe des deux côtés du défaut.
	bool TemoinCablageCOM() {
		// 4 joints, coordonnées toutes distinctes.
		const math::NkVec3f pos[4] = {
			{1.0f, 2.0f, 3.0f},
			{5.0f, 7.0f, 11.0f},
			{13.0f, 17.0f, 19.0f},
			{23.0f, 29.0f, 31.0f},
		};
		const int32 n = 4;
		// Masse UNIFORME => COM = moyenne arithmétique, vérifiable à la main :
		//   x = (1+5+13+23)/4 = 10.5   y = (2+7+17+29)/4 = 13.75   z = (3+11+19+31)/4 = 16
		const math::NkVec3f attendu{10.5f, 13.75f, 16.0f};
		const float32 eps = 1e-4f;

		animphys::NkPoseMass mass;
		mass.SetUniform(n);
		const math::NkVec3f com = mass.ComputeCOMFromPositions(pos, n);
		printf("         COM mesure  = %.6f %.6f %.6f\n", (double)com.x, (double)com.y, (double)com.z);
		printf("         COM attendu = %.6f %.6f %.6f\n", (double)attendu.x, (double)attendu.y, (double)attendu.z);
		const bool exact = (fabsf(com.x - attendu.x) < eps) && (fabsf(com.y - attendu.y) < eps) &&
						   (fabsf(com.z - attendu.z) < eps);

		// CONTRE-ÉPREUVE : le témoin doit DISCRIMINER. On permute les axes
		// (x,y,z) -> (y,z,x) : le COM DOIT changer. S'il ne changeait pas, ce
		// témoin passerait aussi bien sur une extraction fausse.
		math::NkVec3f permute[4];
		for (int32 i = 0; i < n; ++i)
			permute[i] = math::NkVec3f{pos[i].y, pos[i].z, pos[i].x};
		const math::NkVec3f comP = mass.ComputeCOMFromPositions(permute, n);
		printf("         COM axes permutes = %.6f %.6f %.6f (doit DIFFERER)\n", (double)comP.x, (double)comP.y,
			   (double)comP.z);
		const bool discrimine =
			(fabsf(comP.x - com.x) > eps) || (fabsf(comP.y - com.y) > eps) || (fabsf(comP.z - com.z) > eps);

		// GARDE SILENCIEUSE : si le nombre de joints change sans re-synchroniser la
		// masse, la brique renvoie {0,0,0} SANS RIEN SIGNALER — un COM à l'origine.
		// Le câblage doit donc rappeler SetUniform quand le compte change ; ce
		// témoin fige le comportement sur lequel il s'appuie.
		const math::NkVec3f desync = mass.ComputeCOMFromPositions(pos, 3);
		const bool gardeOk = (fabsf(desync.x) < eps) && (fabsf(desync.y) < eps) && (fabsf(desync.z) < eps);
		printf("         count desynchronise -> %.6f %.6f %.6f (doit valoir 0 0 0)\n", (double)desync.x,
			   (double)desync.y, (double)desync.z);

		return exact && discrimine && gardeOk;
	}

	// ── Témoin de la chaîne des NOMS DE JOINTS (glTF -> clip -> masses) ──────
	// Un glTF REEL (CesiumMan, l'asset même que NkAnimaEditor charge) traverse la
	// chaîne complète ajoutée le 2026-08-17 :
	//     NkGLTFNode.name (parse) -> clip.jointNames (bake) -> SetAnthropometric.
	// Le verdict n'est pas « des chaînes sont arrivées » mais « le régime a PRIS » :
	// deux joints de familles différentes doivent porter des masses DIFFÉRENTES —
	// c'est ce qui distingue l'anthropométrique de l'uniforme, et c'est ce que
	// l'étiquette de l'éditeur promet à l'écran.
	bool TemoinNomsDeJoints() {
		// Le cwd varie (racine du dépôt ou dossier de l'exe) : on essaie les deux.
		const char *chemins[] = {
			"Resources/Models/CesiumMan/CesiumMan.glb",
			"../../../../Resources/Models/CesiumMan/CesiumMan.glb",
		};
		renderer::NkGLTFMeshData data;
		bool charge = false;
		const char *utilise = nullptr;
		for (int32 c = 0; c < 2 && !charge; ++c) {
			if (renderer::LoadGLTF(chemins[c], data) && data.isSkinned) {
				charge = true;
				utilise = chemins[c];
			}
		}
		if (!charge) {
			// ÉCHEC EXPLICITE, pas un saut silencieux : un témoin introuvable qui
			// « passe » validerait n'importe quoi.
			printf("         ECHEC : CesiumMan.glb introuvable ou non skinne (cwd inattendu)\n");
			return false;
		}
		printf("         asset : %s (%u joints)\n", utilise, (uint32)data.skinJoints.Size());

		anim::NkAnimationClip clip;
		if (!renderer::BakeClipFromGLTF(data, data.animations.Empty() ? -1 : 0, 30.f, clip)) {
			printf("         ECHEC : BakeClipFromGLTF\n");
			return false;
		}
		const uint32 jc = (uint32)clip.jointInverseBind.Size();
		if ((uint32)clip.jointNames.Size() != jc) {
			printf("         ECHEC : jointNames %u != joints %u\n", (uint32)clip.jointNames.Size(), jc);
			return false;
		}
		uint32 nonVides = 0;
		for (uint32 j = 0; j < jc; ++j)
			if (!clip.jointNames[j].Empty())
				++nonVides;
		printf("         noms non vides : %u/%u — ex: '%s'\n", nonVides, jc,
			   jc > 0 ? clip.jointNames[0].CStr() : "");
		if (nonVides == 0) {
			printf("         ECHEC : aucun nom n a traverse la chaine\n");
			return false;
		}

		// Le régime anthropométrique a PRIS : masses non toutes égales.
		animphys::NkPoseMass mass;
		mass.SetAnthropometric(clip.jointNames);
		if ((uint32)mass.jointMass.Size() != jc || mass.TotalMass() <= 0.f) {
			printf("         ECHEC : SetAnthropometric n a pas produit %u masses\n", jc);
			return false;
		}
		float32 mn = mass.jointMass[0], mx = mass.jointMass[0];
		for (uint32 j = 1; j < jc; ++j) {
			if (mass.jointMass[j] < mn)
				mn = mass.jointMass[j];
			if (mass.jointMass[j] > mx)
				mx = mass.jointMass[j];
		}
		printf("         masses : min=%.3f max=%.3f %s\n", (double)mn, (double)mx,
			   (mx > mn) ? "(NON uniformes : le regime a pris)" : "(UNIFORMES : regime pas pris !)");
		return mx > mn;
	}
} // namespace

int main() {
	printf("=== NkAnimPhysTest — physique d'animation NkAnima (headless, sans GPU) ===\n\n");

	int nbOk = 0;
	int nbTotal = 0;

	// M3.1 — distribution de masse + centre de masse (COM).
	Report("M3.1 NkPoseMass", "barycentre, pondere, monotonie, anthropometrie, gardes",
		   animphys::NkPoseMass::SelfTest(), nbOk, nbTotal);

	// M3.2 — solveur d'équilibre (polygone de support + COM dedans ? + marge).
	Report("M3.2 NkBalance", "dedans/dehors/bord, 2 pieds segment, direction de bascule",
		   animphys::NkBalance::SelfTest(), nbOk, nbTotal);

	// M3.3 — solveur de contacts (+ intégration M3.1+M3.2+M3.3 : debout=équilibré, penché=non).
	Report("M3.3 NkContactDetector", "contact sol, points de support, INTEGRATION debout/penche",
		   animphys::NkContactDetector::SelfTest(), nbOk, nbTotal);

	// M3.4 — optimiseur de pose sous contrainte (ajuste une pose pour respecter l'équilibre).
	Report("M3.4 NkPoseBalancer", "deseq->equilibre, strength 0/0.5/1, pose deja equilibree",
		   animphys::NkPoseBalancer::SelfTest(), nbOk, nbTotal);

	// M3.5 — auto-posing (poses intermediaires equilibrees entre deux cles).
	Report("M3.5 NkAutoPose", "lerp brut deseq -> BlendBalanced equilibre, pieds plantes, bornes t=0/1",
		   animphys::NkAutoPose::SelfTest(), nbOk, nbTotal);

	// Animation par courbe : spline Catmull-Rom + path-follow (os/effecteur IK suit la courbe).
	Report("NkMotionPath", "spline passe par les points, longueur/tangente droite, path-follow loop/once",
		   anim::NkMotionCurve::SelfTest(), nbOk, nbTotal);

	// M3.6 — pont vers l'anim existante : correction physique non destructive d'un clip + lissage.
	Report("M3.6 NkClipBalancePass", "clip qui bascule -> corrige frame par frame (equilibre), pieds fixes, lissage borne",
		   animphys::NkClipBalancePass::SelfTest(), nbOk, nbTotal);

	// M4bis.1 — contexte de role (personnage/personnalite/emotion/objectif/historique),
	// round-trip Archive+JSON, schema strict rejette les variantes malformees (anti texte-libre).
	Report("M4bis.1 NkRoleContext", "FIFO historique, round-trip Archive+JSON, schema accepte/rejette",
		   renderer::NkRoleContext::SelfTest(), nbOk, nbTotal);

	// NKAudio — enregistrement : ring buffer SPSC de la capture (headless, sans micro).
	Report("NKAudio NkAudioCapture", "ring buffer SPSC : write/read/wrap/overflow",
		   audio::NkAudioCapture::SelfTest(), nbOk, nbTotal);

	// NKAudio — débruitage + normalisation (soustraction spectrale + gate + auto-gain).
	Report("NKAudio NkDenoiser", "soustraction spectrale (plancher bruit chute), sinus survit, normalisation",
		   audio::NkDenoiser::SelfTest(), nbOk, nbTotal);

	// M2 — RECIBLAGE d'animation entre squelettes (brique explicitement notee
	// « reellement non commencee » dans la roadmap NkAnima).
	Report("M2 NkAnimRetarget", "appariement par nom, delta au repos, os non etires, racine a l'echelle",
		   anim::NkAnimRetarget::SelfTest(), nbOk, nbTotal);

	// CÂBLAGE éditeur — le COM affiche par NkAnimaEditor. Valide la CHAINE
	// (SetUniform -> ComputeCOMFromPositions), pas la brique : les suites M3.x
	// ci-dessus valident la physique, elles ne disent rien de ce que l'editeur
	// calcule reellement avant de le dessiner.
	Report("CABLAGE NkAnimaEditor COM", "pose asymetrique 3 axes, COM analytique, axes permutes, garde count",
		   TemoinCablageCOM(), nbOk, nbTotal);

	// NOMS DE JOINTS — glTF reel (CesiumMan) -> parse "name" -> bake -> masses
	// anthropometriques NON uniformes. C'est la promesse de l'etiquette a l'ecran.
	Report("CABLAGE noms de joints", "CesiumMan.glb -> jointNames -> SetAnthropometric, masses non uniformes",
		   TemoinNomsDeJoints(), nbOk, nbTotal);

	printf("\n=== Resultat : %d/%d suites OK ===\n", nbOk, nbTotal);
	return (nbOk == nbTotal) ? 0 : 1;
}
