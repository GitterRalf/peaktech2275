param(
    [string]$QtKit,
    [string]$CompilerBin,
    [ValidateSet('x64', 'x86')]
    [string]$Architecture = 'x64'
)

$ErrorActionPreference = 'Stop'

if ($Architecture -eq 'x86') {
    if (!$QtKit) { $QtKit = 'C:/Qt/Qt5.15.19/mingw81_32' }
    if (!$CompilerBin) { $CompilerBin = 'C:/Qt/Qt5.13.2/Tools/mingw810_32/bin' }
    $qtMajor = 5
    $triple = 'i686-w64-mingw32'
    $buildName = 'build-x86'
} else {
    if (!$QtKit) { $QtKit = 'C:/Qt/Qt6/6.11.0/mingw_64' }
    if (!$CompilerBin) { $CompilerBin = 'C:/Qt/Tools/mingw13_1_0/mingw64/bin' }
    $qtMajor = 6
    $triple = 'x86_64-w64-mingw32'
    $buildName = 'build'
}
$compiler = "$CompilerBin/g++.exe"
if ((& $compiler -dumpmachine) -ne $triple) {
    throw "Für $Architecture wird ein MinGW-Compiler für $triple benötigt."
}
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
    $build = Join-Path $source $buildName
    $configure = @('-S', $source, '-B', $build, '-G', 'Ninja',
                   "-DCMAKE_PREFIX_PATH=$QtKit", "-DCMAKE_CXX_COMPILER=$compiler", "-DDEVICE_QT_MAJOR=$qtMajor",
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
    if ($qtMajor -eq 6) {
        & "$QtKit/bin/windeployqt.exe" --release --no-translations --no-system-d3d-compiler --no-opengl-sw $exe
        if ($LASTEXITCODE -ne 0) { throw "Bereitstellen der Qt-Bibliotheken fehlgeschlagen ($LASTEXITCODE)." }
    } else {
        # This Qt 5 MinGW kit's release plugins contain debug info.
        # Copy from the kit directly because windeployqt skips those plugins.
        $qtModules = 'Core', 'Gui', 'Network', 'Svg', 'Widgets', 'SerialPort', 'OpenGL'
        foreach ($module in $qtModules) {
            Copy-Item -LiteralPath "$QtKit/bin/Qt5$module.dll" -Destination $build
        }
        foreach ($dll in 'libgcc_s_dw2-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll') {
            Copy-Item -LiteralPath "$CompilerBin/$dll" -Destination $build
        }
        foreach ($dll in 'libEGL.dll', 'libGLESv2.dll') {
            if (Test-Path -LiteralPath "$QtKit/bin/$dll") {
                Copy-Item -LiteralPath "$QtKit/bin/$dll" -Destination $build
            }
        }
        Set-Content -LiteralPath (Join-Path $build 'qt.conf') -Encoding ascii -Value '[Paths]', 'Prefix=.', 'Plugins=.'
        $pluginRoot = Join-Path $QtKit 'plugins'
        foreach ($group in 'bearer', 'iconengines', 'imageformats', 'styles') {
            $pluginDir = Join-Path $pluginRoot $group
            if (Test-Path -LiteralPath $pluginDir -PathType Container) {
                $target = New-Item -ItemType Directory -Path (Join-Path $build $group) -Force
                Get-ChildItem -LiteralPath $pluginDir -File -Filter '*.dll' |
                    Copy-Item -Destination $target.FullName
            }
        }
        $platforms = New-Item -ItemType Directory -Path (Join-Path $build 'platforms') -Force
        Copy-Item -LiteralPath (Join-Path $pluginRoot 'platforms/qwindows.dll') -Destination $platforms.FullName
    }
}
finally {
    $env:Path = $originalPath
    if ($mappedDrive) { & $subst $mappedDrive /d }
}
