/*
 * NkUIFileBrowser_v3.cpp
 * FileBrowser v3 + ContentBrowser — implémentation complète.
 */
#include "NkUIFileBrowser.h"
#include <cstring>
#include <cstdio>
#include <cctype>
#include <cmath>
#include <ctime>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <shlobj.h>
#else
#  include <dirent.h>
#  include <sys/stat.h>
#  include <unistd.h>
#  include <pwd.h>
#endif

namespace nkentseu {
    namespace nkui {

        // =============================================================================
        //  Unicode UTF-8
        // =============================================================================

        /// Encode un codepoint en UTF-8. Retourne le nombre d'octets écrits.
        static NKUI_INLINE int32 Utf8Encode(uint32 cp, char* out) noexcept {
            if (cp < 0x80u)  { out[0]=(char)cp; return 1; }
            if (cp < 0x800u) { out[0]=(char)(0xC0u|(cp>>6)); out[1]=(char)(0x80u|(cp&0x3Fu)); return 2; }
            if (cp < 0x10000u){ out[0]=(char)(0xE0u|(cp>>12)); out[1]=(char)(0x80u|((cp>>6)&0x3Fu));
                                out[2]=(char)(0x80u|(cp&0x3Fu)); return 3; }
            out[0]=(char)(0xF0u|(cp>>18)); out[1]=(char)(0x80u|((cp>>12)&0x3Fu));
            out[2]=(char)(0x80u|((cp>>6)&0x3Fu)); out[3]=(char)(0x80u|(cp&0x3Fu)); return 4;
        }

        /// Supprime le dernier codepoint UTF-8 d'une chaîne (in-place). Retourne la nouvelle longueur.
        static NKUI_INLINE int32 Utf8BackSpace(char* s, int32 len) noexcept {
            if (len <= 0) return 0;
            int32 i = len - 1;
            // Reculer sur les octets de continuation (10xxxxxx)
            while (i > 0 && (static_cast<uint8>(s[i]) & 0xC0u) == 0x80u) --i;
            s[i] = '\0';
            return i;
        }

        /// Append d'un codepoint dans un buffer (retourne nouvelle longueur ou -1 si plein).
        static NKUI_INLINE int32 Utf8Append(char* buf, int32 len, int32 maxLen, uint32 cp) noexcept {
            char enc[4]; const int32 nb = Utf8Encode(cp, enc);
            if (len + nb >= maxLen) return -1;
            ::memcpy(buf + len, enc, static_cast<size_t>(nb));
            len += nb; buf[len] = '\0';
            return len;
        }

        /// Contient (case-insensitive ASCII + compare byte par byte pour unicode).
        static bool Utf8ContainsI(const char* hay, const char* needle) noexcept {
            if (!needle || !needle[0]) return true;
            if (!hay) return false;
            const int32 nl = static_cast<int32>(::strlen(needle));
            for (int32 i = 0; hay[i]; ++i) {
                bool m = true;
                for (int32 j = 0; j < nl && m; ++j)
                    m = (::tolower((uint8)hay[i+j]) == ::tolower((uint8)needle[j]));
                if (m) return true;
            }
            return false;
        }

        // =============================================================================
        //  Constantes internes
        // =============================================================================
        static constexpr float32 kIconCol  = 22.f;
        static constexpr float32 kSBW      = 10.f;
        static constexpr float32 kBtnR     = 4.f;
        static constexpr float32 kBtnGap   = 3.f;

        // =============================================================================
        //  NkUIFSProvider — NativeProvider
        // =============================================================================

        NkUIFSProvider NkUIFSProvider::NativeProvider() noexcept {
            NkUIFSProvider p;

            p.list = [](const char* path, NkUIFileEntry* out, int32 maxE, void*) -> int32 {
        #if defined(_WIN32)
                if (!path || !path[0] || ::strcmp(path,"Computer")==0) return 0;
                char pat[520]; ::snprintf(pat,sizeof(pat),"%s\\*",path);
                WIN32_FIND_DATAA fd;
                HANDLE h = ::FindFirstFileA(pat, &fd);
                if (h == INVALID_HANDLE_VALUE) return 0;
                int32 n = 0;
                do {
                    if (n >= maxE) break;
                    if (!::strcmp(fd.cFileName,".")||!::strcmp(fd.cFileName,"..")) continue;
                    NkUIFileEntry& e = out[n++];
                    ::strncpy(e.name, fd.cFileName, 255);
                    ::snprintf(e.path, 511, "%s\\%s", path, fd.cFileName);
                    e.type = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                        ? NkUIFileType::NK_FT_DIRECTORY : NkUIFileType::NK_FT_FILE;
                    e.isHidden   = (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)   != 0;
                    e.isReadOnly = (fd.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0;
                    ULARGE_INTEGER sz; sz.LowPart=fd.nFileSizeLow; sz.HighPart=fd.nFileSizeHigh;
                    e.size = sz.QuadPart;
                    ULARGE_INTEGER mt; mt.LowPart=fd.ftLastWriteTime.dwLowDateTime;
                    mt.HighPart=fd.ftLastWriteTime.dwHighDateTime;
                    e.modifiedTime = (mt.QuadPart > 116444736000000000ULL)
                                ? (mt.QuadPart - 116444736000000000ULL)/10000000ULL : 0;
                } while (::FindNextFileA(h,&fd));
                ::FindClose(h); return n;
        #else
                if (!path || !path[0]) path = "/";
                DIR* dir = ::opendir(path); if (!dir) return 0;
                int32 n = 0; struct dirent* de;
                while ((de=::readdir(dir)) && n < maxE) {
                    if (!::strcmp(de->d_name,".")||!::strcmp(de->d_name,"..")) continue;
                    NkUIFileEntry& e = out[n++];
                    ::strncpy(e.name, de->d_name, 255);
                    ::snprintf(e.path, 511, "%s/%s", path, de->d_name);
                    struct stat st{}; ::stat(e.path, &st);
                    e.type = S_ISDIR(st.st_mode) ? NkUIFileType::NK_FT_DIRECTORY
                        : S_ISLNK(st.st_mode) ? NkUIFileType::NK_FT_SYMLINK
                                                : NkUIFileType::NK_FT_FILE;
                    e.size         = static_cast<uint64>(st.st_size);
                    e.modifiedTime = static_cast<uint64>(st.st_mtime);
                    e.isHidden     = (de->d_name[0] == '.');
                }
                ::closedir(dir); return n;
        #endif
            };

            p.stat = [](const char* path, NkUIFileEntry* out, void*) -> bool {
                if (!path || !out) return false;
        #if defined(_WIN32)
                WIN32_FIND_DATAA fd; HANDLE h=::FindFirstFileA(path,&fd);
                if (h==INVALID_HANDLE_VALUE) return false; ::FindClose(h);
                ::strncpy(out->name,fd.cFileName,255); ::strncpy(out->path,path,511);
                out->type=(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
                    ?NkUIFileType::NK_FT_DIRECTORY:NkUIFileType::NK_FT_FILE;
                ULARGE_INTEGER sz; sz.LowPart=fd.nFileSizeLow; sz.HighPart=fd.nFileSizeHigh;
                out->size=sz.QuadPart; return true;
        #else
                struct stat st{}; if (::stat(path,&st)!=0) return false;
                out->type=S_ISDIR(st.st_mode)?NkUIFileType::NK_FT_DIRECTORY:NkUIFileType::NK_FT_FILE;
                out->size=static_cast<uint64>(st.st_size);
                out->modifiedTime=static_cast<uint64>(st.st_mtime);
                const char* sl=::strrchr(path,'/');
                ::strncpy(out->name,sl?sl+1:path,255);
                ::strncpy(out->path,path,511); return true;
        #endif
            };

            p.mkdir_ = [](const char* p2,void*)->bool {
        #if defined(_WIN32)
                return ::CreateDirectoryA(p2,nullptr)!=0;
        #else
                return ::mkdir(p2,0755)==0;
        #endif
            };

            p.move = [](const char* s,const char* d,void*)->bool {
        #if defined(_WIN32)
                return ::MoveFileA(s,d)!=0;
        #else
                return ::rename(s,d)==0;
        #endif
            };

            p.delete_ = [](const char* p2,void*)->bool {
        #if defined(_WIN32)
                DWORD a=::GetFileAttributesA(p2);
                if (a==INVALID_FILE_ATTRIBUTES) return false;
                return (a & FILE_ATTRIBUTE_DIRECTORY) ? ::RemoveDirectoryA(p2) !=0 : ::DeleteFileA(p2)!=0;
        #else
                return ::remove(p2)==0;
        #endif
            };

            p.read = [](const char* p2,void* buf,uint64 mx,void*)->int64 {
                FILE* f=::fopen(p2,"rb"); if (!f) return -1;
                int64 n=static_cast<int64>(::fread(buf,1,static_cast<size_t>(mx),f));
                ::fclose(f); return n;
            };

            p.write = [](const char* p2,const void* data,uint64 bytes,void*)->bool {
                FILE* f=::fopen(p2,"wb"); if (!f) return false;
                bool ok=(::fwrite(data,1,static_cast<size_t>(bytes),f)==static_cast<size_t>(bytes));
                ::fclose(f); return ok;
            };

            return p;
        }
        NkUIFSProvider NkUIFSProvider::Null() noexcept { return {}; }

        // =============================================================================
        //  Helpers texte / tri
        // =============================================================================

        void NkUIFileBrowser::FormatSize(uint64 b,char* out,int32 sz) noexcept {
            if      (b<1024ull)            ::snprintf(out,sz,"%u B",(uint32)b);
            else if (b<1024ull*1024)       ::snprintf(out,sz,"%.1f Ko",b/1024.);
            else if (b<1024ull*1024*1024)  ::snprintf(out,sz,"%.1f Mo",b/(1024.*1024));
            else                           ::snprintf(out,sz,"%.2f Go",b/(1024.*1024*1024));
        }

        void NkUIFileBrowser::FormatDate(uint64 ts,char* out,int32 sz) noexcept {
            if (!ts){ ::snprintf(out,sz,"\xe2\x80\x94"); return; }
            time_t t=static_cast<time_t>(ts);
            struct tm* tm_=::localtime(&t);
            if (!tm_){ ::snprintf(out,sz,"?"); return; }
            ::strftime(out,static_cast<size_t>(sz),"%d/%m/%Y %H:%M",tm_);
        }

        bool NkUIFileBrowser::ExtMatchesFilter(const char* ext,const char* filter) noexcept {
            if (!filter||!filter[0]) return true;
            if (!ext||!ext[0]) return false;
            char buf[128]; ::strncpy(buf,filter,127); buf[127]='\0';
            for (char* t=buf;;) {
                char* e=t; while (*e&&*e!=';') ++e;
                int32 len=static_cast<int32>(e-t);
                if (len>0) {
                    bool ok=(static_cast<int32>(::strlen(ext))==len);
                    for (int32 i=0;ok&&i<len;++i)
                        ok=(::tolower((uint8)ext[i])==::tolower((uint8)t[i]));
                    if (ok) return true;
                }
                if (!*e) break; t=e+1;
            }
            return false;
        }

        bool NkUIFileBrowser::MatchesFilter(const NkUIFileEntry& e,const char* f) noexcept {
            if (!f||!f[0]) return true;
            if (e.type==NkUIFileType::NK_FT_DIRECTORY) return true;
            const char* dot=::strrchr(e.name,'.');
            return dot?ExtMatchesFilter(dot,f):false;
        }

        void NkUIFileBrowser::PushHistory(NkUIFileBrowserState& s,const char* path) noexcept {
            if (s.historyPos<s.historyDepth) s.historyDepth=s.historyPos;
            if (s.historyDepth<NkUIFileBrowserState::MAX_HIST_DEPTH)
                ::strncpy(s.history[s.historyDepth++],path,511);
            s.historyPos=s.historyDepth;
        }

        void NkUIFileBrowser::SortEntries(NkUIFileBrowserState& s,
                                        NkUIFBSortCol col,bool asc) noexcept {
            const int32 n=s.numEntries;
            for (int32 i=0;i<n-1;++i) for (int32 j=0;j<n-1-i;++j) {
                NkUIFileEntry& a=s.entries[j]; NkUIFileEntry& b=s.entries[j+1];
                const bool ad=(a.type==NkUIFileType::NK_FT_DIRECTORY);
                const bool bd=(b.type==NkUIFileType::NK_FT_DIRECTORY);
                int32 cmp=0;
                if (ad!=bd) cmp=ad?-1:1;
                else {
                    switch(col) {
                        case NkUIFBSortCol::NK_FBS_SIZE:
                            cmp=(a.size<b.size)?-1:(a.size>b.size?1:0); break;
                        case NkUIFBSortCol::NK_FBS_DATE:
                            cmp=(a.modifiedTime<b.modifiedTime)?-1:(a.modifiedTime>b.modifiedTime?1:0); break;
                        case NkUIFBSortCol::NK_FBS_TYPE: {
                            const char* da=::strrchr(a.name,'.'), *db=::strrchr(b.name,'.');
                            if (!da&&!db) cmp=0; else if(!da) cmp=1; else if(!db) cmp=-1;
                            else cmp=::strcasecmp(da,db); } break;
                        default: cmp=::strcasecmp(a.name,b.name); break;
                    }
                    if (!asc) cmp=-cmp;
                }
                if (cmp>0){ NkUIFileEntry tmp=a; a=b; b=tmp; }
            }
        }

        // =============================================================================
        //  Navigate / Refresh / Bookmarks
        // =============================================================================

        void NkUIFileBrowser::Navigate(NkUIFileBrowserState& s,const char* path) noexcept {
            if (!path||!path[0]) return;
            if (s.currentPath[0]) PushHistory(s,s.currentPath);
            ::strncpy(s.currentPath,path,511);
            s.needsRefresh=true; s.numSelected=0;
            s.scrollY=0.f; s.gridScrollY=0.f;
            s.renaming=false; s.creatingDir=false; s.creatingFile=false;
            s.searchBuffer[0]='\0';
        }

        void NkUIFileBrowser::Refresh(NkUIFileBrowserState& s,
                                    const NkUIFSProvider& p,
                                    const NkUIFileBrowserConfig& cfg) noexcept {
        #if defined(_WIN32)
            if (!s.currentPath[0]||::strcmp(s.currentPath,"Computer")==0) {
                DWORD drives=::GetLogicalDrives(); int32 n=0;
                for (char d='A';d<='Z'&&n<NkUIFileBrowserState::MAX_ENTRIES;++d) {
                    if (!(drives&(1<<(d-'A')))) continue;
                    NkUIFileEntry& e=s.entries[n++];
                    ::snprintf(e.name,sizeof(e.name),"%c:",d);
                    ::snprintf(e.path,sizeof(e.path),"%c:\\",d);
                    e.type=NkUIFileType::NK_FT_DIRECTORY; e.size=0; e.modifiedTime=0;
                }
                s.numEntries=n; s.needsRefresh=false; return;
            }
        #endif
            if (!p.list){ s.numEntries=0; return; }
            int32 n=p.list(s.currentPath,s.entries,NkUIFileBrowserState::MAX_ENTRIES,p.userData);
            s.numEntries=(n>0)?n:0;
            SortEntries(s,s.sortCol,s.sortAscending);
            s.needsRefresh=false; (void)cfg;
        }

        const char* NkUIFileBrowser::GetSelectedPath(const NkUIFileBrowserState& s) noexcept {
            if (s.numSelected<=0) return "";
            const int32 i=s.selected[0];
            return (i>=0&&i<s.numEntries)?s.entries[i].path:"";
        }

        void NkUIFileBrowser::AddBookmark(NkUIFileBrowserConfig& cfg,
                                        const char* label,const char* path,bool special) noexcept {
            if (cfg.numBookmarks>=NkUIFileBrowserConfig::MAX_BOOKMARKS) return;
            auto& bk=cfg.bookmarks[cfg.numBookmarks++];
            ::strncpy(bk.label,label,63); ::strncpy(bk.path,path,511); bk.isSpecial=special;
        }

        void NkUIFileBrowser::RemoveBookmark(NkUIFileBrowserConfig& cfg,int32 idx) noexcept {
            if (idx<0||idx>=cfg.numBookmarks) return;
            for (int32 i=idx;i<cfg.numBookmarks-1;++i) cfg.bookmarks[i]=cfg.bookmarks[i+1];
            --cfg.numBookmarks;
        }

        void NkUIFileBrowser::AddDefaultBookmarks(NkUIFileBrowserConfig& cfg) noexcept {
        #if defined(_WIN32)
            char desk[MAX_PATH]={},docs[MAX_PATH]={},pics[MAX_PATH]={};
            if (::SHGetSpecialFolderPathA(nullptr,desk,CSIDL_DESKTOPDIRECTORY,FALSE))
                AddBookmark(cfg,"Bureau",desk,true);
            if (::SHGetSpecialFolderPathA(nullptr,docs,CSIDL_PERSONAL,FALSE))
                AddBookmark(cfg,"Documents",docs,true);
            if (::SHGetSpecialFolderPathA(nullptr,pics,CSIDL_MYPICTURES,FALSE))
                AddBookmark(cfg,"Images",pics,true);
            const char* home=::getenv("USERPROFILE");
            if (home){ char dl[MAX_PATH]; ::snprintf(dl,MAX_PATH,"%s\\Downloads",home);
                    AddBookmark(cfg,"Téléchargements",dl,true); }
            AddBookmark(cfg,"Ordinateur","Computer",true);
        #else
            const char* home=::getenv("HOME");
            if (!home){ struct passwd* pw=::getpwuid(::getuid()); home=pw?pw->pw_dir:"/"; }
            char p2[512];
            AddBookmark(cfg,"Accueil",home,true);
            ::snprintf(p2,512,"%s/Bureau",home);       AddBookmark(cfg,"Bureau",p2,true);
            ::snprintf(p2,512,"%s/Documents",home);    AddBookmark(cfg,"Documents",p2,true);
            ::snprintf(p2,512,"%s/Images",home);       AddBookmark(cfg,"Images",p2,true);
            // Note: "Téléchargements" en UTF-8 natif
            // ::snprintf(p2,512,"%s/T\xc3\xa9l\xc3\xa9chargements",home);
            // AddBookmark(cfg,"T\xc3\xa9l\xc3\xa9chargements",p2,true);
            ::snprintf(p2, 512, "%s/T\303\251l\303\251chargements", home);
            AddBookmark(cfg, "T\303\251l\303\251chargements", p2, true);
            AddBookmark(cfg,"/","/",true);
        #endif
        }

        // =============================================================================
        //  Couleurs par extension
        // =============================================================================

        NkColor NkUIFileBrowser::IconColorForEntry(const NkUIFileEntry& e) noexcept {
            if (e.type==NkUIFileType::NK_FT_DIRECTORY) return {255,200,50,255};
            if (e.type==NkUIFileType::NK_FT_SYMLINK)   return {180,120,255,255};
            const char* dot=::strrchr(e.name,'.');
            return dot?ExtensionAccentColor(dot+1):NkColor{160,160,160,255};
        }

        NkColor NkUIFileBrowser::ExtensionAccentColor(const char* x) noexcept {
            if (!x) return {160,160,160,255};
            // Images
            if (!::strcasecmp(x,"png")||!::strcasecmp(x,"jpg")||!::strcasecmp(x,"jpeg")||
                !::strcasecmp(x,"bmp")||!::strcasecmp(x,"tga")||!::strcasecmp(x,"dds")||
                !::strcasecmp(x,"gif")||!::strcasecmp(x,"webp")||!::strcasecmp(x,"svg")||
                !::strcasecmp(x,"psd")||!::strcasecmp(x,"hdr")||!::strcasecmp(x,"exr"))
                return {70,170,255,255};
            // Code/Shaders
            if (!::strcasecmp(x,"cpp")||!::strcasecmp(x,"h")||!::strcasecmp(x,"c")||
                !::strcasecmp(x,"hpp")||!::strcasecmp(x,"cs")||!::strcasecmp(x,"py")||
                !::strcasecmp(x,"js")||!::strcasecmp(x,"ts")||!::strcasecmp(x,"lua")||
                !::strcasecmp(x,"glsl")||!::strcasecmp(x,"hlsl")||!::strcasecmp(x,"wgsl")||
                !::strcasecmp(x,"vert")||!::strcasecmp(x,"frag")||!::strcasecmp(x,"comp"))
                return {100,230,140,255};
            // Data / Config
            if (!::strcasecmp(x,"json")||!::strcasecmp(x,"yaml")||!::strcasecmp(x,"xml")||
                !::strcasecmp(x,"toml")||!::strcasecmp(x,"csv")||!::strcasecmp(x,"ini"))
                return {255,215,80,255};
            // Texte
            if (!::strcasecmp(x,"txt")||!::strcasecmp(x,"md")||!::strcasecmp(x,"log"))
                return {200,200,185,255};
            // Archives
            if (!::strcasecmp(x,"zip")||!::strcasecmp(x,"tar")||!::strcasecmp(x,"gz")||
                !::strcasecmp(x,"rar")||!::strcasecmp(x,"7z")||!::strcasecmp(x,"xz"))
                return {255,140,60,255};
            // Exécutables
            if (!::strcasecmp(x,"exe")||!::strcasecmp(x,"dll")||!::strcasecmp(x,"so")||
                !::strcasecmp(x,"bin")||!::strcasecmp(x,"app")||!::strcasecmp(x,"elf"))
                return {255,80,80,255};
            // Médias
            if (!::strcasecmp(x,"mp4")||!::strcasecmp(x,"mkv")||!::strcasecmp(x,"avi")||
                !::strcasecmp(x,"mp3")||!::strcasecmp(x,"wav")||!::strcasecmp(x,"flac")||
                !::strcasecmp(x,"ogg")||!::strcasecmp(x,"m4a"))
                return {200,90,255,255};
            // 3D
            if (!::strcasecmp(x,"obj")||!::strcasecmp(x,"fbx")||!::strcasecmp(x,"gltf")||
                !::strcasecmp(x,"glb")||!::strcasecmp(x,"blend")||!::strcasecmp(x,"mesh")||
                !::strcasecmp(x,"dae")||!::strcasecmp(x,"3ds"))
                return {255,175,80,255};
            // Fonts
            if (!::strcasecmp(x,"ttf")||!::strcasecmp(x,"otf")||!::strcasecmp(x,"woff")||
                !::strcasecmp(x,"woff2"))
                return {140,200,255,255};
            return {160,160,160,255};
        }

        // =============================================================================
        //  Icônes dessinées en primitives
        // =============================================================================

        void NkUIFileBrowser::DrawIconFolder(NkUIDrawList& dl,NkRect r,NkColor col,bool open) noexcept {
            const float32 cx=r.x+r.w*0.5f, cy=r.y+r.h*0.5f;
            const float32 bw=r.w*0.76f, bh=r.h*0.56f;
            const float32 bx=cx-bw*0.5f, by=cy-bh*0.28f;
            // Corps
            dl.AddRectFilled({bx,by,bw,bh},col,2.5f);
            // Onglet
            dl.AddRectFilled({bx,by-bh*0.32f,bw*0.50f,bh*0.35f},col,2.f);
            // Jointure
            dl.AddRectFilled({bx,by-1.f,bw*0.52f,2.f},col);
            // Ombre intérieure (légèreté)
            const NkColor lighter{(uint8)::fminf(col.r+40,255.f),(uint8)::fminf(col.g+40,255.f),
                                (uint8)::fminf(col.b+40,255.f),static_cast<uint8>(col.a/5)};
            dl.AddRectFilled({bx+2.f,by+2.f,bw-4.f,bh*0.38f},lighter,2.f);
            if (open) { // ouvert : ligne de séparation
                dl.AddLine({bx+2.f,by+bh*0.5f},{bx+bw-2.f,by+bh*0.5f},
                        col.WithAlpha(120),1.f);
            }
        }

        void NkUIFileBrowser::DrawIconFile(NkUIDrawList& dl,NkRect r,NkColor col) noexcept {
            const float32 cx=r.x+r.w*0.5f, cy=r.y+r.h*0.5f;
            const float32 fw=r.w*0.60f, fh=r.h*0.74f;
            const float32 fx=cx-fw*0.5f, fy=cy-fh*0.5f;
            const float32 fold=fw*0.32f;
            // Corps
            dl.AddRectFilled({fx,fy,fw-fold,fh},col,1.f);
            dl.AddRectFilled({fx+fw-fold,fy+fold,fold,fh-fold},col,1.f);
            // Coin plié blanc
            dl.AddTriangleFilled({fx+fw-fold,fy},{fx+fw,fy+fold},{fx+fw-fold,fy+fold},
                                {255,255,255,110});
            // Lignes "texte" intérieures
            const NkColor lc=col.WithAlpha(70);
            for (int32 i=0;i<3;++i) {
                const float32 ly=fy+fh*(0.40f+i*0.17f);
                const float32 lw=(i<2)?fw*0.58f:fw*0.36f;
                dl.AddLine({fx+fw*0.14f,ly},{fx+fw*0.14f+lw,ly},lc,1.f);
            }
        }

        void NkUIFileBrowser::DrawIconSymlink(NkUIDrawList& dl,NkRect r,NkColor col) noexcept {
            // Fichier semi-transparent + flèche de lien
            NkRect fr=r; fr.w*=0.82f; fr.h*=0.82f; fr.x+=r.w*0.04f; fr.y+=r.h*0.04f;
            DrawIconFile(dl,fr,col.WithAlpha(130));
            const float32 ax=r.x+r.w*0.52f,ay=r.y+r.h*0.52f,sz=r.w*0.40f;
            dl.AddCircleFilled({ax+sz*0.5f,ay+sz*0.5f},sz*0.55f,{50,50,60,200},8);
            dl.AddLine({ax+sz*0.2f,ay+sz*0.8f},{ax+sz*0.8f,ay+sz*0.2f},
                    {col.r,col.g,col.b,230},1.5f);
            dl.AddArrow({ax+sz*0.8f,ay+sz*0.2f},sz*0.35f,1,{col.r,col.g,col.b,230});
        }

        void NkUIFileBrowser::DrawIconTrash(NkUIDrawList& dl,NkRect r,NkColor col,bool full) noexcept {
            const float32 cx=r.x+r.w*0.5f, cy=r.y+r.h*0.5f;
            const float32 bw=r.w*0.52f, bh=r.h*0.52f;
            const float32 bx=cx-bw*0.5f, by=cy-bh*0.2f;
            // Corps cylindrique (rectangle arrondi bas)
            dl.AddRectFilled({bx,by,bw,bh},col,2.f);
            // Couvercle
            dl.AddRect({bx-bw*0.10f,by-bh*0.20f,bw*1.20f,bh*0.22f},col,1.5f,2.f);
            // Poignée couvercle
            const float32 px=cx-bw*0.18f, py=by-bh*0.36f;
            dl.AddRectFilled({px,py,bw*0.36f,bh*0.19f},{col.r,col.g,col.b,(uint8)(col.a*0.8f)},2.f);
            // Rayures verticales (poubelle pleine = contenu)
            if (full) {
                for (int32 i=0;i<3;++i) {
                    const float32 lx=bx+bw*(0.25f+i*0.25f);
                    dl.AddLine({lx,by+bh*0.16f},{lx,by+bh*0.86f},{255,255,255,90},1.2f);
                }
                // Petit cercle rouge "alerte"
                dl.AddCircleFilled({bx+bw*0.82f,by-bh*0.12f},bw*0.14f,{255,50,50,240},6);
                dl.AddLine({bx+bw*0.82f,by-bh*0.20f},{bx+bw*0.82f,by-bh*0.06f},{255,255,255,200},1.2f);
            }
        }

        void NkUIFileBrowser::DrawIconNewFolder(NkUIDrawList& dl,NkRect r,NkColor col) noexcept {
            // Dossier décalé + badge +
            NkRect fr={r.x,r.y+r.h*0.10f,r.w*0.80f,r.h*0.82f};
            DrawIconFolder(dl,fr,col.WithAlpha(180));
            // Badge vert +
            const float32 bx=r.x+r.w*0.60f, by=r.y+r.h*0.50f, bs=r.w*0.38f;
            const float32 bcx=bx+bs*0.5f, bcy=by+bs*0.5f;
            dl.AddCircleFilled({bcx,bcy},bs*0.54f,{45,185,100,245},10);
            dl.AddCircle({bcx,bcy},bs*0.54f,{255,255,255,60},1.f,10);
            const float32 arm=bs*0.28f;
            dl.AddLine({bcx-arm,bcy},{bcx+arm,bcy},{255,255,255,235},2.f);
            dl.AddLine({bcx,bcy-arm},{bcx,bcy+arm},{255,255,255,235},2.f);
        }

        void NkUIFileBrowser::DrawIconNewFile(NkUIDrawList& dl,NkRect r,NkColor col) noexcept {
            NkRect fr={r.x,r.y+r.h*0.10f,r.w*0.80f,r.h*0.82f};
            DrawIconFile(dl,fr,col.WithAlpha(180));
            const float32 bx=r.x+r.w*0.60f, by=r.y+r.h*0.50f, bs=r.w*0.38f;
            const float32 bcx=bx+bs*0.5f, bcy=by+bs*0.5f;
            dl.AddCircleFilled({bcx,bcy},bs*0.54f,{55,140,255,245},10);
            dl.AddCircle({bcx,bcy},bs*0.54f,{255,255,255,60},1.f,10);
            const float32 arm=bs*0.28f;
            dl.AddLine({bcx-arm,bcy},{bcx+arm,bcy},{255,255,255,235},2.f);
            dl.AddLine({bcx,bcy-arm},{bcx,bcy+arm},{255,255,255,235},2.f);
        }

        void NkUIFileBrowser::DrawIconRefresh(NkUIDrawList& dl,NkRect r,NkColor col) noexcept {
            const float32 cx=r.x+r.w*0.5f, cy=r.y+r.h*0.5f, rad=r.h*0.33f;
            const float32 a0=NKUI_PI*0.20f, a1=NKUI_PI*1.80f;
            dl.AddArc({cx,cy},rad,a0,a1,col,1.8f,14);
            // Pointe de flèche à la fin de l'arc
            const float32 tx=-::sinf(a1), ty=::cosf(a1);
            const float32 ex=cx+rad*::cosf(a1), ey=cy+rad*::sinf(a1);
            dl.AddTriangleFilled({ex,ey},{ex-tx*rad*0.38f+ty*rad*0.20f,ey-ty*rad*0.38f-tx*rad*0.20f},
                                {ex-tx*rad*0.38f-ty*rad*0.20f,ey-ty*rad*0.38f+tx*rad*0.20f},col);
        }

        void NkUIFileBrowser::DrawIconList(NkUIDrawList& dl,NkRect r,NkColor col) noexcept {
            for (int32 i=0;i<4;++i) {
                const float32 y=r.y+r.h*(0.14f+i*0.22f);
                // Point bullet
                dl.AddCircleFilled({r.x+r.w*0.14f,y+r.h*0.07f},r.w*0.07f,col,5);
                // Ligne texte
                dl.AddRectFilled({r.x+r.w*0.26f,y,r.w*0.64f,r.h*0.10f},col,1.f);
            }
        }

        void NkUIFileBrowser::DrawIconGrid(NkUIDrawList& dl,NkRect r,NkColor col) noexcept {
            const float32 pad=r.w*0.10f, gs=(r.w-pad*3)*0.5f;
            for (int32 row=0;row<2;++row) for (int32 c=0;c<2;++c) {
                const float32 x=r.x+pad+c*(gs+pad), y=r.y+pad+row*(gs+pad);
                dl.AddRectFilled({x,y,gs,gs},col,2.f);
            }
        }

        void NkUIFileBrowser::DrawIconSearch(NkUIDrawList& dl,NkRect r,NkColor col) noexcept {
            const float32 cx=r.x+r.w*0.40f,cy=r.y+r.h*0.40f,rad=r.h*0.28f;
            dl.AddCircle({cx,cy},rad,col,1.8f,10);
            const float32 a=NKUI_PI * 3;
            dl.AddLine({cx+rad*::cosf(a),cy+rad*::sinf(a)},
                    {r.x+r.w*0.86f,r.y+r.h*0.86f},col,2.f);
        }

        void NkUIFileBrowser::DrawEntryIcon(NkUIDrawList& dl,NkRect r,
                                            const NkUIFileEntry& e,float32) noexcept {
            const NkColor col=IconColorForEntry(e);
            if      (e.type==NkUIFileType::NK_FT_DIRECTORY) DrawIconFolder(dl,r,col);
            else if (e.type==NkUIFileType::NK_FT_SYMLINK)   DrawIconSymlink(dl,r,col);
            else                                              DrawIconFile(dl,r,col);
        }

        // Miniature enrichie pour le mode grille
        void NkUIFileBrowser::DrawFileThumbnail(NkUIDrawList& dl,NkRect r,
                                                const NkUIFileEntry& e) noexcept {
            const NkColor col=IconColorForEntry(e);
            const NkColor bg=col.WithAlpha(35);
            dl.AddRectFilled(r,bg,4.f);
            dl.AddRect(r,col.WithAlpha(70),1.f,4.f);
            // Grande icône centrée
            const float32 isz=(r.w<r.h?r.w:r.h)*0.52f;
            const NkRect ir={r.x+(r.w-isz)*0.5f,r.y+(r.h-isz)*0.5f-isz*0.08f,isz,isz};
            DrawEntryIcon(dl,ir,e);
            // Badge extension en bas à droite
            const char* dot=::strrchr(e.name,'.');
            if (dot&&e.type!=NkUIFileType::NK_FT_DIRECTORY) {
                const float32 bw=r.w*0.52f, bh=12.f;
                const NkRect badge={r.x+(r.w-bw)*0.5f,r.y+r.h-bh-3.f,bw,bh};
                dl.AddRectFilled(badge,col.WithAlpha(130),3.f);
            }
        }
 
        // =============================================================================
        //  Layout
        // =============================================================================
        
        NkUIFileBrowser::Layout NkUIFileBrowser::ComputeLayout(
            NkRect rect, const NkUIFileBrowserConfig& cfg) noexcept
        {
            Layout ly;
            ly.hasBookmarks = cfg.showBookmarks && cfg.numBookmarks > 0;
            ly.hasSave      = (cfg.mode == NkUIFBMode::NK_FBM_SAVE);
        
            const float32 tbH   = cfg.toolbarHeight;
            const float32 bcH   = cfg.breadcrumbHeight;
            const float32 sbH   = cfg.statusBarHeight;
            const float32 confH = cfg.showConfirmButtons ? (cfg.rowHeight + 10.f) : 0.f;
            const float32 saveH = ly.hasSave ? (cfg.rowHeight + 10.f) : 0.f;
            const float32 hdrH  = (cfg.viewMode == NkUIFBViewMode::NK_FBVM_LIST) ? cfg.rowHeight : 0.f;
            const float32 bkW   = ly.hasBookmarks ? cfg.bookmarkPanelW : 0.f;
        
            ly.toolbar    = {rect.x, rect.y, rect.w, tbH};
            ly.breadcrumb = {rect.x, rect.y+tbH, rect.w, bcH};
        
            const float32 contentY = rect.y + tbH + bcH;
            const float32 contentH = rect.h - tbH - bcH - sbH - saveH - confH;
        
            ly.bookmarkPanel = {rect.x, contentY, bkW, contentH};
            ly.header        = {rect.x+bkW, contentY, rect.w-bkW, hdrH};
            ly.fileList      = {rect.x+bkW, contentY+hdrH, rect.w-bkW, contentH-hdrH};
        
            const float32 bottomY = rect.y + rect.h;
            ly.statusBar = {rect.x, bottomY - sbH - confH, rect.w, sbH};
            if (ly.hasSave)
                ly.saveRow = {rect.x, bottomY - sbH - confH - saveH, rect.w, saveH};
            return ly;
        }
        
        // =============================================================================
        //  DrawBackground
        // =============================================================================
        
        void NkUIFileBrowser::DrawBackground(NkUIDrawList& dl,NkRect r,
                                            const NkUIContext& ctx) noexcept {
            dl.AddRectFilled(r, ctx.theme.colors.bgWindow, 6.f);
            dl.AddRect(r, ctx.theme.colors.border, 1.f, 6.f);
        }
        
        // =============================================================================
        //  DrawToolbar
        // =============================================================================
        
        void NkUIFileBrowser::DrawToolbar(NkUIContext& ctx, NkUIDrawList& dl,
                                        NkUIFont& font,
                                        const Layout& ly,
                                        NkUIFileBrowserConfig& cfg,
                                        NkUIFileBrowserState& state,
                                        const NkUIFSProvider& prov) noexcept
        {
            const NkRect& r = ly.toolbar;
            dl.AddRectFilled(r, ctx.theme.colors.bgHeader);
            dl.AddLine({r.x,r.y+r.h},{r.x+r.w,r.y+r.h},
                    ctx.theme.colors.separator.WithAlpha(80),1.f);
        
            const float32 ph=5.f, bh=r.h-6.f, bw=bh;
            float32 cx=r.x+ph;
            const float32 cy=r.y+3.f;
            const NkColor iconC=ctx.theme.colors.textPrimary;
            const NkColor iconD=ctx.theme.colors.textDisabled;
        
            // ── Helper bouton icône ────────────────────────────────────────────────
            auto iconBtn=[&](float32 w, bool en, auto drawFn)->bool {
                const NkRect btn={cx,cy,w,bh};
                const bool hov=en&&NkRectContains(btn,ctx.input.mousePos);
                const NkColor bg=hov?ctx.theme.colors.buttonHover
                                :ctx.theme.colors.bgSecondary.WithAlpha(80);
                dl.AddRectFilled(btn,bg,kBtnR);
                dl.AddRect(btn,ctx.theme.colors.border.WithAlpha(50),1.f,kBtnR);
                const NkRect ir={btn.x+2.f,btn.y+2.f,btn.w-4.f,btn.h-4.f};
                drawFn(ir, en?iconC:iconD, hov);
                cx+=w+kBtnGap;
                return hov&&ctx.ConsumeMouseClick(0)&&en;
            };
        
            // ◀ Précédent
            if (iconBtn(bw, state.historyPos>0, [&](NkRect ir,NkColor c,bool){
                    dl.AddArrow({ir.x+ir.w*0.55f,ir.y+ir.h*0.5f},ir.h*0.30f,2,c);})) {
                --state.historyPos;
                ::strncpy(state.currentPath,state.history[state.historyPos],511);
                state.needsRefresh=true;
            }
        
            // ▶ Suivant
            if (iconBtn(bw, state.historyPos<state.historyDepth, [&](NkRect ir,NkColor c,bool){
                    dl.AddArrow({ir.x+ir.w*0.45f,ir.y+ir.h*0.5f},ir.h*0.30f,0,c);})) {
                ::strncpy(state.currentPath,state.history[state.historyPos++],511);
                state.needsRefresh=true;
            }
        
            // ↑ Parent
            if (iconBtn(bw, true, [&](NkRect ir,NkColor c,bool){
                    dl.AddArrow({ir.x+ir.w*0.5f,ir.y+ir.h*0.40f},ir.h*0.30f,3,c);})) {
                char par[512]; ::strncpy(par,state.currentPath,511);
                char* sl=nullptr;
                for (char* pp=par;*pp;++pp) if (*pp=='/'||*pp=='\\') sl=pp;
                if (sl&&sl!=par) { *sl='\0';
                    if (::strlen(par)>=::strlen(cfg.rootPath)) Navigate(state,par); }
            }
        
            // ↻ Actualiser
            if (iconBtn(bw, true, [&](NkRect ir,NkColor c,bool){
                    DrawIconRefresh(dl,ir,c);})) {
                state.needsRefresh=true;
            }
        
            // ── Séparateur visuel ──────────────────────────────────────────────────
            dl.AddLine({cx+1.f,cy+4.f},{cx+1.f,cy+bh-4.f},
                    ctx.theme.colors.separator.WithAlpha(70),1.f);
            cx+=6.f;
        
            // 📁+ Nouveau dossier
            if (cfg.allowCreateDir)
                if (iconBtn(bw, true, [&](NkRect ir,NkColor c,bool hov){
                        DrawIconNewFolder(dl,ir,hov?ctx.theme.colors.accent:c);})) {
                    state.creatingDir=true;
                    ::strcpy(state.newDirBuffer,"NouveauDossier");
                }
        
            // 📄+ Nouveau fichier
            if (cfg.allowCreateFile)
                if (iconBtn(bw, true, [&](NkRect ir,NkColor c,bool hov){
                        DrawIconNewFile(dl,ir,hov?ctx.theme.colors.accent:c);})) {
                    state.creatingFile=true;
                    ::strcpy(state.newFileBuffer,"NouveauFichier.txt");
                }
        
            // ── Séparateur ─────────────────────────────────────────────────────────
            if (cfg.allowDelete||cfg.allowPermanentDelete) {
                dl.AddLine({cx+1.f,cy+4.f},{cx+1.f,cy+bh-4.f},
                        ctx.theme.colors.separator.WithAlpha(70),1.f);
                cx+=6.f;
            }
        
            // 🗑 Corbeille (supprimer)
            if (cfg.allowDelete)
                if (iconBtn(bw, state.numSelected>0, [&](NkRect ir,NkColor c,bool hov){
                        DrawIconTrash(dl,ir,hov?NkColor{220,80,80,255}:c,false);})) {
                    state.confirmDeletePending=true;
                    state.confirmPermanentDelete=false;
                    const char* sp=GetSelectedPath(state);
                    if (sp&&sp[0]) ::strncpy(state.confirmDeletePath,sp,511);
                }
        
            // 🗑⚡ Suppression définitive (force)
            if (cfg.allowPermanentDelete)
                if (iconBtn(bw, state.numSelected>0, [&](NkRect ir,NkColor c,bool hov){
                        DrawIconTrash(dl,ir,hov?NkColor{255,40,40,255}:c,true);})) {
                    state.confirmDeletePending=true;
                    state.confirmPermanentDelete=true;
                    const char* sp=GetSelectedPath(state);
                    if (sp&&sp[0]) ::strncpy(state.confirmDeletePath,sp,511);
                }
        
            // ── Séparateur ─────────────────────────────────────────────────────────
            if (cfg.showViewToggle) {
                dl.AddLine({cx+1.f,cy+4.f},{cx+1.f,cy+bh-4.f},
                        ctx.theme.colors.separator.WithAlpha(70),1.f);
                cx+=6.f;
        
                // Bouton mode liste
                const bool isList=(cfg.viewMode==NkUIFBViewMode::NK_FBVM_LIST);
                if (iconBtn(bw, true, [&](NkRect ir,NkColor c,bool){
                        if (isList) dl.AddRectFilled(ir,ctx.theme.colors.accent.WithAlpha(50),2.f);
                        DrawIconList(dl,ir,isList?ctx.theme.colors.accent:c);})) {
                    cfg.viewMode=NkUIFBViewMode::NK_FBVM_LIST;
                }
                // Bouton mode grille petite
                const bool isSmall=(cfg.viewMode==NkUIFBViewMode::NK_FBVM_ICONS_SMALL);
                if (iconBtn(bw, true, [&](NkRect ir,NkColor c,bool){
                        if (isSmall) dl.AddRectFilled(ir,ctx.theme.colors.accent.WithAlpha(50),2.f);
                        // Grille 3x3 petite
                        const float32 g=ir.w*0.26f,p2=ir.w*0.08f;
                        for (int32 rr=0;rr<3;++rr) for (int32 cc=0;cc<3;++cc)
                            dl.AddRectFilled({ir.x+p2+cc*(g+p2),ir.y+p2+rr*(g+p2),g,g},
                                            isSmall?ctx.theme.colors.accent:c,1.f);})) {
                    cfg.viewMode=NkUIFBViewMode::NK_FBVM_ICONS_SMALL;
                }
                // Bouton mode grille large
                const bool isLarge=(cfg.viewMode==NkUIFBViewMode::NK_FBVM_ICONS_LARGE);
                if (iconBtn(bw, true, [&](NkRect ir,NkColor c,bool){
                        if (isLarge) dl.AddRectFilled(ir,ctx.theme.colors.accent.WithAlpha(50),2.f);
                        DrawIconGrid(dl,ir,isLarge?ctx.theme.colors.accent:c);})) {
                    cfg.viewMode=NkUIFBViewMode::NK_FBVM_ICONS_LARGE;
                }
                // Bouton mode tuiles
                const bool isTile=(cfg.viewMode==NkUIFBViewMode::NK_FBVM_TILES);
                if (iconBtn(bw, true, [&](NkRect ir,NkColor c,bool){
                        if (isTile) dl.AddRectFilled(ir,ctx.theme.colors.accent.WithAlpha(50),2.f);
                        // Tuile = icône+texte côte à côte
                        const float32 sz=ir.h*0.55f;
                        dl.AddRectFilled({ir.x+ir.w*0.08f,ir.y+(ir.h-sz)*0.5f,sz,sz},
                                        isTile?ctx.theme.colors.accent:c,2.f);
                        dl.AddRectFilled({ir.x+ir.w*0.08f+sz+2.f,ir.y+ir.h*0.32f,
                                        ir.w*0.46f,ir.h*0.12f},
                                        isTile?ctx.theme.colors.accent:c,1.f);
                        dl.AddRectFilled({ir.x+ir.w*0.08f+sz+2.f,ir.y+ir.h*0.56f,
                                        ir.w*0.30f,ir.h*0.10f},
                                        isTile?ctx.theme.colors.accent.WithAlpha(150):c.WithAlpha(130),1.f);})) {
                    cfg.viewMode=NkUIFBViewMode::NK_FBVM_TILES;
                }
            }
        
            // ── Barre de recherche ─────────────────────────────────────────────────
            {
                const float32 sw=r.x+r.w-cx-ph;
                if (sw > 60.f) {
                    const NkRect sr={cx,cy,sw,bh};
                    const bool foc=state.searchFocused;
                    dl.AddRectFilled(sr,ctx.theme.colors.inputBg,kBtnR);
                    dl.AddRect(sr,foc?ctx.theme.colors.accent:ctx.theme.colors.border,
                            foc?1.8f:1.f,kBtnR);
        
                    // Icône loupe
                    const NkRect li={sr.x+2.f,sr.y+2.f,bh-4.f,bh-4.f};
                    DrawIconSearch(dl,li,foc?ctx.theme.colors.accent:ctx.theme.colors.textDisabled);
        
                    const float32 textX=sr.x+bh+2.f, maxTW=sw-bh-22.f;
                    const float32 textBl=sr.y+(bh-font.metrics.lineHeight)*0.5f+font.metrics.ascender;
        
                    if (state.searchBuffer[0])
                        font.RenderText(dl,{textX,textBl},state.searchBuffer,
                                    ctx.theme.colors.inputText,maxTW,false);
                    else if (!foc)
                        font.RenderText(dl,{textX,textBl},"Rechercher...",
                                    ctx.theme.colors.textDisabled,maxTW,false);
        
                    // Curseur clignotant
                    if (foc) {
                        const float32 tw=font.MeasureWidth(state.searchBuffer);
                        const float32 carX=textX+tw;
                        if (static_cast<int32>(ctx.time*2.f)%2==0) {
                            const float32 caTop=sr.y+(bh-font.metrics.ascender)*0.5f;
                            dl.AddLine({carX,caTop},{carX,caTop+font.metrics.ascender},
                                    ctx.theme.colors.inputCursor,1.5f);
                        }
                        // ✕ effacer
                        if (state.searchBuffer[0]) {
                            const NkRect xr={sr.x+sr.w-18.f,cy+2.f,14.f,bh-4.f};
                            const bool xh=NkRectContains(xr,ctx.input.mousePos);
                            if (xh) dl.AddRectFilled(xr,ctx.theme.colors.danger.WithAlpha(80),3.f);
                            const float32 m=3.f;
                            dl.AddLine({xr.x+m,xr.y+m},{xr.x+xr.w-m,xr.y+xr.h-m},
                                    ctx.theme.colors.textSecondary,1.5f);
                            dl.AddLine({xr.x+xr.w-m,xr.y+m},{xr.x+m,xr.y+xr.h-m},
                                    ctx.theme.colors.textSecondary,1.5f);
                            if (xh&&ctx.ConsumeMouseClick(0)) state.searchBuffer[0]='\0';
                        }
                    }
        
                    // Focus / blur
                    if (NkRectContains(sr,ctx.input.mousePos)&&ctx.ConsumeMouseClick(0))
                        state.searchFocused=true;
                    if (!NkRectContains(sr,ctx.input.mousePos)&&ctx.input.IsMouseClicked(0))
                        state.searchFocused=false;
        
                    // Saisie clavier — unicode complet
                    if (state.searchFocused) {
                        int32 sbl=static_cast<int32>(::strlen(state.searchBuffer));
                        for (int32 k=0;k<ctx.input.numInputChars;++k) {
                            const uint32 cp=ctx.input.inputChars[k];
                            if (cp=='\b')
                                sbl=Utf8BackSpace(state.searchBuffer,sbl);
                            else if (cp==27u)
                                state.searchFocused=false;
                            else if (cp>=32u) {
                                const int32 nl=Utf8Append(state.searchBuffer,sbl,127,cp);
                                if (nl>=0) sbl=nl;
                            }
                        }
                    }
                }
            }
            (void)prov; (void)font;
        }
        
        // =============================================================================
        //  DrawBreadcrumb
        // =============================================================================
        
        void NkUIFileBrowser::DrawBreadcrumb(NkUIContext& ctx, NkUIDrawList& dl,
                                            NkUIFont& font, const Layout& ly,
                                            NkUIFileBrowserState& state) noexcept
        {
            const NkRect& r=ly.breadcrumb;
            dl.AddRectFilled(r,ctx.theme.colors.bgHeader);
            dl.AddLine({r.x,r.y+r.h},{r.x+r.w,r.y+r.h},
                    ctx.theme.colors.separator.WithAlpha(60),1.f);
        
            const float32 ph=ctx.theme.metrics.paddingX;
            float32 cx=r.x+ph;
            const float32 bly=r.y+(r.h-font.metrics.lineHeight)*0.5f+font.metrics.ascender;
        
            auto drawSeg=[&](const char* label,const char* navPath,bool isLast){
                const float32 tw=font.MeasureWidth(label);
                const float32 bw=tw+ph*2.f;
                if (cx+bw>r.x+r.w-ph) return; // overflow
                const NkRect btn={cx,r.y+2.f,bw,r.h-4.f};
                const bool hov=NkRectContains(btn,ctx.input.mousePos);
                if      (isLast) dl.AddRectFilled(btn,ctx.theme.colors.buttonBg.WithAlpha(110),3.f);
                else if (hov)    dl.AddRectFilled(btn,ctx.theme.colors.buttonHover,3.f);
                font.RenderText(dl,{cx+ph,bly},label,
                    isLast?ctx.theme.colors.textPrimary:ctx.theme.colors.accent,tw,false);
                if (hov&&!isLast&&ctx.ConsumeMouseClick(0)) Navigate(state,navPath);
                cx+=bw;
                if (!isLast) {
                    // Chevron ›
                    const float32 sw=font.MeasureWidth(" \xe2\xba\x85 "); // › UTF-8
                    font.RenderText(dl,{cx,bly}," \xe2\xba\x85 ",
                                ctx.theme.colors.textDisabled,sw,false);
                    cx+=sw;
                }
            };
        
        #if defined(_WIN32)
            const bool atComp=(state.currentPath[0]=='\0'||
                            ::strcmp(state.currentPath,"Computer")==0);
            drawSeg("Ordinateur","Computer",atComp);
            if (atComp) return;
            const char SEP='\\';
        #else
            const bool atRoot=(::strcmp(state.currentPath,"/")==0);
            drawSeg("/","/",atRoot);
            if (atRoot) return;
            const char SEP='/';
        #endif
        
            char accum[512]={};
            const char* p=state.currentPath;
            while (*p==SEP) ++p;
        
            char seg[256]={};
            while (*p) {
                const char* end=p;
                while (*end&&*end!=SEP) ++end;
                const int32 len=static_cast<int32>(end-p);
                if (len<=0&&*end){p=end+1;continue;}
                ::memcpy(seg,p,static_cast<size_t>(len)); seg[len]='\0';
        #if defined(_WIN32)
                if (accum[0]) ::strncat(accum,"\\",1);
                ::strncat(accum,seg,510-::strlen(accum));
        #else
                ::strncat(accum,"/",1);
                ::strncat(accum,seg,510-::strlen(accum));
        #endif
                const bool isLast=(*end=='\0');
                drawSeg(seg,accum,isLast);
                if (!isLast) p=end+1; else break;
            }
        }
        
        // =============================================================================
        //  DrawBookmarkPanel
        // =============================================================================
        
        void NkUIFileBrowser::DrawBookmarkPanel(NkUIContext& ctx, NkUIDrawList& dl,
                                                NkUIFont& font, const Layout& ly,
                                                const NkUIFileBrowserConfig& cfg,
                                                NkUIFileBrowserState& state) noexcept
        {
            const NkRect& r=ly.bookmarkPanel;
            dl.AddRectFilled(r,ctx.theme.colors.bgSecondary);
            dl.AddLine({r.x+r.w,r.y},{r.x+r.w,r.y+r.h},
                    ctx.theme.colors.separator.WithAlpha(80),1.f);
        
            const float32 ih=cfg.rowHeight, ph=ctx.theme.metrics.paddingX;
            float32 y=r.y+4.f;
            const float32 asc=font.metrics.ascender;
        
            // ── Titre section ────────────────────────────────────────────────────────
            font.RenderText(dl,{r.x+ph,y+asc},"Acc\xc3\xa8s rapide",
                        ctx.theme.colors.textDisabled,r.w-ph*2.f,false);
            y+=ih;
            dl.AddLine({r.x+ph,y},{r.x+r.w-ph,y},ctx.theme.colors.separator.WithAlpha(45),1.f);
            y+=3.f;
        
            // ── Liste favoris ────────────────────────────────────────────────────────
            for (int32 i=0;i<cfg.numBookmarks;++i) {
                const NkRect row={r.x,y,r.w,ih};
                const bool hov=NkRectContains(row,ctx.input.mousePos);
                const bool active=(!::strcmp(cfg.bookmarks[i].path,state.currentPath));
                if (active)
                    dl.AddRectFilled(row,ctx.theme.colors.accent.WithAlpha(65),3.f);
                else if (hov)
                    dl.AddRectFilled(row,ctx.theme.colors.buttonHover,3.f);
        
                // Icône
                const NkRect ico={r.x+5.f,y+(ih-14.f)*0.5f,14.f,14.f};
                const NkColor fcol=active?ctx.theme.colors.textOnAccent:NkColor{255,205,60,230};
                DrawIconFolder(dl,ico,fcol);
        
                font.RenderText(dl,{r.x+22.f,y+(ih-font.metrics.lineHeight)*0.5f+asc},
                            cfg.bookmarks[i].label,
                            active?ctx.theme.colors.textOnAccent:ctx.theme.colors.textSecondary,
                            r.w-28.f,true);
        
                if (hov&&ctx.ConsumeMouseClick(0)) Navigate(state,cfg.bookmarks[i].path);
                y+=ih;
            }
        
            // ── Ajouter le dossier courant ───────────────────────────────────────────
            y+=4.f;
            dl.AddLine({r.x+ph,y},{r.x+r.w-ph,y},ctx.theme.colors.separator.WithAlpha(45),1.f);
            y+=3.f;
            const NkRect addRow={r.x,y,r.w,ih};
            const bool addH=NkRectContains(addRow,ctx.input.mousePos);
            if (addH) dl.AddRectFilled(addRow,ctx.theme.colors.buttonHover,3.f);
            const NkRect addIco={r.x+5.f,y+(ih-14.f)*0.5f,14.f,14.f};
            DrawIconNewFolder(dl,addIco,addH?ctx.theme.colors.accent:ctx.theme.colors.textDisabled);
            font.RenderText(dl,{r.x+22.f,y+(ih-font.metrics.lineHeight)*0.5f+asc},
                        "Marquer ici",
                        addH?ctx.theme.colors.accent:ctx.theme.colors.textDisabled,
                        r.w-28.f,false);
            if (addH&&ctx.ConsumeMouseClick(0)) {
                const char* sl=::strrchr(state.currentPath,'/');
                const char* sl2=::strrchr(state.currentPath,'\\');
                if (!sl||sl2>sl) sl=sl2;
                const char* nm=sl?sl+1:state.currentPath;
                if (nm&&nm[0])
                    AddBookmark(const_cast<NkUIFileBrowserConfig&>(cfg),nm,state.currentPath,false);
            }
        }
        
        // =============================================================================
        //  DrawColumnHeaders
        // =============================================================================
        
        void NkUIFileBrowser::DrawColumnHeaders(NkUIContext& ctx, NkUIDrawList& dl,
                                                NkUIFont& font, const Layout& ly,
                                                const NkUIFileBrowserConfig& cfg,
                                                NkUIFileBrowserState& state) noexcept
        {
            if (cfg.viewMode!=NkUIFBViewMode::NK_FBVM_LIST) return;
            const NkRect& r=ly.header;
            dl.AddRectFilled(r,ctx.theme.colors.bgHeader);
            dl.AddLine({r.x,r.y+r.h},{r.x+r.w,r.y+r.h},
                    ctx.theme.colors.separator.WithAlpha(80),1.f);
        
            const float32 ph=ctx.theme.metrics.paddingX;
            const float32 bly=r.y+(r.h-font.metrics.lineHeight)*0.5f+font.metrics.ascender;
            const float32 aw=r.w-kIconCol-kSBW;
            const float32 szW=cfg.showFileSize?state.colSizeW:0.f;
            const float32 dtW=cfg.showModifiedDate?state.colDateW:0.f;
            const float32 tpW=cfg.showTypeCol?state.colTypeW:0.f;
            state.colNameW=aw-szW-dtW-tpW;
        
            struct ColDef{ const char* lbl; float32 x; float32 w; NkUIFBSortCol sc; };
            const float32 cx=r.x+kIconCol;
            const ColDef cols[]={
                {"Nom",cx,state.colNameW,NkUIFBSortCol::NK_FBS_NAME},
                {cfg.showFileSize?"Taille":"",cx+state.colNameW,szW,NkUIFBSortCol::NK_FBS_SIZE},
                {cfg.showModifiedDate?"Modifi\xc3\xa9":"",cx+state.colNameW+szW,dtW,NkUIFBSortCol::NK_FBS_DATE},
                {cfg.showTypeCol?"Type":"",cx+state.colNameW+szW+dtW,tpW,NkUIFBSortCol::NK_FBS_TYPE},
            };
            for (const auto& col:cols) {
                if (!col.lbl||!col.lbl[0]||col.w<4.f) continue;
                const NkRect hr={col.x,r.y,col.w,r.h};
                const bool hov=NkRectContains(hr,ctx.input.mousePos);
                if (hov) dl.AddRectFilled(hr,ctx.theme.colors.buttonHover.WithAlpha(55));
                const bool active=(state.sortCol==col.sc);
                font.RenderText(dl,{col.x+ph,bly},col.lbl,
                    active?ctx.theme.colors.accent:ctx.theme.colors.textSecondary,
                    col.w-ph-14.f,false);
                if (active)
                    dl.AddArrow({col.x+col.w-10.f,r.y+r.h*0.5f},5.f,
                                state.sortAscending?3:1,ctx.theme.colors.accent);
                if (hov&&ctx.ConsumeMouseClick(0)) {
                    if (state.sortCol==col.sc) state.sortAscending=!state.sortAscending;
                    else { state.sortCol=col.sc; state.sortAscending=true; }
                    SortEntries(state,state.sortCol,state.sortAscending);
                }
                dl.AddLine({col.x,r.y+4.f},{col.x,r.y+r.h-4.f},
                        ctx.theme.colors.separator.WithAlpha(45),1.f);
            }
        }
 
        // =============================================================================
        //  Helpers saisie unicode (utilisés par tous les champs)
        // =============================================================================
        
        static void TextFieldInput(NkUIContext& ctx, char* buf, int32 maxLen, bool& editing) noexcept {
            int32 bl=static_cast<int32>(::strlen(buf));
            for (int32 k=0;k<ctx.input.numInputChars;++k) {
                const uint32 cp=ctx.input.inputChars[k];
                if (cp=='\b') bl=Utf8BackSpace(buf,bl);
                else if (cp==27u) { editing=false; }
                else if (cp=='\r'||cp=='\n') { /* géré par l'appelant */ }
                else if (cp>=32u) {
                    const int32 nl=Utf8Append(buf,bl,maxLen,cp);
                    if (nl>=0) bl=nl;
                }
            }
        }
        
        // Rendu d'un champ de texte inline simple
        static void DrawTextField(NkUIContext& ctx, NkUIDrawList& dl, NkUIFont& font, NkRect inp, char* buf, int32 maxLen, bool focused) noexcept {
            dl.AddRectFilled(inp,ctx.theme.colors.inputBg,3.f);
            dl.AddRect(inp,focused?ctx.theme.colors.accent:ctx.theme.colors.border,
                    focused?2.f:1.f,3.f);
            const float32 ph=4.f;
            const float32 bly=inp.y+(inp.h-font.metrics.lineHeight)*0.5f+font.metrics.ascender;
            font.RenderText(dl,{inp.x+ph,bly},buf,ctx.theme.colors.inputText,inp.w-ph*2.f,false);
            if (focused && static_cast<int32>(ctx.time*2.f)%2==0) {
                const float32 tw=font.MeasureWidth(buf);
                const float32 caTop=inp.y+(inp.h-font.metrics.ascender)*0.5f;
                dl.AddLine({inp.x+ph+tw,caTop},{inp.x+ph+tw,caTop+font.metrics.ascender},
                        ctx.theme.colors.inputCursor,1.5f);
            }
            if (focused) {
                bool dummy=true;
                TextFieldInput(ctx,buf,maxLen,dummy); (void)maxLen;
            }
        }
        
        // =============================================================================
        //  DrawEntryRow (mode liste — une ligne)
        // =============================================================================
        
        NkUIFileBrowserResult NkUIFileBrowser::DrawEntryRow(
            NkUIContext& ctx, NkUIDrawList& dl, NkUIFont& font,
            NkRect row, int32 idx, bool selected, bool hovered,
            const NkUIFileBrowserConfig& cfg, NkUIFileBrowserState& state,
            const NkUIFSProvider& prov) noexcept
        {
            NkUIFileBrowserResult result{};
            const NkUIFileEntry& e=state.entries[idx];
            const float32 ph=ctx.theme.metrics.paddingX;
            const float32 bly=row.y+(row.h-font.metrics.lineHeight)*0.5f+font.metrics.ascender;
        
            // Fond
            if      (selected) dl.AddRectFilled(row,ctx.theme.colors.accent.WithAlpha(72),2.f);
            else if (hovered)  dl.AddRectFilled(row,ctx.theme.colors.buttonHover.WithAlpha(50),2.f);
        
            // Icône
            const float32 isz=kIconCol-4.f;
            const NkRect iconR={row.x+2.f,row.y+(row.h-isz)*0.5f,isz,isz};
            DrawEntryIcon(dl,iconR,e);
        
            const float32 nX=row.x+kIconCol;
        
            // ── Renommage inline ────────────────────────────────────────────────────
            if (state.renaming && state.renamingIdx==idx) {
                const NkRect inp={nX,row.y+2.f,state.colNameW-ph,row.h-4.f};
                DrawTextField(ctx,dl,font,inp,state.renameBuffer,255,true);
        
                // Valider Enter / annuler Esc
                for (int32 k=0;k<ctx.input.numInputChars;++k) {
                    const uint32 cp=ctx.input.inputChars[k];
                    if (cp=='\r'||cp=='\n') {
                        if (state.renameBuffer[0]&&prov.move) {
                            char par[512]; ::strncpy(par,e.path,511);
                            char* sl=nullptr;
                            for (char* pp=par;*pp;++pp) if (*pp=='/'||*pp=='\\') sl=pp;
                            char np[512];
                            if (sl){ *sl='\0'; ::snprintf(np,sizeof(np),"%s/%s",par,state.renameBuffer); }
                            else    ::snprintf(np,sizeof(np),"%s",state.renameBuffer);
                            if (prov.move(e.path,np,prov.userData)) {
                                ::strncpy(result.path,e.path,511);
                                ::strncpy(result.target,np,511);
                                result.event=NkUIFileBrowserEvent::NK_FB_RENAME_COMMITTED;
                                state.needsRefresh=true;
                            }
                        }
                        state.renaming=false; state.renamingIdx=-1;
                    } else if (cp==27u) { state.renaming=false; state.renamingIdx=-1; }
                }
            } else {
                // ── Nom ──────────────────────────────────────────────────────────────
                const NkColor nc=e.isHidden?ctx.theme.colors.textDisabled:ctx.theme.colors.textPrimary;
                font.RenderText(dl,{nX+ph,bly},e.name,nc,state.colNameW-ph*2.f,true);
            }
        
            // ── Taille ───────────────────────────────────────────────────────────────
            if (cfg.showFileSize&&state.colSizeW>0.f) {
                if (e.type!=NkUIFileType::NK_FT_DIRECTORY) {
                    char szBuf[32]; FormatSize(e.size,szBuf,32);
                    font.RenderText(dl,{nX+state.colNameW+ph,bly},szBuf,
                                ctx.theme.colors.textSecondary,state.colSizeW-ph*2.f,false);
                }
            }
            // ── Date ─────────────────────────────────────────────────────────────────
            if (cfg.showModifiedDate&&state.colDateW>0.f) {
                char dtBuf[32]; FormatDate(e.modifiedTime,dtBuf,32);
                font.RenderText(dl,{nX+state.colNameW+state.colSizeW+ph,bly},dtBuf,
                            ctx.theme.colors.textSecondary,state.colDateW-ph*2.f,false);
            }
            // ── Type ─────────────────────────────────────────────────────────────────
            if (cfg.showTypeCol&&state.colTypeW>0.f) {
                const char* dot=::strrchr(e.name,'.');
                font.RenderText(dl,{nX+state.colNameW+state.colSizeW+state.colDateW+ph,bly},
                            dot?dot+1:"\xe2\x80\x94",
                            ctx.theme.colors.textDisabled,state.colTypeW-ph*2.f,false);
            }
        
            // ── Interactions ──────────────────────────────────────────────────────────
            if (hovered) {
                if (ctx.input.IsMouseClicked(0)) {
                    // Clic simple : sélectionner
                    const bool ctrl=ctx.input.ctrl, shift=ctx.input.shift;
                    const bool multi=(cfg.mode==NkUIFBMode::NK_FBM_MULTI);
                    if (multi&&ctrl) {
                        bool found=false;
                        for (int32 k=0;k<state.numSelected;++k) if (state.selected[k]==idx) {
                            for (int32 j=k;j<state.numSelected-1;++j) state.selected[j]=state.selected[j+1];
                            --state.numSelected; found=true; break;
                        }
                        if (!found&&state.numSelected<NkUIFileBrowserState::MAX_SELECTED)
                            state.selected[state.numSelected++]=idx;
                    } else if (multi&&shift&&state.lastClickedIdx>=0) {
                        const int32 from=state.lastClickedIdx<idx?state.lastClickedIdx:idx;
                        const int32 to  =state.lastClickedIdx<idx?idx:state.lastClickedIdx;
                        state.numSelected=0;
                        for (int32 kk=from;kk<=to&&state.numSelected<NkUIFileBrowserState::MAX_SELECTED;++kk)
                            state.selected[state.numSelected++]=kk;
                    } else {
                        state.numSelected=1; state.selected[0]=idx;
                    }
                    state.lastClickedIdx=idx;
                }
                if (ctx.input.mouseDblClick[0]) {
                    if (e.type==NkUIFileType::NK_FT_DIRECTORY) {
                        Navigate(state,e.path);
                        result.event=NkUIFileBrowserEvent::NK_FB_DIR_CHANGED;
                        ::strncpy(result.path,e.path,511);
                    } else {
                        result.event=NkUIFileBrowserEvent::NK_FB_FILE_SELECTED;
                        ::strncpy(result.path,e.path,511);
                    }
                }
                if (selected&&cfg.allowRename&&ctx.input.IsKeyPressed(NkKey::NK_F2)) {
                    state.renaming=true; state.renamingIdx=idx;
                    ::strncpy(state.renameBuffer,e.name,255);
                }
                if (selected&&(cfg.allowDelete||cfg.allowPermanentDelete)
                    &&ctx.input.IsKeyPressed(NkKey::NK_DELETE)) {
                    state.confirmDeletePending=true;
                    state.confirmPermanentDelete=ctx.input.ctrl;
                    ::strncpy(state.confirmDeletePath,e.path,511);
                }
                // DnD source
                if (cfg.allowDnD&&!state.dndActive&&ctx.input.IsMouseDown(0)) {
                    const NkVec2 d=ctx.input.mouseDelta;
                    if (d.x*d.x+d.y*d.y>25.f){ state.dndActive=true; state.dndSourceIdx=idx; }
                }
            }
        
            // DnD cible (dossier survolé)
            if (state.dndActive&&hovered&&state.dndSourceIdx!=idx
                &&e.type==NkUIFileType::NK_FT_DIRECTORY) {
                dl.AddRect(row,ctx.theme.colors.accent,2.f,3.f);
                if (!ctx.input.mouseDown[0]) {
                    const NkUIFileEntry& src=state.entries[state.dndSourceIdx];
                    char dst[512]; ::snprintf(dst,sizeof(dst),"%s/%s",e.path,src.name);
                    if (prov.move&&prov.move(src.path,dst,prov.userData)) {
                        ::strncpy(result.path,src.path,511); ::strncpy(result.target,dst,511);
                        result.event=NkUIFileBrowserEvent::NK_FB_DND_DROP;
                        state.needsRefresh=true;
                    }
                    state.dndActive=false; state.dndSourceIdx=-1;
                }
            }
        
            // Séparateur ligne
            dl.AddLine({row.x+4.f,row.y+row.h-0.5f},{row.x+row.w-4.f,row.y+row.h-0.5f},
                    ctx.theme.colors.separator.WithAlpha(18),0.5f);
            return result;
        }
        
        // =============================================================================
        //  DrawListView
        // =============================================================================
        
        NkUIFileBrowserResult NkUIFileBrowser::DrawListView(
            NkUIContext& ctx, NkUIDrawList& dl, NkUIFont& font,
            NkUIID id, const Layout& ly, NkUIFileBrowserConfig& cfg,
            NkUIFileBrowserState& state, const NkUIFSProvider& prov) noexcept
        {
            NkUIFileBrowserResult result{};
            const NkRect& r=ly.fileList;
            const float32 rowH=cfg.rowHeight;
            const NkRect listR={r.x,r.y,r.w-kSBW,r.h};
            dl.PushClipRect(listR,true);
            dl.AddRectFilled(listR,ctx.theme.colors.bgWindow);
        
            // ── Champs de création inline ────────────────────────────────────────────
            float32 createH=0.f;
            auto drawInline=[&](bool& creating, char* buf, int32 bufMax, bool isDir){
                if (!creating) return;
                const NkRect inp={listR.x+2.f,listR.y+createH+2.f,listR.w-4.f,rowH-4.f};
                // Fond
                dl.AddRectFilled({listR.x,listR.y+createH,listR.w,rowH},
                                ctx.theme.colors.accent.WithAlpha(20));
                // Icône
                const NkRect ic={inp.x,inp.y,rowH-4.f,rowH-4.f};
                if (isDir) DrawIconNewFolder(dl,ic,ctx.theme.colors.accent);
                else       DrawIconNewFile(dl,ic,ctx.theme.colors.accent);
                // Champ texte
                const NkRect fi={inp.x+rowH,inp.y,inp.w-rowH,inp.h};
                DrawTextField(ctx,dl,font,fi,buf,bufMax,true);
        
                for (int32 k=0;k<ctx.input.numInputChars;++k) {
                    const uint32 cp=ctx.input.inputChars[k];
                    if (cp=='\r'||cp=='\n') {
                        if (buf[0]) {
                            char np[512]; ::snprintf(np,sizeof(np),"%s/%s",state.currentPath,buf);
                            if (isDir&&prov.mkdir_&&prov.mkdir_(np,prov.userData)) {
                                ::strncpy(result.path,np,511);
                                result.event=NkUIFileBrowserEvent::NK_FB_CREATE_DIR;
                                state.needsRefresh=true;
                            } else if (!isDir&&prov.write) {
                                prov.write(np,nullptr,0,prov.userData);
                                ::strncpy(result.path,np,511);
                                result.event=NkUIFileBrowserEvent::NK_FB_CREATE_FILE;
                                state.needsRefresh=true;
                            }
                        }
                        creating=false;
                    }
                }
                createH+=rowH;
            };
            drawInline(state.creatingDir, state.newDirBuffer, 255, true);
            drawInline(state.creatingFile,state.newFileBuffer,255,false);
        
            // ── Comptage des entrées visibles ────────────────────────────────────────
            int32 vis=0;
            for (int32 i=0;i<state.numEntries;++i) {
                const NkUIFileEntry& e=state.entries[i];
                if (!cfg.showHidden&&e.isHidden) continue;
                if (state.searchBuffer[0]&&!Utf8ContainsI(e.name,state.searchBuffer)) continue;
                if (!MatchesFilter(e,cfg.filterExt)) continue;
                ++vis;
            }
            state.contentH=createH+vis*rowH;
        
            // ── Scroll molette ───────────────────────────────────────────────────────
            if (NkRectContains(r,ctx.input.mousePos)&&!ctx.wheelConsumed&&ctx.input.mouseWheel!=0.f) {
                state.scrollY-=ctx.input.mouseWheel*rowH*3.f; ctx.wheelConsumed=true;
            }
            const float32 maxSc=(state.contentH>r.h)?state.contentH-r.h:0.f;
            if (state.scrollY<0.f) state.scrollY=0.f;
            if (state.scrollY>maxSc) state.scrollY=maxSc;
        
            // ── Navigation clavier ───────────────────────────────────────────────────
            if (!state.renaming&&!state.creatingDir&&!state.creatingFile&&!state.searchFocused) {
                if (ctx.input.IsKeyPressed(NkKey::NK_UP)&&state.numSelected>0) {
                    int32 c=state.selected[0]-1; if (c<0) c=0;
                    state.numSelected=1; state.selected[0]=c;
                    const float32 ty=createH+c*rowH-state.scrollY;
                    if (ty<0.f) state.scrollY+=ty;
                }
                if (ctx.input.IsKeyPressed(NkKey::NK_DOWN)&&state.numSelected>0) {
                    int32 c=state.selected[0]+1; if (c>=state.numEntries) c=state.numEntries-1;
                    state.numSelected=1; state.selected[0]=c;
                    const float32 by=createH+(c+1)*rowH-state.scrollY;
                    if (by>r.h) state.scrollY=createH+(c+1)*rowH-r.h;
                }
                if (ctx.input.IsKeyPressed(NkKey::NK_ENTER)&&state.numSelected>0) {
                    NkUIFileBrowserResult tmp{};
                    const NkUIFileEntry& e=state.entries[state.selected[0]];
                    if (e.type==NkUIFileType::NK_FT_DIRECTORY){
                        Navigate(state,e.path);
                        tmp.event=NkUIFileBrowserEvent::NK_FB_DIR_CHANGED;
                        ::strncpy(tmp.path,e.path,511);
                    } else { tmp.event=NkUIFileBrowserEvent::NK_FB_FILE_SELECTED;
                            ::strncpy(tmp.path,e.path,511); }
                    result=tmp;
                }
                if (ctx.input.ctrl&&ctx.input.IsKeyPressed(NkKey::NK_A)) {
                    state.numSelected=0;
                    for (int32 i=0;i<state.numEntries&&
                        state.numSelected<NkUIFileBrowserState::MAX_SELECTED;++i)
                        state.selected[state.numSelected++]=i;
                }
            }
        
            // ── Dessin des lignes ────────────────────────────────────────────────────
            float32 y=listR.y+createH-state.scrollY;
            state.hoveredIdx=-1;
        
            for (int32 i=0;i<state.numEntries;++i) {
                const NkUIFileEntry& e=state.entries[i];
                if (!cfg.showHidden&&e.isHidden) continue;
                if (state.searchBuffer[0]&&!Utf8ContainsI(e.name,state.searchBuffer)) continue;
                if (!MatchesFilter(e,cfg.filterExt)) continue;
                if (y+rowH<listR.y){ y+=rowH; continue; }
                if (y>listR.y+listR.h) break;
        
                const NkRect rowRect={listR.x,y,listR.w,rowH};
                const bool hov=NkRectContains(rowRect,ctx.input.mousePos);
                if (hov) state.hoveredIdx=i;
                bool sel=false;
                for (int32 k=0;k<state.numSelected;++k) if (state.selected[k]==i){sel=true;break;}
        
                NkUIFileBrowserResult rr=DrawEntryRow(ctx,dl,font,rowRect,i,sel,hov,cfg,state,prov);
                if (rr.event!=NkUIFileBrowserEvent::NK_FB_NONE) result=rr;
                y+=rowH;
            }
        
            // Clic dans le vide → déselectionner
            if (NkRectContains(listR,ctx.input.mousePos)&&state.hoveredIdx<0&&ctx.ConsumeMouseClick(0))
                state.numSelected=0;
        
            dl.PopClipRect();
        
            // ── Scrollbar verticale ──────────────────────────────────────────────────
            if (state.contentH>r.h) {
                const NkRect track={r.x+r.w-kSBW,r.y,kSBW,r.h};
                dl.AddRectFilled(track,ctx.theme.colors.scrollBg,4.f);
                const float32 ratio=r.h/state.contentH;
                const float32 tH=r.h*ratio;
                const float32 tY=r.y+(maxSc>0.f?(state.scrollY/maxSc)*(r.h-tH):0.f);
                const NkRect thumb={track.x+2.f,tY,kSBW-4.f,tH};
                const bool hSB=NkRectContains(track,ctx.input.mousePos);
                const NkUIID sbId=NkHashInt(id,0xFB01u);
                const bool aSB=ctx.IsActive(sbId);
                dl.AddRectFilled(thumb,(aSB||hSB)?ctx.theme.colors.scrollThumbHov:ctx.theme.colors.scrollThumb,4.f);
                if (hSB&&ctx.ConsumeMouseClick(0)) ctx.SetActive(sbId);
                if (aSB){ float32 rel=(ctx.input.mousePos.y-r.y-tH*0.5f)/(r.h-tH);
                    state.scrollY=(rel<0.f?0.f:rel>1.f?1.f:rel)*maxSc;
                    if (!ctx.input.mouseDown[0]) ctx.ClearActive(); }
            }
        
            if (state.dndActive&&!ctx.input.mouseDown[0]){ state.dndActive=false; state.dndSourceIdx=-1; }
            return result;
        }
        
        // =============================================================================
        //  DrawGridView (mode ICONS_SMALL / ICONS_LARGE / TILES)
        // =============================================================================
        
        NkUIFileBrowserResult NkUIFileBrowser::DrawGridView(
            NkUIContext& ctx, NkUIDrawList& dl, NkUIFont& font,
            NkUIID id, const Layout& ly, NkUIFileBrowserConfig& cfg,
            NkUIFileBrowserState& state, const NkUIFSProvider& prov) noexcept
        {
            NkUIFileBrowserResult result{};
            const NkRect& r=ly.fileList;
        
            // Taille de la cellule selon le mode
            float32 cellW, cellH;
            const bool isTile=(cfg.viewMode==NkUIFBViewMode::NK_FBVM_TILES);
            if (isTile) {
                cellW=r.w-kSBW; cellH=cfg.rowHeight*2.0f;
            } else {
                const float32 sz=(cfg.viewMode==NkUIFBViewMode::NK_FBVM_ICONS_SMALL)
                                ?cfg.iconSmallSize:cfg.iconGridSize;
                cellW=sz+8.f; cellH=sz+font.metrics.lineHeight+10.f;
            }
            const int32 cols=isTile?1:(int32)::fmaxf(1.f,::floorf((r.w-kSBW)/cellW));
        
            // Comptage visible
            int32 vis=0;
            for (int32 i=0;i<state.numEntries;++i) {
                const NkUIFileEntry& e=state.entries[i];
                if (!cfg.showHidden&&e.isHidden) continue;
                if (state.searchBuffer[0]&&!Utf8ContainsI(e.name,state.searchBuffer)) continue;
                if (!MatchesFilter(e,cfg.filterExt)) continue;
                ++vis;
            }
            const int32 rows=(vis+cols-1)/cols;
            state.contentH=rows*cellH;
        
            // Scroll molette
            if (NkRectContains(r,ctx.input.mousePos)&&!ctx.wheelConsumed&&ctx.input.mouseWheel!=0.f) {
                state.gridScrollY-=ctx.input.mouseWheel*cellH; ctx.wheelConsumed=true;
            }
            const float32 maxSc=(state.contentH>r.h)?state.contentH-r.h:0.f;
            if (state.gridScrollY<0.f) state.gridScrollY=0.f;
            if (state.gridScrollY>maxSc) state.gridScrollY=maxSc;
        
            const NkRect listR={r.x,r.y,r.w-kSBW,r.h};
            dl.PushClipRect(listR,true);
            dl.AddRectFilled(listR,ctx.theme.colors.bgWindow);
        
            float32 gx=listR.x, gy=listR.y-state.gridScrollY;
            int32 col=0; state.hoveredIdx=-1;
        
            for (int32 i=0;i<state.numEntries;++i) {
                const NkUIFileEntry& e=state.entries[i];
                if (!cfg.showHidden&&e.isHidden) continue;
                if (state.searchBuffer[0]&&!Utf8ContainsI(e.name,state.searchBuffer)) continue;
                if (!MatchesFilter(e,cfg.filterExt)) continue;
        
                const NkRect cell={gx,gy,cellW,cellH};
                if (gy+cellH>=listR.y && gy<=listR.y+listR.h) {
                    const bool hov=NkRectContains(cell,ctx.input.mousePos);
                    if (hov) state.hoveredIdx=i;
                    bool sel=false;
                    for (int32 k=0;k<state.numSelected;++k) if (state.selected[k]==i){sel=true;break;}
        
                    if (isTile) {
                        // ── Mode Tuiles ──────────────────────────────────────────────
                        if      (sel) dl.AddRectFilled(cell,ctx.theme.colors.accent.WithAlpha(72),4.f);
                        else if (hov) dl.AddRectFilled(cell,ctx.theme.colors.buttonHover.WithAlpha(55),4.f);
                        dl.AddRect(cell,ctx.theme.colors.separator.WithAlpha(25),1.f,4.f);
                        // Icône gauche
                        const float32 isz=cellH-6.f;
                        const NkRect ir={cell.x+3.f,cell.y+3.f,isz,isz};
                        DrawFileThumbnail(dl,ir,e);
                        // Info droite
                        const float32 tx=cell.x+isz+8.f;
                        const float32 nameY=cell.y+(cellH-font.metrics.lineHeight*2.f-4.f)*0.5f;
                        const float32 asc=font.metrics.ascender;
                        font.RenderText(dl,{tx,nameY+asc},e.name,
                                    sel?ctx.theme.colors.textOnAccent:ctx.theme.colors.textPrimary,
                                    cell.w-isz-12.f,true);
                        if (e.type!=NkUIFileType::NK_FT_DIRECTORY) {
                            char szBuf[32]; FormatSize(e.size,szBuf,32);
                            font.RenderText(dl,{tx,nameY+font.metrics.lineHeight+4.f+asc},szBuf,
                                        ctx.theme.colors.textSecondary,cell.w-isz-12.f,false);
                        }
                        dl.AddLine({cell.x,cell.y+cellH-0.5f},{cell.x+cell.w,cell.y+cellH-0.5f},
                                ctx.theme.colors.separator.WithAlpha(18),0.5f);
                    } else {
                        // ── Mode Grille ──────────────────────────────────────────────
                        if      (sel) dl.AddRectFilled(cell,ctx.theme.colors.accent.WithAlpha(80),4.f);
                        else if (hov) dl.AddRectFilled(cell,ctx.theme.colors.buttonHover.WithAlpha(60),4.f);
                        dl.AddRect(cell,ctx.theme.colors.separator.WithAlpha(22),1.f,4.f);
                        // Miniature
                        const float32 pad=4.f;
                        const float32 thumbH=cellH-font.metrics.lineHeight-8.f;
                        const NkRect thumbR={cell.x+pad,cell.y+pad,cellW-pad*2.f,thumbH};
                        DrawFileThumbnail(dl,thumbR,e);
                        // Nom centré sous la miniature
                        const float32 nameY=cell.y+pad+thumbH+2.f;
                        const float32 tw=font.MeasureWidth(e.name);
                        const float32 tx=cell.x+(cellW-tw)*0.5f;
                        font.RenderText(dl,{tx,nameY+font.metrics.ascender},e.name,
                                    sel?ctx.theme.colors.textOnAccent:ctx.theme.colors.textPrimary,
                                    cellW-4.f,true);
                    }
        
                    // Interactions
                    if (hov) {
                        if (ctx.input.IsMouseClicked(0)) {
                            const bool ctrl=ctx.input.ctrl,shift=ctx.input.shift;
                            const bool multi=(cfg.mode==NkUIFBMode::NK_FBM_MULTI);
                            if (multi&&ctrl) {
                                bool found=false;
                                for (int32 kk=0;kk<state.numSelected;++kk) if (state.selected[kk]==i){
                                    for (int32 jj=kk;jj<state.numSelected-1;++jj) state.selected[jj]=state.selected[jj+1];
                                    --state.numSelected; found=true; break; }
                                if (!found&&state.numSelected<NkUIFileBrowserState::MAX_SELECTED) state.selected[state.numSelected++]=i;
                            } else if (multi&&shift&&state.lastClickedIdx>=0) {
                                const int32 from=state.lastClickedIdx<i?state.lastClickedIdx:i;
                                const int32 to  =state.lastClickedIdx<i?i:state.lastClickedIdx;
                                state.numSelected=0;
                                for (int32 kk=from;kk<=to&&state.numSelected<NkUIFileBrowserState::MAX_SELECTED;++kk)
                                    state.selected[state.numSelected++]=kk;
                            } else { state.numSelected=1; state.selected[0]=i; }
                            state.lastClickedIdx=i;
                        }
                        if (ctx.input.mouseDblClick[0]) {
                            if (e.type==NkUIFileType::NK_FT_DIRECTORY) {
                                Navigate(state,e.path);
                                result.event=NkUIFileBrowserEvent::NK_FB_DIR_CHANGED;
                                ::strncpy(result.path,e.path,511);
                            } else { result.event=NkUIFileBrowserEvent::NK_FB_FILE_SELECTED;
                                    ::strncpy(result.path,e.path,511); }
                        }
                        if (sel&&cfg.allowRename&&ctx.input.IsKeyPressed(NkKey::NK_F2)) {
                            state.renaming=true; state.renamingIdx=i;
                            ::strncpy(state.renameBuffer,e.name,255);
                        }
                        if (sel&&ctx.input.IsKeyPressed(NkKey::NK_DELETE)) {
                            state.confirmDeletePending=true;
                            state.confirmPermanentDelete=ctx.input.ctrl;
                            ::strncpy(state.confirmDeletePath,e.path,511);
                        }
                    }
                }
        
                if (isTile) { gx=listR.x; gy+=cellH; col=0; }
                else {
                    ++col; gx+=cellW;
                    if (col>=cols) { col=0; gx=listR.x; gy+=cellH; }
                }
            }
        
            if (state.hoveredIdx<0&&NkRectContains(listR,ctx.input.mousePos)&&ctx.ConsumeMouseClick(0))
                state.numSelected=0;
        
            dl.PopClipRect();
        
            // Scrollbar
            if (state.contentH>r.h) {
                const NkRect track={r.x+r.w-kSBW,r.y,kSBW,r.h};
                dl.AddRectFilled(track,ctx.theme.colors.scrollBg,4.f);
                const float32 ratio=r.h/state.contentH;
                const float32 tH=r.h*ratio;
                const float32 tY=r.y+(maxSc>0.f?(state.gridScrollY/maxSc)*(r.h-tH):0.f);
                const NkRect thumb={track.x+2.f,tY,kSBW-4.f,tH};
                const bool hSB=NkRectContains(track,ctx.input.mousePos);
                const NkUIID sbId=NkHashInt(id,0xFB02u);
                const bool aSB=ctx.IsActive(sbId);
                dl.AddRectFilled(thumb,(aSB||hSB)?ctx.theme.colors.scrollThumbHov:ctx.theme.colors.scrollThumb,4.f);
                if (hSB&&ctx.ConsumeMouseClick(0)) ctx.SetActive(sbId);
                if (aSB){ float32 rel=(ctx.input.mousePos.y-r.y-tH*0.5f)/(r.h-tH);
                    state.gridScrollY=(rel<0.f?0.f:rel>1.f?1.f:rel)*maxSc;
                    if (!ctx.input.mouseDown[0]) ctx.ClearActive(); }
            }
            (void)prov;
            return result;
        }
        
        // =============================================================================
        //  DrawStatusBar
        // =============================================================================
        
        void NkUIFileBrowser::DrawStatusBar(NkUIContext& ctx, NkUIDrawList& dl,
                                            NkUIFont& font, const Layout& ly,
                                            const NkUIFileBrowserConfig& cfg,
                                            const NkUIFileBrowserState& state) noexcept
        {
            const NkRect& r=ly.statusBar;
            dl.AddRectFilled(r,ctx.theme.colors.bgHeader);
            dl.AddLine({r.x,r.y},{r.x+r.w,r.y},ctx.theme.colors.separator.WithAlpha(60),1.f);
        
            const float32 ph=ctx.theme.metrics.paddingX;
            const float32 bly=r.y+(r.h-font.metrics.lineHeight)*0.5f+font.metrics.ascender;
        
            char info[160];
            if (state.numSelected==1&&state.selected[0]>=0&&state.selected[0]<state.numEntries) {
                const NkUIFileEntry& e=state.entries[state.selected[0]];
                if (e.type!=NkUIFileType::NK_FT_DIRECTORY) {
                    char sz[32]; FormatSize(e.size,sz,32);
                    ::snprintf(info,sizeof(info),"%s  (%s)",e.name,sz);
                } else ::snprintf(info,sizeof(info),"%s",e.name);
            } else if (state.numSelected>1)
                ::snprintf(info,sizeof(info),"%d \xc3\xa9l\xc3\xa9ments s\xc3\xa9lectionn\xc3\xa9s",state.numSelected);
            else
                ::snprintf(info,sizeof(info),"%d \xc3\xa9l\xc3\xa9ments",state.numEntries);
        
            font.RenderText(dl,{r.x+ph,bly},info,ctx.theme.colors.textSecondary,r.w-ph*2.f,false);
        
            if (cfg.filterExt[0]) {
                const char* fl=cfg.filterLabel[0]?cfg.filterLabel:cfg.filterExt;
                const float32 fw=font.MeasureWidth(fl);
                font.RenderText(dl,{r.x+r.w-fw-ph,bly},fl,ctx.theme.colors.textDisabled,fw+4.f,false);
            }
        }
        
        // =============================================================================
        //  DrawSaveRow
        // =============================================================================
        
        NkUIFileBrowserResult NkUIFileBrowser::DrawSaveRow(
            NkUIContext& ctx, NkUIDrawList& dl, NkUIFont& font,
            NkUIID, const Layout& ly, NkUIFileBrowserConfig& cfg,
            NkUIFileBrowserState& state) noexcept
        {
            NkUIFileBrowserResult result{};
            if (!ly.hasSave) return result;
            const NkRect& r=ly.saveRow;
            const float32 ph=ctx.theme.metrics.paddingX;
            // Libellé "Nom du fichier :"
            const char* lbl="Nom du fichier :";
            const float32 lbW=font.MeasureWidth(lbl)+ph*2.f;
            const float32 bly=r.y+(r.h-font.metrics.lineHeight)*0.5f+font.metrics.ascender;
        
            dl.AddRectFilled(r,ctx.theme.colors.bgHeader);
            dl.AddLine({r.x,r.y},{r.x+r.w,r.y},ctx.theme.colors.separator.WithAlpha(60),1.f);
        
            font.RenderText(dl,{r.x+ph,bly},lbl,ctx.theme.colors.textSecondary,lbW,false);
        
            const NkRect inp={r.x+lbW,r.y+4.f,r.w-lbW-ph,r.h-8.f};
            DrawTextField(ctx,dl,font,inp,state.saveFilenameBuffer,255,true);
        
            // Pré-remplissage si fichier sélectionné
            if (!state.saveFilenameBuffer[0]&&state.numSelected==1) {
                const NkUIFileEntry& e=state.entries[state.selected[0]];
                if (e.type==NkUIFileType::NK_FT_FILE)
                    ::strncpy(state.saveFilenameBuffer,e.name,255);
            }
        
            // Valider avec Enter
            for (int32 k=0;k<ctx.input.numInputChars;++k)
                if (ctx.input.inputChars[k]=='\r'||ctx.input.inputChars[k]=='\n')
                    if (state.saveFilenameBuffer[0]) {
                        char full[512]; ::snprintf(full,sizeof(full),"%s/%s",
                                                state.currentPath,state.saveFilenameBuffer);
                        ::strncpy(result.path,full,511);
                        result.event=NkUIFileBrowserEvent::NK_FB_SAVE_CONFIRMED;
                    }
            (void)cfg;
            return result;
        }
        
        // =============================================================================
        //  DrawConfirmButtons
        // =============================================================================
        
        NkUIFileBrowserResult NkUIFileBrowser::DrawConfirmButtons(
            NkUIContext& ctx, NkUIDrawList& dl, NkUIFont& font,
            const Layout& ly, const NkUIFileBrowserConfig& cfg,
            NkUIFileBrowserState& state) noexcept
        {
            NkUIFileBrowserResult result{};
            if (!cfg.showConfirmButtons) return result;
            const float32 confH=cfg.rowHeight+10.f;
            const NkRect r={ly.statusBar.x,ly.statusBar.y+ly.statusBar.h,ly.statusBar.w,confH};
            dl.AddRectFilled(r,ctx.theme.colors.bgHeader);
            dl.AddLine({r.x,r.y},{r.x+r.w,r.y},ctx.theme.colors.separator.WithAlpha(60),1.f);
        
            const float32 bw=110.f, bh=cfg.rowHeight-2.f, ph=ctx.theme.metrics.paddingX;
            const float32 by=r.y+5.f;
            const float32 asc=font.metrics.ascender;
        
            // ── Annuler ───────────────────────────────────────────────────────────────
            {
                const NkRect btn={r.x+ph,by,bw,bh};
                const bool hov=NkRectContains(btn,ctx.input.mousePos);
                dl.AddRectFilled(btn,hov?ctx.theme.colors.buttonHover:ctx.theme.colors.buttonBg,kBtnR);
                dl.AddRect(btn,ctx.theme.colors.border,1.f,kBtnR);
                const char* lbl="Annuler";
                font.RenderText(dl,{btn.x+(bw-font.MeasureWidth(lbl))*0.5f,
                                    btn.y+(bh-font.metrics.lineHeight)*0.5f+asc},
                            lbl,ctx.theme.colors.textPrimary,bw,false);
                if (hov&&ctx.ConsumeMouseClick(0)) result.event=NkUIFileBrowserEvent::NK_FB_CANCELLED;
            }
        
            // ── OK ─────────────────────────────────────────────────────────────────────
            {
                const NkRect btn={r.x+r.w-bw-ph,by,bw,bh};
                const bool hov=NkRectContains(btn,ctx.input.mousePos);
                dl.AddRectFilled(btn,hov?ctx.theme.colors.accentHover:ctx.theme.colors.accent,kBtnR);
                const char* oklbl=(cfg.mode==NkUIFBMode::NK_FBM_SAVE)?"Enregistrer"
                                :(cfg.mode==NkUIFBMode::NK_FBM_SELECT_DIR)?"S\xc3\xa9lectionner"
                                :"Ouvrir";
                font.RenderText(dl,{btn.x+(bw-font.MeasureWidth(oklbl))*0.5f,
                                    btn.y+(bh-font.metrics.lineHeight)*0.5f+asc},
                            oklbl,ctx.theme.colors.textOnAccent,bw,false);
                if (hov&&ctx.ConsumeMouseClick(0)) {
                    if (cfg.mode==NkUIFBMode::NK_FBM_SAVE&&state.saveFilenameBuffer[0]) {
                        char full[512]; ::snprintf(full,sizeof(full),"%s/%s",
                                                state.currentPath,state.saveFilenameBuffer);
                        ::strncpy(result.path,full,511);
                        result.event=NkUIFileBrowserEvent::NK_FB_SAVE_CONFIRMED;
                    } else if (cfg.mode==NkUIFBMode::NK_FBM_SELECT_DIR) {
                        ::strncpy(result.path,state.currentPath,511);
                        result.event=NkUIFileBrowserEvent::NK_FB_DIR_SELECTED;
                    } else if (cfg.mode==NkUIFBMode::NK_FBM_MULTI) {
                        result.numPaths=0;
                        for (int32 k=0;k<state.numSelected&&k<NkUIFileBrowserResult::MAX_SELECTED;++k)
                            ::strncpy(result.paths[result.numPaths++],
                                    state.entries[state.selected[k]].path,511);
                        if (result.numPaths>0){ ::strncpy(result.path,result.paths[0],511);
                            result.event=NkUIFileBrowserEvent::NK_FB_FILES_SELECTED; }
                    } else {
                        const char* sp=GetSelectedPath(state);
                        if (sp&&sp[0]){ ::strncpy(result.path,sp,511);
                            result.event=NkUIFileBrowserEvent::NK_FB_FILE_SELECTED; }
                    }
                }
            }
            return result;
        }
        
        // =============================================================================
        //  DrawDeleteOverlay
        // =============================================================================
        
        void NkUIFileBrowser::DrawDeleteOverlay(
            NkUIContext& ctx, NkUIDrawList& dl, NkUIFont& font,
            NkRect rect, NkUIFileBrowserState& state,
            NkUIFileBrowserResult& result) noexcept
        {
            if (!state.confirmDeletePending) return;
            dl.AddRectFilled(rect,{0,0,0,145});
        
            const float32 pw=360.f, ph=140.f;
            const NkRect panel={rect.x+(rect.w-pw)*0.5f,rect.y+(rect.h-ph)*0.5f,pw,ph};
            dl.AddRectFilled(panel,ctx.theme.colors.bgPopup,8.f);
            dl.AddRect(panel,ctx.theme.colors.border,1.f,8.f);
        
            const float32 asc=font.metrics.ascender, lh=font.metrics.lineHeight;
            const float32 textX=panel.x+16.f;
        
            const char* title=state.confirmPermanentDelete
                ?"Suppression deinitive":"Deplacer vers la corbeille?";
            font.RenderText(dl,{textX,panel.y+18.f+asc},title,
                        state.confirmPermanentDelete?NkColor{255,80,80,255}:ctx.theme.colors.textPrimary,
                        pw-32.f,false);
        
            // Chemin
            font.RenderText(dl,{textX,panel.y+18.f+lh+8.f+asc},state.confirmDeletePath,
                        ctx.theme.colors.textSecondary,pw-32.f,true);
        
            if (state.confirmPermanentDelete) {
                const char* warn="\xe2\x9a\xa0 Cette action est irr\xc3\xa9versible.";
                font.RenderText(dl,{textX,panel.y+18.f+lh*2.f+14.f+asc},warn,
                            NkColor{255,160,60,220},pw-32.f,false);
            }
        
            const float32 bw=110.f, bh=28.f, bby=panel.y+ph-bh-12.f;
        
            // Non
            {
                const NkRect btn={panel.x+16.f,bby,bw,bh};
                const bool hov=NkRectContains(btn,ctx.input.mousePos);
                dl.AddRectFilled(btn,hov?ctx.theme.colors.buttonHover:ctx.theme.colors.buttonBg,kBtnR);
                dl.AddRect(btn,ctx.theme.colors.border,1.f,kBtnR);
                font.RenderText(dl,{btn.x+(bw-font.MeasureWidth("Non"))*0.5f,
                                    btn.y+(bh-lh)*0.5f+asc},"Non",ctx.theme.colors.textPrimary,bw,false);
                if (hov&&ctx.ConsumeMouseClick(0)) state.confirmDeletePending=false;
            }
            // Oui
            {
                const NkColor okBg=state.confirmPermanentDelete?NkColor{200,40,40,255}:NkColor{180,40,40,255};
                const NkColor okHov=state.confirmPermanentDelete?NkColor{230,60,60,255}:NkColor{210,60,60,255};
                const NkRect btn={panel.x+pw-bw-16.f,bby,bw,bh};
                const bool hov=NkRectContains(btn,ctx.input.mousePos);
                dl.AddRectFilled(btn,hov?okHov:okBg,kBtnR);
                const char* lbl=state.confirmPermanentDelete?"Supprimer":"Corbeille";
                font.RenderText(dl,{btn.x+(bw-font.MeasureWidth(lbl))*0.5f,
                                    btn.y+(bh-lh)*0.5f+asc},lbl,ctx.theme.colors.textOnAccent,bw,false);
                if (hov&&ctx.ConsumeMouseClick(0)) {
                    ::strncpy(result.path,state.confirmDeletePath,511);
                    result.event=state.confirmPermanentDelete
                        ?NkUIFileBrowserEvent::NK_FB_DELETE_PERMANENT
                        :NkUIFileBrowserEvent::NK_FB_DELETE_REQUESTED;
                    state.confirmDeletePending=false;
                }
            }
        }
        
        // =============================================================================
        //  DrawDndGhost
        // =============================================================================
        
        void NkUIFileBrowser::DrawDndGhost(NkUIDrawList& dl, NkUIFont& font,
                                            const NkUIContext& ctx,
                                            const NkUIFileBrowserState& state) noexcept
        {
            if (!state.dndActive||state.dndSourceIdx<0||
                state.dndSourceIdx>=state.numEntries) return;
            const NkUIFileEntry& src=state.entries[state.dndSourceIdx];
            const NkRect ghost={ctx.input.mousePos.x+14.f,ctx.input.mousePos.y-12.f,200.f,26.f};
            dl.AddRectFilled(ghost,ctx.theme.colors.bgWindow.WithAlpha(230),4.f);
            dl.AddRect(ghost,ctx.theme.colors.accent,1.5f,4.f);
            const NkRect ir={ghost.x+3.f,ghost.y+3.f,20.f,20.f};
            DrawEntryIcon(dl,ir,src);
            font.RenderText(dl,{ghost.x+26.f,ghost.y+(ghost.h-font.metrics.lineHeight)*0.5f+font.metrics.ascender},
                        src.name,ctx.theme.colors.textPrimary,ghost.w-30.f,true);
        }
        
        // =============================================================================
        //  NkUIFileBrowser::Draw — point d'entrée principal
        // =============================================================================
        
        NkUIFileBrowserResult NkUIFileBrowser::Draw(
            NkUIContext& ctx, NkUIDrawList& dl, NkUIFont& font,
            NkUIID id, NkRect rect,
            NkUIFileBrowserConfig& cfg,
            NkUIFileBrowserState& state,
            const NkUIFSProvider& provider) noexcept
        {
            NkUIFileBrowserResult result{};
        
            // Init
            if (!state.currentPath[0]) {
                ::strncpy(state.currentPath,cfg.rootPath,511);
                state.needsRefresh=true;
            }
            if (cfg.mode==NkUIFBMode::NK_FBM_SAVE&&!state.saveFilenameBuffer[0]&&cfg.saveFilename[0])
                ::strncpy(state.saveFilenameBuffer,cfg.saveFilename,255);
            if (state.needsRefresh) Refresh(state,provider,cfg);
        
            DrawBackground(dl,rect,ctx);
            dl.PushClipRect(rect,true);
        
            const Layout ly=ComputeLayout(rect,cfg);
        
            DrawToolbar(ctx,dl,font,ly,cfg,state,provider);
            DrawBreadcrumb(ctx,dl,font,ly,state);
        
            if (ly.hasBookmarks)
                DrawBookmarkPanel(ctx,dl,font,ly,cfg,state);
        
            DrawColumnHeaders(ctx,dl,font,ly,cfg,state);
        
            // Vue active
            {
                NkUIFileBrowserResult vr;
                if (cfg.viewMode==NkUIFBViewMode::NK_FBVM_LIST)
                    vr=DrawListView(ctx,dl,font,id,ly,cfg,state,provider);
                else
                    vr=DrawGridView(ctx,dl,font,id,ly,cfg,state,provider);
                if (vr.event!=NkUIFileBrowserEvent::NK_FB_NONE) result=vr;
            }
        
            if (ly.hasSave) {
                NkUIFileBrowserResult sr=DrawSaveRow(ctx,dl,font,id,ly,cfg,state);
                if (sr.event!=NkUIFileBrowserEvent::NK_FB_NONE) result=sr;
            }
        
            DrawStatusBar(ctx,dl,font,ly,cfg,state);
        
            if (cfg.showConfirmButtons) {
                NkUIFileBrowserResult cr=DrawConfirmButtons(ctx,dl,font,ly,cfg,state);
                if (cr.event!=NkUIFileBrowserEvent::NK_FB_NONE) result=cr;
            }
        
            DrawDeleteOverlay(ctx,dl,font,rect,state,result);
        
            if (state.dndActive) DrawDndGhost(dl,font,ctx,state);
        
            dl.PopClipRect();
            return result;
        }

        // =============================================================================
        //  ContentBrowser — utilitaires
        // =============================================================================

        NkUIAssetType NkUIContentBrowser::GuessAssetType(const char* filename) noexcept {
            const char* dot=::strrchr(filename,'.');
            if (!dot) return NkUIAssetType::NK_ASSET_UNKNOWN;
            const char* x=dot+1;
            if (!::strcasecmp(x,"png")||!::strcasecmp(x,"jpg")||!::strcasecmp(x,"jpeg")||
                !::strcasecmp(x,"bmp")||!::strcasecmp(x,"tga")||!::strcasecmp(x,"dds")||
                !::strcasecmp(x,"hdr")||!::strcasecmp(x,"exr")||!::strcasecmp(x,"gif")||
                !::strcasecmp(x,"webp"))
                return NkUIAssetType::NK_ASSET_TEXTURE;
            if (!::strcasecmp(x,"obj")||!::strcasecmp(x,"fbx")||!::strcasecmp(x,"gltf")||
                !::strcasecmp(x,"glb")||!::strcasecmp(x,"blend")||!::strcasecmp(x,"mesh")||
                !::strcasecmp(x,"dae")||!::strcasecmp(x,"3ds"))
                return NkUIAssetType::NK_ASSET_MESH;
            if (!::strcasecmp(x,"mp3")||!::strcasecmp(x,"wav")||!::strcasecmp(x,"ogg")||
                !::strcasecmp(x,"flac")||!::strcasecmp(x,"m4a")||!::strcasecmp(x,"aiff"))
                return NkUIAssetType::NK_ASSET_AUDIO;
            if (!::strcasecmp(x,"mat")||!::strcasecmp(x,"material")||!::strcasecmp(x,"glsl")||
                !::strcasecmp(x,"hlsl")||!::strcasecmp(x,"wgsl")||!::strcasecmp(x,"vert")||
                !::strcasecmp(x,"frag")||!::strcasecmp(x,"shader"))
                return NkUIAssetType::NK_ASSET_MATERIAL;
            if (!::strcasecmp(x,"scene")||!::strcasecmp(x,"level")||!::strcasecmp(x,"world")||
                !::strcasecmp(x,"map"))
                return NkUIAssetType::NK_ASSET_SCENE;
            if (!::strcasecmp(x,"lua")||!::strcasecmp(x,"py")||!::strcasecmp(x,"cs")||
                !::strcasecmp(x,"js")||!::strcasecmp(x,"ts")||!::strcasecmp(x,"cpp")||
                !::strcasecmp(x,"script"))
                return NkUIAssetType::NK_ASSET_SCRIPT;
            if (!::strcasecmp(x,"ttf")||!::strcasecmp(x,"otf")||!::strcasecmp(x,"woff")||
                !::strcasecmp(x,"woff2")||!::strcasecmp(x,"font"))
                return NkUIAssetType::NK_ASSET_FONT;
            if (!::strcasecmp(x,"anim")||!::strcasecmp(x,"animation")||!::strcasecmp(x,"anm"))
                return NkUIAssetType::NK_ASSET_ANIMATION;
            if (!::strcasecmp(x,"prefab")||!::strcasecmp(x,"pfb"))
                return NkUIAssetType::NK_ASSET_PREFAB;
            return NkUIAssetType::NK_ASSET_UNKNOWN;
        }

        const char* NkUIContentBrowser::AssetTypeLabel(NkUIAssetType t) noexcept {
            switch(t) {
                case NkUIAssetType::NK_ASSET_TEXTURE:   return "Texture";
                case NkUIAssetType::NK_ASSET_MESH:      return "Mesh";
                case NkUIAssetType::NK_ASSET_AUDIO:     return "Audio";
                case NkUIAssetType::NK_ASSET_MATERIAL:  return "Material";
                case NkUIAssetType::NK_ASSET_SCENE:     return "Scene";
                case NkUIAssetType::NK_ASSET_SCRIPT:    return "Script";
                case NkUIAssetType::NK_ASSET_FONT:      return "Font";
                case NkUIAssetType::NK_ASSET_ANIMATION: return "Animation";
                case NkUIAssetType::NK_ASSET_PREFAB:    return "Prefab";
                case NkUIAssetType::NK_ASSET_FOLDER:    return "Folder";
                default: return "Asset";
            }
        }

        NkColor NkUIContentBrowser::AssetTypeColor(NkUIAssetType t) noexcept {
            switch(t) {
                case NkUIAssetType::NK_ASSET_TEXTURE:   return {70,170,255,255};
                case NkUIAssetType::NK_ASSET_MESH:      return {255,175,80,255};
                case NkUIAssetType::NK_ASSET_AUDIO:     return {200,90,255,255};
                case NkUIAssetType::NK_ASSET_MATERIAL:  return {100,230,140,255};
                case NkUIAssetType::NK_ASSET_SCENE:     return {80,220,220,255};
                case NkUIAssetType::NK_ASSET_SCRIPT:    return {255,215,80,255};
                case NkUIAssetType::NK_ASSET_FONT:      return {140,200,255,255};
                case NkUIAssetType::NK_ASSET_ANIMATION: return {255,140,200,255};
                case NkUIAssetType::NK_ASSET_PREFAB:    return {200,160,255,255};
                case NkUIAssetType::NK_ASSET_FOLDER:    return {255,200,50,255};
                default: return {160,160,160,255};
            }
        }

        // =============================================================================
        //  DrawAssetTypeIcon — icône vectorielle par type
        // =============================================================================

        void NkUIContentBrowser::DrawAssetTypeIcon(NkUIDrawList& dl,NkRect r,
                                                    NkUIAssetType t,bool large) noexcept {
            const NkColor col=AssetTypeColor(t);
            const float32 cx=r.x+r.w*0.5f, cy=r.y+r.h*0.5f;
            const float32 sz=::fminf(r.w,r.h)*(large?0.60f:0.70f);
            const float32 hs=sz*0.5f;

            switch(t) {
                case NkUIAssetType::NK_ASSET_TEXTURE: {
                    // Cadre image avec montagne
                    dl.AddRect({cx-hs,cy-hs,sz,sz},col,1.5f,2.f);
                    dl.AddTriangleFilled({cx-hs*0.5f,cy+hs*0.3f},{cx,cy-hs*0.2f},{cx+hs*0.5f,cy+hs*0.3f},
                                        col.WithAlpha(160));
                    dl.AddCircle({cx+hs*0.4f,cy-hs*0.3f},hs*0.22f,col,1.f,8);
                } break;
                case NkUIAssetType::NK_ASSET_MESH: {
                    // Cube wireframe simplifié
                    const float32 s=hs*0.7f, off=hs*0.22f;
                    dl.AddRect({cx-s,cy-s+off,s*2.f,s*2.f},col,1.5f);
                    dl.AddRect({cx-s+off,cy-s-off,s*2.f,s*2.f},col,1.f);
                    dl.AddLine({cx-s,cy-s+off},{cx-s+off,cy-s-off},col,1.f);
                    dl.AddLine({cx+s,cy-s+off},{cx+s+off,cy-s-off},col,1.f);
                    dl.AddLine({cx+s,cy+s+off},{cx+s+off,cy+s-off},col,1.f);
                } break;
                case NkUIAssetType::NK_ASSET_AUDIO: {
                    // Onde sonore
                    for (int32 i=0;i<5;++i) {
                        const float32 a=(i-2)*hs*0.32f+cx;
                        const float32 h2 = hs * (0.30f + math::NkAbs(i-2) * 0.15f);
                        dl.AddLine({a,cy-h2},{a,cy+h2},col,2.f);
                    }
                } break;
                case NkUIAssetType::NK_ASSET_MATERIAL: {
                    // Sphère stylisée (cercle + bande de reflet)
                    dl.AddCircleFilled({cx,cy},hs*0.85f,col.WithAlpha(180),16);
                    dl.AddCircle({cx,cy},hs*0.85f,col,1.5f,16);
                    // Reflet
                    dl.AddArc({cx-hs*0.18f,cy-hs*0.25f},hs*0.48f,
                            NKUI_PI*0.95f,NKUI_PI*1.55f,
                            {255,255,255,120},2.5f,8);
                } break;
                case NkUIAssetType::NK_ASSET_SCENE: {
                    // Cadre + cible (scène)
                    dl.AddCircle({cx,cy},hs*0.80f,col,1.5f,16);
                    dl.AddLine({cx-hs,cy},{cx+hs,cy},col.WithAlpha(100),1.f);
                    dl.AddLine({cx,cy-hs},{cx,cy+hs},col.WithAlpha(100),1.f);
                    dl.AddCircleFilled({cx,cy},hs*0.22f,col,8);
                } break;
                case NkUIAssetType::NK_ASSET_SCRIPT: {
                    // Accolades { }
                    const float32 sw=hs*0.18f, sh=hs*0.65f;
                    // {
                    dl.AddLine({cx-hs*0.5f+sw,cy-sh},{cx-hs*0.5f,cy-sh*0.5f},col,1.8f);
                    dl.AddLine({cx-hs*0.5f,cy-sh*0.5f},{cx-hs*0.7f,cy},col,1.8f);
                    dl.AddLine({cx-hs*0.7f,cy},{cx-hs*0.5f,cy+sh*0.5f},col,1.8f);
                    dl.AddLine({cx-hs*0.5f,cy+sh*0.5f},{cx-hs*0.5f+sw,cy+sh},col,1.8f);
                    // }
                    dl.AddLine({cx+hs*0.5f-sw,cy-sh},{cx+hs*0.5f,cy-sh*0.5f},col,1.8f);
                    dl.AddLine({cx+hs*0.5f,cy-sh*0.5f},{cx+hs*0.7f,cy},col,1.8f);
                    dl.AddLine({cx+hs*0.7f,cy},{cx+hs*0.5f,cy+sh*0.5f},col,1.8f);
                    dl.AddLine({cx+hs*0.5f,cy+sh*0.5f},{cx+hs*0.5f-sw,cy+sh},col,1.8f);
                } break;
                case NkUIAssetType::NK_ASSET_FONT: {
                    // Lettre A stylisée
                    dl.AddTriangleFilled({cx,cy-hs*0.75f},{cx-hs*0.55f,cy+hs*0.65f},
                                        {cx+hs*0.55f,cy+hs*0.65f},col.WithAlpha(50));
                    dl.AddLine({cx,cy-hs*0.75f},{cx-hs*0.55f,cy+hs*0.65f},col,2.f);
                    dl.AddLine({cx,cy-hs*0.75f},{cx+hs*0.55f,cy+hs*0.65f},col,2.f);
                    dl.AddLine({cx-hs*0.25f,cy+hs*0.10f},{cx+hs*0.25f,cy+hs*0.10f},col,2.f);
                } break;
                case NkUIAssetType::NK_ASSET_ANIMATION: {
                    // Courbe de Bézier + points de contrôle
                    for (int32 i=0;i<4;++i) {
                        const float32 px=cx-hs*0.8f+i*hs*0.53f;
                        const float32 py=cy+(i%2==0?-hs*0.4f:hs*0.4f);
                        dl.AddCircleFilled({px,py},hs*0.14f,col,6);
                        if (i>0) {
                            const float32 ppx=cx-hs*0.8f+(i-1)*hs*0.53f;
                            const float32 ppy=cy+((i-1)%2==0?-hs*0.4f:hs*0.4f);
                            dl.AddLine({ppx,ppy},{px,py},col.WithAlpha(120),1.5f);
                        }
                    }
                } break;
                case NkUIAssetType::NK_ASSET_PREFAB: {
                    // Hexagone (composant)
                    const float32 hr=hs*0.80f;
                    for (int32 i=0;i<6;++i) {
                        const float32 a0=NKUI_PI/6.f+i*NKUI_PI/3.f;
                        const float32 a1=NKUI_PI/6.f+(i+1)*NKUI_PI/3.f;
                        dl.AddLine({cx+hr*::cosf(a0),cy+hr*::sinf(a0)},
                                {cx+hr*::cosf(a1),cy+hr*::sinf(a1)},col,2.f);
                    }
                    dl.AddCircleFilled({cx,cy},hr*0.35f,col.WithAlpha(200),8);
                } break;
                case NkUIAssetType::NK_ASSET_FOLDER:
                    NkUIFileBrowser::DrawIconFolder(dl,r,col);
                    break;
                default: NkUIFileBrowser::DrawIconFile(dl,r,col); break;
            }
        }

        // =============================================================================
        //  LoadFolder
        // =============================================================================

        void NkUIContentBrowser::LoadFolder(NkUIContentBrowserState& state,
                                            const char* path,
                                            const NkUIFSProvider& prov) noexcept {
            if (!prov.list||!path||!path[0]) return;
            ::strncpy(state.currentPath,path,511);
            NkUIFileEntry entries[NkUIContentBrowserState::MAX_ASSETS];
            const int32 n=prov.list(path,entries,NkUIContentBrowserState::MAX_ASSETS,prov.userData);
            state.numAssets=0;
            for (int32 i=0;i<n&&state.numAssets<NkUIContentBrowserState::MAX_ASSETS;++i) {
                NkUIAssetEntry& a=state.assets[state.numAssets++];
                ::strncpy(a.name,entries[i].name,127);
                ::strncpy(a.path,entries[i].path,511);
                a.size=entries[i].size;
                a.thumbnailTexId=0;
                a.isFolder=(entries[i].type==NkUIFileType::NK_FT_DIRECTORY);
                a.type=a.isFolder?NkUIAssetType::NK_ASSET_FOLDER:GuessAssetType(entries[i].name);
                a.hasUnsavedChanges=false;
            }
            state.needsRefresh=false;
            state.scrollY=0.f;
            state.numSelected=0;
        }

        // =============================================================================
        //  BuildTree
        // =============================================================================

        void NkUIContentBrowser::BuildTree(NkUIContentBrowserState& state,
                                            const char* rootPath,
                                            const NkUIFSProvider& prov) noexcept {
            if (!prov.list) return;
            state.numTreeNodes=0;

            // Nœud racine
            auto& root=state.treeNodes[state.numTreeNodes++];
            ::strncpy(root.name,"Contenu",127);
            ::strncpy(root.path,rootPath,511);
            root.parent=-1; root.expanded=true; root.hasChildren=false;

            // Lister un niveau pour vérifier les enfants
            NkUIFileEntry entries[256];
            const int32 n=prov.list(rootPath,entries,256,prov.userData);
            for (int32 i=0;i<n&&state.numTreeNodes<NkUIContentBrowserState::MAX_TREE_NODES;++i) {
                if (entries[i].type!=NkUIFileType::NK_FT_DIRECTORY) continue;
                root.hasChildren=true;
                auto& node=state.treeNodes[state.numTreeNodes++];
                ::strncpy(node.name,entries[i].name,127);
                ::strncpy(node.path,entries[i].path,511);
                node.parent=0; node.expanded=false; node.hasChildren=false;

                // Vérifier si ce sous-dossier a des enfants (un niveau seulement)
                NkUIFileEntry sub[32];
                const int32 sn=prov.list(entries[i].path,sub,32,prov.userData);
                for (int32 j=0;j<sn;++j)
                    if (sub[j].type==NkUIFileType::NK_FT_DIRECTORY){ node.hasChildren=true; break; }
            }
        }

        // =============================================================================
        //  DrawTree
        // =============================================================================

        void NkUIContentBrowser::DrawTree(NkUIContext& ctx, NkUIDrawList& dl,
                                        NkUIFont& font, NkRect rect,
                                        NkUIContentBrowserConfig& cfg,
                                        NkUIContentBrowserState& state,
                                        const NkUIFSProvider& prov,
                                        NkUIContentBrowserResult& result) noexcept
        {
            dl.AddRectFilled(rect,ctx.theme.colors.bgSecondary);
            dl.AddLine({rect.x+rect.w,rect.y},{rect.x+rect.w,rect.y+rect.h},
                    ctx.theme.colors.separator.WithAlpha(80),1.f);

            const float32 rowH=22.f, ph=ctx.theme.metrics.paddingX, asc=font.metrics.ascender;
            float32 y=rect.y+2.f;
            dl.PushClipRect(rect,true);

            for (int32 i=0;i<state.numTreeNodes;++i) {
                auto& node=state.treeNodes[i];
                // Indentation selon la profondeur
                const float32 depth=(node.parent<0)?0.f:
                    (state.treeNodes[node.parent].parent<0)?1.f:2.f;
                const float32 indent=depth*12.f;
                const NkRect row={rect.x,y,rect.w,rowH};
                const bool hov=NkRectContains(row,ctx.input.mousePos);
                const bool active=(state.selectedTreeNode==i);

                if      (active) dl.AddRectFilled(row,ctx.theme.colors.accent.WithAlpha(65),3.f);
                else if (hov)    dl.AddRectFilled(row,ctx.theme.colors.buttonHover,3.f);

                // Triangle expand
                if (node.hasChildren) {
                    const float32 tx=rect.x+indent+3.f, ty2=y+(rowH-10.f)*0.5f;
                    if (node.expanded)
                        dl.AddTriangleFilled({tx,ty2},{tx+8.f,ty2},{tx+4.f,ty2+7.f},
                                            active?ctx.theme.colors.textOnAccent:ctx.theme.colors.textSecondary);
                    else
                        dl.AddTriangleFilled({tx,ty2},{tx,ty2+8.f},{tx+7.f,ty2+4.f},
                                            active?ctx.theme.colors.textOnAccent:ctx.theme.colors.textSecondary);
                }

                // Icône dossier
                const NkRect ico={rect.x+indent+12.f,y+(rowH-14.f)*0.5f,14.f,14.f};
                NkUIFileBrowser::DrawIconFolder(dl,ico,
                    active?ctx.theme.colors.textOnAccent:NkColor{255,205,60,220});

                font.RenderText(dl,{rect.x+indent+28.f,y+(rowH-font.metrics.lineHeight)*0.5f+asc},
                            node.name,
                            active?ctx.theme.colors.textOnAccent:ctx.theme.colors.textSecondary,
                            rect.w-indent-32.f,true);

                if (hov&&ctx.ConsumeMouseClick(0)) {
                    state.selectedTreeNode=i;
                    // Expand/collapse
                    if (node.hasChildren) node.expanded=!node.expanded;
                    // Charger le dossier
                    LoadFolder(state,node.path,prov);
                    result.event=NkUIContentBrowserEvent::NK_CB_FOLDER_CHANGED;
                    ::strncpy(result.path,node.path,511);
                }
                y+=rowH;
            }
            dl.PopClipRect();
        }

        // =============================================================================
        //  DrawToolbar (ContentBrowser)
        // =============================================================================

        void NkUIContentBrowser::DrawToolbar(NkUIContext& ctx, NkUIDrawList& dl,
                                            NkUIFont& font, NkRect rect,
                                            NkUIContentBrowserConfig& cfg,
                                            NkUIContentBrowserState& state,
                                            const NkUIFSProvider& prov) noexcept
        {
            dl.AddRectFilled(rect,ctx.theme.colors.bgHeader);
            dl.AddLine({rect.x,rect.y+rect.h},{rect.x+rect.w,rect.y+rect.h},
                    ctx.theme.colors.separator.WithAlpha(80),1.f);

            const float32 ph=6.f, bh=rect.h-6.f, bw=bh;
            float32 cx=rect.x+ph;
            const float32 cy=rect.y+3.f;

            auto iconBtn=[&](float32 w,bool en,auto drawFn)->bool {
                const NkRect btn={cx,cy,w,bh};
                const bool hov=en&&NkRectContains(btn,ctx.input.mousePos);
                dl.AddRectFilled(btn,hov?ctx.theme.colors.buttonHover:ctx.theme.colors.bgSecondary.WithAlpha(70),kBtnR);
                dl.AddRect(btn,ctx.theme.colors.border.WithAlpha(50),1.f,kBtnR);
                drawFn(NkRect{btn.x+2.f,btn.y+2.f,btn.w-4.f,btn.h-4.f},
                    en?ctx.theme.colors.textPrimary:ctx.theme.colors.textDisabled,hov);
                cx+=w+kBtnGap;
                return hov&&ctx.ConsumeMouseClick(0)&&en;
            };

            // Refresh
            if (iconBtn(bw,true,[&](NkRect ir,NkColor c,bool){
                    NkUIFileBrowser::DrawIconRefresh(dl,ir,c);})) {
                if (state.currentPath[0]) LoadFolder(state,state.currentPath,prov);
            }

            cx+=4.f;

            // Séparateur puis slider de taille des tuiles
            dl.AddLine({cx,cy+4.f},{cx,cy+bh-4.f},ctx.theme.colors.separator.WithAlpha(60),1.f);
            cx+=6.f;

            // Label "Taille :"
            {
                const char* lbl="Taille";
                const float32 lw=font.MeasureWidth(lbl);
                font.RenderText(dl,{cx,cy+(bh-font.metrics.lineHeight)*0.5f+font.metrics.ascender},
                            lbl,ctx.theme.colors.textSecondary,lw,false);
                cx+=lw+4.f;
            }

            // Slider miniature
            {
                const float32 sw=80.f, sh=4.f, sy=cy+(bh-sh)*0.5f;
                const NkRect track={cx,sy,sw,sh};
                dl.AddRectFilled(track,ctx.theme.colors.scrollBg,2.f);
                const float32 ratio=(state.thumbnailSizeCurrent-cfg.minThumbnailSize)
                                /(cfg.maxThumbnailSize-cfg.minThumbnailSize);
                const float32 tx=track.x+ratio*(sw-10.f);
                const NkRect thumb={tx,sy-4.f,10.f,sh+8.f};
                const bool ht=NkRectContains(thumb,ctx.input.mousePos);
                dl.AddRectFilled(thumb,ht?ctx.theme.colors.accent:ctx.theme.colors.scrollThumb,4.f);
                const NkUIID slId=NkHash("cb_thumb_slider");
                if (ht&&ctx.ConsumeMouseClick(0)) ctx.SetActive(slId);
                if (ctx.IsActive(slId)) {
                    float32 rel=(ctx.input.mousePos.x-track.x)/(sw-10.f);
                    if (rel<0.f) rel=0.f; if (rel>1.f) rel=1.f;
                    state.thumbnailSizeCurrent=cfg.minThumbnailSize
                        +rel*(cfg.maxThumbnailSize-cfg.minThumbnailSize);
                    if (!ctx.input.mouseDown[0]) ctx.ClearActive();
                }
                cx+=sw+8.f;
            }

            // Barre de recherche
            const float32 sw=rect.x+rect.w-cx-ph;
            if (sw>60.f) {
                const NkRect sr={cx,cy,sw,bh};
                const bool foc=state.searchFocused;
                dl.AddRectFilled(sr,ctx.theme.colors.inputBg,kBtnR);
                dl.AddRect(sr,foc?ctx.theme.colors.accent:ctx.theme.colors.border,foc?1.8f:1.f,kBtnR);
                const NkRect li={sr.x+2.f,sr.y+2.f,bh-4.f,bh-4.f};
                NkUIFileBrowser::DrawIconSearch(dl,li,ctx.theme.colors.textDisabled);
                const float32 textX=sr.x+bh+2.f;
                const float32 bly=sr.y+(bh-font.metrics.lineHeight)*0.5f+font.metrics.ascender;
                if (state.searchBuffer[0])
                    font.RenderText(dl,{textX,bly},state.searchBuffer,ctx.theme.colors.inputText,sw-bh-10.f,false);
                else if (!foc)
                    font.RenderText(dl,{textX,bly},"Rechercher assets...",ctx.theme.colors.textDisabled,sw-bh-10.f,false);
                if (NkRectContains(sr,ctx.input.mousePos)&&ctx.ConsumeMouseClick(0)) state.searchFocused=true;
                if (!NkRectContains(sr,ctx.input.mousePos)&&ctx.input.IsMouseClicked(0)) state.searchFocused=false;
                if (state.searchFocused) {
                    int32 sbl=static_cast<int32>(::strlen(state.searchBuffer));
                    for (int32 k=0;k<ctx.input.numInputChars;++k) {
                        const uint32 cp=ctx.input.inputChars[k];
                        if (cp=='\b') sbl=Utf8BackSpace(state.searchBuffer,sbl);
                        else if (cp==27u) state.searchFocused=false;
                        else if (cp>=32u) { const int32 nl=Utf8Append(state.searchBuffer,sbl,127,cp); if (nl>=0) sbl=nl; }
                    }
                }
            }
            (void)cfg;
        }

        // =============================================================================
        //  DrawAssetTile
        // =============================================================================

        void NkUIContentBrowser::DrawAssetTile(NkUIContext& ctx, NkUIDrawList& dl,
                                                NkUIFont& font, NkRect cell,
                                                int32 idx, bool selected, bool hovered,
                                                float32 thumbSize,
                                                NkUIContentBrowserState& state,
                                                NkUIContentBrowserResult& result) noexcept
        {
            NkUIAssetEntry& a=state.assets[idx];
            const NkColor acol=AssetTypeColor(a.type);

            // Fond cellule
            if      (selected) dl.AddRectFilled(cell,ctx.theme.colors.accent.WithAlpha(80),6.f);
            else if (hovered)  dl.AddRectFilled(cell,ctx.theme.colors.buttonHover.WithAlpha(65),6.f);
            dl.AddRect(cell,ctx.theme.colors.separator.WithAlpha(28),1.f,6.f);

            // Zone miniature
            const float32 padH=6.f, padV=6.f;
            const float32 thumbH=thumbSize*0.72f;
            const NkRect thumbR={cell.x+padH,cell.y+padV,cell.w-padH*2.f,thumbH};

            if (a.thumbnailTexId>0) {
                // Miniature GPU (si disponible)
                dl.AddImageRounded(a.thumbnailTexId,thumbR,4.f);
            } else {
                // Fond coloré + icône type
                dl.AddRectFilled(thumbR,acol.WithAlpha(32),4.f);
                dl.AddRect(thumbR,acol.WithAlpha(80),1.f,4.f);

                const float32 isz=thumbH*0.55f;
                const NkRect ir={thumbR.x+(thumbR.w-isz)*0.5f,
                                thumbR.y+(thumbR.h-isz)*0.5f,isz,isz};
                DrawAssetTypeIcon(dl,ir,a.type,true);
            }

            // Badge type d'asset (coin haut-droite)
            {
                const char* lbl=AssetTypeLabel(a.type);
                const float32 lw=font.MeasureWidth(lbl);
                const NkRect badge={thumbR.x+thumbR.w-lw-8.f,thumbR.y+3.f,lw+6.f,14.f};
                dl.AddRectFilled(badge,acol.WithAlpha(220),3.f);
                font.RenderText(dl,{badge.x+3.f,badge.y+font.metrics.ascender},lbl,
                            {255,255,255,230},lw+4.f,false);
            }

            // Indicateur "non sauvegardé"
            if (a.hasUnsavedChanges)
                dl.AddCircleFilled({thumbR.x+thumbR.w-6.f,thumbR.y+thumbR.h-6.f},5.f,
                                {255,200,50,240},8);

            // Nom sous la miniature
            const float32 nameY=cell.y+padV+thumbH+3.f;
            const float32 nameH=cell.h-padV-thumbH-6.f;
            const float32 asc=font.metrics.ascender;

            // Centrage du nom
            const float32 tw=font.MeasureWidth(a.name);
            const float32 nameX=cell.x+(cell.w-tw)*0.5f;
            font.RenderText(dl,{nameX,nameY+(nameH-font.metrics.lineHeight)*0.5f+asc},
                        a.name,
                        selected?ctx.theme.colors.textOnAccent:ctx.theme.colors.textPrimary,
                        cell.w-4.f,true);

            // Interactions
            if (hovered) {
                if (ctx.input.IsMouseClicked(0)) {
                    const bool ctrl=ctx.input.ctrl;
                    if (ctrl) {
                        bool found=false;
                        for (int32 k=0;k<state.numSelected;++k) if (state.selected[k]==idx){
                            for (int32 j=k;j<state.numSelected-1;++j) state.selected[j]=state.selected[j+1];
                            --state.numSelected; found=true; break;}
                        if (!found&&state.numSelected<NkUIContentBrowserState::MAX_SEL)
                            state.selected[state.numSelected++]=idx;
                    } else { state.numSelected=1; state.selected[0]=idx; }
                    result.event=NkUIContentBrowserEvent::NK_CB_ASSET_SELECTED;
                    ::strncpy(result.path,a.path,511);
                }
                if (ctx.input.mouseDblClick[0]) {
                    if (a.isFolder) {
                        result.event=NkUIContentBrowserEvent::NK_CB_FOLDER_CHANGED;
                        ::strncpy(result.path,a.path,511);
                    } else {
                        result.event=NkUIContentBrowserEvent::NK_CB_ASSET_DOUBLE_CLICKED;
                        ::strncpy(result.path,a.path,511);
                    }
                }
                // DnD
                if (!state.dndActive&&ctx.input.IsMouseDown(0)) {
                    const NkVec2 d=ctx.input.mouseDelta;
                    if (d.x*d.x+d.y*d.y>25.f){state.dndActive=true;state.dndSourceIdx=idx;}
                }
            }
        }

        // =============================================================================
        //  DrawAssetGrid
        // =============================================================================

        NkUIContentBrowserResult NkUIContentBrowser::DrawAssetGrid(
            NkUIContext& ctx, NkUIDrawList& dl, NkUIFont& font,
            NkUIID id, NkRect rect,
            NkUIContentBrowserConfig& cfg,
            NkUIContentBrowserState& state) noexcept
        {
            NkUIContentBrowserResult result{};
            const float32 thumbSz=state.thumbnailSizeCurrent;
            const float32 cellW=thumbSz+12.f;
            const float32 cellH=thumbSz*0.72f+font.metrics.lineHeight+20.f;
            const int32 cols=(int32)::fmaxf(1.f,::floorf((rect.w-kSBW)/cellW));

            // Comptage visible
            int32 vis=0;
            for (int32 i=0;i<state.numAssets;++i) {
                if (state.searchBuffer[0]&&!Utf8ContainsI(state.assets[i].name,state.searchBuffer)) continue;
                ++vis;
            }
            const int32 rows=(vis+cols-1)/cols;
            const float32 totalH=(float32)rows*cellH;

            // Scroll
            if (NkRectContains(rect,ctx.input.mousePos)&&!ctx.wheelConsumed&&ctx.input.mouseWheel!=0.f) {
                state.scrollY-=ctx.input.mouseWheel*cellH; ctx.wheelConsumed=true;
            }
            const float32 maxSc=(totalH>rect.h)?totalH-rect.h:0.f;
            if (state.scrollY<0.f) state.scrollY=0.f;
            if (state.scrollY>maxSc) state.scrollY=maxSc;

            const NkRect listR={rect.x,rect.y,rect.w-kSBW,rect.h};
            dl.PushClipRect(listR,true);
            dl.AddRectFilled(listR,ctx.theme.colors.bgWindow);

            float32 gx=listR.x+4.f, gy=listR.y+4.f-state.scrollY;
            int32 col=0; state.hoveredIdx=-1;

            for (int32 i=0;i<state.numAssets;++i) {
                if (state.searchBuffer[0]&&!Utf8ContainsI(state.assets[i].name,state.searchBuffer)) continue;

                const NkRect cell={gx,gy,cellW,cellH};
                if (gy+cellH>=listR.y&&gy<=listR.y+listR.h) {
                    const bool hov=NkRectContains(cell,ctx.input.mousePos);
                    if (hov) state.hoveredIdx=i;
                    bool sel=false;
                    for (int32 k=0;k<state.numSelected;++k) if (state.selected[k]==i){sel=true;break;}
                    DrawAssetTile(ctx,dl,font,cell,i,sel,hov,thumbSz,state,result);
                }

                ++col; gx+=cellW;
                if (col>=cols){ col=0; gx=listR.x+4.f; gy+=cellH; }
            }

            if (state.hoveredIdx<0&&NkRectContains(listR,ctx.input.mousePos)&&ctx.ConsumeMouseClick(0))
                state.numSelected=0;

            dl.PopClipRect();

            // Scrollbar
            if (totalH>rect.h) {
                const NkRect track={rect.x+rect.w-kSBW,rect.y,kSBW,rect.h};
                dl.AddRectFilled(track,ctx.theme.colors.scrollBg,4.f);
                const float32 ratio=rect.h/totalH;
                const float32 tH=rect.h*ratio;
                const float32 tY=rect.y+(maxSc>0.f?(state.scrollY/maxSc)*(rect.h-tH):0.f);
                const NkRect thumb={track.x+2.f,tY,kSBW-4.f,tH};
                const bool hSB=NkRectContains(track,ctx.input.mousePos);
                const NkUIID sbId=NkHashInt(id,0xCB01u);
                const bool aSB=ctx.IsActive(sbId);
                dl.AddRectFilled(thumb,(aSB||hSB)?ctx.theme.colors.scrollThumbHov:ctx.theme.colors.scrollThumb,4.f);
                if (hSB&&ctx.ConsumeMouseClick(0)) ctx.SetActive(sbId);
                if (aSB){ float32 rel=(ctx.input.mousePos.y-rect.y-tH*0.5f)/(rect.h-tH);
                    state.scrollY=(rel<0.f?0.f:rel>1.f?1.f:rel)*maxSc;
                    if (!ctx.input.mouseDown[0]) ctx.ClearActive(); }
            }
            (void)cfg;
            return result;
        }

        // =============================================================================
        //  NkUIContentBrowser::Draw — point d'entrée
        // =============================================================================

        NkUIContentBrowserResult NkUIContentBrowser::Draw(
            NkUIContext& ctx, NkUIDrawList& dl, NkUIFont& font,
            NkUIID id, NkRect rect,
            NkUIContentBrowserConfig& cfg,
            NkUIContentBrowserState& state,
            const NkUIFSProvider& provider) noexcept
        {
            NkUIContentBrowserResult result{};

            // Init
            if (!state.currentPath[0]) {
                ::strncpy(state.currentPath,cfg.rootPath,511);
                state.needsRefresh=true;
            }
            if (state.needsRefresh) {
                LoadFolder(state,state.currentPath,provider);
                if (cfg.showTree) BuildTree(state,cfg.rootPath,provider);
            }

            // Fond
            dl.AddRectFilled(rect,ctx.theme.colors.bgWindow,4.f);
            dl.AddRect(rect,ctx.theme.colors.border,1.f,4.f);
            dl.PushClipRect(rect,true);

            // Toolbar (en haut, hauteur 34px)
            const float32 tbH=34.f;
            const NkRect toolbar={rect.x,rect.y,rect.w,tbH};
            DrawToolbar(ctx,dl,font,toolbar,cfg,state,provider);

            const float32 contentY=rect.y+tbH;
            const float32 contentH=rect.h-tbH;

            // Panneau arbre gauche
            if (cfg.showTree) {
                const NkRect treeR={rect.x,contentY,cfg.treeWidth,contentH};
                DrawTree(ctx,dl,font,treeR,cfg,state,provider,result);

                // Grille assets à droite
                const NkRect gridR={rect.x+cfg.treeWidth,contentY,rect.w-cfg.treeWidth,contentH};
                NkUIContentBrowserResult gr=DrawAssetGrid(ctx,dl,font,id,gridR,cfg,state);
                if (gr.event!=NkUIContentBrowserEvent::NK_CB_NONE) result=gr;

                // Ligne de séparation arbre/grille
                dl.AddLine({rect.x+cfg.treeWidth,contentY},{rect.x+cfg.treeWidth,contentY+contentH},
                        ctx.theme.colors.separator,1.f);
            } else {
                const NkRect gridR={rect.x,contentY,rect.w,contentH};
                NkUIContentBrowserResult gr=DrawAssetGrid(ctx,dl,font,id,gridR,cfg,state);
                if (gr.event!=NkUIContentBrowserEvent::NK_CB_NONE) result=gr;
            }

            // DnD ghost asset
            if (state.dndActive&&state.dndSourceIdx>=0&&state.dndSourceIdx<state.numAssets) {
                NkUIAssetEntry& src=state.assets[state.dndSourceIdx];
                const NkRect ghost={ctx.input.mousePos.x+14.f,ctx.input.mousePos.y-12.f,200.f,26.f};
                dl.AddRectFilled(ghost,ctx.theme.colors.bgWindow.WithAlpha(230),4.f);
                dl.AddRect(ghost,ctx.theme.colors.accent,1.5f,4.f);
                const NkRect ic={ghost.x+3.f,ghost.y+3.f,20.f,20.f};
                DrawAssetTypeIcon(dl,ic,src.type,false);
                font.RenderText(dl,{ghost.x+26.f,ghost.y+(ghost.h-font.metrics.lineHeight)*0.5f+font.metrics.ascender},
                            src.name,ctx.theme.colors.textPrimary,ghost.w-30.f,true);
                if (!ctx.input.mouseDown[0]){ state.dndActive=false; state.dndSourceIdx=-1; }
            }

            dl.PopClipRect();
            return result;
        }

    } // namespace nkui
} // namespace nkentseu