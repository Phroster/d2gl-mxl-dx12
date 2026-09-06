param([string]$BackupDirectory=$PSScriptRoot)
$ErrorActionPreference='Stop'
if (Get-Process -Name Game,'Diablo II' -ErrorAction SilentlyContinue) { throw 'Close the game before restoring.' }
foreach ($taskProcess in @(Get-Process -Name launcher -ErrorAction SilentlyContinue)) {
    if ($taskProcess.Path -and [Diagnostics.FileVersionInfo]::GetVersionInfo($taskProcess.Path).FileDescription -eq 'Median XL Launcher') { throw 'Close the Median XL launcher before restoring.' }
}
$taskBackup=(Resolve-Path -LiteralPath $BackupDirectory).Path
$taskReceipt=Get-Content -LiteralPath (Join-Path $taskBackup 'receipt.json') -Raw | ConvertFrom-Json
$taskGame=(Resolve-Path -LiteralPath $taskReceipt.game_directory).Path
$taskExpected=@('d2fps.dll','glide3x.dll','ddraw.dll','d2gl.mpq','d2gl.ini','d2fps.ini')
if ($taskReceipt.status -ne 'installed-awaiting-game-launch' -or $taskReceipt.files.Count -ne 6) { throw 'This is not an installed package receipt.' }
if (@($taskReceipt.files.name | Sort-Object -Unique).Count -ne 6) { throw 'Duplicate receipt entries.' }
$taskChangedConfigs=@()
foreach ($taskEntry in $taskReceipt.files) {
    if ($taskEntry.name -notin $taskExpected) { throw 'Unexpected rollback filename.' }
    $taskTarget=Join-Path $taskGame $taskEntry.name
    if (-not (Test-Path -LiteralPath $taskTarget)) { throw "Installed file is missing: $($taskEntry.name)" }
    if ((Get-FileHash -LiteralPath $taskTarget).Hash -ne $taskEntry.after_sha256) {
        if ($taskEntry.name -in @('d2gl.ini','d2fps.ini')) { $taskChangedConfigs += $taskEntry.name }
        else { throw "Binary changed since installation: $($taskEntry.name). Review its version before restoring the backup." }
    }
    if ($taskEntry.before_sha256 -and (Get-FileHash -LiteralPath (Join-Path $taskBackup $taskEntry.name)).Hash -ne $taskEntry.before_sha256) { throw 'Backup checksum failed.' }
}
if ($taskChangedConfigs.Count) {
    $taskSavedConfigs=Join-Path $taskBackup ('post-install-configs-'+[guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $taskSavedConfigs | Out-Null
    foreach ($taskName in $taskChangedConfigs) {
        $taskCurrent=Join-Path $taskGame $taskName
        $taskSaved=Join-Path $taskSavedConfigs $taskName
        Copy-Item -LiteralPath $taskCurrent -Destination $taskSaved
        if ((Get-FileHash -LiteralPath $taskSaved).Hash -ne (Get-FileHash -LiteralPath $taskCurrent).Hash) { throw 'Could not preserve the edited configuration.' }
    }
    Write-Output "Your later INI edits are preserved in: $taskSavedConfigs"
}
foreach ($taskEntry in $taskReceipt.files) {
    $taskTarget=Join-Path $taskGame $taskEntry.name
    if ($taskEntry.before_sha256) {
        Copy-Item -LiteralPath (Join-Path $taskBackup $taskEntry.name) -Destination $taskTarget -Force
        if ((Get-FileHash -LiteralPath $taskTarget).Hash -ne $taskEntry.before_sha256) { throw 'Restored file checksum failed.' }
    } else { Remove-Item -LiteralPath $taskTarget }
}
$taskReceipt.status='restored'
$taskReceipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $taskBackup 'receipt.json')
Write-Output 'The previous game files and INIs have been restored.'
