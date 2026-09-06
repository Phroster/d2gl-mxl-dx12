param(
    [Parameter(Mandatory=$true)][string]$GameDirectory,
    [string]$OfficialD2FpsPath,
    [switch]$PreserveFrameRate
)
$ErrorActionPreference = 'Stop'
$taskGame = (Resolve-Path -LiteralPath $GameDirectory).Path
$taskPackage = Split-Path -Parent $PSScriptRoot
$taskManifestPath = Join-Path $taskPackage 'manifest.json'
if (-not (Test-Path -LiteralPath $taskManifestPath)) { throw 'Run this installer from the extracted release package.' }
$taskManifest = Get-Content -LiteralPath $taskManifestPath -Raw | ConvertFrom-Json
$taskExpectedClient = 'DD8BC6025DE921216A97C17F97CD1A50FBB85926E838EC60E13451448836D906'
$taskExpectedFps = 'DB9DE4D4D320A7B70E66FE6B4AAA0E6F1560A5300A4993CC81CF4512AB1240C1'
$taskPayloadNames = @('glide3x.dll','ddraw.dll','d2gl.mpq','d2gl.ini','d2fps.ini')

function Assert-GameClosed {
    if (Get-Process -Name Game,'Diablo II' -ErrorAction SilentlyContinue) { throw 'Close Diablo II before installing.' }
    foreach ($taskProcess in @(Get-Process -Name launcher -ErrorAction SilentlyContinue)) {
        if ($taskProcess.Path -and [Diagnostics.FileVersionInfo]::GetVersionInfo($taskProcess.Path).FileDescription -eq 'Median XL Launcher') {
            throw 'Close the Median XL launcher before installing.'
        }
    }
}
function Get-Hash([string]$Path) { (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash }
function Set-IniValue([string]$Text,[string]$Section,[string]$Key,[string]$Value) {
    $taskEol = if ($Text.Contains("`r`n")) { "`r`n" } else { "`n" }
    $taskLines = [Collections.Generic.List[string]]::new()
    foreach ($taskLine in ($Text -split '\r?\n')) { $taskLines.Add($taskLine) }
    $taskSectionStart = 0
    $taskSectionEnd = $taskLines.Count
    if ($Section) {
        $taskSections = @()
        for ($taskI=0; $taskI -lt $taskLines.Count; $taskI++) {
            if ($taskLines[$taskI] -match ('^\s*\['+[regex]::Escape($Section)+'\]\s*$')) { $taskSections += $taskI }
        }
        if ($taskSections.Count -ne 1) { throw "Expected exactly one [$Section] section in d2gl.ini." }
        $taskSectionStart = $taskSections[0]+1
        for ($taskI=$taskSectionStart; $taskI -lt $taskLines.Count; $taskI++) {
            if ($taskLines[$taskI] -match '^\s*\[') { $taskSectionEnd=$taskI; break }
        }
    }
    $taskMatches = @()
    for ($taskI=$taskSectionStart; $taskI -lt $taskSectionEnd; $taskI++) {
        if ($taskLines[$taskI] -match ('^\s*'+[regex]::Escape($Key)+'\s*=')) { $taskMatches += $taskI }
    }
    if ($taskMatches.Count -gt 1) { throw "Duplicate INI setting: $Key" }
    if ($taskMatches.Count -eq 1) { $taskLines[$taskMatches[0]]="$Key=$Value" }
    else { $taskLines.Insert($taskSectionEnd,"$Key=$Value") }
    $taskLines -join $taskEol
}

Assert-GameClosed
if (-not (Test-Path -LiteralPath (Join-Path $taskGame 'Game.exe')) -or
    (Get-Hash (Join-Path $taskGame 'D2Client.dll')) -ne $taskExpectedClient -or
    -not (Test-Path -LiteralPath (Join-Path $taskGame 'D2Sigma.dll'))) {
    throw 'This is not the supported Median XL / Diablo II 1.13c game folder.'
}
foreach ($taskName in $taskPayloadNames) {
    $taskEntry = @($taskManifest.files | Where-Object name -eq $taskName)
    if ($taskEntry.Count -ne 1 -or (Get-Hash (Join-Path $taskPackage $taskName)) -ne $taskEntry[0].sha256) {
        throw "Release package does not verify: $taskName"
    }
}

# Prepare all inputs before changing any game file.
$taskStage = Join-Path $taskPackage ('install-cache\'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskStage -Force | Out-Null
$taskExistingFps = Join-Path $taskGame 'd2fps.dll'
$taskOfficial = Join-Path $taskStage 'd2fps.dll'
if ($OfficialD2FpsPath) {
    Copy-Item -LiteralPath (Resolve-Path -LiteralPath $OfficialD2FpsPath).Path -Destination $taskOfficial
} elseif ((Test-Path -LiteralPath $taskExistingFps) -and (Get-Hash $taskExistingFps) -eq $taskExpectedFps) {
    Copy-Item -LiteralPath $taskExistingFps -Destination $taskOfficial
} else {
    Invoke-WebRequest -Uri 'https://get.median-xl.com/get.php?type=mxl_release&tag=2.14.0&file=d2fps.dll' -OutFile $taskOfficial
}
if ((Get-Hash $taskOfficial) -ne $taskExpectedFps) { throw 'The official D2FPS download does not match the supported build. No game files changed.' }

foreach ($taskName in @('glide3x.dll','ddraw.dll','d2gl.mpq')) {
    Copy-Item -LiteralPath (Join-Path $taskPackage $taskName) -Destination (Join-Path $taskStage $taskName)
}
$taskUtf8 = [Text.UTF8Encoding]::new($false,$true)
foreach ($taskName in @('d2gl.ini','d2fps.ini')) {
    $taskCurrent = Join-Path $taskGame $taskName
    $taskSource = if (Test-Path -LiteralPath $taskCurrent) { $taskCurrent } else { Join-Path $taskPackage $taskName }
    # StreamReader preserves Unicode text and detects UTF-8/UTF-16 byte-order marks.
    $taskReader = [IO.StreamReader]::new($taskSource,$taskUtf8,$true)
    try { $taskText=$taskReader.ReadToEnd(); $taskEncoding=$taskReader.CurrentEncoding } finally { $taskReader.Dispose() }
    if ($taskName -eq 'd2gl.ini') {
        $taskText=Set-IniValue $taskText 'Screen' 'foreground_fps' 'false'
        $taskText=Set-IniValue $taskText 'Screen' 'background_fps' 'false'
        $taskText=Set-IniValue $taskText 'Feature' 'motion_prediction' 'false'
        $taskText=Set-IniValue $taskText 'Other' 'frame_latency' '1'
    } else {
        if (-not $PreserveFrameRate) { $taskText=Set-IniValue $taskText '' 'fps' '0' }
        foreach ($taskPair in @(@('bg-fps','25'),@('menu-fps','true'),@('game-fps','true'),@('motion-smoothing','true'),@('arcane-bg','false'),@('reapply-patches','true'),@('integrity-checks','true'))) {
            $taskText=Set-IniValue $taskText '' $taskPair[0] $taskPair[1]
        }
    }
    [IO.File]::WriteAllText((Join-Path $taskStage $taskName),$taskText,$taskEncoding)
}

$taskNames=@('d2fps.dll','glide3x.dll','ddraw.dll','d2gl.mpq','d2gl.ini','d2fps.ini')
$taskBackup=Join-Path $taskGame ('mxl-combined-backups\'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $taskBackup -Force | Out-Null
$taskEntries=@()
foreach ($taskName in $taskNames) {
    $taskDestination=Join-Path $taskGame $taskName
    $taskBefore=$null
    if (Test-Path -LiteralPath $taskDestination) {
        $taskBefore=Get-Hash $taskDestination
        Copy-Item -LiteralPath $taskDestination -Destination (Join-Path $taskBackup $taskName)
        if ((Get-Hash (Join-Path $taskBackup $taskName)) -ne $taskBefore) { throw "Backup did not verify: $taskName" }
    }
    $taskEntries += [pscustomobject]@{name=$taskName;before_sha256=$taskBefore;after_sha256=(Get-Hash (Join-Path $taskStage $taskName))}
}
$taskReceipt=[ordered]@{game_directory=$taskGame;created_at=(Get-Date -Format o);status='prepared';files=$taskEntries}
$taskReceipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $taskBackup 'receipt.json')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'restore.ps1') -Destination (Join-Path $taskBackup 'restore.ps1')
Assert-GameClosed
try {
    foreach ($taskEntry in $taskEntries) {
        Copy-Item -LiteralPath (Join-Path $taskStage $taskEntry.name) -Destination (Join-Path $taskGame $taskEntry.name) -Force
        if ((Get-Hash (Join-Path $taskGame $taskEntry.name)) -ne $taskEntry.after_sha256) { throw "Installed file did not verify: $($taskEntry.name)" }
    }
    $taskReceipt.status='installed-awaiting-game-launch'
} catch {
    foreach ($taskEntry in $taskEntries) {
        $taskDestination=Join-Path $taskGame $taskEntry.name
        if ($taskEntry.before_sha256) { Copy-Item -LiteralPath (Join-Path $taskBackup $taskEntry.name) -Destination $taskDestination -Force }
        elseif ((Test-Path -LiteralPath $taskDestination) -and (Get-Hash $taskDestination) -eq $taskEntry.after_sha256) { Remove-Item -LiteralPath $taskDestination }
    }
    $taskReceipt.status='failed-restored-backup'
    $taskReceipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $taskBackup 'receipt.json')
    throw
}
$taskReceipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $taskBackup 'receipt.json')
Write-Output "Installed. Backup and restore script: $taskBackup"
Write-Output 'Launch normally. The read-only smoothing status is in the D2GL menu; startup details are in mxl-smoothing.log.'
