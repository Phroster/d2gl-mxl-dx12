param(
    [string]$BuildDirectory=(Join-Path ([IO.Path]::GetTempPath()) 'MXL-DX12-Build'),
    [switch]$SkipGpuTests
)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'setup.ps1')
if(-not $?){throw 'Dependency setup failed.'}
$taskBuild=[IO.Path]::GetFullPath($BuildDirectory)
if($taskBuild.Length -gt 80){throw 'Use a shorter build directory, for example C:\MXL-DX12-Build, to avoid MSBuild path limits.'}
cmake -S $taskRoot -B $taskBuild -G 'Visual Studio 17 2022' -A Win32
if($LASTEXITCODE -ne 0){throw 'CMake configuration failed.'}
$taskTargets=@('glide3x','ddraw','dx12_shader_test','dx12_timing_test')
if(-not $SkipGpuTests){$taskTargets+=@('dx12_device_test','dx12_render_test','dx12_glide_test','dx12_menu_test','dx12_diagnostics_test','dx12_upload_cache_test','dx12_input_profile_test','dx12_reveal_probe_test','dx12_auto_reveal_test')}
cmake --build $taskBuild --config Release --target $taskTargets --parallel 6
if($LASTEXITCODE -ne 0){throw 'DX12 build failed.'}
$taskTests=@('dx12_shader_test','dx12_timing_test')
if(-not $SkipGpuTests){$taskTests+=@('dx12_device_test','dx12_render_test','dx12_glide_test','dx12_menu_test','dx12_diagnostics_test','dx12_upload_cache_test','dx12_input_profile_test','dx12_reveal_probe_test','dx12_auto_reveal_test')}
foreach($taskTest in $taskTests){
    if($taskTest -in @('dx12_diagnostics_test','dx12_upload_cache_test','dx12_input_profile_test','dx12_reveal_probe_test','dx12_auto_reveal_test')){
        & (Join-Path $taskBuild "Release\$taskTest.exe") (Join-Path $taskBuild ($taskTest+'-'+(Get-Date -Format 'yyyyMMdd-HHmmss')))
    }else{
        & (Join-Path $taskBuild "Release\$taskTest.exe")
    }
    if($LASTEXITCODE -ne 0){throw "Verification failed: $taskTest"}
}
Write-Output "DX12 DLLs: $taskBuild\Release"
Write-Output 'This build does not install files into your game.'
