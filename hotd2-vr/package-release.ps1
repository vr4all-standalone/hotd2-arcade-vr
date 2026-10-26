<#
  Packages HOTD2 Arcade VR builds for the Quest and PC (implementation remains in progress)
  in dist\ (packaging checks do not establish gameplay or headset validation):

    hotd2-arcade-vr-quest.apk   the Quest app (Quest 3, 3S, 2, Pro)
    hotd2-arcade-vr-pcvr.zip    the PC version with VR on (flycast.exe, emu.cfg, README.txt,
                                        LICENSE.txt, Play HOTD2 Arcade.cmd; with the bundled
                                        models also CREDITS.txt and data\models\)

  Neither holds anything of the game: the players bring their own, and what the app makes from it
  is made on the spot. Both are checked for that (the names in their file lists: discs, keys,
  rips, frame grabs, models made from a game), and the APK for its signer: signed with the public
  debug key (no release key, see make-release-key.ps1) it is refused, since anyone could sign an
  "update" of the app with that key. The APK is also checked for being HOTD2 Arcade VR
  (its package, version and label, and its start screen the only entry in the headset's library),
  and the zip for holding exactly its files, with a README.txt that names no other game.

  The models that come with the app, hotd2-vr\models (guns\, controllers\ and CREDITS.txt: other
  people's work under their own licences, filled by hand and never in git): when the folder is
  there the APK has them as assets/models (the vr build takes the folder) and the zip as
  data\models\ with CREDITS.txt at its top, the same files in both. Every GLB in either must be
  named on a "File:" line of its CREDITS.txt (File: guns/zapper.glb), and git must track nothing
  in hotd2-vr\models; -CheckOnly checks those two as well.

  The licence, LICENSE.txt: GPL-3.0 and cgltf's MIT notice (and, with the bundled models, a line
  that they aren't under it). The vr build writes it into the APK as assets/LICENSE.txt
  (build.gradle.kts, LicenceText: the APK is downloaded on its own too), and the zip gets that same
  file, copied out of the APK. Both are checked for it, with the GPL-3.0 text and cgltf's notice
  (core\deps\cgltf\LICENSE) in it; -CheckOnly checks that too.

  A release is built from a commit, the one its published source is: the working tree must be
  clean (the gamesdk submodule's local change aside), and the APK's versionName (1.0.4-g<commit>),
  its libflycast.so and flycast.exe must all name HEAD (a -SkipBuild after a new commit fails
  that). -AllowDebugKey test builds skip both checks.

  .\package-release.ps1                  build both, then package
  .\package-release.ps1 -SkipBuild       package what was built last
  .\package-release.ps1 -AllowDebugKey   take a debug-signed APK too (a test build, not for release)
  .\package-release.ps1 -CheckOnly a.zip,b.apk   only check these archives' contents

  Uses the Android SDK's build-tools (ANDROID_HOME, default %LOCALAPPDATA%\Android\Sdk) and a JDK
  (JAVA_HOME, or a Microsoft JDK 17 in Program Files), as build-quest.cmd does.
#>
param([switch]$SkipBuild, [switch]$AllowDebugKey, [string[]]$CheckOnly)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$dist = Join-Path $repo 'dist'
$cmd = Join-Path $env:SystemRoot 'System32\cmd.exe'
Add-Type -AssemblyName System.IO.Compression.FileSystem

# The app: its application ID, label and version (shell\android-studio\flycast\build.gradle.kts and
# src\vr\AndroidManifest.xml), its library entry, and the PC zip's start script (hotd2-vr\assets).
$appId = 'com.vrlightgunhub.hotd2arcade'
$appLabel = 'HOTD2 Arcade VR'
$appVersion = '1.0.4'
$appLauncher = 'com.flycast.emulator.Hotd2ArcadeStartActivity'
$playCmd = 'Play HOTD2 Arcade.cmd'

# What must never be in a release: game discs and keys, rips (rip\), frame grabs, models made
# from a game (the app makes those on the player's device, from the player's game), and archives
# or disc images inside it (a ROM set, a BIOS zip). Signing material stays private too.
$bannedNames = '*.chd', '*.gdi', '*.cue', '*.cdi', '*.pic', 'shot_*.png', 'hands*.bin', 'staff*.bin', 'HND1*.bin',
	'gun-*.bin', '*.zip', '*.7z', '*.rar', '*.iso', '*.jks', '*.keystore', 'release-signing.properties', '*.ic*', 'dcox_us.*', '*.ram', '*.nvmem', '*.eeprom', '*.state'
$bannedDirs = 'rip', '.vrlightgunhub'

# The entries of a zip or APK whose names are on the lists above.
function Find-GameData([string]$archive) {
	$zip = [IO.Compression.ZipFile]::OpenRead((Resolve-Path -LiteralPath $archive).Path)
	try {
		foreach ($e in $zip.Entries) {
			$parts = ($e.FullName -replace '\\', '/').Split('/')
			$name = $parts[-1]
			$dirs = if ($parts.Count -gt 1) { $parts[0..($parts.Count - 2)] } else { @() }
			if (($dirs | Where-Object { $bannedDirs -contains $_ }) -or
				($bannedNames | Where-Object { $name -like $_ })) { $e.FullName }
		}
	} finally { $zip.Dispose() }
}

# The bundled models' GLBs in a zip or APK (under models/: the APK's assets/models/, the zip's data/models/) that its
# CREDITS.txt (assets/models/CREDITS.txt, the zip's CREDITS.txt) doesn't name on a "File: <path from models/>" line.
function Find-UncreditedModels([string]$archive) {
	$zip = [IO.Compression.ZipFile]::OpenRead((Resolve-Path -LiteralPath $archive).Path)
	try {
		$entries = @($zip.Entries | Where-Object { $_.Name })
		$glbs = @($entries | Where-Object { ($_.FullName -replace '\\', '/') -match '(^|/)models/.+\.glb$' })
		if (-not $glbs.Count) { return }
		$credits = $entries | Where-Object { @('assets/models/CREDITS.txt', 'CREDITS.txt') -contains ($_.FullName -replace '\\', '/') } |
			Select-Object -First 1
		$text = ''
		if ($credits) {
			$reader = New-Object IO.StreamReader($credits.Open())
			try { $text = $reader.ReadToEnd() } finally { $reader.Dispose() }
		}
		foreach ($g in $glbs) {
			$rel = ($g.FullName -replace '\\', '/') -replace '^(.*?/)?models/', ''
			if (-not $credits) { "$($g.FullName) (no CREDITS.txt)" }
			elseif ($text -notmatch "(?m)^\s*File:\s*$([regex]::Escape($rel))\s*$") { $g.FullName }
		}
	} finally { $zip.Dispose() }
}

# What's wrong with the licence text in a zip or APK (nothing when it's fine): the APK's assets/LICENSE.txt, the zip's
# LICENSE.txt, with the GPL-3.0 text and cgltf's MIT notice (core\deps\cgltf\LICENSE: cgltf is in the build, it reads
# the GLB models) in it, whitespace aside.
function Find-LicenceFault([string]$archive) {
	$name = if ([IO.Path]::GetExtension($archive) -eq '.apk') { 'assets/LICENSE.txt' } else { 'LICENSE.txt' }
	$text = $null
	$zip = [IO.Compression.ZipFile]::OpenRead((Resolve-Path -LiteralPath $archive).Path)
	try {
		$entry = $zip.GetEntry($name)
		if ($entry) {
			$reader = New-Object IO.StreamReader($entry.Open())
			try { $text = $reader.ReadToEnd() } finally { $reader.Dispose() }
		}
	} finally { $zip.Dispose() }
	if ($null -eq $text) { return "no $name" }
	$flat = $text -replace '\s+', ' '
	$cgltf = ((Get-Content -Raw (Join-Path $repo 'core\deps\cgltf\LICENSE')) -replace '\s+', ' ').Trim()
	if ($flat -notmatch 'GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007') { return "$name has no GPL-3.0 text" }
	if (-not $cgltf -or -not $flat.Contains($cgltf)) { return "$name has no cgltf notice (core\deps\cgltf\LICENSE)" }
}

function Test-Archives([string[]]$archives) {
	$bad = $false
	foreach ($a in $archives) {
		$hits = @(Find-GameData $a)
		if ($hits.Count) {
			$bad = $true
			Write-Host "$a holds possible game data or private signing material, not released:" -ForegroundColor Red
			$hits | Select-Object -First 20 | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
		} else {
			Write-Host "content check: $(Split-Path -Leaf $a) has no game-data or signing-material names"
		}
		$uncredited = @(Find-UncreditedModels $a)
		if ($uncredited.Count) {
			$bad = $true
			Write-Host "$a has models its CREDITS.txt doesn't name (a File: line each):" -ForegroundColor Red
			$uncredited | Select-Object -First 20 | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
		} else {
			Write-Host "credits check: $(Split-Path -Leaf $a) names every model it has in its CREDITS.txt"
		}
		$licence = Find-LicenceFault $a
		if ($licence) {
			$bad = $true
			Write-Host "$a lacks the licence text: $licence" -ForegroundColor Red
		} else {
			Write-Host "licence check: $(Split-Path -Leaf $a) has the licence text, GPL-3.0 and cgltf's MIT notice"
		}
	}
	-not $bad
}

# hotd2-vr\models is never in git (other people's models, under their own licences; .gitignore)
function Test-ModelsUntracked {
	$ErrorActionPreference = 'Continue'
	$tracked = @(& git -C $repo ls-files -- hotd2-vr/models 2>$null)
	if ($LASTEXITCODE -ne 0) { Write-Host 'git ls-files failed: hotd2-vr\models not checked' -ForegroundColor Red; return $false }
	if ($tracked.Count) {
		Write-Host 'git tracks files in hotd2-vr\models, never released that way (git rm --cached them):' -ForegroundColor Red
		$tracked | Select-Object -First 20 | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
		return $false
	}
	Write-Host 'git check: nothing in hotd2-vr\models is tracked'
	$true
}

if ($CheckOnly) {
	# powershell -File passes "a.zip,b.apk" as one string
	$untracked = Test-ModelsUntracked
	if ((Test-Archives ($CheckOnly | ForEach-Object { $_ -split ',' })) -and $untracked) { exit 0 } else { exit 1 }
}

function Invoke-Git { $ErrorActionPreference = 'Continue'; & git -C $repo @args 2>$null }

# One entry of a zip or APK, as bytes ($null when it has none by that name).
function Read-ZipEntry([string]$archive, [string]$name) {
	$z = [IO.Compression.ZipFile]::OpenRead($archive)
	try {
		$e = $z.GetEntry($name)
		if (-not $e) { return $null }
		$ms = New-Object IO.MemoryStream
		$s = $e.Open()
		try { $s.CopyTo($ms) } finally { $s.Dispose() }
		, $ms.ToArray()
	} finally { $z.Dispose() }
}

# The policy-disabled executable is for local research only. Its optimized binary omits
# the rejection diagnostic; require the compiled guard in both platform artifacts.
function Test-StandalonePolicy([byte[]]$bytes) {
    $bytes -and [Text.Encoding]::ASCII.GetString($bytes).Contains('HOTD2 Arcade VR supports the measured USA hotd2.zip cartridge and HOTD2-specific hod2bios.zip BIOS. Dreamcast media and other arcade revisions are unsupported.')
}
# A built binary names this commit: the version core\version.h gave it (v<tag>-<n>-g<commit>).
function Test-NamesCommit([string]$what, [byte[]]$bytes, [string]$hash) {
	if ($bytes -and [Text.Encoding]::ASCII.GetString($bytes) -match ([regex]::Escape("-g$hash") + '(?![0-9a-fA-F])')) {
		Write-Host "version check: $what names $hash"
		return $true
	}
	Write-Host ("$what does not name HEAD ($hash): build again without -SkipBuild (if libflycast.so still " +
		"doesn't, delete shell\android-studio\flycast\.cxx so its CMake configure runs again).") -ForegroundColor Red
	$false
}

# The activities and aliases whose intent filters have the LAUNCHER category (an entry in the
# headset's library), from aapt2's dump of the APK's AndroidManifest.xml.
function Get-LauncherOwners([string[]]$tree) {
	$owner = $null; $ownerIndent = -1; $named = $false
	foreach ($line in $tree) {
		if ($line -match '^(\s*)E: (\S+)') {
			$indent = $Matches[1].Length
			if ($owner -ne $null -and $indent -le $ownerIndent) { $owner = $null }
			if ($Matches[2] -eq 'activity' -or $Matches[2] -eq 'activity-alias') {
				$owner = '(no name)'; $ownerIndent = $indent; $named = $false
			}
		} elseif ($owner -ne $null -and -not $named -and $line -match '^(\s*)A: \S*:name\([^)]*\)="([^"]+)"' -and
			$Matches[1].Length -eq $ownerIndent + 2) {
			$owner = $Matches[2]; $named = $true
		} elseif ($line -match '="android\.intent\.category\.LAUNCHER"') {
			if ($owner -ne $null) { $owner } else { '(outside an activity)' }
		}
	}
}

# Pure metadata checks, also exercised by package-release-tests.ps1 with synthetic aapt output.
function Find-AppIdentityFault([string[]]$badgingLines, [string[]]$manifestTree, [string]$shortHash, [bool]$testBuild) {
    $badging = $badgingLines | Where-Object { $_ -match '^package:' } | Select-Object -First 1
    $wantVersion = [regex]::Escape("$appVersion-g") + $(if ($testBuild) { '[0-9a-f]+' } else { [regex]::Escape($shortHash) })
    if ($badging -notmatch "^package: name='$([regex]::Escape($appId))'") { "the package is not $appId" }
    if ($badging -notmatch "versionCode='5'") { 'the versionCode is not 5' }
    if ($badging -notmatch "versionName='$wantVersion'") {
        "the versionName is not $appVersion-g$(if ($testBuild) { '<commit>' } else { $shortHash })"
    }
    if ($badgingLines -notcontains "application-label:'$appLabel'") { "the label is not '$appLabel'" }
    $launchers = @(Get-LauncherOwners $manifestTree)
    if ($launchers.Count -ne 1 -or $launchers[0] -ne $appLauncher) {
        "the library entries (LAUNCHER) are '$($launchers -join "', '")', not $appLauncher alone"
    }
    if (@($manifestTree | Where-Object { $_ -match 'VrSetupActivity|CmStartActivity|LupinStartActivity' }).Count) { 'the manifest still has an older setup activity' }
    if (-not @($manifestTree | Where-Object { $_ -match '"com\.oculus\.permission\.RENDER_MODEL"' }).Count) {
        'the manifest has no com.oculus.permission.RENDER_MODEL (the controllers drawn in the hands)'
    }
}

function Find-PcArchiveFault([string]$archive, [string[]]$expectedFiles) {
    $z = [IO.Compression.ZipFile]::OpenRead($archive)
    try { $inZip = @($z.Entries | Where-Object { $_.Name } | ForEach-Object { $_.FullName }) } finally { $z.Dispose() }
    $extra = @($inZip | Where-Object { $expectedFiles -notcontains $_ })
    $lacking = @($expectedFiles | Where-Object { $inZip -notcontains $_ })
    $duplicates = @($inZip | Group-Object | Where-Object { $_.Count -gt 1 })
    if ($extra.Count -or $lacking.Count -or $duplicates.Count) {
        "$(Split-Path -Leaf $archive) must hold exactly $($expectedFiles -join ', '); also has: $($extra -join ', '); lacks: $($lacking -join ', '); duplicates: $($duplicates.Name -join ', ')"
    }
    $readmeBytes = Read-ZipEntry $archive 'README.txt'
    $readme = if ($readmeBytes) { [Text.Encoding]::ASCII.GetString($readmeBytes) } else { '' }
    foreach ($required in @($appLabel, 'roms\hotd2.zip', 'roms\hod2bios.zip', $playCmd)) {
        if (-not $readme.Contains($required)) { "README.txt does not name $required" }
    }
    foreach ($other in @('Light Gun Hub', 'Death Crimson OX', 'Maze of the Kings')) {
        if ($readme -match [regex]::Escape($other)) { "README.txt names $other; its play instructions must be for $appLabel" }
    }
    $configBytes = Read-ZipEntry $archive 'emu.cfg'
    $config = if ($configBytes) { [Text.Encoding]::ASCII.GetString($configBytes) } else { '' }
    foreach ($setting in @('pvr\.rend\s*=\s*0', 'rend\.NativeDepthInterpolation\s*=\s*yes', 'vr\.Xr\s*=\s*yes')) {
        if ($config -notmatch "(?m)^\s*$setting\s*$") { "emu.cfg lacks portable VR default $setting" }
    }
    $launcherBytes = Read-ZipEntry $archive $playCmd
    $launcher = if ($launcherBytes) { [Text.Encoding]::ASCII.GetString($launcherBytes) } else { '' }
    if ($launcher -notmatch 'start\s+""\s+/D\s+"%~dp0"' -or -not $launcher.Contains('"%~dp0flycast.exe"') -or
        -not $launcher.Contains('"%~dp0roms\hotd2.zip"')) { "$playCmd does not launch the original set with a portable working directory" }
}
# the working tree as committed (the gamesdk submodule's local change aside)
$head = Invoke-Git rev-parse HEAD
if (-not $AllowDebugKey) {
	$dirty = @(Invoke-Git status --porcelain | Where-Object { $_ -and $_ -notmatch '^.. core/deps/gamesdk$' })
	if ($dirty.Count) {
		Write-Host 'The working tree has changes that are not committed; a release is built from a commit:' -ForegroundColor Red
		$dirty | Select-Object -First 20 | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
		exit 1
	}
}
if (-not (Test-ModelsUntracked)) { exit 1 }

# the SDK's newest build-tools and a JDK, as build-quest.cmd finds them
$sdk = if ($env:ANDROID_HOME) { $env:ANDROID_HOME } else { Join-Path $env:LOCALAPPDATA 'Android\Sdk' }
$tools = Get-ChildItem (Join-Path $sdk 'build-tools') -Directory -ErrorAction SilentlyContinue |
	Where-Object { Test-Path (Join-Path $_.FullName 'apksigner.bat') } |
	Sort-Object { [version]($_.Name -replace '[^0-9.].*$', '') } -Descending | Select-Object -First 1
if (-not $tools) { Write-Host "No build-tools with apksigner in $sdk (set ANDROID_HOME)." -ForegroundColor Red; exit 1 }
$jdk = @(Get-ChildItem (Join-Path $env:ProgramFiles 'Microsoft') -Directory -Filter 'jdk-17*' -ErrorAction SilentlyContinue |
	ForEach-Object { $_.FullName }) + @($env:JAVA_HOME) |
	Where-Object { $_ -and (Test-Path (Join-Path $_ 'bin\keytool.exe')) } | Select-Object -First 1
if (-not $jdk) { Write-Host 'No JDK found (set JAVA_HOME).' -ForegroundColor Red; exit 1 }
$env:JAVA_HOME = $jdk   # this process only: apksigner.bat runs on it

if (-not $SkipBuild) {
	# The PC first: build-win.cmd runs CMake's configure every time, which writes core\version.h
	# (the version both builds embed) for HEAD. The Quest build's configure runs only when its
	# CMake files change, so it alone would keep an old commit's version.h; its ninja rebuilds
	# what includes the new one.
	& $cmd /c (Join-Path $PSScriptRoot 'build-win.cmd') Release
	if ($LASTEXITCODE -ne 0) { throw 'build-win.cmd failed' }
	$short = Invoke-Git rev-parse --short HEAD
	if ((Get-Content -Raw (Join-Path $repo 'core\version.h')) -notmatch "GIT_HASH\s+""$short""") { throw "core\version.h does not name HEAD ($short)" }
	& $cmd /c (Join-Path $PSScriptRoot 'build-quest.cmd')
	if ($LASTEXITCODE -ne 0) { throw 'build-quest.cmd failed' }
}
New-Item -ItemType Directory -Force $dist | Out-Null
$apkOut = Join-Path $dist 'hotd2-arcade-vr-quest.apk'
$zip = Join-Path $dist 'hotd2-arcade-vr-pcvr.zip'
$stage = Join-Path $dist 'hotd2-arcade-vr-pcvr'
foreach ($f in $apkOut, $zip) { if (Test-Path $f) { (Get-Item $f).Delete() } }   # never leave an old one

# the Quest app: who signed it
$apk = Join-Path $repo 'shell\android-studio\flycast\build\outputs\apk\vr\flycast-vr.apk'
$ErrorActionPreference = 'Continue'   # the tools may write warnings on stderr
$certs = & (Join-Path $tools.FullName 'apksigner.bat') verify --print-certs $apk 2>&1 | ForEach-Object { "$_" }
$signed = $LASTEXITCODE -eq 0
$badgingAll = @(& (Join-Path $tools.FullName 'aapt2.exe') dump badging $apk 2>&1 | ForEach-Object { "$_" })
$badging = $badgingAll | Where-Object { $_ -match '^package:' } | Select-Object -First 1   # not a warning line
$manifestTree = @(& (Join-Path $tools.FullName 'aapt2.exe') dump xmltree --file AndroidManifest.xml $apk 2>&1 |
	ForEach-Object { "$_" })
$tmp = Join-Path $env:TEMP "vrlgh-debug-$PID.cer"
& (Join-Path $jdk 'bin\keytool.exe') -exportcert -keystore (Join-Path $repo 'shell\android-studio\debug.keystore') `
	-storepass android -alias androiddebugkey -file $tmp 2>&1 | Out-Null
$ErrorActionPreference = 'Stop'
if (-not $signed -or -not (Test-Path $tmp)) {
	Write-Host "Could not check the APK's signer:" -ForegroundColor Red; $certs | Write-Host; exit 1
}
$sha = [Security.Cryptography.SHA256]::Create()
$debugDigest = (($sha.ComputeHash([IO.File]::ReadAllBytes($tmp)) | ForEach-Object { $_.ToString('x2') }) -join '')
Remove-Item $tmp
$digests = @($certs | Select-String 'Signer #\d+ certificate SHA-256 digest: ([0-9a-f]+)' |
	ForEach-Object { $_.Matches[0].Groups[1].Value })
if (-not $digests.Count) { Write-Host 'No signer certificate in the APK:' -ForegroundColor Red; $certs | Write-Host; exit 1 }
Write-Host $badging
$certs | Select-String 'certificate DN' | ForEach-Object { Write-Host "$_" }
if ($digests -contains $debugDigest) {
	if (-not $AllowDebugKey) {
		Write-Host ("The APK is signed with the public debug key (shell\android-studio\debug.keystore): not released. " +
			'Make the release key with hotd2-vr\make-release-key.ps1 and build again (or -AllowDebugKey for a test build).') -ForegroundColor Red
		exit 1
	}
	Write-Host 'Signed with the public debug key: a test build, do not publish it.' -ForegroundColor Yellow
}
# The app's identity and sole library entry, checked before copying any release output.
$short = Invoke-Git rev-parse --short HEAD
$wrong = @(Find-AppIdentityFault $badgingAll $manifestTree $short ([bool]$AllowDebugKey))
if ($wrong.Count) {
	Write-Host "The APK is not $appLabel as released:" -ForegroundColor Red
	$wrong | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
	exit 1
}
Write-Host "app check: $appId, '$appLabel', its start screen the only library entry"
# built from HEAD: the APK's versionName, its libflycast.so and flycast.exe
if (-not $AllowDebugKey) {
	$built = if ($badging -match "versionName='[^']*-g([0-9a-f]+)'") { $Matches[1] } else { $null }
	$at = if ($built) { Invoke-Git rev-parse --verify --quiet "$built^{commit}" } else { $null }
	if (-not $built -or -not $head -or $at -ne $head) {
		Write-Host ("The APK was built from $(if ($built) { "commit $built" } else { 'no commit its versionName names' }), " +
			"not HEAD ($head): build it again (without -SkipBuild).") -ForegroundColor Red
		exit 1
	}
	if (-not (Test-NamesCommit 'libflycast.so in the APK' (Read-ZipEntry $apk 'lib/arm64-v8a/libflycast.so') $built) -or
		-not (Test-NamesCommit 'build-win\flycast.exe' ([IO.File]::ReadAllBytes((Join-Path $repo 'build-win\flycast.exe'))) $built)) {
		exit 1
	}
}
if (-not (Test-StandalonePolicy (Read-ZipEntry $apk 'lib/arm64-v8a/libflycast.so')) -or
    -not (Test-StandalonePolicy ([IO.File]::ReadAllBytes((Join-Path $repo 'build-win\flycast.exe'))))) {
    throw 'Research build with standalone game policy OFF cannot be packaged. Rebuild both platforms with VR_STANDALONE_HOTD2ARCADE=ON.'
}
Copy-Item $apk $apkOut

# the PC version: VR on, OpenGL (the only renderer VR draws with)
$stage = [IO.Path]::GetFullPath($stage)
$distPrefix = [IO.Path]::GetFullPath($dist) + [IO.Path]::DirectorySeparatorChar
if (-not $stage.StartsWith($distPrefix, [StringComparison]::OrdinalIgnoreCase)) { throw 'Release staging path escaped dist' }
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null
Copy-Item (Join-Path $repo 'build-win\flycast.exe') $stage
Copy-Item -LiteralPath (Join-Path $PSScriptRoot "assets\$playCmd") $stage
# the models that come with the app, when hotd2-vr\models is there: data\models\ (where flycast.exe looks last, after
# the player's folders) and their CREDITS.txt at the top; the APK must have the same ones (assets/models)
$models = Join-Path $PSScriptRoot 'models'
$modelFiles = @()
if (Test-Path -LiteralPath $models) {
	foreach ($sub in 'guns', 'controllers') {
		$from = Join-Path $models $sub
		if (-not (Test-Path -LiteralPath $from)) { continue }
		$to = Join-Path $stage "data\models\$sub"
		New-Item -ItemType Directory -Force $to | Out-Null
		foreach ($f in @(Get-ChildItem -LiteralPath $from -File | Where-Object { '.glb', '.json' -contains $_.Extension } | Sort-Object Name)) {
			Copy-Item -LiteralPath $f.FullName $to
			$modelFiles += "data/models/$sub/$($f.Name)"
		}
	}
	if (Test-Path -LiteralPath (Join-Path $models 'CREDITS.txt')) {
		Copy-Item -LiteralPath (Join-Path $models 'CREDITS.txt') $stage
		$modelFiles += 'CREDITS.txt'
	}
} else {
	Write-Host 'No hotd2-vr\models: this release comes without the bundled models (README, Releasing).' -ForegroundColor Yellow
}
$a = [IO.Compression.ZipFile]::OpenRead($apk)
try {
	$apkModels = @($a.Entries | Where-Object { $_.Name -and $_.FullName -like 'assets/models/*' } |
		ForEach-Object { $_.FullName.Substring('assets/models/'.Length) } | Sort-Object)
} finally { $a.Dispose() }
$wantModels = @($modelFiles | ForEach-Object { $_ -replace '^data/models/', '' } | Sort-Object)
$modelsGood = ($apkModels -join '|') -eq ($wantModels -join '|')
if ($modelsGood) { Write-Host "models check: the APK and the zip have the same $($wantModels.Count) bundled model files" }
else {
	Write-Host ("The APK's bundled models ($($apkModels -join ', ')) are not those of hotd2-vr\models ($($wantModels -join ', ')): " +
		'build again without -SkipBuild.') -ForegroundColor Red
}
Set-Content -Path (Join-Path $stage 'emu.cfg') -Encoding ascii -Value @'
[config]
pvr.rend = 0
rend.NativeDepthInterpolation = yes
vr.Xr = yes
'@
$readme = @'
HOTD2 Arcade VR, experimental PC VR build

The original Sega NAOMI arcade cartridge in stereo VR. Physical headset testing remains pending.

1. Put your complete owned USA MAME set in roms\hotd2.zip beside flycast.exe.
   It needs epr-21585.ic22 (2 MiB, CRC b23d1a0c) and all twenty 8 MiB MPR ROMs.
2. Put your HOTD2-specific BIOS in roms\hod2bios.zip, or embed a supported BIOS in hotd2.zip.
   Supported 2 MiB files: epr-21331.ic27, epr-21330.ic27, epr-21329.ic27.
   An ordinary NAOMI BIOS cannot replace this. No CHD is needed; Dreamcast media is unsupported.
3. Connect your headset to the active OpenXR runtime and run "Play HOTD2 Arcade.cmd".
   This launcher keeps this portable copy's configuration and cache in its own folder.

First launch runs the arcade's opening scene to prepare its pistol from your own game.
B or Y skips preparation. Arcade gun, Zapper and custom GLB choices are also available.
Default two-player assignment: P1 left, P2 right. X starts P1; A joins P2. The paused
Two players setting reverses the hands and their Start/reload controls. Solo play can use
either trigger. B/Y reload; the Reload setting selects shoot-off-screen or point-off-screen.
Left Menu pauses for settings; left stick click recenters; right stick click toggles the laser.
Gun, angle, reload, hands, laser, haptics, world size, panel distance, wider view and blood color are saved.
Blood color: choose Red or Green in the paused settings menu, then restart the game.
It uses the arcade game's original effects; (next start) marks a pending change.
Loading a save state can restore its old effects; start without a state to apply the color.

Only the USA hotd2 arcade revision is adapted. Later chapters and endings remain unverified.
See the corresponding source's validation notes and headset checklist for detailed scope.
No game ROM, BIOS, generated game model, cabinet save or signing material is supplied.

Credits: Flycast by flyinghead and contributors, GPL-2.0-or-later; mikermak's hotd2-vr;
the shared standalone VR adaptations and their retained source notices.
Namco Arcade Gun by Martoscar, CC BY 4.0:
https://sketchfab.com/3d-models/namco-arcade-gun-15fbd5b9add94a34b5c21746e3dd32be
Re-oriented, quantised, textures/materials removed, recoloured and converted to C++.
OpenXR loader by Khronos, Apache-2.0. Optional bundled model credits are in CREDITS.txt.

Licence: this build is distributed under GNU GPL version 3 (LICENSE.txt), including cgltf's
MIT notice. Distribute its corresponding source with any redistributed binary.
Unofficial fan project, not affiliated with Sega or Meta.
'@
if ($modelFiles.Count) {
	$readme = $readme -replace '(?m)^Licence: this build', ("The other guns and the controllers, in data\models, are 3D models made by others, under their`n" +
		"own licences: CREDITS.txt names each one, its author, its licence and what was changed.`n`nLicence: this build")
}
Set-Content -Path (Join-Path $stage 'README.txt') -Encoding ascii -Value ($readme -replace "`r?`n", "`r`n")
# the licence: the APK's assets/LICENSE.txt (GPL-3.0 and cgltf's MIT notice, written by the vr build: build.gradle.kts,
# LicenceText), the same file in the zip; Test-Archives checks both (without it both fail, and neither is left in dist\)
$licence = Read-ZipEntry $apk 'assets/LICENSE.txt'
if ($null -ne $licence) { [IO.File]::WriteAllBytes((Join-Path $stage 'LICENSE.txt'), $licence) }
else { Write-Host 'The APK has no assets/LICENSE.txt (the licence text): build it again (without -SkipBuild).' -ForegroundColor Red }
# the zip, its entry names with forward slashes (Windows PowerShell 5.1's Compress-Archive writes data\models\...)
Add-Type -AssemblyName System.IO.Compression
$out = [IO.File]::Open($zip, 'CreateNew')
$archive = New-Object IO.Compression.ZipArchive($out, [IO.Compression.ZipArchiveMode]::Create)
try {
	foreach ($f in @(Get-ChildItem -LiteralPath $stage -Recurse -File)) {
		$entry = $f.FullName.Substring($stage.Length).TrimStart('\') -replace '\\', '/'
		[void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $f.FullName, $entry, [IO.Compression.CompressionLevel]::Optimal)
	}
} finally { $archive.Dispose(); $out.Dispose() }

# The zip contains exactly its files, portable defaults and launcher, and HOTD2 Arcade's own instructions.
$zipFiles = @('flycast.exe', 'emu.cfg', 'README.txt', 'LICENSE.txt', $playCmd) + $modelFiles
$zipFaults = @(Find-PcArchiveFault $zip $zipFiles)
$zipGood = $zipFaults.Count -eq 0
if ($zipGood) {
    Write-Host "zip check: $(Split-Path -Leaf $zip) holds exactly $($zipFiles -join ', '), with HOTD2 Arcade instructions and portable defaults"
} else {
    $zipFaults | ForEach-Object { Write-Host "zip check: $_" -ForegroundColor Red }
}
if (-not (Test-Archives $apkOut, $zip) -or -not $zipGood -or -not $modelsGood) {
	foreach ($f in $apkOut, $zip) { if (Test-Path $f) { (Get-Item $f).Delete() } }
	exit 1
}
Get-ChildItem $dist -File | Select-Object Name, @{ n = 'MB'; e = { [math]::Round($_.Length / 1MB, 1) } } | Format-Table | Out-String | Write-Host
exit 0
