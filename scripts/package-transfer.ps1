param([string]$OutputDirectory)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
if(!$OutputDirectory) { $OutputDirectory=Join-Path $projectRoot 'transfers' }
$name='ACS-H5P-Viewer-0.1.0-Windows-x64-'+(Get-Date -Format 'yyyyMMdd-HHmmss')
$pack=Join-Path $OutputDirectory $name
$payload=Join-Path $pack 'payload'
New-Item -ItemType Directory -Path $payload | Out-Null
$release=Join-Path $projectRoot 'release\0.1.0'
Copy-Item -LiteralPath (Join-Path $release 'ACSH5PViewer.dll'),(Join-Path $release 'ACSH5PViewer_assets') -Destination $payload -Recurse
# Never transfer the source machine's absolute Node path into runtime configuration.
@{node='CONFIGURED_BY_SETUP';version='0.1.0';player='0.3.1'} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $payload 'ACSH5PViewer_assets\runtime.json') -Encoding UTF8
foreach($file in @('Setup.ps1','START-HERE.txt','VERIFY.txt')) { Copy-Item -LiteralPath (Join-Path $projectRoot ('transfer\'+$file)) -Destination $pack }
foreach($action in @('Check','Install','Verify','Uninstall')) {
  @('@echo off',('powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Setup.ps1" -Action '+$action),'set "result=%ERRORLEVEL%"','echo.','pause','exit /b %result%') | Set-Content -LiteralPath (Join-Path $pack ($action+'.cmd')) -Encoding ASCII
}
foreach($file in @('LICENSE','THIRD-PARTY-NOTICES.md','dependencies.json')) { Copy-Item -LiteralPath (Join-Path $projectRoot $file) -Destination $pack }
$source=Join-Path $pack 'source'
New-Item -ItemType Directory -Path $source | Out-Null
foreach($folder in @('native','helper','web','transfer')) { Copy-Item -LiteralPath (Join-Path $projectRoot $folder) -Destination $source -Recurse }
foreach($file in @('CMakeLists.txt','dependencies.json','package.json','LICENSE','THIRD-PARTY-NOTICES.md')) { Copy-Item -LiteralPath (Join-Path $projectRoot $file) -Destination $source }
New-Item -ItemType Directory -Path (Join-Path $source 'scripts') | Out-Null
foreach($file in @('bootstrap.ps1','build.ps1','package-transfer.ps1')) { Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination (Join-Path $source 'scripts') }
# build.ps1 writes its build audit here when reproducing the release from source.
New-Item -ItemType Directory -Path (Join-Path $source 'audit') | Out-Null
'Source rebuild requires Visual Studio 2022 C++ Build Tools and internet for pinned dependencies. Run scripts\bootstrap.ps1 then scripts\build.ps1. Normal installation uses the prebuilt payload and needs no compiler. The transfer setup localises runtime.json.' | Set-Content -LiteralPath (Join-Path $source 'README.txt')
$files=@(Get-ChildItem -LiteralPath $pack -Recurse -File | ForEach-Object {
  @{path=$_.FullName.Substring($pack.Length+1).Replace('\','/');bytes=$_.Length;lastWriteUtc=$_.LastWriteTimeUtc.ToString('o');sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
})
@{product='ACS H5P Viewer';version='0.1.0';packFormat=1;createdUtc=[DateTime]::UtcNow.ToString('o');sourceMachine=$env:COMPUTERNAME;sourcePaths=@('release/0.1.0','native','helper','web','transfer','scripts');includedSkills=@();targetPaths=@('<OpusDirectory>\Viewers\ACSH5PViewer.dll','<OpusDirectory>\Viewers\ACSH5PViewer_assets');notes=@('Node path is configured on the target; prerequisites and lessons are not bundled.','Manifest hashes exclude the manifest itself.','Source build and dependency references included; personal prototype.');files=$files} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $pack 'manifest.json') -Encoding UTF8
$zip=$pack+'.zip'
Compress-Archive -LiteralPath $pack -DestinationPath $zip -CompressionLevel Optimal
$hash=(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash
"$hash  $name.zip" | Set-Content -LiteralPath ($zip+'.sha256.txt') -Encoding ASCII
@{folder=$pack;zip=$zip;sha256=$hash;files=$files.Count;bytes=(Get-Item -LiteralPath $zip).Length} | ConvertTo-Json
