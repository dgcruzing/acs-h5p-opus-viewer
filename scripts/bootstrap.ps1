$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$deps=Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'dependencies.json') | ConvertFrom-Json
$vendor=Join-Path $projectRoot 'vendor'
New-Item -ItemType Directory -Path $vendor,(Join-Path $vendor 'json'),(Join-Path $vendor 'player') -Force | Out-Null
function Get-Verified($dep,[string]$destination) {
  if(!(Test-Path -LiteralPath $destination)) { Invoke-WebRequest -Uri $dep.url -OutFile $destination }
  if((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $dep.sha256) {throw "Dependency hash mismatch: $destination"}
}
Get-Verified $deps.opusSdk (Join-Path $vendor 'opus_sdk.zip')
Get-Verified $deps.webview2 (Join-Path $vendor 'webview2.zip')
Get-Verified $deps.json (Join-Path $vendor 'json\json.hpp')
Get-Verified $deps.player (Join-Path $vendor 'h5p-offline-player-0.3.1.tgz')
Expand-Archive -LiteralPath (Join-Path $vendor 'opus_sdk.zip') -DestinationPath (Join-Path $vendor 'opus-sdk') -Force
Expand-Archive -LiteralPath (Join-Path $vendor 'webview2.zip') -DestinationPath (Join-Path $vendor 'webview2') -Force
& tar.exe -xf (Join-Path $vendor 'h5p-offline-player-0.3.1.tgz') -C (Join-Path $vendor 'player')
if($LASTEXITCODE) {throw 'Player archive extraction failed.'}
$jsonLicence=Join-Path $vendor 'json\LICENSE.MIT'
if(!(Test-Path -LiteralPath $jsonLicence)) { Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/nlohmann/json/v3.12.0/LICENSE.MIT' -OutFile $jsonLicence }
$player=Join-Path $vendor 'player\package'
$webAssets=Join-Path $projectRoot 'web\assets'
$webLicences=Join-Path $projectRoot 'web\licenses'
New-Item -ItemType Directory -Path $webAssets,$webLicences -Force | Out-Null
Get-ChildItem -LiteralPath (Join-Path $player 'dist') | Copy-Item -Destination $webAssets -Recurse -Force
Copy-Item -LiteralPath (Join-Path $player 'NOTICE.md') -Destination (Join-Path $webLicences 'h5p-offline-player-NOTICE.md') -Force
Copy-Item -LiteralPath (Join-Path $player 'licenses\GPL-3.0.txt') -Destination $webLicences -Force
Write-Output 'Pinned native and player dependencies verified and prepared.'
