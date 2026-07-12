#pragma once
// =============================================================================
// NkSyntaxLangs.h — Langages GÉNÉRIQUES par défaut pour la coloration
// data-driven (NkLangSpec). Mots-clés issus des définitions libres publiques
// (références croisées grammaires TextMate/Kate/highlight.js, licences
// MIT/BSD — listes de mots réservés des langages, non protégeables).
// Appelé UNE fois au boot (main). Extensible plus tard via data/syntax/*.cfg.
// =============================================================================
#include "NKCode/Editor/NkSyntax.h"

namespace nkentseu {
	namespace nkcode {

		inline void NkSynInitDefaultLangs() {
			NkVector<NkLangSpec> &out = NkSynSpecs();
			if (!out.Empty())
				return; // déjà initialisé
			auto split = [](const char *s, NkVector<NkString> &dst) {
				NkString cur;
				for (const char *p = s;; ++p) {
					if (*p == ' ' || *p == '\0') {
						if (!cur.Empty()) {
							dst.PushBack(cur);
							cur.Clear();
						}
						if (*p == '\0')
							break;
					} else
						cur += *p;
				}
			};
			auto add = [&](const char *name, const char *exts, const char *lineC, const char *bOpen,
						   const char *bClose, const char *strs, const char *kw, const char *ty) {
				NkLangSpec sp;
				sp.name = name;
				split(exts, sp.exts);
				sp.lineComment = lineC;
				sp.blockOpen = bOpen;
				sp.blockClose = bClose;
				sp.strChars = strs;
				split(kw, sp.keywords);
				split(ty, sp.types);
				NkSymSortDedup(sp.keywords); // requis par la recherche binaire NkSymHas
				NkSymSortDedup(sp.types);
				out.PushBack(sp);
			};

			add("CSS", ".css .scss .less", "", "/*", "*/", "\"'",
				"important inherit initial unset auto none hover focus active before after root media supports "
				"keyframes import charset font-face not and only screen print",
				"color background margin padding border width height display position top left right bottom "
				"font flex grid gap opacity transform transition animation content overflow cursor z-index");
			add("JavaScript", ".js .mjs .cjs .jsx", "//", "/*", "*/", "\"'`",
				"var let const function return if else for while do switch case break continue default new delete "
				"typeof instanceof in of class extends super this null undefined true false try catch finally "
				"throw async await yield import export from static get set",
				"console window document Math JSON Promise Array Object String Number Boolean Map Set Symbol");
			add("TypeScript", ".ts .tsx", "//", "/*", "*/", "\"'`",
				"var let const function return if else for while do switch case break continue default new delete "
				"typeof instanceof in of class extends super this null undefined true false try catch finally "
				"throw async await yield import export from static get set interface type enum namespace declare "
				"readonly public private protected abstract implements keyof as is",
				"string number boolean any void never unknown object console Promise Array Map Set Record Partial");
			add("JSON", ".json .jsonc", "//", "/*", "*/", "\"", "true false null", "");
			add("Lua", ".lua .luau", "--", "--[[", "]]", "\"'",
				"and break do else elseif end false for function goto if in local nil not or repeat return then "
				"true until while",
				"print pairs ipairs table string math io os type tostring tonumber require");
			add("Java", ".java", "//", "/*", "*/", "\"'",
				"abstract assert break case catch class const continue default do else enum extends final finally "
				"for goto if implements import instanceof interface native new package private protected public "
				"return static strictfp super switch synchronized this throw throws transient try volatile while "
				"true false null var record sealed permits",
				"void int long short byte char float double boolean String Object List Map Set Integer");
			add("CSharp", ".cs", "//", "/*", "*/", "\"'",
				"abstract as base break case catch checked class const continue default delegate do else enum "
				"event explicit extern false finally fixed for foreach goto if implicit in interface internal is "
				"lock namespace new null operator out override params private protected public readonly ref "
				"return sealed sizeof stackalloc static struct switch this throw true try typeof unchecked unsafe "
				"using virtual volatile while var async await record init required",
				"void int uint long ulong short ushort byte sbyte char float double decimal bool string object "
				"List Dictionary Task Action Func");
			add("Rust", ".rs", "//", "/*", "*/", "\"",
				"as async await break const continue crate dyn else enum extern false fn for if impl in let loop "
				"match mod move mut pub ref return self Self static struct super trait true type unsafe use where "
				"while macro_rules",
				"i8 i16 i32 i64 i128 isize u8 u16 u32 u64 u128 usize f32 f64 bool char str String Vec Option "
				"Result Box Rc Arc HashMap");
			add("Zig", ".zig", "//", "", "", "\"",
				"const var fn pub return if else while for switch defer errdefer try catch orelse and or break "
				"continue struct enum union error test comptime inline export extern packed align volatile "
				"allowzero async await suspend resume nosuspend threadlocal unreachable usingnamespace",
				"i8 i16 i32 i64 u8 u16 u32 u64 usize isize f16 f32 f64 bool void type anyerror anytype noreturn");
			add("Go", ".go", "//", "/*", "*/", "\"'`",
				"break case chan const continue default defer else fallthrough for func go goto if import "
				"interface map package range return select struct switch type var true false nil iota",
				"int int8 int16 int32 int64 uint uint8 uint16 uint32 uint64 uintptr float32 float64 complex64 "
				"complex128 bool byte rune string error make new len cap append copy delete panic recover");
			add("YAML", ".yaml .yml", "#", "", "", "\"'", "true false null yes no on off", "");
			add("TOML", ".toml", "#", "", "", "\"'", "true false", "");
			add("Ini", ".ini .cfg .conf .properties", "#", "", "", "\"'", "true false", "");
			add("Shell", ".sh .bash .zsh", "#", "", "", "\"'",
				"if then else elif fi for while do done case esac in function return break continue local export "
				"source alias set unset shift exit trap true false",
				"echo printf cd ls rm cp mv mkdir grep sed awk cat test read");
			add("PowerShell", ".ps1 .psm1 .psd1", "#", "<#", "#>", "\"'",
				"if else elseif switch foreach for while do until break continue return function param begin "
				"process end try catch finally throw trap in filter workflow class enum using true false null",
				"Write-Host Write-Output Get-Item Set-Item Get-Content Set-Content Get-Process Get-ChildItem");
			add("Batch", ".bat .cmd", "rem", "", "", "\"",
				"echo set if else exist not goto call start exit for in do rem pause setlocal endlocal shift "
				"errorlevel defined off on",
				"");
			add("SQL", ".sql", "--", "/*", "*/", "\"'",
				"select from where insert into values update delete create table drop alter index view join "
				"inner outer left right on group by order having distinct union all as and or not null primary "
				"key foreign references limit offset between like in exists case when then else end",
				"int integer varchar char text date datetime timestamp float double decimal boolean blob");
			add("Html", ".html .htm .xml .svg .xaml .axaml", "", "<!--", "-->", "\"'",
				"html head body div span script style link meta title img table tr td th ul ol li form input "
				"button a p h1 h2 h3 br hr nav footer header section article class id href src rel type",
				"");
			add("Php", ".php", "//", "/*", "*/", "\"'",
				"abstract and array as break callable case catch class clone const continue declare default do "
				"echo else elseif empty enddeclare endfor endforeach endif endswitch endwhile extends final "
				"finally fn for foreach function global goto if implements include instanceof insteadof "
				"interface isset list match namespace new or print private protected public readonly require "
				"return static switch throw trait try unset use var while xor yield true false null",
				"int float string bool object mixed void never iterable self parent");
		}

	} // namespace nkcode
} // namespace nkentseu
