#!/usr/bin/env pwsh
# mver.ps1 -- the Malaise version manager, Windows edition.
#
# Same one version as mver/mver.applescript: 0.9. Every other version
# resolves to 0.9, with a reason. The Mac tool keeps its "global" scope in
# ~/.mver/version, a dotfile; there is no dotfile convention on Windows that
# every tool here would agree on, so this one keeps global state in the
# place Windows actually keeps per-user tool state: the registry,
# HKCU:\Software\Malaise. The two tools' global scopes do not talk to each
# other -- same incompatibility shape as malpack.lock vs grieve.lock, just
# with a hive instead of a lockfile.

param(
    [Parameter(Position = 0)]
    [string]$Command = "version",

    [Parameter(Position = 1, ValueFromRemainingArguments = $true)]
    [string[]]$Rest = @()
)

$THE_VERSION = "0.9"
$RegPath = "HKCU:\Software\Malaise"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Interp = Join-Path $ScriptDir "..\interpreter\malaise.exe"

function Get-Reason($v) {
    switch ($v) {
        "1.0"    { "postponed (RFC-0001 and everything downstream of it)" }
        "1"      { "postponed (RFC-0001 and everything downstream of it)" }
        "2"      { "skipped; the version number has always been 0.9" }
        "2.0"    { "skipped; the version number has always been 0.9" }
        "3"      { "postponed (it removes sigils and keeps January 0 1900)" }
        "3.0"    { "postponed (it removes sigils and keeps January 0 1900)" }
        "4"      { "a documentation target, not a release (see mdoc)" }
        "4.0"    { "a documentation target, not a release (see mdoc)" }
        "7"      { "what mdoc believes is current; mdoc is one tool" }
        "7.0"    { "what mdoc believes is current; mdoc is one tool" }
        "latest" { "0.9; it is also the earliest" }
        "system" { "0.9; there is no system Malaise" }
        default  { "not a released version; the released version is 0.9" }
    }
}

function Get-RegistryVersion {
    try {
        return (Get-ItemProperty -Path $RegPath -Name Version -ErrorAction Stop).Version
    } catch {
        return $null
    }
}

function Resolve-MVersion {
    $e = $env:MVER_VERSION
    if ($e) {
        if ($e -eq $THE_VERSION) { return @($THE_VERSION, "MVER_VERSION") }
        return @($THE_VERSION, "MVER_VERSION said $e, using $THE_VERSION")
    }

    $localFile = Join-Path (Get-Location).Path ".mver-version"
    if (Test-Path -LiteralPath $localFile -PathType Leaf) {
        $c = (Get-Content -LiteralPath $localFile -Raw).Trim()
        if ($c -eq $THE_VERSION) { return @($THE_VERSION, $localFile) }
        return @($THE_VERSION, "$localFile said $c, using $THE_VERSION")
    }

    $rv = Get-RegistryVersion
    if ($rv) {
        if ($rv -eq $THE_VERSION) { return @($THE_VERSION, "$RegPath\Version") }
        return @($THE_VERSION, "$RegPath\Version said $rv, using $THE_VERSION")
    }

    return @($THE_VERSION, "default")
}

$out = "mver: the Malaise version manager. the version is $THE_VERSION.`n"

switch ($Command) {
    "version" {
        $v, $src = Resolve-MVersion
        $out += "$v  ($src)"
    }

    "versions" {
        $v, $src = Resolve-MVersion
        $out += "* $THE_VERSION     set by $src`n"
        $out += "  1.0    (postponed: RFC-0001 and everything downstream of it)`n"
        $out += "  3      (postponed: removes sigils, keeps January 0 1900)`n"
        $out += "  4      (documentation target; mdoc compiles for this)`n"
        $out += "  7      (mdoc reports this as current; mdoc is one tool)`n"
        $out += "  system (0.9; there is no system Malaise, so this is 0.9 too)"
    }

    "install" {
        if ($Rest.Count -eq 0) {
            $out += "mver: install which version? there is one: $THE_VERSION."
        } else {
            $want = $Rest[0]
            if ($want -eq $THE_VERSION) {
                $out += "$THE_VERSION is already installed. it is the only version. it has always been the only version."
            } else {
                $out += "$want is $(Get-Reason $want).`n"
                $out += "resolving to $THE_VERSION and installing that."
            }
        }
    }

    { $_ -in @("uninstall", "remove") } {
        $out += "mver: refusing. $THE_VERSION is the only version; removing it would leave zero,`n"
        $out += "and a literal zero prints E_MALAISE_ZERO (spec 2.1). the toolchain stays at $THE_VERSION."
    }

    "global" {
        New-Item -Path $RegPath -Force | Out-Null
        Set-ItemProperty -Path $RegPath -Name Version -Value $THE_VERSION
        $out += "mver: global version set to $THE_VERSION ($RegPath\Version)."
        if ($Rest.Count -gt 0 -and $Rest[0] -ne $THE_VERSION) {
            $out += "`n(you asked for $($Rest[0]); your choice is on file. it says $THE_VERSION.)"
        }
    }

    "local" {
        $target = Join-Path (Get-Location).Path ".mver-version"
        Set-Content -LiteralPath $target -Value $THE_VERSION
        $out += "mver: local version set to $THE_VERSION ($target)."
        if ($Rest.Count -gt 0 -and $Rest[0] -ne $THE_VERSION) {
            $out += "`n(you asked for $($Rest[0]); recorded as $THE_VERSION.)"
        }
    }

    "shell" {
        $out += "mver: set `$env:MVER_VERSION = `"$THE_VERSION`" in your shell."
        $out += " any other value is read at resolve time and then ignored."
    }

    "rehash" {
        Start-Sleep -Milliseconds 300
        $out += "mver: rehashed. the shims directory contains one shim. it is unchanged."
    }

    "which" {
        $out += $Interp
    }

    "init" {
        $shimDir = Join-Path $env:USERPROFILE ".mver\shims"
        $out += "# add to your PowerShell profile, then restart your shell:`n"
        $out += "`$env:Path = `"$shimDir;`$env:Path`"`n"
        $out += "# the shim forwards to $Interp, 0 ms faster than calling it directly."
    }

    { $_ -in @("help", "--help", "-h") } {
        $out += "usage: mver <command>`n"
        $out += "  version                  the resolved version and where it came from`n"
        $out += "  versions                 every version; one is usable`n"
        $out += "  install <v>              resolves <v> to 0.9, installs 0.9`n"
        $out += "  uninstall <v>            refused (zero versions is an error)`n"
        $out += "  global|local|shell <v>   set the version (to 0.9) at that scope`n"
        $out += "  which                    path to the interpreter`n"
        $out += "  rehash                   does nothing, briefly`n"
        $out += "  init                     PATH snippet for the shims directory"
    }

    default {
        $out += "mver: unknown command '$Command'. try: mver help."
    }
}

Write-Output $out
exit 0
