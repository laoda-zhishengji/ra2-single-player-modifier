$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $projectRoot 'build-vs\Release'
$distRoot = Join-Path $projectRoot 'dist\RA2SinglePlayerModifier'

$productFiles = @(
    'ra2_product_hub.exe',
    'ra2_product_status.exe',
    'ra2_product_control.exe',
    'ra2_product_production.exe',
    'ra2_product_super.exe',
    'ra2_product_fog.exe',
    'ra2_product_build_distance.exe',
    'ra2_product_services.exe',
    'ra2_product_auto_repair.exe',
    'ra2_product_garrison_repair.exe',
    'ra2_product_instant_service.exe',
    'ra2_product_super_service.exe'
)

New-Item -ItemType Directory -Force -Path $distRoot | Out-Null
foreach ($runtimePath in @(
    (Join-Path $distRoot 'product-state'),
    (Join-Path $distRoot 'ra2_modifier_ui.exe')
)) {
    if (Test-Path -LiteralPath $runtimePath) {
        Remove-Item -LiteralPath $runtimePath -Recurse -Force
    }
}
foreach ($name in $productFiles) {
    $source = Join-Path $buildRoot $name
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Missing product component: $name"
    }
    Copy-Item -LiteralPath $source -Destination (Join-Path $distRoot $name) -Force
}
$newUi = Join-Path $buildRoot 'ra2_modifier_ui.exe'
if (-not (Test-Path -LiteralPath $newUi -PathType Leaf)) {
    throw "Missing product component: ra2_modifier_ui.exe"
}
Copy-Item -LiteralPath $newUi -Destination (Join-Path $distRoot 'ra2_product_ui.exe') -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'README.txt') -Destination (Join-Path $distRoot 'README.txt') -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'RELEASE_MANIFEST.txt') -Destination (Join-Path $distRoot 'RELEASE_MANIFEST.txt') -Force
$assetSource = Join-Path $projectRoot 'assets\tactical-map.png'
if (-not (Test-Path -LiteralPath $assetSource -PathType Leaf)) { throw 'Missing UI art asset: tactical-map.png' }
$heroSource = Join-Path $projectRoot 'assets\ra2-steam-hero.png'
if (-not (Test-Path -LiteralPath $heroSource -PathType Leaf)) { throw 'Missing UI art asset: ra2-steam-hero.png' }
$iconsSource = Join-Path $projectRoot 'assets\feature-icons.png'
if (-not (Test-Path -LiteralPath $iconsSource -PathType Leaf)) { throw 'Missing UI art asset: feature-icons.png' }
New-Item -ItemType Directory -Force -Path (Join-Path $distRoot 'assets') | Out-Null
Copy-Item -LiteralPath $assetSource -Destination (Join-Path $distRoot 'assets\tactical-map.png') -Force
Copy-Item -LiteralPath $heroSource -Destination (Join-Path $distRoot 'assets\ra2-steam-hero.png') -Force
Copy-Item -LiteralPath $iconsSource -Destination (Join-Path $distRoot 'assets\feature-icons.png') -Force
$allowedFiles = $productFiles + @('ra2_product_ui.exe', 'README.txt', 'RELEASE_MANIFEST.txt')
$unexpected = Get-ChildItem -LiteralPath $distRoot -File | Where-Object { $_.Name -notin $allowedFiles }
if ($unexpected) {
    throw "Unexpected files in release directory: $($unexpected.Name -join ', ')"
}
Write-Output "RELEASE_PACKAGE=$distRoot"
Write-Output "COMPONENTS=$($productFiles.Count)"
$hub = Join-Path $distRoot 'ra2_product_hub.exe'
& $hub --self-check
if ($LASTEXITCODE -ne 0) {
    throw "Packaged product self-check failed with exit code $LASTEXITCODE"
}
Write-Output 'PACKAGE_SELF_CHECK=PASS'
