#include "NkDialogs.h"

// ---------------------------------------------------------------------------
// ImplÃ©mentations spÃ©cifiques aux plateformes
// ---------------------------------------------------------------------------

// Inclusions nÃ©cessaires selon la plateforme
#if defined(NKENTSEU_PLATFORM_WIN32) // Windows classique
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#include <cstring>
#pragma comment(lib, "comdlg32.lib")
#elif defined(NKENTSEU_PLATFORM_UWP) || defined(NKENTSEU_PLATFORM_XBOX)
// UWP et Xbox n'ont pas de dialogues Win32 classiques ; on utilisera des stubs
#elif defined(NKENTSEU_PLATFORM_COCOA)									// macOS
// Pour macOS, on utilisera des commandes via osascript
#elif defined(NKENTSEU_PLATFORM_XLIB) || defined(NKENTSEU_PLATFORM_XCB) // Linux
// Pour Linux, on utilisera Zenity (outil GTK en ligne de commande)
#elif defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_UIKIT) || defined(NKENTSEU_PLATFORM_WASM)
// Plateformes mobiles/Web : stubs
#endif

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

// ===========================================================================
// Windows (Win32)
// ===========================================================================
#if defined(NKENTSEU_PLATFORM_WIN32)

// Fonction utilitaire pour convertir un filtre utilisateur (ex: "*.png;*.jpg")
// en chaÃ®ne pour OPENFILENAME (double null-terminated avec des paires description|pattern)
static std::string Win32PrepareFilter(const std::string &userFilter) {
	if (userFilter.empty() || userFilter == "*.*")
		return "All Files\0*.*\0";

	// On va construire un filtre simple : on prend l'extension et on met un libellÃ©
	// Exemple : "*.png;*.jpg" -> "Image Files (*.png;*.jpg)\0*.png;*.jpg\0"
	std::string result;
	result.reserve(userFilter.size() + 32);

	// CrÃ©er une description basÃ©e sur l'extension
	result += "Fichiers (";
	result += userFilter;
	result += ")\0";
	result += userFilter;
	result += "\0";

	// Remplacer les ';' par des '\0' dans la partie pattern (OPENFILENAME attend des patterns sÃ©parÃ©s par des '\0')
	// Mais la chaÃ®ne doit avoir des '\0' entre chaque pattern et un double '\0' Ã  la fin.
	// Pour simplifier, on ne gÃ¨re pas les patterns multiples ; on les laisse tels quels,
	// l'utilisateur peut passer "*.png;*.jpg" et Ã§a fonctionnera avec l'API Windows?
	// En rÃ©alitÃ©, OPENFILENAME attend une liste de patterns sÃ©parÃ©s par ';' dans une seule chaÃ®ne, donc "*.png;*.jpg"
	// est correct. On ajoute juste un double null Ã  la fin.
	result.push_back('\0'); // dÃ©jÃ  un null de la fin de la chaÃ®ne prÃ©cÃ©dente, mais on en ajoute un pour doubler
	return result;
}

inline NkDialogResult NkDialogs::OpenFileDialog(const std::string &filter, const std::string &title) {
	char buf[MAX_PATH] = {};
	OPENFILENAMEA ofn = {};
	ofn.lStructSize = sizeof(ofn);
	std::string winFilter = Win32PrepareFilter(filter);
	ofn.lpstrFilter = winFilter.c_str();
	ofn.lpstrFile = buf;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrTitle = title.empty() ? nullptr : title.c_str();
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

	NkDialogResult r;
	r.confirmed = (GetOpenFileNameA(&ofn) == TRUE);
	r.path = buf;
	return r;
}

inline NkDialogResult NkDialogs::SaveFileDialog(const std::string &defaultExt, const std::string &title) {
	char buf[MAX_PATH] = {};
	OPENFILENAMEA ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFile = buf;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrTitle = title.empty() ? nullptr : title.c_str();
	ofn.lpstrDefExt = defaultExt.empty() ? nullptr : defaultExt.c_str();
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;

	NkDialogResult r;
	r.confirmed = (GetSaveFileNameA(&ofn) == TRUE);
	r.path = buf;
	return r;
}

inline void NkDialogs::OpenMessageBox(const std::string &message, const std::string &title, int type) {
	UINT flags = MB_OK;
	switch (type) {
		case 1:
			flags |= MB_ICONWARNING;
			break;
		case 2:
			flags |= MB_ICONERROR;
			break;
		default:
			flags |= MB_ICONINFORMATION;
			break;
	}
	MessageBoxA(nullptr, message.c_str(), title.empty() ? nullptr : title.c_str(), flags);
}

inline NkDialogResult NkDialogs::ColorPicker(NkU32 initial) {
	static COLORREF customColors[16] = {};
	CHOOSECOLORA cc = {};
	cc.lStructSize = sizeof(cc);
	// initial est RGBA, Windows utilise BGR
	cc.rgbResult = RGB((initial >> 16) & 0xFF, (initial >> 8) & 0xFF, initial & 0xFF);
	cc.lpCustColors = customColors;
	cc.Flags = CC_FULLOPEN | CC_RGBINIT;

	NkDialogResult res;
	res.confirmed = (ChooseColorA(&cc) == TRUE);
	COLORREF c = cc.rgbResult;
	res.color = (GetRValue(c) << 16) | (GetGValue(c) << 8) | GetBValue(c) | 0xFF000000; // RGBA
	return res;
}

// ===========================================================================
// Linux (via Zenity)
// ===========================================================================
#elif defined(NKENTSEU_PLATFORM_XLIB) || defined(NKENTSEU_PLATFORM_XCB)

static std::string ExecCommand(const char *cmd) {
	std::string result;
	FILE *pipe = popen(cmd, "r");
	if (!pipe)
		return result;
	char buffer[128];
	while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
		result += buffer;
	}
	pclose(pipe);
	// Enlever le saut de ligne final
	if (!result.empty() && result.back() == '\n')
		result.pop_back();
	return result;
}

inline NkDialogResult NkDialogs::OpenFileDialog(const std::string &filter, const std::string &title) {
	NkDialogResult res;
	// Construction de la commande zenity
	std::string cmd = "zenity --file-selection --title=\"";
	cmd += title;
	cmd += "\"";
	if (!filter.empty() && filter != "*.*") {
		// Zenity accepte --file-filter='Nom *.extension'
		cmd += " --file-filter=\"";
		cmd += filter;
		cmd += "\"";
	}
	std::string path = ExecCommand(cmd.c_str());
	res.confirmed = !path.empty();
	res.path = path;
	return res;
}

inline NkDialogResult NkDialogs::SaveFileDialog(const std::string &defaultExt, const std::string &title) {
	NkDialogResult res;
	std::string cmd = "zenity --file-selection --save --confirm-overwrite --title=\"";
	cmd += title;
	cmd += "\"";
	if (!defaultExt.empty()) {
		cmd += " --file-filter=\"*.";
		cmd += defaultExt;
		cmd += "\"";
	}
	std::string path = ExecCommand(cmd.c_str());
	res.confirmed = !path.empty();
	res.path = path;
	return res;
}

inline void NkDialogs::OpenMessageBox(const std::string &message, const std::string &title, int type) {
	std::string cmd = "zenity --";
	switch (type) {
		case 1:
			cmd += "warning";
			break;
		case 2:
			cmd += "error";
			break;
		default:
			cmd += "info";
			break;
	}
	cmd += " --text=\"";
	cmd += message;
	cmd += "\" --title=\"";
	cmd += title;
	cmd += "\"";
	system(cmd.c_str());
}

inline NkDialogResult NkDialogs::ColorPicker(NkU32 initial) {
	NkDialogResult res;
	// Convertir initial en #RRGGBB pour zenity
	char hex[8];
	snprintf(hex, sizeof(hex), "#%02X%02X%02X", (initial >> 16) & 0xFF, (initial >> 8) & 0xFF, initial & 0xFF);
	std::string cmd = "zenity --color-selection --color=";
	cmd += hex;
	std::string output = ExecCommand(cmd.c_str());
	// Le format de sortie est gÃ©nÃ©ralement #RRGGBB
	if (output.empty() || output[0] != '#') {
		res.confirmed = false;
	} else {
		res.confirmed = true;
		unsigned int r, g, b;
		if (sscanf(output.c_str(), "#%02x%02x%02x", &r, &g, &b) == 3) {
			res.color = (r << 24) | (g << 16) | (b << 8) | 0xFF;
		}
	}
	return res;
}

// ===========================================================================
// macOS (via osascript)
// ===========================================================================
#elif defined(NKENTSEU_PLATFORM_COCOA)

static std::string ExecCommand(const char *cmd) {
	std::string result;
	FILE *pipe = popen(cmd, "r");
	if (!pipe)
		return result;
	char buffer[128];
	while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
		result += buffer;
	}
	pclose(pipe);
	if (!result.empty() && result.back() == '\n')
		result.pop_back();
	return result;
}

// Pour les dialogues de fichiers, on utilise osascript (AppleScript)
inline NkDialogResult NkDialogs::OpenFileDialog(const std::string &filter, const std::string &title) {
	NkDialogResult res;
	// Construction d'un script AppleScript pour choisir un fichier
	std::string script = "osascript -e 'POSIX path of (choose file with prompt \"" + title + "\"";
	if (!filter.empty() && filter != "*.*") {
		// Transformer le filtre "*.png;*.jpg" en liste pour AppleScript
		// AppleScript attend des types comme {"png","jpg"}
		script += " of type {";
		std::string f = filter;
		size_t pos = 0;
		bool first = true;
		while ((pos = f.find(';')) != std::string::npos) {
			std::string ext = f.substr(0, pos);
			if (!ext.empty()) {
				if (!first)
					script += ",";
				// enlever le *.
				if (ext.size() > 2 && ext[0] == '*' && ext[1] == '.')
					ext = ext.substr(2);
				script += "\"" + ext + "\"";
				first = false;
			}
			f.erase(0, pos + 1);
		}
		if (!f.empty()) {
			if (!first)
				script += ",";
			if (f.size() > 2 && f[0] == '*' && f[1] == '.')
				f = f.substr(2);
			script += "\"" + f + "\"";
		}
		script += "}";
	}
	script += ")'";
	std::string path = ExecCommand(script.c_str());
	res.confirmed = !path.empty();
	res.path = path;
	return res;
}

inline NkDialogResult NkDialogs::SaveFileDialog(const std::string &defaultExt, const std::string &title) {
	NkDialogResult res;
	std::string script = "osascript -e 'POSIX path of (choose file name with prompt \"" + title + "\"";
	if (!defaultExt.empty()) {
		script += " default name \"untitled." + defaultExt + "\"";
	}
	script += ")'";
	std::string path = ExecCommand(script.c_str());
	res.confirmed = !path.empty();
	res.path = path;
	return res;
}

inline void NkDialogs::OpenMessageBox(const std::string &message, const std::string &title, int type) {
	std::string script = "osascript -e 'display dialog \"" + message + "\" with title \"" + title + "\"";
	switch (type) {
		case 1:
			script += " with icon caution";
			break;
		case 2:
			script += " with icon stop";
			break;
		default:
			script += " with icon note";
			break;
	}
	script += "'";
	system(script.c_str());
}

inline NkDialogResult NkDialogs::ColorPicker(NkU32 initial) {
	// Pas de color picker simple en ligne de commande sur macOS, on peut utiliser un script plus complexe.
	// Pour simplifier, on renvoie un stub.
	(void)initial;
	return {};
}

// ===========================================================================
// Autres plateformes (UWP, Xbox, Android, iOS, WASM) : stubs
// ===========================================================================
#else

inline NkDialogResult NkDialogs::OpenFileDialog(const std::string &, const std::string &) {
	return {};
}
inline NkDialogResult NkDialogs::SaveFileDialog(const std::string &, const std::string &) {
	return {};
}
inline void NkDialogs::OpenMessageBox(const std::string &, const std::string &, int) {
}
inline NkDialogResult NkDialogs::ColorPicker(NkU32) {
	return {};
}

#endif

} // namespace nkentseu
