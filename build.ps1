param(
    [string]$BuildDirectory=(Join-Path ([IO.Path]::GetTempPath()) 'MXL-DX12-Build'),
    [switch]$SkipGpuTests
)
& (Join-Path $PSScriptRoot 'experimental\build.ps1') -BuildDirectory $BuildDirectory -SkipGpuTests:$SkipGpuTests
if(-not $?){throw 'DX12 experiment build failed.'}
