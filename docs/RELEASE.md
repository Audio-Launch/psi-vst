# Releasing Ψ Spectral

The canonical recipe for shipping a new build. The site's `/download` page and the in-plugin
update check both read from `audiolab.tools/vst/version.json`, so those two facts (the release
asset URL and the version.json bump) are what make a new build actually reach users.

## Recipe

1. **Bump the version.** In `CMakeLists.txt`, update `project(PsiSpectral VERSION X.Y.Z ...)`.
   This value is compiled into `PSI_VERSION` and is what the update check compares against.

2. **Rebuild.** Regenerate the bundled web UI, then build Release:
   ```powershell
   & "E:\psi-vst\scripts\bundle-ui.ps1"
   & "C:\Program Files\CMake\bin\cmake.exe" --build "E:\psi-vst\build" --config Release
   ```

3. **Zip the .vst3**:
   ```powershell
   $v = "X.Y.Z"
   Compress-Archive -Path "E:\psi-vst\build\PsiSpectral_artefacts\Release\VST3\Psi Spectral.vst3" `
     -DestinationPath "$env:USERPROFILE\Desktop\Psi-Spectral-$v-win-vst3.zip" `
     -CompressionLevel Optimal -Force
   ```

4. **Tag + publish the GitHub release** (repo is public: `Audio-Launch/psi-vst`):
   ```powershell
   cd E:\psi-vst
   git tag -a v$v -m "v$v"
   git push origin v$v
   gh release create v$v "$env:USERPROFILE\Desktop\Psi-Spectral-$v-win-vst3.zip" `
     --title "v$v" --notes "<what changed, one paragraph>"
   ```

5. **Wire the site.** In `E:\New audiolab.dev\public\vst\version.json`, update:
   - `version` to `"X.Y.Z"`
   - `url` to the release asset (pattern:
     `https://github.com/Audio-Launch/psi-vst/releases/download/vX.Y.Z/Psi-Spectral-X.Y.Z-win-vst3.zip`)
   - `size` (roughly, from `Get-Item ...zip | Select Length`)
   - `notes` (one line, shown on `/download`)

6. **Commit + push + redeploy** the site (Vercel: Status filter 7/7, newest CANCELED deploy,
   Redeploy). Two things now happen:
   - `/download` shows the active button pointing at the new zip.
   - Any older installed plugin that opens after the deploy sees the newer version, and shows
     the in-app "⟳ Update available → Download" chip. The device key stays (account.json in
     `%APPDATA%\AudioLab\PsiSpectral`, DPAPI, survives the binary swap).

## What NOT to change between releases

- **The zip layout.** Keep a single `Psi Spectral.vst3` at the top of the zip. The
  install steps on `/download` tell users to copy it into `C:\Program Files\Common Files\VST3\`.
- **The version.json shape.** Only bump the four fields above. The plugin parses those keys.
- **Repo visibility.** Public. Release assets on a private repo require auth to download and
  the plain download button would 404 without a token proxy.

## Signing (deferred)

Windows may warn on an unsigned .vst3. A code-signing certificate removes that warning; it is
not required for the download to work. Add signing when we have a certificate; until then the
zip ships unsigned.
