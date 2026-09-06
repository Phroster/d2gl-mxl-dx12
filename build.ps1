param([string]$VisualStudioPath)
$ErrorActionPreference = 'Stop'
$taskSavedInclude = $env:INCLUDE
$taskSavedLib = $env:LIB
if (-not $VisualStudioPath) {
    $taskVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $VisualStudioPath = & $taskVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
if (-not $VisualStudioPath) { throw 'Visual Studio C++ Build Tools were not found.' }
$taskMsvc = Get-ChildItem -LiteralPath (Join-Path $VisualStudioPath 'VC\Tools\MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1
$taskSdk = (Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots').KitsRoot10
$taskSdkVersion = Get-ChildItem -LiteralPath (Join-Path $taskSdk 'Include') -Directory | Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'um\Windows.h') } | Sort-Object Name -Descending | Select-Object -First 1
if (-not $taskMsvc -or -not $taskSdkVersion) { throw 'The C++ compiler or Windows SDK was not found.' }
$taskCompiler = Join-Path $taskMsvc.FullName 'bin\Hostx64\x86\cl.exe'
$taskMsbuild = Join-Path $VisualStudioPath 'MSBuild\Current\Bin\MSBuild.exe'
$taskSdkVersion = $taskSdkVersion.Name
$taskOutput = Join-Path $PSScriptRoot 'build\bin'
New-Item -ItemType Directory -Path $taskOutput -Force | Out-Null
Push-Location $PSScriptRoot
try {
    $env:INCLUDE = "$($taskMsvc.FullName)\include;$taskSdk\Include\$taskSdkVersion\ucrt;$taskSdk\Include\$taskSdkVersion\shared;$taskSdk\Include\$taskSdkVersion\um"
    $env:LIB = "$($taskMsvc.FullName)\lib\x86;$taskSdk\Lib\$taskSdkVersion\ucrt\x86;$taskSdk\Lib\$taskSdkVersion\um\x86"
    & $taskCompiler /nologo /O2 /W4 /WX /MT /D_CRT_SECURE_NO_WARNINGS /DMXL_SMOOTHING_TEST 'd2gl\d2gl\src\mxl_smoothing.c' "/Fe:$taskOutput\mxl-smoothing-tests.exe" "/Fo:$taskOutput\mxl-smoothing-tests.obj" /link /DYNAMICBASE /NXCOMPAT advapi32.lib
    if ($LASTEXITCODE -ne 0) { throw 'Native smoothing test build failed.' }
    & (Join-Path $taskOutput 'mxl-smoothing-tests.exe') | Tee-Object -FilePath 'build\native-tests.log'
    if ($LASTEXITCODE -ne 0) { throw 'Native smoothing tests failed.' }
    foreach ($taskRenderer in @('glide3x','ddraw')) {
        $taskProject = Join-Path $PSScriptRoot "d2gl\$taskRenderer\$taskRenderer.vcxproj"
        & $taskMsbuild $taskProject /t:Build /m /nologo /v:minimal /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 "/p:WindowsTargetPlatformVersion=$taskSdkVersion" "/p:SolutionDir=$PSScriptRoot\d2gl\" "/p:OutDir=$taskOutput\" "/p:IntDir=$PSScriptRoot\build\obj\$taskRenderer\" /p:PostBuildEventUseInBuild=false "/p:GameDir=$PSScriptRoot\build\no-auto-deploy\" 2>&1 | Tee-Object -FilePath "build\$taskRenderer-build.log"
        if ($LASTEXITCODE -ne 0) { throw "$taskRenderer build failed; see its build log." }
    }
    Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $taskOutput 'glide3x.dll'),(Join-Path $taskOutput 'ddraw.dll')
} finally {
    $env:INCLUDE = $taskSavedInclude
    $env:LIB = $taskSavedLib
    Pop-Location
}
