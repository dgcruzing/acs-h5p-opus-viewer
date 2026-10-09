param([ValidateSet('Debug','Release')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
New-Item -ItemType Directory -Path (Join-Path $projectRoot 'audit') -Force | Out-Null
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual C++ x64 Build Tools are required.' }
$cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
& $cmake -S $projectRoot -B (Join-Path $projectRoot 'build') -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE) { throw 'CMake configuration failed.' }
& $cmake --build (Join-Path $projectRoot 'build') --config $Configuration --parallel 4
if ($LASTEXITCODE) { throw 'Native build failed.' }
$releaseRoot = Join-Path $projectRoot 'release\0.1.1'
$assetsRoot = Join-Path $releaseRoot 'ACSH5PViewer_assets'
New-Item -ItemType Directory -Path $assetsRoot -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $projectRoot "build\$Configuration\ACSH5PViewer.dll"),(Join-Path $projectRoot "build\$Configuration\ACSH5PTestHost.exe") -Destination $releaseRoot -Force
foreach ($folder in @('helper','web')) { Copy-Item -LiteralPath (Join-Path $projectRoot $folder) -Destination $assetsRoot -Recurse -Force }
$licencesRoot=Join-Path $assetsRoot 'licenses'
New-Item -ItemType Directory -Path $licencesRoot -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $projectRoot 'LICENSE'),(Join-Path $projectRoot 'THIRD-PARTY-NOTICES.md') -Destination $licencesRoot -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'vendor\json\LICENSE.MIT') -Destination (Join-Path $licencesRoot 'nlohmann-json-LICENSE.MIT') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'vendor\webview2\LICENSE.txt') -Destination (Join-Path $licencesRoot 'WebView2-LICENSE.txt') -Force
$node = (Get-Command node.exe -ErrorAction Stop).Source
@{node=$node;version='0.1.1';player='0.3.1'} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $assetsRoot 'runtime.json') -Encoding utf8
Get-ChildItem -LiteralPath $releaseRoot -Recurse -File | ForEach-Object { @{path=$_.FullName.Substring($releaseRoot.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash;bytes=$_.Length} } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $projectRoot 'audit\release-files.json')
Write-Output "Built and staged: $releaseRoot"
