# ComputeCardSim WebUI

This is a browser-first simulator for the current ComputeCardOS synth app. It follows the same interaction rules modeled in the C++ scaffold:

- 4 synth tracks mapped to DX7, Juno, Sampler, and DX7
- Hold left track buttons to open Tone, Parameter, Layout, and Utility matrices
- Use the right-side 3x4 selector rail to choose the active matrix cell
- Press the 4x14 keybed to simulate note on and note off events
- Press `FN` to preview the future app popup surface

## Run

```powershell
cd .\Devices\ComputeCardSim\WebUI
npm install
npm run dev -- --host
```

Open the local URL printed by Vite, usually `http://localhost:4173`.

## Build

```powershell
cd .\Devices\ComputeCardSim\WebUI
npm install
npm run build
```

## Current scope

- The browser simulator mirrors the synth app state model locally in JavaScript.
- It does not yet drive the C++ simulator host or a WASM runtime.
- This keeps the WebUI iteration loop fast while the real simulator bridge is still being designed.