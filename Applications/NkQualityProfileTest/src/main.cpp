// =============================================================================
// NkQualityProfileTest — clause (3) du critere de reussite (Rodolf, 2026-09-02)
// =============================================================================
// Regle gravee : LE MOTEUR PORTE LA QUALITE, L'APPLICATION LA SUBIT.
//
// Critere de reussite du chantier, en trois points :
//   (1) aucune ligne de l'application ne nomme une plateforme ni un preset ;
//   (2) NkRenderQuality est LU -- au moins un chemin change selon sa valeur ;
//   (3) UN BANC ECHOUE si on force `quality` a une autre valeur.
//
// ⚠️ POURQUOI (3) EXISTE. Mesure du 2026-09-02 : `quality` etait ECRIT 7 fois
// et LU 0 fois dans tout le depot. Sans un banc qui rougit quand la valeur
// change, on remplace un drapeau inerte par un drapeau lu une fois et sans
// effet observable -- le meme defaut avec un deguisement neuf.
//
// CPU-only : aucun contexte GPU n'est cree (la carte est a Ilyana).
// =============================================================================
#include "NKRenderer/Core/NkRendererConfig.h"

#include <cstdio>

using nkentseu::renderer::NkRendererConfig;
using nkentseu::renderer::NkRenderQuality;

namespace {

	int gPass = 0;
	int gFail = 0;

	void Check(bool cond, const char *libelle) {
		if (cond) {
			++gPass;
			std::printf("  [OK]   %s\n", libelle);
		} else {
			++gFail;
			std::printf("  [FAIL] %s\n", libelle);
		}
	}

	// Construit une config a une qualite donnee, SANS passer par un preset.
	NkRendererConfig At(NkRenderQuality q) {
		NkRendererConfig c;
		c.quality = q;
		c.ApplyQuality();
		return c;
	}

} // namespace

int main() {
	std::printf("=== NkQualityProfileTest : la qualite est-elle LUE ? ===\n");

	// ── (2) l'enum est LU : chaque palier doit produire un reglage distinct ──
	const NkRendererConfig mob = At(NkRenderQuality::NK_MOBILE);
	const NkRendererConfig low = At(NkRenderQuality::NK_LOW);
	const NkRendererConfig med = At(NkRenderQuality::NK_MEDIUM);
	const NkRendererConfig hig = At(NkRenderQuality::NK_HIGH);
	const NkRendererConfig ult = At(NkRenderQuality::NK_ULTRA);
	const NkRendererConfig cin = At(NkRenderQuality::NK_CINEMATIC);

	std::printf("-- resolution d'ombre par palier --\n");
	std::printf("     MOBILE=%u LOW=%u MEDIUM=%u HIGH=%u ULTRA=%u CINEMATIC=%u\n", mob.shadow.resolution,
				low.shadow.resolution, med.shadow.resolution, hig.shadow.resolution, ult.shadow.resolution,
				cin.shadow.resolution);

	// ⚠️ Temoin d'existence : une egalite entre deux mesures ne vaut que si
	// l'une d'elles existe. `0 == 0` serait vrai sur une config jamais remplie.
	Check(mob.shadow.resolution > 0 && cin.shadow.resolution > 0, "temoin d'existence : les resolutions sont non nulles");

	// C'EST LA CLAUSE (3) : forcer une autre valeur DOIT changer le resultat.
	Check(mob.shadow.resolution != hig.shadow.resolution, "MOBILE et HIGH ne donnent pas la meme resolution d'ombre");
	Check(mob.shadow.cascadeCount != hig.shadow.cascadeCount, "MOBILE et HIGH ne donnent pas le meme nombre de cascades");
	// ⚠️ `msaaSamples` N'EST PAS pilote par la qualite, DELIBEREMENT :
	// l'anticrenelage est une POLITIQUE (il interagit avec le pipeline --
	// le differe ne sait pas faire de MSAA), pas un palier. Le lier a
	// `quality` aurait fait passer les 31 sites appelants de ForGame de
	// MSAA 1 a MSAA 4 en silence : un changement de rendu et de cout
	// memoire que personne n'avait demande.
	//
	// 📌 Cette assertion testait le MSAA et a rougi des que le champ a quitte
	// ApplyQuality. Elle est remplacee -- pas supprimee -- par un champ que
	// la qualite pilote vraiment. C'est le motif corrige le matin meme sur
	// NkAssetIODemo : une garde perimee accuse le progres au lieu de le voir.
	Check(mob.shadow.poissonSamples != cin.shadow.poissonSamples,
		  "MOBILE et CINEMATIC ne donnent pas le meme echantillonnage d'ombre");
	Check(mob.msaaSamples == cin.msaaSamples,
		  "le MSAA est volontairement INDEPENDANT du palier de qualite");
	Check(mob.maxLights != cin.maxLights, "MOBILE et CINEMATIC ne donnent pas la meme limite de lumieres");

	// Monotonie : la qualite doit MONTER, jamais redescendre en montant d'un cran.
	Check(mob.shadow.resolution <= low.shadow.resolution && low.shadow.resolution <= med.shadow.resolution &&
			  med.shadow.resolution <= hig.shadow.resolution && hig.shadow.resolution <= ult.shadow.resolution &&
			  ult.shadow.resolution <= cin.shadow.resolution,
		  "la resolution d'ombre est monotone croissante sur les 6 paliers");
	Check(mob.maxLights < cin.maxLights, "la limite de lumieres croit de MOBILE a CINEMATIC");

	// Effets couteux : absents en bas, presents en haut.
	Check(!mob.postProcess.bloom && !mob.postProcess.ssao, "MOBILE : bloom et SSAO desactives");
	Check(cin.postProcess.bloom && cin.postProcess.ssao && cin.postProcess.taa, "CINEMATIC : bloom, SSAO et TAA actifs");
	Check(!mob.hdr && cin.hdr, "HDR : eteint en MOBILE, allume en CINEMATIC");
	Check(!mob.shadow.pcss && cin.shadow.pcss, "PCSS : eteint en MOBILE, allume en CINEMATIC");

	// ── (1) le selecteur : le jeu ne nomme NI plateforme NI preset ───────────
	std::printf("-- ForTarget() : le moteur choisit --\n");
	const NkRendererConfig cible = NkRendererConfig::ForTarget();
	std::printf("     quality=%d  ombre=%u  cascades=%u  msaa=%u  auto=%d\n", (int)cible.quality,
				cible.shadow.resolution, cible.shadow.cascadeCount, cible.msaaSamples, (int)cible.qualityAuto);

	Check(cible.qualityAuto, "ForTarget() marque le profil comme choisi par le moteur");
	Check(cible.shadow.resolution > 0 && cible.shadow.cascadeCount > 0, "ForTarget() produit des reglages remplis");
	// Le profil doit etre COHERENT avec ce que ApplyQuality derive de sa qualite :
	// autrement dit ForTarget ne bricole pas ses champs a la main.
	const NkRendererConfig temoin = At(cible.quality);
	Check(cible.shadow.resolution == temoin.shadow.resolution &&
			  cible.shadow.poissonSamples == temoin.shadow.poissonSamples &&
			  cible.maxLights == temoin.maxLights,
		  "ForTarget() derive ses reglages de `quality`, il ne les ecrit pas a la main");

	// ── La porte de sortie doit se VOIR ─────────────────────────────────────
	NkRendererConfig ecrase = NkRendererConfig::ForTarget();
	Check(ecrase.qualityOverrideCount == 0, "aucun ecrasement n'est compte tant qu'on n'en pose pas");
	ecrase.shadow.resolution = 512;
	ecrase.OverrideAfterQuality("banc : verification que l'ecrasement se voit");
	Check(ecrase.qualityOverrideCount == 1 && ecrase.qualityOverrideLast != nullptr,
		  "un ecrasement explicite est COMPTE et porte sa raison");

	// ── L'ARBITRAGE : un reglage explicite POSTERIEUR doit ECRASER ──────────
	// Contrat arbitre par Rodolf le 2026-09-02 : `quality` est la source, le
	// preset l'exprime, un reglage explicite pose apres gagne.
	//
	// ⚠️ CE QUE CE BLOC PROTEGE, et c'est different de la clause (3) ci-dessus.
	// (3) dit « l'enum est lu ». Celui-ci dit « l'ORDRE est le bon ». Si
	// quelqu'un deplaçait ApplyQuality() APRES les lignes du preset, la clause
	// (3) resterait verte et toutes les deviations assumees seraient effacees
	// en silence. C'est precisement le genre d'intention qui, laissee dans un
	// commentaire, ne protege rien.
	std::printf("-- arbitrage : le reglage explicite posterieur gagne --\n");

	const NkRendererConfig editeur = NkRendererConfig::ForEditor();
	const NkRendererConfig hautNu = At(NkRenderQuality::NK_HIGH);
	std::printf("     ForEditor: ombre=%u (palier HIGH nu = %u)  overrides=%u\n", editeur.shadow.resolution,
				hautNu.shadow.resolution, editeur.qualityOverrideCount);

	Check(hautNu.shadow.resolution == 2048, "temoin : le palier HIGH nu derive bien 2048");
	// C'EST L'ASSERTION D'ORDRE. ForEditor pose 1024 APRES ApplyQuality() :
	// si l'ordre etait inverse, on lirait 2048 et cette ligne rougirait.
	Check(editeur.shadow.resolution == 1024,
		  "ForEditor : la deviation posterieure (1024) ECRASE le palier (2048)");
	Check(editeur.shadow.resolution != hautNu.shadow.resolution,
		  "la deviation d'un preset se distingue de son palier nu");
	Check(editeur.qualityOverrideCount == 1 && editeur.qualityOverrideLast != nullptr,
		  "ForEditor DECLARE sa deviation : elle est comptee et porte sa raison");

	// ForArchviz eteint le bloom alors que son palier ULTRA l'allume.
	const NkRendererConfig archviz = NkRendererConfig::ForArchviz();
	const NkRendererConfig ultraNu = At(NkRenderQuality::NK_ULTRA);
	Check(ultraNu.postProcess.bloom, "temoin : le palier ULTRA nu allume le bloom");
	Check(!archviz.postProcess.bloom, "ForArchviz : le bloom eteint APRES le palier gagne");
	Check(archviz.qualityOverrideCount == 1, "ForArchviz DECLARE sa deviation");

	// Un preset SANS deviation ne doit rien declarer : sinon le compteur ne
	// voudrait plus rien dire.
	const NkRendererConfig jeu = NkRendererConfig::ForGame();
	Check(jeu.qualityOverrideCount == 0, "ForGame ne declare aucune deviation (il suit son palier)");
	Check(jeu.shadow.resolution == hautNu.shadow.resolution,
		  "ForGame : ombre identique au palier HIGH nu (il l'EXPRIME, il ne le contredit pas)");

	std::printf("=== Resultat : %d OK / %d FAIL ===\n", gPass, gFail);
	return gFail == 0 ? 0 : 1;
}
