$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskDependencies=Join-Path $taskRoot 'external\DiligentCore'
$taskRevision='bf9b8aef111f9068dcc4de7573b652bc92cfa142'
if(-not (Test-Path -LiteralPath $taskDependencies)){
    New-Item -ItemType Directory -Path (Split-Path -Parent $taskDependencies) -Force | Out-Null
    git -c core.longpaths=true clone --no-checkout --depth 1 https://github.com/DiligentGraphics/DiligentCore.git $taskDependencies
    if($LASTEXITCODE -ne 0){throw 'Shader dependency checkout failed.'}
    git -C $taskDependencies -c core.longpaths=true fetch --depth 1 origin $taskRevision
    if($LASTEXITCODE -ne 0){throw 'Pinned dependency fetch failed.'}
    git -C $taskDependencies -c core.longpaths=true checkout --detach $taskRevision
    if($LASTEXITCODE -ne 0){throw 'Pinned dependency checkout failed.'}
}
$taskActual=git -C $taskDependencies rev-parse HEAD
if($LASTEXITCODE -ne 0 -or $taskActual -ne $taskRevision){throw 'The existing shader dependency directory is not the pinned revision. Preserve any edits and use a fresh dependency directory.'}
git -C $taskDependencies -c core.longpaths=true submodule update --init --depth 1 ThirdParty/glslang ThirdParty/SPIRV-Cross
if($LASTEXITCODE -ne 0){throw 'Shader compiler dependency setup failed.'}
$taskExpected=@{
    'ThirdParty/glslang'='275822a6261ee689aadb1da5f09a0ec2f058685c'
    'ThirdParty/SPIRV-Cross'='1a6169566c73d3da552748fc372fe2bbb856e46e'
}
foreach($taskName in $taskExpected.Keys){
    $taskPath=Join-Path $taskDependencies $taskName
    $taskActual=git -C $taskPath rev-parse HEAD
    if($LASTEXITCODE -ne 0 -or $taskActual -ne $taskExpected[$taskName]){throw "Dependency identity mismatch: $taskName"}
}
Write-Output 'Pinned shader compiler sources are ready.'
