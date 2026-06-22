# NkSparkHAL

Accès matériel : description des **registres périphériques** et cartes mémoire des puces.

## Rôle

- Modèle des **périphériques** (GPIO, UART, TIMER, SPI, I2C, …) : adresses de base,
  registres, champs de bits, valeurs de reset.
- **Import CMSIS-SVD** (XML fourni par les fabricants) → génération de **bindings NkSpark**
  typés (`Gpio.PORTA.ODR`, etc.) consommables directement par le langage.
- **Cartes mémoire** par cible (taille/adresses flash & RAM, vecteurs) → fournies à
  NkSparkLink et NkSparkRT.
- **Profils de cible** : RV32I QEMU `virt`, STM32F4, ATmega328P, ESP32… (ajout incrémental).

## Notes

- Le HAL est *généré/déclaratif* : il ne contient pas de logique, juste la description
  matérielle exploitée par le compilateur et le runtime.

## Dépendances

- NkSparkCore (types), consommé par NkSparkSema (typage MMIO), NkSparkLink, NkSparkRT.
