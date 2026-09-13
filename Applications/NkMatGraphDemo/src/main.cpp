// =============================================================================
// NkMatGraphDemo — le graphe de materiaux REND VRAIMENT, et on le prouve par des
//                  valeurs calculables, pas par une image.
// =============================================================================
// POURQUOI CE BANC EXISTE, alors que NkMatGraphCheck compile deja sur quatre
// backends : compiler n'est pas rendre. Un shader peut traverser GL, Vulkan,
// DX11 et DX12 sans qu'aucune des valeurs declarees dans le graphe n'arrive au
// pixel — un defaut de prise perdu, un canal permute, un facteur de melange
// ignore. Rien de tout cela n'empeche la compilation.
//
// ⚠️ ET CE BANC NE JUGE PAS PAR L'IMAGE. C'est la lecon la plus chere de la
// nuit du 21 au 22/08 : en verifiant qu'un temoin par signature d'image saurait
// attraper une erreur de binding, on a ecrit VOLONTAIREMENT un descripteur sur
// un binding absent du layout. Resultat : aucune erreur, aucun journal, et les
// cinq signatures d'image IDENTIQUES. **Une ressource qui n'arrive pas ne change
// pas l'image tant que personne ne la lit.** Un temoin par capture est donc
// structurellement aveugle a toute une classe de defauts.
//
// CE QU'ON MESURE A LA PLACE : la DIFFERENCE ENTRE CANAUX d'un meme pixel, et
// la difference entre deux rendus. Les deux sont calculables A PARTIR DU GRAPHE,
// sans rien savoir du modele d'eclairage — et c'est tout l'interet.
//
//   Le shader engendre finit par :
//       fragColor = diffuse + specular + emission
//   Avec un noeud Emission, le graphe impose albedo = 0 et metallic = 0. Donc :
//       diffuse   = albedo * (...)              = 0
//       specColor = mix(vec3(1), albedo, 0)     = vec3(1)
//       specular  = vec3(1) * spec * 0.3        = ACHROMATIQUE
//   **Tout ce qui n'est pas l'emission est donc gris.** Quel que soit
//   l'eclairage, quelle que soit la normale, quel que soit `spec` :
//       pixel = (S, S, S) + emission,  S inconnu mais IDENTIQUE aux trois canaux
//   La soustraction de deux canaux ELIMINE S. Ce qui reste est exactement ce que
//   le graphe a declare, et rien d'autre.
//
// On rend donc en RGBA8 **UNORM** (lineaire, pas sRGB) : une emission de
// 128/255 dans le graphe doit rendre un ecart de canal de **128, exactement**.
// C'est une valeur attendue calculable, pas une ressemblance.
//
// SANS FENETRE : device DX11 headless (« Mode headless (pas de HWND) », motif
// deja prouve par NkGpuProbe) + `Tools/Offscreen`, REUTILISE tel quel. Aucune
// ligne de NkOffscreenTarget n'est modifiee : on se sert de `ReadbackPixels`,
// jamais de `Capture(path)` — un PNG serait precisement le temoin aveugle.
//
// APPLICATION et non `tests/` : la politique du workspace desactive l'execution
// des tests unitaires ; un banc qui doit prouver quelque chose est une
// application console.
// =============================================================================
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Core/NkIDevice.h"

#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Materials/Graph/NkMatGraphCompile.h"
#include "NKRenderer/Shader/NkShaderLibrary.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"

// ── POURQUOI CE BANC N'IMPRIME PLUS AVEC printf ─────────────────────────────
// Rodolf, 2026-08-22 (et c'est la deuxieme fois) : « ne pas utiliser directement
// printf, le systeme definit des loggers, pourquoi ne pas les utiliser ? »
//
// ⚠️ ET PASSER A `Infof` N'AURAIT RIEN CORRIGE. `Infof` prend une chaine de
// format et des arguments variadiques NON TYPES — la meme mecanique que printf.
// Le defaut mesure la meme nuit par l'agent NK3DModeler — un printf reclamant
// sept `%u` pour six arguments, le septieme lisant la pile et affichant
// « nonmanif=18 » sur un cube parfaitement manifold — se reproduit a l'identique
// avec `Infof`. Rien ne plante, rien n'avertit, ET LE NOMBRE EST CREDIBLE.
//
// Seule la forme POSITIONNELLE change la nature du probleme : `NkFormat("{0}")`
// capture les arguments PAR LEUR TYPE, jamais reinterpretes. S'il en manque un,
// on obtient un TROU VISIBLE, pas un entier plausible. Pour un banc dont toute
// la valeur tient dans les nombres qu'il imprime, c'est la seule propriete qui
// compte : un trou se voit ; un « 18 » plausible se recopie dans un rapport,
// puis dans une ROADMAP, puis dans une decision.
//
//   `-Werror=format` (pose dans le .jenga) DETECTE le desalignement.
//   La forme `{0}` le rend IRREPRESENTABLE. On garde les deux : le drapeau est
//   un filet pour ce qui resterait, la forme est la solution.
//
// Le motif de journal est « %v » : le message SEUL, sans horodatage ni fichier.
// La sortie d'un banc EST son resultat ; une decoration la rendrait illisible et
// instable d'une execution a l'autre.
//
// ⚠️ Et les chaines IMPRIMEES sont en ASCII : le puits console ne transporte pas
// l'UTF-8 (mesure — « — » sortait « - », les guillemets sortaient « ? »). Mieux
// vaut ecrire ce qui sera lu que laisser des caracteres se perdre en chemin. Les
// commentaires, eux, gardent leur typographie : ils ne sont jamais imprimes.
#include "NKContainers/String/NkFormat.h"
#include "NKLogger/NkLog.h"
#include "NKSL/Compiler/NkSLCompiler.h"
#include <stdio.h> // fwrite : ecrire une source SANS passer par le formateur

// NkShaderStage existe DEUX FOIS : celui du RHI (bitmask, = NkSLStage) et
// celui de renderer/NkShaderBackend.h. Sans cet alias, toute mention du nom
// est ambigue. Meme levee que NkMaterialSystem.cpp.
using RHIStage = ::nkentseu::NkShaderStage;

using namespace nkentseu;
using namespace nkentseu::graph;
using namespace nkentseu::renderer;
using namespace nkentseu::renderer::matgraph;

// ── comptage des cas ─────────────────────────────────────────────────────────
// LE REGISTRE DE PROTOTYPES DE CE PROCESSUS DE BANC.
//
// Depuis le 2026-08-23 le registre est un PARAMETRE : il n'existe plus
// d'instance de processus. Ce `gReg` n'est donc PAS un retour du defaut --
// c'est le registre d'un document unique, celui que ce banc manipule. Les
// cas qui ont besoin de DEUX documents en declarent deux, localement, et
// c'est precisement ce que le temoin d'isolement mesure.
static NkMatRegistreProtos gReg;

static uint32 gCas = 0;
static uint32 gEchecs = 0;

static void Cas(const char *nom, bool ok, const NkString &detail) {
	++gCas;
	if (!ok)
		++gEchecs;
	logger.Info("{0:<32} {1} | {2}", NkString(nom), NkString(ok ? "OK  " : "ECHEC"), detail);
}

// ── L'UBO camera, tel que le shader engendre le declare ──────────────────────
// La disposition doit correspondre EXACTEMENT au bloc emis par
// NkMatCompileToNkSL. Un decalage ici ne provoquerait aucune erreur : le shader
// lirait simplement d'autres octets, et `camPos` vaudrait n'importe quoi. C'est
// la meme famille de defaut silencieux que le binding non declare.
struct DemoCameraUBO {
		float32 view[16] = {};
		float32 proj[16] = {};
		float32 viewProj[16] = {};
		float32 invViewProj[16] = {};
		float32 camPos[4] = {0.f, 0.f, 5.f, 1.f};
		float32 camDir[4] = {0.f, 0.f, -1.f, 0.f};
		float32 viewport[2] = {64.f, 64.f};
		float32 time = 0.f;
		float32 deltaTime = 0.f;
		float32 iblStrength = 1.f;
		float32 _pad[3] = {};
};

// ── Un sommet du triangle plein ecran ────────────────────────────────────────
// On FOURNIT les varyings au lieu de les calculer : la position est deja en
// espace de clip, la normale et la couleur sont constantes. Le shader engendre
// n'a alors plus aucune entree cachee — tout ce qu'il lit vient soit du graphe,
// soit d'ici.
struct DemoVertex {
		float32 pos[3];
};

static const char *const kVertexNkSL = R"NKSL(
// Vertex minimal du banc : il ne transforme RIEN et ne lit qu'UN attribut.
//
// ⚠️ UN SEUL ATTRIBUT, ET IL S'APPELLE `aPos`. Le generateur HLSL deduit la
// SEMANTIQUE d'un attribut de son NOM DE VARIABLE (table dans
// NkSLCodeGenHLSLStructs.cpp) : « apos » y donne POSITION. « acolor » et « auv »
// ne figurent dans aucune entree et retomberaient sur TEXCOORD<location> — le
// layout declare cote C++ ne correspondrait plus au shader, et rien ne le
// dirait. On synthetise donc les autres varyings ici : elles deviennent des
// constantes connues de l'appelant, ce qui est justement ce qu'on veut.
@location(0) in vec3 aPos;

@location(0) out vec3 vWorldPos;
@location(1) out vec3 vNormal;
@location(2) out vec2 vUV;
@location(3) out vec4 vColor;

@stage(vertex)
@entry
void main() {
    vWorldPos   = aPos;
    vNormal     = vec3(0.0, 0.0, 1.0);
    // L'UV VARIE avec la position : sans cela toute derivee d'ecran vaut
    // ZERO, et `Bump` comme la base tangente de `Normal Map` ne pourraient
    // etre mesures que sur leur absence d'effet. Les cas d'emission n'en
    // sont pas touches : ils ne lisent pas l'UV.
    vUV         = aPos.xy * 0.5 + 0.5;
    vColor      = vec4(1.0, 1.0, 1.0, 1.0);
    gl_Position = vec4(aPos, 1.0);
}
)NKSL";

// ── Le contexte de rendu, monte une seule fois ───────────────────────────────
struct DemoContexte {
		NkIDevice *device = nullptr;
		NkTextureLibrary texLib;
		NkShaderLibrary shaders;
		NkOffscreenTarget cible;
		NkBufferHandle vbo;
		NkBufferHandle ubo;
		NkDescSetHandle setLayout;
		NkDescSetHandle set;
		uint32 largeur = 64, hauteur = 64;

		bool Monter();
		void Demonter();
		// Compile le NkSL donne, rend un triangle plein ecran, et rend la couleur
		// du pixel CENTRAL. `false` si quoi que ce soit echoue.
		bool RendreEtLire(const NkString &fragmentNkSL, const char *nom, uint8 rgba[4]);
};

bool DemoContexte::Monter() {
	// DX11 headless : pas de surface -> pas de swapchain. Motif deja prouve par
	// NkGpuProbe, et le device le dit lui-meme dans son journal.
	NkDeviceInitInfo di;
	di.api = NkGraphicsApi::NK_GFX_API_DX11;
	di.width = 0;
	di.height = 0;
	device = NkDeviceFactory::Create(di);
	if (!device || !device->IsValid()) {
		logger.Info("device DX11 headless : KO");
		return false;
	}
	if (texLib.Init(device, nullptr) != NkRResult::NK_OK) {
		logger.Info("NkTextureLibrary : KO");
		return false;
	}
	if (!shaders.Init(device, device->GetApi(), /*useNkSL=*/true)) {
		logger.Info("NkShaderLibrary : KO");
		return false;
	}

	NkOffscreenDesc od;
	od.width = largeur;
	od.height = hauteur;
	// PAS DE PROFONDEUR : un seul triangle, rien a trier. Une passe sans
	// attachement de profondeur evite d'avoir a accorder l'etat du pipeline avec
	// la passe — un desaccord la se paierait en pixels manquants sans message.
	od.hasDepth = false;
	// ⚠️ UNORM, PAS sRGB. Avec sRGB, l'ecart de canal ne vaudrait plus la valeur
	// declaree dans le graphe mais son encodage — l'attendu cesserait d'etre
	// calculable, et on retomberait sur « ca ressemble ».
	od.colorFmt = NkGPUFormat::NK_RGBA8_UNORM;
	od.readable = true;
	od.readback = true;
	od.name = NkString("NkMatGraphDemo");
	if (!cible.Init(device, &texLib, od)) {
		logger.Info("NkOffscreenTarget : KO");
		return false;
	}

	// Triangle plein ecran (3 sommets, pas de quad : un triangle suffit et evite
	// la couture diagonale au centre, la ou l'on echantillonne).
	const DemoVertex sommets[3] = {
		{{-1.f, -1.f, 0.f}},
		{{3.f, -1.f, 0.f}},
		{{-1.f, 3.f, 0.f}},
	};
	vbo = device->CreateBuffer(NkBufferDesc::Vertex(sizeof(sommets), sommets));
	if (!vbo.IsValid()) {
		logger.Info("vertex buffer : KO");
		return false;
	}

	DemoCameraUBO cam;
	ubo = device->CreateBuffer(NkBufferDesc::Uniform(sizeof(DemoCameraUBO)));
	if (!ubo.IsValid()) {
		logger.Info("uniform buffer : KO");
		return false;
	}
	device->WriteBuffer(ubo, &cam, sizeof(cam));

	NkDescriptorSetLayoutDesc ld;
	ld.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER, RHIStage::NK_ALL_GRAPHICS);
	setLayout = device->CreateDescriptorSetLayout(ld);
	set = device->AllocateDescriptorSet(setLayout);
	NkDescriptorWrite w{};
	w.set = set;
	w.binding = 0;
	w.type = NkDescriptorType::NK_UNIFORM_BUFFER;
	w.buffer = ubo;
	w.bufferRange = sizeof(DemoCameraUBO);
	device->UpdateDescriptorSets(&w, 1);
	return true;
}

void DemoContexte::Demonter() {
	cible.Shutdown();
	shaders.Shutdown();
	texLib.Shutdown();
	if (device)
		NkDeviceFactory::Destroy(device);
}

bool DemoContexte::RendreEtLire(const NkString &fragmentNkSL, const char *nom, uint8 rgba[4]) {
	::nkentseu::NkShaderHandle prog = shaders.CompileVF(NkString(kVertexNkSL), fragmentNkSL, NkString(nom));
	::nkentseu::NkShaderHandle rhi = shaders.GetRHIHandle(prog);
	if (!rhi.IsValid())
		return false;

	NkGraphicsPipelineDesc pd;
	pd.shader = rhi;
	pd.vertexLayout.AddBinding(0, (uint32)sizeof(DemoVertex))
		.AddAttribute(0, 0, NkGPUFormat::NK_RGB32_FLOAT, 0, "POSITION", 0);
	// Aucun cull : le sens d'enroulement du triangle plein ecran n'a aucune
	// importance ici, et un cull mal oriente rendrait une image NOIRE sans le
	// moindre message — encore un echec silencieux.
	pd.rasterizer.cullMode = NkCullMode::NK_NONE;
	pd.depthStencil = NkDepthStencilDesc::NoDepth();
	pd.renderPass = cible.GetRP();
	pd.descriptorSetLayouts.PushBack(setLayout);
	pd.debugName = nom;
	NkPipelineHandle pipe = device->CreateGraphicsPipeline(pd);
	if (!pipe.IsValid())
		return false;

	NkICommandBuffer *cmd = device->CreateCommandBuffer();
	if (!cmd || !cmd->Begin())
		return false;
	// Fond MAGENTA opaque, jamais noir. Si le tracé échouait en silence, un fond
	// noir se confondrait avec « le graphe a rendu du noir » — la couleur de
	// repli doit être une couleur que le graphe ne peut pas produire.
	// BeginCapture pose lui-meme la barriere de texture, l'effacement, la passe,
	// le viewport et le ciseau. EndCapture ferme la passe ET remet la texture en
	// SHADER_READ — etat que ReadbackPixels SUPPOSE. Ecrire la passe a la main
	// laisserait la texture dans le mauvais etat, et le readback lirait du vide
	// sans se plaindre.
	cible.BeginCapture(cmd, /*clearColor=*/true, NkVec4f{1.f, 0.f, 1.f, 1.f}, /*clearDepth=*/false);
	cmd->BindGraphicsPipeline(pipe);
	cmd->BindDescriptorSet(set, 0);
	cmd->BindVertexBuffer(0, vbo);
	cmd->Draw(3);
	cible.EndCapture(cmd);
	cmd->End();
	device->Submit(&cmd, 1);
	device->WaitIdle();

	// ReadbackPixels fait lui-meme la copie GPU, la soumission, l'attente et le
	// retournement selon le backend. On lit tout, puis on prend le pixel CENTRAL.
	const uint32 n = largeur * hauteur * 4u;
	NkVector<uint8> px;
	px.Resize(n);
	if (!cible.ReadbackPixels(px.Data(), largeur * 4u))
		return false;
	const uint32 c = ((hauteur / 2u) * largeur + (largeur / 2u)) * 4u;
	rgba[0] = px[c + 0];
	rgba[1] = px[c + 1];
	rgba[2] = px[c + 2];
	rgba[3] = px[c + 3];
	device->DestroyPipeline(pipe);
	return true;
}

// ── Les graphes du banc ──────────────────────────────────────────────────────
// L'emission vaut 128/255 EXACTEMENT. Ce n'est pas une coquetterie : en RGBA8
// lineaire, une valeur de 128/255 rend un octet de 128, et l'ecart entre le
// canal emissif et un canal voisin doit donc valoir 128 SANS arrondi. Un
// attendu qui tomberait entre deux octets obligerait a une tolerance, et une
// tolerance cache exactement ce qu'on cherche.
static const float32 kE = 128.f / 255.f;
static const int32 kEcartAttendu = 128;

// Emission pure sur un canal : color = (E,0,0) ou (0,E,0), strength = 1.
static NkNodeId AjouteEmission(NkNodeGraph &g, const NkMatTypes &t, uint32 canal) {
	const NkNodeId n = NkMatAddNode(gReg, g, NK_MN_EMISSION);
	float32 c[3] = {0.f, 0.f, 0.f};
	c[canal] = kE;
	g.SetSocketDefault(n, "color", NkSocketDir::Input, NkValueVec(t.color, c, 3));
	g.SetSocketDefault(n, "strength", NkSocketDir::Input, NkValueReal(t.real, 1.f));
	return n;
}

// Un graphe : une emission (canal donne) vers la sortie.
static bool GrapheEmission(uint32 canal, NkString &out) {
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId sortie = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId e = AjouteEmission(g, t, canal);
	g.Connect(e, "emission", sortie, "surface");
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	out = r.source;
	return r.ok;
}

// Le graphe de Rodolf : DEUX emissions melangees par un Mix Shader.
static bool GrapheMelange(float32 fac, NkString &out) {
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId sortie = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId mix = NkMatAddNode(gReg, g, NK_MN_MIX_SHADER);
	const NkNodeId rouge = AjouteEmission(g, t, 0);
	const NkNodeId vert = AjouteEmission(g, t, 1);
	g.Connect(rouge, "emission", mix, "shader1");
	g.Connect(vert, "emission", mix, "shader2");
	g.Connect(mix, "shader", sortie, "surface");
	g.SetSocketDefault(mix, "fac", NkSocketDir::Input, NkValueReal(t.real, fac));
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	out = r.source;
	return r.ok;
}

// Le graphe qui prouve qu'une PROPRIETE ENUMEREE change le pixel :
//     RGB(rouge) --color1--> Mix Color --color--> Emission --> sortie
//     RGB(vert)  --color2-->
// L'emission garde albedo = 0 et metallic = 0, donc l'invariant achromatique
// tient : tout ce qui n'est pas l'emission reste gris, et la soustraction de
// deux canaux elimine ce gris inconnu.
static bool GrapheMelangeCouleur(const char *operation, float32 fac, NkString &out) {
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId sortie = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId emis = NkMatAddNode(gReg, g, NK_MN_EMISSION);
	const NkNodeId mix = NkMatAddNode(gReg, g, NK_MN_MIX_COLOR);
	const NkNodeId rouge = NkMatAddNode(gReg, g, NK_MN_RGB);
	const NkNodeId vert = NkMatAddNode(gReg, g, NK_MN_RGB);
	g.Connect(emis, "emission", sortie, "surface");
	g.Connect(mix, "color", emis, "color");
	g.Connect(rouge, "color", mix, "color1");
	g.Connect(vert, "color", mix, "color2");
	const float32 cr[3] = {kE, 0.f, 0.f};
	const float32 cv[3] = {0.f, kE, 0.f};
	g.SetProp(rouge, NK_MPROP_COLOR, NkValueVec(t.color, cr, 3));
	g.SetProp(vert, NK_MPROP_COLOR, NkValueVec(t.color, cv, 3));
	g.SetProp(mix, NK_MPROP_OPERATION, NkValueText(t.real, operation));
	g.SetSocketDefault(mix, "fac", NkSocketDir::Input, NkValueReal(t.real, fac));
	g.SetSocketDefault(emis, "strength", NkSocketDir::Input, NkValueReal(t.real, 1.f));
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	out = r.source;
	return r.ok;
}

// Le graphe qui prouve qu'une charge utile VARIABLE atteint le GPU :
//     Value(fac) --> ColorRamp --> Emission --> sortie
// La rampe recoit N arrets ; l'emission garde albedo = 0 et metallic = 0, donc
// l'invariant achromatique tient et la soustraction de deux canaux elimine le
// terme d'eclairage inconnu.
static bool GrapheRampe(const float32 *arrets, uint32 nbReels, float32 fac, NkString &out) {
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId sortie = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId emis = NkMatAddNode(gReg, g, NK_MN_EMISSION);
	const NkNodeId ramp = NkMatAddNode(gReg, g, NK_MN_COLOR_RAMP);
	const NkNodeId val = NkMatAddNode(gReg, g, NK_MN_VALUE);
	g.Connect(emis, "emission", sortie, "surface");
	g.Connect(ramp, "color", emis, "color");
	g.Connect(val, "value", ramp, "fac");
	g.SetProp(ramp, NK_MPROP_STOPS, NkValueVec(t.ramp, arrets, nbReels));
	g.SetProp(ramp, NK_MPROP_INTERP, NkValueText(t.ramp, "lineaire"));
	g.SetProp(val, NK_MPROP_VALUE, NkValueReal(t.real, fac));
	g.SetSocketDefault(emis, "strength", NkSocketDir::Input, NkValueReal(t.real, 1.f));
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	out = r.source;
	return r.ok;
}


// Le graphe qui prouve que la SECONDE charge variable atteint le GPU, et que la
// valeur remodelee traverse bien un noeud du rang 3 avant d'arriver au pixel :
//     Value(x) --> Float Curve --> Combine XYZ (canal VERT) --> Emission
// Le rouge et le bleu du Combine restent a zero : l'invariant achromatique tient
// et la soustraction de deux canaux elimine le terme d'eclairage inconnu.
//
// ⚠️ C'est aussi le PREMIER passage au pixel d'un noeud du rang 3 -- les quatre
// autres n'etaient prouves que sur la source emise. Une valeur juste dans le
// texte du shader et fausse a l'ecran resterait invisible.
static bool GrapheCourbe(const float32 *pts, uint32 nbReels, float32 x, NkString &out) {
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId sortie = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId emis = NkMatAddNode(gReg, g, NK_MN_EMISSION);
	const NkNodeId comb = NkMatAddNode(gReg, g, NK_MN_COMBINE_XYZ);
	const NkNodeId courbe = NkMatAddNode(gReg, g, NK_MN_FLOAT_CURVE);
	const NkNodeId val = NkMatAddNode(gReg, g, NK_MN_VALUE);
	g.Connect(emis, "emission", sortie, "surface");
	g.Connect(comb, "vector", emis, "color");
	g.Connect(courbe, "value", comb, "y");
	g.Connect(val, "value", courbe, "value");
	g.SetSocketDefault(comb, "x", NkSocketDir::Input, NkValueReal(t.real, 0.f));
	g.SetSocketDefault(comb, "z", NkSocketDir::Input, NkValueReal(t.real, 0.f));
	g.SetProp(courbe, NK_MPROP_POINTS, NkValueVec(t.curve, pts, nbReels));
	g.SetProp(courbe, NK_MPROP_INTERP, NkValueText(t.curve, "lineaire"));
	g.SetProp(val, NK_MPROP_VALUE, NkValueReal(t.real, x));
	g.SetSocketDefault(emis, "strength", NkSocketDir::Input, NkValueReal(t.real, 1.f));
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	out = r.source;
	return r.ok;
}

// Un Principled gris dont la NORMALE est imposee directement, en passant par un
// noeud `RGB` : c'est l'ETALON. Il donne, avec le meme eclairage et le meme
// shader, la couleur que produit une normale connue — sans que le banc ait a
// reimplanter la moindre ligne d'ombrage.
static bool GrapheNormaleImposee(float32 nx, float32 ny, float32 nz, NkString &out) {
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId sortie = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	const NkNodeId rgb = NkMatAddNode(gReg, g, NK_MN_RGB);
	g.Connect(bsdf, "bsdf", sortie, "surface");
	g.Connect(rgb, "color", bsdf, "normal");
	const float32 n[3] = {nx, ny, nz};
	g.SetProp(rgb, NK_MPROP_COLOR, NkValueVec(t.color, n, 3));
	const float32 gris[3] = {0.5f, 0.5f, 0.5f};
	g.SetSocketDefault(bsdf, "base_color", NkSocketDir::Input, NkValueVec(t.color, gris, 3));
	g.SetSocketDefault(bsdf, "metallic", NkSocketDir::Input, NkValueReal(t.real, 0.f));
	g.SetSocketDefault(bsdf, "roughness", NkSocketDir::Input, NkValueReal(t.real, 1.f));
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	out = r.source;
	return r.ok;
}

// Le meme Principled gris, mais dont la normale vient d'un `Bump` alimente par
// la coordonnee de texture : la hauteur croit lineairement a l'ecran, donc son
// gradient est CONSTANT et connu.
static bool GrapheRelief(float32 force, NkString &out) {
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId sortie = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	const NkNodeId bump = NkMatAddNode(gReg, g, NK_MN_BUMP);
	const NkNodeId sep = NkMatAddNode(gReg, g, NK_MN_SEPARATE_XYZ);
	const NkNodeId coord = NkMatAddNode(gReg, g, NK_MN_TEX_COORD);
	g.Connect(bsdf, "bsdf", sortie, "surface");
	g.Connect(bump, "normal", bsdf, "normal");
	g.Connect(sep, "x", bump, "height");
	g.Connect(coord, "uv", sep, "vector");
	g.SetSocketDefault(bump, "strength", NkSocketDir::Input, NkValueReal(t.real, force));
	g.SetSocketDefault(bump, "distance", NkSocketDir::Input, NkValueReal(t.real, 1.f));
	const float32 gris[3] = {0.5f, 0.5f, 0.5f};
	g.SetSocketDefault(bsdf, "base_color", NkSocketDir::Input, NkValueVec(t.color, gris, 3));
	g.SetSocketDefault(bsdf, "metallic", NkSocketDir::Input, NkValueReal(t.real, 0.f));
	g.SetSocketDefault(bsdf, "roughness", NkSocketDir::Input, NkValueReal(t.real, 1.f));
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	out = r.source;
	return r.ok;
}

// Une carte de normales PLATE, donnee par un `RGB` a (0,5 ; 0,5 ; 1) — le texel
// neutre. Elle doit rendre exactement la normale geometrique.
static bool GrapheCarteNormales(float32 tr, float32 tv, float32 tb, NkString &out) {
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId sortie = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId bsdf = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	const NkNodeId nm = NkMatAddNode(gReg, g, NK_MN_NORMAL_MAP);
	const NkNodeId rgb = NkMatAddNode(gReg, g, NK_MN_RGB);
	g.Connect(bsdf, "bsdf", sortie, "surface");
	g.Connect(nm, "normal", bsdf, "normal");
	g.Connect(rgb, "color", nm, "color");
	const float32 texel[3] = {tr, tv, tb};
	g.SetProp(rgb, NK_MPROP_COLOR, NkValueVec(t.color, texel, 3));
	const float32 gris[3] = {0.5f, 0.5f, 0.5f};
	g.SetSocketDefault(bsdf, "base_color", NkSocketDir::Input, NkValueVec(t.color, gris, 3));
	g.SetSocketDefault(bsdf, "metallic", NkSocketDir::Input, NkValueReal(t.real, 0.f));
	g.SetSocketDefault(bsdf, "roughness", NkSocketDir::Input, NkValueReal(t.real, 1.f));
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	out = r.source;
	return r.ok;
}

// Un damier dont les deux couleurs partent dans une emission. `decalage` deplace
// la coordonnee AVANT le damier, ce qui fait basculer la case.
//
// ⚠️ L'ATTENDU SE CALCULE ENTIEREMENT. Au pixel central l'UV vaut (0,5 ; 0,5).
// Le damier vaut `mod(floor(x) + floor(y) + floor(z), 2)` sur la coordonnee
// multipliee par l'echelle :
//   sans decalage, echelle 5 : (2,5 ; 2,5 ; 0) -> 2 + 2 + 0 = 4 -> PAIR  -> color2
//   decalage +0,2, echelle 5 : (3,5 ; 2,5 ; 0) -> 3 + 2 + 0 = 5 -> IMPAIR -> color1
// Les deux cases sont donc predites, et par un calcul qui ne doit RIEN a
// l'eclairage : c'est le damier lui-meme qu'on mesure.
static bool GrapheDamier(float32 decalage, NkString &out) {
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId sortie = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId emis = NkMatAddNode(gReg, g, NK_MN_EMISSION);
	const NkNodeId dam = NkMatAddNode(gReg, g, NK_MN_CHECKER);
	const NkNodeId map = NkMatAddNode(gReg, g, NK_MN_MAPPING);
	const NkNodeId coord = NkMatAddNode(gReg, g, NK_MN_TEX_COORD);
	g.Connect(emis, "emission", sortie, "surface");
	g.Connect(dam, "color", emis, "color");
	g.Connect(map, "vector_out", dam, "vector");
	g.Connect(coord, "uv", map, "vector");
	const float32 loc[3] = {decalage, 0.f, 0.f};
	g.SetSocketDefault(map, "location", NkSocketDir::Input, NkValueVec(t.vector, loc, 3));
	const float32 rouge[3] = {kE, 0.f, 0.f};
	const float32 vert[3] = {0.f, kE, 0.f};
	g.SetSocketDefault(dam, "color1", NkSocketDir::Input, NkValueVec(t.color, rouge, 3));
	g.SetSocketDefault(dam, "color2", NkSocketDir::Input, NkValueVec(t.color, vert, 3));
	g.SetSocketDefault(dam, "scale", NkSocketDir::Input, NkValueReal(t.real, 5.f));
	g.SetSocketDefault(emis, "strength", NkSocketDir::Input, NkValueReal(t.real, 1.f));
	NkMatCompileResult r = NkMatCompileToNkSL(gReg, g);
	out = r.source;
	return r.ok;
}

// ═══════════════════════════════════════════════════════════════════════════
//  MATRICE D'ESSAIS : DEUX BLOCS UNIFORMES SUR DX11 (22/08/2026)
// ═══════════════════════════════════════════════════════════════════════════
//
// Chaque ligne isole UNE variable et rapporte l'etat des DEUX blocs, pas
// seulement du second. C'est la seule facon de distinguer « le second n'arrive
// pas » de « le second casse le premier » — deux pannes qui se ressemblent
// quand on ne regarde que le second.
//
// LECTURE : le fragment ecrit le premier bloc dans le ROUGE, le second dans le
// VERT, et une constante connue dans le BLEU. Un bloc absent lit zero. Le bleu
// distingue « le bloc n'est pas arrive » de « le shader n'a jamais tourne » —
// sans lui, une passe qui echoue rendrait la meme chose qu'un bloc manquant.
// Le fond MAGENTA (255,0,255) reste la troisieme reponse possible : rien n'a
// ete trace du tout.
struct BlocEssai {
		float32 v[4] = {0.f, 0.f, 0.f, 0.f};
};

static const uint8 kAttenduA = 128; // premier bloc  : 128/255
static const uint8 kAttenduB = 192; // second bloc   : 192/255
static const uint8 kAttenduM = 64;  // marqueur bleu : 64/255

struct LigneMatrice {
		const char *nom;
		uint32 nbBlocs;			 ///< 1 ou 2 blocs declares dans le shader
		uint32 setA, bindA;		 ///< set/binding du premier, tels que le SHADER les declare
		uint32 setB, bindB;		 ///< idem second
		uint32 nbSetsCpp;		 ///< 1 = les deux descripteurs dans le MEME set
		bool entreeVideAuMilieu; ///< une entree de layout VIDE inseree a l'index 1
		// ⚠️ TAILLE DU PREMIER BLOC. La matrice initiale montait deux blocs de 16
		// octets ; le montage d'origine avait un CameraUBO d'environ 300 octets en
		// premier. C'est le seul des trois ecarts que j'avais nommes qui porte sur
		// la MECANIQUE et non sur l'habillage.
		bool premierBlocLarge;
		const char *isole;
};

// Construit le fragment. Les varyings sont declarees a l'identique de ce
// qu'emet le compilateur du graphe : le vertex du banc en sort quatre, et un
// fragment qui n'en declarerait pas autant changerait la signature du couple —
// on mesurerait alors autre chose que ce qu'on croit.
static void PutEntier(NkString &s, uint32 v) {
	char b[16];
	uint32 n = 0;
	if (v == 0)
		b[n++] = (char)48;
	while (v > 0) {
		b[n++] = (char)(48 + (v % 10u));
		v /= 10u;
	}
	char o[17];
	for (uint32 i = 0; i < n; ++i)
		o[i] = b[n - 1u - i];
	o[n] = 0;
	s.Append(o);
}

static NkString FragmentMatrice(const LigneMatrice &l) {
	NkString s;
	s.Append("@location(0) in vec3 vWorldPos;\n");
	s.Append("@location(1) in vec3 vNormal;\n");
	s.Append("@location(2) in vec2 vUV;\n");
	s.Append("@location(3) in vec4 vColor;\n\n");
	s.Append("@location(0) out vec4 fragColor;\n\n");
	// ⚠️ SURTOUT PAS `NkFormat` ICI, ET CE N EST PAS UN GOUT.
	//
	// Mesure du 22/08 : `NkFormat("uniform BlocA {\n vec4 va;\n} uA;")` rend
	// `uniform BlocA 0 uA;`. Les ACCOLADES LITTERALES du bloc uniforme sont
	// prises pour des marqueurs de substitution, et le CORPS DU BLOC DISPARAIT.
	//
	// Ce qui rend la panne mechante : le shader ampute COMPILE quand meme (NkSL
	// est permissif, cf. Q29), le pipeline se cree, la passe tourne, et le seul
	// symptome est un fond magenta. On accuse alors la couche RHI d une panne
	// qu on vient de fabriquer soi-meme dans son propre generateur de source.
	//
	// TROISIEME fois que cet outil de formatage abime la donnee qu il transporte.
	// Regle : aucun texte contenant une accolade litterale ne passe par NkFormat.
	if (l.nbBlocs >= 1) {
		s.Append("@binding(set=");
		PutEntier(s, l.setA);
		s.Append(", binding=");
		PutEntier(s, l.bindA);
		if (l.premierBlocLarge)
			// Membres RECOPIES a l'identique de ce qu'emet NkMatCompileToNkSL pour
			// CameraUBO : c'est la taille ET la forme du bloc d'origine.
			s.Append(")\nuniform BlocA {\n    mat4  view;\n    mat4  proj;\n                     mat4  viewProj;\n    mat4  invViewProj;\n    vec4  camPos;\n                     vec4  camDir;\n    vec2  viewport;\n    float time;\n                     float deltaTime;\n    float iblStrength;\n} uA;\n\n");
		else
			s.Append(")\nuniform BlocA {\n    vec4 va;\n} uA;\n\n");
	}
	if (l.nbBlocs == 2) {
		s.Append("@binding(set=");
		PutEntier(s, l.setB);
		s.Append(", binding=");
		PutEntier(s, l.bindB);
		s.Append(")\nuniform BlocB {\n    vec4 vb;\n} uB;\n\n");
	}
	const char *lireA = l.premierBlocLarge ? "uA.camPos.x" : "uA.va.x";
	s.Append("@stage(fragment)\n@entry\nvoid main() {\n");
	if (l.nbBlocs == 2) {
		s.Append("    fragColor = vec4(");
		s.Append(lireA);
		s.Append(", uB.vb.x, 0.250980392, 1.0);\n");
	} else if (l.nbBlocs == 1) {
		s.Append("    fragColor = vec4(");
		s.Append(lireA);
		s.Append(", 0.0, 0.250980392, 1.0);\n");
	} else {
		s.Append("    fragColor = vec4(0.0, 0.0, 0.250980392, 1.0);\n");
	}
	s.Append("}\n");
	return s;
}

// Monte et rend UNE ligne. Rend false seulement si la ligne NE PEUT PAS se
// monter — auquel cas la case reste vide dans le tableau plutot que d'etre
// approximee, ce qui serait pire qu'une absence de mesure.
static bool JoueLigne(DemoContexte &ctx, const LigneMatrice &l, uint8 rgba[4], NkString &pourquoiPas) {
	BlocEssai a, b;
	a.v[0] = (float32)kAttenduA / 255.f;
	b.v[0] = (float32)kAttenduB / 255.f;

	// Le premier bloc, large, reprend EXACTEMENT la structure du CameraUBO du
	// banc : meme taille, memes membres, meme decalage pour la valeur lue
	// (`camPos.x`). C'est la seule facon que la ligne teste la TAILLE et non
	// une structure inventee qui se trouverait faire le meme nombre d'octets.
	DemoCameraUBO large;
	large.camPos[0] = (float32)kAttenduA / 255.f;

	const uint32 tailleA = l.premierBlocLarge ? (uint32)sizeof(DemoCameraUBO) : (uint32)sizeof(BlocEssai);
	NkBufferHandle bufA = ctx.device->CreateBuffer(NkBufferDesc::Uniform(tailleA));
	NkBufferHandle bufB = ctx.device->CreateBuffer(NkBufferDesc::Uniform(sizeof(BlocEssai)));
	if (!bufA.IsValid() || (l.nbBlocs == 2 && !bufB.IsValid())) {
		pourquoiPas = NkString("creation de tampon refusee");
		return false;
	}
	if (l.premierBlocLarge)
		ctx.device->WriteBuffer(bufA, &large, sizeof(large));
	else
		ctx.device->WriteBuffer(bufA, &a, sizeof(a));
	if (l.nbBlocs == 2)
		ctx.device->WriteBuffer(bufB, &b, sizeof(b));

	// ── Les layouts, dans l'ordre exact ou le pipeline les recevra ──────────
	NkVector<NkDescSetHandle> layouts;
	NkDescSetHandle lA, lB, lVide;
	{
		NkDescriptorSetLayoutDesc d;
		d.Add(l.bindA, NkDescriptorType::NK_UNIFORM_BUFFER, RHIStage::NK_ALL_GRAPHICS);
		if (l.nbBlocs == 2 && l.nbSetsCpp == 1)
			d.Add(l.bindB, NkDescriptorType::NK_UNIFORM_BUFFER, RHIStage::NK_ALL_GRAPHICS);
		lA = ctx.device->CreateDescriptorSetLayout(d);
	}
	if (!lA.IsValid()) {
		pourquoiPas = NkString("layout du premier set refuse");
		return false;
	}
	layouts.PushBack(lA);

	if (l.entreeVideAuMilieu) {
		NkDescriptorSetLayoutDesc d; // AUCUNE entree : c'est tout l'objet de la ligne
		lVide = ctx.device->CreateDescriptorSetLayout(d);
		if (!lVide.IsValid()) {
			pourquoiPas = NkString("layout VIDE refuse par la couche -- la ligne ne peut pas se monter");
			return false;
		}
		layouts.PushBack(lVide);
	}
	if (l.nbBlocs == 2 && l.nbSetsCpp == 2) {
		NkDescriptorSetLayoutDesc d;
		d.Add(l.bindB, NkDescriptorType::NK_UNIFORM_BUFFER, RHIStage::NK_ALL_GRAPHICS);
		lB = ctx.device->CreateDescriptorSetLayout(d);
		if (!lB.IsValid()) {
			pourquoiPas = NkString("layout du second set refuse");
			return false;
		}
		layouts.PushBack(lB);
	}

	// ── Les sets, et les ecritures ──────────────────────────────────────────
	NkDescSetHandle sA = ctx.device->AllocateDescriptorSet(lA);
	NkDescSetHandle sB;
	if (!sA.IsValid()) {
		pourquoiPas = NkString("allocation du premier set refusee");
		return false;
	}
	NkDescriptorWrite w[2] = {};
	uint32 nw = 0;
	w[nw].set = sA;
	w[nw].binding = l.bindA;
	w[nw].type = NkDescriptorType::NK_UNIFORM_BUFFER;
	w[nw].buffer = bufA;
	w[nw].bufferRange = tailleA;
	++nw;
	if (l.nbBlocs == 2) {
		if (l.nbSetsCpp == 1) {
			w[nw].set = sA;
		} else {
			sB = ctx.device->AllocateDescriptorSet(lB);
			if (!sB.IsValid()) {
				pourquoiPas = NkString("allocation du second set refusee");
				return false;
			}
			w[nw].set = sB;
		}
		w[nw].binding = l.bindB;
		w[nw].type = NkDescriptorType::NK_UNIFORM_BUFFER;
		w[nw].buffer = bufB;
		w[nw].bufferRange = sizeof(BlocEssai);
		++nw;
	}
	ctx.device->UpdateDescriptorSets(w, nw);

	// ── Le rendu ────────────────────────────────────────────────────────────
	const NkString frag = FragmentMatrice(l);
	::nkentseu::NkShaderHandle prog = ctx.shaders.CompileVF(NkString(kVertexNkSL), frag, NkString(l.nom));
	::nkentseu::NkShaderHandle rhi = ctx.shaders.GetRHIHandle(prog);
	if (!rhi.IsValid()) {
		pourquoiPas = NkString("le shader de la ligne ne compile pas");
		return false;
	}
	NkGraphicsPipelineDesc pd;
	pd.shader = rhi;
	pd.vertexLayout.AddBinding(0, (uint32)sizeof(DemoVertex))
		.AddAttribute(0, 0, NkGPUFormat::NK_RGB32_FLOAT, 0, "POSITION", 0);
	pd.rasterizer.cullMode = NkCullMode::NK_NONE;
	pd.depthStencil = NkDepthStencilDesc::NoDepth();
	pd.renderPass = ctx.cible.GetRP();
	for (uint32 i = 0; i < (uint32)layouts.Size(); ++i)
		pd.descriptorSetLayouts.PushBack(layouts[i]);
	pd.debugName = l.nom;
	if (getenv("NK_MATRICE_DIAG")) {
		logger.Info("--- source de {0} ---", NkString(l.nom));
		logger.Info("{0}", frag);
		logger.Info("--- handles : bufA={1} bufB={2} lA={3} sA={4} sB={5} layouts={6} ---", NkString(""),
					(uint32)bufA.IsValid(), (uint32)bufB.IsValid(), (uint32)lA.IsValid(), (uint32)sA.IsValid(),
					(uint32)sB.IsValid(), (uint32)layouts.Size());
	}
	NkPipelineHandle pipe = ctx.device->CreateGraphicsPipeline(pd);
	if (!pipe.IsValid()) {
		pourquoiPas = NkString("le pipeline de la ligne est refuse");
		return false;
	}

	NkICommandBuffer *cmd = ctx.device->CreateCommandBuffer();
	if (!cmd || !cmd->Begin()) {
		pourquoiPas = NkString("command buffer refuse");
		return false;
	}
	ctx.cible.BeginCapture(cmd, true, NkVec4f{1.f, 0.f, 1.f, 1.f}, false);
	cmd->BindGraphicsPipeline(pipe);
	// ⚠️ CHAQUE SET EST LIE EXPLICITEMENT, a son index dans la liste des
	// layouts. Lier le set 0 ne lie pas le set 2 : ils ne se confondent que
	// dans l'espace de registres de DX11, pas dans l'appel.
	cmd->BindDescriptorSet(sA, 0);
	if (l.nbBlocs == 2 && l.nbSetsCpp == 2)
		cmd->BindDescriptorSet(sB, l.entreeVideAuMilieu ? 2u : 1u);
	cmd->BindVertexBuffer(0, ctx.vbo);
	cmd->Draw(3);
	ctx.cible.EndCapture(cmd);
	cmd->End();
	ctx.device->Submit(&cmd, 1);
	ctx.device->WaitIdle();

	const uint32 n = ctx.largeur * ctx.hauteur * 4u;
	NkVector<uint8> px;
	px.Resize(n);
	if (!ctx.cible.ReadbackPixels(px.Data(), ctx.largeur * 4u)) {
		pourquoiPas = NkString("relecture de pixels refusee");
		return false;
	}
	const uint32 c = ((ctx.hauteur / 2u) * ctx.largeur + (ctx.largeur / 2u)) * 4u;
	for (uint32 i = 0; i < 4; ++i)
		rgba[i] = px[c + i];
	ctx.device->DestroyPipeline(pipe);
	return true;
}

static void MatriceDeuxBlocs(DemoContexte &ctx) {
	const LigneMatrice lignes[7] = {
		{"0-AUCUN-bloc-temoin-d-appareil", 0, 0, 0, 0, 0, 1, false, false, "mon appareil trace-t-il quoi que ce soit"},
		{"1-un-seul-bloc-b0", 1, 0, 0, 0, 0, 1, false, false, "temoin : le mecanisme marche-t-il du tout"},
		{"2-deux-blocs-b0-b8-MEME-set", 2, 0, 0, 0, 8, 1, false, false, "le NOMBRE de blocs, sans histoire de sets"},
		{"3-deux-blocs-b0-b1-MEME-set", 2, 0, 0, 0, 1, 1, false, false, "le NUMERO du second (bas contre haut)"},
		{"4-deux-blocs-DEUX-sets-sans-vide", 2, 0, 0, 1, 8, 2, false, false, "le nombre de SETS"},
		{"5-deux-sets-AVEC-entree-vide", 2, 0, 0, 2, 8, 2, true, false, "l'entree de layout vide au milieu"},
		{"6-PREMIER-BLOC-LARGE-300o-plus-vide", 2, 0, 0, 2, 8, 2, true, true,
		 "la TAILLE du premier bloc -- au plus pres du montage d'origine"},
	};
	logger.Info("");
	logger.Info("== MATRICE : deux blocs uniformes sur DX11 ==");
	logger.Info("   lecture : R=premier bloc (attendu {0}), V=second (attendu {1}), B=marqueur (attendu {2})",
				(uint32)kAttenduA, (uint32)kAttenduB, (uint32)kAttenduM);
	logger.Info("   un bloc absent lit ZERO. Fond magenta (255,0,255) = rien n'a ete trace.");
	logger.Info("");
	for (uint32 i = 0; i < 7; ++i) {
		uint8 c[4] = {0, 0, 0, 0};
		NkString pourquoi;
		if (!JoueLigne(ctx, lignes[i], c, pourquoi)) {
			logger.Info("@@LIGNE {0} | CASE VIDE -- {1}", NkString(lignes[i].nom), pourquoi);
			continue;
		}
		const bool tourne = (c[2] >= kAttenduM - 1 && c[2] <= kAttenduM + 1);
		const bool arriveA = (c[0] >= kAttenduA - 1 && c[0] <= kAttenduA + 1);
		const bool arriveB = (lignes[i].nbBlocs == 2) && (c[1] >= kAttenduB - 1 && c[1] <= kAttenduB + 1);
		logger.Info("@@LIGNE {0} | pixel=({1},{2},{3}) | shader a tourne={4} | PREMIER bloc arrive={5} | "
					"SECOND bloc arrive={6} | isole : {7}",
					NkString(lignes[i].nom), (uint32)c[0], (uint32)c[1], (uint32)c[2], tourne ? 1 : 0,
					arriveA ? 1 : 0, lignes[i].nbBlocs == 2 ? (arriveB ? 1 : 0) : 9, NkString(lignes[i].isole));
	}
	logger.Info("");
}


// ═══════════════════════════════════════════════════════════════════════════
//  LES PARAMETRES EXPOSES, CABLES JUSQU'AU PIXEL
// ═══════════════════════════════════════════════════════════════════════════
//
// ⚠️ CE CABLAGE EST COMMITE, ET C'EST LE POINT.
//
// Un premier essai avait ete monte puis RETIRE sans jamais etre commite. Il a
// fallu trois jours pour s'apercevoir qu'on raisonnait sur un SOUVENIR de code
// au lieu d'un code : impossible de le relire, impossible de le bissecter,
// impossible de dire ce qu'il faisait vraiment. La lecon n'est pas « il ne
// fallait pas le retirer », c'est **un echafaudage qui a produit une mesure
// doit survivre a la mesure**. Meme rouge. Surtout rouge.
//
// La forme du cas est celle d'origine, et pas une plus simple : DEUX valeurs
// ecrites dans le meme tampon entre deux tirs, et l'ecart de canal qui suit.
// Un cas qui ne rendrait qu'une fois prouverait que le bloc arrive, pas qu'un
// `SetFloat` PILOTE quoi que ce soit — or c'est exactement la question.

// Un graphe dont l'emission est un PARAMETRE EXPOSE. Le reste est identique
// aux cas d'emission deja verts : meme noeud, meme puits, meme chaine.
// Seule difference : la couleur ne vient plus d'un defaut de prise fige dans
// le shader, elle vient du bloc uniforme.
static bool GrapheEmissionExposee(NkMatCompileResult &r, bool avecLeReelDevant) {
	NkNodeGraph g;
	const NkMatTypes t = NkMatRegisterTypes(g);
	const NkNodeId sortie = NkMatAddNode(gReg, g, NK_MN_OUTPUT);
	const NkNodeId b = NkMatAddNode(gReg, g, NK_MN_PRINCIPLED);
	g.Connect(b, "bsdf", sortie, "surface");
	// ⚠️ DEUX parametres exposes, et le choix des deux prises n'est PAS libre.
	//
	// Premier essai, ecarte : exposer `strength` et `color` d'un noeud Emission.
	// Le shader les MULTIPLIE l'un par l'autre -- donc si l'un des deux
	// n'arrivait pas, le pixel tombait au plancher achromatique dans les DEUX
	// cas, et le cas etait incapable de dire LEQUEL manquait. C'est la regle
	// deja gravee au CLAUDE.md parent : un cas neutre ne teste pas ce qui
	// multiplie par zero, et ici c'est pire -- il ne teste pas non plus ce qui
	// EST multiplie.
	//
	// Forme retenue : `roughness` (un reel) et `emission` (un vecteur). La
	// rugosite ne touche QUE le speculaire, qui est achromatique quand
	// metallic vaut zero -- elle s'annule donc dans un ecart entre canaux. Ce
	// que l'ecart mesure est alors l'emission SEULE, et un ecart nul accuse
	// l'emission sans ambiguite.
	//
	// Et l'ordre compte : en std140 un reel puis un vecteur donnent 0 puis 16.
	// Sans le reel devant, le vecteur serait au decalage ZERO et un banc qui
	// ecrirait a l'offset 0 par paresse passerait quand meme.
	g.SetSocketDefault(b, "roughness", NkSocketDir::Input, NkValueReal(t.real, 0.5f));
	const float32 noir[3] = {0.f, 0.f, 0.f};
	g.SetSocketDefault(b, "emission", NkSocketDir::Input, NkValueVec(t.color, noir, 3));
	if (avecLeReelDevant) {
		NkString c1(NK_MPROP_EXPOSE_PREFIX);
		c1.Append("roughness");
		g.SetProp(b, c1.CStr(), NkValueText(t.real, "usure"));
	}
	NkString c2(NK_MPROP_EXPOSE_PREFIX);
	c2.Append("emission");
	g.SetProp(b, c2.CStr(), NkValueText(t.real, "teinte"));
	r = NkMatCompileToNkSL(gReg, g);
	return r.ok;
}

// Rend DEUX fois le meme materiau en ne changeant QUE le contenu du bloc de
// parametres entre les deux tirs. Pipeline, sets et tampon sont montes UNE
// fois : c'est le regime reel, celui ou du code de jeu ecrit un parametre
// entre deux images.
static bool RendreParamsDeuxValeurs(DemoContexte &ctx, const NkMatCompileResult &r, const char *nom,
									const char *nomParam, const float32 valA[3], const float32 valB[3],
									uint8 rgbaA[4], uint8 rgbaB[4], NkString &pourquoiPas) {
	const NkMatParamExpose *p = r.TrouveParam(nomParam);
	if (!p) {
		pourquoiPas = NkString("parametre absent du resultat de compilation");
		return false;
	}
	// ⚠️ ON ECRIT AU DECALAGE DECLARE PAR LE COMPILATEUR, jamais a un decalage
	// deduit du rang. En std140 un vec3 s'aligne sur 16 et en occupe 12 : une
	// deduction par l'ordre de declaration ecrirait a cote des le premier vec3,
	// sans erreur et avec une valeur credible.
	const uint32 taille = r.paramsTaille < 16u ? 16u : r.paramsTaille;
	NkVector<uint8> octets;
	octets.Resize(taille);

	NkBufferHandle ubo = ctx.device->CreateBuffer(NkBufferDesc::Uniform(taille));
	if (!ubo.IsValid()) {
		pourquoiPas = NkString("tampon de parametres refuse");
		return false;
	}

	// ── Les layouts, dans l'ordre que le SHADER declare ─────────────────────
	// Le shader engendre met la camera en set 0 et les parametres en set 2. La
	// liste des layouts doit donc porter trois entrees, celle du milieu VIDE.
	// Forme prouvee par la ligne 6 de la matrice, avec un premier bloc de meme
	// taille que le CameraUBO : ce n'est pas une supposition.
	NkDescSetHandle lCam, lVide, lPar;
	{
		NkDescriptorSetLayoutDesc d;
		d.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER, RHIStage::NK_ALL_GRAPHICS);
		lCam = ctx.device->CreateDescriptorSetLayout(d);
	}
	{
		NkDescriptorSetLayoutDesc d;
		lVide = ctx.device->CreateDescriptorSetLayout(d);
	}
	{
		NkDescriptorSetLayoutDesc d;
		d.Add(NK_MATBIND_GRAPH_PARAMS, NkDescriptorType::NK_UNIFORM_BUFFER, RHIStage::NK_ALL_GRAPHICS);
		lPar = ctx.device->CreateDescriptorSetLayout(d);
	}
	if (!lCam.IsValid() || !lVide.IsValid() || !lPar.IsValid()) {
		pourquoiPas = NkString("un des trois layouts est refuse");
		return false;
	}
	NkDescSetHandle sCam = ctx.device->AllocateDescriptorSet(lCam);
	NkDescSetHandle sPar = ctx.device->AllocateDescriptorSet(lPar);
	if (!sCam.IsValid() || !sPar.IsValid()) {
		pourquoiPas = NkString("allocation d un set refusee");
		return false;
	}
	NkDescriptorWrite w[2] = {};
	w[0].set = sCam;
	w[0].binding = 0;
	w[0].type = NkDescriptorType::NK_UNIFORM_BUFFER;
	w[0].buffer = ctx.ubo;
	w[0].bufferRange = sizeof(DemoCameraUBO);
	w[1].set = sPar;
	w[1].binding = NK_MATBIND_GRAPH_PARAMS;
	w[1].type = NkDescriptorType::NK_UNIFORM_BUFFER;
	w[1].buffer = ubo;
	w[1].bufferRange = taille;
	ctx.device->UpdateDescriptorSets(w, 2);

	// ── Le pipeline, monte UNE fois ─────────────────────────────────────────
	::nkentseu::NkShaderHandle prog = ctx.shaders.CompileVF(NkString(kVertexNkSL), r.source, NkString(nom));
	::nkentseu::NkShaderHandle rhi = ctx.shaders.GetRHIHandle(prog);
	if (!rhi.IsValid()) {
		pourquoiPas = NkString("le shader engendre ne compile pas");
		return false;
	}
	NkGraphicsPipelineDesc pd;
	pd.shader = rhi;
	pd.vertexLayout.AddBinding(0, (uint32)sizeof(DemoVertex))
		.AddAttribute(0, 0, NkGPUFormat::NK_RGB32_FLOAT, 0, "POSITION", 0);
	pd.rasterizer.cullMode = NkCullMode::NK_NONE;
	pd.depthStencil = NkDepthStencilDesc::NoDepth();
	pd.renderPass = ctx.cible.GetRP();
	pd.descriptorSetLayouts.PushBack(lCam);
	pd.descriptorSetLayouts.PushBack(lVide);
	pd.descriptorSetLayouts.PushBack(lPar);
	pd.debugName = nom;
	NkPipelineHandle pipe = ctx.device->CreateGraphicsPipeline(pd);
	if (!pipe.IsValid()) {
		pourquoiPas = NkString("pipeline refuse");
		return false;
	}

	// ── Les DEUX tirs, seul le contenu du tampon change ─────────────────────
	for (uint32 tir = 0; tir < 2; ++tir) {
		const float32 *v = (tir == 0) ? valA : valB;
		for (uint32 i = 0; i < (uint32)octets.Size(); ++i)
			octets[i] = 0;
		const uint32 nb = (p->taille >= 12u) ? 3u : 1u;
		for (uint32 c = 0; c < nb; ++c) {
			const float32 f = v[c];
			const uint8 *src = (const uint8 *)&f;
			for (uint32 k = 0; k < 4; ++k)
				octets[p->decalage + c * 4u + k] = src[k];
		}
		// Le reel est ecrit lui aussi, a SON decalage, et il vaut 0.5 aux deux
		// tirs : il ne doit rien expliquer de l'ecart mesure, seulement occuper
		// la place qui pousse la couleur a un decalage non nul.
		const NkMatParamExpose *pi = r.TrouveParam("usure");
		if (pi) {
			const float32 un = 0.5f;
			const uint8 *su = (const uint8 *)&un;
			for (uint32 k = 0; k < 4; ++k)
				octets[pi->decalage + k] = su[k];
		}
		ctx.device->WriteBuffer(ubo, octets.Data(), (uint32)octets.Size());

		NkICommandBuffer *cmd = ctx.device->CreateCommandBuffer();
		if (!cmd || !cmd->Begin()) {
			pourquoiPas = NkString("command buffer refuse");
			return false;
		}
		ctx.cible.BeginCapture(cmd, true, NkVec4f{1.f, 0.f, 1.f, 1.f}, false);
		cmd->BindGraphicsPipeline(pipe);
		// LES DEUX SETS SONT LIES. Lier le set 0 ne lie pas le set 2.
		cmd->BindDescriptorSet(sCam, 0);
		cmd->BindDescriptorSet(sPar, 2);
		cmd->BindVertexBuffer(0, ctx.vbo);
		cmd->Draw(3);
		ctx.cible.EndCapture(cmd);
		cmd->End();
		ctx.device->Submit(&cmd, 1);
		ctx.device->WaitIdle();

		const uint32 n = ctx.largeur * ctx.hauteur * 4u;
		NkVector<uint8> px;
		px.Resize(n);
		if (!ctx.cible.ReadbackPixels(px.Data(), ctx.largeur * 4u)) {
			pourquoiPas = NkString("relecture refusee");
			return false;
		}
		const uint32 c = ((ctx.hauteur / 2u) * ctx.largeur + (ctx.largeur / 2u)) * 4u;
		uint8 *dst = (tir == 0) ? rgbaA : rgbaB;
		for (uint32 i = 0; i < 4; ++i)
			dst[i] = px[c + i];
	}
	ctx.device->DestroyPipeline(pipe);
	return true;
}

int main() {
	// ⚠️ `Pattern()` est GLOBAL ET PERSISTANT : il modifie l'instance de journal
	// du PROCESSUS, pas l'appel. On le pose donc UNE FOIS ici, et pas a chaque
	// ligne — sinon chaque composant du moteur qui journalise ensuite herite
	// silencieusement du motif du banc. Consequence assumee : les lignes du
	// moteur perdent aussi leur horodatage dans ce processus ; elles restent
	// reconnaissables a leur crochet ouvrant, et se filtrent par « ^\[ ».
	logger.Pattern("%v");
	logger.Info("== NkMatGraphDemo - le graphe REND, et l'attendu est calculable ==");
	logger.Info("   DX11 headless, 64x64 hors-ecran, RGBA8 UNORM (lineaire).");
	logger.Info("   On ne compare AUCUNE image : on mesure des ecarts entre canaux,");
	logger.Info("   que le modele d'eclairage ne peut ni creer ni masquer.\n");

	DemoContexte ctx;
	if (!ctx.Monter()) {
		logger.Info("\n-- montage impossible : AUCUN cas n'a tourne. C'est un ECHEC, pas un saut. --");
		return 1;
	}

	// Matrice d'essais des deux blocs uniformes, sur demande du coordinateur.
	// Gate par variable d'environnement : elle ne fait pas partie des cas, elle
	// MESURE une panne qui n'est pas la mienne.
	if (getenv("NK_MATRICE")) {
		MatriceDeuxBlocs(ctx);
		ctx.Demonter();
		return 0;
	}

	uint8 rouge[4] = {}, vert[4] = {}, mix0[4] = {}, mix1[4] = {}, mixMoitie[4] = {};
	NkString src;
	bool ok = true;

	// ── 1. emission rouge ────────────────────────────────────────────────
	ok = GrapheEmission(0, src) && ctx.RendreEtLire(src, "matgraph_rouge", rouge);
	{
		// DISCRIMINE : V == B elimine le terme achromatique ; l'ecart R-V est
		// exactement ce que le graphe a declare. Un defaut de prise perdu
		// donnerait 0, un canal permute mettrait l'ecart ailleurs.
		const int32 ecart = (int32)rouge[0] - (int32)rouge[1];
		NkString d;
		d = NkFormat("pixel=({0},{1},{2}) | V==B : {3} | ecart R-V={4} (attendu {5}, sans tolerance)", rouge[0], rouge[1], rouge[2], rouge[1] == rouge[2] ? "oui" : "NON", ecart, kEcartAttendu);
		Cas("rendu/emission-rouge", ok && rouge[1] == rouge[2] && ecart == kEcartAttendu, d);
	}

	// ── 2. emission verte ────────────────────────────────────────────────
	ok = GrapheEmission(1, src) && ctx.RendreEtLire(src, "matgraph_vert", vert);
	{
		// LE TEMOIN DE L'AUTRE CANAL. Sans lui, un shader qui ecrirait toujours
		// dans le rouge passerait le cas precedent.
		const int32 ecart = (int32)vert[1] - (int32)vert[0];
		NkString d;
		d = NkFormat("pixel=({0},{1},{2}) | R==B : {3} | ecart V-R={4} (attendu {5})", vert[0], vert[1], vert[2], vert[0] == vert[2] ? "oui" : "NON", ecart, kEcartAttendu);
		Cas("rendu/emission-verte", ok && vert[0] == vert[2] && ecart == kEcartAttendu, d);
	}

	// ── 3. le terme achromatique est bien LE MEME dans les deux rendus ───
	{
		// DISCRIMINE : c'est ce qui autorise a comparer les cas entre eux. Les
		// deux graphes ne different QUE par l'emission ; leur plancher gris doit
		// donc etre identique au bit. S'il ne l'etait pas, toutes les
		// comparaisons croisees qui suivent seraient sans valeur.
		NkString d;
		d = NkFormat("plancher gris rouge={0} vert={1} (identiques attendus)", rouge[2], vert[2]);
		Cas("rendu/plancher-achromatique-stable", rouge[2] == vert[2], d);
	}

	// ── 4. Mix Shader, facteur 0 puis 1 ──────────────────────────────────
	ok = GrapheMelange(0.f, src) && ctx.RendreEtLire(src, "matgraph_mix0", mix0);
	{
		// fac=0 doit rendre EXACTEMENT le premier shader. Comparaison des trois
		// canaux : un melange qui prendrait la moyenne quel que soit le facteur
		// donnerait un rouge a moitie, ce qu'un simple « R > V » laisserait
		// passer.
		const bool identique = mix0[0] == rouge[0] && mix0[1] == rouge[1] && mix0[2] == rouge[2];
		NkString d;
		d = NkFormat("pixel=({0},{1},{2}) | identique au rendu rouge seul : {3}", mix0[0], mix0[1], mix0[2], identique ? "oui" : "NON");
		Cas("rendu/melange-fac-0-donne-le-premier", ok && identique, d);
	}

	ok = GrapheMelange(1.f, src) && ctx.RendreEtLire(src, "matgraph_mix1", mix1);
	{
		const bool identique = mix1[0] == vert[0] && mix1[1] == vert[1] && mix1[2] == vert[2];
		NkString d;
		d = NkFormat("pixel=({0},{1},{2}) | identique au rendu vert seul : {3}", mix1[0], mix1[1], mix1[2], identique ? "oui" : "NON");
		Cas("rendu/melange-fac-1-donne-le-second", ok && identique, d);
	}

	// ── 5. Mix Shader a mi-chemin — LE CAS QUE RODOLF A DEMANDE ──────────
	ok = GrapheMelange(0.5f, src) && ctx.RendreEtLire(src, "matgraph_mixmoitie", mixMoitie);
	{
		// TROIS choses a la fois, et il en faut trois :
		//   R == V           : les deux emissions arrivent a parts egales ;
		//   ecart == 64      : la MOITIE de 128, calcule depuis le graphe ;
		//   plancher inchange: le terme achromatique n'a pas bouge, donc l'ecart
		//                      mesure bien l'emission et rien d'autre.
		// Un melange qui ignorerait le facteur rendrait 128 ; un melange
		// n'agissant que sur une composante casserait R == V.
		const int32 ecartR = (int32)mixMoitie[0] - (int32)mixMoitie[2];
		const int32 ecartV = (int32)mixMoitie[1] - (int32)mixMoitie[2];
		const bool plancher = mixMoitie[2] == rouge[2];
		NkString d;
		d = NkFormat("pixel=({0},{1},{2}) | R==V : {3} | ecarts R-B={4} V-B={5} (attendu {6}) | plancher intact : {7}", mixMoitie[0], mixMoitie[1], mixMoitie[2], mixMoitie[0] == mixMoitie[1] ? "oui" : "NON", ecartR, ecartV, kEcartAttendu / 2, plancher ? "oui" : "NON");
		Cas("rendu/melange-a-mi-chemin",
			ok && mixMoitie[0] == mixMoitie[1] && ecartR == kEcartAttendu / 2 && ecartV == kEcartAttendu / 2 &&
				plancher,
			d);
	}

	// ── 6. Mix Color : l'OPERATION choisie change le pixel, d'un facteur
	//    exactement calculable ──────────────────────────────────────────
	uint8 melange[4] = {}, eclairci[4] = {};
	ok = GrapheMelangeCouleur("melanger", 0.5f, src) && ctx.RendreEtLire(src, "matgraph_mixmelange", melange);
	{
		// « melanger » a fac = 0,5 : mix((E,0,0), (0,E,0), 0,5) = (E/2, E/2, 0).
		// Les deux ecarts valent donc la MOITIE de l'ecart de reference.
		const int32 dR = (int32)melange[0] - (int32)melange[2];
		const int32 dV = (int32)melange[1] - (int32)melange[2];
		Cas("rendu/mixcolor-melanger",
			ok && melange[0] == melange[1] && dR == kEcartAttendu / 2 && dV == kEcartAttendu / 2,
			NkFormat("pixel=({0},{1},{2}) | R==V : {3} | ecarts R-B={4} V-B={5} (attendu {6})", melange[0],
					 melange[1], melange[2], NkString(melange[0] == melange[1] ? "oui" : "NON"), dR, dV,
					 kEcartAttendu / 2));
	}

	ok = GrapheMelangeCouleur("eclaircir", 1.f, src) && ctx.RendreEtLire(src, "matgraph_mixeclair", eclairci);
	{
		// ⚠️ LE CAS QUI PROUVE QUE LA PROPRIETE PILOTE VRAIMENT LE CALCUL.
		// « eclaircir » a fac = 1 : max((E,0,0), (0,E,0)) = (E, E, 0). Les deux
		// ecarts valent donc l'ecart PLEIN — exactement le DOUBLE du cas
		// precedent. Un compilateur qui ignorerait l'operation et melangerait
		// toujours rendrait 64 ici comme la, et la difference entre les deux
		// cas est la seule chose qui le denonce.
		const int32 dR = (int32)eclairci[0] - (int32)eclairci[2];
		const int32 dV = (int32)eclairci[1] - (int32)eclairci[2];
		const int32 dRefR = (int32)melange[0] - (int32)melange[2];
		Cas("rendu/mixcolor-eclaircir-double-melanger",
			ok && eclairci[0] == eclairci[1] && dR == kEcartAttendu && dV == kEcartAttendu && dR == 2 * dRefR,
			NkFormat("pixel=({0},{1},{2}) | R==V : {3} | ecarts={4}/{5} (attendu {6}) | vaut le DOUBLE de "
					 "melanger : {7}",
					 eclairci[0], eclairci[1], eclairci[2], NkString(eclairci[0] == eclairci[1] ? "oui" : "NON"),
					 dR, dV, kEcartAttendu, NkString(dR == 2 * dRefR ? "oui" : "NON")));
	}

	// ── 7. ColorRamp : la charge VARIABLE atteint le GPU ─────────────────
	{
		// Deux arrets : rouge a 0, vert a 1. Trois lectures.
		const float32 deux[8] = {0.f, kE, 0.f, 0.f, 1.f, 0.f, kE, 0.f};
		uint8 a0[4] = {}, a1[4] = {}, am[4] = {};
		const bool c0 = GrapheRampe(deux, 8, 0.f, src) && ctx.RendreEtLire(src, "matgraph_ramp0", a0);
		const bool c1 = GrapheRampe(deux, 8, 1.f, src) && ctx.RendreEtLire(src, "matgraph_ramp1", a1);
		const bool cm = GrapheRampe(deux, 8, 0.5f, src) && ctx.RendreEtLire(src, "matgraph_rampm", am);
		const bool bornes = (a0[0] - a0[2] == kEcartAttendu) && a0[1] == a0[2] &&
							(a1[1] - a1[2] == kEcartAttendu) && a1[0] == a1[2];
		const bool milieu = am[0] == am[1] && (am[0] - am[2]) == kEcartAttendu / 2;
		Cas("rendu/rampe-deux-arrets", c0 && c1 && cm && bornes && milieu,
			NkFormat("fac=0 ({0},{1},{2}) | fac=1 ({3},{4},{5}) | fac=0,5 ({6},{7},{8}) | bornes={9} milieu={10}",
					 a0[0], a0[1], a0[2], a1[0], a1[1], a1[2], am[0], am[1], am[2], bornes ? 1 : 0,
					 milieu ? 1 : 0));
	}
	{
		// ⚠️ LE CAS QUI PROUVE LA CHARGE VARIABLE, ET LUI SEUL.
		// Trois arrets : rouge a 0, VERT AU MILIEU, rouge a 1. A fac=0,5 la
		// couleur doit valoir EXACTEMENT l'arret du milieu — vert plein.
		//
		// Un emetteur qui ne lirait que le PREMIER et le DERNIER arret — l'erreur
		// naturelle quand on traite une liste comme une paire — rendrait du rouge
		// interpole avec du rouge, donc (E, 0, 0) : R==128 et V==B. Les deux
		// resultats sont des couleurs parfaitement plausibles ; seul le canal ou
		// tombe l'ecart les distingue.
		const float32 trois[12] = {0.f, kE, 0.f, 0.f, 0.5f, 0.f, kE, 0.f, 1.f, kE, 0.f, 0.f};
		uint8 px[4] = {};
		const bool c = GrapheRampe(trois, 12, 0.5f, src) && ctx.RendreEtLire(src, "matgraph_ramp3", px);
		const int32 dV = (int32)px[1] - (int32)px[2];
		const bool arretDuMilieuHonore = px[0] == px[2] && dV == kEcartAttendu;
		Cas("rendu/rampe-arret-du-milieu-honore", c && arretDuMilieuHonore,
			NkFormat("pixel=({0},{1},{2}) | R==B : {3} | ecart V-B={4} (attendu {5} : l'arret du milieu, pas une "
					 "interpolation des bords)",
					 px[0], px[1], px[2], NkString(px[0] == px[2] ? "oui" : "NON"), dV, kEcartAttendu));
	}


	// ── 7bis. Float Curve : la SECONDE charge variable atteint le GPU ─────
	{
		// ⚠️ LE POINT DU MILIEU, AU PIXEL. Trois points : 0 en x=0, kE en x=0,5,
		// 0 en x=1. Lue en 0,5 la courbe doit rendre EXACTEMENT kE sur le canal
		// vert -- donc un ecart V-B de 128.
		//
		// Un emetteur qui ne lirait que le PREMIER et le DERNIER point -- l'erreur
		// naturelle quand on traite une liste comme une paire -- interpolerait 0
		// avec 0 et rendrait du NOIR : ecart 0. Les deux images sont plausibles,
		// seul l'ecart les distingue. C'est le pendant exact de
		// `rendu/rampe-arret-du-milieu-honore`, une charge variable plus loin.
		const float32 troisP[6] = {0.f, 0.f, 0.5f, kE, 1.f, 0.f};
		uint8 pm[4] = {}, p0[4] = {};
		const bool cm = GrapheCourbe(troisP, 6, 0.5f, src) && ctx.RendreEtLire(src, "matgraph_courbem", pm);
		// TEMOIN : lue en 0, la MEME courbe rend 0. Sans ce second tir, un
		// emetteur qui rendrait kE partout -- en ignorant l'abscisse -- passerait
		// le premier tir et serait faux partout ailleurs.
		const bool c0 = GrapheCourbe(troisP, 6, 0.f, src) && ctx.RendreEtLire(src, "matgraph_courbe0", p0);
		const int32 dV = (int32)pm[1] - (int32)pm[2];
		const int32 dV0 = (int32)p0[1] - (int32)p0[2];
		const bool milieuHonore = pm[0] == pm[2] && dV == kEcartAttendu;
		const bool temoinNoir = p0[0] == p0[2] && dV0 == 0;
		Cas("rendu/courbe-point-du-milieu-honore", cm && c0 && milieuHonore && temoinNoir,
			NkFormat("x=0,5 ({0},{1},{2}) ecart V-B={3} (attendu {4} : le point du milieu, pas une "
					 "interpolation des bords) | TEMOIN x=0 ({5},{6},{7}) ecart={8} (attendu 0)",
					 pm[0], pm[1], pm[2], dV, kEcartAttendu, p0[0], p0[1], p0[2], dV0));
	}

	// ── 8. le relief : la carte plate, puis le SENS de la bosse ─────────
	uint8 plate[4] = {}, geo[4] = {};
	{
		// ⚠️ LE PREMIER DES DEUX CONTROLES : une carte plate (0,5 ; 0,5 ; 1) doit
		// rendre EXACTEMENT la normale geometrique. On compare donc au meme
		// graphe dont la normale est imposee a (0, 0, 1) — trois canaux, au bit.
		//
		// Ce cas passe dans LES DEUX conventions : c'est justement pourquoi il ne
		// suffit pas, et pourquoi le suivant existe.
		const bool a = GrapheCarteNormales(0.5f, 0.5f, 1.f, src) && ctx.RendreEtLire(src, "matgraph_platte", plate);
		const bool b = GrapheNormaleImposee(0.f, 0.f, 1.f, src) && ctx.RendreEtLire(src, "matgraph_geo", geo);
		const bool egal = plate[0] == geo[0] && plate[1] == geo[1] && plate[2] == geo[2];
		Cas("rendu/carte-plate-rend-la-normale-geometrique", a && b && egal,
			NkFormat("carte plate ({0},{1},{2}) | normale geometrique imposee ({3},{4},{5}) | identiques={6}",
					 plate[0], plate[1], plate[2], geo[0], geo[1], geo[2], egal ? 1 : 0));
	}
	{
		// ⚠️ LE CONTROLE QUI ATTRAPE L'INVERSION, et il ne reimplante rien.
		//
		// On ETALONNE d'abord : le meme shader, le meme eclairage, mais une
		// normale imposee inclinee vers -x puis vers +x. Ces deux rendus donnent
		// les deux couleurs possibles SANS que le banc calcule quoi que ce soit.
		// Puis on rend le relief a force +1 et -1.
		//
		// Ce qui est exige : les deux reliefs DIFFERENT (sinon le signe est
		// ignore), et chacun tombe sur l'UN des deux etalons — donc le relief
		// incline la surface du bon cote, et l'inverser inverse l'eclairage.
		uint8 versMoinsX[4] = {}, versPlusX[4] = {}, bPlus[4] = {}, bMoins[4] = {};
		const bool e1 = GrapheNormaleImposee(-0.5f, 0.f, 1.f, src) &&
						ctx.RendreEtLire(src, "matgraph_etal_mx", versMoinsX);
		const bool e2 = GrapheNormaleImposee(0.5f, 0.f, 1.f, src) &&
						ctx.RendreEtLire(src, "matgraph_etal_px", versPlusX);
		const bool r1 = GrapheRelief(1.f, src) && ctx.RendreEtLire(src, "matgraph_bump_p", bPlus);
		const bool r2 = GrapheRelief(-1.f, src) && ctx.RendreEtLire(src, "matgraph_bump_m", bMoins);

		auto memePixel = [](const uint8 *a, const uint8 *b) {
			return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
		};
		const bool etalonsDifferent = !memePixel(versMoinsX, versPlusX);
		const bool reliefsDifferent = !memePixel(bPlus, bMoins);
		// ⚠️ L'APPARIEMENT EST ORIENTE, et il DOIT l'etre. Ma premiere version
		// acceptait « chacun tombe sur l'un des deux etalons », dans n'importe
		// quel ordre — une INVERSION GLOBALE du signe du relief l'aurait donc
		// passee au vert, puisque les deux resultats se contentent d'echanger.
		// C'est exactement le defaut que ce cas existe pour attraper.
		//
		// Le sens se DEDUIT DU GRAPHE, sans reimplanter le moindre ombrage : la
		// hauteur vaut `u`, qui croit avec +x ; son gradient pointe donc vers +x ;
		// et la formule du relief incline la normale a L'OPPOSE du gradient
		// (N = geomN - force x distance x gradient). Une force POSITIVE doit donc
		// rendre l'etalon incline vers **-x**, et une force negative l'autre.
		const bool appariement = memePixel(bPlus, versMoinsX) && memePixel(bMoins, versPlusX);
		Cas("rendu/relief-incline-du-bon-cote",
			e1 && e2 && r1 && r2 && etalonsDifferent && reliefsDifferent && appariement,
			NkFormat("etalons -x({0},{1},{2}) +x({3},{4},{5}) differents={6} | relief +1({7},{8},{9}) "
					 "-1({10},{11},{12}) differents={13} | force +1 tombe sur l etalon -x et -1 sur +x={14}",
					 versMoinsX[0], versMoinsX[1], versMoinsX[2], versPlusX[0], versPlusX[1], versPlusX[2],
					 etalonsDifferent ? 1 : 0, bPlus[0], bPlus[1], bPlus[2], bMoins[0], bMoins[1], bMoins[2],
					 reliefsDifferent ? 1 : 0, appariement ? 1 : 0));
	}

	{
		// ⚠️ CE CAS EXISTE PARCE QU'UNE MUTATION A SURVECU. La carte PLATE ne peut
		// pas voir une base tangente cassee : avec un texel (0,5 ; 0,5 ; 1) la
		// normale tangente vaut (0, 0, 1), donc T et B sont multiplies par ZERO et
		// n'interviennent pas. Le cas precedent est vrai et necessaire — il ne
		// suffit pas.
		//
		// On rend donc une carte INCLINEE : texel (0,75 ; 0,5 ; 1) donne une
		// normale tangente (0,5 ; 0 ; 1), qui penche vers +u — c'est-a-dire vers
		// +x, puisque l'UV croit avec x. Elle doit tomber du cote de l'etalon +x,
		// donc etre PLUS SOMBRE que la carte plate.
		uint8 penchee[4] = {}, plateBis[4] = {}, etalonPlusX[4] = {};
		const bool a = GrapheCarteNormales(0.75f, 0.5f, 1.f, src) &&
					   ctx.RendreEtLire(src, "matgraph_nm_penchee", penchee);
		const bool b = GrapheCarteNormales(0.5f, 0.5f, 1.f, src) &&
					   ctx.RendreEtLire(src, "matgraph_nm_plate", plateBis);
		const bool c = GrapheNormaleImposee(0.5f, 0.f, 1.f, src) &&
					   ctx.RendreEtLire(src, "matgraph_nm_etal", etalonPlusX);
		// La base tangente agit : la carte penchee differe de la plate.
		const bool agit = a && b && (penchee[0] != plateBis[0]);
		// Et elle penche du BON cote : vers +x, donc du cote sombre.
		const bool bonCote = a && b && c && penchee[0] < plateBis[0] && penchee[0] >= etalonPlusX[0];
		Cas("rendu/carte-penchee-suit-la-base-tangente", agit && bonCote,
			NkFormat("penchee({0}) | plate({1}) | etalon +x({2}) | la base tangente agit={3} | penche du cote +x, "
					 "donc plus sombre que plate et pas au-dela de l etalon={4}",
					 penchee[0], plateBis[0], etalonPlusX[0], agit ? 1 : 0, bonCote ? 1 : 0));
	}

	// ── 9. LE PROCEDURAL : un damier dont on predit la case ─────────────
	{
		// ⚠️ CE CAS NE MESURE PAS UN EFFET, IL MESURE UN CALCUL. Contrairement aux
		// precedents, l'attendu ne vient pas d'une relation entre canaux mais de
		// l'arithmetique du damier elle-meme — et il designe LAQUELLE des deux
		// couleurs doit sortir. Une erreur d'un demi-carreau donnerait l'autre.
		uint8 paire[4] = {}, impaire[4] = {};
		const bool a = GrapheDamier(0.f, src) && ctx.RendreEtLire(src, "matgraph_dam_p", paire);
		const bool b = GrapheDamier(0.2f, src) && ctx.RendreEtLire(src, "matgraph_dam_i", impaire);
		// Case PAIRE -> color2 (vert) : l'ecart tombe sur le VERT.
		const bool casePaire = paire[0] == paire[2] && (paire[1] - paire[2]) == kEcartAttendu;
		// Case IMPAIRE -> color1 (rouge) : l'ecart tombe sur le ROUGE.
		const bool caseImpaire = impaire[1] == impaire[2] && (impaire[0] - impaire[2]) == kEcartAttendu;
		Cas("rendu/damier-la-bonne-case", a && b && casePaire && caseImpaire,
			NkFormat("sans decalage ({0},{1},{2}) : case paire -> vert attendu={3} | decale de 0,2 ({4},{5},{6}) : "
					 "case impaire -> rouge attendu={7}",
					 paire[0], paire[1], paire[2], casePaire ? 1 : 0, impaire[0], impaire[1], impaire[2],
					 caseImpaire ? 1 : 0));
	}

	// ── 10. le fond de repli n'a jamais ete lu ───────────────────────────
	{
		// ⚠️ LE CAS QUI EMPECHE DE SE REJOUIR TROP VITE. Si le trace avait
		// echoue sans le dire, on lirait le MAGENTA d'effacement. Comme il est
		// impossible a produire par ces graphes (B vaut le plancher gris, jamais
		// 255), sa presence signerait un rendu qui n'a pas eu lieu.
		const bool magenta = rouge[0] == 255 && rouge[1] == 0 && rouge[2] == 255;
		NkString d;
		d = NkFormat("premier pixel=({0},{1},{2}) | magenta d'effacement lu : {3}", rouge[0], rouge[1], rouge[2], magenta ? "OUI -- le trace n'a pas eu lieu" : "non");
		Cas("rendu/le-trace-a-bien-eu-lieu", !magenta, d);
	}

	// ── 11. LES PARAMETRES EXPOSES PILOTENT LE PIXEL ─────────────────────
	{
		// ⚠️ LA FORME EST CELLE D'ORIGINE, PAS UNE PLUS SIMPLE. Deux valeurs
		// ecrites dans le MEME tampon entre deux tirs, pipeline et sets montes
		// une seule fois. Un cas qui ne rendrait qu'une fois prouverait que le
		// bloc arrive ; il ne prouverait PAS qu'un parametre PILOTE quoi que ce
		// soit, et c'est toute la question posee depuis trois jours.
		//
		// L'attendu est calculable et sans tolerance : albedo nul annule le
		// diffus, et le speculaire est ACHROMATIQUE quand metallic vaut zero --
		// il s'annule donc dans un ecart entre canaux. Ce qui reste est
		// exactement la valeur ecrite dans le bloc.
		//
		// 🔴 DEUX VARIANTES, ET C'EST LA COMPARAISON QUI EST LA MESURE.
		// Un seul parametre expose (un vec3 seul, bloc de 16 octets) contre
		// deux (un reel PUIS un vec3, bloc de 32). Tout le reste est identique
		// -- meme graphe, meme cablage, meme code, meme tir. Une variante qui
		// passe pendant que l'autre tombe designe l'axe elle-meme, sans qu'on
		// ait a le deviner.
		const float32 valA[3] = {kE, 0.f, 0.f};			   // rouge 128/255
		const float32 valB[3] = {0.f, 192.f / 255.f, 0.f}; // vert  192/255
		bool toutVert = true;
		NkString detail;
		for (uint32 variante = 0; variante < 2; ++variante) {
			const bool avecReel = (variante == 1);
			NkMatCompileResult rp;
			const bool compile = GrapheEmissionExposee(rp, avecReel);
			uint8 pa[4] = {}, pb[4] = {};
			NkString pourquoi;
			if (compile && getenv("NK_DUMP_PARAMS")) {
				// fwrite, PAS le journal : une source de shader est pleine
				// d accolades litterales, et le formateur les detruit.
				FILE *f = fopen(avecReel ? "mesures_matgraph/params_2.nksl" : "mesures_matgraph/params_1.nksl", "wb");
				if (f) {
					fwrite(rp.source.CStr(), 1, (size_t)rp.source.Size(), f);
					fclose(f);
				}
				// Et le HLSL REELLEMENT produit : c est lui que la carte execute,
				// pas le NkSL. Les regles de rangement d un cbuffer HLSL ne sont
				// PAS celles de std140.
				NkSLCompiler cc;
				NkSLCompileResult hr = cc.Compile(rp.source, NkSLStage::NK_FRAGMENT, NkSLTarget::NK_HLSL_DX11);
				FILE *fh = fopen(avecReel ? "mesures_matgraph/params_2.hlsl" : "mesures_matgraph/params_1.hlsl", "wb");
				if (fh) {
					fwrite(hr.source.CStr(), 1, (size_t)hr.source.Size(), fh);
					fclose(fh);
				}
			}
			const bool rendu = compile && RendreParamsDeuxValeurs(ctx, rp, avecReel ? "matgraph_params2" : "matgraph_params1",
																  "teinte", valA, valB, pa, pb, pourquoi);
			const int32 ecartA = (int32)pa[0] - (int32)pa[1];
			const int32 ecartB = (int32)pb[1] - (int32)pb[0];
			const bool bonA = rendu && pa[1] == pa[2] && ecartA == kEcartAttendu;
			const bool bonB = rendu && pb[0] == pb[2] && ecartB == 192;
			// Deux tirs identiques signeraient un tampon jamais relu par la
			// carte -- precisement la panne qu'on traque.
			const bool ontChange = rendu && !(pa[0] == pb[0] && pa[1] == pb[1] && pa[2] == pb[2]);
			const bool ok = bonA && bonB && ontChange;
			toutVert = toutVert && ok;
			const NkMatParamExpose *pt = rp.ok ? rp.TrouveParam("teinte") : nullptr;
			detail.Append(NkFormat("[{0} param(s), bloc {1}o, teinte au decalage {2} : tir1=({3},{4},{5}) "
								   "ecart={6} | tir2=({7},{8},{9}) ecart={10} | different={11} -> {12}] ",
								   (uint32)rp.params.Size(), rp.paramsTaille, pt ? pt->decalage : 9999u,
								   pa[0], pa[1], pa[2], ecartA, pb[0], pb[1], pb[2], ecartB,
								   ontChange ? 1 : 0, NkString(ok ? "OK" : "ECHEC")));
		}
		Cas("rendu/parametre-expose-pilote-le-pixel", toutVert, detail);
	}

	ctx.Demonter();
	logger.Info("\n-- {0} cas, {1} echec(s) --", gCas, gEchecs);
	return gEchecs == 0 ? 0 : 1;
}
