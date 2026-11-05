param(
    [Parameter(Mandatory = $true)][string]$ServerRoot,
    [Parameter(Mandatory = $true)][string]$Library,
    [ValidatePattern('^[a-zA-Z0-9-]+$')][string]$Test = 'interiors',
    [switch]$Baseline,
    [switch]$ContactCache,
    [switch]$LegacyInclude
)

$ErrorActionPreference = 'Stop'
$repository = Split-Path $PSScriptRoot -Parent
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('a3d-interiors-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
foreach ($folder in 'components', 'gamemodes', 'scriptfiles/3dandreas') {
    New-Item -ItemType Directory -Path (Join-Path $testRoot $folder) | Out-Null
}
Copy-Item -LiteralPath (Join-Path $ServerRoot 'omp-server.exe') -Destination $testRoot
foreach ($name in 'Actors', 'Classes', 'Console', 'CustomModels', 'Dialogs', 'GangZones', 'LegacyNetwork', 'NPCs', 'Objects', 'Pawn', 'TextDraws', 'TextLabels', 'Timers', 'Variables', 'Vehicles') {
    Copy-Item -LiteralPath (Join-Path $ServerRoot "components/$name.dll") -Destination (Join-Path $testRoot 'components')
}
Copy-Item -LiteralPath $Library -Destination (Join-Path $testRoot 'components/3DAndreas.dll')
Copy-Item -LiteralPath (Join-Path $ServerRoot 'scriptfiles/3dandreas/world.db') -Destination (Join-Path $testRoot 'scriptfiles/3dandreas')

$config = @{
    announce = $false
    enable_query = $false
    max_players = 8
    max_bots = 4
    name = '3DAndreas interior regression'
    network = @{ bind = '127.0.0.1'; port = 7819 }
    pawn = @{ main_scripts = @('interiors'); side_scripts = @(); legacy_plugins = @() }
    rcon = @{ enable = $false; password = 'isolated-test-only' }
    '3dandreas' = @{ threads = 2; contact_cache = $ContactCache.IsPresent }
}
$config | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $testRoot 'config.json') -Encoding ASCII
$amx = Join-Path $testRoot 'gamemodes/interiors.amx'
$includes = if ($LegacyInclude) { @("-i$ServerRoot/qawno/include", "-i$repository/include") } else { @("-i$repository/include", "-i$ServerRoot/qawno/include") }
$compilerArgs = @((Join-Path $PSScriptRoot "$Test.pwn")) + $includes + @('-;+', '-(+', '-d2', "-o$amx")
if ($Baseline) {
    $compilerArgs += 'A3D_INTERIOR_BASELINE=1'
}
$started = Get-Date
$ErrorActionPreference = 'Continue'
$build = & (Join-Path $ServerRoot 'qawno/pawncc.exe') @compilerArgs 2>&1 | ForEach-Object { "$_" }
$ErrorActionPreference = 'Stop'
$build | Set-Content -LiteralPath (Join-Path $testRoot 'build.log')
$build | Write-Output
if ($build -match ' : (fatal error|error) [0-9]+:' -or !(Test-Path -LiteralPath $amx) -or (Get-Item -LiteralPath $amx).LastWriteTime -lt $started) {
    throw "Pawn build failed: $testRoot"
}
$bytes = [IO.File]::ReadAllBytes($amx)
if ([BitConverter]::ToUInt16($bytes, 4) -ne 0xF1E0 -or [BitConverter]::ToInt32($bytes, 0) -gt $bytes.Length) {
    throw "Invalid AMX: $amx"
}
$executable = Join-Path $testRoot 'omp-server.exe'
$process = Start-Process -FilePath $executable -WorkingDirectory $testRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $testRoot 'stdout.log') -RedirectStandardError (Join-Path $testRoot 'stderr.log')
$process.ProcessorAffinity = [IntPtr]((1 -shl [Math]::Min([Environment]::ProcessorCount, 4)) - 1)
Write-Output "Test directory: $testRoot; PID: $($process.Id)"
if (!$process.WaitForExit(45000)) {
    $running = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
    if ($running -and $running.Path -eq $executable) {
        Stop-Process -Id $process.Id
    }
    throw "Test server timed out: $testRoot"
}
$output = Get-Content -LiteralPath (Join-Path $testRoot 'stdout.log') -Raw
$output -split '\r?\n' | Where-Object { $_ -match '\[interiors\]|\[water\]|\[queries\]|\[groups\]|\[vehicles\]|\[smoke\]|\[optimization\]|\[rolling\]|3DAndreas|error|warning' } | Write-Output
$tag = ($Test -split '-')[0]
$result = [regex]::Match($output, "\[$tag\] DONE checks (\d+) failures (\d+)")
$counted = (Get-Content -LiteralPath (Join-Path $PSScriptRoot "$Test.pwn") -Raw) -match 'DONE checks'
if ($output -match '(?i)Run time error|\[Error\]|Unknown native|not found in script|\bFAIL\b' -or ($counted -and (!$result.Success -or [int]$result.Groups[2].Value -ne 0))) {
    throw "Regression failed: $testRoot"
}
