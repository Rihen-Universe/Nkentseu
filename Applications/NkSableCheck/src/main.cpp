// =============================================================================
// NkSableCheck — UNE ROUE PASSE, LE SOL GARDE-T-IL SON ORNIERE ? ET LE TAS
//                S'EFFONDRE-T-IL AU-DELA DE SON ANGLE DE REPOS ?
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Sederis - Rihen
// =============================================================================
// Quatre familles de criteres, chacune avec ses negatifs, et cinq mutations.
//
//   (t0) LA CONDITION D'ESSAI ELLE-MEME. On mesure la pente du terrain NU avant
//        d'y poser quoi que ce soit -- « un banc a mesure une pente de 10 degres
//        sur un sol parfaitement plat ». Et le compteur de pixels prouve son
//        ZERO avant tout le reste : meme cible, meme effacement, AUCUN trace.
//
//   (t1) LE NEGATIF CAPITAL. Sans aucune deformation, la hauteur vaut EXACTEMENT
//        celle de l'image, AU BIT. Mesure sur un terrain PLAT *et* sur un terrain
//        EN PENTE : un plat a zero cacherait une couche qui fuit d'un ULP.
//
//   (t2) UN CORPS CREUSE. L'enfoncement n'est pas donne au code : il est DERIVE
//        de la masse et de l'aire de contact par `NkSableEnfoncement`, la loi
//        publique -- le banc recalcule le meme nombre et exige l'egalite AU BIT.
//        NEGATIFS : roche (deformabilite 0) -> aucun creusement ; masse sous la
//        portance -> aucun creusement. Dans les deux cas le champ est intact AU BIT.
//
//   (t3) LE TALUS NATUREL. C'est ce qui separe du sable d'une pate a modeler.
//        Un cone pose a 45 deg DOIT s'effondrer sous un angle de repos de 33 deg.
//        NEGATIF CAPITAL : angle de repos a 90 deg -> le tas NE BOUGE PAS, AU BIT.
//        CONTRE-NEGATIF : angle de repos a 0 deg -> il s'aplatit. Sans lui, un
//        relaxeur qui ne fait RIEN passerait le negatif de 90 deg avec succes.
//
//   (t4) CA SE VOIT. Une SUITE d'images -- le sol nu, l'orniere, le tas avant et
//        apres effondrement -- plus des cartes de deformation vues de dessus.
//        « 138 criteres verts d'un autre agent ne voyaient pas trois infidelites
//        que les images ont montrees en une minute. »
//
// SANS FENETRE. Device DX11 headless (`NkDeviceInitInfo` sans surface, width=0,
// height=0), rendu hors-ecran par `Tools/Offscreen` REUTILISE tel quel. Ce banc
// n'ouvre AUCUNE fenetre -- il n'y a donc aucune fenetre a journaliser, et rien
// a fermer.
//
// PAS DE CODEC EN AMONT, ET C'EST DELIBERE. Les terrains d'essai sont construits
// depuis un tableau de niveaux par `NkTerrainDepuisNiveaux`, sans passer par un
// PNG. L'aller-retour disque est deja SOUS TEMOIN dans `NkTerrainCheck` (t0) ;
// le refaire ici melangerait deux mesures, et un defaut de codec sortirait sous
// le nom du sable.
//
// LE JOURNAL EST POSITIONNEL (`NkFormat("{0}")`), jamais variadique, et il sort
// par `fwrite` sur stdout EN PLUS du logger : mesure du 14/09, le puits console
// de NKLogger ne traverse pas les poignees redirigees, et Rodolf lit une console
// vide pendant que le verdict dort dans logs/app.log.
// =============================================================================
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Core/NkIDevice.h"

#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Mesh/NkTerrainSable.h"
#include "NKRenderer/Shader/NkShaderLibrary.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"

#include "NKImage/Core/NkImage.h"

#include "NKContainers/String/NkFormat.h"
#include "NKLogger/NkLog.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

using RHIStage = ::nkentseu::NkShaderStage;

using namespace nkentseu;
using namespace nkentseu::renderer;

// ─────────────────────────────────────────────────────────────────────────────
//  Comptage des cas. Un binaire qui n'a execute AUCUN cas annonce la meme chose
//  qu'un binaire dont tout passe : le nombre de cas est donc imprime, toujours.
// ─────────────────────────────────────────────────────────────────────────────
static uint32 gCas = 0;
static uint32 gEchecs = 0;

template <typename... Args>
static void Dire(const char *format, Args... args) {
	const NkString ligne = NkFormat(format, args...);
	const char *c = ligne.CStr();
	logger.Info("{0}", ligne);
	if (c != nullptr)
		fwrite(c, 1, strlen(c), stdout);
	fputc('\n', stdout);
	fflush(stdout);
}

static void Cas(const char *nom, bool ok, const NkString &detail) {
	++gCas;
	if (!ok)
		++gEchecs;
	Dire("  [{0}] {1} : {2}", NkString(ok ? "OK  " : "ROUGE"), NkString(nom), detail);
}

// ─────────────────────────────────────────────────────────────────────────────
//  MUTATIONS — casser volontairement, et exiger qu'UN SEUL critere rougisse.
//  « Une garde verte peut ne rien garder du tout. »
// ─────────────────────────────────────────────────────────────────────────────
enum class Mutation : uint8 {
	AUCUNE = 0,
	DEPOT_EPSILON,	  // un seul creux d'un ULP dans un champ cense etre vierge -> (t1)
	ROCHE_COUCHE0,	  // la deformabilite lue sur la couche 0 au lieu de la 2  -> (t2 roche)
	SANS_RELAXATION,  // on ne relaxe pas le tas                               -> (t3)
	BOURRELET_AMPUTE, // la moitie du bourrelet disparait                      -> (t2 volume)
	VIDE,			  // rien n'est execute : le NOMBRE DE CAS doit s'effondrer
};
static Mutation gMutation = Mutation::AUCUNE;

static const char *NomMutation(Mutation m) {
	switch (m) {
		case Mutation::AUCUNE:
			return "aucune";
		case Mutation::DEPOT_EPSILON:
			return "depot-epsilon";
		case Mutation::ROCHE_COUCHE0:
			return "roche-couche0";
		case Mutation::SANS_RELAXATION:
			return "sans-relaxation";
		case Mutation::BOURRELET_AMPUTE:
			return "bourrelet-ampute";
		case Mutation::VIDE:
			return "vide";
	}
	return "?";
}

// ⚠️ UNE CAPTURE D'UNE EXECUTION MUTEE NE DOIT PAS PORTER LE NOM DE LA VRAIE.
// Regle heritee de NkTerrainCheck, et elle vient d'une vraie alerte du 14/09 :
// une image de mutation ecrite sous le nom d'une saine, et « Rodolf, regarde ces
// captures » deja ecrit dans le canal. Le nom est le seul endroit qu'on regarde.
static NkString CheminCapture(const char *base) {
	if (gMutation == Mutation::AUCUNE)
		return NkString(base);
	return NkFormat("MUTE_{0}_{1}", NkString(NomMutation(gMutation)), NkString(base));
}

// ─────────────────────────────────────────────────────────────────────────────
//  LES PARAMETRES DU BANC — TOUT ATTENDU EN DESCEND, AUCUN N'EST RECOPIE
// ─────────────────────────────────────────────────────────────────────────────
static const uint32 kN = 129u, kM = 129u; // 129 x 129 sommets
static const float32 kPas = 0.02f;		  // 2 cm -> terrain de 2,56 m x 2,56 m

// La roue.
static const float32 kDemiLargeur = 0.12f;	  // m  -> trace de 24 cm de large
static const float32 kLongueurContact = 0.20f; // m
static const float32 kMasseRoue = 400.f;	  // kg
// Masse SOUS la portance, pour le negatif : la masse limite vaut
// portance * aire / gravite ; elle est calculee dans le corps, pas ecrite ici.
static const float32 kMasseLegere = 20.f; // kg
static const float32 kTraceAX = -0.9f, kTraceAZ = 0.f;
static const float32 kTraceBX = 0.9f, kTraceBZ = 0.f;

// Le tas.
static const float32 kConeRayon = 0.30f;   // m
static const float32 kConeHauteur = 0.30f; // m  -> pente posee = atan(1) = 45 deg
static const uint32 kIterationsMax = 4000u;

static const double kRadEnDeg = 57.29577951308232;

static NkTerrainParams ParamsTerrain() {
	NkTerrainParams p;
	p.pasX = kPas;
	p.pasZ = kPas;
	p.hauteurMin = 0.f;
	// Puissance de deux : toutes les hauteurs attendues sont EXACTES en float32.
	// Avec (max-min)/255 aucune ne le serait, et le negatif « au bit » deviendrait
	// inatteignable. La raison complete est dans NkTerrainHeightMap.h.
	p.hauteurParNiveau = 1.f / 256.f;
	p.canal = 0;
	p.centre = true;
	p.lisse = true;
	return p;
}

static NkTerrainSableParams ParamsSable() {
	NkTerrainSableParams p;
	p.portance = 6000.f;   // Pa
	p.raideur = 600000.f;  // Pa/m
	p.gravite = 9.81f;
	p.enfoncementMax = 0.5f;
	p.angleReposDeg = 33.f;
	p.facteurRelaxation = 0.5f;
	p.largeurBourrelet = 1.8f;
	return p;
}

// ─────────────────────────────────────────────────────────────────────────────
//  OUTILS DE COMPARAISON AU BIT
//
//  ⚠️ ON COMPARE DES MOTIFS DE BITS, PAS DES FLOTTANTS. `a == b` rend VRAI pour
//  (-0.f, +0.f) -- or c'est EXACTEMENT le couple que le chemin zero de
//  `NkSableHauteurCombinee` existe pour distinguer. Un banc qui compare avec
//  `==` declarerait le negatif capital vert sans jamais pouvoir le voir tomber.
// ─────────────────────────────────────────────────────────────────────────────
static uint32 Bits(float32 v) {
	uint32 b;
	memcpy(&b, &v, sizeof(b));
	return b;
}

static uint32 DiffAuBit(const float32 *a, const float32 *b, uint32 n) {
	uint32 d = 0;
	for (uint32 k = 0; k < n; ++k)
		if (Bits(a[k]) != Bits(b[k]))
			++d;
	return d;
}

// ─────────────────────────────────────────────────────────────────────────────
//  CONSTRUCTION DES TERRAINS D'ESSAI
// ─────────────────────────────────────────────────────────────────────────────
static void NiveauxPlats(NkVector<float32> &out) {
	out.Resize(kN * kM);
	for (uint32 k = 0; k < kN * kM; ++k)
		out[k] = 0.f;
}

// Pente le long de X : niveau = i, donc hauteur = i / 256.
// tan(pente) = (1/256) / kPas -> l'attendu se derive, il n'est pas recopie.
static void NiveauxEnPente(NkVector<float32> &out) {
	out.Resize(kN * kM);
	for (uint32 j = 0; j < kM; ++j)
		for (uint32 i = 0; i < kN; ++i)
			out[j * kN + i] = (float32)i;
}

struct Terrain {
		NkVector<float32> niveaux;
		NkEditMesh mesh;
		NkTerrainSable champ;
		NkVector<float32> baseTemoin; // copie des y du maillage, prise AVANT tout
};

static bool MonterTerrain(const char *nom, bool enPente, Terrain &t) {
	const NkTerrainParams tp = ParamsTerrain();
	if (enPente)
		NiveauxEnPente(t.niveaux);
	else
		NiveauxPlats(t.niveaux);

	const NkTerrainStatut st = NkTerrainDepuisNiveaux(t.niveaux.Data(), kN, kM, tp, t.mesh);
	if (st != NkTerrainStatut::NK_OK) {
		Cas(nom, false, NkFormat("NkTerrainDepuisNiveaux rend {0}", NkString(NkTerrainStatutNom(st))));
		return false;
	}
	const NkSableStatut ss = NkSableInitDepuisNiveaux(t.niveaux.Data(), kN, kM, tp, t.champ);
	if (ss != NkSableStatut::NK_OK) {
		Cas(nom, false, NkFormat("NkSableInitDepuisNiveaux rend {0}", NkString(NkSableStatutNom(ss))));
		return false;
	}
	t.baseTemoin.Resize(kN * kM);
	for (uint32 k = 0; k < kN * kM; ++k)
		t.baseTemoin[k] = t.mesh.verts[k].pos.y;
	Cas(nom, true,
		NkFormat("{0}x{1} sommets, pas={2} m, hauteurParNiveau=1/256", kN, kM, kPas));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  LA CARTE DE DEFORMATION, VUE DE DESSUS — l'image la plus lisible du lot
//
//  Aucun GPU, aucune projection, aucune normale : un pixel = une cellule. C'est
//  precisement ce qui la rend INCONTOURNABLE comme temoin -- un defaut de
//  pipeline, de matrice ou d'eclairage ne peut pas s'y cacher, et un depot nul
//  ne peut pas s'y faire passer pour une orniere.
//  Creuse -> petrole Rihen (bleu sombre). Bourrelet -> orange Rihen. Intact ->
//  gris neutre. L'echelle est NORMALISEE par le plus grand ecart rencontre, et
//  ce nombre est IMPRIME : une carte sans son echelle ment sur l'amplitude.
// ─────────────────────────────────────────────────────────────────────────────
static void EcrireCarteDepot(const NkTerrainSable &c, const char *chemin) {
	float32 ampl = 0.f;
	const uint32 n = c.Taille();
	for (uint32 k = 0; k < n; ++k) {
		const float32 a = c.depot[k] < 0.f ? -c.depot[k] : c.depot[k];
		if (a > ampl)
			ampl = a;
	}
	// ⚠️ AGRANDIE AU PLUS PROCHE VOISIN, ET C'EST POUR L'OEIL. A l'echelle
	// naturelle la carte fait 129 pixels de cote : elle est JUSTE et illisible.
	// Le plus proche voisin n'invente aucune valeur -- une interpolation
	// bilineaire lisserait la levre de l'orniere, c'est-a-dire exactement ce
	// qu'on veut montrer.
	const uint32 zoom = 4u;
	NkImage img = NkImage::Create(c.N * zoom, c.M * zoom, NkImagePixelFormat::NK_RGBA32, 0u);
	if (!img.IsValid()) {
		Dire("  carte {0} : NkImage::Create a echoue", NkString(chemin));
		return;
	}
	const float32 inv = ampl > 0.f ? 1.f / ampl : 0.f;
	for (uint32 j = 0; j < c.M; ++j) {
		for (uint32 i = 0; i < c.N; ++i) {
			const float32 d = c.depot[j * c.N + i] * inv; // [-1, 1]
			uint8 r, g, b;
			if (d < 0.f) {
				const float32 t = -d;
				r = (uint8)(160.f - 150.f * t);
				g = (uint8)(160.f - 75.f * t);
				b = (uint8)(160.f - 65.f * t); // -> 10, 85, 95 : petrole #0A555F
			} else {
				const float32 t = d;
				r = (uint8)(160.f + 87.f * t); // -> 247
				g = (uint8)(160.f - 6.f * t);  // -> 154
				b = (uint8)(160.f - 120.f * t); // -> 40 : orange #F79A28
			}
			for (uint32 zj = 0; zj < zoom; ++zj)
				for (uint32 zi = 0; zi < zoom; ++zi)
					img.SetPixel((int32)(i * zoom + zi), (int32)(j * zoom + zj),
								 math::NkColor(r, g, b, 255));
		}
	}
	const NkString p = CheminCapture(chemin);
	if (!img.SavePNG(p.CStr()))
		Dire("  carte {0} : SavePNG a echoue", p);
	else
		Dire("  carte ecrite : {0}   (amplitude max = {1} m ; petrole = creuse, orange = bourrelet)", p,
			 ampl);
}

// ─────────────────────────────────────────────────────────────────────────────
//  LE PROFIL TRANSVERSAL, EN TEXTE
//  Une coupe perpendiculaire a la trace, imprimee en clair. C'est la mesure que
//  Rodolf peut relire sans aucun outil, et c'est aussi celle qui montre les DEUX
//  CRETES de part et d'autre du creux -- ce qui fait qu'une orniere est une
//  orniere et pas un trou.
// ─────────────────────────────────────────────────────────────────────────────
static void ImprimerProfil(const NkTerrainSable &c, uint32 iCoupe) {
	Dire("  profil transversal a la colonne i={0} (z croissant vers le bas) :", iCoupe);
	const uint32 jCentre = c.M / 2u;
	const uint32 demi = 18u;
	for (uint32 d = 0; d <= 2u * demi; ++d) {
		const uint32 j = jCentre - demi + d;
		if (j >= c.M)
			continue;
		const uint32 k = j * c.N + iCoupe;
		const float32 dep = c.depot[k];
		// Barre proportionnelle : 40 colonnes pour 0,20 m.
		char barre[84];
		memset(barre, ' ', sizeof(barre));
		int32 col = 40 + (int32)(dep * 200.f);
		if (col < 0)
			col = 0;
		if (col > 80)
			col = 80;
		barre[40] = '|';
		barre[col] = (dep < 0.f) ? '#' : (dep > 0.f ? '+' : '|');
		barre[81] = 0;
		// LA BARRE D'ABORD. Mise apres les nombres, elle se decalait d'une ligne
		// a l'autre parce que le formatage d'un flottant n'a pas de largeur fixe :
		// le profil devenait illisible alors que les nombres etaient justes.
		Dire("   {0}  z={1}  depot={2} m", NkString(barre), (float32)(c.z0 + (float32)j * c.pasZ),
			 dep);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  (t4) LA SCENE HORS-ECRAN — reprise telle quelle de NkTerrainCheck
// ─────────────────────────────────────────────────────────────────────────────
static const char *const kVertexNkSL = R"NKSL(
// Le sommet arrive DEJA EN ESPACE DE CLIP (w = 1) : la projection est faite au
// processeur. Aucune matrice ne traverse ce nuanceur, donc aucun defaut de
// matrice ne peut se faire passer pour un defaut de sable.
// ⚠️ LES NOMS D'ATTRIBUTS SONT IMPOSES : le generateur HLSL deduit la SEMANTIQUE
// du NOM (kSemanticRules, NkSLCodeGenHLSLStructs.cpp:28-38). « apos » -> POSITION,
// « anormal » -> NORMAL. Tout autre nom retomberait sur TEXCOORD<location> et le
// layout declare cote C++ cesserait de correspondre, SANS AUCUN MESSAGE.
@location(0) in vec3 aPos;
@location(1) in vec3 aNormal;

@location(0) out vec3 vNormal;

@stage(vertex)
@entry
void main() {
    vNormal     = aNormal;
    gl_Position = vec4(aPos, 1.0);
}
)NKSL";

static const char *const kFragmentNkSL = R"NKSL(
@binding(set=0, binding=0) uniform NkSableUBO { vec4 lumiere; } U;

@location(0) in vec3 vNormal;
@location(0) out vec4 fragColor;

@stage(fragment)
@entry
void main() {
    vec3  L = normalize(U.lumiere.xyz);
    float d = max(dot(normalize(vNormal), L), 0.0);
    // ⚠️ LE CANAL ROUGE EST FORCE A ZERO. Le fond est magenta (255,0,255) : avec
    // r = 0, aucun pixel du sol ne peut coincider avec le fond, quelle que soit
    // la lumiere. Le compteur devient EXACT au lieu d'etre probable.
    fragColor = vec4(0.0, 0.18 + 0.82 * d, 0.0, 1.0);
}
)NKSL";

struct SommetRendu {
		float32 pos[3];
		float32 nrm[3];
};

struct UboLumiere {
		// Lumiere RASANTE. Une lumiere zenithale eclairerait un sol plat et le
		// fond d'une orniere EXACTEMENT PAREIL (les deux ont la normale +Y) :
		// l'image serait aveugle a ce que ce banc doit montrer. Rasante, ce sont
		// les FLANCS qui payent, donc l'orniere se voit.
		float32 lumiere[4] = {0.75f, 0.28f, 0.60f, 0.f};
};

struct Scene {
		NkIDevice *device = nullptr;
		NkTextureLibrary texLib;
		NkShaderLibrary shaders;
		NkOffscreenTarget cible;
		NkBufferHandle ubo;
		NkDescSetHandle setLayout;
		NkDescSetHandle set;
		NkPipelineHandle pipe;
		uint32 largeur = 512, hauteur = 512;

		bool Monter();
		void Demonter();
		// `sommets == nullptr` => AUCUN trace. C'est la preuve du zero.
		bool RendreEtCompter(const SommetRendu *sommets, uint32 nbSommets, const uint32 *indices,
							 uint32 nbIndices, uint32 &outNonFond, const char *capture,
							 uint32 *outNuances = nullptr);
};

bool Scene::Monter() {
	NkDeviceInitInfo di;
	di.api = NkGraphicsApi::NK_GFX_API_DX11;
	di.width = 0; // pas de surface -> headless
	di.height = 0;
	device = NkDeviceFactory::Create(di);
	if (!device || !device->IsValid())
		return false;
	// TEMOIN D'IDENTITE : on LIT l'API obtenue au lieu de croire celle demandee.
	if (device->GetApi() != NkGraphicsApi::NK_GFX_API_DX11)
		return false;
	if (texLib.Init(device, nullptr) != NkRResult::NK_OK)
		return false;
	if (!shaders.Init(device, device->GetApi(), /*useNkSL=*/true))
		return false;

	NkOffscreenDesc od;
	od.width = largeur;
	od.height = hauteur;
	od.hasDepth = true;
	// ⚠️ UNORM, PAS sRGB. Le DEFAUT du struct est NK_RGBA8_SRGB : il est contre
	// nous, on l'ecrit.
	od.colorFmt = NkGPUFormat::NK_RGBA8_UNORM;
	od.readable = true;
	od.readback = true;
	od.name = NkString("NkSableCheck");
	if (!cible.Init(device, &texLib, od))
		return false;

	UboLumiere u;
	ubo = device->CreateBuffer(NkBufferDesc::Uniform(sizeof(UboLumiere)));
	if (!ubo.IsValid())
		return false;
	device->WriteBuffer(ubo, &u, sizeof(u));

	NkDescriptorSetLayoutDesc ld;
	ld.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER, RHIStage::NK_ALL_GRAPHICS);
	setLayout = device->CreateDescriptorSetLayout(ld);
	set = device->AllocateDescriptorSet(setLayout);
	NkDescriptorWrite w{};
	w.set = set;
	w.binding = 0;
	w.type = NkDescriptorType::NK_UNIFORM_BUFFER;
	w.buffer = ubo;
	w.bufferRange = sizeof(UboLumiere);
	device->UpdateDescriptorSets(&w, 1);

	::nkentseu::NkShaderHandle prog =
		shaders.CompileVF(NkString(kVertexNkSL), NkString(kFragmentNkSL), NkString("nksable"));
	::nkentseu::NkShaderHandle rhi = shaders.GetRHIHandle(prog);
	if (!rhi.IsValid())
		return false;

	NkGraphicsPipelineDesc pd;
	pd.shader = rhi;
	pd.vertexLayout.AddBinding(0, (uint32)sizeof(SommetRendu))
		.AddAttribute(0, 0, NkGPUFormat::NK_RGB32_FLOAT, 0, "POSITION", 0)
		.AddAttribute(1, 0, NkGPUFormat::NK_RGB32_FLOAT, 12, "NORMAL", 0);
	// Aucun cull : le sens d'enroulement n'est PAS ce que (t4) mesure, et un cull
	// mal oriente rendrait une image entierement magenta sans le moindre message.
	pd.rasterizer.cullMode = NkCullMode::NK_NONE;
	pd.depthStencil = NkDepthStencilDesc::Default();
	pd.renderPass = cible.GetRP();
	pd.descriptorSetLayouts.PushBack(setLayout);
	pd.debugName = "NkSableCheck";
	pipe = device->CreateGraphicsPipeline(pd);
	return pipe.IsValid();
}

void Scene::Demonter() {
	cible.Shutdown();
	shaders.Shutdown();
	texLib.Shutdown();
	if (device)
		NkDeviceFactory::Destroy(device);
	device = nullptr;
}

bool Scene::RendreEtCompter(const SommetRendu *sommets, uint32 nbSommets, const uint32 *indices,
							uint32 nbIndices, uint32 &outNonFond, const char *capture,
							uint32 *outNuances) {
	outNonFond = 0;
	if (outNuances)
		*outNuances = 0;
	NkBufferHandle vbo, ibo;
	const bool trace = (sommets != nullptr && nbSommets > 0u && indices != nullptr && nbIndices > 0u);
	if (trace) {
		vbo = device->CreateBuffer(NkBufferDesc::Vertex((uint64)nbSommets * sizeof(SommetRendu), sommets));
		ibo = device->CreateBuffer(NkBufferDesc::Index((uint64)nbIndices * sizeof(uint32), indices));
		if (!vbo.IsValid() || !ibo.IsValid())
			return false;
	}

	NkICommandBuffer *cmd = device->CreateCommandBuffer();
	if (!cmd || !cmd->Begin())
		return false;
	// FOND MAGENTA OPAQUE, jamais noir : un fond noir se confond avec un objet
	// sombre, et le compteur dirait « rien » pour « tout, mais sombre ».
	cible.BeginCapture(cmd, /*clearColor=*/true, NkVec4f{1.f, 0.f, 1.f, 1.f}, /*clearDepth=*/true);
	if (trace) {
		cmd->BindGraphicsPipeline(pipe);
		cmd->BindDescriptorSet(set, 0);
		cmd->BindVertexBuffer(0, vbo);
		cmd->BindIndexBuffer(ibo, NkIndexFormat::NK_UINT32);
		cmd->DrawIndexed(nbIndices);
	}
	cible.EndCapture(cmd);
	cmd->End();
	device->Submit(&cmd, 1);
	device->WaitIdle();

	NkVector<uint8> px;
	px.Resize(largeur * hauteur * 4u);
	if (!cible.ReadbackPixels(px.Data()))
		return false;

	uint8 vus[256];
	memset(vus, 0, sizeof(vus));
	for (uint32 k = 0; k < largeur * hauteur; ++k) {
		const uint8 r = px[k * 4u + 0u], g = px[k * 4u + 1u], b = px[k * 4u + 2u];
		if (!(r == 255u && g == 0u && b == 255u)) {
			++outNonFond;
			vus[g] = 1u;
		}
	}
	if (outNuances) {
		uint32 n = 0;
		for (uint32 g = 0; g < 256u; ++g)
			n += vus[g];
		*outNuances = n;
	}

	// La capture PNG est POUR L'OEIL DE RODOLF, pas pour le verdict. Le banc ne
	// juge AUCUNE image : un temoin par capture est structurellement aveugle a
	// toute une classe de defauts. Le verdict, lui, est le comptage.
	if (capture != nullptr)
		cible.Capture(CheminCapture(capture).CStr());

	if (trace) {
		device->DestroyBuffer(vbo);
		device->DestroyBuffer(ibo);
	}
	return true;
}

// ── La projection, faite au processeur ──────────────────────────────────────
struct Vue {
		float32 f[3];
		float32 up[3];
		float32 k;
};

static void Normaliser(float32 v[3]) {
	const float32 l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	if (l > 1e-8f) {
		v[0] /= l;
		v[1] /= l;
		v[2] /= l;
	}
}
static void Croix(const float32 a[3], const float32 b[3], float32 o[3]) {
	o[0] = a[1] * b[2] - a[2] * b[1];
	o[1] = a[2] * b[0] - a[0] * b[2];
	o[2] = a[0] * b[1] - a[1] * b[0];
}

// ⚠️ L'ECHELLE EST IMPOSEE, PAS DEDUITE DU MAILLAGE. Une projection qui recadre
// sur la boite du maillage change d'echelle entre « avant » et « apres » : les
// deux images ne seraient plus comparables, et un creux profond se verrait
// IDENTIQUE a un creux peu profond. `demiEtendue` est fixee une fois pour toutes
// les vues de cette execution.
static void Projeter(const NkVector<NkVertex3D> &tv, const Vue &vue, float32 demiEtendue,
					 NkVector<SommetRendu> &out) {
	float32 f[3] = {vue.f[0], vue.f[1], vue.f[2]};
	Normaliser(f);
	float32 up[3] = {vue.up[0], vue.up[1], vue.up[2]};
	float32 r[3];
	Croix(f, up, r);
	Normaliser(r);
	float32 u[3];
	Croix(r, f, u);
	Normaliser(u);

	const uint32 n = (uint32)tv.Size();
	out.Resize(n);
	for (uint32 i = 0; i < n; ++i) {
		const NkVec3f p = tv[i].pos;
		const float32 vx = p.x * r[0] + p.y * r[1] + p.z * r[2];
		const float32 vy = p.x * u[0] + p.y * u[1] + p.z * u[2];
		const float32 vd = p.x * f[0] + p.y * f[1] + p.z * f[2];
		SommetRendu s;
		s.pos[0] = vx / demiEtendue * vue.k;
		s.pos[1] = vy / demiEtendue * vue.k;
		// Profondeur dans [0.05, 0.95] : a l'interieur des DEUX conventions
		// ([0,1] DirectX et [-1,1] OpenGL), donc jamais ecretee par l'une ni par
		// l'autre. Le plus PROCHE (vd petit) gagne (NK_LESS, effacement 1.0).
		float32 z = 0.5f + vd / (4.f * demiEtendue);
		if (z < 0.05f)
			z = 0.05f;
		if (z > 0.95f)
			z = 0.95f;
		s.pos[2] = z;
		s.nrm[0] = tv[i].normal.x;
		s.nrm[1] = tv[i].normal.y;
		s.nrm[2] = tv[i].normal.z;
		out[i] = s;
	}
}

// Rend un maillage et journalise. `attenduNuancesMin` : 0 = pas d'exigence.
static void RendreMaillage(Scene &sc, const char *nom, const NkEditMesh &mesh, const Vue &vue,
						   float32 demiEtendue, const char *capture, uint32 *outNuances) {
	NkVector<NkVertex3D> tv;
	NkVector<uint32> ti;
	NkVector<NkEmId> tf;
	mesh.Triangulate(tv, ti, tf);
	NkVector<SommetRendu> so;
	Projeter(tv, vue, demiEtendue, so);
	uint32 nonFond = 0, nuances = 0;
	const bool ok = sc.RendreEtCompter(so.Data(), (uint32)so.Size(), ti.Data(), (uint32)ti.Size(),
									   nonFond, capture, &nuances);
	if (outNuances)
		*outNuances = nuances;
	Cas(nom, ok && nonFond > 0u,
		NkFormat("pixels non-fond={0} . nuances de vert={1} . capture={2}", nonFond, nuances,
				 CheminCapture(capture)));
}

// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char **argv) {
	logger.Pattern("%v");

	for (int a = 1; a < argc; ++a) {
		if (strncmp(argv[a], "--mutation=", 11) == 0) {
			const char *m = argv[a] + 11;
			if (strcmp(m, "depot-epsilon") == 0)
				gMutation = Mutation::DEPOT_EPSILON;
			else if (strcmp(m, "roche-couche0") == 0)
				gMutation = Mutation::ROCHE_COUCHE0;
			else if (strcmp(m, "sans-relaxation") == 0)
				gMutation = Mutation::SANS_RELAXATION;
			else if (strcmp(m, "bourrelet-ampute") == 0)
				gMutation = Mutation::BOURRELET_AMPUTE;
			else if (strcmp(m, "vide") == 0)
				gMutation = Mutation::VIDE;
			else {
				Dire("mutation inconnue : {0}", NkString(m));
				return 2;
			}
		}
	}

	Dire("== NkSableCheck -- une roue passe, le sol garde-t-il son orniere ? ==");
	Dire("   mutation en cours : {0}", NkString(NomMutation(gMutation)));
	Dire("   AUCUNE fenetre n'est ouverte par ce banc (device DX11 headless).");
	Dire("");

	if (gMutation == Mutation::VIDE) {
		Dire("mutation `vide` : rien n'est execute. Le NOMBRE DE CAS doit s'effondrer.");
		Dire("");
		Dire("== {0} cas, {1} rouge(s) ==", gCas, gEchecs);
		return gEchecs == 0u ? 0 : 1;
	}

	const NkTerrainParams tp = ParamsTerrain();
	NkTerrainSableParams sp = ParamsSable();
	const float32 aireCellule = tp.pasX * tp.pasZ;

	// ═════════════════════════════════════════════════════════════════════
	//  (t0) LA CONDITION D'ESSAI ELLE-MEME
	// ═════════════════════════════════════════════════════════════════════
	Dire("-- (t0) la condition d'essai : on mesure le terrain NU avant d'y toucher --");

	Terrain plat, pente;
	if (!MonterTerrain("t0.1 terrain PLAT construit", false, plat))
		return 2;
	if (!MonterTerrain("t0.2 terrain EN PENTE construit", true, pente))
		return 2;

	// La pente du sol NU. « Un banc a mesure une pente de 10 degres sur un sol
	// parfaitement plat, avec des chiffres parfaitement coherents entre eux. »
	{
		const float32 pNue = NkSablePenteMaxDeg(plat.champ);
		Cas("t0.3 le terrain plat est PLAT", pNue == 0.f,
			NkFormat("pente max du sol nu = {0} deg (attendu 0 EXACT : tous les niveaux valent 0)",
					 pNue));
	}
	{
		// ATTENDU ECRIT AVANT ET DERIVE : la hauteur monte de hauteurParNiveau
		// par cellule sur une distance pasX, donc tan = hauteurParNiveau / pasX.
		const float32 tanAttendu = tp.hauteurParNiveau / tp.pasX;
		const float32 attendu = (float32)(atan((double)tanAttendu) * kRadEnDeg);
		const float32 mesure = NkSablePenteMaxDeg(pente.champ);
		const float32 ecart = mesure > attendu ? mesure - attendu : attendu - mesure;
		Cas("t0.4 la pente du terrain incline est celle de la formule", ecart <= 1e-3f,
			NkFormat("mesure={0} deg . attendu=atan(hauteurParNiveau/pasX)={1} deg . ecart={2} "
					 "(exigence <= 1e-3)",
					 mesure, attendu, ecart));
	}

	// La base recopiee du maillage est-elle celle que la loi recalcule ?
	// C'est la question du FMA, posee au lieu d'etre supposee : le compilateur a
	// le droit de fusionner `hauteurMin + niveau * pas` dans une unite de
	// compilation et pas dans l'autre. Si ce critere rougissait, ce serait la
	// reponse -- pas un defaut du sable.
	{
		NkTerrainSable copie = plat.champ;
		const NkSableStatut st = NkSableAdopterBaseDuMaillage(plat.mesh, copie);
		const uint32 d = DiffAuBit(copie.base.Data(), plat.champ.base.Data(), kN * kM);
		Cas("t0.5 base recalculee == base recopiee du maillage AU BIT",
			st == NkSableStatut::NK_OK && d == 0u,
			NkFormat("statut={0} . sommets differant AU BIT={1}/{2} (question du FMA, posee et non "
					 "supposee)",
					 NkString(NkSableStatutNom(st)), d, kN * kM));
	}
	{
		NkTerrainSable copie = pente.champ;
		const NkSableStatut st = NkSableAdopterBaseDuMaillage(pente.mesh, copie);
		const uint32 d = DiffAuBit(copie.base.Data(), pente.champ.base.Data(), kN * kM);
		Cas("t0.6 idem sur le terrain EN PENTE", st == NkSableStatut::NK_OK && d == 0u,
			NkFormat("statut={0} . sommets differant AU BIT={1}/{2}", NkString(NkSableStatutNom(st)), d,
					 kN * kM));
	}

	// ── LE CHEMIN ZERO, ISOLE ───────────────────────────────────────────
	// C'est LE cas qui justifie que `NkSableHauteurCombinee` ne soit pas une
	// simple addition. `volatile` pour que le compilateur ne plie pas le calcul.
	{
		volatile float32 mz = -0.f;
		volatile float32 z = 0.f;
		const float32 parAddition = (float32)mz + (float32)z;
		const float32 parCombinee = NkSableHauteurCombinee((float32)mz, (float32)z);
		const bool ok = (Bits(parCombinee) == Bits(-0.f)) && (Bits(parAddition) != Bits(-0.f));
		Cas("t0.7 le chemin zero merite d'exister : (-0.f)+(0.f) N'EST PAS (-0.f)", ok,
			NkFormat("bits(-0.f)={0} . bits((-0.f)+(0.f))={1} . bits(Combinee(-0.f,0.f))={2}",
					 Bits(-0.f), Bits(parAddition), Bits(parCombinee)));
	}

	// ── LA SCENE, ET LE ZERO DU COMPTEUR AVANT TOUT LE RESTE ────────────
	Scene sc;
	bool scenePrete = sc.Monter();
	if (!scenePrete) {
		Cas("t0.8 scene hors-ecran DX11 headless", false,
			NkString("montage impossible : (t4) sera IGNORE, les criteres processeur restent"));
	} else {
		uint32 zero = 0, nuancesZero = 0;
		const bool ok = sc.RendreEtCompter(nullptr, 0u, nullptr, 0u, zero, nullptr, &nuancesZero);
		Cas("t0.8 LE COMPTEUR PROUVE SON ZERO (meme cible, meme effacement, aucun trace)",
			ok && zero == 0u && nuancesZero == 0u,
			NkFormat("pixels non-fond={0} (attendu 0 EXACT) . nuances={1} (attendu 0) . cible "
					 "{2}x{3} UNORM, fond magenta",
					 zero, nuancesZero, sc.largeur, sc.hauteur));
	}
	Dire("");

	// ═════════════════════════════════════════════════════════════════════
	//  (t1) LE NEGATIF CAPITAL — sans deformation, la hauteur est celle de
	//       l'image, AU BIT
	// ═════════════════════════════════════════════════════════════════════
	Dire("-- (t1) LE NEGATIF CAPITAL : sans deformation, la hauteur vaut celle de l'image AU BIT --");

	// MUTATION : un seul creux d'un ULP dans un champ cense etre vierge.
	if (gMutation == Mutation::DEPOT_EPSILON) {
		plat.champ.depot[(kM / 2u) * kN + (kN / 2u)] = 1.401298464e-45f; // plus petit denormal
		plat.champ.combine[(kM / 2u) * kN + (kN / 2u)] =
			NkSableHauteurCombinee(plat.champ.base[(kM / 2u) * kN + (kN / 2u)],
								   plat.champ.depot[(kM / 2u) * kN + (kN / 2u)]);
		pente.champ.depot[(kM / 2u) * kN + (kN / 2u)] = 1.401298464e-45f;
		pente.champ.combine[(kM / 2u) * kN + (kN / 2u)] =
			NkSableHauteurCombinee(pente.champ.base[(kM / 2u) * kN + (kN / 2u)],
								   pente.champ.depot[(kM / 2u) * kN + (kN / 2u)]);
	}

	{
		NkEditMesh copie = plat.mesh;
		const NkSableStatut st = NkSableAppliquerAuMaillage(plat.champ, copie);
		uint32 d = 0;
		for (uint32 k = 0; k < kN * kM; ++k)
			if (Bits(copie.verts[k].pos.y) != Bits(plat.baseTemoin[k]))
				++d;
		Cas("t1.1 champ VIERGE sur terrain plat -> maillage intact AU BIT",
			st == NkSableStatut::NK_OK && d == 0u,
			NkFormat("statut={0} . sommets modifies AU BIT={1}/{2} (attendu 0)",
					 NkString(NkSableStatutNom(st)), d, kN * kM));
	}
	{
		// Un terrain PLAT A ZERO cacherait une couche qui fuit : 0 + eps == eps,
		// et l'ecart absolu resterait minuscule. Sur une pente, chaque sommet a
		// une valeur differente, donc toute fuite se voit.
		NkEditMesh copie = pente.mesh;
		const NkSableStatut st = NkSableAppliquerAuMaillage(pente.champ, copie);
		uint32 d = 0;
		for (uint32 k = 0; k < kN * kM; ++k)
			if (Bits(copie.verts[k].pos.y) != Bits(pente.baseTemoin[k]))
				++d;
		Cas("t1.2 champ VIERGE sur terrain EN PENTE -> maillage intact AU BIT",
			st == NkSableStatut::NK_OK && d == 0u,
			NkFormat("statut={0} . sommets modifies AU BIT={1}/{2} (attendu 0)",
					 NkString(NkSableStatutNom(st)), d, kN * kM));
	}
	{
		// Creuser puis reinitialiser doit RENDRE LES MEMES OCTETS, pas des
		// octets proches.
		NkTerrainSable c = pente.champ;
		NkVector<float32> avant;
		avant.Resize(kN * kM);
		for (uint32 k = 0; k < kN * kM; ++k)
			avant[k] = c.combine[k];
		NkSablePassageRoue(c, sp, kTraceAX, kTraceAZ, kTraceBX, kTraceBZ, kDemiLargeur,
						   kLongueurContact, kMasseRoue);
		const uint32 dPendant = DiffAuBit(c.combine.Data(), avant.Data(), kN * kM);
		NkSableReinitialiser(c);
		const uint32 dApres = DiffAuBit(c.combine.Data(), avant.Data(), kN * kM);
		Cas("t1.3 creuser PUIS reinitialiser -> retour AU BIT", dPendant > 0u && dApres == 0u,
			NkFormat("cellules changees pendant={0} (doit etre > 0, sinon on ne mesure rien) . "
					 "apres reinitialisation={1}/{2} (attendu 0)",
					 dPendant, dApres, kN * kM));
	}
	{
		// Une empreinte ENTIEREMENT hors de la grille ne doit toucher aucun octet
		// -- et surtout pas se faire plaquer sur le bord, ce qui ressemblerait a
		// un resultat.
		NkTerrainSable c = plat.champ;
		NkVector<float32> avant;
		avant.Resize(kN * kM);
		for (uint32 k = 0; k < kN * kM; ++k)
			avant[k] = c.combine[k];
		float32 vol = -1.f;
		const NkSableStatut st = NkSableEmpreinte(c, sp, 50.f, 50.f, 0.2f, 0.1f, &vol);
		const uint32 d = DiffAuBit(c.combine.Data(), avant.Data(), kN * kM);
		Cas("t1.4 empreinte HORS grille -> aucun octet touche, aucun plaquage au bord",
			st == NkSableStatut::NK_OK && d == 0u && vol == 0.f,
			NkFormat("statut={0} . cellules changees={1} (attendu 0) . volume retire={2} (attendu 0)",
					 NkString(NkSableStatutNom(st)), d, vol));
	}
	Dire("");

	// ═════════════════════════════════════════════════════════════════════
	//  (t2) UN CORPS CREUSE
	// ═════════════════════════════════════════════════════════════════════
	Dire("-- (t2) UN CORPS CREUSE : l'enfoncement est DERIVE, jamais donne --");

	// ATTENDU ECRIT AVANT LA MESURE, DERIVE DES PARAMETRES DU BANC.
	const float32 aireContact = 2.f * kDemiLargeur * kLongueurContact;
	const float32 pression = kMasseRoue * sp.gravite / aireContact;
	const float32 dAttendu = NkSableEnfoncement(sp, kMasseRoue, aireContact);
	Dire("  attendus ECRITS AVANT : aire de contact = 2*{0}*{1} = {2} m2", kDemiLargeur,
		 kLongueurContact, aireContact);
	Dire("                          pression = {0}*{1}/{2} = {3} Pa", kMasseRoue, sp.gravite,
		 aireContact, pression);
	Dire("                          enfoncement = (p - portance)/raideur = {0} m", dAttendu);
	Dire("                          masse limite (p == portance) = {0} kg",
		 sp.portance * aireContact / sp.gravite);

	NkTerrainSable sol = plat.champ; // terrain plat, sable partout
	float32 volRetire = 0.f, volRedepose = 0.f, dRendu = 0.f;
	{
		const NkSableStatut st =
			NkSablePassageRoue(sol, sp, kTraceAX, kTraceAZ, kTraceBX, kTraceBZ, kDemiLargeur,
							   kLongueurContact, kMasseRoue, &dRendu, &volRetire, &volRedepose);
		Cas("t2.1 l'enfoncement RENDU est celui de la loi publique, AU BIT",
			st == NkSableStatut::NK_OK && Bits(dRendu) == Bits(dAttendu),
			NkFormat("statut={0} . rendu={1} m . recalcule par le banc={2} m . bits {3} vs {4}",
					 NkString(NkSableStatutNom(st)), dRendu, dAttendu, Bits(dRendu), Bits(dAttendu)));
	}

	// MUTATION : la moitie du bourrelet disparait.
	if (gMutation == Mutation::BOURRELET_AMPUTE) {
		for (uint32 k = 0; k < kN * kM; ++k)
			if (sol.depot[k] > 0.f) {
				sol.depot[k] *= 0.5f;
				sol.combine[k] = NkSableHauteurCombinee(sol.base[k], sol.depot[k]);
			}
	}

	{
		// Au CENTRE de la trace : q = 0, donc lisser(0,0,1) = 0, donc
		// creuse = profondeur * 1 * deformabilite = d. Le depot vaut -d EXACTEMENT.
		const uint32 iC = kN / 2u, jC = kM / 2u;
		const float32 dep = sol.depot[jC * kN + iC];
		const float32 ecart = fabsf(dep + dAttendu);
		Cas("t2.2 au centre de la trace, le sol descend d'exactement l'enfoncement", ecart == 0.f,
			NkFormat("depot au centre={0} m . attendu=-{1} m . ecart={2} (attendu 0 EXACT : q=0 "
					 "donne lisser=0, donc creuse = profondeur)",
					 dep, dAttendu, ecart));
	}
	{
		// CONSERVATION DE LA MATIERE. Borne ECRITE AVANT et DERIVEE : le
		// bourrelet est une somme de `nbCellules` termes en float32, chacun
		// entache d'au plus eps = 1,19e-7 en relatif ; l'erreur relative de la
		// somme est donc bornee par nbCellules * eps. On compte les cellules du
		// bourrelet au lieu de les supposer.
		uint32 nbBourrelet = 0;
		for (uint32 k = 0; k < kN * kM; ++k)
			if (sol.depot[k] > 0.f)
				++nbBourrelet;
		const float32 borne = (float32)nbBourrelet * 1.1920929e-7f;
		const float32 ecartRel =
			volRetire > 0.f ? fabsf(volRedepose - volRetire) / volRetire : 1.f;
		Cas("t2.3 le volume redepose en bourrelet vaut le volume retire",
			volRetire > 0.f && ecartRel <= borne,
			NkFormat("retire={0} m3 . redepose={1} m3 . ecart relatif={2} . borne DERIVEE "
					 "({3} cellules * 1.19e-7)={4}",
					 volRetire, volRedepose, ecartRel, nbBourrelet, borne));
	}
	{
		// Et le bilan global, mesure independamment par le champ lui-meme.
		const float32 bilan = NkSableVolumeDepot(sol);
		const float32 borne = volRetire * 1e-4f;
		Cas("t2.4 le bilan de matiere du champ est nul", fabsf(bilan) <= borne,
			NkFormat("volume total du depot={0} m3 . borne=1e-4 * volume retire={1} m3 . (la somme "
					 "est faite en float64 dans NkSableVolumeDepot : l'annulation catastrophique "
					 "mesurerait sinon l'arithmetique du banc)",
					 bilan, borne));
	}
	{
		// LE BOURRELET EXISTE. Une orniere sans crete n'est pas une orniere,
		// c'est un trou. On regarde a une distance comprise entre demiLargeur et
		// demiLargeur * largeurBourrelet, perpendiculairement a la trace.
		const uint32 iC = kN / 2u;
		const float32 zCrete = 0.5f * kDemiLargeur * (1.f + sp.largeurBourrelet);
		const int32 jCrete = (int32)((zCrete - sol.z0) / sol.pasZ + 0.5f);
		float32 hCrete = 0.f;
		if (jCrete >= 0 && jCrete < (int32)kM)
			hCrete = sol.depot[(uint32)jCrete * kN + iC];
		Cas("t2.5 l'orniere a une CRETE, elle n'est pas un trou", hCrete > 0.f,
			NkFormat("depot a z={0} m (milieu de l'anneau)={1} m (attendu > 0)", zCrete, hCrete));
	}
	{
		// NEGATIF : LA ROCHE NE SE CREUSE PAS.
		NkTerrainSable roche = plat.champ;
		NkSableDeformabiliteUniforme(roche, 0.f);
		NkVector<float32> avant;
		avant.Resize(kN * kM);
		for (uint32 k = 0; k < kN * kM; ++k)
			avant[k] = roche.combine[k];
		float32 vr = -1.f, vd = -1.f, dd = -1.f;
		NkSablePassageRoue(roche, sp, kTraceAX, kTraceAZ, kTraceBX, kTraceBZ, kDemiLargeur,
						   kLongueurContact, kMasseRoue, &dd, &vr, &vd);
		const uint32 diff = DiffAuBit(roche.combine.Data(), avant.Data(), kN * kM);
		Cas("t2.6 NEGATIF : corps pose sur de la ROCHE -> AUCUN creusement, champ intact AU BIT",
			diff == 0u && vr == 0.f && vd == 0.f,
			NkFormat("cellules changees={0} (attendu 0) . volume retire={1} . redepose={2} . "
					 "enfoncement calcule={3} m (il reste NON NUL : c'est le SOL qui refuse, pas "
					 "la loi)",
					 diff, vr, vd, dd));
	}
	{
		// NEGATIF : SOUS LA PORTANCE, RIEN NE S'ENFONCE. La masse limite est
		// DERIVEE (portance * aire / g), pas recopiee.
		NkTerrainSable leger = plat.champ;
		NkVector<float32> avant;
		avant.Resize(kN * kM);
		for (uint32 k = 0; k < kN * kM; ++k)
			avant[k] = leger.combine[k];
		float32 dd = -1.f, vr = -1.f;
		NkSablePassageRoue(leger, sp, kTraceAX, kTraceAZ, kTraceBX, kTraceBZ, kDemiLargeur,
						   kLongueurContact, kMasseLegere, &dd, &vr);
		const uint32 diff = DiffAuBit(leger.combine.Data(), avant.Data(), kN * kM);
		const float32 masseLimite = sp.portance * aireContact / sp.gravite;
		Cas("t2.7 NEGATIF : masse SOUS la portance -> enfoncement 0, champ intact AU BIT",
			dd == 0.f && diff == 0u && vr == 0.f,
			NkFormat("masse={0} kg . masse limite DERIVEE={1} kg . enfoncement={2} . cellules "
					 "changees={3} (attendu 0)",
					 kMasseLegere, masseLimite, dd, diff));
	}
	{
		// LA ROCHE EST LA COUCHE 2, PAS LA COUCHE 0. Le lot demandait « couche 0
		// dominante » ; NkTerrainSplat.h dit HERBE=0, TERRE=1, ROCHE=2, NEIGE=3.
		// Ce critere est celui qui aurait rougi si l'on avait code l'enonce.
		NkTerrainSable c = plat.champ;
		NkVector<NkTerrainPoids> poids;
		poids.Resize(kN * kM);
		for (uint32 k = 0; k < kN * kM; ++k) {
			NkTerrainPoids w;
			w.w[0] = 0.f;
			w.w[1] = 0.f;
			w.w[2] = 0.f;
			w.w[3] = 0.f;
			// Moitie gauche : ROCHE pure. Moitie droite : HERBE pure.
			const uint32 i = k % kN;
			if (i < kN / 2u)
				w.w[NK_TERRAIN_ROCHE] = 1.f;
			else
				w.w[NK_TERRAIN_HERBE] = 1.f;
			poids[k] = w;
		}
		// MUTATION : lire la deformabilite sur la couche 0 au lieu de la couche 2.
		if (gMutation == Mutation::ROCHE_COUCHE0) {
			for (uint32 k = 0; k < kN * kM; ++k) {
				float32 d = 1.f - poids[k].w[0];
				c.deformabilite[k] = d;
			}
		} else {
			NkSableDeformabiliteDepuisPoids(poids.Data(), kN * kM, c);
		}
		uint32 rocheDure = 0, herbeMolle = 0;
		for (uint32 k = 0; k < kN * kM; ++k) {
			const uint32 i = k % kN;
			if (i < kN / 2u) {
				if (c.deformabilite[k] == 0.f)
					++rocheDure;
			} else {
				if (c.deformabilite[k] == 1.f)
					++herbeMolle;
			}
		}
		const uint32 nRoche = (kN / 2u) * kM;
		const uint32 nHerbe = (kN - kN / 2u) * kM;
		Cas("t2.8 la ROCHE est la couche 2 : roche pure -> 0, herbe pure -> 1",
			rocheDure == nRoche && herbeMolle == nHerbe,
			NkFormat("roche pure a deformabilite 0 : {0}/{1} . herbe pure a deformabilite 1 : "
					 "{2}/{3} (l'enonce du lot disait `couche 0` : ce critere l'aurait pris)",
					 rocheDure, nRoche, herbeMolle, nHerbe));
	}
	Dire("");
	ImprimerProfil(sol, kN / 2u);
	EcrireCarteDepot(sol, "nksable_carte_orniere.png");
	Dire("");

	// ═════════════════════════════════════════════════════════════════════
	//  (t3) LE TALUS NATUREL
	// ═════════════════════════════════════════════════════════════════════
	Dire("-- (t3) LE TALUS : un tas plus raide que son angle de repos DOIT s'effondrer --");

	// ATTENDU ECRIT AVANT : un cone LINEAIRE de rayon R et de hauteur H a une
	// pente de atan(H/R) partout. Elle ne depend pas du pas d'echantillonnage :
	// le denivele par cellule vaut (H/R)*pas, et pas est au denominateur.
	const float32 penteConeAttendue = (float32)(atan((double)(kConeHauteur / kConeRayon)) * kRadEnDeg);
	const float32 dhMaxX = NkSableDeniveleMax(sp, kPas);
	Dire("  attendus ECRITS AVANT : pente du cone pose = atan({0}/{1}) = {2} deg", kConeHauteur,
		 kConeRayon, penteConeAttendue);
	Dire("                          denivele max tolere a {0} deg sur un pas de {1} m = {2} m",
		 sp.angleReposDeg, kPas, dhMaxX);

	NkTerrainSable tas = plat.champ;
	NkSablePoserCone(tas, 0.f, 0.f, kConeRayon, kConeHauteur);
	const float32 penteTasPosee = NkSablePenteMaxDeg(tas);
	const float32 volTasPose = NkSableVolumeDepot(tas);
	{
		const float32 ecart = fabsf(penteTasPosee - penteConeAttendue);
		Cas("t3.1 le cone pose a la pente que la formule dit", ecart <= 1.f,
			NkFormat("mesuree={0} deg . attendue=atan(H/R)={1} deg . ecart={2} (exigence <= 1 deg : "
					 "l'echantillonnage discret peut depasser legerement au sommet)",
					 penteTasPosee, penteConeAttendue, ecart));
	}
	EcrireCarteDepot(tas, "nksable_carte_tas_pose.png");

	NkTerrainSable tasRelaxe = tas;
	uint32 iterations = 0;
	float32 penteFinale = penteTasPosee;
	if (gMutation != Mutation::SANS_RELAXATION)
		iterations = NkSableRelaxer(tasRelaxe, sp, kIterationsMax, &penteFinale);
	{
		// DEUX conditions, et la seconde est indispensable : une pente finale
		// basse obtenue en ATTEIGNANT le plafond d'iterations ne prouverait pas
		// la convergence, elle prouverait qu'on s'est arrete en chemin.
		//
		// ⚠️ C'EST CE CRITERE QUI A TROUVE UN VRAI DEFAUT, PREMIERE EXECUTION.
		// Le relaxeur ne terminait JAMAIS : chaque passe ne retire qu'une
		// fraction de l'exces, donc l'ecart a l'angle de repos decroit
		// geometriquement sans jamais l'atteindre -- 33,0003 deg apres 4000
		// iterations, plafond atteint. Un banc qui aurait exige « pente <= 33 + un
		// peu » aurait rendu VERT sur une boucle qui ne termine pas.
		//
		// L'ATTENDU EST DERIVE, PAS RECOPIE : la relaxation ne garantit pas
		// l'angle de repos exact mais l'angle ELARGI de la tolerance d'arret,
		// c'est-a-dire atan(dhMax * (1 + tolerance) / pas). `NkSablePenteGarantieDeg`
		// l'expose ; si la tolerance change, l'attendu suit.
		const float32 garantie = NkSablePenteGarantieDeg(sp, kPas);
		const bool converge = (iterations > 0u && iterations < kIterationsMax);
		Cas("t3.2 le tas S'EFFONDRE jusqu'a son angle de repos, et il a CONVERGE",
			converge && penteFinale <= garantie,
			NkFormat("pente posee={0} deg -> finale={1} deg . garantie DERIVEE (angle de repos {2} "
					 "deg elargi de la tolerance {3}) = {4} deg . iterations={5}/{6} (converge = "
					 "s'arrete AVANT le plafond)",
					 penteTasPosee, penteFinale, sp.angleReposDeg, sp.toleranceExcesRelative, garantie,
					 iterations, kIterationsMax));
	}
	{
		// LA RELAXATION NE CREE NI NE DETRUIT DE SABLE. Chaque transfert retire x
		// d'un cote et ajoute x de l'autre : la conservation est une propriete de
		// la forme du code, et on la mesure quand meme.
		const float32 volApres = NkSableVolumeDepot(tasRelaxe);
		const float32 ecartRel = volTasPose != 0.f ? fabsf(volApres - volTasPose) / fabsf(volTasPose) : 1.f;
		Cas("t3.3 la relaxation conserve le volume", ecartRel <= 1e-4f,
			NkFormat("avant={0} m3 . apres={1} m3 . ecart relatif={2} (exigence <= 1e-4)", volTasPose,
					 volApres, ecartRel));
	}
	EcrireCarteDepot(tasRelaxe, "nksable_carte_tas_effondre.png");
	{
		// NEGATIF CAPITAL DE (t3) : ANGLE DE REPOS A 90 DEG -> RIEN NE BOUGE.
		// C'est ici que le garde anti-tanf paye : sans lui, un denivele maximal
		// NEGATIF rendrait toute paire excedentaire et le tas s'effondrerait
		// COMPLETEMENT -- l'inverse exact de ce qu'on exige.
		NkTerrainSableParams p90 = sp;
		p90.angleReposDeg = 90.f;
		NkTerrainSable t90 = tas;
		NkVector<float32> avant;
		avant.Resize(kN * kM);
		for (uint32 k = 0; k < kN * kM; ++k)
			avant[k] = t90.depot[k];
		float32 pf = -1.f;
		const uint32 it = NkSableRelaxer(t90, p90, kIterationsMax, &pf);
		const uint32 diff = DiffAuBit(t90.depot.Data(), avant.Data(), kN * kM);
		Cas("t3.4 NEGATIF CAPITAL : angle de repos a 90 deg -> le tas NE s'effondre PAS, AU BIT",
			diff == 0u && it == 1u && Bits(pf) == Bits(penteTasPosee),
			NkFormat("cellules changees={0} (attendu 0) . iterations={1} (attendu 1 : un tour sans "
					 "rien trouver) . pente finale={2} deg vs posee={3} deg . denivele max a 90 "
					 "deg={4}",
					 diff, it, pf, penteTasPosee, NkSableDeniveleMax(p90, kPas)));
	}
	{
		// CONTRE-NEGATIF, ET IL EST INDISPENSABLE. Sans lui, un relaxeur qui ne
		// fait STRICTEMENT RIEN passerait t3.4 avec succes. A 0 deg, tout
		// denivele est excedentaire : le tas doit s'aplatir.
		NkTerrainSableParams p0 = sp;
		p0.angleReposDeg = 0.f;
		NkTerrainSable t0 = tas;
		float32 pf = -1.f;
		const uint32 it = NkSableRelaxer(t0, p0, kIterationsMax, &pf);
		Cas("t3.5 CONTRE-NEGATIF : angle de repos a 0 deg -> le tas s'APLATIT (sinon t3.4 ne "
			"garde rien)",
			pf < 1.f && it > 1u,
			NkFormat("pente finale={0} deg (attendu < 1) . iterations={1} (attendu > 1) . denivele "
					 "max a 0 deg={2}",
					 pf, it, NkSableDeniveleMax(p0, kPas)));
	}
	{
		// NEGATIF : LA ROCHE NE COULE PAS.
		NkTerrainSable tRoche = tas;
		NkSableDeformabiliteUniforme(tRoche, 0.f);
		NkVector<float32> avant;
		avant.Resize(kN * kM);
		for (uint32 k = 0; k < kN * kM; ++k)
			avant[k] = tRoche.depot[k];
		float32 pf = -1.f;
		const uint32 it = NkSableRelaxer(tRoche, sp, kIterationsMax, &pf);
		const uint32 diff = DiffAuBit(tRoche.depot.Data(), avant.Data(), kN * kM);
		Cas("t3.6 NEGATIF : un tas de ROCHE ne s'effondre pas", diff == 0u && it == 1u,
			NkFormat("cellules changees={0} (attendu 0) . iterations={1} . pente finale={2} deg "
					 "(inchangee)",
					 diff, it, pf));
	}

	// ── LA LEVRE DE L'ORNIERE SE RABOTE — ET C'EST LE PROFIL QUI L'A DIT ────
	// Le profil transversal imprime plus haut montre une MARCHE au bord de la
	// trace : le depot passe de -0,009 m a +0,113 m en UNE cellule, soit une
	// pente de 80 deg. C'est correct en sortie du creusement -- mais du sable ne
	// tient pas 80 deg. C'est exactement ce que (t3) existe pour corriger, et les
	// deux moities du lot se rejoignent ici : une roue creuse, puis la levre
	// s'effondre a l'angle de repos.
	NkTerrainSable orniereStable = sol;
	float32 penteOrniereAvant = NkSablePenteMaxDeg(sol);
	float32 penteOrniereApres = penteOrniereAvant;
	uint32 itOrniere = 0;
	if (gMutation != Mutation::SANS_RELAXATION)
		itOrniere = NkSableRelaxer(orniereStable, sp, kIterationsMax, &penteOrniereApres);
	{
		const float32 garantie = NkSablePenteGarantieDeg(sp, kPas);
		Cas("t3.7 la LEVRE de l'orniere se rabote a l'angle de repos",
			itOrniere > 0u && itOrniere < kIterationsMax && penteOrniereApres <= garantie &&
				penteOrniereAvant > garantie,
			NkFormat("pente de l'orniere brute={0} deg (elle DOIT depasser, sinon on ne mesure "
					 "rien) -> apres talus={1} deg . garantie DERIVEE={2} deg . iterations={3}",
					 penteOrniereAvant, penteOrniereApres, garantie, itOrniere));
	}
	{
		// Et le creux SURVIT au rabotage : une orniere rabotee jusqu'a disparaitre
		// ne serait plus une orniere. Attendu ECRIT AVANT : le fond remonte, mais
		// il reste sous la base -- on exige qu'il garde au moins la moitie de son
		// enfoncement.
		const float32 fond = orniereStable.depot[(kM / 2u) * kN + (kN / 2u)];
		Cas("t3.7b le creux SURVIT au rabotage", fond < -0.5f * dAttendu,
			NkFormat("fond de l'orniere apres talus={0} m . enfoncement initial=-{1} m . exigence "
					 "< -{2} m (la moitie)",
					 fond, dAttendu, 0.5f * dAttendu));
	}
	EcrireCarteDepot(orniereStable, "nksable_carte_orniere_stable.png");
	Dire("  profil transversal APRES rabotage de la levre :");
	ImprimerProfil(orniereStable, kN / 2u);
	Dire("");

	// ═════════════════════════════════════════════════════════════════════
	//  (t4) CA SE VOIT
	// ═════════════════════════════════════════════════════════════════════
	Dire("-- (t4) CA SE VOIT : une SUITE d'images, pas un chiffre --");
	if (!scenePrete) {
		Dire("  scene non montee : (t4) est IGNORE. Les cartes de deformation, elles, sont ecrites "
			 "(elles ne demandent aucun GPU).");
	} else {
		// L'ECHELLE EST IMPOSEE ET COMMUNE AUX QUATRE VUES : sans cela, un creux
		// profond et un creux peu profond rendraient la meme image.
		const float32 demiEtendue = 0.5f * (float32)(kN - 1u) * kPas; // 1,28 m
		Vue oblique;
		oblique.f[0] = 0.45f;
		oblique.f[1] = -0.55f;
		oblique.f[2] = 0.70f;
		oblique.up[0] = 0.f;
		oblique.up[1] = 1.f;
		oblique.up[2] = 0.f;
		oblique.k = 0.92f;

		uint32 nuancesNu = 0, nuancesOrniere = 0, nuancesTas = 0, nuancesEffondre = 0,
			   nuancesStable = 0;

		RendreMaillage(sc, "t4.1 le sol NU", plat.mesh, oblique, demiEtendue, "nksable_1_sol_nu.png",
					   &nuancesNu);

		NkEditMesh mOrniere = plat.mesh;
		NkSableAppliquerAuMaillage(sol, mOrniere);
		RendreMaillage(sc, "t4.2 le sol APRES le passage de la roue", mOrniere, oblique, demiEtendue,
					   "nksable_2_orniere.png", &nuancesOrniere);

		NkEditMesh mStable = plat.mesh;
		NkSableAppliquerAuMaillage(orniereStable, mStable);
		RendreMaillage(sc, "t4.2b l'orniere APRES rabotage de la levre", mStable, oblique, demiEtendue,
					   "nksable_2b_orniere_stable.png", &nuancesStable);

		NkEditMesh mTas = plat.mesh;
		NkSableAppliquerAuMaillage(tas, mTas);
		RendreMaillage(sc, "t4.3 le tas POSE (45 deg)", mTas, oblique, demiEtendue,
					   "nksable_3_tas_pose.png", &nuancesTas);

		NkEditMesh mTasR = plat.mesh;
		NkSableAppliquerAuMaillage(tasRelaxe, mTasR);
		RendreMaillage(sc, "t4.4 le tas EFFONDRE (33 deg)", mTasR, oblique, demiEtendue,
					   "nksable_4_tas_effondre.png", &nuancesEffondre);

		// ⚠️ LE CRITERE QUI MORD N'EST PAS « il y a des pixels », C'EST « il y en
		// a PLUS DE NUANCES qu'avant ». Un sol plat eclaire par une lumiere fixe
		// n'a QU'UNE nuance : si l'orniere n'en ajoutait aucune, la deformation
		// ne serait pas arrivee jusqu'aux normales -- et l'image serait
		// identique, sans qu'aucun chiffre de (t1)(t2)(t3) ne bouge.
		Cas("t4.5 l'orniere AJOUTE des nuances au sol nu", nuancesOrniere > nuancesNu,
			NkFormat("nuances sol nu={0} (un plan eclaire n'en a qu'UNE) . avec orniere={1} "
					 "(attendu strictement plus)",
					 nuancesNu, nuancesOrniere));
		Cas("t4.6 le tas EFFONDRE n'est pas le tas POSE", nuancesEffondre != nuancesTas ||
															 nuancesTas == 0u,
			NkFormat("nuances tas pose={0} . tas effondre={1} (deux images identiques diraient que "
					 "la relaxation n'a pas atteint le maillage)",
					 nuancesTas, nuancesEffondre));
		sc.Demonter();
		Dire("  scene demontee. Aucune fenetre n'a ete ouverte ni fermee.");
	}

	Dire("");
	Dire("== {0} cas, {1} rouge(s) . mutation={2} ==", gCas, gEchecs, NkString(NomMutation(gMutation)));
	if (gMutation != Mutation::AUCUNE)
		Dire("   (execution MUTEE : ses captures portent le prefixe MUTE_, ce ne sont PAS le produit)");
	return gEchecs == 0u ? 0 : 1;
}
