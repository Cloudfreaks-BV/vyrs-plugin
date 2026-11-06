# VYRS plug-in (VST3 / AU)

VYRS inside the DAW: your songs and takes, files dragged straight onto tracks, the DAW's tempo next to the take's, **record your vocal in the DAW straight onto a VYRS take, in time**, and (since 0.2.0) **write the lyrics and get the Hit score without leaving the DAW**.

## Licence

The VYRS plug-in is free software under the **GNU Affero General Public License v3** (see `LICENSE`). It is built with JUCE 8 under JUCE's AGPLv3 option (tag 8.0.15, outside this folder) and the Steinberg VST 3 SDK (MIT). The source is published at https://github.com/Cloudfreaks-BV/vyrs-plugin, and the plug-in links to it.

Only this plug-in is open source. The VYRS service it connects to (the web app and server) is a separate program and stays proprietary; using it needs a VYRS account with the Ultimate plan.

## Build (macOS)

```bash
brew install cmake ninja
git clone --depth 1 --branch 8.0.15 https://github.com/juce-framework/JUCE.git ~/Projects/sdks/JUCE
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build
```

Copy `build/VYRS_artefacts/Release/AU/VYRS.component` to `~/Library/Audio/Plug-Ins/Components/` and `VST3/VYRS.vst3` to `~/Library/Audio/Plug-Ins/VST3/`. Check the AU with `auval -v aufx Vyrs Clfr`.

## How it works

- **Sign-in** like VYRS Desktop: a code approved in the browser, token in `~/Library/Application Support/VYRS/plugin.token` (only readable by the user). The desktop app and the plug-in on one computer count as one of the 2 computers.
- **Files** go to `Music/VYRS/<song>/`, same names as VYRS Desktop, so both share the downloads.
- **Recording:** put VYRS on the vocal track (input monitoring on), arm, press play. Recording starts when the DAW plays and stops when it stops; the WAV goes to `/api/desktop/vocal` with `offsetMs` = DAW position at the start minus where the take starts in the DAW (default 0 s). The vocal appears as **Your vocal** in the VYRS console, in time.
- **Lyrics** (tab next to Takes): the song's sections as `[Verse 1]` headings with lines underneath, saved with Save or ⌘S (Ctrl+S) via `PUT /api/desktop/songs/<id>/lyrics` (same limits as the web studio). The save sends the `updatedAt` it started from; if the song changed in the browser meanwhile the server answers 409 and the plug-in asks: keep mine or load from VYRS. Unsaved edits are saved in the background when you switch songs or close the window, and kept in the plug-in until VYRS has them. **Score** saves first, then `POST …/score` returns the Hit score and feedback (the same scoring as the studio; it is stored on the song). **Rewrite section** offers three new takes on the section the cursor is in (`POST …/rewrite`); pick one and Save to keep it. Songs shared to listen only are read-only.
- **Development server:** put a URL in `~/Library/Application Support/VYRS/server.txt` (e.g. `http://localhost:3077`).
