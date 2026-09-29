param(
    [string]$QtBin = 'D:\QT\Qt5.14.2\5.14.2\mingw73_64\bin',
    [string]$CompilerBin = 'D:\QT\Qt5.14.2\Tools\mingw730_64\bin'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$originalPath = $env:PATH
$originalPlatform = $env:QT_QPA_PLATFORM
try {
    $env:PATH = "$CompilerBin;$QtBin;$originalPath"
    $env:QT_QPA_PLATFORM = 'offscreen'
    $jobs = @(
        @{ Directory = 'PasswordHash'; Project = 'tests/passwordhash.pro'; Binary = 'passwordhash_tests.exe' },
        @{ Directory = 'Tests'; Project = 'tests/security/security.pro'; Binary = 'security_tests.exe' }
    )
    foreach ($job in $jobs) {
        $buildPath = Join-Path $projectRoot ('.build-security/' + $job.Directory)
        New-Item -ItemType Directory -Force -Path $buildPath | Out-Null
        Push-Location $buildPath
        try {
            & (Join-Path $QtBin 'qmake.exe') (Join-Path $projectRoot $job.Project) -spec win32-g++ 'CONFIG+=debug'
            if ($LASTEXITCODE -ne 0) { throw 'qmake failed' }
            & (Join-Path $CompilerBin 'mingw32-make.exe') -j4
            if ($LASTEXITCODE -ne 0) { throw 'Test build failed' }
            & (Join-Path $buildPath ('debug/' + $job.Binary))
            if ($LASTEXITCODE -ne 0) { throw ('Test failed: ' + $job.Binary) }
        } finally { Pop-Location }
    }
} finally {
    $env:PATH = $originalPath
    $env:QT_QPA_PLATFORM = $originalPlatform
}
