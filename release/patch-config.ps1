<#  Gamepads of Valhalla - config auto-patcher
    Applies the gamepad bindings to User.ini and the required joystick
    settings to Rune.ini, in the folder this script sits in (the game's
    System\ folder). Makes one-time backups (*.gov-bak) and can restore them.

    Usage (normally via the .bat launchers):
      patch-config.ps1            -> apply
      patch-config.ps1 -Restore   -> restore the original .ini from backup
#>
param([switch]$Restore)
$ErrorActionPreference = 'Stop'

# resolve our own folder (PS 2.0-safe for un-updated Windows 7)
$ScriptDir = $PSScriptRoot
if (-not $ScriptDir) { $ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition }

function Say($m,$c='Gray'){ Write-Host $m -ForegroundColor $c }
$enc = [System.Text.Encoding]::GetEncoding(1252)   # ANSI, no BOM - what UE1 expects
function WriteLines($path,$arr){ [System.IO.File]::WriteAllLines($path,[string[]]$arr,$enc) }

try {
    $dir   = $ScriptDir
    $user  = Join-Path $dir 'User.ini'
    $rune  = Join-Path $dir 'Rune.ini'
    $bindF = Join-Path $dir 'copy_this_to_User_ini.txt'

    Say "=== Gamepads of Valhalla - config patcher ===" Cyan
    Say "Folder: $dir`n"

    # game must be closed (UE1 rewrites both .ini on exit)
    if (Get-Process -Name 'Rune' -ErrorAction SilentlyContinue) {
        throw "Rune is running. Close the game completely, then run this again (it rewrites User.ini/Rune.ini on exit)."
    }

    # ---------- RESTORE ----------
    if ($Restore) {
        $any = $false
        foreach ($f in @($user,$rune)) {
            $bak = "$f.gov-bak"
            if (Test-Path $bak) { Copy-Item $bak $f -Force; Say "Restored $(Split-Path $f -Leaf) from backup." Green; $any=$true }
            else { Say "No backup for $(Split-Path $f -Leaf) (expected $(Split-Path $bak -Leaf))." Yellow }
        }
        if ($any) { Say "`nOriginal configuration restored." Green }
        return
    }

    # ---------- APPLY ----------
    foreach ($f in @($user,$rune)) {
        if (-not (Test-Path $f)) { throw "Not found: $f`nPut these files in the game's System\ folder (next to Rune.exe) and run again." }
    }
    if (-not (Test-Path $bindF)) { throw "Missing copy_this_to_User_ini.txt next to this script." }

    if ($dir -match 'Program Files') {
        Say "WARNING: the game is under 'Program Files'. Windows may redirect saved configs to" Yellow
        Say "         %LOCALAPPDATA%\VirtualStore, so these edits might not take effect." Yellow
        Say "         If controls don't apply: run this as Administrator, or install Rune outside Program Files.`n" Yellow
    }

    # one-time backups (preserve the pristine originals)
    foreach ($f in @($user,$rune)) {
        $bak = "$f.gov-bak"
        if (-not (Test-Path $bak)) { Copy-Item $f $bak; Say "Backup: $(Split-Path $bak -Leaf)" }
    }

    # bindings block from the txt (strip comments/blanks)
    $joyLines = @(Get-Content $bindF | Where-Object { $_ -match '^\s*Joy' } | ForEach-Object { $_.Trim() })
    if ($joyLines.Count -eq 0) { throw "No Joy* lines found in copy_this_to_User_ini.txt." }
    $joyKeys  = @($joyLines | ForEach-Object { ($_ -split '=',2)[0].Trim() })

    # ---- User.ini: replace Joy* lines inside [Engine.Input] ----
    $lines = @(Get-Content $user)
    $out = New-Object System.Collections.Generic.List[string]
    $inSec=$false; $inserted=$false; $seen=$false
    foreach ($ln in $lines) {
        if ($ln -match '^\s*\[(.+)\]\s*$') {
            if ($inSec -and -not $inserted) { $joyLines | ForEach-Object { $out.Add($_) }; $inserted=$true }
            $inSec = ($Matches[1] -eq 'Engine.Input')
            if ($inSec) { $seen=$true }
            $out.Add($ln); continue
        }
        if ($inSec -and ($joyKeys -contains (($ln -split '=',2)[0].Trim()))) { continue }
        $out.Add($ln)
    }
    if ($inSec -and -not $inserted) { $joyLines | ForEach-Object { $out.Add($_) } }
    if (-not $seen) { $out.Add('[Engine.Input]'); $joyLines | ForEach-Object { $out.Add($_) } }
    WriteLines $user $out
    Say "User.ini : gamepad bindings applied." Green

    # ---- Rune.ini: set keys in [WinDrv.WindowsClient] ----
    # explicit order array keeps PS 2.0 compatibility (no [ordered])
    $desiredOrder = @('UseJoystick','InvertVertical','DeadZoneXYZ','DeadZoneRUV','ScaleXYZ','ScaleRUV','UseDirectInput')
    $desired = @{
        'UseJoystick'    = 'True'
        'InvertVertical' = 'False'
        'DeadZoneXYZ'    = 'True'
        'DeadZoneRUV'    = 'True'
        'ScaleXYZ'       = '1000.000000'
        'ScaleRUV'       = '1000.000000'
        'UseDirectInput' = 'False'
    }
    $lines = @(Get-Content $rune)
    $out = New-Object System.Collections.Generic.List[string]
    $inSec=$false; $seen=$false; $applied=@{}
    foreach ($ln in $lines) {
        if ($ln -match '^\s*\[(.+)\]\s*$') {
            if ($inSec) { foreach ($k in $desiredOrder) { if (-not $applied[$k]) { $out.Add("$k=$($desired[$k])"); $applied[$k]=$true } } }
            $inSec = ($Matches[1] -eq 'WinDrv.WindowsClient')
            if ($inSec) { $seen=$true }
            $out.Add($ln); continue
        }
        if ($inSec) {
            $key = ($ln -split '=',2)[0].Trim()
            if ($desired.ContainsKey($key)) { $out.Add("$key=$($desired[$key])"); $applied[$key]=$true; continue }
        }
        $out.Add($ln)
    }
    if ($inSec) { foreach ($k in $desiredOrder) { if (-not $applied[$k]) { $out.Add("$k=$($desired[$k])"); $applied[$k]=$true } } }
    if (-not $seen) { $out.Add('[WinDrv.WindowsClient]'); foreach ($k in $desiredOrder) { $out.Add("$k=$($desired[$k])") } }
    WriteLines $rune $out
    Say "Rune.ini : joystick settings applied." Green

    Say "`nDone! Connect ONE gamepad (powered on) and launch Rune." Green
    Say "Backups: User.ini.gov-bak / Rune.ini.gov-bak  ->  run restore-original-config.bat to undo."
    Say ""
    Say "TIP: if a button doesn't respond in-game (for example A / Jump):" Yellow
    Say "  1) Close Rune" Yellow
    Say "  2) Switch the gamepad Off and turn it back On (unplug/plug it back" Yellow
    Say "     if it is wired), then start the game again" Yellow
    Say "  3) If it doesn't help - simply restart your PC (or re-login into your" Yellow
    Say "     Windows user on Win10/11) and it will remedy the bug" Yellow
}
catch {
    Say "`nERROR: $($_.Exception.Message)" Red
}
finally {
    Write-Host ''
    Read-Host 'Press Enter to close' | Out-Null
}
