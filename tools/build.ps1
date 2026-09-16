param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [ValidateSet('v145','v143')][string]$Toolset = 'v145',
    [switch]$Analyze
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path $vswhere)) { throw 'Instale Visual Studio com Desenvolvimento para desktop com C++.' }
$msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (!$msbuild) { throw 'MSBuild nao encontrado.' }
$buildArguments = @((Join-Path $root 'VoxelNative.sln'), '/m', '/t:Rebuild', "/p:Configuration=$Configuration", '/p:Platform=x64', "/p:PlatformToolset=$Toolset")
if ($Analyze) { $buildArguments += '/p:RunCodeAnalysis=true' }
& $msbuild @buildArguments
if ($LASTEXITCODE -ne 0) { throw 'Compilacao falhou.' }
& (Join-Path $root "bin\x64\$Configuration\CoreTests.exe")
if ($LASTEXITCODE -ne 0) { throw 'Testes falharam.' }
Write-Host "Compilacao e testes OK. Executavel: $root\bin\x64\$Configuration\VoxelNative.exe"
