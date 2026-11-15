param(
    [Parameter(Mandatory = $true)]
    [string]$PackageRoot,
    [switch]$SkipCommentCheck
)

$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath($PackageRoot)
$errors = [System.Collections.Generic.List[string]]::new()

function Require-File([string]$RelativePath) {
    $path = Join-Path $root $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        $errors.Add("缺少必需文件: $RelativePath")
    }
}

if (-not (Test-Path -LiteralPath $root -PathType Container)) {
    Write-Error "交付目录不存在: $root"
}

$required = @(
    'README.md',
    'k230/big-core/main.cc',
    'k230/big-core/falldown_detect.cc',
    'k230/little-core/rtsp-server/src/main.cpp',
    'k230/little-core/status-client/status_tcp_client.cpp',
    'k230/model/yolov5n-falldown.kmodel',
    'k230/bin/falldown_rtsp_status_safe_v3.elf',
    'k230/bin/rtspServer',
    'k230/bin/k230_status_tcp_client',
    'rk3566/status-service/rk3566_status_tcp_server.c',
    'rk3566/status-service/tests/test_protocol.c',
    'rk3566/qt-viewer/src/main.cpp',
    'rk3566/qt-viewer/src/rtsp_decoder.cpp',
    'docs/00-从零开始.md',
    'docs/01-系统架构与数据流.md',
    'docs/02-逐文件详解.md',
    'docs/03-编译部署运行.md',
    'docs/04-协议与接口.md',
    'docs/05-故障排查与验收.md',
    'docs/06-版本依赖与限制.md',
    'manifests/source-map.csv'
)
$required | ForEach-Object { Require-File $_ }

$forbiddenDirs = Get-ChildItem -LiteralPath $root -Directory -Recurse -Force |
    Where-Object { $_.Name -in @('CMakeFiles', '.git', '__pycache__') -or $_.FullName -match '[\\/](out|build)[\\/]' }
foreach ($dir in $forbiddenDirs) {
    $errors.Add("包含不应交付的构建/缓存目录: $($dir.FullName.Substring($root.Length + 1))")
}

$codeExtensions = @('.c', '.cc', '.cpp', '.h', '.hpp', '.cmake', '.sh')
$codeFiles = Get-ChildItem -LiteralPath $root -File -Recurse |
    Where-Object { $codeExtensions -contains $_.Extension.ToLowerInvariant() -or $_.Name -in @('CMakeLists.txt', 'Makefile') }
if ($codeFiles.Count -lt 35) {
    $errors.Add("源码文件数量异常: $($codeFiles.Count)，预期至少 35")
}

if (-not $SkipCommentCheck) {
    foreach ($file in $codeFiles) {
        $text = Get-Content -LiteralPath $file.FullName -Raw
        if ($text -notmatch '交付版注释') {
            $errors.Add("源码缺少交付版中文文件头: $($file.FullName.Substring($root.Length + 1))")
        }
    }
}

$modelPath = Join-Path $root 'k230/model/yolov5n-falldown.kmodel'
if ((Test-Path -LiteralPath $modelPath) -and (Get-Item -LiteralPath $modelPath).Length -lt 1MB) {
    $errors.Add('KModel 文件尺寸异常，小于 1 MiB')
}

$detailPath = Join-Path $root 'docs/02-逐文件详解.md'
if (Test-Path -LiteralPath $detailPath) {
    $detail = Get-Content -LiteralPath $detailPath -Raw
    foreach ($file in $codeFiles) {
        $relative = $file.FullName.Substring($root.Length + 1).Replace('\', '/')
        if (-not $detail.Contains($relative)) {
            $errors.Add("逐文件文档未覆盖: $relative")
        }
    }
}

if ($errors.Count -gt 0) {
    $errors | ForEach-Object { Write-Host "[FAIL] $_" -ForegroundColor Red }
    Write-Error "交付包检查失败，共 $($errors.Count) 项"
}

Write-Host "[PASS] 交付包结构检查通过；源码文件 $($codeFiles.Count) 个。" -ForegroundColor Green

