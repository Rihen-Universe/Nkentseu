// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_ubo_mouillage.cpp — LA SONDE DU BLOC UNIFORME PARTAGÉ, et le témoin de la
// formule de mouillage (2026-09-07, nuit).
//
// POURQUOI ELLE EXISTE. Brancher le mouillage sur le nuanceur PBR veut dire
// agrandir `ObjectUBO`, un bloc std140 partagé par TOUT le rendu 3D et recopié à
// la main dans huit endroits du C++ et dans une dizaine de fichiers de nuanceur.
// Le dépôt a déjà payé quatre fois le même mécanisme (`NkGLTFNode::name` déclaré
// deux fois) : deux branches ajoutent un membre à deux ENDROITS différents, git
// ne voit aucun conflit, et la panne sort loin de sa cause. Ici la panne serait
// pire que bruyante : elle serait SILENCIEUSE — un bloc dont la disposition
// diverge d'un étage à l'autre ne provoque ni erreur ni avertissement, il rend
// simplement des valeurs décalées.
//
// ⚠️ CE QUE CETTE SONDE PROUVE, ET CE QU'ELLE NE PROUVE PAS. Elle lit du TEXTE et
// des sorties de GÉNÉRATEUR ; elle ne lance aucun GPU, n'ouvre aucune fenêtre et
// ne lit aucun pixel. Elle prouve que les déclarations concordent et que la
// formule écrite dans le nuanceur est celle qui est prouvée côté C++. Elle ne
// prouve PAS que le pilote a exécuté quoi que ce soit. Cette limite est écrite
// ici, dans l'en-tête de l'instrument, comme le corpus l'exige — pas dans un
// document voisin.
//
// ⚠️ ET « ÇA COMPILE SUR LES QUATRE BACKENDS » NE PROUVE RIEN : le corpus le dit
// noir sur blanc, `NkSLCompiler::Compile` vers GL/VK-GLSL/DX11/DX12 n'appelle
// que le GÉNÉRATEUR. C'est pourquoi (u5) ne se contente pas de `success` : il
// relit le bloc uniforme DANS LE TEXTE PRODUIT et vérifie sa séquence de types.
// Un générateur qui perdrait un membre passerait `success` et tomberait ici.
// Seule la cible SPIR-V traverse un vrai compilateur (glslang), et (u6) la lit
// AU MOT MAGIQUE, jamais à `success` — un repli qui préserve `success` en
// remettant du texte GLSL dans `bytecode` est un mensonge connu de ce dépôt.
//
// LES TÉMOINS :
//   (u0) la sonde TROUVE son sujet (racine du dépôt, fichiers présents) — sinon
//        elle ÉCHOUE. Une garde qui ne trouve pas son sujet doit échouer, jamais
//        applaudir.
//   (u1) la référence : 15 membres, séquence de types, 240 octets en std140.
//   (u2) le C++ : `ObjectUBO` et les SEPT copies locales `ObjBlock` de
//        NkRender3D.cpp portent la même séquence, et chacune porte sa garde de
//        taille à 240.
//   (u3) les DEUX nuanceurs VIVANTS (NkSL vert + frag) portent la même séquence.
//   (u4) et ils portent les MÊMES NOMS de membres — règle GLSL du bloc partagé :
//        deux étages qui déclarent le même bloc avec des noms différents ne lient
//        pas. C'est le seul endroit où les noms comptent.
//   (u5) les CINQ générateurs (GL, Vulkan-GLSL, DX11, DX12, MSL) réémettent le
//        bloc avec la même séquence de types, LUE DANS LE TEXTE PRODUIT.
//   (u6) la cible SPIR-V rend le mot magique 0x07230203.
//   (u7) INVENTAIRE des déclarations MORTES : les copies qu'aucun chargeur
//        n'ouvre. Elles sont COMPTÉES et NOMMÉES, et elles ne font pas rougir —
//        les faire rougir masquerait les vraies.
//   (w1) à sec, la formule de mouillage est l'IDENTITÉ EXACTE (bit pour bit).
//   (w2) la forme RÉDUITE que le nuanceur calcule est ÉGALE à `NkApplyWetness`
//        sur une grille — c'est ce qui transporte la preuve du C++ au GLSL.
//   (w3) et le nuanceur écrit bien cette forme-là : la ligne est relue dans le
//        fichier. Sans (w3), (w2) prouverait une formule que personne n'écrit.
//
// QUI EST « VIVANT », mesuré et non supposé — `NkShaderLibrary::LoadOrCompileVF`
// cherche dans cet ordre : (1) Resources/NKRenderer/Shaders/PBR/NkSL/
// pbr.{vert,frag}.nksl, (2) .../PBR/VK/pbr.{vert,frag}.vk.glsl, (3) les sources
// embarquées kPBR_VS/kPBR_FS. Les deux fichiers de (1) existent, donc ce sont eux
// qui tournent. Les dossiers PBR/GL, PBR/DX11, PBR/DX12, PBR/MSL ne sont ouverts
// par PERSONNE : c'est le générateur NkSL qui produit ces langages.
// =============================================================================
#include "NKFileSystem/NkFile.h"
#include "NKMath/NkFunctions.h"
#include "NKMath/NkWetnessMap.h"
#include "NKRenderer/Shader/NkShaderIncludeResolver.h"
#include "NKSL/Compiler/NkSLCompiler.h"
#include "NKSL/Core/NkSLTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include <cstdio>

using namespace nkentseu;

namespace {

	int gP = 0, gF = 0;

	void UCHECK(bool ok, const char *quoi) {
		if (ok) {
			++gP;
			std::fprintf(stderr, "  [ok]    %s\n", quoi);
		} else {
			++gF;
			std::fprintf(stderr, "  [ROUGE] %s\n", quoi);
		}
	}

	// ── Le vocabulaire de types, commun aux six langages ────────────────────
	// M = matrice 4x4, V = vecteur 4, F = scalaire. C'est tout ce dont std140 a
	// besoin pour poser les offsets : les NOMS ne changent aucun octet, et ils
	// diffèrent réellement d'un langage à l'autre (`clearcoatRough` en C++,
	// `clearcoatRoughness` en NkSL, `ccRough` dans un repli). Juger sur les noms
	// aurait produit un rouge qui ne dit rien sur la mémoire.
	const char *kRef = "MMVFFFFFFFFVVVV"; // 15 membres
	const uint32 kRefTaille = 240u;

	// std140 : M4 -> align 16 taille 64 ; V4 -> align 16 taille 16 ;
	//          F  -> align 4  taille 4  ; le total est arrondi au multiple de 16.
	uint32 TailleStd140(const char *seq) {
		uint32 off = 0;
		for (uint32 i = 0; seq[i]; ++i) {
			uint32 a = (seq[i] == 'F') ? 4u : 16u;
			uint32 t = (seq[i] == 'M') ? 64u : ((seq[i] == 'V') ? 16u : 4u);
			off = ((off + a - 1u) / a) * a;
			off += t;
		}
		return ((off + 15u) / 16u) * 16u;
	}

	// ── Lecture de fichier, et la racine du dépôt ───────────────────────────
	// `basePath` du moteur est relatif au RÉPERTOIRE COURANT : ce banc peut donc
	// être lancé d'ailleurs. On remonte jusqu'à trouver `Nkentseu.jenga`, et on
	// ÉCHOUE si on ne le trouve pas — un instrument qui ne trouve pas son sujet
	// ne doit pas rendre un verdict.
	NkString gRacine;

	bool TrouveRacine() {
		NkString p = "";
		for (uint32 k = 0; k < 8u; ++k) {
			NkString marqueur = p;
			marqueur += "Nkentseu.jenga";
			if (NkFile::Exists(marqueur.CStr())) {
				gRacine = p;
				return true;
			}
			p += "../";
		}
		return false;
	}

	NkString Lire(const char *relatif) {
		NkString p = gRacine;
		p += relatif;
		if (!NkFile::Exists(p.CStr()))
			return NkString("");
		return NkFile::ReadAllText(p.CStr());
	}

	// ── Retrait des commentaires ────────────────────────────────────────────
	// Indispensable, et pas une précaution de style : les blocs que nous lisons
	// portent des commentaires qui CITENT des noms de membres et des types. Sans
	// ce retrait, la sonde compterait ses propres explications.
	NkString SansCommentaires(const NkString &src) {
		NkString out;
		const char *s = src.CStr();
		const uint32 n = (uint32)src.Size();
		for (uint32 i = 0; i < n;) {
			if (s[i] == '/' && i + 1 < n && s[i + 1] == '/') {
				while (i < n && s[i] != '\n')
					++i;
			} else if (s[i] == '/' && i + 1 < n && s[i + 1] == '*') {
				i += 2;
				while (i + 1 < n && !(s[i] == '*' && s[i + 1] == '/'))
					++i;
				i += 2;
			} else {
				out += s[i];
				++i;
			}
		}
		return out;
	}

	bool EstAlnum(char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
	}

	// Extrait le corps { ... } du n-ième bloc dont l'entête contient `nom`.
	// Rend faux quand il n'y en a pas d'autre : c'est ainsi qu'on ÉNUMÈRE les
	// sept copies sans en supposer le nombre.
	bool CorpsDuBloc(const NkString &texteSansCom, const char *nom, uint32 index, NkString &corps) {
		const char *s = texteSansCom.CStr();
		const uint32 n = (uint32)texteSansCom.Size();
		uint32 lnom = 0;
		while (nom[lnom])
			++lnom;
		uint32 vus = 0;
		for (uint32 i = 0; i + lnom <= n; ++i) {
			bool eq = true;
			for (uint32 k = 0; k < lnom && eq; ++k)
				eq = (s[i + k] == nom[k]);
			if (!eq)
				continue;
			// jeton entier, jamais une sous-chaîne (le corpus a payé « d= » trouvé
			// dans « id= » : on cherche un JETON, pas un `contains`).
			if (i > 0 && EstAlnum(s[i - 1]))
				continue;
			if (i + lnom < n && EstAlnum(s[i + lnom]))
				continue;
			// On saute ce qui separe le nom de son accolade : un saut de ligne
			// (GLSL), un `: register(b1)` (HLSL), rien (MSL). On s arrete sur un
			// `;` ou une virgule -- c est la que `ObjBlock ob{};` doit etre ecarte,
			// et c'est pourquoi l'appelant passe « struct ObjBlock » et pas
			// « ObjBlock » : sans le mot-cle, la sonde comptait 16 copies pour 7.
			uint32 j = i + lnom, garde = 0;
			// ⚠️ ON NE S'ARRETE PAS SUR UNE VIRGULE : le HLSL SM6 ecrit
			// `register(b1, space0)`. S'y arreter faisait rendre a la sonde un FAUX
			// ROUGE sur DX12 -- pendant que DX11, qui ecrit `register(b1)`, passait.
			// Deux backends, un seul defaut, et il etait dans l'instrument.
			while (j < n && s[j] != '{' && s[j] != ';' && garde < 160u) {
				++j;
				++garde;
			}
			if (j >= n || s[j] != '{')
				continue; // une mention, pas une déclaration
			if (vus++ != index) {
				i = j;
				continue;
			}
			uint32 prof = 0, d = j;
			for (; d < n; ++d) {
				if (s[d] == '{')
					++prof;
				else if (s[d] == '}') {
					if (--prof == 0)
						break;
				}
			}
			if (d >= n)
				return false;
			corps = texteSansCom.SubStr(j + 1, d - j - 1);
			return true;
		}
		return false;
	}

	// Séquence de types d'un corps de bloc. Gère `mat4 a, b;` et `float x,y,z,w;`
	// et les orthographes HLSL/MSL (float4x4, float4).
	NkString SequenceTypes(const NkString &corps) {
		NkString seq;
		const char *s = corps.CStr();
		const uint32 n = (uint32)corps.Size();
		uint32 i = 0;
		while (i < n) {
			while (i < n && !EstAlnum(s[i]))
				++i;
			uint32 d = i;
			while (i < n && EstAlnum(s[i]))
				++i;
			if (i == d)
				break;
			NkString mot = corps.SubStr(d, i - d);
			char t = 0;
			// Les SIX orthographes du meme type. Le C++ ecrit NkMat4f/NkVec4f/float32,
			// le GLSL mat4/vec4/float, le HLSL et le MSL float4x4/float4/float. Ne
			// connaitre qu'un dialecte faisait sortir une sequence VIDE, donc un rouge
			// qui accusait le code au lieu de la sonde.
			if (mot == "mat4" || mot == "float4x4" || mot == "NkMat4f")
				t = 'M';
			else if (mot == "vec4" || mot == "float4" || mot == "NkVec4f")
				t = 'V';
			else if (mot == "float" || mot == "float32")
				t = 'F';
			if (t == 0)
				continue; // un identifiant, une qualification (layout, std140…)
			// Compte les déclarateurs jusqu'au `;` : un par virgule, plus un.
			uint32 k = i, decl = 1;
			bool crochet = false;
			while (k < n && s[k] != ';') {
				if (s[k] == ',')
					++decl;
				if (s[k] == '[')
					crochet = true;
				++k;
			}
			if (crochet) {
				// Un tableau n'appartient pas à cette disposition : on le signale en
				// posant un type inconnu, qui fera diverger la séquence bruyamment
				// plutôt que de se taire.
				seq += '?';
				i = k;
				continue;
			}
			for (uint32 q = 0; q < decl; ++q)
				seq += t;
			i = k;
		}
		return seq;
	}

	// Les NOMS déclarés, dans l'ordre (dernier identifiant avant `,` ou `;`).
	NkString NomsDuBloc(const NkString &corps) {
		NkString out;
		const char *s = corps.CStr();
		const uint32 n = (uint32)corps.Size();
		uint32 i = 0;
		while (i < n) {
			uint32 fin = i;
			while (fin < n && s[fin] != ';')
				++fin;
			// dans [i, fin) : découpe par virgules, garde le dernier jeton de chaque
			uint32 d = i;
			while (d <= fin) {
				uint32 e = d;
				while (e < fin && s[e] != ',')
					++e;
				// dernier jeton de [d, e)
				uint32 b = e;
				while (b > d && !EstAlnum(s[b - 1]))
					--b;
				uint32 a = b;
				while (a > d && EstAlnum(s[a - 1]))
					--a;
				if (b > a) {
					NkString mot = corps.SubStr(a, b - a);
					if (mot != "mat4" && mot != "vec4" && mot != "float" && mot != "float4" &&
						mot != "float4x4" && mot != "NkMat4f" && mot != "NkVec4f" && mot != "float32") {
						if (!out.Empty())
							out += " ";
						out += mot;
					}
				}
				if (e >= fin)
					break;
				d = e + 1;
			}
			i = fin + 1;
		}
		return out;
	}

	// ── Un site de déclaration ──────────────────────────────────────────────
	struct Site {
			const char *chemin;
			const char *bloc;
			const char *quoi;
	};

	bool VerifieSite(const NkString &texte, const char *nomBloc, uint32 index, const char *quoi, bool vivant,
					 NkString *nomsSortie) {
		NkString corps;
		if (!CorpsDuBloc(texte, nomBloc, index, corps)) {
			if (vivant)
				UCHECK(false, quoi);
			return false;
		}
		NkString seq = SequenceTypes(corps);
		const bool ok = (seq == kRef);
		if (!ok)
			std::fprintf(stderr, "     %-46s sequence '%s' (attendu '%s', %u octets std140 au lieu de %u)\n", quoi,
						 seq.CStr(), kRef, TailleStd140(seq.CStr()), kRefTaille);
		if (nomsSortie)
			*nomsSortie = NomsDuBloc(corps);
		if (vivant)
			UCHECK(ok, quoi);
		return ok;
	}

	// ── (u5)(u6) les générateurs ────────────────────────────────────────────
	void TemoinGenerateurs(const NkString &srcVert, const NkString &srcFrag) {
		NkSLCompiler c;
		struct Cible {
				NkSLTarget t;
				const char *nom;
		};
		const Cible cibles[5] = {
			{NkSLTarget::NK_GLSL, "GLSL-OpenGL"},	  {NkSLTarget::NK_GLSL_VULKAN, "GLSL-Vulkan"},
			{NkSLTarget::NK_HLSL_DX11, "HLSL-DX11"},  {NkSLTarget::NK_HLSL_DX12, "HLSL-DX12"},
			{NkSLTarget::NK_MSL, "MSL-Metal"},
		};
		// ⚠️ TOLERANCE DATEE, ET ELLE SE RESORBE TOUTE SEULE. Au 2026-09-07, le
		// generateur MSL n'emet AUCUNE declaration de bloc uniforme : son point
		// d'entree prend `constant ObjectUBO& uObj [[buffer(1)]]` et le fichier
		// produit ne contient que trois `struct` (NkShadowSlot, FragIn, FragOut).
		// Ce Metal ne compile pas, et ce n'est pas le mouillage qui l'a casse --
		// c'est l'etat connu du depot (NkSL : cinq backends sur six).
		// La regle du corpus s'applique : une tolerance s'ecrit `<=`, jamais `==`,
		// pour qu'elle laisse passer sa propre disparition. Le jour ou MSL declarera
		// ses blocs, ce temoin verdira SANS qu'on y touche.
		uint32 manquants = 0, manquantsHorsMSL = 0;
		for (uint32 i = 0; i < 5u; ++i) {
			for (uint32 e = 0; e < 2u; ++e) {
				const NkString &src = (e == 0u) ? srcVert : srcFrag;
				const NkSLStage st = (e == 0u) ? NkSLStage::NK_VERTEX : NkSLStage::NK_FRAGMENT;
				NkSLCompileResult r = c.Compile(src, st, cibles[i].t);
				// ⚠️ On ne juge PAS sur `r.success` : le corpus a mesuré qu'un shader
				// appelant une fonction inexistante rend `success` sur ces cibles. Le
				// verdict porte sur le TEXTE produit.
				NkString sansCom = SansCommentaires(r.source);
				NkString corps;
				char quoi[192];
				std::snprintf(quoi, sizeof(quoi), "(u5) %-11s %-8s : le bloc ObjectUBO genere garde les 15 membres",
							  cibles[i].nom, (e == 0u) ? "vertex" : "fragment");
				if (!CorpsDuBloc(sansCom, "ObjectUBO", 0, corps)) {
					std::fprintf(stderr, "     %s : aucun bloc ObjectUBO dans %u octets generes (success=%d)\n",
								 cibles[i].nom, (unsigned)r.source.Size(), r.success ? 1 : 0);
					// Le texte produit est DEPOSE : un rouge qui ne laisse pas de quoi
					// comprendre oblige a relancer, et la relance est justement ce qu'on
					// ne peut pas faire quand personne ne regarde.
					{
						char dest[256];
						std::snprintf(dest, sizeof(dest), "Build/Tests/sonde_ubo_%s_%s.txt", cibles[i].nom,
									  (e == 0u) ? "vertex" : "fragment");
						NkFile::WriteAllText(dest, r.source);
						std::fprintf(stderr, "        texte genere depose dans %s\n", dest);
					}
					++manquants;
					if (cibles[i].t != NkSLTarget::NK_MSL)
						++manquantsHorsMSL;
					// Pas de UCHECK ici : le verdict est rendu APRES la boucle, sur
					// l'ENSEMBLE des backends. Un rouge par backend aurait fige la
					// dette MSL en six lignes rouges permanentes, et six rouges
					// permanents se desarment.
					continue;
				}
				NkString seq = SequenceTypes(corps);
				if (seq != kRef) {
					std::fprintf(stderr, "     %s %s : sequence generee '%s'\n", cibles[i].nom,
								 (e == 0u) ? "VS" : "FS", seq.CStr());
					++manquants;
					if (cibles[i].t != NkSLTarget::NK_MSL)
						++manquantsHorsMSL;
					continue;
				}
				UCHECK(true, quoi);
			}
		}
		std::fprintf(stderr,
					 "     (u5) backends qui ne reemettent PAS le bloc : %u au total, dont %u hors MSL\n",
					 manquants, manquantsHorsMSL);
		UCHECK(manquantsHorsMSL == 0u,
			   "(u5) AUCUN backend hors MSL ne perd le bloc uniforme (MSL : dette datee, cf. en-tete)");
		UCHECK(manquants <= 2u,
			   "(u5b) et le manque MSL ne s'est pas ETENDU : au plus les deux etages Metal");

		// (u6) SPIR-V : le SEUL chemin qui traverse un vrai compilateur (glslang).
		// Lu AU MOT MAGIQUE. `success` reste vrai sur un repli qui remet du texte
		// GLSL dans `bytecode` — c'est écrit dans le corpus, et c'est la raison de
		// cette ligne plutôt qu'un `r.success`.
		for (uint32 e = 0; e < 2u; ++e) {
			const NkString &src = (e == 0u) ? srcVert : srcFrag;
			const NkSLStage st = (e == 0u) ? NkSLStage::NK_VERTEX : NkSLStage::NK_FRAGMENT;
			NkSLCompileResult r = c.Compile(src, st, NkSLTarget::NK_SPIRV);
			bool magique = false;
			if (r.bytecode.Size() >= 4u) {
				const uint32 m = (uint32)r.bytecode[0] | ((uint32)r.bytecode[1] << 8) |
								 ((uint32)r.bytecode[2] << 16) | ((uint32)r.bytecode[3] << 24);
				magique = (m == 0x07230203u);
			}
			char quoi[160];
			std::snprintf(quoi, sizeof(quoi),
						  "(u6) SPIR-V %-8s : mot magique 0x07230203 (glslang, PAS le drapeau success)",
						  (e == 0u) ? "vertex" : "fragment");
			std::fprintf(stderr, "     SPIR-V %s : %u octets, success=%d, magique=%s\n",
						 (e == 0u) ? "VS" : "FS", (unsigned)r.bytecode.Size(), r.success ? 1 : 0,
						 magique ? "oui" : "NON");
			UCHECK(magique, quoi);
		}
	}

	// ── (w1)(w2)(w3) la formule ─────────────────────────────────────────────
	// La forme RÉDUITE, écrite ici EXACTEMENT comme le nuanceur l'écrit. C'est
	// l'unique endroit où les deux se rejoignent, et (w3) va relire le fichier
	// pour vérifier que le nuanceur dit bien ça.
	void FormeReduite(float32 wetK, float32 waterK, const math::NkVec3f &albDry, float32 rogDry, float32 f0Dry,
					  math::NkVec3f &alb, float32 &rog, float32 &f0) {
		alb = albDry * (1.f - 0.8f * wetK);
		rog = math::NkClamp(rogDry * (1.f - 0.4f * wetK), 0.f, 1.f);
		const float32 nWater = 1.33f;
		const float32 f0Water = ((nWater - 1.f) / (nWater + 1.f)) * ((nWater - 1.f) / (nWater + 1.f));
		f0 = f0Dry + (f0Water - f0Dry) * waterK;
	}

	void TemoinFormule(const NkString &fragBrut) {
		using namespace nkentseu::math;

		// (w1) À SEC, L'IDENTITÉ EXACTE. Pas « à 1e-6 près » : bit pour bit. C'est
		// ce qui garantit qu'une scène qui n'alimente pas le champ rend exactement
		// l'image d'avant — le sens d'erreur choisi.
		{
			const NkVec3f alb0 = {0.7f, 0.31f, 0.9f};
			NkVec3f a;
			float32 r, f;
			FormeReduite(0.f, 0.f, alb0, 0.42f, 0.04f, a, r, f);
			const bool exact = (a.x == alb0.x && a.y == alb0.y && a.z == alb0.z && r == 0.42f && f == 0.04f);
			UCHECK(exact, "(w1) a sec, la formule du nuanceur est l'IDENTITE EXACTE (bit pour bit)");
		}

		// (w2) LA RÉDUCTION EST VRAIE. Le nuanceur ne reçoit pas `porosite` et
		// `wet` : il reçoit leur produit. Ce témoin est la charnière de toute la
		// preuve — si la réduction était fausse, le nuanceur calculerait autre
		// chose que `NkApplyWetness`, qui est la fonction dont les témoins (d) de
		// NKPhysics_Tests garantissent qu'elle est bien Lagarde 3b.
		{
			float32 pire = 0.f;
			float32 pireF0 = 0.f;
			for (uint32 iw = 0; iw <= 10u; ++iw) {
				for (uint32 ip = 0; ip <= 10u; ++ip) {
					for (uint32 il = 0; il <= 4u; ++il) {
						const float32 w = (float32)iw * 0.1f;
						const float32 por = (float32)ip * 0.1f;
						const float32 lay = (float32)il * 0.25f;
						NkWetMaterial m;
						m.porosity = por;
						m.waterLayer = lay;
						const NkVec3f albDry = {0.62f, 0.44f, 0.21f};
						const NkWetShading ref = NkApplyWetness(albDry, 0.53f, 0.04f, w, m);
						NkVec3f a;
						float32 r, f;
						FormeReduite(w * por, w * lay, albDry, 0.53f, 0.04f, a, r, f);
						const float32 e = NkMax(NkMax(NkFabs(a.x - ref.albedo.x), NkFabs(a.y - ref.albedo.y)),
												NkMax(NkFabs(a.z - ref.albedo.z), NkFabs(r - ref.roughness)));
						if (e > pire)
							pire = e;
						const float32 ef = NkFabs(f - ref.f0);
						if (ef > pireF0)
							pireF0 = ef;
					}
				}
			}
			std::fprintf(stderr,
						 "     (w2) 605 points (wet x porosite x pellicule) : ecart max albedo/rugosite %.3g, F0 %.3g\n",
						 (double)pire, (double)pireF0);
			UCHECK(pire < 1e-6f && pireF0 < 1e-6f,
				   "(w2) la forme REDUITE du nuanceur EGALE NkApplyWetness (Lagarde 3b) partout");
		}

		// (w2b) CONTRE-ÉPREUVE. Une forme réduite FAUSSE doit sortir de la bande :
		// sans ça, (w2) pourrait passer parce que la grille ne porte pas l'écart.
		{
			NkWetMaterial m;
			m.porosity = 1.f;
			const NkVec3f albDry = {0.62f, 0.44f, 0.21f};
			const NkWetShading ref = NkApplyWetness(albDry, 0.53f, 0.04f, 1.f, m);
			// le mauvais coefficient : 0.4 au lieu de 0.8 sur l'albedo
			const float32 faux = albDry.x * (1.f - 0.4f * 1.f);
			std::fprintf(stderr, "     (w2b) coefficient errone 0.4 : %.4f contre %.4f attendu -> ecart %.4f\n",
						 (double)faux, (double)ref.albedo.x, (double)NkFabs(faux - ref.albedo.x));
			UCHECK(NkFabs(faux - ref.albedo.x) > 1e-3f,
				   "(w2b) CONTRE-EPREUVE : un coefficient errone SORT de la bande de (w2)");
		}

		// (w3) ET LE NUANCEUR ÉCRIT BIEN CETTE FORME-LÀ. Sans ce témoin, (w2)
		// prouverait une formule que personne n'écrit — la faute exacte que le
		// corpus appelle « une justification écrite se croit ».
		{
			NkString sansCom = SansCommentaires(fragBrut);
			const bool a = sansCom.Contains("albedo *= (1.0 - 0.8 * wetK);");
			const bool b = sansCom.Contains("rog     = clamp(rog * (1.0 - 0.4 * wetK), 0.0, 1.0);");
			const bool d = sansCom.Contains("F0 = mix(F0, vec3(f0Water), waterK);");
			const bool e = sansCom.Contains("float nWater  = 1.33;");
			if (!(a && b && d && e))
				std::fprintf(stderr, "     (w3) manquant : albedo=%d rugosite=%d F0=%d n=%d\n", a ? 1 : 0, b ? 1 : 0,
							 d ? 1 : 0, e ? 1 : 0);
			UCHECK(a && b && d && e,
				   "(w3) le fragment VIVANT ecrit EXACTEMENT la forme reduite prouvee par (w2)");
		}
	}

} // namespace

// Appelée depuis le main() de la suite (test_eau_sph.cpp). Rend le nombre
// d'échecs, pour que la suite ne puisse pas sortir 0 sur une sonde rouge.
int NkSondeUBOMouillage() {
	gP = 0;
	gF = 0;
	std::fprintf(stderr, "=== LE BLOC UNIFORME PARTAGE : le mouillage branche sur le PBR ===\n");

	// (u0) LA SONDE TROUVE-T-ELLE SON SUJET ?
	if (!TrouveRacine()) {
		std::fprintf(stderr, "  [ROUGE] (u0) racine du depot introuvable depuis le repertoire courant.\n");
		std::fprintf(stderr, "          Lancer la suite depuis la racine du worktree. La sonde REFUSE de\n");
		std::fprintf(stderr, "          rendre un verdict plutot que d'en rendre un vide.\n");
		return 1;
	}
	UCHECK(true, "(u0) racine du depot trouvee : la sonde a un sujet");

	// (u1) LA RÉFÉRENCE ELLE-MÊME EST-ELLE COHÉRENTE ? Un attendu faux fait
	// corriger du code juste : on le vérifie avant de juger quoi que ce soit.
	{
		const uint32 t = TailleStd140(kRef);
		std::fprintf(stderr, "     (u1) reference : %u membres, sequence '%s', %u octets std140\n",
					 (unsigned)15, kRef, t);
		UCHECK(t == kRefTaille, "(u1) la reference std140 fait bien 240 octets");
	}

	const NkString cpp = SansCommentaires(
		Lire("Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/Render3D/NkRender3D.cpp"));
	const NkString cppBrut =
		Lire("Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/Render3D/NkRender3D.cpp");
	if (cpp.Empty()) {
		std::fprintf(stderr, "  [ROUGE] (u2) NkRender3D.cpp illisible depuis la racine trouvee.\n");
		return 1;
	}

	// (u2) LE C++ : ObjectUBO + toutes les copies locales ObjBlock.
	VerifieSite(cpp, "struct ObjectUBO", 0, "(u2) C++  ObjectUBO         : 15 membres, 240 octets", true, nullptr);
	{
		uint32 copies = 0, bonnes = 0;
		for (uint32 i = 0; i < 32u; ++i) {
			NkString corps;
			if (!CorpsDuBloc(cpp, "struct ObjBlock", i, corps))
				break;
			++copies;
			if (SequenceTypes(corps) == kRef)
				++bonnes;
			else
				std::fprintf(stderr, "     ObjBlock #%u : sequence '%s'\n", i, SequenceTypes(corps).CStr());
		}
		std::fprintf(stderr, "     (u2) copies locales ObjBlock : %u trouvees, %u conformes\n", copies, bonnes);
		UCHECK(copies > 0u && copies == bonnes,
			   "(u2) TOUTES les copies locales ObjBlock portent la meme disposition");

		// La garde de taille : chaque copie doit en avoir une, et à 240. C'est
		// elle qui transforme une copie oubliee -- ici ou a une fusion -- en
		// ERREUR DE COMPILATION au lieu d'un octet lu de travers.
		uint32 gardes = 0, pos = 0;
		const char *needle = "static_assert(sizeof(ObjBlock) == 240";
		for (;;) {
			const NkString::SizeType f = cppBrut.Find(needle, pos);
			if (f == NkString::npos)
				break;
			++gardes;
			pos = (uint32)f + 8u;
		}
		std::fprintf(stderr, "     (u2) gardes de taille a 240 : %u pour %u copies\n", gardes, copies);
		UCHECK(gardes == copies,
			   "(u2b) chaque copie porte SA garde de taille : une copie oubliee ne compilera pas");
	}

	// (u3)(u4) LES DEUX NUANCEURS VIVANTS.
	const NkString vertBrut = Lire("Resources/NKRenderer/Shaders/PBR/NkSL/pbr.vert.nksl");
	const NkString fragBrut = Lire("Resources/NKRenderer/Shaders/PBR/NkSL/pbr.frag.nksl");
	if (vertBrut.Empty() || fragBrut.Empty()) {
		std::fprintf(stderr, "  [ROUGE] (u3) les nuanceurs VIVANTS sont introuvables : rien a juger.\n");
		return gF + 1;
	}
	NkString nomsV, nomsF;
	const NkString vertSansCom = SansCommentaires(vertBrut);
	const NkString fragSansCom = SansCommentaires(fragBrut);
	VerifieSite(vertSansCom, "ObjectUBO", 0, "(u3) NkSL pbr.vert.nksl (VIVANT) : meme disposition", true, &nomsV);
	VerifieSite(fragSansCom, "ObjectUBO", 0, "(u3) NkSL pbr.frag.nksl (VIVANT) : meme disposition", true, &nomsF);
	if (nomsV != nomsF)
		std::fprintf(stderr, "     (u4) vertex : %s\n     (u4) fragment: %s\n", nomsV.CStr(), nomsF.CStr());
	UCHECK(!nomsV.Empty() && nomsV == nomsF,
		   "(u4) les deux etages declarent les MEMES NOMS (regle GLSL du bloc partage)");

	// (u5)(u6) les generateurs et glslang.
	// ⚠️ ON PASSE PAR LE MEME CHEMIN QUE LE CHARGEUR : `NkShaderIncludeResolver`
	// d'abord, compilateur ensuite. Sans lui, le fragment rendait 0 octet et
	// `success=0` -- et la sonde accusait le nuanceur d'un defaut qui etait le
	// sien. Une sonde qui ne reproduit qu'une PARTIE du chemin du produit ne
	// mesure pas ce produit.
	{
		const NkString cheminV = gRacine + "Resources/NKRenderer/Shaders/PBR/NkSL/pbr.vert.nksl";
		const NkString cheminF = gRacine + "Resources/NKRenderer/Shaders/PBR/NkSL/pbr.frag.nksl";
		const NkString vertResolu = renderer::NkShaderIncludeResolver::Resolve(vertBrut, cheminV);
		const NkString fragResolu = renderer::NkShaderIncludeResolver::Resolve(fragBrut, cheminF);
		std::fprintf(stderr, "     includes resolus : vertex %u -> %u octets, fragment %u -> %u octets\n",
					 (unsigned)vertBrut.Size(), (unsigned)vertResolu.Size(), (unsigned)fragBrut.Size(),
					 (unsigned)fragResolu.Size());
		UCHECK(fragResolu.Size() > fragBrut.Size(),
			   "(u4b) les #include du fragment sont bien resolus (sinon la sonde jugerait un texte tronque)");
		TemoinGenerateurs(vertResolu, fragResolu);
	}

	// (u7) L'INVENTAIRE DES DECLARATIONS QUE PERSONNE NE CHARGE. Elles ne font
	// pas rougir : les faire rougir noierait les vraies. Elles sont NOMMEES avec
	// leur ecart, parce qu'une divergence connue et taue redevient un piege.
	{
		const Site morts[8] = {
			{"Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/Render3D/NkRender3D_PBRShaders.inl", "ObjectUBO",
			 "repli embarque kPBR_VS/kPBR_FS"},
			{"Resources/NKRenderer/Shaders/PBR/VK/pbr.vert.vk.glsl", "ObjectUBO", "repli VK vertex"},
			{"Resources/NKRenderer/Shaders/PBR/VK/pbr.frag.vk.glsl", "ObjectUBO", "repli VK fragment"},
			{"Resources/NKRenderer/Shaders/PBR/GL/pbr.frag.gl.glsl", "ObjectUBO", "copie GL (jamais chargee)"},
			{"Resources/NKRenderer/Shaders/PBR/DX11/pbr.frag.dx11.hlsl", "ObjectUBO",
			 "copie DX11 (jamais chargee)"},
			{"Resources/NKRenderer/Shaders/PBR/DX12/pbr.frag.dx12.hlsl", "ObjectUBO",
			 "copie DX12 (jamais chargee)"},
			{"Resources/NKRenderer/Shaders/PBR/MSL/pbr.frag.metal.msl", "ObjectUBO",
			 "copie MSL (jamais chargee)"},
			{"Kernel/Runtime/NKRenderer/src/NKRenderer/Shaders/PBR/NkSL/pbr.nksl", "ObjectUBO",
			 "copie source du 05/05 (jamais chargee)"},
		};
		uint32 divergentes = 0, lues = 0;
		std::fprintf(stderr, "  -- (u7) declarations qu'AUCUN chargeur n'ouvre (inventaire, pas un verdict) --\n");
		for (uint32 i = 0; i < 8u; ++i) {
			NkString t = SansCommentaires(Lire(morts[i].chemin));
			if (t.Empty()) {
				std::fprintf(stderr, "     %-34s : fichier absent\n", morts[i].quoi);
				continue;
			}
			NkString corps;
			if (!CorpsDuBloc(t, morts[i].bloc, 0, corps)) {
				std::fprintf(stderr, "     %-34s : aucun bloc ObjectUBO\n", morts[i].quoi);
				continue;
			}
			++lues;
			NkString seq = SequenceTypes(corps);
			const bool conforme = (seq == kRef);
			if (!conforme)
				++divergentes;
			std::fprintf(stderr, "     %-34s : '%s' -> %u octets  %s\n", morts[i].quoi, seq.CStr(),
						 TailleStd140(seq.CStr()), conforme ? "conforme" : "DIVERGENTE");
		}
		std::fprintf(stderr,
					 "     (u7) %u declarations mortes lues, %u divergent de la disposition reelle.\n"
					 "          Elles ne font PAS rougir : aucune n'est chargee. Elles sont ici pour\n"
					 "          qu'on ne les prenne pas un jour pour la verite du bloc.\n",
					 lues, divergentes);
		UCHECK(lues > 0u, "(u7) l'inventaire des declarations mortes a bien trouve son sujet");
	}

	// (w1)(w2)(w3) la formule.
	std::fprintf(stderr, "  -- la formule de mouillage : du papier au nuanceur --\n");
	TemoinFormule(fragBrut);

	std::fprintf(stderr, "=== bloc uniforme + mouillage : %d passes, %d echecs ===\n", gP, gF);
	return gF;
}
