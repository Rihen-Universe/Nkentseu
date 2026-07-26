// =============================================================================
// NKSpeech/NkG2P.h — G2P (grapheme-to-phoneme) RULE-BASED, multilingue fr/en/bbj.
// -----------------------------------------------------------------------------
// Brique 4 de la Phase 8 (TTS front-end, cf. Kernel/AI/ROADMAP.md) : texte -> suite
// de phonèmes (+ tons pour le bbj). From-scratch, zero-STL, namespace nkentseu::ai.
// Aucune donnée ni modèle appris : tables de correspondance + règles écrites à la
// main, dérivées de sources RÉELLES et VÉRIFIÉES (voir bas de fichier + README.md).
//
// ⚠️ PRIORITÉ bbj (ghomala') — règle du projet : AUCUNE source non vérifiée, jamais
// de génération par LLM pour cette langue peu dotée. Toutes les règles bbj ci-dessous
// sont tracées à une source réelle (voir "SOURCES" en bas de ce fichier). fr/en ont
// été ÉTENDUES (2026-07-25) avec de VRAIES règles graphème-phonème SOURCÉES — liaisons,
// e muet (final + "-es"), voyelles nasales étendues, digraphe -ill- pour le français ;
// "magic e" (voyelle longue + e final muet), th voisé/non voisé, c/g mous vs durs pour
// l'anglais — cf. SOURCES [6]-[11] en bas de ce fichier. Remplacent les règles
// "best-effort non sourcées" livrées le 2026-07-23. Restent non exhaustives : limites
// et irrégularités NON gérées documentées inline dans NkG2P.cpp (ex. schwa médian
// français, exceptions lexicales anglaises have/give/get...).
//
// -----------------------------------------------------------------------------
// PIPELINE bbj (le plus abouti) :
//   1. Décodage UTF-8 -> codepoints (encoding::utf8::NkDecodeChar). Tout le
//      matching se fait en comparant des ENTIERS (codepoints Unicode), jamais des
//      littéraux de chaîne contenant des caractères non-ASCII (portabilité MSVC/
//      charset source garantie : aucun risque de mauvais encodage à la compilation).
//   2. Découpage en mots (séparateurs : espace, ponctuation ASCII ; l'apostrophe
//      N'EST PAS un séparateur — c'est un phonème en ghomala', l'occlusive
//      glottale ʔ, cf. sources).
//   3. Pour chaque mot, appariement glouton PLUS LONG D'ABORD :
//        - digraphes consonantiques ASCII (gh, zh, sh, ts, dz, bv, pf, ph, bh, th,
//          dh, kh) avant les lettres seules ;
//        - voyelles spéciales (ɑ ɛ ɔ ə ʉ) ou ASCII (a e i o u), puis consommation
//          des diacritiques combinants qui suivent (ton haut/bas/montant/
//          descendant, nasalisation) ;
//        - voyelles latines précomposées accentuées (á à é è í ì ó ò ú ù ã õ â
//          ê î ô û) -> même résultat (base, ton[, nasalisé]) ;
//        - apostrophe (' ou ʼ U+02BC) -> occlusive glottale ;
//        - consonnes simples restantes (table).
//      Inconnu (résidu de contamination du fichier source, ex. mots portugais
//      « texto »/« que » présents dans le NT numérisé) -> ignoré silencieusement,
//      documenté comme limite connue (voir README.md NKSpeech).
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace ai {

		// Langue cible du G2P.
		enum class NkG2PLang : uint8 {
			Fr = 0, // français — règles orthographiques simplifiées
			En = 1, // anglais — règles orthographiques simplifiées (irrégulier, best-effort)
			Bbj = 2, // ghomala' — règles tracées aux sources (voir bas de fichier)
		};

		// Ton lexical (pertinent seulement pour bbj ; fr/en = None). 5 tons ghomala'
		// (source Wikipedia EN "Ghomala' language", d'après Nissim 1981) : haut/bas/
		// moyen(non marqué)/montant/descendant.
		enum class NkTone : uint8 {
			None = 0,	// pas de ton (consonne, ou langue non tonale)
			Mid,		// non marqué à l'écrit
			High,		// diacritique aigu ( ́)
			Low,		// diacritique grave ( ̀)
			Rising,		// diacritique caron ( ̌)
			Falling,	// diacritique circonflexe ( ̂)
		};

		// Symbole phonétique de sortie (voyelles + consonnes). Espace phonétique
		// PARTAGÉ entre les 3 langues (facilite un vocodeur/synthèse commun, comme
		// NkVoiceSynth::Phoneme). Étiquettes ASCII volontairement (voir SymbolLabel).
		enum class NkPhonSym : uint8 {
			Sil = 0, // silence / frontière de mot

			// --- Voyelles (10 attestées dans l'orthographe bbj réelle, cf. sources) ---
			V_a,	// a  /a/
			V_aa,	// ɑ  /ɑ/  (voyelle arrière basse, distincte de /a/ en bbj)
			V_e,	// e  /e/
			V_eh,	// ɛ  /ɛ/
			V_eu,	// ə  /ə/  (schwa — voyelle la plus fréquente du bbj)
			V_i,	// i  /i/
			V_o,	// o  /o/
			V_oh,	// ɔ  /ɔ/
			V_u,	// u  /u/
			V_uu,	// ʉ  /ʉ/  (centrale fermée arrondie)
			V_y,	// (fr/en seulement) /y/ — "u" français (tu, rue) ; voir NkVoiceSynth 'u'

			// --- Consonnes plosives ---
			C_p, C_b, C_t, C_d, C_k, C_g,

			// --- Fricatives ---
			C_f, C_s, C_h,
			C_zh, // ʒ (digraphe "zh")
			C_sh, // ʃ (digraphe "sh")
			C_gh, // ɣ (digraphe "gh" — donne son nom au "ghomala'")
			C_v,  // v (allophone de bv en bbj ; phonème plein en fr/en)
			C_z,  // z (allophone de dz en bbj ; phonème plein en fr/en)
			C_dentalTh,	 // θ (anglais "th" non voisé, ex. "think") — DISTINCT de C_th_affr
			C_dentalDh,	 // ð (anglais "th" voisé, ex. "this")

			// --- Affriquées (6 en bbj : p͡f t͡s t͡ʃ b͡v d͡z d͡ʒ, cf. sources) ---
			C_pf, C_ts, C_dz, C_bv,
			C_tsh, // t͡ʃ (graphème "c" en bbj ; "ch" en fr/en)
			C_dzh, // d͡ʒ (graphème "j" en bbj ; "j"/"dg" en en)

			// --- Réalisations affriquées de plosive+h (bbj, règle sourcée Wikipedia) ---
			C_ph_affr, // [pɸ]
			C_bh_affr, // [bβ]
			C_th_affr, // [tθ]
			C_dh_affr, // [dð]
			C_kh_affr, // [kx]

			// --- Nasales ---
			C_m, C_n, C_ng, // ŋ

			// --- Approximantes / autres ---
			C_w, C_y /*glide j*/, C_l, C_r /*emprunts/noms propres*/,
			C_glottal, // ʔ (bbj : apostrophe, seulement en fin de mot d'après les sources)

			Count
		};

		// Une unité phonétique : symbole + ton (voyelles bbj) + nasalisation.
		struct NkPhonemeUnit {
				NkPhonSym symbol = NkPhonSym::Sil;
				NkTone tone = NkTone::None;
				bool nasalized = false; // combining tilde U+0303 (ex. bbj "ã", "õ") ou fr an/on/in

				NkPhonemeUnit() = default;
				NkPhonemeUnit(NkPhonSym s, NkTone t = NkTone::None, bool nasal = false)
					: symbol(s), tone(t), nasalized(nasal) {
				}
		};

		// G2P rule-based : texte -> suite de phonèmes. Une instance = sans état
		// (méthodes statiques), comme NkAudioFeatures/NkGriffinLim.
		class NkG2P {
			public:
				// Point d'entrée générique (dispatch vers la table de la langue).
				static NkVector<NkPhonemeUnit> ToPhonemes(const NkString &text, NkG2PLang lang);

				// bbj (ghomala') — implémentation PRINCIPALE, tracée aux sources (voir bas
				// de NkG2P.h/.cpp). Gère les 10 voyelles, les 5 tons, la nasalisation, les
				// 6 affriquées, l'occlusive glottale (apostrophe) et les digraphes attestés
				// dans le Nouveau Testament ghomala' (corpus du projet).
				static NkVector<NkPhonemeUnit> ToPhonemesBbj(const NkString &text);

				// fr — règles graphème-phonème SOURCÉES (voir SOURCES [6][7] en bas de
				// fichier) : e muet final + "-es" (avec exception des monosyllabes
				// grammaticaux le/je/de/ce/me/te/se/ne/que qui gardent le schwa),
				// voyelles nasales an/en/on/in/un/am/em/om/im/um/ain/aim/ein (bloquées par
				// doublement nn/mm ou voyelle suivante), digraphes ch/ou/oi/eau/gn/qu,
				// digraphe -ill- (+ exceptions ville/mille/tranquille), consonnes finales
				// muettes (règle "CaReFuL") avec LIAISON vers le mot suivant s'il commence
				// par une voyelle/h. Non exhaustif (schwa médian non géré, cf. NkG2P.cpp).
				static NkVector<NkPhonemeUnit> ToPhonemesFr(const NkString &text);

				// en — règles graphème-phonème SOURCÉES (voir SOURCES [8]-[11]) : "magic e"
				// (voyelle allongée + e final muet : cake/bike/note/cute), "th" voisé/non
				// voisé selon une liste de mots grammaticaux (the/this/that/these/those/
				// they/them/their/there/then/than/thus/though), digraphes sh/ch/ph/wh
				// (+ exception wh+o -> /h/ : who/whole), c/g mous vs durs (avant e/i/y).
				// L'anglais reste notoirement irrégulier (exceptions lexicales non gérées,
				// ex. have/give pour le "magic e") : PAS un dictionnaire type CMUdict.
				static NkVector<NkPhonemeUnit> ToPhonemesEn(const NkString &text);

				// Étiquette ASCII de debug/test (ex. "eu" pour ə, "ng" pour ŋ). Jamais de
				// littéral non-ASCII ici : portabilité garantie quel que soit le charset
				// source du compilateur.
				static const char *SymbolLabel(NkPhonSym s);
				static const char *ToneLabel(NkTone t); // "-", "M", "H", "L", "R", "F"

				// Auto-test headless : mots RÉELS ghomala' (NT + lexique lamba-africa.com,
				// cf. sources) -> vérifie tons/voyelles/consonnes attendus ; fr/en : mots
				// RÉELS choisis pour exercer chaque règle sourcée (e muet, "-es", nasales,
				// -ill-/exceptions, liaison "un ami" cité de [7], "le" monosyllabe, magic e
				// cake/bike/note/cute, "the" th voisé, "ice" c mou, "who" wh+o exception) ->
				// assertions contre la prononciation de référence standard (cf. SOURCES).
				static bool SelfTest();
		};

	} // namespace ai
} // namespace nkentseu

// =============================================================================
// SOURCES (vérifiées, consultées 2026-07-23) — voir aussi NKSpeech/README.md :
//
// [1] Wikipedia (EN) « Ghomálá' language » — https://en.wikipedia.org/wiki/Ghomala%CA%BC_language
//     Inventaire consonantique/vocalique complet (IPA), système à 5 tons (aigu=haut,
//     grave=bas, non marqué=moyen, caron=montant, circonflexe=descendant), règle
//     d'affrication p/b/t/d/k + h -> [pɸ bβ tθ dð kx], nasalisation ɐ/u/ɔ près de ŋ.
//     Cite : Nissim, Gabriel M. (1981). « Le Bamileke-Ghomálá' (parler de Bandjoun,
//     Cameroun) : phonologie - morphologie nominale... ». SELAF, Paris. ISBN 978-2-85297-104-2.
// [2] Wikipédia (FR) « Ghomala' » — https://fr.wikipedia.org/wiki/Ghomala' — confirme
//     l'alphabet à 40+ caractères, digraphes bv/dz/pf, tons (aigu/grave/circonflexe/
//     caron/non marqué).
// [3] Wikipédia (FR) « Alphabet général des langues camerounaises » —
//     https://fr.wikipedia.org/wiki/Alphabet_g%C3%A9n%C3%A9ral_des_langues_camerounaises
//     AGLC/ALCAM (Tadadjeu & Sadembouo, 1978/1979) : voyelles ɛ ə ɔ ʉ, consonnes ŋ ɣ,
//     digraphes mb/nd/ny/pf/ts/dz, notation tonale par accents.
// [4] Corpus RÉEL du projet (déjà présent avant cette tâche) : Nouveau Testament
//     ghomala', © 2002 Bible Society of Cameroon — fichier
//     `AI/corpus/lamba/bbj_ghomala_nt.txt` (aussi `Resources/Datasets/bbj_ghomala_nt.txt`).
//     Confirmé existant/attribué via https://bibliamundi.com/wp-content/uploads/2023/09/Ghomala-Bible-New-Testament.pdf
//     et https://www.bible.com/languages/bbj (Bible Society of Cameroon, 2002).
//     Analyse EMPIRIQUE de ce texte (script Python, 2026-07-23) : fréquences des
//     codepoints Unicode -> confirme les 10 graphèmes voyelles (a ɑ e ɛ ə i o ɔ u ʉ),
//     les diacritiques combinants réellement utilisés (U+0301 aigu, U+0300 grave,
//     U+030C caron — très fréquent, 9350 occurrences —, U+0302 circonflexe rare,
//     U+0303 tilde de nasalisation sur a/o), et les digraphes consonantiques attestés
//     (gh, zh, sh, kh, th, ts, dz, bv, pf, cw/hw/gw/kw = C+w compositionnel).
// [5] Lexique lamba-africa.com (déjà utilisé par le projet, cf. ROADMAP Phase 8) —
//     `AI/corpus/lamba/ghomala/records.json` : mots isolés AVEC traduction française
//     vérifiée, utilisés comme cas de test du G2P : "dɔ̀mnyə̀" = « impasse »,
//     "lɛtə̌" = « solide ».
// [6] Omniglot, « Ghomala' language and alphabet » — https://www.omniglot.com/writing/ghomala.htm
//     section « Sample text » : « Pə pə́ gɔ́m Ghɔmáláʼ bí pô pɔ́kpə̀... » = « Let us
//     speak the Ghɔmáláʼ language to our children... », lui-même sourcé par Omniglot
//     à https://www.ghomalaonline.com/bbj/mcùŋ/jɛ-ghɔmáláʼ (dictionnaire Ghomala'-
//     Français en ligne). Récupéré 2026-07-26 en HTML BRUT (curl, PAS une paraphrase)
//     pour lire les entités numériques exactes (&#x0259;=U+0259 SCHWA, &#x0301;=U+0301
//     COMBINING ACUTE, etc.) octet à octet — évite tout risque de mauvaise
//     transcription des diacritiques. Utilisé pour 2 nouvelles assertions G2P (paire
//     minimale de ton) : "Pə" (ton non marqué) / "pə́" (ton aigu = High), cf.
//     `NkG2P::SelfTest()`. NOTE : une autre ressource identifiée mais NON exploitable
//     par extraction directe de texte (contenu audio/interactif, pas de liste de mots
//     scrapable) : resulam.com « Mes premiers 500 mots... Bamileke-ghomala » (2020,
//     Resulam/Ndjeup) et lughayangu.com/ghomala (dictionnaire communautaire sans
//     entrées affichées côté page statique) — documenté honnêtement, pas exploité.
//     Wiktionary « Category:Ghomala' lemmas » (en.wiktionary.org) référence 542
//     entrées lexicales (licence CC BY-SA) mais sans gloses visibles depuis la page de
//     catégorie (nécessiterait de charger chaque page de mot individuellement) —
//     piste réelle pour un enrichissement futur du corpus, non exploitée dans le temps
//     imparti de cette tâche (cf. Kernel/AI/ROADMAP.md Phase 8, brique 7).
//
// Ce qui N'EST PAS directement sourcé (documenté honnêtement, pas inventé au hasard) :
//   - le graphème "c" -> /tʃ/ et "j" -> /dʒ/ : DÉDUIT de l'usage récurrent dans le
//     corpus NT (ex. "Cyəpɔ" = Seigneur, "cwə" ; "jyə", "jap") combiné à la convention
//     ALCAM standard (c=/tʃ/, j=/dʒ/, y=/j/) — pas une citation directe d'un article ;
//   - "ç" -> /s/ : graphème rare (12 occurrences) dans le fichier NT numérisé, traité
//     par défaut comme un /s/ à la française (le texte contient une contamination
//     mineure de portugais dans l'en-tête — cf. README) ; pas une règle bbj établie ;
//   - /r/ : absent de l'inventaire phonémique [1], mais présent dans le texte réel
//     (noms propres/emprunts : "Roma", "Kristo") -> transcrit /r/ par pragmatisme,
//     PAS un phonème natif bbj confirmé ;
//
// -----------------------------------------------------------------------------
// SOURCES fr/en (règles réelles, remplaçant les règles "best-effort" du 2026-07-23 ;
// vérifiées/consultées 2026-07-25) :
//
// [6] Wikipedia (EN) « French phonology » — https://en.wikipedia.org/wiki/French_phonology
//     Schwa (e caduc) : élision en fin de mot devant un mot suivant commençant par une
//     voyelle, élision "le/je/de/que" (généralisée ici au 'e' muet ABSOLU en fin de mot,
//     hors ces monosyllabes qui gardent le schwa) ; section "Medial syllables" décrit
//     aussi la suppression du schwa APRÈS UNE CONSONNE UNIQUE EN SYLLABE NON ACCENTUÉE
//     (ex. "appeler" -> [aple]) — RÈGLE NON IMPLÉMENTÉE ICI (nécessiterait un découpage
//     syllabique, hors de portée d'un scanner caractère-par-caractère ; limite honnête).
//     Voyelles nasales : inventaire /ɑ̃ ɛ̃ ɔ̃ œ̃/ et graphies (an/am, in/im/ain/ein/yn,
//     on/om, un/um) ; nasalisation bloquée par une consonne nasale DOUBLÉE (nn/mm, ex.
//     "inné") ou par une voyelle suivante.
// [7] Wikipedia (EN) « Liaison (French) » — https://en.wikipedia.org/wiki/Liaison_(French)
//     Table de correspondance consonne-finale -> son de liaison : -s/-z/-x -> [z] (ex.
//     "les enfants"), -t/-d -> [t] (ex. "tout homme"), -n -> [n] (ex. "un ami" ->
//     /œ̃.n‿a.mi/, EXEMPLE REPRIS TEL QUEL comme cas de test), -p/-g -> [p]/[k] (cas
//     rares). Condition : le mot suivant doit commencer par une voyelle ou un "h" muet.
//     Section « H aspiré/muet » : opposition LEXICALE (haricot/héros = aspiré, bloque la
//     liaison ; homme = muet, autorise la liaison) — PAS dérivable de la graphie seule,
//     donc NON distinguée ici (tout 'h' traité comme autorisant la liaison ; limite
//     honnête, simplification assumée). Distinction liaison obligatoire/facultative/
//     interdite (dépend de la syntaxe) également NON implémentée (limite honnête).
//   [7bis] Règle pédagogique standard du digraphe -ill- (connaissance générale des
//     manuels de FLE/orthographe française) : /j/ (glide) après consonne précédé d'un
//     /i/ entendu (ex. "fille" /fij/), glide seul après voyelle (ex. "grenouille"
//     /ɡʁənuj/) ; exceptions lexicales bien connues ville/mille/tranquille = /il/ (pas
//     de glide). Règle mnémonique "CaReFuL" des consonnes finales muettes (c/f/l/r
//     restent prononcées, les autres sont muettes sauf liaison) — connaissance générale
//     standard de l'enseignement du français, cohérente avec [7].
// [8] Wikipedia (EN) « Silent e » — https://en.wikipedia.org/wiki/Silent_e
//     Règle du "magic e" / VCe (voyelle-consonne-e final) : le e final devient muet et
//     ALLONGE la voyelle précédente vers le son de son NOM — a_e->/eɪ/ (slate), e_e->/iː/
//     (mete), i_e->/aɪ/ (gripe), o_e->/oʊ/ (code), u_e->/juː/ (cute). Exceptions
//     lexicales connues (have, give, love, come, some...) NON gérées (limite honnête,
//     documentée dans NkG2P.cpp).
// [9] Wikipedia (EN) « Pronunciation of English ⟨th⟩ » —
//     https://en.wikipedia.org/wiki/Pronunciation_of_English_%E2%9F%A8th%E2%9F%A9
//     Valeur par défaut /θ/ (non voisé) pour la grande majorité des mots (dont TOUS les
//     mots nouveaux) ; liste fermée de mots grammaticaux fréquents en /ð/ (voisé) : the,
//     this, that, these, those, they, them, their, there, then, than, thus, though (+
//     dérivés) — sous-ensemble repris tel quel dans le code.
// [10] Wikipedia (EN) « English orthography » — https://en.wikipedia.org/wiki/English_orthography
//     Digraphes ch (/tʃ/ par défaut), sh (/ʃ/), ph (/f/), wh (/w/ en général, /h/ devant
//     "o" — who, whole, whom, whose) ; convention générale du e muet marquant une
//     voyelle "altérée" (cf. aussi [8]).
// [11] Wikipedia (EN) « Hard and soft C » — https://en.wikipedia.org/wiki/Hard_and_soft_C
//     (+ pendant « Hard and soft G », même encyclopédie) : "soft" c/g = /s/ /dʒ/ devant
//     e/i/y (cell, city, cycle ; gem, giant) ; "hard" c/g = /k/ /ɡ/ ailleurs (car, cost,
//     cube ; game, go, gum). Exceptions lexicales (soccer, Celtic ; get, give, girl)
//     NON gérées (limite honnête).
//
//   fr/en, ce qui N'EST TOUJOURS PAS géré (honnêteté, pas d'invention) :
//   - fr : schwa médian après consonne unique en syllabe non accentuée (source [6],
//     "medial syllables") ; "s" intervocalique -> /z/ (ex. "rose", "maison" — la lettre
//     "s" est toujours rendue /s/ ici, jamais /z/, sauf en position de liaison) ;
//     -ail/-eil/-euil finaux (glide /j/ après diphtongue) non traités séparément du cas
//     -ill- ; distinction syntaxique liaison obligatoire/facultative/interdite ; h
//     muet/aspiré (lexical, cf. [7]).
//   - en : exceptions lexicales du "magic e" (have/give/love/come/some/done/gone/one) ;
//     exceptions du c/g mou (soccer, Celtic, get, give, girl, gift) ; "ch" à valeur /k/
//     (chemistry) ou /ʃ/ (machine) selon l'origine du mot (grec/français), non déductible
//     de la graphie seule -> laissé à la valeur par défaut /tʃ/ (limite documentée dans
//     l'article [10] lui-même) ; diphtongues rendues par des monophtongues approchées
//     faute de symboles dédiés dans l'inventaire partagé avec le bbj.
// =============================================================================
