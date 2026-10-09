# ACS H5P Viewer for Directory Opus

**Viewer in action — 2 min 16 sec**

https://github.com/user-attachments/assets/b3fe72ec-dac2-4e1e-821c-a1fb41473ecb

[Watch via JW Player](https://cdn.jwplayer.com/videos/aEBA3kk1-42TyAa1a.mp4) · [Download MP4](https://raw.githubusercontent.com/dgcruzing/acs-h5p-opus-viewer/main/docs/media/acs-h5p-viewer-demo.mp4)

Play local `.h5p` lessons inside the **Directory Opus 13 viewer pane** on Windows x64.
Slides, questions, audio, video and captions remain interactive. Select another file
to unload the previous lesson; use Reload to read updated package contents.

**Version 0.1.1 fixes preview placement after pane/window movement and display changes.**
The maintainer confirmed improved operation in the actual Opus remote-login session.
See the [0.1.1 release notes](docs/RELEASE-0.1.1.md) for tests and remaining limits.

This is an early community source release. The maintainer reports a
successful installation on a second clean computer. Detailed media automation passed
in the compiled-DLL test host; real Opus rendering and file switching were separately
observed on the development machine. See [validation scope](docs/VALIDATION.md).

## Requirements

- Windows x64 and [Directory Opus 13](https://www.gpsoft.com.au/).
- [Node.js x64 LTS](https://nodejs.org/en/download/), version 22 or later.
- [Microsoft WebView2 Evergreen Runtime](https://developer.microsoft.com/en-us/microsoft-edge/webview2/).
- A local `.h5p` lesson containing its required libraries and media.

Node and WebView2 are separate prerequisites. DopusWorX and Lumi are not required.
The plugin claims only `.h5p`; it does not replace other viewers or edit lessons.

## Sample lesson

Download the [From footing to blockwall sample](samples/footing-blockwall/README.md)
to try slides, synthetic narration, animated videos, captions, questions and five
full-page illustrations. The fourteen-slide package includes its media and H5P
libraries and plays offline. Read [how it was made](samples/footing-blockwall/HOW-IT-WAS-MADE.md)
for the Lumi MCP, HyperFrames and Qwen voice-cloning workflow. Authoring tools are
not required for playback. Sample content has [separate licence terms](samples/footing-blockwall/LICENSE.md).

## Build and package

This initial repository provides source and build scripts, without a hosted binary
release. Install Visual Studio 2022 Build Tools with Desktop development with C++,
CMake and a Windows SDK. Run from the repository root in PowerShell 7:

```powershell
& .\scripts\bootstrap.ps1
& .\scripts\build.ps1
& .\scripts\package-transfer.ps1
```

Bootstrap downloads dependencies pinned by URL and SHA-256 in `dependencies.json`,
including `@missing-elements/h5p-offline-player` **0.3.1** and the official Opus/WebView2
SDKs. No `npm install` is needed for the build.

The build stages `release/0.1.1/`. Packaging creates a timestamped ZIP under
`transfers/` with the DLL, player assets, source, licences, setup scripts and a hash
manifest. Close the standalone host before rebuilding its executable.

## Install on another computer

Prefer using an agent? Copy the [agent setup prompt](docs/AGENT-SETUP-PROMPT.md).
It handles either a source checkout or an extracted prebuilt transfer pack,
checks target-PC prerequisites, and guides actual Opus verification.

1. Extract the generated transfer ZIP into a writable local folder.
2. Run **Check.cmd** to verify the pack and prerequisites.
3. Exit Opus normally. Run **Install.cmd** and approve Windows elevation.
4. In Opus, open **Settings → Preferences → Viewer → Plugins → Refresh**.
5. Enable **ACS H5P Viewer**, click OK, enable the viewer pane and select an `.h5p`.
6. Run **Verify.cmd** and the included **VERIFY.txt** playback checklist.

Setup records the target PC's Node path. It refuses existing ACS files by default;
documented `-Force` replacement backs them up. It never closes Opus for you. Keep the
extracted pack for verification/uninstall. Lessons are copied separately.

For custom paths see [setup instructions](transfer/START-HERE.txt). The scripts in
`transfer/` are templates: run them from a generated pack containing `payload/` and
`manifest.json`, not directly from the source checkout.

## Standalone preview

After building, substitute your own lesson path:

```powershell
& .\release\0.1.1\ACSH5PTestHost.exe --plugin 'C:\Lessons\example.h5p'
```

This loads the real DLL through its Opus SDK interface in a separate host; it does
not prove actual Opus integration.

## Design and limits

A C++ viewer embeds WebView2 and starts a hidden Node helper per viewer instance.
Paths cross private pipes; the browser receives opaque package URLs over loopback
HTTP with byte-range support. The player reads the ZIP without server extraction.
Profiles and helpers are isolated per viewer, and file changes invalidate old sessions.
See [architecture](docs/ARCHITECTURE.md).

- Playback only: no editing, saved learner results, LMS submission, thumbnails or updates.
- Autoplay and learner resume are disabled; media requires interaction.
- External resources are blocked. Missing libraries are not downloaded automatically.
- ZIP64 and split archives are unsupported; metadata size limits apply.
- Not every H5P content type is tested. Malformed packages may display a library error.
- This is an unsigned Opus 13 x64 prototype, not an Explorer preview handler.
- The DLL remains resident while Opus runs; updates/removal require a normal exit.

## Tests

```powershell
node --test tests/*.test.mjs
```

The portable tests need only Node and generate their own ZIP fixture. They cover
helper transport, ranges, stale sessions, errors, Unicode paths, independent instances
and shell settings. They do not establish browser media playback. The Windows CI
job also bootstraps dependencies and compiles the DLL/host; it does not run Opus.

Complete [manual acceptance](transfer/VERIFY.txt) inside real Opus for each target PC.

## Licence and support

Original ACS code is [MIT licensed](LICENSE). Dependency terms are separate: the
player distribution includes MIT and GPL-3.0-only components. Bootstrap fetches these
assets into ignored directories and generated packs retain notices. Read
[third-party notices](THIRD-PARTY-NOTICES.md) before distributing compiled assets.

Open an issue with versions and reproduction steps. Do not upload private lessons,
personal paths or unredacted logs. Local logs are under
`%LOCALAPPDATA%\ACS\H5PViewer\logs`.

To uninstall, exit Opus and run `Uninstall.cmd` from the generated pack. Other plugins,
lessons and local profiles/logs are preserved.
