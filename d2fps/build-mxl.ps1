$ErrorActionPreference = 'Stop'
$taskOldFlags = $env:CARGO_ENCODED_RUSTFLAGS
$taskCargoRoot = if ($env:CARGO_HOME) { $env:CARGO_HOME } else { Join-Path $env:USERPROFILE '.cargo' }
$taskFlags = @(
    '-Ctarget-feature=+crt-static'
    "--remap-path-prefix=$PSScriptRoot=source"
    "--remap-path-prefix=$taskCargoRoot=rust-dependencies"
)
Push-Location $PSScriptRoot
try {
    # Encoded arguments preserve spaces in Windows paths. Restore the caller's
    # environment afterward; no global Rust or game configuration is changed.
    $env:CARGO_ENCODED_RUSTFLAGS = $taskFlags -join [char]0x1f
    New-Item -ItemType Directory -Path 'target' -Force | Out-Null
    cargo test --locked -p d2fps --lib --release 2>&1 | Tee-Object -FilePath 'target/mxl-tests.log'
    if ($LASTEXITCODE -ne 0) { throw 'Native x86 tests failed.' }
    cargo build --locked -p d2fps --release 2>&1 | Tee-Object -FilePath 'target/mxl-build.log'
    if ($LASTEXITCODE -ne 0) { throw 'D2FPS DLL build failed.' }
    Get-FileHash -Algorithm SHA256 -LiteralPath 'target/i686-pc-windows-msvc/release/d2fps.dll'
} finally {
    $env:CARGO_ENCODED_RUSTFLAGS = $taskOldFlags
    Pop-Location
}
