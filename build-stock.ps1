# 编译 Stock 插件为 DLL（在项目根目录执行）
# 用法: .\build-stock.ps1
# 可选: .\build-stock.ps1 -Config Debug -Platform Win32

param(
    [string]$Config = "Release",
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $scriptDir

# 查找 MSBuild
$msbuild = $null
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $found = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" 2>$null
    if ($found) { $msbuild = $found | Select-Object -First 1 }
}
if (-not $msbuild) {
    $vsRoot = "C:\Program Files\Microsoft Visual Studio\2022"
    foreach ($edition in @("Community", "Professional", "Enterprise", "BuildTools")) {
        $p = Join-Path $vsRoot "$edition\MSBuild\Current\Bin\MSBuild.exe"
        if (Test-Path $p) { $msbuild = $p; break }
    }
}
if (-not $msbuild) {
    Write-Host "未找到 MSBuild。请任选其一："
    Write-Host "  1. 从开始菜单打开「Developer PowerShell for VS 2022」，在该窗口执行："
    Write-Host "     cd $scriptDir"
    Write-Host "     msbuild TrafficMonitorPlugins.sln /t:Stock /p:Configuration=$Config /p:Platform=$Platform"
    Write-Host "  2. 或确认已安装 Visual Studio 2022 且包含「使用 C++ 的桌面开发」工作负载。"
    exit 1
}

Write-Host "使用 MSBuild: $msbuild"
Write-Host "编译 Stock 项目: Configuration=$Config, Platform=$Platform"
& $msbuild TrafficMonitorPlugins.sln /t:Stock /p:Configuration=$Config /p:Platform=$Platform /v:minimal
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# 输出目录提示
if ($Platform -eq "Win32") {
    $outDir = "bin\$Config"
} else {
    $outDir = "bin\$Platform\$Config"
}
$dll = Join-Path $scriptDir "$outDir\Stock.dll"
if (Test-Path $dll) {
    Write-Host "`n生成成功: $dll"
} else {
    Write-Host "`n请检查输出目录: $outDir"
}
