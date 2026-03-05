Pour résoudre votre problème de connexion SSH avec GitHub, voici la procédure complète pour créer et configurer une clé SSH sur **Windows, Linux et macOS**. Suivez les étapes correspondant à votre système.

### Pour les 3 systèmes : Génération de la clé SSH

La première étape est identique partout. Ouvrez votre terminal (PowerShell sur Windows, Terminal sur Linux/macOS) et exécutez la commande suivante en remplaçant l'email par le vôtre  :

```bash
ssh-keygen -t ed25519 -C "votre_email@example.com"
```

* **Type d'algorithme** : `ed25519` est l'algorithme recommandé pour sa sécurité et ses performances. Si votre système ne le supporte pas, utilisez plutôt `ssh-keygen -t rsa -b 4096 -C "votre_email@example.com"` .
* **Emplacement** : Lorsqu'il vous demande où sauvegarder la clé, appuyez sur **Entrée** pour accepter l'emplacement par défaut (`~/.ssh/id_ed25519`) .
* **Phrase de passe** : Il vous demandera ensuite une "passphrase". C'est un mot de passe qui protège votre clé privée. Vous pouvez en mettre un pour plus de sécurité, ou laisser vide si vous ne voulez pas avoir à le taper à chaque utilisation .

Une fois l'opération terminée, deux fichiers seront créés :

* `id_ed25519` : Votre **clé privée** (à ne **JAMAIS** partager).
* `id_ed25519.pub` : Votre **clé publique** (celle-ci peut être partagée).

---

### Configuration spécifique par système d'exploitation

#### 🪟 Windows

1. **Démarrer l'agent SSH** (en tant qu'administrateur) :
   Ouvrez PowerShell en mode administrateur et exécutez ces commandes pour configurer l'agent SSH de Windows afin qu'il se lance automatiquement  :

   ```powershell
   Get-Service -Name ssh-agent | Set-Service -StartupType Manual
   Start-Service ssh-agent
   ```
2. **Ajouter la clé à l'agent** (dans un terminal normal, non administrateur) :

   ```powershell
   ssh-add $env:USERPROFILE\.ssh\id_ed25519
   ```
3. **Forcer Git à utiliser le client SSH de Windows** (Solution au conflit avec Git Bash) :
   Pour éviter les problèmes où Git utilise le mauvais client SSH, exécutez cette commande  :

   ```bash
   git config --global core.sshCommand "C:/Windows/System32/OpenSSH/ssh.exe"
   ```

#### 🐧 Linux

1. **Démarrer l'agent SSH en arrière-plan** :

   ```bash
   eval "$(ssh-agent -s)"
   ```
2. **Ajouter la clé à l'agent** :

   ```bash
   ssh-add ~/.ssh/id_ed25519
   ```

#### 🍏 macOS

1. **Configurer le fichier de configuration SSH** (optionnel mais recommandé) :
   Vérifiez si le fichier de config existe, et créez-le si besoin  :

   ```bash
   open ~/.ssh/config
   ```

   Si le fichier n'existe pas, créez-le avec `touch ~/.ssh/config`.
   Ensuite, éditez-le (avec nano, vim, etc.) pour qu'il contienne ces lignes afin de gérer automatiquement votre passphrase  :

   ```text
   Host github.com
     AddKeysToAgent yes
     UseKeychain yes
     IdentityFile ~/.ssh/id_ed25519
   ```
2. **Démarrer l'agent SSH** :

   ```bash
   eval "$(ssh-agent -s)"
   ```
3. **Ajouter la clé à l'agent et au trousseau** :

   ```bash
   ssh-add --apple-use-keychain ~/.ssh/id_ed25519
   ```

   *Note : Sur les versions de macOS antérieures à Monterey, utilisez `-K` à la place de `--apple-use-keychain`* .

---

### Pour finir : Ajouter la clé publique à GitHub

Peu importe votre système d'exploitation, vous devez maintenant fournir votre **clé publique** à GitHub.

1. **Copier la clé publique** :

   * **Windows (PowerShell)** : `cat ~/.ssh/id_ed25519.pub | clip`
   * **Linux** : `cat ~/.ssh/id_ed25519.pub` puis sélectionnez-la manuellement.
   * **macOS** : `pbcopy < ~/.ssh/id_ed25519.pub`
2. **Ajouter sur GitHub**  :

   * Connectez-vous à GitHub.
   * Cliquez sur votre photo de profil en haut à droite, puis **Settings**.
   * Dans la barre latérale, cliquez sur **SSH and GPG keys**.
   * Cliquez sur **New SSH key**.
   * Donnez-lui un titre (ex: "Mon PC portable").
   * Dans le champ "Key", collez votre clé publique.
   * Cliquez sur **Add SSH key**.

### ✅ Tester la connexion

Pour vérifier que tout fonctionne, tapez la commande :

```bash
ssh -T git@github.com
```

Si vous voyez un message du type `Hi RihenUniverse! You've successfully authenticated...`, c'est que c'est bon ! Vous pouvez maintenant utiliser `git push` normalement.
