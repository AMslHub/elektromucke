# Daisy_Griesinger_Reverb

Griesinger-inspirierter Stereo-Reverb für das **Electro-Smith Daisy Seed**.

## Inhalt

- `main.cpp`: Daisy-Seed-Audio-Callback und Initialisierung.
- `griesinger_reverb.h`: Komplette Reverb-Engine (Comb + Allpass + Damping + Width/Mix).
- `Makefile`: Build über `libDaisy`-Toolchain.

## Build

Voraussetzung: `libDaisy` und `DaisySP` sind ausgecheckt und die ARM-Toolchain ist installiert.

```bash
cd Daisy_Griesinger_Reverb
make
```

## Flash

```bash
make program-dfu
```

## Parameter-Tipps

- `SetDecay(0.86f..0.92f)` → längere Hallfahne
- `SetDamping(2200..4200)` → dunkler/vintage
- `SetDamping(7000..10000)` → klarer/hi-fi
- Bei Metallik: einzelne Comb-Delays um ±5..20 Samples ändern

## Neues GitHub-Repository anlegen

Falls du das als eigenes GitHub-Repo veröffentlichen willst:

```bash
mkdir Daisy_Griesinger_Reverb_repo
cp -r Daisy_Griesinger_Reverb/* Daisy_Griesinger_Reverb_repo/
cd Daisy_Griesinger_Reverb_repo
git init
git add .
git commit -m "Initial Daisy Griesinger reverb project"
# optional mit GitHub CLI:
# gh repo create Daisy_Griesinger_Reverb --public --source=. --remote=origin --push
```

Wenn du möchtest, kann ich dir als nächsten Schritt zusätzlich eine Version mit Modulation + Freeze bauen.
