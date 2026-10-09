# Set up ACS H5P Viewer with your agent

Copy the complete prompt below into an agent that can read local files and run Windows commands. Set the folder line to your extracted repository or transfer-pack folder. If you have not downloaded anything, leave it as written and let the agent obtain the public repository.

The GitHub source download requires a build. A generated transfer pack already contains the compiled plugin and needs no compiler. The prompt distinguishes these two cases. This file is also included at the top level of newly generated transfer packs.

```text
Please get ACS H5P Viewer for Directory Opus running on this Windows computer,
then help me verify it with the public Footing to blockwall H5P sample.

Repository: https://github.com/dgcruzing/acs-h5p-opus-viewer
My extracted folder: use the current workspace, or ask me for its location if
it is not identifiable. If I have no local copy, clone/download the repository
into a writable folder I choose.

You are authorised to inspect the local files, check prerequisites, download
the declared dependencies, build the source when needed, and use the supplied
setup scripts to install this plugin. Explain any prerequisite installation,
large download, elevation or reboot needed. I will handle Windows UAC prompts
and close Directory Opus normally when needed. Preserve my other plugins,
lessons and existing work; never force-close Opus or bypass an installer refusal.
Proceed through routine checks without asking me to approve every command.

1. Identify what I have before running installation commands.
   Read README.md (or README.txt) and the relevant local setup instructions.
   A prebuilt transfer pack has Setup.ps1, manifest.json and
   payload/ACSH5PViewer.dll at its top level. Use that folder for setup.
   A source checkout has CMakeLists.txt and scripts/bootstrap.ps1 but may have
   no built DLL. The transfer/ directory in a source checkout is a template,
   not an installable pack. If I opened the outer extraction folder, locate
   the actual pack/repository inside it. Work outside the ZIP in a writable
   local folder. Record the version and source commit when available.

2. Check this PC rather than assuming the original author's paths.
   The target is Windows x64 with Directory Opus 13 x64, Node.js x64 LTS
   version 22 or later, and Microsoft WebView2 Evergreen Runtime.
   Detect existing Opus and Node installation paths and the registered
   WebView2 runtime. Verify actual versions and architecture.
   If prerequisites are missing, explain the official installation required
   and help me install it. Directory Opus licensing remains my responsibility.
   Official sources are linked in the repository README and START-HERE.txt.
   A source build also needs PowerShell 7 and Visual Studio 2022 C++ Build
   Tools with Desktop development with C++, CMake tools and a Windows SDK.
   Check these before building. A prebuilt pack does not need those build tools.
   Lumi, MCP, HyperFrames and Qwen were authoring tools for the sample;
   they are not required to run this viewer or play the sample.

3. If this is source, prepare a real transfer pack.
   From the repository root, use the supplied commands in PowerShell 7:
     & .\scripts\bootstrap.ps1
     & .\scripts\build.ps1
     & .\scripts\package-transfer.ps1
   If this source copy includes tests/, first run:
     node --test tests/*.test.mjs
   If tests/ is absent from a supplied source copy, record that check as
   unavailable rather than claiming it passed. Check each exit/result before
   proceeding. Bootstrap checks dependency
   hashes against dependencies.json; do not replace pins or ignore mismatches.
   No npm install is required for this repository's build.
   The release is staged under release/0.1.1 and the packaging script reports
   the generated transfer folder/ZIP under transfers/. Use that generated
   pack's Setup.ps1 and manifest.json for installation, not transfer/Setup.ps1.
   Keep logs and the generated pack for verification and later uninstall.
   If I already have a prebuilt pack, skip this source-build step.

4. Check and install from the actual transfer-pack folder.
   Read START-HERE.txt and VERIFY.txt. Run Check.cmd, or its non-pausing
   equivalent from that folder:
     powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Setup.ps1 -Action Check
   Use the documented -OpusDirectory and -NodePath options if detection fails,
   with paths verified on this PC. Keep the same options for later actions.
   Check verifies pack hashes and prerequisites; it does not install anything.
   If an ACS H5P Viewer installation already exists, inspect/verify it first.
   Explain any conflict and obtain my approval before using the documented
   -Force replacement, which backs up the existing ACS installation.
   When Check passes, tell me if Opus or the standalone test host must close.
   Wait until I close them normally, then run Install.cmd or:
     powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Setup.ps1 -Action Install
   Let me approve UAC. Do not disable Windows protection globally, modify
   DopusWorX, or copy a source machine's runtime.json into this installation.
   The supplied setup localises the Node path and verifies the installed files.

5. Enable the viewer in actual Directory Opus.
   Reopen Opus. Help me go to Settings > Preferences > Viewer > Plugins,
   click Refresh, enable ACS H5P Viewer and click OK. Enable the viewer pane.
   If your tools cannot operate the native UI, give me these exact manual
   actions and wait for the result rather than claiming you performed them.

6. Obtain and verify the public sample.
   In a source checkout, use:
     samples/footing-blockwall/acs-footing-blockwall-full-pdf-1.1.1.h5p
   A transfer pack does not bundle lessons. If the sample is absent, download
   the raw binary from:
     https://raw.githubusercontent.com/dgcruzing/acs-h5p-opus-viewer/main/samples/footing-blockwall/acs-footing-blockwall-full-pdf-1.1.1.h5p
   Save it as an .h5p in a writable lessons folder. Do not save the GitHub HTML
   page as the lesson. Read the sample README, licence notes and manifest.
   Verify SHA-256 before opening. The published 1.1.1 file's expected hash is:
     70ce97604b49d754af34554298572c58ba898b632e52f7485ccd8a9eb9899828
   If the file/version differs, inspect the matching published manifest and
   explain the difference; do not ignore a checksum mismatch.

7. Verify the installed files and actual panel behaviour.
   From the same pack, run Verify.cmd or:
     powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Setup.ps1 -Action Verify
   Then select the sample in Opus and use VERIFY.txt as the acceptance checklist.
   Visit all 14 slides. Play audio on slides 1, 3, 5 and 8; open transcripts.
   Play videos on slides 2 and 6, turn English captions on, seek and resume.
   Test wrong-answer feedback and retry/correct answers on slides 4 and 7.
   Inspect the complete PDF pages on slides 10–14 using fullscreen as needed.
   Switch files during playback and confirm the old media stops. Reopen the
   lesson, try Reload, resize the pane and check keyboard focus.
   Check multiple viewers, Unicode/network-share paths and playback without
   internet where practical for this PC. Do not disrupt my network connection
   or active work to simulate offline mode; coordinate any such test with me.
   Record PASS, FAIL or NOT TESTED for each check. Installer Verify checks
   hashes/configuration, not playback. Browser or standalone-host tests do
   not prove actual Opus-panel behaviour. Preserve source lesson hashes.

8. If something fails, diagnose it without unrelated changes.
   Inspect setup-result.txt and %LOCALAPPDATA%\ACS\H5PViewer\logs.
   Check Node/runtime paths, WebView2, plugin enablement and package integrity.
   Report the exact error and next corrective step. Use the supplied scripts
   and documented backup/uninstall procedures; retain the transfer folder.
   This is a playback plugin for Opus, not an Explorer preview handler or LMS.

Finish with the installed version, detected prerequisite versions, pack and
sample locations, verification results and any remaining manual checks.
State whether playback was actually observed inside Opus. Save a concise
setup report locally; keep personal paths, machine details and logs private.
```

For manual setup, use [the repository README](https://github.com/dgcruzing/acs-h5p-opus-viewer#install-on-another-computer). For what the sample exercises, see [the sample introduction](https://github.com/dgcruzing/acs-h5p-opus-viewer/tree/main/samples/footing-blockwall).
