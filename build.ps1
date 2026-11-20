param(
    [string]$QtKit = 'C:/Qt/Qt6/6.11.0/mingw_64',
    [string]$CompilerBin = 'C:/Qt/Tools/mingw13_1_0/mingw64/bin'
)

$ErrorActionPreference = 'Stop'
$originalPath = $env:Path
$source = $PSScriptRoot
$mappedDrive = $null
$subst = Join-Path $env:SystemRoot 'System32\subst.exe'

try {
    if ($source -match '[^\x00-\x7F]') {
        # MinGW generators can misread the umlaut in the workspace path.
        $drives = @(Get-PSDrive -PSProvider FileSystem | Select-Object -ExpandProperty Name)
        $letter = 'V','W','P','N','O','I','H','G','B' | Where-Object { $_ -notin $drives } | Select-Object -First 1
        if (!$letter) { throw 'Kein freier Laufwerksbuchstabe für den MinGW-Build.' }
        & $subst "${letter}:" $source
        if ($LASTEXITCODE -ne 0) { throw 'Temporärer Laufwerksalias konnte nicht eingerichtet werden.' }
        $mappedDrive = "${letter}:"
        $source = "$mappedDrive/"
    }

    $env:Path = "$CompilerBin;$QtKit/bin;$originalPath"
    $build = Join-Path $source 'build'
    $configure = @('-S', $source, '-B', $build, '-G', 'Ninja',
                   "-DCMAKE_PREFIX_PATH=$QtKit", "-DCMAKE_CXX_COMPILER=$CompilerBin/g++.exe",
                   '-DCMAKE_BUILD_TYPE=Release')
    $cache = Join-Path $build 'CMakeCache.txt'
    if (Test-Path -LiteralPath $cache) {
        $cachedSource = Select-String -LiteralPath $cache -Pattern '^CMAKE_HOME_DIRECTORY:INTERNAL=(.*)$'
        $expected = $source.Replace('\', '/').TrimEnd('/')
        if (!$cachedSource -or $cachedSource.Matches[0].Groups[1].Value.TrimEnd('/') -ne $expected) {
            $configure += '--fresh'
        }
    }
    & cmake @configure
    if ($LASTEXITCODE -ne 0) { throw "CMake-Konfiguration fehlgeschlagen ($LASTEXITCODE)." }
    & cmake --build $build --parallel 4
    if ($LASTEXITCODE -ne 0) { throw "Build fehlgeschlagen ($LASTEXITCODE)." }
    $exe = Join-Path $build 'PeakTech2275.exe'
    & "$QtKit/bin/windeployqt.exe" --release --no-translations --no-system-d3d-compiler --no-opengl-sw $exe
    if ($LASTEXITCODE -ne 0) { throw "Bereitstellen der Qt-Bibliotheken fehlgeschlagen ($LASTEXITCODE)." }
}
finally {
    $env:Path = $originalPath
    if ($mappedDrive) { & $subst $mappedDrive /d }
}
