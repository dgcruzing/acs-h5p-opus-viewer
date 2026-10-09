param(
  [ValidateSet('Check','Install','Verify','Uninstall')][string]$Action='Check',
  [string]$OpusDirectory,
  [string]$NodePath,
  [switch]$Force
)
$ErrorActionPreference='Stop'
$packRoot=$PSScriptRoot
$payload=Join-Path $packRoot 'payload'
$manifestPath=Join-Path $packRoot 'manifest.json'

function Resolve-PackFile([string]$Relative) {
  $root=[IO.Path]::GetFullPath($packRoot).TrimEnd('\')+'\'
  $resolved=[IO.Path]::GetFullPath((Join-Path $packRoot $Relative))
  if (!$resolved.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)) { throw 'Manifest path escapes the transfer folder.' }
  return $resolved
}
function Test-Pack {
  $manifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
  if ($manifest.version -ne '0.1.1' -or @($manifest.files).Count -lt 10) { throw 'Invalid transfer manifest.' }
  foreach($entry in $manifest.files) {
    $file=Resolve-PackFile $entry.path
    if (!(Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing package file: $($entry.path)" }
    if ((Get-Item -LiteralPath $file).Length -ne $entry.bytes -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $entry.sha256) { throw "Package integrity failed: $($entry.path)" }
  }
  Write-Host "Package hashes verified: $(@($manifest.files).Count) files."
}
function Get-PeMachine([string]$Path) {
  $stream=[IO.File]::OpenRead($Path)
  $reader=New-Object IO.BinaryReader($stream)
  try {
    if($reader.ReadUInt16() -ne 0x5a4d) { throw 'Not a Windows executable.' }
    $stream.Position=0x3c; $offset=$reader.ReadInt32(); $stream.Position=$offset
    if($reader.ReadUInt32() -ne 0x4550) { throw 'Invalid PE header.' }
    return $reader.ReadUInt16()
  } finally { $reader.Dispose() }
}
function Find-Opus {
  $candidates=@()
  if($OpusDirectory) { $candidates+= $OpusDirectory }
  else {
    foreach($key in @('HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\dopus.exe','HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\dopus.exe')) {
      if(Test-Path -LiteralPath $key) {
        $exe=(Get-Item -LiteralPath $key).GetValue('')
        if($exe) { $candidates+=Split-Path -Parent $exe.Trim('"') }
      }
    }
    $candidates+=Join-Path $env:ProgramFiles 'GPSoftware\Directory Opus'
  }
  foreach($directory in $candidates) {
    $exe=Join-Path $directory 'dopus.exe'
    if(Test-Path -LiteralPath $exe -PathType Leaf) {
      if((Get-PeMachine $exe) -ne 0x8664) { throw 'This transfer requires x64 Directory Opus; x86 and ARM64 builds are not supported.' }
      if((Get-Item -LiteralPath $exe).VersionInfo.FileMajorPart -ne 13) { throw 'This prototype is validated for Directory Opus 13 only.' }
      return [IO.Path]::GetFullPath($directory)
    }
  }
  throw 'Directory Opus 13 x64 was not found. Install it first, or pass -OpusDirectory with its installation folder.'
}
function Find-Node {
  $candidates=@()
  if($NodePath) { $candidates+=$NodePath }
  else {
    # Preserve this machine's existing configuration when reinstalling.
    $existing=Join-Path $script:opus 'Viewers\ACSH5PViewer_assets\runtime.json'
    if(Test-Path -LiteralPath $existing) {
      try { $candidates+=(Get-Content -LiteralPath $existing -Raw | ConvertFrom-Json).node } catch { }
    }
    $cmd=Get-Command node.exe -ErrorAction SilentlyContinue
    if($cmd) { $candidates+=$cmd.Source }
    $candidates+=Join-Path $env:ProgramFiles 'nodejs\node.exe'
  }
  foreach($candidate in $candidates) {
    if(!$candidate -or !(Test-Path -LiteralPath $candidate -PathType Leaf)) { continue }
    $resolved=(Resolve-Path -LiteralPath $candidate).Path
    $facts=& $resolved -p 'JSON.stringify({version:process.versions.node,arch:process.arch})'
    if($LASTEXITCODE -ne 0) { continue }
    $facts=$facts | ConvertFrom-Json
    if($facts.arch -eq 'x64' -and [int]($facts.version.Split('.')[0]) -ge 22) {
      Write-Host "Node $($facts.version): $resolved"
      return $resolved
    }
  }
  throw 'Install Node.js x64 LTS (22 or later), reopen setup, or pass -NodePath with node.exe. https://nodejs.org/en/download/'
}
function Test-WebView {
  $id='{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}'
  foreach($key in @("HKLM:\SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\$id","HKCU:\Software\Microsoft\EdgeUpdate\Clients\$id")) {
    $entry=Get-ItemProperty -LiteralPath $key -ErrorAction SilentlyContinue
    if($entry -and $entry.pv -and [version]$entry.pv -gt [version]'0.0.0.0') {
      Write-Host "WebView2 Evergreen: $($entry.pv)"; return
    }
  }
  throw 'Install Microsoft Edge WebView2 Evergreen Runtime (x64), then retry. https://developer.microsoft.com/en-us/microsoft-edge/webview2/'
}
function Test-Targets {
  foreach($target in @($script:opus,$script:viewers,$script:assets,$script:dll)) {
    if((Test-Path -LiteralPath $target) -and ((Get-Item -LiteralPath $target).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "Linked install target is unsupported: $target" }
  }
  if((Split-Path -Parent $script:dll) -ne $script:viewers -or (Split-Path -Parent $script:assets) -ne $script:viewers -or (Split-Path -Leaf $script:assets) -ne 'ACSH5PViewer_assets') { throw 'Unexpected installation target.' }
  if(Test-Path -LiteralPath $script:assets) {
    foreach($item in Get-ChildItem -LiteralPath $script:assets -Recurse -Force) {
      if($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Linked asset is unsupported: $($item.FullName)" }
    }
  }
}
function Test-Closed {
  if(Get-Process -Name dopus,ACSH5PTestHost -ErrorAction SilentlyContinue) { throw 'Exit Directory Opus and the ACS test host normally, then retry. No process has been closed.' }
}
function Is-Admin {
  $identity=[Security.Principal.WindowsIdentity]::GetCurrent()
  return (New-Object Security.Principal.WindowsPrincipal($identity)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}
function Assert-Installed {
  $recordPath=Join-Path $script:assets 'installation.json'
  if(!(Test-Path -LiteralPath $recordPath)) { throw 'No transfer installation record found. Existing files were left untouched.' }
  $record=Get-Content -LiteralPath $recordPath -Raw | ConvertFrom-Json
  if($record.product -ne 'ACS H5P Viewer' -or $record.opusDirectory -ne $script:opus) { throw 'Installation record does not match this target.' }
  $manifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
  foreach($entry in $manifest.files | Where-Object { $_.path.StartsWith('payload/') -and $_.path -notlike '*/runtime.json' }) {
    $relative=$entry.path.Substring(8)
    $installed=Join-Path $script:viewers $relative
    if(!(Test-Path -LiteralPath $installed -PathType Leaf) -or (Get-FileHash -LiteralPath $installed -Algorithm SHA256).Hash -ne $entry.sha256) { throw "Installed file differs or is missing: $relative" }
  }
  $config=Join-Path $script:assets 'runtime.json'
  if((Get-FileHash -LiteralPath $config -Algorithm SHA256).Hash -ne $record.runtimeSha256) { throw 'Installed runtime configuration changed; inspect it before continuing.' }
  Write-Host 'Installed DLL/assets match this transfer, including the locally configured runtime.'
}
try {
  if(![Environment]::Is64BitProcess) { throw 'Run setup in 64-bit Windows PowerShell.' }
  Test-Pack
  $script:opus=Find-Opus
  $script:viewers=[IO.Path]::GetFullPath((Join-Path $script:opus 'Viewers'))
  $script:dll=Join-Path $script:viewers 'ACSH5PViewer.dll'
  $script:assets=Join-Path $script:viewers 'ACSH5PViewer_assets'
  Test-Targets
  Write-Host "Directory Opus: $script:opus"
  if($Action -ne 'Uninstall') { $NodePath=Find-Node; Test-WebView }
  if($Action -eq 'Check') { Write-Host 'Prerequisites passed. Run Install.cmd when ready.'; exit 0 }
  if($Action -eq 'Verify') { Assert-Installed; exit 0 }
  Test-Closed
  if(!(Is-Admin)) {
    $arguments="& '$($PSCommandPath.Replace("'","''"))' -Action '$Action' -OpusDirectory '$($script:opus.Replace("'","''"))'"
    if($NodePath) { $arguments+=" -NodePath '$($NodePath.Replace("'","''"))'" }
    if($Force) { $arguments+=' -Force' }
    $encoded=[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($arguments))
    $process=Start-Process -FilePath "$PSHOME\powershell.exe" -Verb RunAs -WindowStyle Hidden -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-EncodedCommand',$encoded) -Wait -PassThru
    if($process.ExitCode -ne 0) { throw 'Elevated setup failed. Read setup-result.txt in this extracted folder.' }
    Get-Content -LiteralPath (Join-Path $packRoot 'setup-result.txt') | Write-Host
    exit 0
  }
  if($Action -eq 'Install') {
    if((Test-Path -LiteralPath $script:dll) -or (Test-Path -LiteralPath $script:assets)) {
      if(!$Force) { throw 'An ACS viewer already exists. Nothing was overwritten. Verify it, or review START-HERE.txt before using -Force.' }
      $backup=Join-Path $packRoot ('backups\'+[DateTime]::Now.ToString('yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8))
      New-Item -ItemType Directory -Path $backup | Out-Null
      foreach($old in @($script:dll,$script:assets)) { if(Test-Path -LiteralPath $old) { Copy-Item -LiteralPath $old -Destination $backup -Recurse } }
      Write-Host "Existing ACS files backed up to $backup"
    }
    New-Item -ItemType Directory -Path $script:viewers -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $payload 'ACSH5PViewer_assets') -Destination $script:viewers -Recurse -Force
    @{node=$NodePath;version='0.1.1';player='0.3.1'} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $script:assets 'runtime.json') -Encoding UTF8
    Copy-Item -LiteralPath (Join-Path $payload 'ACSH5PViewer.dll') -Destination $script:dll -Force
    @{product='ACS H5P Viewer';version='0.1.1';opusDirectory=$script:opus;installedAt=[DateTime]::UtcNow.ToString('o');runtimeSha256=(Get-FileHash -LiteralPath (Join-Path $script:assets 'runtime.json') -Algorithm SHA256).Hash} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $script:assets 'installation.json') -Encoding UTF8
    Assert-Installed
    $message='Installed ACS H5P Viewer 0.1.1. Open Opus > Settings > Preferences > Viewer > Plugins > Refresh. Enable ACS H5P Viewer, then select an .h5p file with the viewer pane open.'
  } else {
    Assert-Installed
    # Refuse unexpected files and links rather than recursively deleting user additions.
    $manifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    $expected=@($manifest.files | Where-Object { $_.path.StartsWith('payload/ACSH5PViewer_assets/') } | ForEach-Object { $_.path.Substring('payload/ACSH5PViewer_assets/'.Length).Replace('/','\') })+@('installation.json')
    foreach($item in Get-ChildItem -LiteralPath $script:assets -Recurse -Force) {
      if($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Linked asset found; uninstall stopped.' }
      if(!$item.PSIsContainer -and $item.FullName.Substring($script:assets.Length+1) -notin $expected) { throw "Unrecognised asset preserved: $($item.FullName)" }
    }
    Test-Targets
    Remove-Item -LiteralPath $script:dll
    Remove-Item -LiteralPath $script:assets -Recurse
    $message='Removed ACS H5P Viewer only. Other plugins, lesson files and local logs/profiles were preserved.'
  }
  $message | Set-Content -LiteralPath (Join-Path $packRoot 'setup-result.txt') -Encoding UTF8
  Write-Host $message
} catch {
  $message=$_.Exception.Message
  try { $message | Set-Content -LiteralPath (Join-Path $packRoot 'setup-result.txt') -Encoding UTF8 } catch { }
  Write-Host "SETUP STOPPED: $message" -ForegroundColor Red
  exit 1
}
