// =============================================================================
// Applications/NKEditTargetTest/src/main.cpp
// =============================================================================
// POURQUOI CE PROGRAMME EXISTE
// -----------------------------------------------------------------------------
// Le mode edition de NKCraft ne s'ouvre que sur les 86 objets de DEMONSTRATION.
// Un objet cree par l'utilisateur, ou importe, vit sur un autre gizmo : TAB ne
// le voit pas, et le journal affiche « Selectionne un objet (clic) avant TAB »
// alors qu'un objet EST selectionne. Un modeleur dont le mode edition ne
// fonctionne que sur ses propres demonstrations n'est pas un modeleur.
//
// La regle vivait en trois lignes au milieu du gestionnaire de TAB, dans
// NkDemo3D.cpp — 18 000 lignes, device graphique requis. Aucun banc ne pouvait
// l'atteindre. C'est le mur qui avait deja fait retirer NkMatInventaireTest du
// workspace le 17/08, et la raison d'etre de NkVpMatTypeDefaults.h. Meme parade :
// la REGLE sort dans NkVpEditTarget.h, la vue l'appelle, ce banc l'exerce.
//
// ⚠️ CE BANC EST ECRIT AVANT LE CORRECTIF, ET IL DOIT ROUGIR.
// Un temoin ecrit apres le diagnostic ne mesure que le diagnostic. Les cas
// « objet de l'utilisateur » echouent aujourd'hui, et c'est exactement ce qui
// prouve qu'ils mesurent quelque chose.
//
// CE QUI EST COUVERT, ET CE QUI NE L'EST PAS
// -----------------------------------------------------------------------------
// COUVERT : la RESOLUTION de la cible — quel objet TAB prend, a partir de l'etat
//   de selection. C'est la brique absente.
// NON COUVERT : ce que fait ensuite Demo3D_EnterEditOnObject (cloner les donnees
//   CPU, construire la cage, la persistance en sortie). Ce code exige un device
//   et reste hors de portee d'un banc console — il faudra une capture.
//   ⚠️ Le dire ici plutot que de laisser croire que « le mode edition est teste ».
// =============================================================================
#include "NK3DModeler/Viewport/NkVpEditTarget.h"

#include <stdio.h>

using namespace nkentseu;
using namespace nkentseu::demo;

namespace {

	int32 gPass = 0, gFail = 0;

	void Cas(bool ok, const char *quoi) {
		if (ok) {
			gPass++;
			printf("  OK    %s\n", quoi);
		} else {
			gFail++;
			printf("  FAIL  %s\n", quoi);
		}
	}

	// Selection d'un objet UTILISATEUR : le slot `u` correspond au noeud
	// kNkvpFirstUser + u, donc a l'indice (kNkvpFirstUser + u) - kNkvpEmptyBase
	// dans le gizmo des empties.
	NkVpEditQuery SelectionUtilisateur(int32 u, uint8 kind, bool meshValide = true, bool supprime = false) {
		NkVpEditQuery q;
		q.selDemo = -1;
		q.selEmpty = (kNkvpFirstUser + u) - kNkvpEmptyBase;
		q.userKind = kind;
		q.userMeshValid = meshValide;
		q.userDeleted = supprime;
		return q;
	}

	// ─────────────────────────────────────────────────────────────────────────
	// 1. L'ESPACE D'INDICES : la conversion doit etre exacte dans les deux sens
	// ⚠️ Sans ce cas, tous les suivants pourraient viser le mauvais slot et
	// echouer pour une raison qui n'est pas celle qu'on croit mesurer.
	// ─────────────────────────────────────────────────────────────────────────
	void Test_EspaceDIndices() {
		printf("-- cible/espace-d-indices --\n");
		Cas(NkVpUserSlotOfEmpty((kNkvpFirstUser + 0) - kNkvpEmptyBase) == 0,
			"cible/espace-d-indices : le premier noeud utilisateur est le slot 0");
		Cas(NkVpUserSlotOfEmpty((kNkvpFirstUser + 7) - kNkvpEmptyBase) == 7,
			"cible/espace-d-indices : le huitieme noeud utilisateur est le slot 7");
		// Un EMPTY de parentage (noeud 90..95) n'est pas un slot utilisateur.
		Cas(NkVpUserSlotOfEmpty(0) == -1,
			"cible/espace-d-indices : un empty de parentage n'est pas un slot utilisateur");
		Cas(NkVpUserSlotOfEmpty(-1) == -1, "cible/espace-d-indices : aucune selection rend -1");
		// Au-dela du dernier slot : rien, et surtout pas un indice hors bornes
		// qui irait lire nkvpUserKind au hasard.
		Cas(NkVpUserSlotOfEmpty((kNkvpFirstUser + kNkvpMaxUser) - kNkvpEmptyBase) == -1,
			"cible/espace-d-indices : au-dela du dernier slot, rien (pas de lecture hors bornes)");
	}

	// ─────────────────────────────────────────────────────────────────────────
	// 2. CE QUI MARCHE DEJA — il doit continuer a marcher
	// ⚠️ Un correctif qui rendrait les objets utilisateur editables EN CASSANT
	// les objets de demonstration serait un echange, pas une correction.
	// ─────────────────────────────────────────────────────────────────────────
	void Test_ObjetDeDemonstration() {
		printf("-- cible/objet-de-demonstration --\n");
		NkVpEditQuery q;
		q.selDemo = 12;
		const NkVpEditTarget t = NkVpResolveEditTarget(q);
		Cas(t.kind == NkVpEditKind::Demo && t.index == 12,
			"cible/objet-de-demonstration : un objet de demo reste editable, au bon indice");
	}

	void Test_RienDeSelectionne() {
		printf("-- cible/rien-de-selectionne --\n");
		NkVpEditQuery q;
		const NkVpEditTarget t = NkVpResolveEditTarget(q);
		Cas(t.kind == NkVpEditKind::Aucun, "cible/rien-de-selectionne : aucune cible, et le message a raison");
	}

	// ─────────────────────────────────────────────────────────────────────────
	// 3. LA BRIQUE ABSENTE — ces cas doivent ROUGIR aujourd'hui
	// ─────────────────────────────────────────────────────────────────────────
	void Test_ObjetDeLUtilisateur() {
		printf("-- cible/objet-de-l-utilisateur --\n");
		// Nature 2 = famille cube. ⚠️ C'est AUSSI la nature que prend toute
		// geometrie IMPORTEE (cf. HostAllocUser) : ce cas couvre donc les deux
		// gestes que Rodolf a demandes, creer et importer.
		{
			const NkVpEditTarget t = NkVpResolveEditTarget(SelectionUtilisateur(3, (uint8)NkVpUserKind::Cube));
			Cas(t.kind == NkVpEditKind::Utilisateur && t.index == 3,
				"cible/objet-de-l-utilisateur : un CUBE cree par l'utilisateur est editable");
		}
		{
			const NkVpEditTarget t = NkVpResolveEditTarget(SelectionUtilisateur(0, (uint8)NkVpUserKind::Sphere));
			Cas(t.kind == NkVpEditKind::Utilisateur && t.index == 0,
				"cible/objet-de-l-utilisateur : une SPHERE creee par l'utilisateur est editable");
		}
		{
			const NkVpEditTarget t = NkVpResolveEditTarget(SelectionUtilisateur(9, (uint8)NkVpUserKind::Plan));
			Cas(t.kind == NkVpEditKind::Utilisateur && t.index == 9,
				"cible/objet-de-l-utilisateur : un PLAN cree par l'utilisateur est editable");
		}
	}

	// ─────────────────────────────────────────────────────────────────────────
	// 4. CE QUI NE DOIT PAS DEVENIR EDITABLE
	// ⚠️ Sans ces cas, « rendre les objets utilisateur editables » pourrait se
	// resumer a « rendre TOUT editable », et une lampe ou un marqueur sans
	// geometrie entrerait en mode edition sur une cage vide.
	// ─────────────────────────────────────────────────────────────────────────
	void Test_CeQuiNEstPasEditable() {
		printf("-- cible/pas-editable --\n");
		{
			const NkVpEditTarget t = NkVpResolveEditTarget(SelectionUtilisateur(1, (uint8)NkVpUserKind::Empty));
			Cas(t.kind == NkVpEditKind::Aucun, "cible/pas-editable : un EMPTY n'a pas de geometrie");
		}
		{
			const NkVpEditTarget t = NkVpResolveEditTarget(SelectionUtilisateur(2, (uint8)NkVpUserKind::Lumiere));
			Cas(t.kind == NkVpEditKind::Aucun, "cible/pas-editable : une LUMIERE n'a pas de geometrie");
		}
		{
			// Natures 6..9 : marqueurs types en attendant leur backend.
			const NkVpEditTarget t = NkVpResolveEditTarget(SelectionUtilisateur(4, (uint8)NkVpUserKind::Courbe));
			Cas(t.kind == NkVpEditKind::Aucun, "cible/pas-editable : une COURBE est un marqueur sans geometrie");
		}
		{
			const NkVpEditTarget t =
				NkVpResolveEditTarget(SelectionUtilisateur(5, (uint8)NkVpUserKind::Cube, /*meshValide*/ false));
			Cas(t.kind == NkVpEditKind::Aucun,
				"cible/pas-editable : un slot dont le maillage n'existe pas encore");
		}
		{
			const NkVpEditTarget t = NkVpResolveEditTarget(
				SelectionUtilisateur(6, (uint8)NkVpUserKind::Cube, /*meshValide*/ true, /*supprime*/ true));
			Cas(t.kind == NkVpEditKind::Aucun, "cible/pas-editable : un objet SUPPRIME ne se rouvre pas");
		}
		{
			const NkVpEditTarget t = NkVpResolveEditTarget(SelectionUtilisateur(7, (uint8)NkVpUserKind::Libre));
			Cas(t.kind == NkVpEditKind::Aucun, "cible/pas-editable : un slot LIBRE ne designe aucun objet");
		}
	}

	// ─────────────────────────────────────────────────────────────────────────
	// 5. LES DEUX A LA FOIS — la priorite doit etre ECRITE, pas subie
	// ⚠️ Les deux gizmos peuvent porter une selection en meme temps. Sans ce
	// cas, la regle « le premier qui repond gagne » serait un accident d'ordre
	// des `if`, et le jour ou quelqu'un les reordonne, la cible change sans que
	// rien ne le dise.
	// ─────────────────────────────────────────────────────────────────────────
	void Test_PrioriteQuandLesDeuxRepondent() {
		printf("-- cible/priorite --\n");
		NkVpEditQuery q = SelectionUtilisateur(3, (uint8)NkVpUserKind::Cube);
		q.selDemo = 5;
		const NkVpEditTarget t = NkVpResolveEditTarget(q);
		Cas(t.kind == NkVpEditKind::Demo && t.index == 5,
			"cible/priorite : l'objet de demonstration l'emporte (regle ecrite, pas subie)");
	}

} // namespace

int main() {
	printf("=== NKEditTargetTest : quel objet le mode EDITION prend-il ? (console, sans UI) ===\n");
	Test_EspaceDIndices();
	Test_ObjetDeDemonstration();
	Test_RienDeSelectionne();
	Test_ObjetDeLUtilisateur();
	Test_CeQuiNEstPasEditable();
	Test_PrioriteQuandLesDeuxRepondent();
	printf("=== Resultat : %d OK / %d FAIL ===\n", gPass, gFail);
	return gFail == 0 ? 0 : 1;
}
