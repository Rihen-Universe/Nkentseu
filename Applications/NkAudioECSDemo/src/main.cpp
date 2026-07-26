// =============================================================================
// Applications/NkAudioECSDemo/src/main.cpp
// =============================================================================
// Preuve d'exécution réelle pour Engine/Noge/src/Noge/ECS/Systems/NkAudioSystem
// (pont ECS -> NKAudio, cf. Engine/Noge/ROADMAP.md, Phase G1, item G1.1).
// Crée une entité ECS Noge (NkTransform + NkAudioSourceComponent), la joue via
// le système (playOnStart automatique) puis via NkAudioSystem::PlaySource()
// explicite, et vérifie PAR ASSERTIONS que la voix est active :
// audio::AudioEngine::IsPlaying(handle) / GetActiveVoices(). L'API NKAudio
// réelle (Kernel/Runtime/NKAudio/src/NKAudio/NkAudio.h) n'expose PAS de
// `GetVoiceState(handle)` -- IsPlaying()/IsPaused()/GetActiveVoices() sont les
// équivalents réellement présents, utilisés ici.
//
// "jenga test" / *_Tests sont bloqués par la politique de workspace (cf.
// Applications/NkEditableMeshDemo, même contournement) : cette petite
// application console n'est PAS une TestSuite, elle exécute le pipeline réel
// et affiche [OK]/[FAIL] par assertion manuelle.
// =============================================================================
#include "Noge/ECS/Systems/NkAudioSystem.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "NKECS/World/NkWorld.h"
#include "NKLogger/NkLog.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
#include <thread>

using namespace nkentseu;
using namespace nkentseu::ecs;
using namespace nkentseu::audio;

namespace {

	int gFailCount = 0;
	int gPassCount = 0;

	void Check(bool cond, const char *what) noexcept {
		if (cond) {
			++gPassCount;
			logger.Infof("  [OK]   %s\n", what);
		} else {
			++gFailCount;
			logger.Errorf("  [FAIL] %s\n", what);
		}
	}

	bool FileExists(const char *path) noexcept {
		std::ifstream f(path, std::ios::binary);
		return f.good();
	}

	// Résout un chemin de ressource audio INDÉPENDAMMENT du répertoire de
	// travail (CWD) au lancement. `dependfiles(["../../Resources/Audio"])`
	// (cf. NkAudioECSDemo.jenga) déclare la dépendance pour le suivi
	// incrémental de jenga mais NE COPIE PAS le dossier a côté de l'exe —
	// donc deux façons légitimes de lancer ce binaire coexistent dans ce
	// dépôt : depuis la racine du repo (convention historique, cf.
	// Applications/NkAudioDemo/src/main.cpp) OU en exécutant directement
	// l'exe depuis Build/Bin/<cfg>-<system>/NkAudioECSDemo/. On essaie les
	// deux avant d'abandonner, pour ne PAS dépendre d'une hypothèse sur qui
	// lance le binaire et depuis où.
	std::string ResolveAudioPath(const char *filename) noexcept {
		char buf[512];
		std::snprintf(buf, sizeof(buf), "Resources/Audio/%s", filename);
		if (FileExists(buf))
			return buf;

		// Build/Bin/<cfg>-<system>/NkAudioECSDemo/ -> 4 niveaux jusqu'à la racine.
		std::snprintf(buf, sizeof(buf), "../../../../Resources/Audio/%s", filename);
		if (FileExists(buf))
			return buf;

		// Ni l'un ni l'autre : retourne le chemin "racine repo" par défaut,
		// pour que l'échec de chargement plus bas pointe vers un message
		// diagnostiquable plutôt qu'une cascade d'échecs sans cause visible.
		std::snprintf(buf, sizeof(buf), "Resources/Audio/%s", filename);
		return buf;
	}

} // namespace

int main() {
	logger.Infof("=== NkAudioECSDemo — preuve d'execution Noge/ECS/Systems/NkAudioSystem ===\n");

	NkWorld world;
	NkAudioSystem audioSys;

	// Backend NULL_OUTPUT : deterministe, ne requiert aucun peripherique audio
	// reel (adapte a une execution automatisee/CI), mais fait tourner un VRAI
	// thread de mixage temps reel (cf. NullAudioBackend::ThreadFunc) -- les
	// voix avancent donc en temps reel, exactement comme avec WASAPI/ALSA/etc.
	AudioEngineConfig cfg;
	cfg.backend = AudioBackendType::NULL_OUTPUT;
	cfg.sampleRate = 48000;
	cfg.channels = 2;
	cfg.bufferSize = 512;

	Check(audioSys.Init(cfg), "NkAudioSystem::Init(cfg) reussi");
	Check(AudioEngine::Instance().IsInitialized(), "AudioEngine::Instance().IsInitialized() == true");

	// -------------------------------------------------------------------
	// 1) Entite avec lecture automatique (playOnStart) + listener.
	// -------------------------------------------------------------------
	const NkEntityId src = world.Create().With<NkTransform>(NkTransform{}).With<NkAudioSourceComponent>({}).Build();

	const std::string powerupPath = ResolveAudioPath("powerup.wav");
	if (!FileExists(powerupPath.c_str())) {
		logger.Errorf("[NkAudioECSDemo] Introuvable : %s -- lance depuis la racine du repo OU depuis "
					  "Build/Bin/<cfg>-<system>/NkAudioECSDemo/\n",
					  powerupPath.c_str());
	}

	NkAudioSourceComponent &srcComp = world.GetRef<NkAudioSourceComponent>(src);
	srcComp.clipPath = powerupPath.c_str();
	srcComp.playOnStart = true;
	srcComp.loop = false;
	srcComp.spatialize = false;
	srcComp.volume = 0.8f;

	const NkEntityId listener =
		world.Create().With<NkTransform>(NkTransform{}).With<NkAudioListenerComponent>({}).Build();
	(void)listener;

	Check(!srcComp.playing, "composant.playing == false avant la premiere Execute()");

	// Une frame de scheduler ECS : NkAudioSystem::Execute() consomme
	// playOnStart et appelle réellement audio::AudioEngine::Play().
	audioSys.Execute(world, 1.0f / 60.0f);

	const AudioHandle voice = audioSys.VoiceOf(src);
	Check(voice.IsValid(), "NkAudioSystem::VoiceOf(src).IsValid() apres Execute() (playOnStart)");
	Check(AudioEngine::Instance().IsPlaying(voice), "AudioEngine::IsPlaying(voice) == true : voix active");
	Check(AudioEngine::Instance().GetActiveVoices() >= 1, "AudioEngine::GetActiveVoices() >= 1");
	Check(world.Get<NkAudioSourceComponent>(src)->playing, "composant.playing == true (resynchronise par le systeme)");

	// -------------------------------------------------------------------
	// 2) API de controle explicite (equivalent concret du "NkAudioSource.
	//    Play()" du jalon) sur une deuxieme entite.
	// -------------------------------------------------------------------
	const NkEntityId src2 = world.Create().With<NkTransform>(NkTransform{}).With<NkAudioSourceComponent>({}).Build();
	const std::string bleepPath = ResolveAudioPath("bleep.wav");
	if (!FileExists(bleepPath.c_str())) {
		logger.Errorf("[NkAudioECSDemo] Introuvable : %s -- lance depuis la racine du repo OU depuis "
					  "Build/Bin/<cfg>-<system>/NkAudioECSDemo/\n",
					  bleepPath.c_str());
	}

	NkAudioSourceComponent &src2Comp = world.GetRef<NkAudioSourceComponent>(src2);
	src2Comp.clipPath = bleepPath.c_str();
	src2Comp.spatialize = false;
	src2Comp.volume = 0.7f;

	const AudioHandle voice2 = audioSys.PlaySource(world, src2);
	Check(voice2.IsValid(), "NkAudioSystem::PlaySource(src2) explicite -> handle valide");
	Check(AudioEngine::Instance().IsPlaying(voice2), "AudioEngine::IsPlaying(voice2) == true juste apres PlaySource()");

	// -------------------------------------------------------------------
	// 3) Attend la fin reelle des deux clips (poll : le backend Null tourne
	//    sur un thread reel independant, en temps reel).
	// -------------------------------------------------------------------
	bool finished1 = false, finished2 = false;
	for (int i = 0; i < 400 && !(finished1 && finished2); ++i) {
		std::this_thread::sleep_for(std::chrono::milliseconds(25));
		audioSys.Execute(world, 0.025f);
		finished1 = !AudioEngine::Instance().IsPlaying(voice);
		finished2 = !AudioEngine::Instance().IsPlaying(voice2);
	}
	Check(finished1, "voice (powerup.wav) terminee : IsPlaying redevient false");
	Check(finished2, "voice2 (bleep.wav) terminee : IsPlaying redevient false");
	Check(!world.Get<NkAudioSourceComponent>(src)->playing,
		  "composant.playing resynchronise a false en fin de lecture");

	audioSys.StopSource(world, src);
	audioSys.Shutdown();
	Check(!AudioEngine::Instance().IsInitialized(), "AudioEngine::IsInitialized() == false apres Shutdown()");

	logger.Infof("=== Resultat : %d OK / %d FAIL ===\n", gPassCount, gFailCount);
	return gFailCount == 0 ? 0 : 1;
}
