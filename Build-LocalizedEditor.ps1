# Builds the standalone F8 editor for HellForge 0.5.0-nightly.20261115.
# -Test runs the editor tests. -EditorDataDir enables overlay tests with installed editor data.
# -UpstreamRepo points to a read-only Git checkout containing the pinned release commit.
param(
    [switch]$Test,
    [string]$EditorDataDir,
    [string]$UpstreamRepo = 'E:\EXCHANGE\github-C\SkyeShade_HellForge'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$ProgressPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

if ($EditorDataDir) {
    if (-not $Test) { throw '-EditorDataDir requires -Test.' }
    $EditorDataDir = (Resolve-Path -LiteralPath $EditorDataDir).ProviderPath
    if (-not (Test-Path -LiteralPath (Join-Path $EditorDataDir 'editor-catalog.json') -PathType Leaf)) {
        throw 'The editor data directory must contain editor-catalog.json.'
    }
}

function Get-DirectoryPrefix([string]$Path) {
    $fullPath = [IO.Path]::GetFullPath($Path)
    # Normalize DOS and UNC device paths for comparison only.
    $fullPath = $fullPath -ireplace '^\\\\[.?]\\UNC\\', '\\'
    $fullPath = $fullPath -creplace '^\\\\[.?]\\(?=[A-Za-z]:\\)', ''
    return $fullPath.TrimEnd('\', '/') + '\'
}

$releaseCommit = 'ba81dbb4d72217b34d5c26186249c5dd2a98e72a'
$UpstreamRepo = (Resolve-Path -LiteralPath $UpstreamRepo).ProviderPath
$upstreamPrefix = Get-DirectoryPrefix $UpstreamRepo
$pathComparison = [StringComparison]::OrdinalIgnoreCase
if ((Get-DirectoryPrefix $PSScriptRoot).StartsWith($upstreamPrefix, $pathComparison)) {
    throw 'The localization workspace must be outside the read-only upstream checkout.'
}
$tools = Join-Path $PSScriptRoot 'tools'
$out = Join-Path $PSScriptRoot 'out'
foreach ($directory in @($tools, $out)) {
    if ($upstreamPrefix.StartsWith((Get-DirectoryPrefix $directory), $pathComparison)) {
        throw "The read-only upstream checkout overlaps a build output directory: $directory"
    }
}
$readGit = @('--no-optional-locks', '--no-lazy-fetch', '--no-replace-objects', '-C', $UpstreamRepo)
$gitRoot = & git @readGit rev-parse --show-toplevel
if ($LASTEXITCODE -ne 0 -or -not [string]::Equals((Get-DirectoryPrefix $gitRoot), $upstreamPrefix, $pathComparison)) {
    throw "Specify the root of the upstream Git checkout with -UpstreamRepo: $UpstreamRepo"
}
$upstreamCommit = & git @readGit rev-parse --verify "$releaseCommit^{commit}"
if ($LASTEXITCODE -ne 0 -or $upstreamCommit -cne $releaseCommit) {
    throw "The upstream checkout does not contain the pinned editor commit $releaseCommit."
}

$localSources = Join-Path $PSScriptRoot 'src'
$objects = Join-Path $out 'obj'
foreach ($directory in @($tools, $out, $objects)) {
    $item = New-Item -ItemType Directory -Path $directory -Force
    if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
        throw "Build output must be a regular directory: $directory"
    }
}
$staging = Join-Path $objects ('editor-' + [Guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $staging

# Keep the compiler caches under the localization workspace.
$priorLocal = $env:ZIG_LOCAL_CACHE_DIR
$priorGlobal = $env:ZIG_GLOBAL_CACHE_DIR
try {
    # Combine the pinned editor sources with the localization files in one temporary tree.
    $archive = Join-Path $staging 'upstream.zip'
    & git @readGit archive --format=zip "--output=$archive" $releaseCommit -- native/editor native/mod-sdk scripts/Get-BuildTools.ps1
    if ($LASTEXITCODE -ne 0) { throw "Could not archive the upstream editor at $releaseCommit." }
    Expand-Archive -LiteralPath $archive -DestinationPath $staging
    $src = Join-Path $staging 'native/editor'
    $sdk = Join-Path $staging 'native/mod-sdk'
    foreach ($file in Get-ChildItem -LiteralPath $localSources -File -Recurse) {
        $relative = $file.FullName.Substring($localSources.Length + 1)
        $destination = Join-Path $src $relative
        $null = [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination))
        [IO.File]::Copy($file.FullName, $destination, $true)
    }
    Write-Output ("Upstream editor source: {0} ({1})" -f $UpstreamRepo, $releaseCommit)
    $buildTools = & (Join-Path $staging 'scripts/Get-BuildTools.ps1') -Root $PSScriptRoot
    $zig = $buildTools.Zig

    $imguiSha = 'FD37507C8476A6D14CC7C4B352401F31BCBD0F0D995D35390811E968C466F46E'
    $imguiArchive = Join-Path $buildTools.Downloads 'imgui-1.91.9b.zip'
    if (-not (Test-Path -LiteralPath $imguiArchive -PathType Leaf) -or (Get-FileHash -LiteralPath $imguiArchive).Hash -ne $imguiSha) {
        $partial = $imguiArchive + '.partial'
        Invoke-WebRequest -Uri 'https://github.com/ocornut/imgui/archive/refs/tags/v1.91.9b.zip' -OutFile $partial -UseBasicParsing
        if ((Get-FileHash -LiteralPath $partial).Hash -ne $imguiSha) {
            Remove-Item -LiteralPath $partial
            throw 'Dear ImGui does not match its pinned checksum.'
        }
        Move-Item -LiteralPath $partial -Destination $imguiArchive -Force
    }
    $imguiRoot = Join-Path $tools 'imgui'
    $imgui = Join-Path $imguiRoot 'imgui-1.91.9b'
    $imguiMarker = Join-Path $imguiRoot '.hellforge-archive-sha256'
    if (-not (Test-Path -LiteralPath $imguiMarker -PathType Leaf) -or [IO.File]::ReadAllText($imguiMarker) -cne $imguiSha) {
        Expand-Archive -LiteralPath $imguiArchive -DestinationPath $imguiRoot -Force
        [IO.File]::WriteAllText($imguiMarker, $imguiSha)
    }

    $env:ZIG_LOCAL_CACHE_DIR = Join-Path $tools 'zig-local-cache'
    $env:ZIG_GLOBAL_CACHE_DIR = Join-Path $tools 'zig-global-cache'
    # Preserve the source paths embedded in the r4 build, including ImGui's __FILE__.
    $common = @(
        '-target', 'x86_64-windows-gnu', '-O2', '-std=c++17', '-fno-exceptions', '-fno-rtti',
        '-I', $sdk, '-I', $imgui, '-I', (Join-Path $imgui 'backends'),
        '-DIMGUI_DISABLE_OBSOLETE_FUNCTIONS', '-DIMGUI_USE_WCHAR32', ('-ffile-prefix-map=' + $PSScriptRoot + '=./hellforge-editor-zh-cn')
    )
    $imguiSources = @(
        'imgui.cpp', 'imgui_draw.cpp', 'imgui_tables.cpp', 'imgui_widgets.cpp', 'imgui_demo.cpp',
        'backends/imgui_impl_win32.cpp', 'backends/imgui_impl_dx11.cpp', 'backends/imgui_impl_dx12.cpp'
    )
    $shaderLine = 'static const char* pixelShader ='
    $shaderHook = 'extern const char* g_hellforgePixelShader; const char* pixelShader = g_hellforgePixelShader != nullptr ? g_hellforgePixelShader :'
    $imguiObjects = foreach ($name in $imguiSources) {
        $source = Join-Path $imgui $name
        if ($name -in 'backends/imgui_impl_dx11.cpp', 'backends/imgui_impl_dx12.cpp') {
            $text = [IO.File]::ReadAllText($source)
            if ([regex]::Matches($text, [regex]::Escape($shaderLine)).Count -ne 1) {
                throw "The HDR shader hook does not fit $name."
            }
            $source = Join-Path $objects ([IO.Path]::GetFileName($source))
            [IO.File]::WriteAllText($source, $text.Replace($shaderLine, $shaderHook))
        }
        $object = Join-Path $objects ([IO.Path]::GetFileNameWithoutExtension($name) + '.o')
        & $zig c++ @common -w -c $source -o $object
        if ($LASTEXITCODE -ne 0) { throw "Dear ImGui did not compile: $name." }
        $object
    }

    $editorSources = @(
        'editor.cpp', 'app.cpp', 'app_values.cpp', 'app_staging.cpp', 'app_widgets.cpp', 'app_page.cpp',
        'app_presets.cpp', 'app_mods_logs.cpp', 'app_sync.cpp', 'app_lobbies.cpp', 'app_keys.cpp',
        'app_browse.cpp', 'app_views.cpp', 'canvas.cpp', 'catalog.cpp', 'icons.cpp', 'json.cpp',
        'local_stats.cpp', 'sounds.cpp', 'zh_cn.cpp'
    ) | ForEach-Object { Join-Path $src $_ }
    $dll = Join-Path $out 'hellforge_editor.dll'
    $libraries = @('-ld3d11', '-ld3d12', '-ldxgi', '-ld3dcompiler_47', '-ldwmapi', '-lgdi32', '-lole32', '-luser32')
    & $zig c++ @common -shared -Wall -Werror -Wno-unused-parameter -s @editorSources @imguiObjects @libraries -o $dll
    if ($LASTEXITCODE -ne 0) { throw 'The editor did not compile.' }
    $importLibrary = Join-Path $out 'hellforge_editor.lib'
    if (Test-Path -LiteralPath $importLibrary -PathType Leaf) { Remove-Item -LiteralPath $importLibrary }
    Write-Output ('Built {0} ({1} bytes)' -f $dll, (Get-Item -LiteralPath $dll).Length)

    if ($Test) {
        $testOut = Join-Path $out 'test'
        $null = New-Item -ItemType Directory -Path $testOut -Force
        $localStatsTest = Join-Path $testOut 'local_stats_test.exe'
        $localStatsSources = @('test/local_stats_test.cpp', 'local_stats.cpp', 'json.cpp') |
            ForEach-Object { Join-Path $src $_ }
        & $zig c++ -target x86_64-windows-gnu -O1 -std=c++17 -Wall -Werror @localStatsSources -o $localStatsTest
        if ($LASTEXITCODE -ne 0) { throw 'The Local Stats test did not compile.' }
        & $localStatsTest
        if ($LASTEXITCODE -ne 0) { throw 'The Local Stats test failed.' }

        $translationTest = Join-Path $testOut 'zh_cn_test.exe'
        $imguiCore = @($imguiObjects | Where-Object { [IO.Path]::GetFileName($_) -notlike 'imgui_impl_*' })
        $translationSources = @('test/zh_cn_test.cpp', 'zh_cn.cpp', 'canvas.cpp', 'json.cpp') | ForEach-Object { Join-Path $src $_ }
        & $zig c++ @common -municode -UNDEBUG -Wall -Werror @translationSources @imguiCore -o $translationTest
        if ($LASTEXITCODE -ne 0) { throw 'The localization test did not compile or link.' }
        $fontTestArgs = @((Join-Path $env:WINDIR 'Fonts/msyh.ttc'))
        if ($EditorDataDir) { $fontTestArgs += $EditorDataDir }
        & $translationTest @fontTestArgs
        if ($LASTEXITCODE -ne 0) { throw 'The localization test failed.' }

        if ($EditorDataDir) {
            $minHookInclude = Join-Path $buildTools.MinHook 'include'
            $hookObjects = foreach ($name in @('src/buffer.c', 'src/hook.c', 'src/trampoline.c', 'src/hde/hde64.c')) {
                $object = Join-Path $testOut ('mh_' + [IO.Path]::GetFileNameWithoutExtension($name) + '.o')
                & $zig cc -target x86_64-windows-gnu -O1 -w -I $minHookInclude -c (Join-Path $buildTools.MinHook $name) -o $object
                if ($LASTEXITCODE -ne 0) { throw "MinHook did not compile: $name." }
                $object
            }
            $overlayTest = Join-Path $testOut 'overlay_test.exe'
            $overlayArgs = @(
                '-target', 'x86_64-windows-gnu', '-municode', '-O1', '-std=c++17', '-Wall', '-Werror', '-Wno-unused-parameter',
                '-I', $sdk, '-I', $minHookInclude, (Join-Path $src 'test/overlay_test.cpp'), (Join-Path $src 'json.cpp')
            )
            & $zig c++ @overlayArgs @hookObjects -ld3d11 -ld3d12 -ldxgi -lgdi32 -o $overlayTest
            if ($LASTEXITCODE -ne 0) { throw 'The overlay test did not compile.' }

            $catalog = Get-Content -LiteralPath (Join-Path $EditorDataDir 'editor-catalog.json') -Raw -Encoding UTF8 | ConvertFrom-Json
            if ([string]$catalog.build -ne '25480438') {
                throw "The overlay fixture requires official editor catalogue build 25480438 (found $($catalog.build))."
            }
            Write-Output ('Official editor catalogue build: ' + $catalog.build)
            $priorD3d11 = $env:HELLFORGE_EDITOR_TEST_D3D11
            $priorHdr = $env:HELLFORGE_EDITOR_HDR
            try {
                $scenarios = @(
                    @{ Shot = 'screen.bmp'; D3d11 = $null; Hdr = 'off'; RenderLog = 'SDR (back buffer format 24)' },
                    @{ Shot = 'screen11.bmp'; D3d11 = '1'; Hdr = 'off'; RenderLog = 'SDR (back buffer format 24)' },
                    @{ Shot = 'screen-hdr.bmp'; D3d11 = $null; Hdr = 'pq:200'; RenderLog = 'HDR, HDR10 (PQ) (back buffer format 24): SDR white 200 nits' },
                    @{ Shot = 'screen11-scrgb.bmp'; D3d11 = '1'; Hdr = 'scrgb:200'; RenderLog = 'HDR, scRGB (back buffer format 10): SDR white 200 nits' }
                )
                foreach ($scenario in $scenarios) {
                    $env:HELLFORGE_EDITOR_TEST_D3D11 = $scenario.D3d11
                    $env:HELLFORGE_EDITOR_HDR = $scenario.Hdr
                    $report = @()
                    & $overlayTest $dll (Join-Path $testOut $scenario.Shot) $EditorDataDir --full | Tee-Object -Variable report
                    if ($LASTEXITCODE -ne 0) { throw ('Overlay test failed: ' + $scenario.Shot) }
                    $modes = @($report | Where-Object { $_ -match '^  mod: editor: (SDR \(|HDR, )' })
                    $prefix = '  mod: editor: ' + $scenario.RenderLog
                    if ($modes.Count -eq 0 -or @($modes | Where-Object { -not $_.StartsWith($prefix, [StringComparison]::Ordinal) }).Count -ne 0) {
                        throw ('Wrong render mode for {0}; expected {1}; saw {2}' -f $scenario.Shot, $scenario.RenderLog, ($modes -join ', '))
                    }
                    Write-Output ('PASS render mode ({0}): {1}' -f $scenario.Shot, $scenario.RenderLog)
                }
            } finally {
                $env:HELLFORGE_EDITOR_TEST_D3D11 = $priorD3d11
                $env:HELLFORGE_EDITOR_HDR = $priorHdr
            }
        }
    }
} finally {
    $env:ZIG_LOCAL_CACHE_DIR = $priorLocal
    $env:ZIG_GLOBAL_CACHE_DIR = $priorGlobal

    $stagedDirectory = Get-Item -LiteralPath $staging -Force
    $resolvedStaging = (Resolve-Path -LiteralPath $staging).ProviderPath
    if ($resolvedStaging -ine $staging -or $stagedDirectory.Parent.FullName -ine $objects -or
        ($stagedDirectory.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Unexpected staging directory: $resolvedStaging"
    }
    Remove-Item -LiteralPath $resolvedStaging -Recurse -Force
}
