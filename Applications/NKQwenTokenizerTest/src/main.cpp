// =============================================================================
// NKQwenTokenizerTest — jalon 1 du chantier QLoRA (cf Documentation/
// notes_etape4_qlora.md §2.1 et §4) : valide NkQwen2Tokenizer SANS GPU.
//
// Trois parties :
//   (a) mini-vocab SYNTHÉTIQUE en dur (aucun fichier) : les 256 chaînes
//       mono-caractère remappées — reconstruites ICI indépendamment du code de
//       production, depuis la spec bytes_to_unicode — + quelques fusions
//       choisies + les 3 tokens spéciaux ChatML. Encode/decode vérifiés À LA
//       MAIN, y compris l'ordre d'application des fusions par RANG (pas par
//       position) et la pré-tokenisation (contractions, borne 3 chiffres,
//       espaces).
//   (b) round-trip decode(encode(x)) == x OCTET À OCTET sur les 1 000
//       premières lignes réelles du corpus SFT BulkGen — avec le mini-vocab
//       synthétique, qui couvre TOUS les octets (propriété byte-level) : le
//       round-trip est donc exigible sur du texte arbitraire, accents inclus.
//   (c) SI un chemin GGUF est fourni en argv[1] : chargement du VRAI
//       vocabulaire (152 064 tokens + fusions), round-trip sur les mêmes
//       lignes, et affichage des ids de « Bonjour », « le », « <|im_start|> »
//       pour vérification manuelle contre une référence externe. Sans
//       argument, (c) est SAUTÉ avec un message clair — jamais un échec.
//
// Usage : NKQwenTokenizerTest.exe [chemin_gguf_ou_blob_ollama] [chemin_corpus]
//   corpus par défaut : D:\Projets\Camrail\AI\BulkGen\dlg_ollama_fr.txt
// =============================================================================
#include "NKInfer/NkQwen2Tokenizer.h"
#include "NKFileSystem/NkFile.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace nkentseu;
using namespace nkentseu::ai;

static int g_pass = 0, g_fail = 0;

static void Check(bool ok, const char *name) {
	(ok ? g_pass : g_fail)++;
	printf("  [ %s ] %s\n", ok ? "OK" : "KO", name);
}

// =============================================================================
// Reconstruction INDÉPENDANTE de la table bytes_to_unicode (spec GPT-2), pour
// que le mini-vocab du test ne soit PAS dérivé du code de production testé :
// les imprimables 33-126, 161-172, 174-255 restent eux-mêmes ; les 68 octets
// exclus reçoivent 256+n dans l'ordre croissant des octets exclus.
// =============================================================================
static int32 TestByteToCp(int32 b) {
	const bool keep = (b >= 33 && b <= 126) || (b >= 161 && b <= 172) || (b >= 174 && b <= 255);
	if (keep)
		return b;
	int32 n = 0;
	for (int32 x = 0; x < b; ++x) {
		const bool k = (x >= 33 && x <= 126) || (x >= 161 && x <= 172) || (x >= 174 && x <= 255);
		if (!k)
			n++;
	}
	return 256 + n;
}

// Encodage UTF-8 minimal du test (codepoints < 0x800 uniquement : la table ne
// dépasse jamais 323) — écrit indépendamment du helper de production.
static NkString TestCpToUtf8(int32 cp) {
	NkString s;
	if (cp < 0x80) {
		s.Append((char)cp);
	} else {
		s.Append((char)(0xC0 | (cp >> 6)));
		s.Append((char)(0x80 | (cp & 0x3F)));
	}
	return s;
}

// Compare deux NkString octet à octet (l'égalité de contenu, pas de pointeur).
static bool SameBytes(const NkString &a, const NkString &b) {
	if (a.Size() != b.Size())
		return false;
	return std::memcmp(a.Data(), b.Data(), (size_t)a.Size()) == 0;
}

// Représentation lisible d'une liste d'ids (pour les messages d'échec).
static void PrintIds(const char *label, const NkVector<int32> &ids) {
	printf("    %s = [", label);
	for (nk_size i = 0; i < ids.Size(); ++i)
		printf("%s%d", i == 0 ? "" : ", ", ids[i]);
	printf("]\n");
}

// =============================================================================
// PARTIE (a) : mini-vocab synthétique en dur.
// =============================================================================

// Ids du mini-vocab (256 tokens de base + fusions + spéciaux) — tenus à jour À
// LA MAIN pour que les attendus du test soient indépendants du chargement.
enum : int32 {
	kIdBo = 256,	   // "Bo"
	kIdNj = 257,	   // "nj"
	kIdBonj = 258,	   // "Bonj"
	kIdOu = 259,	   // "ou"
	kIdOur = 260,	   // "our"
	kIdBonjour = 261,  // "Bonjour"
	kIdGl = 262,	   // "Ġl"  (espace remappé + l)
	kIdGle = 263,	   // "Ġle"
	kIdEot = 264,	   // <|endoftext|>
	kIdImStart = 265,  // <|im_start|>
	kIdImEnd = 266,	   // <|im_end|>
	kMiniVocabSize = 267
};

static void BuildMiniVocab(NkVector<NkString> &tokens, NkVector<NkString> &merges) {
	// 256 chaînes mono-caractère remappées (ids 0..255 == octet source).
	for (int32 b = 0; b < 256; ++b)
		tokens.PushBack(TestCpToUtf8(TestByteToCp(b)));

	const NkString G = TestCpToUtf8(TestByteToCp((int32)' ')); // « Ġ » (U+0120)

	// Fusions dans l'ordre des RANGS (0..7). Chaque résultat est aussi ajouté
	// au vocabulaire (le fichier merges ne donne jamais l'id : il est retrouvé
	// par lookup de la concaténation, exactement comme sur un GGUF réel).
	tokens.PushBack(NkString("Bo"));	  // 256
	tokens.PushBack(NkString("nj"));	  // 257
	tokens.PushBack(NkString("Bonj"));	  // 258
	tokens.PushBack(NkString("ou"));	  // 259
	tokens.PushBack(NkString("our"));	  // 260
	tokens.PushBack(NkString("Bonjour")); // 261
	NkString gl = G;
	gl.Append('l');
	tokens.PushBack(gl); // 262 "Ġl"
	NkString gle = gl;
	gle.Append('e');
	tokens.PushBack(gle); // 263 "Ġle"
	tokens.PushBack(NkString("<|endoftext|>")); // 264
	tokens.PushBack(NkString("<|im_start|>"));	// 265
	tokens.PushBack(NkString("<|im_end|>"));	// 266

	merges.PushBack(NkString("B o"));		// rang 0
	merges.PushBack(NkString("n j"));		// rang 1
	merges.PushBack(NkString("Bo nj"));		// rang 2
	merges.PushBack(NkString("o u"));		// rang 3
	merges.PushBack(NkString("ou r"));		// rang 4
	merges.PushBack(NkString("Bonj our"));	// rang 5
	NkString mGl = G;
	mGl.Append(" l");
	merges.PushBack(mGl); // rang 6 : "Ġ l"
	NkString mGle = gl;
	mGle.Append(" e");
	merges.PushBack(mGle); // rang 7 : "Ġl e"
}

// Vérifie qu'un Encode produit EXACTEMENT la liste attendue.
static void CheckEncode(const infer::NkQwen2Tokenizer &tok, const char *text, const int32 *expected, int32 nExpected,
						const char *name) {
	NkVector<int32> ids;
	bool ok = tok.Encode(NkString(text), ids);
	bool same = ok && (int32)ids.Size() == nExpected;
	if (same) {
		for (int32 i = 0; i < nExpected; ++i)
			if (ids[(nk_size)i] != expected[i])
				same = false;
	}
	Check(same, name);
	if (!same)
		PrintIds("obtenu", ids);
}

// Vérifie la pré-tokenisation : segments attendus, dans l'ordre.
static void CheckPreTok(const char *text, const char *const *expected, int32 nExpected, const char *name) {
	NkVector<NkString> segs;
	infer::NkQwen2Tokenizer::PreTokenize(NkString(text), segs);
	bool same = (int32)segs.Size() == nExpected;
	if (same) {
		for (int32 i = 0; i < nExpected; ++i)
			if (!SameBytes(segs[(nk_size)i], NkString(expected[i])))
				same = false;
	}
	Check(same, name);
	if (!same) {
		printf("    segments obtenus : ");
		for (nk_size i = 0; i < segs.Size(); ++i)
			printf("[%s] ", segs[i].CStr());
		printf("\n");
	}
}

static void CheckRoundTrip(const infer::NkQwen2Tokenizer &tok, const NkString &text, const char *name) {
	NkVector<int32> ids;
	bool ok = tok.Encode(text, ids);
	NkString back = tok.Decode(ids);
	Check(ok && SameBytes(back, text), name);
}

static void RunSyntheticTests(infer::NkQwen2Tokenizer &tok) {
	printf("-- PARTIE (a) : mini-vocab synthétique en dur (aucun fichier) --\n");

	NkVector<NkString> tokens, merges;
	BuildMiniVocab(tokens, merges);

	NkString err;
	bool loaded = tok.LoadFromLists(tokens, merges, &err);
	Check(loaded, "LoadFromLists réussi sur le mini-vocab synthétique");
	if (!loaded) {
		printf("  Erreur : %s\n", err.CStr());
		return;
	}
	Check(tok.VocabSize() == kMiniVocabSize, "VocabSize == 267 (256 octets + 8 fusions + 3 spéciaux)");
	Check(tok.MergeCount() == 8 && tok.SkippedMergeCount() == 0, "8 fusions chargées, 0 ignorée");

	// ---- table octet<->codepoint : vérifs ponctuelles calculées à la main ----
	// 'A' (65, imprimable) reste lui-même -> id de base 65.
	// L'espace (32, exclu) devient « Ġ » = U+0120 (0x20 est le 33e octet exclu,
	// n=32 -> 256+32=288=0x120) -> le token id 32 du vocab est la chaîne "Ġ".
	Check(tok.FindTokenId(NkString("A")) == 65, "table : 'A' (imprimable) est son propre token de base (id 65)");
	Check(tok.FindTokenId(TestCpToUtf8(0x120)) == 32, "table : l'espace remappé « Ġ » (U+0120) est le token id 32");
	Check(tok.FindTokenId(NkString("Bonjour")) == kIdBonjour, "lookup exact : \"Bonjour\" -> id 261");
	Check(tok.FindTokenId(NkString("zzz_absent")) == -1, "lookup exact : chaîne absente -> -1 (pas de faux positif)");

	// ---- tokens spéciaux résolus par chaîne ----
	Check(tok.EndOfTextId() == kIdEot && tok.ImStartId() == kIdImStart && tok.ImEndId() == kIdImEnd,
		  "spéciaux résolus par chaîne : <|endoftext|>=264, <|im_start|>=265, <|im_end|>=266");

	// ---- encode : fusions dans le bon ordre de RANG ----
	{
		// "Bonjour" : B o n j o u r -> (B,o)r0 -> (n,j)r1 -> (Bo,nj)r2 ->
		// (o,u)r3 -> (ou,r)r4 -> (Bonj,our)r5 -> [261]. Chaque étape vérifiée à
		// la main : si l'ordre de rang était faux, le résultat final différerait.
		const int32 exp[] = {kIdBonjour};
		CheckEncode(tok, "Bonjour", exp, 1, "Encode(\"Bonjour\") == [261] (cascade complète des 6 fusions)");
	}
	{
		// "Bou" : les paires candidates sont (B,o) rang 0 et (o,u) rang 3 — le
		// rang MINIMAL doit gagner : Bo u -> [256,117]. Si l'implémentation
		// fusionnait par position ou par rang max, on obtiendrait [66,259].
		const int32 exp[] = {kIdBo, (int32)'u'};
		CheckEncode(tok, "Bou", exp, 2, "Encode(\"Bou\") == [256,117] (rang 0 prioritaire sur rang 3)");
	}
	{
		// " le" : espace collé au mot (Ġ l e) -> (Ġ,l)r6 -> (Ġl,e)r7 -> [263].
		const int32 exp[] = {kIdGle};
		CheckEncode(tok, " le", exp, 1, "Encode(\" le\") == [263] (espace remappé fusionné avec le mot)");
	}
	{
		// Phrase : "Bonjour le" -> ["Bonjour", " le"] -> [261, 263].
		const int32 exp[] = {kIdBonjour, kIdGle};
		CheckEncode(tok, "Bonjour le", exp, 2, "Encode(\"Bonjour le\") == [261,263]");
	}
	{
		// "monde" : aucune fusion applicable -> 5 tokens de base.
		const int32 exp[] = {(int32)'m', (int32)'o', (int32)'n', (int32)'d', (int32)'e'};
		CheckEncode(tok, "monde", exp, 5, "Encode(\"monde\") == octets de base (aucune fusion applicable)");
	}

	// ---- pré-tokenisation : cas choisis, segments attendus à la main ----
	{
		const char *exp[] = {"He", "'s", " ok"};
		CheckPreTok("He's ok", exp, 3, "PreTok : contraction anglaise 's isolée");
	}
	{
		const char *exp[] = {"1234"};
		// borne Qwen : 3 chiffres max par token -> "123" + "4".
		const char *exp2[] = {"123", "4"};
		(void)exp;
		CheckPreTok("1234", exp2, 2, "PreTok : borne Qwen 1-3 chiffres (\"1234\" -> \"123\"+\"4\")");
	}
	{
		const char *exp[] = {"a", " ", " b"};
		CheckPreTok("a  b", exp, 3, "PreTok : suite d'espaces, le dernier part avec le mot suivant");
	}
	{
		const char *exp[] = {"l", "'", "e\xCC\x81" "cole"};
		(void)exp; // (variante décomposée non testée : le corpus est en NFC)
		const char *expNfc[] = {"l", "'", "\xC3\xA9" "cole"};
		CheckPreTok("l'\xC3\xA9" "cole", expNfc, 3, "PreTok : apostrophe non-contraction + mot accentué UTF-8 entier");
	}
	{
		const char *exp[] = {"Question", ":", " 42", " ,", " ok", "\n"};
		CheckPreTok("Question: 42 , ok\n", exp, 6, "PreTok : ponctuation avec espace optionnel + \\n final");
	}

	// ---- round-trip octet à octet (couvre TOUS les octets par construction) --
	CheckRoundTrip(tok, NkString("Bonjour le monde, il fait 23 \xC2\xB0" "C aujourd'hui !"),
				   "round-trip : phrase française accentuée == identité");
	CheckRoundTrip(tok, NkString("  \t\n\r\xC3\xA9\xC3\xA8\xC3\xAA 12345 --=[]{}"),
				   "round-trip : espaces mixtes + accents + chiffres + ponctuation");
	{
		// Tous les 256 octets d'affilée (y compris NUL) : NkString(buf, 256).
		char all[256];
		for (int32 i = 0; i < 256; ++i)
			all[i] = (char)i;
		NkString s(all, (NkString::SizeType)256);
		NkVector<int32> ids;
		bool ok = tok.Encode(s, ids);
		NkString back = tok.Decode(ids);
		Check(ok && SameBytes(back, s), "round-trip : les 256 octets bruts (0x00-0xFF) == identité");
	}

	// ---- EncodeWithSpecials ----
	{
		NkVector<int32> ids;
		NkString text("<|im_start|>user\nBonjour<|im_end|>");
		bool ok = tok.EncodeWithSpecials(text, ids);
		bool shape = ok && ids.Size() >= 3 && ids[0] == kIdImStart && ids[(nk_size)(ids.Size() - 1)] == kIdImEnd;
		Check(shape, "EncodeWithSpecials : <|im_start|> et <|im_end|> émis comme ids atomiques");
		bool hasBonjour = false;
		for (nk_size i = 0; i < ids.Size(); ++i)
			if (ids[i] == kIdBonjour)
				hasBonjour = true;
		Check(hasBonjour, "EncodeWithSpecials : le texte entre spéciaux passe par le BPE (\"Bonjour\" -> 261)");
		NkString back = tok.Decode(ids);
		Check(SameBytes(back, text), "EncodeWithSpecials + Decode : round-trip exact (spéciaux recopiés tels quels)");
	}
	{
		// Sans spéciaux dans le texte : EncodeWithSpecials == Encode.
		NkVector<int32> a, b;
		bool ok = tok.EncodeWithSpecials(NkString("Bonjour le"), a) && tok.Encode(NkString("Bonjour le"), b);
		bool same = ok && a.Size() == b.Size();
		if (same)
			for (nk_size i = 0; i < a.Size(); ++i)
				if (a[i] != b[i])
					same = false;
		Check(same, "EncodeWithSpecials sans spécial dans le texte == Encode");
	}
	printf("\n");
}

// =============================================================================
// Lecture des N premières lignes d'un fichier texte (séparateur '\n', le '\r'
// éventuel est conservé dans la ligne : il participe au round-trip).
// =============================================================================
static bool ReadFirstLines(const char *path, int32 maxLines, NkVector<NkString> &outLines) {
	NkFile f(path, NkFileMode::NK_READ_BINARY);
	if (!f.IsOpen())
		return false;
	const nk_int64 fileSize = f.Size();
	if (fileSize <= 0)
		return false;
	NkVector<char> buf;
	buf.Resize((nk_size)fileSize);
	const usize got = f.Read(buf.Data(), (usize)fileSize);
	if (got == 0)
		return false;
	const int64 n = (int64)got;
	int64 lineStart = 0;
	for (int64 i = 0; i < n && (int32)outLines.Size() < maxLines; ++i) {
		if (buf[(nk_size)i] == '\n') {
			outLines.PushBack(NkString(buf.Data() + lineStart, (NkString::SizeType)(i - lineStart)));
			lineStart = i + 1;
		}
	}
	if ((int32)outLines.Size() < maxLines && lineStart < n)
		outLines.PushBack(NkString(buf.Data() + lineStart, (NkString::SizeType)(n - lineStart)));
	return outLines.Size() > 0;
}

// Round-trip sur des lignes réelles ; renvoie le nombre de lignes exactes.
static int32 RoundTripLines(const infer::NkQwen2Tokenizer &tok, const NkVector<NkString> &lines, int64 &outTokenCount) {
	int32 nOk = 0;
	outTokenCount = 0;
	for (nk_size i = 0; i < lines.Size(); ++i) {
		NkVector<int32> ids;
		if (!tok.Encode(lines[i], ids))
			continue;
		outTokenCount += (int64)ids.Size();
		NkString back = tok.Decode(ids);
		if (SameBytes(back, lines[i]))
			nOk++;
		else if (nOk + 8 > (int32)i) { // n'affiche que les premières divergences
			printf("    [divergence] ligne %u : %u octets -> %u octets\n", (uint32)i, (uint32)lines[i].Size(),
				   (uint32)back.Size());
		}
	}
	return nOk;
}

int main(int argc, char **argv) {
	printf("=== NKQwenTokenizerTest : BPE byte-level Qwen2 (NKInfer, QLoRA jalon 1) ===\n\n");

	// -------------------------------------------------------------------
	// (a) Mini-vocab synthétique (aucun fichier, aucun GPU).
	// -------------------------------------------------------------------
	infer::NkQwen2Tokenizer miniTok;
	RunSyntheticTests(miniTok);

	// -------------------------------------------------------------------
	// (b) Round-trip sur les 1 000 premières lignes réelles du corpus SFT.
	// Le mini-vocab couvre tous les octets : l'identité octet-à-octet est
	// exigible quel que soit le contenu (français accentué du corpus inclus).
	// -------------------------------------------------------------------
	const char *corpusPath = (argc > 2) ? argv[2] : "D:\\Projets\\Camrail\\AI\\BulkGen\\dlg_ollama_fr.txt";
	printf("-- PARTIE (b) : round-trip sur le corpus réel --\n");
	printf("  Corpus : %s\n", corpusPath);
	{
		NkVector<NkString> lines;
		bool read = ReadFirstLines(corpusPath, 1000, lines);
		Check(read, "corpus lisible (au moins une ligne)");
		if (read) {
			int64 tokenCount = 0;
			const int32 nOk = RoundTripLines(miniTok, lines, tokenCount);
			printf("  %d/%u lignes en round-trip exact, %lld tokens produits (mini-vocab)\n", nOk, (uint32)lines.Size(),
				   (long long)tokenCount);
			Check(nOk == (int32)lines.Size(), "decode(encode(x)) == x octet à octet sur TOUTES les lignes lues");
		}
	}
	printf("\n");

	// -------------------------------------------------------------------
	// (c) OPTIONNEL : vrai vocabulaire depuis un GGUF (argv[1]).
	// -------------------------------------------------------------------
	printf("-- PARTIE (c) : vocabulaire réel depuis un GGUF (optionnel) --\n");
	if (argc > 1) {
		const char *ggufPath = argv[1];
		printf("  GGUF : %s\n", ggufPath);
		infer::NkQwen2Tokenizer realTok;
		NkString err;
		bool loaded = realTok.LoadFromGGUF(ggufPath, &err);
		Check(loaded, "LoadFromGGUF : tokens + merges lus en entier");
		if (!loaded) {
			printf("  Erreur : %s\n", err.CStr());
		} else {
			printf("  vocab=%lld tokens, fusions=%lld (ignorées=%lld)\n", (long long)realTok.VocabSize(),
				   (long long)realTok.MergeCount(), (long long)realTok.SkippedMergeCount());
			Check(realTok.VocabSize() > 100000, "vocabulaire réel plausible (> 100 000 tokens)");
			Check(realTok.MergeCount() > 100000, "fusions réelles plausibles (> 100 000)");
			Check(realTok.EndOfTextId() >= 0 && realTok.ImStartId() >= 0 && realTok.ImEndId() >= 0,
				  "les 3 tokens spéciaux ChatML trouvés par chaîne dans le vocab réel");

			NkVector<NkString> lines;
			if (ReadFirstLines(corpusPath, 1000, lines)) {
				int64 tokenCount = 0;
				const int32 nOk = RoundTripLines(realTok, lines, tokenCount);
				printf("  %d/%u lignes en round-trip exact, %lld tokens produits (vocab réel)\n", nOk,
					   (uint32)lines.Size(), (long long)tokenCount);
				Check(nOk == (int32)lines.Size(), "round-trip vocab RÉEL == identité sur toutes les lignes lues");
			} else {
				Check(false, "corpus lisible pour le round-trip vocab réel");
			}

			// Ids à vérifier À LA MAIN contre une référence externe (llama.cpp /
			// transformers) : c'est le juge de paix de la pré-tokenisation
			// approchée — cf en-tête de NkQwen2Tokenizer.h.
			NkVector<int32> ids;
			realTok.Encode(NkString("Bonjour"), ids);
			PrintIds("ids(\"Bonjour\")", ids);
			ids.Clear();
			realTok.Encode(NkString("le"), ids);
			PrintIds("ids(\"le\")", ids);
			ids.Clear();
			realTok.EncodeWithSpecials(NkString("<|im_start|>"), ids);
			PrintIds("ids(\"<|im_start|>\")", ids);
			Check(ids.Size() == 1 && ids[0] == realTok.ImStartId(),
				  "EncodeWithSpecials(\"<|im_start|>\") == [id du spécial] (1 seul token)");
		}
	} else {
		printf("  (c) SAUTÉ : aucun chemin GGUF fourni en argument 1 -- ce n'est pas un échec.\n");
		printf("      Pour l'activer : NKQwenTokenizerTest.exe <chemin_gguf_ou_blob_ollama> [corpus]\n");
	}

	printf("\n=== Résultat : %d OK, %d échec(s) ===\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}
