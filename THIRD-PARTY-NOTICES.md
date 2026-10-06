# Third-party components

The root MIT licence covers original ACS source only. It does not relicense the
dependencies below or user-supplied lessons. This repository tracks original source,
dependency URLs/hashes and build scripts. Bootstrap downloads dependencies into
ignored directories.

- **@missing-elements/h5p-offline-player 0.3.1** declares `MIT AND GPL-3.0-only`.
  Its component is MIT; the bundled H5P runtime includes GPL-3.0-only components.
  [Source](https://github.com/missing-elements/h5p-offline-player).
- **H5P runtime assets** are copied unchanged from the pinned distribution. Its
  NOTICE files identify upstream sources, build instructions and h5p-standalone 3.8.2.
- **zip.js** uses BSD-3-Clause terms, retained in the distributed scripts/notices.
- **Fonts and other assets** retain their upstream licences and notices.
- **WebView2 SDK 1.0.3650.58** is subject to Microsoft's SDK licence, copied into
  generated packs. The Evergreen Runtime is installed separately.
- **nlohmann/json 3.12.0** is MIT licensed.
  [Source](https://github.com/nlohmann/json/tree/v3.12.0).
- **Directory Opus SDK** headers are copyright GP Software and subject to its terms.
  [Official SDK](https://cdn2.gpsoft.com.au/files/Misc/opus_sdk.zip).
  Opus is not included and requires its own installation/licence.
- **Node.js** is installed separately, not bundled.

Generated packs preserve dependency licences and corresponding source references.
A source link alone is not a blanket statement that binary redistribution obligations
have been fulfilled. This initial publication does not host a binary release; review
upstream terms and corresponding-source requirements before distributing one publicly.

No lessons are published here. The helper fixture is generated from original test
metadata and is not a complete playable lesson.
