#pragma once

// =============================================================================
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// NkWebAudioJs.h
// -----------------------------------------------------------------------------
// Le JavaScript du dorsal audio Web (Emscripten) : l'AudioContext, le ScriptProcessor, et SURTOUT le deblocage
// par un geste de l'utilisateur.
//
// LE DEFAUT (08/11, « il se pourrait que le web ait des problemes avec l'audio ») : le contexte etait cree au demarrage
// du programme, donc SANS geste de l'utilisateur. La regle de lecture automatique des navigateurs (Chrome, Edge, Firefox,
// Safari, l'apercu WebView2 de NKCode) le laisse alors en etat « suspended » ; `resume()` appele hors d'un geste ne
// debloque rien, et le code ne rappelait `resume()` qu'a Init et Start -- jamais au premier clic ou a la premiere
// touche. Resultat : une page Web silencieuse, sans la moindre erreur. Mesure : tools/banc_webaudio.sh.
//
// LE GESTE : tant que le contexte n'est pas « running », chaque geste (clic, touche, toucher) rappelle `resume()` ; le
// dernier ecouteur disparait quand le contexte tourne.
// Contre-epreuve du banc : -DNK_WEBAUDIO_MUTATION_SANS_DEBLOCAGE retire l'ecoute des gestes (l'ancien comportement).
//
// Inclus UNE fois par NkAudioBackends.cpp (et par le banc tools/webaudio/test_webaudio.cpp).
// =============================================================================

#include <emscripten/emscripten.h>
#include <emscripten/em_js.h>

#ifdef NK_WEBAUDIO_MUTATION_SANS_DEBLOCAGE
#define NK_WEBAUDIO_DEBLOCAGE 0
#else
#define NK_WEBAUDIO_DEBLOCAGE 1
#endif

// Cree ou reutilise l'AudioContext global (Chrome en limite le nombre) + le ScriptProcessor, et cable le callback C.
// deblocage : 1 en production ; 0 seulement pour la contre-epreuve du banc.
EM_JS(int, NkWebAudio_JSInit, (int sampleRate, int channels, int bufferSize, int deblocage), {
	if (!window.NkAudioCtx) {
		var Ctx = window.AudioContext || window.webkitAudioContext;
		if (!Ctx)
			return 0;
		window.NkAudioCtx = new Ctx({sampleRate : sampleRate});
	}
	var ctx = window.NkAudioCtx;
	// Le contexte cree hors geste est « suspended » : on essaie (reussit si la page a deja eu un geste) ...
	if (ctx.state === 'suspended')
		ctx.resume();
	// ... et sinon on attend le PREMIER geste. Seuls certains evenements comptent comme une activation de la page
	// (clic, fin de toucher, touche) : on ecoute tous les gestes et on s'arrete des que le contexte tourne.
	if (deblocage && ctx.state !== 'running' && !window.NkAudioDeblocage) {
		var gestes = [ 'pointerdown', 'pointerup', 'mousedown', 'mouseup', 'click', 'touchstart', 'touchend', 'keydown', 'keyup' ];
		var retirer = function() {
			for (var i = 0; i < gestes.length; ++i)
				window.removeEventListener(gestes[i], essayer, true);
			window.NkAudioDeblocage = null;
		};
		var essayer = function() {
			if (window.NkAudioCtx.state === 'running') {
				retirer();
				return;
			}
			var p = window.NkAudioCtx.resume();
			if (p && p.then)
				p.then(function() {
					if (window.NkAudioCtx.state === 'running')
						retirer();
				}, function() {});
		};
		window.NkAudioDeblocage = essayer;
		for (var i = 0; i < gestes.length; ++i)
			window.addEventListener(gestes[i], essayer, true);
	}
	// bufferSize : puissance de 2 entre 256 et 16384.
	var bs = bufferSize;
	if (bs < 256)
		bs = 256;
	if (bs > 16384)
		bs = 16384;
	bs = Math.pow(2, Math.ceil(Math.log2(bs)));
	var node = ctx.createScriptProcessor(bs, 0, channels);
	// Tampon entrelace alloue cote C.
	var heapPtr = _malloc(bs * channels * 4);
	window.NkAudioHeapPtr = heapPtr;
	window.NkAudioHeapSize = bs * channels;
	window.NkAudioTrames = 0; // les trames reellement demandees par le navigateur (le temoin du banc)
	node.onaudioprocess = function(e) {
		window.NkAudioTrames++;
		Module._NkWebAudioFillBuffer(bs, heapPtr);
		var floatPtr = heapPtr >> 2;
		for (var c = 0; c < channels; ++c) {
			var out = e.outputBuffer.getChannelData(c);
			for (var i = 0; i < bs; ++i) {
				out[i] = HEAPF32[floatPtr + i * channels + c];
			}
		}
	};
	node.connect(ctx.destination);
	window.NkAudioNode = node;
	return ctx.sampleRate; // le taux reel
});

EM_JS(void, NkWebAudio_JSShutdown, (), {
	if (window.NkAudioNode) {
		window.NkAudioNode.disconnect();
		window.NkAudioNode.onaudioprocess = null;
		window.NkAudioNode = null;
	}
	if (window.NkAudioHeapPtr) {
		_free(window.NkAudioHeapPtr);
		window.NkAudioHeapPtr = 0;
	}
});

EM_JS(void, NkWebAudio_JSResume, (), {
	if (window.NkAudioCtx && window.NkAudioCtx.state === 'suspended') {
		window.NkAudioCtx.resume();
	}
});

EM_JS(void, NkWebAudio_JSSuspend, (), {
	if (window.NkAudioCtx && window.NkAudioCtx.state === 'running') {
		window.NkAudioCtx.suspend();
	}
});
