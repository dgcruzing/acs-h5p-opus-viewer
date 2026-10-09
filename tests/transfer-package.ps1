param([Parameter(Mandatory=$true)][string]$Zip)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$testRoot=Join-Path $projectRoot ('audit\transfer-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
Expand-Archive -LiteralPath $Zip -DestinationPath $testRoot
$pack=(Get-ChildItem -LiteralPath $testRoot -Directory | Select-Object -First 1).FullName
$manifestPath=Join-Path $pack 'manifest.json'
$manifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
foreach($entry in $manifest.files) {
  $file=Join-Path $pack $entry.path
  if((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $entry.sha256) { throw "Extraction hash failure: $($entry.path)" }
}
$setup=Join-Path $pack 'Setup.ps1'
$ps="$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe"
$node=(Get-Command node.exe).Source
$results=New-Object 'System.Collections.Generic.List[object]'
function Run-Case([string]$Name,[string[]]$Arguments,[int]$Expected) {
  $output=& $ps -NoProfile -ExecutionPolicy Bypass -File $setup @Arguments 2>&1
  $code=$LASTEXITCODE
  $output | Set-Content -LiteralPath (Join-Path $testRoot ($Name+'.log'))
  $results.Add(@{name=$Name;exitCode=$code;expected=$Expected;passed=($code -eq $Expected)})
  if($code -ne $Expected) { throw "$Name failed ($code): $output" }
}
Run-Case 'prerequisites' @('-Action','Check') 0
Run-Case 'missing-node' @('-Action','Check','-NodePath',(Join-Path $testRoot 'missing-node.exe')) 1
$asset=Join-Path $pack 'payload\ACSH5PViewer_assets\runtime.json'
$original=[IO.File]::ReadAllBytes($asset)
[IO.File]::AppendAllText($asset,'tampered')
Run-Case 'tampered-package-rejected' @('-Action','Check') 1
[IO.File]::WriteAllBytes($asset,$original)

# Exercise all file mutations in a fake Opus directory, with only the elevation
# and live-process guards mocked. The real dopus.exe is copied only for PE/version
# inspection, never launched. No Program Files target is passed to a mutation.
$fakeOpus=Join-Path $testRoot 'Target PC Unicode cafe - test\Directory Opus'
New-Item -ItemType Directory -Path (Join-Path $fakeOpus 'Viewers') -Force | Out-Null
Copy-Item -LiteralPath 'C:\Program Files\GPSoftware\Directory Opus\dopus.exe' -Destination $fakeOpus
$sentinel=Join-Path $fakeOpus 'Viewers\DopusWorX.dll'
'Unrelated viewer sentinel' | Set-Content -LiteralPath $sentinel
$sentinelHash=(Get-FileHash -LiteralPath $sentinel).Hash
$text=Get-Content -LiteralPath $setup -Raw
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseInput($text,[ref]$tokens,[ref]$errors)
$closed=$ast.Find({param($x) $x -is [Management.Automation.Language.FunctionDefinitionAst] -and $x.Name -eq 'Test-Closed'},$true)
$admin=$ast.Find({param($x) $x -is [Management.Automation.Language.FunctionDefinitionAst] -and $x.Name -eq 'Is-Admin'},$true)
$guard="function Test-Closed { if(`$script:opus -ne '$($fakeOpus.Replace("'","''"))') { throw 'TEST TARGET ESCAPED' } }"
$text=$text.Replace($closed.Extent.Text,$guard).Replace($admin.Extent.Text,'function Is-Admin { return $true }')
[IO.File]::WriteAllText($setup,$text,(New-Object Text.UTF8Encoding($true)))
$entry=$manifest.files | Where-Object path -eq 'Setup.ps1'
$entry.sha256=(Get-FileHash -LiteralPath $setup).Hash
$entry.bytes=(Get-Item -LiteralPath $setup).Length
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
$common=@('-OpusDirectory',$fakeOpus,'-NodePath',$node)
Run-Case 'install-isolated' (@('-Action','Install')+$common) 0
$runtime=Get-Content -LiteralPath (Join-Path $fakeOpus 'Viewers\ACSH5PViewer_assets\runtime.json') -Raw | ConvertFrom-Json
if($runtime.node -ne $node) { throw 'Target Node path not localised.' }
Run-Case 'verify-installed' (@('-Action','Verify')+$common) 0
Run-Case 'conflict-refused' (@('-Action','Install')+$common) 1
Run-Case 'force-backup' (@('-Action','Install','-Force')+$common) 0
if(!(Get-ChildItem -LiteralPath (Join-Path $pack 'backups') -Recurse -Filter 'ACSH5PViewer.dll')) { throw 'Backup was not created.' }
$extra=Join-Path $fakeOpus 'Viewers\ACSH5PViewer_assets\keep-user-file.txt'
'preserve me' | Set-Content -LiteralPath $extra
Run-Case 'uninstall-refuses-unknown-file' (@('-Action','Uninstall')+$common) 1
Remove-Item -LiteralPath $extra
Run-Case 'uninstall-isolated' (@('-Action','Uninstall')+$common) 0
if(Test-Path -LiteralPath (Join-Path $fakeOpus 'Viewers\ACSH5PViewer.dll')) { throw 'Uninstall left DLL.' }
if((Get-FileHash -LiteralPath $sentinel).Hash -ne $sentinelHash) { throw 'Unrelated viewer changed.' }
@{zip=$Zip;zipSha256=(Get-FileHash -LiteralPath $Zip).Hash;filesVerified=@($manifest.files).Count;cases=$results;allPassed=$true;scope='ZIP extraction/hash checks; real prerequisites read only; installer mutations in isolated fake Opus folder with elevation and live-process guards mocked. No actual second-PC test or UAC interaction.'} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $testRoot 'result.json') -Encoding UTF8
Get-Content -LiteralPath (Join-Path $testRoot 'result.json')
