<# Synthetic release checks only. Never builds, launches Flycast, reads signing keys, or packages real game data. #>
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$packageScript = Join-Path $PSScriptRoot 'package-release.ps1'
$tokens = $null; $parseErrors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($packageScript, [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count) { throw ($parseErrors | Out-String) }
# Load the real validation functions and literal identity/banning configuration, without executing
# the build, SDK, signing, packaging or git-attestation orchestration.
foreach ($statement in $ast.EndBlock.Statements) {
    if ($statement -is [Management.Automation.Language.FunctionDefinitionAst]) {
        . ([scriptblock]::Create($statement.Extent.Text))
    } elseif ($statement -is [Management.Automation.Language.AssignmentStatementAst] -and
        $statement.Left.VariablePath.UserPath -in @('appId', 'appLabel', 'appVersion', 'appLauncher', 'playCmd', 'bannedNames', 'bannedDirs')) {
        . ([scriptblock]::Create($statement.Extent.Text))
    }
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.IO.Compression
$script:checks = 0; $script:failures = 0
function Check([string]$name, [bool]$condition) {
    $script:checks++
    if ($condition) { Write-Host "PASS $name" } else { $script:failures++; Write-Host "FAIL $name" -ForegroundColor Red }
}
$work = Join-Path $repo ('.cache\death-package-tests-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work -Force | Out-Null
function Archive([string]$name, [hashtable]$entries) {
    $path = Join-Path $work $name
    $file = [IO.File]::Open($path, 'CreateNew')
    $zip = New-Object IO.Compression.ZipArchive($file, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($entryName in $entries.Keys) {
            $entry = $zip.CreateEntry($entryName)
            $stream = $entry.Open()
            try {
                $bytes = [Text.Encoding]::UTF8.GetBytes([string]$entries[$entryName])
                $stream.Write($bytes, 0, $bytes.Length)
            } finally { $stream.Dispose() }
        }
    } finally { $zip.Dispose(); $file.Dispose() }
    $path
}
try {
    Check 'HOTD2 Arcade application ID' ($appId -eq 'com.vrlightgunhub.hotd2arcade')
    Check 'HOTD2 Arcade label' ($appLabel -eq 'HOTD2 Arcade - VR4ALL')
    Check 'HOTD2 Arcade release base version' ($appVersion -eq '1.0.5')
    Check 'HOTD2 Arcade setup launcher' ($appLauncher -eq 'com.flycast.emulator.Hotd2ArcadeStartActivity')
    Check 'HOTD2 Arcade PC launcher name' ($playCmd -eq 'Play HOTD2 Arcade.cmd')
    $banned = @('roms/LUPINSHOO.ZIP','roms/deathcoxj/mpr-23514.ic1','gds-0018.cue','gds-0018.gdi',
        'epr-23524a.ic22','data/gun-lp-pistol.bin','rip/frame.txt','shot_00001.png','hod2bios.zip')
    $entries = @{}; foreach ($name in $banned) { $entries[$name] = 'synthetic fixture' }
    $archive = Archive 'game-data.zip' $entries
    $found = @(Find-GameData $archive)
    Check 'all inherited game-data bans retained' ($found.Count -eq $banned.Count)
    $archive = Archive 'secrets.zip' @{'release.jks'='synthetic'; 'release-signing.properties'='synthetic'; 'keys/debug.keystore'='synthetic'; '.vrlightgunhub/note.txt'='synthetic'}
    Check 'release signing material is excluded' (@(Find-GameData $archive).Count -eq 4)
    $archive = Archive 'uncredited.zip' @{'data/models/guns/fake.glb'='synthetic'}
    Check 'uncredited model rejected' (@(Find-UncreditedModels $archive).Count -eq 1)
    $archive = Archive 'credited.zip' @{'data/models/guns/fake.glb'='synthetic'; 'CREDITS.txt'="File: guns/fake.glb`nSynthetic test model, no game data"}
    Check 'credited model accepted' (@(Find-UncreditedModels $archive).Count -eq 0)
    $gpl = Get-Content -LiteralPath (Join-Path $repo 'core/deps/picotcp/LICENSE.GPLv3') -Raw
    $mit = Get-Content -LiteralPath (Join-Path $repo 'core/deps/cgltf/LICENSE') -Raw
    $licence = "HOTD2 Arcade - VR4ALL`n$gpl`n$mit"
    $archive = Archive 'licensed.zip' @{'LICENSE.txt'=$licence}
    Check 'GPL-3 and cgltf notice accepted' (-not (Find-LicenceFault $archive))
    $archive = Archive 'missing-mit.apk' @{'assets/LICENSE.txt'=$gpl}
    Check 'missing cgltf notice rejected' ([bool](Find-LicenceFault $archive))
    $archive = Archive 'missing-gpl.apk' @{'assets/LICENSE.txt'=$mit}
    Check 'missing GPL-3 rejected' ([bool](Find-LicenceFault $archive))
    $archive = Archive 'missing-license.zip' @{'readme.txt'='synthetic'}
    Check 'missing licence rejected' ([bool](Find-LicenceFault $archive))
    Check 'matching binary commit accepted' (Test-NamesCommit 'synthetic binary' ([Text.Encoding]::ASCII.GetBytes('v0-1-gabc123')) 'abc123')
    Check 'different binary commit rejected' (-not (Test-NamesCommit 'synthetic binary' ([Text.Encoding]::ASCII.GetBytes('v0-1-gdef456')) 'abc123'))
    Check 'longer commit with matching prefix rejected' (-not (Test-NamesCommit 'synthetic binary' ([Text.Encoding]::ASCII.GetBytes('v0-1-gabc123f')) 'abc123'))

    $policyCheck = Get-Command Test-StandalonePolicy -ErrorAction SilentlyContinue
    Check 'standalone release policy validator exists' ([bool]$policyCheck)
    if ($policyCheck) {
        Check 'standalone build policy accepted' (Test-StandalonePolicy ([Text.Encoding]::ASCII.GetBytes('HOTD2 Arcade - VR4ALL supports the measured USA hotd2.zip cartridge and HOTD2-specific hod2bios.zip BIOS. Dreamcast media and other arcade revisions are unsupported.')))
        Check 'research build policy rejected' (-not (Test-StandalonePolicy ([Text.Encoding]::ASCII.GetBytes('generic Flycast research build'))))
    }
    $tree = @('E: manifest', '  E: uses-permission', '    A: android:name(0x01010003)="com.oculus.permission.RENDER_MODEL"',
        '  E: application', '    E: activity', '      A: android:name(0x01010003)="com.flycast.emulator.Hotd2ArcadeStartActivity"',
        '      E: intent-filter', '        E: category', '          A: android:name(0x01010003)="android.intent.category.LAUNCHER"')
    Check 'manifest launcher parser uses activity owner' (@(Get-LauncherOwners $tree) -join ',' -eq 'com.flycast.emulator.Hotd2ArcadeStartActivity')
    $badging = @("package: name='com.vrlightgunhub.hotd2arcade' versionCode='6' versionName='1.0.5-gabc123'", "application-label:'HOTD2 Arcade - VR4ALL'")
    $hasIdentityCheck = [bool](Get-Command Find-AppIdentityFault -ErrorAction SilentlyContinue)
    Check 'APK metadata validator is independently testable' $hasIdentityCheck
    if ($hasIdentityCheck) {
        Check 'valid HOTD2 Arcade APK metadata accepted' (@(Find-AppIdentityFault $badging $tree 'abc123' $false).Count -eq 0)
        foreach ($case in @(
            @('package', 'com.vrlightgunhub.hotd2arcade', 'com.vrlightgunhub.confmiss'),
            @('version code', "versionCode='6'", "versionCode='9'"),
            @('base version', '1.0.5-gabc123', '1.2.0-gabc123'),
            @('commit version', '1.0.5-gabc123', '1.0.5-gdef456'),
            @('label', 'HOTD2 Arcade - VR4ALL', 'Confidential Mission VR'))) {
            $changed = @($badging | ForEach-Object { $_.Replace($case[1],$case[2]) })
            Check "wrong APK $($case[0]) rejected" (@(Find-AppIdentityFault $changed $tree 'abc123' $false).Count -gt 0)
        }
        $changed = @($tree | ForEach-Object { $_.Replace('Hotd2ArcadeStartActivity','CmStartActivity') })
        Check 'old setup activity rejected' (@(Find-AppIdentityFault $badging $changed 'abc123' $false).Count -gt 0)
        $changed = @($tree | Where-Object { $_ -notmatch 'RENDER_MODEL' })
        Check 'missing controller-model permission rejected' (@(Find-AppIdentityFault $badging $changed 'abc123' $false).Count -gt 0)
        $changed = @($badging | ForEach-Object { $_.Replace('abc123','def456') })
        Check 'test build keeps identity while allowing another commit' (@(Find-AppIdentityFault $changed $tree 'abc123' $true).Count -eq 0)
    }
    $hasPcCheck = [bool](Get-Command Find-PcArchiveFault -ErrorAction SilentlyContinue)
    Check 'portable PC archive validator is independently testable' $hasPcCheck
    if ($hasPcCheck) {
        $files = @('flycast.exe','emu.cfg','README.txt','LICENSE.txt','Play HOTD2 Arcade.cmd')
        $readme = 'HOTD2 Arcade - VR4ALL. Implementation in progress. roms\hotd2.zip roms\hod2bios.zip Play HOTD2 Arcade.cmd'
        $entries = @{'flycast.exe'='synthetic'; 'emu.cfg'="[config]`npvr.rend = 0`nrend.NativeDepthInterpolation = yes`nvr.Xr = yes"; 'README.txt'=$readme; 'LICENSE.txt'=$licence; 'Play HOTD2 Arcade.cmd'='start "" /D "%~dp0" "%~dp0flycast.exe" "%~dp0roms\hotd2.zip"'}
        $archive = Archive 'portable-valid.zip' $entries
        Check 'exact portable HOTD2 Arcade archive accepted' (@(Find-PcArchiveFault $archive $files).Count -eq 0)
        $goodConfig = $entries['emu.cfg']
        $entries['emu.cfg'] = $goodConfig.Replace('pvr.rend', 'pvrXrend')
        $archive = Archive 'portable-typo-default.zip' $entries
        Check 'misspelled renderer default rejected' (@(Find-PcArchiveFault $archive $files).Count -gt 0)
        $entries['emu.cfg'] = $goodConfig.Replace('vr.Xr = yes', 'vr.Xr = no')
        $archive = Archive 'portable-vr-off.zip' $entries
        Check 'VR-off portable default rejected' (@(Find-PcArchiveFault $archive $files).Count -gt 0)
        $entries['emu.cfg'] = $goodConfig
        $goodLauncher = $entries['Play HOTD2 Arcade.cmd']
        $entries['Play HOTD2 Arcade.cmd'] = $goodLauncher.Replace('hotd2.zip','confmiss.zip')
        $archive = Archive 'portable-wrong-launch.zip' $entries
        Check 'foreign-game launcher rejected' (@(Find-PcArchiveFault $archive $files).Count -gt 0)
        $entries['Play HOTD2 Arcade.cmd'] = $goodLauncher
        # Exercise the actual literal README/config generators and actual launcher, without
        # running the package orchestration or consuming any build artefact.
        $readmeStatement = $ast.EndBlock.Statements | Where-Object {
            $_ -is [Management.Automation.Language.AssignmentStatementAst] -and $_.Left.VariablePath.UserPath -eq 'readme'
        } | Select-Object -First 1
        $generatedReadme = $readmeStatement.Right.Find({param($n) $n -is [Management.Automation.Language.StringConstantExpressionAst]}, $true).Value
        $generatedConfig = $ast.Find({param($n) $n -is [Management.Automation.Language.StringConstantExpressionAst] -and $n.Value.StartsWith('[config]')}, $true).Value
        $production = $entries.Clone()
        $production['README.txt'] = $generatedReadme
        $production['emu.cfg'] = $generatedConfig
        $production['Play HOTD2 Arcade.cmd'] = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'assets/Play HOTD2 Arcade.cmd') -Raw
        $archive = Archive 'portable-production-templates.zip' $production
        Check 'production README config and launcher satisfy portable checks' (@(Find-PcArchiveFault $archive $files).Count -eq 0)
        Check 'packaged README records hardware verification limit' ($generatedReadme.Contains('Physical headset testing remains pending.'))
        $entries['README.txt'] = $readme.Replace('HOTD2 Arcade - VR4ALL','Confidential Mission VR')
        $archive = Archive 'portable-wrong-readme.zip' $entries
        Check 'stale CM README rejected' (@(Find-PcArchiveFault $archive $files).Count -gt 0)
        $entries['README.txt'] = $readme
        $entries['unexpected.txt'] = 'synthetic'
        $archive = Archive 'portable-extra.zip' $entries
        Check 'unexpected portable archive file rejected' (@(Find-PcArchiveFault $archive $files).Count -gt 0)
        $entries.Remove('unexpected.txt'); $entries.Remove('emu.cfg')
        $archive = Archive 'portable-missing.zip' $entries
        Check 'missing portable archive file rejected' (@(Find-PcArchiveFault $archive $files).Count -gt 0)
    }
    $launcher = Join-Path $PSScriptRoot 'assets/Play HOTD2 Arcade.cmd'
    Check 'HOTD2 Arcade launcher exists' (Test-Path -LiteralPath $launcher)
    if (Test-Path -LiteralPath $launcher) {
        $text = Get-Content -LiteralPath $launcher -Raw
        Check 'launcher uses portable working directory' ($text -match 'start\s+""\s+/D\s+"%~dp0"')
        Check 'launcher selects original HOTD2 Arcade set' ($text.Contains('%~dp0roms\hotd2.zip'))
    }
    Write-Host "Release checks: $script:checks total, $script:failures failed"
} finally {
    $resolved = [IO.Path]::GetFullPath($work)
    $cache = [IO.Path]::GetFullPath((Join-Path $repo '.cache')) + [IO.Path]::DirectorySeparatorChar
    if (-not $resolved.StartsWith($cache, [StringComparison]::OrdinalIgnoreCase)) { throw 'Test cleanup escaped private cache' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
if ($script:failures) { exit 1 }

exit 0
