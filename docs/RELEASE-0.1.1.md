# 0.1.1 - preview placement fix

Prepared 10 October 2026.

An H5P preview could appear offset outside the Directory Opus panel, then recover
when resized. This was reported while using remote login. The old implementation
updated WebView2 bounds on resize but missed other geometry changes.

The viewer now synchronizes client-relative bounds on movement, resize, navigation
completion, display/DPI notifications and Opus redraw. A 500 ms change detector
catches ancestor-window movement even when no resize message reaches the pane.
It makes no browser geometry calls when the detected geometry is unchanged.
These updates do not request keyboard focus or change Opus process DPI settings.

## Verification

- Windows x64 Release build passed.
- All five public helper/shell tests passed. All transfer ZIP file hashes
  verified after extraction, and all nine isolated installer cases passed.
  Those installer tests mock administrator elevation and the live-Opus guard;
  they do not prove installation on a second computer.
- The native test host loaded the real DLL exports and navigated an H5P package.
- At 175% display scaling, child movement and ancestor-only movement preserved
  client-relative bounds without requiring resize. All 11 recorded geometry
  updates returned successful controller results and matched bounds readback.
- Synthetic display/DPI events, Opus resize messages, clear and close were checked.
- The maintainer installed the candidate and reported the actual Opus preview
  working correctly through the remote-login session.

The hidden native test is an API test, not a visual assertion. The user confirmation
does not establish every remote reconnect, monitor transition or DPI combination.
Those remain manual checks, alongside full media/focus testing on other computers.

To repeat the native geometry scenario, launch the standalone host with
`--plugin --geometry-test` and your own H5P path. It moves/resizes only its own
window, then closes itself. Pass that process's JSONL log from
`%LOCALAPPDATA%\ACS\H5PViewer\logs` to
`node tests/check-native-geometry.mjs <log-file>`. This checker is optional and
does not run as part of the portable CI test glob.

## Upgrade

Build and package using the README instructions, or use the matching prebuilt pack
when supplied. Run Check.cmd, exit Opus normally (including its tray instance), then
run this command from the extracted pack to explicitly update an existing install:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Setup.ps1 -Action Install -Force
```

Setup backs up the existing ACS DLL/assets and detects this computer's Node path.
Reopen Opus, select an H5P lesson, then test pane movement, resize and navigation.
Use Verify.cmd and VERIFY.txt. Keep the extracted pack and backup for rollback.

This update does not change H5P lesson files or bundle missing content libraries.
It remains an unsigned Windows x64 prototype; the offline player stays at 0.3.1.
