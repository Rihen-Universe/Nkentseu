#!/usr/bin/env python3
"""
=============================================================================
PV3DE / Humanoid AI — Fine-tuning LoRA avec Unsloth
=============================================================================
Ce script fine-tune un LLM de base (Llama3, Mistral, Phi-3, Qwen...)
sur des dialogues patient/médecin pour produire un modèle spécialisé
qui comprend les personnalités cliniques et répond de façon cohérente.

PRÉREQUIS HARDWARE :
  Minimum : GPU 12GB VRAM (RTX 3060 12GB, RTX 4070)
  Recommandé : GPU 24GB VRAM (RTX 3090, RTX 4090, A10G)
  Cloud : RunPod A40/A100, Lambda Labs, Google Colab Pro (A100)

PRÉREQUIS LOGICIEL :
  pip install unsloth transformers datasets peft trl accelerate bitsandbytes

COMMENT LES GRANDES BOITES FONT (simplifié pour toi) :
  1. Pré-entraînement : milliards de tokens, milliers de H100 → coût $$$M
     → Meta (Llama), Mistral AI, Google... TU UTILISES CE MODÈLE DE BASE.

  2. Fine-tuning SFT (ce que ce script fait) :
     → Tu prends le modèle de base et l'affines sur tes données.
     → Avec LoRA : seuls ~0.1% des paramètres sont entraînés.
     → Résultat : un modèle qui "parle" comme ton patient virtuel.

  3. RLHF/DPO (optionnel, Phase 3) :
     → Tu entraînes un reward model sur des préférences humaines.
     → Ex: "Réponse A (stoïque) est meilleure que B (dramatique) pour ce profil"

UTILISATION :
  # Fine-tuning standard (RTX 4090 24GB)
  python finetune_lora.py --model mistralai/Mistral-7B-Instruct-v0.3 \
                          --dataset Data/train_dialogues.jsonl \
                          --output Models/pv3de-patient-v1 \
                          --epochs 3

  # Mode économique 12GB VRAM (QLoRA 4-bit)
  python finetune_lora.py --model meta-llama/Meta-Llama-3.2-3B-Instruct \
                          --dataset Data/train_dialogues.jsonl \
                          --output Models/pv3de-patient-mini \
                          --quant 4bit --epochs 2

  # Export vers Ollama (pour l'utiliser localement dans NkOllamaBackend)
  python finetune_lora.py --export-ollama Models/pv3de-patient-v1

COÛT ESTIMÉ SUR CLOUD (RunPod, Lambda Labs) :
  RTX A40 48GB    : ~0.30€/h → ~2h pour 10k exemples = ~0.60€
  A100 40GB       : ~0.80€/h → ~1h pour 10k exemples = ~0.80€
  H100 80GB       : ~2.50€/h → ~30min = ~1.25€
=============================================================================
"""

import argparse
import json
import os
from pathlib import Path

# ── Imports conditionnels ─────────────────────────────────────────────────────
try:
    from unsloth import FastLanguageModel
    UNSLOTH_AVAILABLE = True
except ImportError:
    UNSLOTH_AVAILABLE = False
    print("⚠ Unsloth non installé. Utiliser: pip install unsloth")

try:
    from transformers import TrainingArguments
    from trl import SFTTrainer
    from datasets import load_dataset, Dataset
    TRANSFORMERS_AVAILABLE = True
except ImportError:
    TRANSFORMERS_AVAILABLE = False
    print("⚠ transformers/trl non installés.")


# =============================================================================
# ÉTAPE 1 — Chargement du modèle de base avec LoRA
# =============================================================================
def load_model_with_lora(model_name: str,
                          quant: str = "4bit",
                          max_seq_len: int = 2048):
    """
    Charge un modèle de base et y attache des adaptateurs LoRA.

    LoRA (Low-Rank Adaptation) : au lieu de modifier tous les 7 milliards
    de paramètres, on ajoute de petites matrices basse-rang (r=16, ~0.1%
    des paramètres). Cela permet :
    - Fine-tuning sur 12GB VRAM au lieu de 80GB+
    - Training 3–5x plus rapide
    - Le modèle de base reste intact (on peut charger plusieurs LoRA)

    QLoRA (Quantized LoRA) : même chose mais le modèle de base est
    quantifié en 4 bits (NF4). Encore moins de VRAM, légèrement moins
    précis mais suffisant pour du dialogue.
    """
    if not UNSLOTH_AVAILABLE:
        raise RuntimeError("Installer unsloth: pip install unsloth")

    print(f"📥 Chargement du modèle: {model_name} ({quant})")

    dtype = None  # Unsloth détecte automatiquement (bfloat16 sur Ampere+)
    load_in_4bit = (quant == "4bit")

    model, tokenizer = FastLanguageModel.from_pretrained(
        model_name        = model_name,
        max_seq_length    = max_seq_len,
        dtype             = dtype,
        load_in_4bit      = load_in_4bit,
        # token = "hf_...",  # Décommenter si modèle privé (Llama3 nécessite token HF)
    )

    # Attacher les adaptateurs LoRA
    # r : rang (16–64 recommandé, plus haut = plus de capacité, plus de VRAM)
    # lora_alpha : scaling factor (généralement = r ou 2*r)
    # target_modules : quelles couches affiner
    #   - q_proj, k_proj, v_proj, o_proj : attention (le plus important)
    #   - gate_proj, up_proj, down_proj : MLP (ajouter si dataset > 50k)
    model = FastLanguageModel.get_peft_model(
        model,
        r                   = 16,
        target_modules       = ["q_proj", "k_proj", "v_proj", "o_proj",
                                 "gate_proj", "up_proj", "down_proj"],
        lora_alpha           = 16,
        lora_dropout         = 0.05,
        bias                 = "none",
        use_gradient_checkpointing = True,  # économise ~40% VRAM
        random_state         = 42,
    )

    print(f"✅ Modèle chargé avec LoRA — paramètres entraînables: "
          f"{sum(p.numel() for p in model.parameters() if p.requires_grad):,}")

    return model, tokenizer


# =============================================================================
# ÉTAPE 2 — Préparation du dataset
# =============================================================================
def prepare_dataset(dataset_path: str, tokenizer, max_samples: int = 0):
    """
    Charge et formate le dataset JSONL en format de conversation.

    Format attendu (data/train_dialogues.jsonl) :
    {
      "system": "Tu es un patient stoïque de 65 ans...",
      "conversation": [
        {"role": "user",      "content": "Où avez-vous mal?"},
        {"role": "assistant", "content": "Dans la poitrine... pas terrible."},
        {"role": "user",      "content": "Depuis quand?"},
        {"role": "assistant", "content": "Ce matin, vers huit heures."}
      ]
    }
    """
    print(f"📂 Chargement dataset: {dataset_path}")

    if not os.path.exists(dataset_path):
        print(f"⚠ Dataset non trouvé: {dataset_path}")
        print("🔧 Génération d'un dataset de démonstration...")
        return generate_demo_dataset(tokenizer)

    # Lire le JSONL
    samples = []
    with open(dataset_path, 'r', encoding='utf-8') as f:
        for i, line in enumerate(f):
            if max_samples and i >= max_samples:
                break
            data = json.loads(line.strip())
            samples.append(format_conversation(data, tokenizer))

    print(f"✅ {len(samples)} exemples chargés")
    return Dataset.from_dict({"text": samples})


def format_conversation(data: dict, tokenizer) -> str:
    """
    Convertit un dialogue en format de chat tokenisé.
    Utilise le template de chat du tokenizer (ChatML, Llama3, Mistral...).
    """
    messages = []

    if "system" in data:
        messages.append({"role": "system", "content": data["system"]})

    for msg in data.get("conversation", []):
        messages.append({"role": msg["role"], "content": msg["content"]})

    # Appliquer le template de chat du tokenizer
    text = tokenizer.apply_chat_template(
        messages,
        tokenize=False,
        add_generation_prompt=False
    )
    return text


def generate_demo_dataset(tokenizer) -> 'Dataset':
    """Dataset de démonstration avec différentes personnalités."""
    examples = [
        # Patient stoïque
        {
            "system": ("Tu es Jean Dupont, 62 ans, retraité. Tu souffres de "
                       "douleurs chroniques depuis 5 ans. Tu es stoïque, peu bavard. "
                       "Tu minimises ta douleur. Douleur actuelle: 7/10."),
            "conversation": [
                {"role": "user", "content": "Bonjour, où avez-vous mal exactement?"},
                {"role": "assistant", "content": "C'est là, dans la poitrine. "
                                                  "Ça tire un peu vers le bras gauche."},
                {"role": "user", "content": "C'est intense? Sur 10?"},
                {"role": "assistant", "content": "Bah... sept, peut-être. C'est "
                                                  "supportable. J'ai connu pire."},
            ]
        },
        # Patient anxieux
        {
            "system": ("Tu es Marie Chen, 34 ans. Tu es très anxieuse, surtout "
                       "dans les hôpitaux. Douleur: 5/10 mais tu la ressens très fort. "
                       "Tu parles beaucoup, tu catastrophises."),
            "conversation": [
                {"role": "user", "content": "Qu'est-ce qui vous amène?"},
                {"role": "assistant", "content": "J'ai très mal au ventre depuis hier "
                                                  "soir... je pensais que ça passerait mais "
                                                  "là c'est de pire en pire, j'ai eu peur "
                                                  "que ce soit l'appendicite."},
                {"role": "user", "content": "Décrivez la douleur."},
                {"role": "assistant", "content": "C'est... c'est en bas à droite "
                                                  "surtout, ça fait comme des crampes, "
                                                  "parfois ça remonte. Ah... (grimace) "
                                                  "là j'ai eu une pointe."},
            ]
        },
        # Patient épuisé, dyspnée
        {
            "system": ("Tu es Robert Martin, 71 ans, cardiaque. Tu es très fatigué, "
                       "tu as du mal à respirer même au repos. SpO2 93%. Tu parles "
                       "lentement, parfois tu t'arrêtes pour reprendre souffle."),
            "conversation": [
                {"role": "user", "content": "Comment vous sentez-vous?"},
                {"role": "assistant", "content": "Pas... pas bien. J'ai du mal "
                                                  "à respirer. (pause) Même assis là."},
                {"role": "user", "content": "Depuis combien de temps?"},
                {"role": "assistant", "content": "Trois jours... peut-être quatre. "
                                                  "J'ai attendu... pensé que ça passerait."},
            ]
        },
        # Patient en détresse aiguë (douleur 9/10)
        {
            "system": ("Tu es Karim Benali, 48 ans. Tu as une douleur thoracique "
                       "intense, 9/10. Tu irradies vers le bras gauche. Tu es "
                       "en sueur, très anxieux. Tu as du mal à parler calmement."),
            "conversation": [
                {"role": "user", "content": "Parlez-moi de votre douleur."},
                {"role": "assistant", "content": "Ah... c'est terrible. Là, dans "
                                                  "la poitrine... comme si quelqu'un "
                                                  "m'écrasait. (grimace) Et là... "
                                                  "dans le bras..."},
                {"role": "user", "content": "Avez-vous des antécédents cardiaques?"},
                {"role": "assistant", "content": "Oui, j'ai eu... (s'arrête) ...un "
                                                  "problème il y a deux ans. Le médecin "
                                                  "m'avait dit... Aïe. Désolé."},
            ]
        },
        # Patient nauseux
        {
            "system": ("Tu es Sophie Laurent, 28 ans, enceinte de 6 semaines. "
                       "Tu as des nausées intenses, tu as vomi ce matin. "
                       "Tu es fatiguée. Tu fais confiance au médecin."),
            "conversation": [
                {"role": "user", "content": "Qu'est-ce qui ne va pas?"},
                {"role": "assistant", "content": "J'ai des nausées depuis ce matin, "
                                                  "j'ai vomi deux fois. Je me sens "
                                                  "vraiment pas bien."},
                {"role": "user", "content": "Avez-vous de la fièvre?"},
                {"role": "assistant", "content": "Non, pas de fièvre. Juste... "
                                                  "cet état de malaise. Et une "
                                                  "fatigue intense."},
            ]
        },
    ]

    # Multiplier pour avoir plus d'exemples (20 conversations de démo)
    augmented = examples * 4
    formatted = [format_conversation(e, tokenizer) for e in augmented]

    print(f"✅ Dataset de démonstration: {len(formatted)} exemples")
    return Dataset.from_dict({"text": formatted})


# =============================================================================
# ÉTAPE 3 — Entraînement
# =============================================================================
def train(model, tokenizer, dataset, output_dir: str,
          epochs: int = 3, batch_size: int = 2):
    """
    Lance le fine-tuning SFT (Supervised Fine-Tuning) avec LoRA.

    HYPERPARAMÈTRES CLÉS :
    - learning_rate : 2e-4 standard pour LoRA, pas besoin de réduire
    - per_device_train_batch_size : 2 pour 12GB VRAM, 4 pour 24GB
    - gradient_accumulation_steps : simule un batch effectif de 8–16
    - warmup_ratio : 0.03 = 3% des steps pour la montée en LR
    - num_train_epochs : 1–3 (plus = risque d'overfitting)
    - max_steps : si défini, override num_epochs
    """
    print(f"🚀 Démarrage du fine-tuning — {epochs} epochs, batch={batch_size}")

    training_args = TrainingArguments(
        output_dir               = output_dir,
        num_train_epochs         = epochs,
        per_device_train_batch_size = batch_size,
        gradient_accumulation_steps = 4,   # batch effectif = 8
        warmup_ratio             = 0.03,
        learning_rate            = 2e-4,
        fp16                     = False,  # Unsloth utilise bfloat16
        bf16                     = True,
        logging_steps            = 10,
        save_steps               = 100,
        save_total_limit         = 2,
        optim                    = "adamw_8bit",  # économise VRAM
        lr_scheduler_type        = "cosine",
        weight_decay             = 0.01,
        report_to                = "none",  # désactiver W&B/TensorBoard si non installé
    )

    trainer = SFTTrainer(
        model            = model,
        tokenizer        = tokenizer,
        train_dataset    = dataset,
        dataset_text_field = "text",
        max_seq_length   = 2048,
        args             = training_args,
    )

    # Lancer l'entraînement
    trainer_stats = trainer.train()

    print(f"\n✅ Entraînement terminé!")
    print(f"   Loss final : {trainer_stats.training_loss:.4f}")
    print(f"   Temps      : {trainer_stats.metrics['train_runtime']:.0f}s")

    return trainer


# =============================================================================
# ÉTAPE 4 — Sauvegarde et export
# =============================================================================
def save_model(model, tokenizer, output_dir: str, merge: bool = True):
    """
    Sauvegarde le modèle fine-tuné.

    DEUX FORMATS :
    1. LoRA seul (petit, ~100MB) : rapide à sauvegarder
       → Nécessite le modèle de base pour l'inférence
    2. Merged (modèle complet, ~4–14GB) : autonome
       → Peut être converti en GGUF pour Ollama
    """
    os.makedirs(output_dir, exist_ok=True)

    print(f"💾 Sauvegarde dans {output_dir}...")

    if merge:
        # Fusionner LoRA dans le modèle de base (modèle complet)
        print("   Fusion LoRA → modèle complet (peut prendre quelques minutes)...")
        model.save_pretrained_merged(output_dir, tokenizer,
                                      save_method="merged_16bit")
        print("✅ Modèle complet sauvegardé (format HuggingFace)")
    else:
        # Sauvegarder seulement les adaptateurs LoRA
        model.save_pretrained(output_dir)
        tokenizer.save_pretrained(output_dir)
        print("✅ Adaptateurs LoRA sauvegardés")


def export_to_gguf(model_dir: str, output_name: str = "pv3de-patient"):
    """
    Convertit le modèle HuggingFace en format GGUF pour Ollama.

    Nécessite llama.cpp :
      git clone https://github.com/ggerganov/llama.cpp
      cd llama.cpp && cmake -B build && cmake --build build -j

    Quantification GGUF recommandée pour PV3DE :
      Q4_K_M : meilleur compromis qualité/taille (4bit, ~4GB pour 7B)
      Q5_K_M : légèrement meilleur (5bit, ~5GB pour 7B)
      Q8_0   : quasi sans perte (8bit, ~8GB pour 7B) si VRAM suffisante
    """
    print(f"📦 Export GGUF depuis {model_dir}...")
    print("   Étape 1: Conversion HF → GGUF (float16)")
    print(f"   python llama.cpp/convert_hf_to_gguf.py {model_dir} "
          f"--outtype f16 --outfile {output_name}-f16.gguf")
    print("   Étape 2: Quantification Q4_K_M")
    print(f"   llama.cpp/build/bin/llama-quantize "
          f"{output_name}-f16.gguf {output_name}-q4km.gguf Q4_K_M")
    print("   Étape 3: Créer le Modelfile Ollama")

    modelfile_content = f"""FROM ./{output_name}-q4km.gguf
PARAMETER temperature 0.7
PARAMETER top_p 0.9
PARAMETER num_predict 150
PARAMETER stop "<|eot_id|>"
PARAMETER stop "</s>"
SYSTEM "Tu es un patient virtuel dans une simulation médicale PV3DE."
"""
    with open(f"{output_name}.Modelfile", "w") as f:
        f.write(modelfile_content)

    print("   Étape 4: Charger dans Ollama")
    print(f"   ollama create {output_name} -f {output_name}.Modelfile")
    print(f"   ollama run {output_name}")
    print("\n✅ Puis configurer NkOllamaBackend avec model_name='{output_name}'")


# =============================================================================
# ÉTAPE 5 — Génération du dataset d'entraînement
# =============================================================================
def generate_training_dataset(output_path: str, num_examples: int = 500):
    """
    Génère un dataset JSONL de dialogues patient/médecin annotés.
    En production : remplacer par de vrais cas cliniques annotés par des experts.

    STRUCTURE D'UN BON DATASET POUR PV3DE :
    - Diversité des personnalités (stoïque, anxieux, dépressif, coopératif)
    - Diversité des pathologies (IDM, pneumonie, appendicite, migraine...)
    - Diversité des stades (douleur légère → sévère)
    - Cohérence interne (le stoïque ne pleure pas soudainement)
    """
    print(f"🔧 Génération du dataset d'entraînement ({num_examples} exemples)...")

    personalities = [
        {"type": "stoic",    "age": 65, "name": "Jean Dupont",    "gender": "M",
         "desc": "retraité stoïque, minimise sa douleur, peu bavard",
         "expr_scale": 0.4, "verbosity": "brève"},
        {"type": "anxious",  "age": 34, "name": "Marie Chen",     "gender": "F",
         "desc": "très anxieuse, parle beaucoup, catastrophise",
         "expr_scale": 1.4, "verbosity": "longue"},
        {"type": "coop",     "age": 48, "name": "Karim Benali",   "gender": "M",
         "desc": "coopératif, fait confiance au médecin, honnête",
         "expr_scale": 1.0, "verbosity": "moyenne"},
        {"type": "depressed","age": 71, "name": "Robert Martin",  "gender": "M",
         "desc": "épuisé, peu d'énergie, réponses courtes",
         "expr_scale": 0.6, "verbosity": "très brève"},
        {"type": "young",    "age": 28, "name": "Sophie Laurent", "gender": "F",
         "desc": "jeune, effrayée, cherche du réconfort",
         "expr_scale": 1.2, "verbosity": "moyenne"},
    ]

    cases = [
        {"diag": "IDM",         "pain": 9.0, "symptoms": ["douleur thoracique", "irradiation bras gauche", "sueurs"]},
        {"diag": "Pneumonie",   "pain": 4.0, "symptoms": ["toux productive", "fièvre", "dyspnée"]},
        {"diag": "Appendicite", "pain": 7.0, "symptoms": ["douleur abdominale droite", "nausée", "fièvre"]},
        {"diag": "Migraine",    "pain": 6.0, "symptoms": ["céphalée intense", "photophobie", "nausée"]},
        {"diag": "Panique",     "pain": 3.0, "symptoms": ["palpitations", "vertiges", "dyspnée"]},
    ]

    qa_templates = {
        "où avez-vous mal": {
            "stoic":    "C'est là... {site}. Pas terrible.",
            "anxious":  "J'ai très mal {site}! Depuis hier c'est de pire en pire!",
            "coop":     "La douleur est principalement {site}, elle irradie parfois.",
            "depressed":"Là... {site}. (pause)",
            "young":    "Oh... ici {site}. C'est grave?",
        },
        "depuis quand": {
            "stoic":    "Ce matin. Vers huit heures.",
            "anxious":  "Depuis hier soir! J'ai attendu mais là j'en pouvais plus!",
            "coop":     "Depuis ce matin, vers 8h. Ça a commencé progressivement.",
            "depressed":"Ce matin... ou hier soir. Je ne sais plus trop.",
            "young":    "Depuis hier soir environ. J'ai pas bien dormi à cause de ça.",
        },
        "intensité sur 10": {
            "stoic":    "Sept... peut-être. C'est supportable.",
            "anxious":  "Au moins neuf! C'est insupportable!",
            "coop":     "Je dirais sept sur dix. C'est assez intense.",
            "depressed":"Six ou sept. (soupir)",
            "young":    "Euh... sept? Huit peut-être. C'est vraiment fort.",
        },
    }

    dataset = []
    import random
    random.seed(42)

    for i in range(num_examples):
        pers = personalities[i % len(personalities)]
        case = cases[i % len(cases)]
        site = case["symptoms"][0]

        # Construire le system prompt
        system = (f"Tu incarnes {pers['name']}, {pers['age']} ans ({pers['gender']}), "
                  f"{pers['desc']}. "
                  f"Situation: aux urgences pour {case['diag']}. "
                  f"Douleur: {case['pain']}/10. "
                  f"Réponses {pers['verbosity']}s, langage naturel parlé.")

        # Générer une conversation
        questions = list(qa_templates.keys())
        random.shuffle(questions)
        questions = questions[:random.randint(2, 3)]

        conversation = []
        for q in questions:
            answer_template = qa_templates[q].get(
                pers["type"],
                qa_templates[q].get("coop", "Je ne sais pas.")
            )
            answer = answer_template.format(site=site)
            # Ajouter de la variabilité si douleur élevée
            if case["pain"] >= 7 and random.random() > 0.6:
                answer = answer.replace(".", "... (grimace)")

            conversation.append({"role": "user",      "content": q.capitalize() + "?"})
            conversation.append({"role": "assistant", "content": answer})

        dataset.append({
            "system": system,
            "conversation": conversation,
            "metadata": {
                "personality": pers["type"],
                "diagnosis": case["diag"],
                "pain_level": case["pain"]
            }
        })

    # Sauvegarder en JSONL
    os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        for sample in dataset:
            f.write(json.dumps(sample, ensure_ascii=False) + "\n")

    print(f"✅ Dataset sauvegardé: {output_path} ({len(dataset)} exemples)")
    print("   Pour améliorer la qualité: ajouter des cas réels annotés par des experts")
    return output_path


# =============================================================================
# MAIN
# =============================================================================
def parse_args():
    parser = argparse.ArgumentParser(
        description="PV3DE — Fine-tuning LoRA pour personnages IA"
    )
    parser.add_argument("--model",   default="unsloth/mistral-7b-instruct-v0.3-bnb-4bit",
                        help="Modèle de base HuggingFace")
    parser.add_argument("--dataset", default="Data/train_dialogues.jsonl",
                        help="Chemin vers le dataset JSONL")
    parser.add_argument("--output",  default="Models/pv3de-patient-v1",
                        help="Dossier de sortie")
    parser.add_argument("--epochs",  type=int, default=3)
    parser.add_argument("--batch",   type=int, default=2)
    parser.add_argument("--quant",   choices=["4bit","8bit","none"], default="4bit",
                        help="Quantification (4bit recommandé pour RTX)")
    parser.add_argument("--max-samples", type=int, default=0,
                        help="Limiter le nombre d'exemples (0=tous)")
    parser.add_argument("--generate-dataset", type=int, default=0,
                        help="Générer N exemples de dataset synthétique")
    parser.add_argument("--export-ollama", action="store_true",
                        help="Exporter vers format GGUF/Ollama")
    parser.add_argument("--export-name", default="pv3de-patient",
                        help="Nom du modèle Ollama")
    parser.add_argument("--no-merge", action="store_true",
                        help="Ne pas fusionner LoRA (garder adaptateurs séparés)")
    return parser.parse_args()


def main():
    args = parse_args()

    print("=" * 70)
    print(" PV3DE — Fine-tuning LoRA pour humanoïde IA")
    print("=" * 70)

    # Générer le dataset si demandé
    if args.generate_dataset > 0:
        generate_training_dataset(args.dataset, args.generate_dataset)
        if not TRANSFORMERS_AVAILABLE:
            return

    # Mode export uniquement
    if args.export_ollama:
        export_to_gguf(args.output, args.export_name)
        return

    if not TRANSFORMERS_AVAILABLE or not UNSLOTH_AVAILABLE:
        print("\n⚠ Installation requise:")
        print("  pip install unsloth transformers trl datasets peft accelerate bitsandbytes")
        print("\n📖 Guide cloud (pas de GPU local):")
        print("  1. Créer un compte sur runpod.io ou lambdalabs.com")
        print("  2. Louer une instance A40/A100 (~0.30–0.80€/h)")
        print("  3. Uploader ce script + votre dataset")
        print("  4. Lancer : python finetune_lora.py --generate-dataset 500")
        print("              python finetune_lora.py --model mistralai/Mistral-7B-Instruct-v0.3")
        return

    # Pipeline complet
    # 1. Charger le modèle
    model, tokenizer = load_model_with_lora(args.model, args.quant)

    # 2. Préparer le dataset
    dataset = prepare_dataset(args.dataset, tokenizer, args.max_samples)

    # 3. Entraîner
    train(model, tokenizer, dataset, args.output, args.epochs, args.batch)

    # 4. Sauvegarder
    save_model(model, tokenizer, args.output, merge=not args.no_merge)

    # 5. Instructions export GGUF
    print("\n📦 Pour utiliser avec Ollama (NkOllamaBackend) :")
    export_to_gguf(args.output, args.export_name)

    print(f"\n🎉 Fine-tuning terminé!")
    print(f"   Modèle sauvegardé dans: {args.output}/")
    print(f"   Configurer NkOllamaBackend avec model_name='{args.export_name}'")


if __name__ == "__main__":
    main()
