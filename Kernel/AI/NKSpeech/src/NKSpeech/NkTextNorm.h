// =============================================================================
// NKSpeech/NkTextNorm.h — Normalisation de texte du front-end TTS (Phase 8,
// cf. Kernel/AI/ROADMAP.md : "TTS front-end : normalisation texte (nombres,
// ponctuation), durées"). From-scratch, zero-STL, namespace nkentseu::ai.
// -----------------------------------------------------------------------------
// RÔLE DANS LE PIPELINE : texte brut -> texte "parlable" + pauses, EN AMONT du
// G2P (NkG2P::ToPhonemes). Nécessaire car NkG2P ignore délibérément les
// CHIFFRES (cf. NkG2P.cpp, fonction interne IsSeparator : les chiffres '0'-'9'
// sont traités comme des séparateurs de mots, à l'origine pour absorber les
// numéros de versets collés aux mots dans le corpus bbj — cf. NkG2P.h SOURCES
// [4]). Résultat : un G2P appelé directement sur "123" ignore silencieusement
// les chiffres au lieu de les prononcer. NkTextNorm comble ce trou en amont :
// il faut convertir les nombres en toutes lettres AVANT d'appeler
// NkG2P::ToPhonemes (il n'existe PAS de méthode "Convert" dans NkG2P — l'API
// réelle est NkG2P::ToPhonemes(const NkString&, NkG2PLang), vérifiée dans
// NkG2P.h).
//
// Portée : fr et en uniquement (bbj EXCLU ICI : le système de numération
// ghomala' n'est pas sourcé de façon vérifiée à ce stade du projet — aucune
// règle inventée, cf. règle projet "priorité bbj sourcé" déjà énoncée dans
// NkG2P.h ; limite honnête, pas un oubli).
//
// -----------------------------------------------------------------------------
// SOURCES (vérifiées, consultées 2026-07-25) :
//
// [1] Wikipedia (EN) « Names of large numbers » (via les articles « Billion »
//     et « Trillion », mêmes définitions) — https://en.wikipedia.org/wiki/Billion
//     https://en.wikipedia.org/wiki/Trillion — confirme l'échelle courte
//     ("short scale") utilisée aujourd'hui en anglais AMÉRICAIN ET BRITANNIQUE :
//     thousand=10^3, million=10^6, billion=10^9, trillion=10^12 (groupes de 3
//     chiffres, un nouveau nom par puissance de 1000). Utilisée ici pour la
//     table d'échelle anglaise.
// [2] Le français utilise l'échelle LONGUE (source [1], section historique) :
//     mille=10^3, million=10^6, milliard=10^9, billion=10^12 — "milliard" est
//     le nom intercalaire propre à l'échelle longue (10^9), distinct du
//     "billion" anglais (10^9, échelle courte). Confirmé également par l'usage
//     scolaire français standard (numération par tranches de 3 chiffres,
//     "milliard" enseigné comme palier entre million et billion).
// [3] Yolaine Bodin, « Rules about the spelling of French numbers » —
//     https://yolainebodin.com/the-language-nook/french/spelling-french-numbers
//     et « Vingt or vingts: the plural mark on French numbers » (Of Languages
//     and Numbers) — https://www.languagesandnumbers.com/articles/en/vingt-or-vingts/
//     Règles reprises ici :
//       - "cent" prend un 's' UNIQUEMENT quand il est multiplié (précédé d'un
//         nombre > 1) ET qu'il termine le nombre (rien après) : "deux cents"
//         mais "deux cent un", "cent" (jamais "un cent"/"cents" seul).
//       - "quatre-vingts" prend un 's' UNIQUEMENT en fin de nombre (rien
//         après) : "quatre-vingts" mais "quatre-vingt-un", "quatre-vingt-dix".
//       - "et" relie les dizaines 20/30/40/50/60 à "un" (vingt et un, trente
//         et un...) et 70 à "onze" (soixante et onze) ; PAS "quatre-vingt-un"
//         ni "quatre-vingt-onze" (règle explicite, sans "et").
//       - Depuis 1990, l'Académie française recommande des traits d'union entre
//         TOUS les éléments d'un nombre écrit en lettres, sauf autour de "et".
//         SIMPLIFICATION ASSUMÉE ICI : trait d'union seulement pour les
//         dizaines composées (21-99, usage traditionnel le plus répandu),
//         espace entre centaine/mille/million et le reste — SANS CONSÉQUENCE
//         ACOUSTIQUE : NkG2P (cf. NkG2P.cpp IsSeparator) traite l'espace ET le
//         trait d'union comme des séparateurs de mots équivalents (silence),
//         donc ce choix est purement orthographique, jamais phonétique.
//       - RÉSOLU (2026-07-25, cf. [3bis]) : "cent"/"quatre-vingts" devant
//         "million"/"milliard"/"billion" — voir SOURCES [3bis] ci-dessous.
// [3bis] Fix 2026-07-25 de la limite ci-dessus. Office québécois de la langue
//     française, Vitrine linguistique, « Pluriel de vingt, de cent et de
//     mille » — https://vitrinelinguistique.oqlf.gouv.qc.ca/21532/la-grammaire/
//     les-determinants/determinants-numeraux/pluriel-de-vingt-de-cent-et-de-mille
//     + chiffreenlettre.fr « Cent, vingt, mille : les règles d'accord des
//     nombres en français » — https://chiffreenlettre.fr/2026/04/05/regles-accord-nombres/
//     Règle confirmée par les DEUX sources : devant "millier"/"million"/
//     "milliard" (des NOMS, contrairement à "mille" qui est un adjectif
//     numéral invariable), "cent" et "quatre-vingts" S'ACCORDENT normalement
//     quand ils sont multipliés — "quatre-vingts millions", "deux cents
//     millions" (avec 's'), MAIS "quatre-vingt mille", "trois cent mille"
//     (sans 's', "mille" ne déclenche PAS l'accord). Comportement CORRIGÉ dans
//     NumberToWords (2026-07-25) : le groupe des millions/milliards/billions
//     passe désormais `isUnitsGroup=true` à Format999Fr (au lieu de `false`
//     par défaut conservateur avant ce correctif), exactement comme le groupe
//     des unités.
// [4] Convention de lecture des décimaux en synthèse vocale (littérature TN
//     générale) : Zhaorui Zhang, « Text Normalization for Text-to-Speech »,
//     Uppsala University — https://uu.diva-portal.org/smash/get/diva2:1764605/FULLTEXT01.pdf
//     + « Positional Description for Numerical Normalization » (arXiv
//     2408.12430) — https://arxiv.org/pdf/2408.12430 — confirment la classe
//     sémiotique DECIMAL des systèmes de normalisation de texte pour la parole
//     et la lecture chiffre par chiffre de la partie décimale après le
//     séparateur ("3.14" -> "three point one four" / "3,14" -> "trois virgule
//     un quatre") — convention retenue ici car elle évite l'ambiguïté des
//     zéros non significatifs (ex. "3,05" -> "trois virgule zéro cinq", pas
//     "trois virgule cinq").
// [5] Abréviations de civilité : « 40 Common French Acronyms and Abbreviations »
//     (talkinfrench.com) confirme M.=Monsieur, Mme=Madame, Mlle=Mademoiselle ;
//     usage anglais standard (dictionnaire courant) Mr=Mister, Mrs=Missus,
//     Dr=Doctor. GitHub DrewThomasson/ebook2audiobook issue #764 (« French text
//     issues, incorrect abbreviation expansion ») documente un piège connu des
//     systèmes de normalisation naïfs : confondre un préfixe ("st", "dr") au
//     début d'un mot ORDINAIRE (ex. "stéréo", "drôle") avec l'abréviation.
//     MITIGATION APPLIQUÉE ICI : la table n'est consultée que sur un TOKEN
//     ENTIER déjà segmenté (mot complet entre séparateurs), jamais sur un
//     préfixe de mot -> "stéréo" et "St" sont deux tokens disjoints, le bug
//     rapporté dans l'issue ne peut pas se produire ici.
//     Liste volontairement COURTE : reste de la casuistique des abréviations
//     (ex. "St" = Saint vs Street en anglais, dépend du contexte, NON résolu)
//     documentée honnêtement comme NON couverte plutôt qu'inventée. Depuis
//     2026-07-25, chaque entrée porte aussi une classe `isTitle` utilisée par
//     l'heuristique de désambiguïsation abréviation/fin de phrase — voir
//     SOURCES [abbrev-sbd] plus loin dans ce fichier.
// [6] Durées de pause associées à la ponctuation (PauseDurationMs) : AUCUNE
//     source académique unique ne fixe des valeurs en millisecondes précises ;
//     valeurs choisies par convention d'ingénierie usuelle en synthèse vocale
//     (pause courte virgule/point-virgule/deux-points < pause longue en fin de
//     phrase point/./!/?), cohérentes avec la durée par défaut déjà utilisée
//     dans le projet pour un NkPhone de silence (cf. NkVoiceSynth.h,
//     NkPhone::durationMs, défaut 120 ms ; cf. aussi l'usage de sil.durationMs
//     = 70 ms dans Applications/NKSpeechTest/src/main.cpp). NON sourcé de façon
//     rigoureuse : documenté honnêtement comme un choix d'ingénierie, pas une
//     règle établie citée.
// [7] Ordinaux français (SOURCES ajoutées 2026-07-25, consultées ce jour) :
//     Vaia « Ordinal Numbers French: Usage & Examples » —
//     https://www.vaia.com/en-us/explanations/french/french-vocabulary/ordinal-numbers-french/
//     et Lawless French « French Ordinal Numbers » —
//     https://www.lawlessfrench.com/vocabulary/ordinal-numbers/ — confirment :
//     suffixe "-ième" ajouté au cardinal (le "e" muet final est élidé avant :
//     "quatre" -> "quatrième", "onze" -> "onzième") ; "cinq" -> "cinquième"
//     (u intercalaire) ; "neuf" -> "neuvième" (f -> v) ; "un" -> "unième" DANS
//     UN COMPOSÉ ("vingt et unième", jamais "vingt et premier") ; SEUL le
//     nombre 1 isolé est irrégulier -> "premier"/"première" (RÉSOLU
//     2026-07-25, distinction de genre implémentée, cf. SOURCES [fem-ord]
//     ci-dessous). Abréviations usuelles :
//     "1er"/"1re" (irréguliers), "2e", "3e"... (jamais "2ième", faute
//     fréquente signalée par ces mêmes sources). Implémenté ici en réutilisant
//     NumberToWords : seul le DERNIER "mot" du cardinal (après le dernier
//     espace OU trait d'union) reçoit la transformation, le reste du nombre
//     reste un cardinal inchangé (compatible avec la marque plurielle de
//     "cents"/"quatre-vingts"/"millions" du groupe précédent, qui n'est PAS
//     répercutée sur l'ordinal, cf. OrdinalToWords).
// [8] Ordinaux anglais (SOURCES consultées 2026-07-25) : suffixe numérique
//     1st/2nd/3rd/4th... avec exception 11th/12th/13th (vedantu.com « Write
//     Ordinal Numbers Correctly », ukcalculator.com) et formation des MOTS
//     complets (readle-app.com « English Numbers: Ordinal Numbers ») :
//     suffixe "-th" par défaut ("four" -> "fourth", "hundred" -> "hundredth"),
//     "-y" -> "-ieth" pour les dizaines ("twenty" -> "twentieth", "ninety" ->
//     "ninetieth"), irréguliers "one/two/three/five/eight/nine/twelve" ->
//     "first/second/third/fifth/eighth/ninth/twelfth". Implémenté de la même
//     façon que le français (transformation du dernier "mot" du cardinal
//     uniquement) : couvre aussi les composés terminés par ces irréguliers
//     ("twenty-one" -> "twenty-first", "one hundred twelve" -> "one hundred
//     twelfth").
// [9] Dates/heures (SOURCES consultées 2026-07-25) : Comme une Française
//     « How to Read, Write and Say Dates in French » et numbersinfrench.com
//     « Dates in French: Format, Year in Words, Premier Rule » confirment la
//     règle française : le jour se lit en CARDINAL sauf le 1er du mois, qui se
//     lit "premier" (ordinal) — JAMAIS "deuxième"/"troisième" pour les autres
//     jours (contrairement à l'anglais, qui utilise l'ordinal pour tous les
//     jours). Lawless French « Telling Time in French » + kwiziq.com
//     confirment : "heure" est FÉMININ ("une heure", pas "un heure"),
//     s'accorde au pluriel dès 2 ("deux heures"), pas de mot "minutes" en
//     lecture standard ("quinze heures trente"). englishlearningtips.com
//     « Telling time in English » confirme la convention américaine "oh" pour
//     les minutes à un chiffre en lecture digitale ("3:05" -> "three oh
//     five"). Implémenté en RÉUTILISANT NumberToWords/OrdinalToWords (aucune
//     nouvelle table de nombres) :
//       - Date "D[D]/M[M]/AAAA" ou "D[D]/M[M]/AA" (RÉSOLU 2026-07-25, SOURCES
//         [2digit-year]) : ordre JJ/MM (fr, jour cardinal + "premier" pour le
//         1er + nom du mois + année cardinale) ou MM/DD (en, nom du mois +
//         jour ORDINAL + année cardinale, convention américaine courante).
//         Année en cardinal plein par défaut ("two thousand twenty-six"), ou
//         par PAIRES en option (RÉSOLU 2026-07-25, SOURCES [year-pairs], cf.
//         YearToWords) — repli automatique sur le cardinal plein pour les cas
//         irréguliers non généralisables (années "rondes", hors bornes).
//         Validation RÉELLE du calendrier (RÉSOLU 2026-07-25, SOURCES
//         [leap-year]) : le nombre de jours réel du mois est désormais
//         vérifié (ex. "31/04/2026" est REJETÉ, avril n'a que 30 jours).
//       - Heure "HH(h|:)MM" : fr "minuit"/"midi" pour 0h/12h (RÉSOLU
//         2026-07-25, cf. SOURCES [minuit-midi] ci-dessous), "une heure" (1h,
//         féminin), "<N> heures" sinon, minute ajoutée en cardinal si non
//         nulle (pas de mot "minutes"). en : 24h par défaut ("<N> o clock" si
//         minute nulle, écrit SANS apostrophe, choix ASCII simple assumé ;
//         "<N> oh <minute>" si minute < 10 ; "<N> <minute>" sinon), ou 12h
//         AM/PM en option (RÉSOLU 2026-07-25, SOURCES [ampm]).
//
// [dialect] Fix 2026-07-25 (limite "variantes belges/suisses" ci-dessus,
//     RÉSOLUE). numbersinfrench.com « Numbers in Belgium and Swiss —
//     Septante, Huitante, Nonante » et elon.io « Septante, huitante, nonante :
//     les nombres en Belgique et en Suisse » confirment : "huitante" (80) est
//     la forme utilisée dans les cantons suisses de Vaud/Valais/Fribourg ;
//     Genève/Jura/Neuchâtel gardent "quatre-vingts" comme en France ; "octante"
//     est obsolète partout aujourd'hui (survivance historique uniquement,
//     déplacée par "huitante" ou "quatre-vingts" selon la région) — donc
//     "octante" n'est volontairement PAS implémenté (forme éteinte, seul
//     "huitante" est produit). Implémenté via `NkFrNumberDialect` (Standard
//     défaut inchangé / BelgeSuisse) : 70-99 deviennent des mots DE BASE
//     réguliers ("septante-deux", "huitante et un", "nonante-neuf" — même
//     formation que 20-69, jamais de composé à partir de "dix"/"vingt"),
//     jamais pluralisés (contrairement à "quatre-vingts", qui est
//     littéralement "4 × vingt" donc pluralisable ; "huitante" n'est PAS une
//     multiplication de "vingt" — déduction logique à partir de la règle
//     d'accord de SOURCES [3]).
// [abbrev-sbd] Fix 2026-07-25 (désambiguïsation abréviation vs fin de phrase,
//     limite ci-dessus RÉSOLUE par une heuristique documentée, PAS une
//     solution parfaite). Wikipedia (EN) « Sentence boundary disambiguation »
//     — https://en.wikipedia.org/wiki/Sentence_boundary_disambiguation — cite
//     la statistique du corpus Brown annoté : environ 90% des points sont des
//     fins de phrase réelles, 10% des points d'abréviation, ~0.5% les deux à
//     la fois. Palmer & Hearst, « Adaptive Multilingual Sentence Boundary
//     Disambiguation » — https://people.ischool.berkeley.edu/~hearst/papers/cl-palmer.pdf
//     (voir aussi https://aclanthology.org/A94-1013.pdf) confirment l'usage de
//     règles heuristiques simples (casse du mot suivant, catégorie lexicale de
//     l'abréviation) plutôt qu'une résolution parfaite. HEURISTIQUE RETENUE
//     ICI (compromis assumé, documenté honnêtement) : la table d'abréviations
//     distingue désormais deux CLASSES — les titres de civilité (M./Mme/
//     Mlle/Dr/Mr/Mrs), qui sont PAR DÉFINITION toujours suivis d'un nom propre
//     (c'est leur seule fonction linguistique) et ne déclenchent donc JAMAIS
//     la pause longue de fin de phrase (pause courte systématique) ; et
//     "etc.", qui PEUT terminer une phrase ou non — tranché par la casse du
//     mot qui suit le point (majuscule => pause longue de fin de phrase,
//     cohérent avec la statistique ~90% ; minuscule => pause courte,
//     abréviation qui continue la phrase). LIMITE ASSUMÉE : un titre qui
//     termine réellement le texte (rare) reçoit quand même une pause courte
//     (sous-estimation documentée, acceptée car marginale en pratique).
// [fem-ord] Fix 2026-07-25 (ordinal féminin "1re"/"première", limite ci-dessus
//     RÉSOLUE). Mêmes sources que [7] (Vaia, Lawless French) : seul "premier"/
//     "première" varie en genre parmi les ordinaux français (2e, 3e...
//     épicènes). Le marqueur féminin est détecté dans le LITTÉRAL d'entrée
//     ("1re", ou "1ere" — rendu ASCII de "1ère", cohérent avec la convention
//     sans accent du reste du fichier) et produit "premiere" au lieu de
//     "premier" via un paramètre `feminine` explicite sur OrdinalToWords.
// [minuit-midi]/[ampm] Fix 2026-07-25 (12h/AM-PM + minuit/midi, limite
//     ci-dessus RÉSOLUE). fr.wikipedia.org « Système horaire sur 12 heures »
//     et eurekoi.org « Pourquoi dit-on "midi" et pas "12 heures" ? »
//     confirment l'usage français : "minuit" (0h) et "midi" (12h) remplacent
//     les lectures littérales "zéro heure"/"douze heures". Wikipedia (EN)
//     « 12-hour clock » — https://en.wikipedia.org/wiki/12-hour_clock —
//     confirme la convention standard anglaise : 12 AM = minuit, 12 PM = midi,
//     puis heure modulo 12 pour le reste (1-11 le matin/l'après-midi).
//     Implémenté via `NkTextNormTimeFormat` (H24 défaut inchangé / H12,
//     ANGLAIS uniquement — `format` est ignoré en français, qui n'a pas de
//     convention AM/PM standard, cf. sources françaises ci-dessus).
// [2digit-year] Fix 2026-07-25 (année sur 2 chiffres, limite ci-dessus
//     RÉSOLUE). Wikipedia (EN) « Date windowing » —
//     https://en.wikipedia.org/wiki/Date_windowing — confirme la convention de
//     windowing standard POSIX (utilisée historiquement pour la conformité
//     Y2K) : les valeurs 00-68 désignent 2000-2068, les valeurs 69-99
//     désignent 1969-1999 (pivot = 69). Appliquée automatiquement dans
//     ExpandDateLiteral quand le groupe année ne fait que 2 chiffres (un
//     format à 4 chiffres reste également accepté et prioritaire, inchangé).
// [year-pairs] Fix 2026-07-25 (lecture de l'année par paires en anglais,
//     limite ci-dessus RÉSOLUE comme OPTION, pas comme nouveau défaut).
//     VOA Learning English « Pronouncing Years in American English » et
//     usinggrammar.com « Pronunciation and spelling of the years in English »
//     confirment : la lecture par paires de deux chiffres ("nineteen
//     eighty-four" pour 1984) est la convention DOMINANTE pour 1100-1999, et
//     reste courante/majoritaire pour 2000+ après 2011 ("twenty twenty-six")
//     — mais 2001-2009 se lit plus couramment avec "thousand" ("two thousand
//     nine"), et les années "rondes" (1900, 2000...) suivent des conventions
//     irrégulières non généralisables ("nineteen hundred" mais "two thousand",
//     jamais "twenty hundred"). CHOIX ASSUMÉ ICI, documenté honnêtement :
//     implémenté comme OPTION explicite (`NkTextNormYearReading::Paired`, pas
//     le défaut — le défaut `Full` = cardinal plein reste inchangé) via
//     `YearToWords` ; repli automatique sur le cardinal plein pour les années
//     hors [1000,9999] ou dont les deux derniers chiffres sont "00" (cas
//     irrégulier non généralisable, pas d'invention de règle).
// [leap-year] Fix 2026-07-25 (validité calendaire réelle, limite ci-dessus
//     RÉSOLUE). US Naval Observatory, « Leap Years » —
//     https://aa.usno.navy.mil/faq/leap_years — confirme la règle grégorienne
//     complète : année bissextile si divisible par 4, SAUF les années
//     séculaires (divisibles par 100), SAUF celles divisibles par 400
//     (exception de l'exception — ex. 1900/2100 ne sont PAS bissextiles, 2000
//     l'est). ExpandDateLiteral vérifie désormais le nombre de jours RÉEL du
//     mois (fonction DaysInMonth interne, 28/29/30/31 selon le mois et
//     l'année) : une date impossible (ex. "30/02/2026", "31/04/2026") est
//     REJETÉE (chaîne vide renvoyée, MÊME CONVENTION que le reste du fichier
//     pour une entrée non reconnue — pas de crash, pas de log, ce module est
//     sans état/logger comme NkG2P/NkAudioFeatures/NkGriffinLim).
//
// -----------------------------------------------------------------------------
// BILAN Phase 8 (2026-07-25) : les 7 limites ci-dessus explicitement visées par
// cette itération (variantes belges/suisses, désambiguïsation abréviation vs
// fin de phrase, ordinal féminin "1re"/"première", 12h/AM-PM + minuit/midi,
// année sur 2 chiffres, lecture de l'année par paires en anglais, validité
// calendaire réelle) sont désormais RÉSOLUES et testées (cf. SelfTest). Ce qui
// reste honnêtement NON couvert (hors du périmètre de cette mission, pas des
// oublis) :
//   - bbj (ghomala') : numération non sourcée de façon vérifiée, cf. portée en
//     tête de ce fichier — volontairement exclu, aucune règle inventée.
//   - fr : accord féminin CARDINAL "un" -> "une" (ex. "vingt et une pommes")
//     non implémenté (seul l'ORDINAL "premier"/"première" a été demandé et
//     résolu cette itération, cf. [fem-ord] ; le cardinal "un" reste toujours
//     masculin dans NumberToWords).
//   - en : variante britannique avec "and" ("one hundred AND twenty-three")
//     non implémentée (style américain sans "and" ici, choix documenté).
//   - dates/heures : pas de jour de la semaine, pas de format textuel ("12
//     mars 2026", "March 12th") — formats NUMÉRIQUES uniquement (JJ/MM/AAAA,
//     MM/DD/AAAA, HHhMM, HH:MM).
//   - ordinaux : suffixe anglais non revalidé contre le chiffre (ex. "3nd" est
//     accepté et traité comme "3rd" le serait — simplification tolérante,
//     seule la valeur numérique compte pour la sortie).
//   - fr/en : abréviations au-delà de la petite liste sourcée [5] (élargie de
//     classe "titre"/"autre" cette itération, mais toujours courte) ; sigles/
//     acronymes (lus lettre par lettre par certains systèmes, non traité ici) ;
//     ponctuation autre que . , ; : ! ? traitée comme séparateur neutre (pas de
//     pause spécifique) — parenthèses, guillemets, tiret, cf. limite [6].
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace ai {

		// Langue cible de la normalisation (portée fr/en, voir en-tête : bbj exclu,
		// numération non sourcée à ce stade).
		enum class NkTextNormLang : uint8 {
			Fr = 0,
			En = 1,
		};

		// Variante régionale de la numération française 70-99 (SOURCES [dialect]).
		// Standard = France métropolitaine ("soixante-dix"/"quatre-vingts"/
		// "quatre-vingt-dix", comportement PAR DÉFAUT, inchangé). BelgeSuisse =
		// "septante"/"huitante"/"nonante" (Belgique + cantons suisses de Vaud/
		// Valais/Fribourg pour "huitante" — Genève/Jura/Neuchâtel gardent
		// "quatre-vingts" même en Suisse ; "octante" est obsolète partout,
		// volontairement PAS produit ici, cf. SOURCES [dialect]). Sans effet sur
		// `NkTextNormLang::En`.
		enum class NkFrNumberDialect : uint8 {
			Standard = 0,
			BelgeSuisse = 1,
		};

		// Format de lecture de l'heure (SOURCES [ampm]/[minuit-midi]). H24 = 24h
		// littéral (comportement PAR DÉFAUT, inchangé ; en fr, "minuit"/"midi"
		// remplacent désormais "zéro heure"/"douze heures" pour 0h/12h quel que
		// soit ce paramètre — pas de convention AM/PM standard en français). H12 =
		// 12h + suffixe am/pm (ANGLAIS uniquement ; ignoré en français, cf.
		// ExpandTimeLiteral).
		enum class NkTextNormTimeFormat : uint8 {
			H24 = 0,
			H12 = 1,
		};

		// Lecture orale de l'année dans une date (SOURCES [year-pairs], ANGLAIS
		// uniquement — sans effet en français, qui n'a pas de convention par
		// paires attestée). Full = cardinal plein ("two thousand twenty-six",
		// comportement PAR DÉFAUT, inchangé). Paired = lecture par paires de deux
		// chiffres ("twenty twenty-six", "nineteen eighty-four") — repli
		// automatique sur Full pour les années hors [1000,9999] ou "rondes"
		// (dernier deux chiffres à 00, ex. 1900/2000 : convention incohérente
		// selon les sources, cf. YearToWords).
		enum class NkTextNormYearReading : uint8 {
			Full = 0,
			Paired = 1,
		};

		// Nature d'un token normalisé : mot "parlable" (prêt pour NkG2P::ToPhonemes)
		// ou pause symbolique (issue de ponctuation), avec sa durée indicative.
		enum class NkTextNormTokenKind : uint8 {
			Word = 0,
			Pause = 1,
		};

		// Un token de sortie de NkTextNorm::Normalize. Si kind == Word, `text`
		// contient une ou plusieurs "mots" séparés par des espaces/traits d'union
		// (ex. un nombre développé "cent vingt-trois") ; si kind == Pause, `text`
		// est vide et `pauseMs` porte la durée indicative (voir SOURCES [6]).
		struct NkTextNormToken {
				NkTextNormTokenKind kind = NkTextNormTokenKind::Word;
				NkString text;			  // valide si kind == Word
				float32 pauseMs = 0.0f;	  // valide si kind == Pause

				NkTextNormToken() = default;

				static NkTextNormToken MakeWord(const NkString &t) {
					NkTextNormToken tok;
					tok.kind = NkTextNormTokenKind::Word;
					tok.text = t;
					return tok;
				}

				static NkTextNormToken MakePause(float32 ms) {
					NkTextNormToken tok;
					tok.kind = NkTextNormTokenKind::Pause;
					tok.pauseMs = ms;
					return tok;
				}
		};

		// Normalisation de texte du front-end TTS : nombres -> lettres, ponctuation
		// -> pauses symboliques, quelques abréviations sourcées. Sans état (méthodes
		// statiques), comme NkG2P/NkAudioFeatures/NkGriffinLim.
		class NkTextNorm {
			public:
				// Nombre CARDINAL entier (signé) -> texte en toutes lettres. Gère le
				// signe ("moins"/"minus"), zéro, et l'échelle courte/longue selon la
				// langue (SOURCES [1][2]) jusqu'au groupe des "billions"/"trillion"
				// (10^12), largement au-delà de l'exigence "au moins jusqu'aux
				// milliers". Voir SOURCES [3] pour les règles d'accord françaises.
				// `dialect` (SOURCES [dialect], défaut Standard = comportement
				// inchangé) : BelgeSuisse produit "septante"/"huitante"/"nonante" pour
				// 70-99 au lieu de "soixante-dix"/"quatre-vingts"/"quatre-vingt-dix".
				// Sans effet si `lang == En`.
				static NkString NumberToWords(int64 value, NkTextNormLang lang,
											   NkFrNumberDialect dialect = NkFrNumberDialect::Standard);

				// Nombre décimal simple donné en texte (ex. "123", "-42", "3,14" fr ou
				// "3.14" en) -> texte en toutes lettres, partie décimale lue chiffre
				// par chiffre après "virgule"/"point" (SOURCES [4]). Accepte le
				// séparateur ',' ou '.' indépendamment de la langue (robustesse) ;
				// seule la lecture du connecteur ("virgule" vs "point") dépend de
				// `lang`. Renvoie une chaîne vide si `literal` n'est pas un nombre
				// reconnu (pas d'assertion : robustesse en amont d'un pipeline texte).
				// `dialect` : voir NumberToWords (répercuté sur la partie entière).
				static NkString ExpandNumberLiteral(const NkString &literal, NkTextNormLang lang,
													 NkFrNumberDialect dialect = NkFrNumberDialect::Standard);

				// Nombre ORDINAL entier (value >= 1) -> texte en toutes lettres (ex. 1 ->
				// "premier"/"premiere" fr / "first" en, 21 -> "vingt et unieme" fr /
				// "twenty-first" en, 100 -> "centieme" fr / "one hundredth" en).
				// Construit sur NumberToWords : seul le dernier "mot" du cardinal recoit
				// la transformation ordinale (SOURCES [7] fr, [8] en). Renvoie une chaine
				// vide si value <= 0 (pas d'ordinal pour zero/negatif, limite honnete).
				// `feminine` (SOURCES [fem-ord], défaut false = comportement inchange) :
				// seul "premier"/"premiere" (value == 1, fr) varie en genre — les autres
				// ordinaux francais sont epicenes (2e, 3e... identiques aux deux genres),
				// et `feminine` est ignore en anglais. `dialect` (SOURCES [dialect]) :
				// voir NumberToWords, repercute sur le mot cardinal de base (ex. 70 ->
				// "septantieme" en BelgeSuisse).
				static NkString OrdinalToWords(int64 value, NkTextNormLang lang, bool feminine = false,
												NkFrNumberDialect dialect = NkFrNumberDialect::Standard);

				// Detecte et etend un LITTERAL ordinal ecrit chiffres+suffixe (ex. "21e",
				// "1er", "1re", "1ere", "2eme", "1st", "2nd", "3rd", "4th", "11th") ->
				// texte en toutes lettres via OrdinalToWords. Le suffixe n'est PAS
				// revalide contre le chiffre (simplification tolerante aux fautes de
				// frappe, ex. "3nd" est accepte comme "3rd" le serait), SAUF pour le
				// marqueur feminin "re"/"ere" (SOURCES [fem-ord]) qui est detecte ici et
				// produit "premiere" au lieu de "premier" pour value == 1. Renvoie une
				// chaine vide si `literal` ne commence pas par des chiffres.
				static NkString ExpandOrdinalLiteral(const NkString &literal, NkTextNormLang lang,
													  NkFrNumberDialect dialect = NkFrNumberDialect::Standard);

				// Detecte et etend un LITTERAL date numerique simple "D[D]/M[M]/AAAA" ou
				// "D[D]/M[M]/AA" (annee sur 2 chiffres, SOURCES [2digit-year]) -> texte en
				// toutes lettres (ex. "12/03/2026" fr -> "douze mars deux mille
				// vingt-six" ; "03/12/2026" en -> "march twelfth two thousand
				// twenty-six"). Ordre JJ/MM (fr) ou MM/DD (en) SELON `lang` (SOURCES [9]).
				// Annee a 4 OU 2 chiffres : sur 2 chiffres, windowing POSIX standard
				// (SOURCES [2digit-year]) 00-68 -> 20xx, 69-99 -> 19xx. Validation REELLE
				// du calendrier (SOURCES [leap-year]) : mois 1-12, jour 1-31 ET jour <=
				// nombre de jours reel du mois (fevrier variable selon l'annee
				// bissextile, regle gregorienne complete) — ex. "30/02/2026" et
				// "31/04/2026" sont desormais REJETES (chaine vide), alors qu'ils
				// n'etaient pas verifies avant cette version. `dialect` (SOURCES
				// [dialect]) : repercute sur le jour/l'annee cardinaux fr. `yearReading`
				// (SOURCES [year-pairs], ANGLAIS uniquement) : lecture de l'annee en
				// cardinal plein (Full, defaut) ou par paires (Paired), cf. YearToWords.
				// Renvoie une chaine vide si non reconnu/hors bornes/calendrierement
				// invalide.
				static NkString ExpandDateLiteral(const NkString &literal, NkTextNormLang lang,
												   NkFrNumberDialect dialect = NkFrNumberDialect::Standard,
												   NkTextNormYearReading yearReading = NkTextNormYearReading::Full);

				// Annee (4 chiffres, deja resolue si elle provenait d'un format 2
				// chiffres) -> texte en toutes lettres, SEUL le mode de lecture change
				// (SOURCES [year-pairs]). fr : toujours cardinal plein, `reading` ignore
				// (aucune convention par paires attestee en francais). en, Full :
				// cardinal plein inchange ("two thousand twenty-six"). en, Paired :
				// lecture par paires de deux chiffres ("nineteen eighty-four", "twenty
				// twenty-six"), UNIQUEMENT pour une annee dans [1000,9999] dont les DEUX
				// derniers chiffres sont non nuls (repli automatique sur le cardinal
				// plein sinon — annees "rondes" type 1900/2000, convention incoherente
				// selon les sources, ou hors bornes).
				static NkString YearToWords(int32 year, NkTextNormLang lang,
											 NkTextNormYearReading reading = NkTextNormYearReading::Full);

				// Detecte et etend un LITTERAL heure numerique simple "HH(h|:)MM" -> texte
				// en toutes lettres (ex. "15h30"/"15:30" fr -> "quinze heures trente" ;
				// "15:30" en -> "fifteen thirty"). fr : "minuit" (0h) et "midi" (12h)
				// remplacent desormais "zero heure"/"douze heures" (SOURCES
				// [minuit-midi]) ; `format` est IGNORE en francais (pas de convention
				// AM/PM standard). en : `format == H24` (defaut, comportement inchange)
				// ou `format == H12` -> conversion 12h + suffixe am/pm (SOURCES [ampm] :
				// 0h -> "twelve o clock am", 12h -> "twelve o clock pm", modulo 12
				// sinon). Renvoie une chaine vide si hors bornes (heure > 23, minute >
				// 59) ou non reconnu.
				static NkString ExpandTimeLiteral(const NkString &literal, NkTextNormLang lang,
												   NkTextNormTimeFormat format = NkTextNormTimeFormat::H24);

				// Durée de pause indicative (ms) associée à un caractère de ponctuation
				// reconnu (. , ; : ! ?) — SOURCES [6]. Renvoie 0 pour tout autre
				// caractère (pas une pause reconnue ici).
				static float32 PauseDurationMs(char punct);

				// Pipeline complet : texte brut -> suite de tokens (mots développés +
				// pauses). Les nombres rencontrés (entiers ou décimaux simples) sont
				// développés via ExpandNumberLiteral ; la ponctuation reconnue devient
				// une pause (fusion des ponctuations consécutives, ex. "..." ou "?!" ->
				// une seule pause, durée = max des durées individuelles) ; les
				// abréviations sourcées (SOURCES [5]) sont développées ; tout le reste
				// (mots ordinaires) passe inchangé, prêt pour NkG2P::ToPhonemes.
				// Désambiguïsation abréviation/fin de phrase (SOURCES [abbrev-sbd]) :
				// un titre de civilité (M./Mme/Mlle/Dr/Mr/Mrs) suivi d'un '.' reçoit
				// toujours une pause COURTE (jamais la pause longue de fin de phrase,
				// puisqu'un titre est par définition suivi d'un nom) ; "etc." suivi
				// d'un '.' reçoit une pause longue si le mot suivant commence par une
				// MAJUSCULE (heuristique de casse, ≈90% des points sont des fins de
				// phrase selon la littérature SBD citée), courte sinon. `dialect`/
				// `yearReading` (défauts Standard/Full = comportement inchangé) sont
				// répercutés sur les nombres/ordinaux/dates rencontrés (SOURCES
				// [dialect]/[year-pairs]) ; les heures rencontrées dans le texte
				// restent toujours en 24h (le format 12h n'est exposé que via un appel
				// direct à ExpandTimeLiteral, cf. cette fonction).
				static NkVector<NkTextNormToken> Normalize(const NkString &text, NkTextNormLang lang,
															NkFrNumberDialect dialect = NkFrNumberDialect::Standard,
															NkTextNormYearReading yearReading = NkTextNormYearReading::Full);

				// Convenience : aplatit Normalize() en un unique NkString "parlable"
				// (tokens Word séparés par un espace ; les tokens Pause n'ajoutent pas
				// de marqueur textuel car NkG2P insère déjà un silence à chaque
				// frontière de mot/espace — cf. NkG2P.cpp — seule la DURÉE différenciée
				// de la pause est perdue dans ce texte à plat ; utiliser Normalize()
				// directement si les durées de pause doivent être préservées en aval).
				// Résultat directement utilisable : NkG2P::ToPhonemes(result, lang2p).
				static NkString NormalizeToText(const NkString &text, NkTextNormLang lang,
												 NkFrNumberDialect dialect = NkFrNumberDialect::Standard,
												 NkTextNormYearReading yearReading = NkTextNormYearReading::Full);

				// Auto-test headless : cas réels couvrant nombres entiers (0, dizaines
				// irrégulières fr 70-99, centaines, milliers, millions), décimaux
				// fr/en, ponctuation -> pauses, abréviations sourcées, accord
				// cent/quatre-vingts devant million/milliard (fix 2026-07-25, SOURCES
				// [3bis]), ordinaux fr/en (SOURCES [7][8]), dates/heures fr/en (SOURCES
				// [9]) et leur détection dans Normalize() ; PLUS (fix 2026-07-25) :
				// variante régionale fr septante/huitante/nonante (SOURCES [dialect]),
				// désambiguïsation abréviation/fin de phrase (SOURCES [abbrev-sbd]),
				// ordinal féminin "1re"/"première" (SOURCES [fem-ord]), minuit/midi +
				// 12h/AM-PM (SOURCES [minuit-midi]/[ampm]), année sur 2 chiffres
				// (SOURCES [2digit-year]), lecture de l'année par paires en anglais
				// (SOURCES [year-pairs]), validité calendaire réelle (SOURCES
				// [leap-year]).
				static bool SelfTest();
		};

	} // namespace ai
} // namespace nkentseu
