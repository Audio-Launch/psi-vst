# Bundles the Psi Spectral web UI into webui.zip for embedding in the plugin (self-contained,
# no dev server). Packs ONLY the plugin page + code assets (js/css/fonts), not the 54 MB of
# marketing HTML/images. Zip entries use forward slashes (via .NET ZipFile) so JUCE's
# juce::ZipFile lookups (_astro/foo.js, plugin/index.html) match on Windows.
#
# Usage:   powershell -File scripts\bundle-ui.ps1            # builds the web app, then zips
#          powershell -File scripts\bundle-ui.ps1 -NoBuild   # zip an already-built dist/
# After running, (re)build the plugin (release/embedded config) so the new zip is baked in.

$ErrorActionPreference = 'Stop'
$web = 'E:\New audiolab.dev'
$out = 'E:\psi-vst\webui.zip'
$dist = Join-Path $web 'dist'

if ($args -notcontains '-NoBuild') {
  Push-Location $web
  try { npm run build } finally { Pop-Location }
}

$page = Join-Path $dist 'plugin\index.html'
if (-not (Test-Path $page)) { throw 'dist/plugin/index.html not found - did the build succeed?' }

$stage = Join-Path $env:TEMP ('psi-webui-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path (Join-Path $stage 'plugin'), (Join-Path $stage '_astro') | Out-Null
Copy-Item $page (Join-Path $stage 'plugin\index.html')
Get-ChildItem (Join-Path $dist '_astro') -File |
  Where-Object { $_.Extension -in '.js', '.css', '.woff2', '.woff' } |
  ForEach-Object { Copy-Item $_.FullName (Join-Path $stage ('_astro\' + $_.Name)) }

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
if (Test-Path $out) { Remove-Item $out -Force }
# Build entry-by-entry with explicit forward-slash names. .NET Framework's
# CreateFromDirectory writes backslash entry names, which JUCE's ZipFile (and the
# WebView's forward-slash request paths) will not match.
$fs = [System.IO.File]::Open($out, [System.IO.FileMode]::CreateNew)
$zip = New-Object System.IO.Compression.ZipArchive($fs, [System.IO.Compression.ZipArchiveMode]::Create)
try {
  Get-ChildItem $stage -Recurse -File | ForEach-Object {
    $rel = $_.FullName.Substring($stage.Length + 1).Replace('\', '/')
    [void][System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $_.FullName, $rel)
  }
} finally { $zip.Dispose(); $fs.Dispose() }
Remove-Item $stage -Recurse -Force

$kb = [math]::Round((Get-Item $out).Length / 1KB, 0)
Write-Host ('webui.zip -> ' + $out + '  (' + $kb + ' KB)')