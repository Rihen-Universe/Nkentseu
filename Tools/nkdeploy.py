# -*- coding: utf-8 -*-
"""nkdeploy — construire, deployer, lancer et capturer une cible Nkentseu.

POURQUOI CET OUTIL EXISTE
    Construire est facile (`jenga build`). DEPLOYER ne l'est pas : chaque
    plateforme a ses incantations, et aucune n'est devinable. Elles ont ete
    payees une par une au diagnostic, et sans elles ecrites quelque part elles
    se reperdent au prochain redemarrage :

      Windows    lancer l'exe ; NK_CAPTURE pour une capture offscreen
      Linux      via WSL ; /tmp de WSL est VOLATILE entre deux `wsl -e`
                 (la VM s'arrete quand plus rien ne tourne) -> ecrire sous /mnt/c
      Android    adb -s 127.0.0.1:21503 (MEmu n'est PAS le peripherique par
                 defaut) ; la demo se choisit par `setprop debug.nk.demo N`
                 car une NativeActivity ne recoit aucun argument
      HarmonyOS  `hdc list targets` renvoie [Empty] : il FAUT un
                 `hdc tconn 127.0.0.1:5555` explicite d'abord ; et le cache
                 hvigor repackage l'ANCIENNE .so -> purger harmony-build/entry/build
      Web        servir en HTTP (file:// interdit le fetch du .wasm/.data) ;
                 la demo se choisit par ?demo=N dans l'URL

UTILISATION
    python Tools/nkdeploy.py <plateforme> [--target renderdemo] [--config Debug|Release]
                             [--demo N] [--no-build] [--capture chemin.png]

    plateforme : windows | linux | android | harmonyos | web | all

EXEMPLES
    python Tools/nkdeploy.py android --demo 2
    python Tools/nkdeploy.py harmonyos --demo 2 --config Release
    python Tools/nkdeploy.py web --demo 2
    python Tools/nkdeploy.py all --demo 2       # etat de sante multi-plateforme

Chaque etape est TRACEE avec la commande exacte executee : l'outil doit rester
lisible comme une documentation executable, pas comme une boite noire.
"""
import argparse
import os
import shutil
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
NOWIN = subprocess.CREATE_NO_WINDOW if hasattr(subprocess, "CREATE_NO_WINDOW") else 0

# Outils externes. Chemins constates sur ce poste ; surchargeables par variable
# d'environnement pour ne pas figer une machine dans le depot.
ADB = os.environ.get("NK_ADB", r"D:\Program Files\Microvirt\MEmu\adb.exe")
ADB_SERIAL = os.environ.get("NK_ADB_SERIAL", "127.0.0.1:21503")
HDC = os.environ.get("NK_HDC", r"C:\ohos\command-line-tools\sdk\default\openharmony\toolchains\hdc.exe")
HDC_TARGET = os.environ.get("NK_HDC_TARGET", "127.0.0.1:5555")
WSL_DISTRO = os.environ.get("NK_WSL_DISTRO", "Ubuntu-22.04")

BUNDLE_ANDROID = "com.nkentseu.renderdemo"
BUNDLE_HARMONY = "com.nkentseu.renderdemo"


def say(msg):
    print("[nkdeploy] " + msg, flush=True)


def run(cmd, cwd=None, timeout=1800, check=False, quiet=False):
    """Execute une commande en TRACANT ce qui est lance (l'outil doit rester lisible)."""
    if not quiet:
        printable = cmd if isinstance(cmd, str) else " ".join('"%s"' % c if " " in str(c) else str(c) for c in cmd)
        say("$ " + printable)
    r = subprocess.run(cmd, cwd=cwd or ROOT, shell=isinstance(cmd, str), creationflags=NOWIN,
                       capture_output=True, text=True, errors="replace", timeout=timeout)
    if check and r.returncode != 0:
        say("ECHEC (code %d)" % r.returncode)
        tail = (r.stdout or "") + (r.stderr or "")
        print(tail[-2500:])
    return r


def build(target, config, platform):
    say("build %s / %s / %s" % (target, config, platform))
    r = run(["jenga", "build", "--target", target, "--config", config, "--platform", platform], timeout=2400)
    ok = ("SUCCESS" in (r.stdout or "")) and r.returncode == 0
    if not ok:
        say("BUILD EN ECHEC — extrait :")
        blob = (r.stdout or "") + (r.stderr or "")
        for line in blob.splitlines():
            if "error" in line.lower():
                print("   " + line.strip()[:200])
    else:
        say("build OK")
    return ok


# ── WINDOWS ─────────────────────────────────────────────────────────────────
def deploy_windows(a):
    exe = os.path.join(ROOT, "Build", "Bin", "%s-Windows" % a.config, a.target, a.target + ".exe")
    if not os.path.isfile(exe):
        say("introuvable : " + exe)
        return False
    env = dict(os.environ)
    env["NK_FIX_CAM"] = "1"
    env["NK_MAXFRAMES"] = "50"
    if a.capture:
        env["NK_CAPTURE"] = "40"
        env["NK_CAPTURE_PATH"] = a.capture
    say("$ %s --demo=%d" % (exe, a.demo))
    r = subprocess.run([exe, "--demo=%d" % a.demo], cwd=ROOT, env=env, creationflags=NOWIN,
                       capture_output=True, text=True, errors="replace", timeout=300)
    ok = a.capture is None or os.path.isfile(os.path.join(ROOT, a.capture))
    say("Windows : %s" % ("capture ecrite" if ok else "code %d, pas de capture" % r.returncode))
    return ok


# ── LINUX (WSL) ─────────────────────────────────────────────────────────────
def deploy_linux(a):
    # /tmp de WSL est VOLATILE entre deux `wsl -e` : la VM s'arrete des que plus
    # aucun processus ne tourne. Toute sortie a conserver va donc sous /mnt/c.
    outdir = "/mnt/c/nkcap"
    png = "%s/nkdeploy_linux.png" % outdir
    binp = "Build/Bin/%s-Linux/%s/%s" % (a.config, a.target, a.target)
    envs = "NK_FIX_CAM=1 NK_MAXFRAMES=50"
    if a.capture:
        envs += " NK_CAPTURE=40 NK_CAPTURE_PATH=%s" % png
    cmd = ("cd /mnt/d/Projets/2026/Nkentseu/Nkentseu; chmod +x %s 2>/dev/null; mkdir -p %s; "
           "%s timeout 180 ./%s --demo=%d > %s/nkdeploy_linux.log 2>&1; echo EXIT=$?; ls -la %s"
           % (binp, outdir, envs, binp, a.demo, outdir, png))
    r = run(["wsl", "-d", WSL_DISTRO, "-e", "bash", "-lc", cmd], timeout=600)
    print((r.stdout or "")[-600:])
    ok = "EXIT=0" in (r.stdout or "")
    if ok and a.capture:
        src = r"C:\nkcap\nkdeploy_linux.png"
        if os.path.isfile(src):
            shutil.copy(src, os.path.join(ROOT, a.capture))
            say("capture recuperee -> " + a.capture)
    return ok


# ── ANDROID (MEmu) ──────────────────────────────────────────────────────────
def deploy_android(a):
    if not os.path.isfile(ADB):
        say("adb introuvable (%s) — definir NK_ADB" % ADB)
        return False
    adb = [ADB, "-s", ADB_SERIAL]
    run([ADB, "connect", ADB_SERIAL], quiet=True)
    r = run(adb + ["shell", "true"])
    if r.returncode != 0:
        say("MEmu injoignable sur %s — l'emulateur est-il lance ?" % ADB_SERIAL)
        return False
    apk = None
    for cand in ("android-build-universal/%s-%s.apk" % (a.target, a.config),
                 "android-build/%s-%s.apk" % (a.target, a.config)):
        p = os.path.join(ROOT, "Build", "Bin", "%s-Android" % a.config, a.target, *cand.split("/"))
        if os.path.isfile(p):
            apk = p
            break
    if not apk:
        say("APK introuvable sous Build/Bin/%s-Android/%s/" % (a.config, a.target))
        return False
    run(adb + ["install", "-r", "-g", apk], timeout=900, check=True)
    # Une NativeActivity ne recoit AUCUN argument : la demo passe par une propriete.
    run(adb + ["shell", "setprop", "debug.nk.demo", str(a.demo)])
    run(adb + ["shell", "am", "force-stop", BUNDLE_ANDROID], quiet=True)
    run(adb + ["logcat", "-c"], quiet=True)
    run(adb + ["shell", "monkey", "-p", BUNDLE_ANDROID, "-c", "android.intent.category.LAUNCHER", "1"])
    say("lance ; attente de 12 s pour laisser la scene s'etablir")
    time.sleep(12)
    log = run(adb + ["logcat", "-d", "-s", "nkentseu", "NkRHI_GL", "SDL", "libc"], quiet=True).stdout or ""
    for key in ("CompileVF", "vsGlsl=0", "Demo 3D", "FATAL"):
        n = log.count(key)
        if n:
            say("logcat : %-12s x%d" % (key, n))
    if a.capture:
        run(adb + ["shell", "screencap", "-p", "/sdcard/nkdeploy.png"], quiet=True)
        dst = os.path.join(ROOT, a.capture)
        run(adb + ["pull", "/sdcard/nkdeploy.png", dst], quiet=True)
        if os.path.isfile(dst):
            say("capture recuperee -> " + a.capture)
            return True
        say("capture non recuperee")
        return False
    return True


# ── HARMONYOS ───────────────────────────────────────────────────────────────
def deploy_harmonyos(a):
    if not os.path.isfile(HDC):
        say("hdc introuvable (%s) — definir NK_HDC" % HDC)
        return False
    # `hdc list targets` renvoie [Empty] tant qu'on n'a pas connecte
    # EXPLICITEMENT l'emulateur : ce tconn n'est pas optionnel.
    run([HDC, "tconn", HDC_TARGET])
    tl = (run([HDC, "list", "targets"]).stdout or "").strip()
    say("targets : " + (tl if tl else "(vide)"))
    if not tl or "Empty" in tl:
        say("aucun peripherique HarmonyOS — l'emulateur est-il demarre ?")
        return False
    hap = os.path.join(ROOT, "Build", "Bin", "%s-HarmonyOS" % a.config, a.target, a.target + ".hap")
    if not os.path.isfile(hap):
        say("HAP introuvable : " + hap)
        return False
    run([HDC, "install", "-r", hap], timeout=900, check=True)
    # Une ability ne recoit pas d'argument : la demo passe par un fichier lu au
    # demarrage (cf. main.cpp, liste d'emplacements avec repli).
    run([HDC, "shell", "sh", "-c", "echo %d > /data/local/tmp/nk_demo.txt" % a.demo])
    run([HDC, "shell", "aa", "force-stop", BUNDLE_HARMONY], quiet=True)
    run([HDC, "shell", "aa", "start", "-a", "EntryAbility", "-b", BUNDLE_HARMONY])
    say("lance ; attente de 15 s")
    time.sleep(15)
    hl = run([HDC, "shell", "hilog", "-x", "-T", "renderdemo"], quiet=True).stdout or ""
    for key in ("nkmain", "NkRHI", "cannot locate", "Error relocating"):
        n = hl.count(key)
        if n:
            say("hilog : %-18s x%d" % (key, n))
    if a.capture:
        run([HDC, "shell", "snapshot_display", "-f", "/data/local/tmp/nkdeploy.jpeg"], quiet=True)
        dst = os.path.join(ROOT, a.capture)
        run([HDC, "file", "recv", "/data/local/tmp/nkdeploy.jpeg", dst], quiet=True)
        if os.path.isfile(dst):
            say("capture recuperee -> " + a.capture)
            return True
        say("capture non recuperee")
        return False
    return True


# ── WEB ─────────────────────────────────────────────────────────────────────
def deploy_web(a):
    d = os.path.join(ROOT, "Build", "Bin", "%s-Web" % a.config, a.target)
    html = None
    for f in ("index.html", a.target + ".html"):
        if os.path.isfile(os.path.join(d, f)):
            html = f
            break
    if not html:
        say("page HTML introuvable sous " + d)
        return False
    # file:// interdit le fetch du .wasm et du .data : il FAUT un serveur HTTP.
    port = 8765
    srv = subprocess.Popen([sys.executable, "-m", "http.server", str(port)], cwd=d,
                           creationflags=NOWIN, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    url = "http://127.0.0.1:%d/%s?demo=%d" % (port, html, a.demo)
    say("servi sur " + url)
    try:
        time.sleep(1.5)
        chrome = None
        for c in (r"C:\Program Files\Google\Chrome\Application\chrome.exe",
                  r"C:\Program Files (x86)\Google\Chrome\Application\chrome.exe"):
            if os.path.isfile(c):
                chrome = c
                break
        if not chrome:
            say("Chrome introuvable : ouvre l'URL a la main dans un navigateur.")
            time.sleep(20)
            return True
        if a.capture:
            dst = os.path.join(ROOT, a.capture)
            # --virtual-time-budget laisse tourner plusieurs frames : au premier
            # frame le pipeline n'est pas encore cree (pipeValid=0) et on ne
            # capturerait que la couleur de clear.
            run([chrome, "--headless=new", "--disable-gpu", "--use-gl=swiftshader",
                 "--window-size=1280,720", "--virtual-time-budget=15000",
                 "--screenshot=" + dst, url], timeout=180)
            ok = os.path.isfile(dst)
            say("Web : " + ("capture ecrite -> " + a.capture if ok else "pas de capture"))
            return ok
        subprocess.Popen([chrome, url], creationflags=NOWIN)
        say("ouvert dans Chrome ; fenetre laissee 30 s")
        time.sleep(30)
        return True
    finally:
        srv.terminate()


HANDLERS = {
    "windows": ("Windows", deploy_windows),
    "linux": ("Linux", deploy_linux),
    "android": ("Android", deploy_android),
    "harmonyos": ("HarmonyOS", deploy_harmonyos),
    "web": ("Web", deploy_web),
}


def main():
    ap = argparse.ArgumentParser(description="Construire, deployer, lancer et capturer une cible Nkentseu.")
    ap.add_argument("platform", choices=sorted(HANDLERS.keys()) + ["all"])
    ap.add_argument("--target", default="renderdemo")
    ap.add_argument("--config", default="Debug", choices=["Debug", "Release"])
    ap.add_argument("--demo", type=int, default=2, help="index de demo (2 = Demo3D)")
    ap.add_argument("--no-build", action="store_true", help="deployer sans reconstruire")
    ap.add_argument("--capture", default=None, help="chemin de capture (relatif au depot)")
    a = ap.parse_args()

    plats = sorted(HANDLERS.keys()) if a.platform == "all" else [a.platform]
    results = {}
    for p in plats:
        jenga_name, fn = HANDLERS[p]
        say("=" * 62)
        say("PLATEFORME %s" % jenga_name.upper())
        if not a.no_build and not build(a.target, a.config, jenga_name):
            results[p] = "build en echec"
            continue
        cap = a.capture
        if a.platform == "all" and cap is None:
            cap = "Captures/nkdeploy_%s.png" % p
        sub = argparse.Namespace(**vars(a))
        sub.capture = cap
        try:
            results[p] = "OK" if fn(sub) else "deploiement en echec"
        except Exception as e:
            results[p] = "exception : %s" % e

    say("=" * 62)
    say("RESUME")
    for p in plats:
        say("  %-10s %s" % (p, results.get(p, "non tente")))
    return 0 if all(v == "OK" for v in results.values()) else 1


if __name__ == "__main__":
    sys.exit(main())
