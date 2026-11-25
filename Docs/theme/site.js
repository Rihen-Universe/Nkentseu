// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Le script du site de documentation : thème (clair, sombre, choix gardé), menus, recherche hors ligne, copie d'un
// exemple, table « Sur cette page ». Aucun appel réseau : l'index de recherche (recherche.js, écrit par le
// générateur pour chaque langue et chaque version) est un fichier statique chargé à côté.
(function () {
  "use strict";
  var q = new URLSearchParams(location.search);

  // ---- Thème : appliqué avant le premier dessin (ce script est chargé dans <head>) ----
  function themeInitial() {
    var t = q.get("theme");
    if (t === "clair" || t === "sombre") return t;
    try { t = localStorage.getItem("nkdoc-theme"); } catch (e) { t = null; }
    if (t === "clair" || t === "sombre") return t;
    return (window.matchMedia && matchMedia("(prefers-color-scheme: light)").matches) ? "clair" : "sombre";
  }
  document.documentElement.setAttribute("data-theme", themeInitial());

  var NOMS = {
    fr: { R: "Référence", M: "Manuel", G: "Guides", T: "Tutoriels" },
    en: { R: "Reference", M: "Manual", G: "Guides", T: "Tutorials" }
  };

  function echapper(s) { return String(s).replace(/[&<>"]/g, function (c) { return { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]; }); }
  function sansAccent(s) { return String(s).normalize("NFD").replace(/[̀-ͯ]/g, "").toLowerCase(); }
  function surligner(t, m) {
    if (!m) return echapper(t);
    var i = sansAccent(t).indexOf(sansAccent(m));
    if (i < 0) return echapper(t);
    return echapper(t.slice(0, i)) + "<mark>" + echapper(t.slice(i, i + m.length)) + "</mark>" + echapper(t.slice(i + m.length));
  }
  function score(e, m) {
    var t = sansAccent(e.t), n = sansAccent(m);
    var court = t.split("::").pop();
    if (court === n || t === n) return 100;
    if (court.indexOf(n) === 0) return 80;
    if (t.indexOf(n) >= 0) return 60;
    if (sansAccent((e.m || "") + " " + e.c).indexOf(n) >= 0) return 30;
    return 0;
  }

  document.addEventListener("DOMContentLoaded", function () {
    var html = document.documentElement;
    var langue = html.lang === "en" ? "en" : "fr";
    var racine = html.getAttribute("data-racine") || "";
    var version = html.getAttribute("data-version") || "";

    // Thème
    document.querySelectorAll(".theme").forEach(function (b) {
      b.addEventListener("click", function () {
        var t = html.getAttribute("data-theme") === "clair" ? "sombre" : "clair";
        html.setAttribute("data-theme", t);
        try { localStorage.setItem("nkdoc-theme", t); } catch (e) { }
      });
    });
    // La langue garde le thème choisi (les liens FR / EN mènent à la même page dans l'autre langue)
    document.querySelectorAll(".langues a").forEach(function (a) {
      if (q.get("theme")) a.href += (a.href.indexOf("?") < 0 ? "?" : "&") + "theme=" + q.get("theme");
    });

    // Menus (version)
    function fermerMenus() { document.querySelectorAll(".choix.ouvert").forEach(function (c) { c.classList.remove("ouvert"); c.firstElementChild.setAttribute("aria-expanded", "false"); }); }
    document.querySelectorAll(".choix > button").forEach(function (b) {
      b.addEventListener("click", function (ev) { ev.stopPropagation(); var p = b.parentNode; var o = p.classList.contains("ouvert"); fermerMenus(); if (!o) p.classList.add("ouvert"); b.setAttribute("aria-expanded", String(!o)); });
    });
    document.addEventListener("click", fermerMenus);
    if (q.get("menu") === "version") { var mv = document.querySelector(".choix.version"); if (mv) mv.classList.add("ouvert"); }

    // Recherche
    var voile = document.querySelector(".voile"), champ = voile && voile.querySelector("input"), res = voile && voile.querySelector(".resultats");
    var filtre = "", sel = 0, liens = [];
    function rendre() {
      var INDEX = window.NK_INDEX || [];
      var m = champ.value.trim();
      var liste = INDEX.filter(function (e) { return !filtre || e.k === filtre; })
        .map(function (e) { return { e: e, s: m ? score(e, m) : 1 }; })
        .filter(function (x) { return x.s > 0; })
        .sort(function (a, b) { return b.s - a.s; })
        .slice(0, m ? 60 : 8);
      var parGroupe = {};
      liste.forEach(function (x) { (parGroupe[x.e.k] = parGroupe[x.e.k] || []).push(x.e); });
      var out = "";
      ["R", "M", "G", "T"].forEach(function (k) {
        if (!parGroupe[k]) return;
        out += '<div class="grp">' + NOMS[langue][k] + "</div>";
        parGroupe[k].forEach(function (e) {
          out += '<a href="' + echapper(racine + e.u) + '"><span class="ty-r">' + k + '</span><span><span class="t-r">' + surligner(e.t, m) +
            '</span><span class="c-r">' + echapper(e.c) + '</span></span><span class="v-r">' + echapper(version) + '</span></a>';
        });
      });
      res.innerHTML = out || '<div class="rien">' + echapper(html.getAttribute("data-rien") || "") + " « " + echapper(m) + " »</div>";
      liens = Array.prototype.slice.call(res.querySelectorAll("a"));
      sel = 0; marquer();
    }
    function marquer() { liens.forEach(function (a, i) { a.classList.toggle("sel", i === sel); }); }
    function ouvrir(txt) { if (!voile) return; voile.classList.add("ouvert"); champ.value = txt || ""; rendre(); champ.focus(); }
    function fermer() { voile.classList.remove("ouvert"); }
    if (voile) {
      document.querySelectorAll("[data-recherche]").forEach(function (b) { b.addEventListener("click", function () { ouvrir(""); }); });
      voile.addEventListener("click", function (ev) { if (ev.target === voile) fermer(); });
      champ.addEventListener("input", rendre);
      voile.querySelectorAll(".filtres button").forEach(function (b) {
        b.addEventListener("click", function () {
          filtre = b.dataset.k; voile.querySelectorAll(".filtres button").forEach(function (x) { x.setAttribute("aria-pressed", x === b ? "true" : "false"); }); rendre(); champ.focus();
        });
      });
      document.addEventListener("keydown", function (ev) {
        var ouvert = voile.classList.contains("ouvert");
        if ((ev.ctrlKey || ev.metaKey) && ev.key.toLowerCase() === "k") { ev.preventDefault(); ouvert ? fermer() : ouvrir(""); return; }
        if (!ouvert && ev.key === "/" && !/input|textarea/i.test(document.activeElement.tagName)) { ev.preventDefault(); ouvrir(""); return; }
        if (!ouvert) { if (ev.key === "Escape") fermerMenus(); return; }
        if (ev.key === "Escape") fermer();
        else if (ev.key === "ArrowDown") { ev.preventDefault(); sel = Math.min(sel + 1, liens.length - 1); marquer(); }
        else if (ev.key === "ArrowUp") { ev.preventDefault(); sel = Math.max(sel - 1, 0); marquer(); }
        else if (ev.key === "Enter" && liens[sel]) { location.href = liens[sel].getAttribute("href"); }
      });
      if (q.has("recherche")) { var lancer = function () { ouvrir(q.get("recherche")); }; if (window.NK_INDEX) lancer(); else window.addEventListener("load", lancer); }
    }

    // Copier un exemple
    document.querySelectorAll(".code [data-copier]").forEach(function (b) {
      var lib = b.textContent;
      b.addEventListener("click", function () {
        var pre = b.closest(".code").querySelector("pre");
        if (navigator.clipboard) navigator.clipboard.writeText(pre.innerText).then(function () { b.textContent = "✓"; setTimeout(function () { b.textContent = lib; }, 1400); });
      });
    });

    // « Sur cette page » suit la lecture
    var ancres = Array.prototype.slice.call(document.querySelectorAll(".toc a[href^='#']"));
    if (ancres.length && "IntersectionObserver" in window) {
      var cibles = ancres.map(function (a) { return document.getElementById(a.getAttribute("href").slice(1)); }).filter(Boolean);
      var obs = new IntersectionObserver(function (es) {
        es.forEach(function (e) {
          if (e.isIntersecting) ancres.forEach(function (a) { a.classList.toggle("actif", a.getAttribute("href") === "#" + e.target.id); });
        });
      }, { rootMargin: "-70px 0px -70% 0px" });
      cibles.forEach(function (c) { obs.observe(c); });
    }
  });
})();
