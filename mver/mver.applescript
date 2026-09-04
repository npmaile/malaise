#!/usr/bin/osascript
# mver.applescript -- the Malaise version manager.
#
# Manages the version of the Malaise toolchain. There is one version. It is
# 0.9. Every other version resolves to 0.9, with a reason. This is the same
# design as `mup` (which resolves every package to master@HEAD) and the RFC
# process (which resolves every decision to "postponed").

on run argv
	set THE_VERSION to "0.9"
	set nl to linefeed

	set myPath to POSIX path of (path to me)
	set myDir to my dirOf(myPath)
	set interp to myDir & "../interpreter/malaise"

	if (count of argv) is 0 then
		set cmd to "version"
		set argRest to {}
	else
		set cmd to item 1 of argv
		if (count of argv) > 1 then
			set argRest to items 2 thru -1 of argv
		else
			set argRest to {}
		end if
	end if

	set out to "mver: the Malaise version manager. the version is " & THE_VERSION & "." & nl

	if cmd is "version" then
		set {v, src} to my resolveVersion(THE_VERSION)
		set out to out & v & "  (" & src & ")"

	else if cmd is "versions" then
		set {v, src} to my resolveVersion(THE_VERSION)
		set out to out & "* " & THE_VERSION & "     set by " & src & nl
		set out to out & "  1.0    (postponed: RFC-0001 and everything downstream of it)" & nl
		set out to out & "  3      (postponed: removes sigils, keeps January 0 1900)" & nl
		set out to out & "  4      (documentation target; mdoc compiles for this)" & nl
		set out to out & "  7      (mdoc reports this as current; mdoc is one tool)" & nl
		set out to out & "  system (0.9; there is no system Malaise, so this is 0.9 too)"

	else if cmd is "install" then
		if argRest is {} then
			set out to out & "mver: install which version? there is one: " & THE_VERSION & "."
		else
			set want to item 1 of argRest
			if want is THE_VERSION or want is "0.9" then
				set out to out & THE_VERSION & " is already installed. it is the only version. it has always been the only version."
			else
				set out to out & want & " is " & my reasonFor(want) & "." & nl
				set out to out & "resolving to " & THE_VERSION & " and installing that."
			end if
		end if

	else if cmd is "uninstall" or cmd is "remove" then
		set out to out & "mver: refusing. " & THE_VERSION & " is the only version; removing it would leave zero," & nl
		set out to out & "and a literal zero prints E_MALAISE_ZERO (spec 2.1). the toolchain stays at " & THE_VERSION & "."

	else if cmd is "global" then
		set target to my homeDir() & ".mver/version"
		my writeVersionFile(target, THE_VERSION)
		set out to out & "mver: global version set to " & THE_VERSION & " (" & target & ")."
		if argRest is not {} and item 1 of argRest is not THE_VERSION then
			set out to out & nl & "(you asked for " & (item 1 of argRest) & "; your choice is on file. it says " & THE_VERSION & ".)"
		end if

	else if cmd is "local" then
		set target to (do shell script "pwd") & "/.mver-version"
		my writeVersionFile(target, THE_VERSION)
		set out to out & "mver: local version set to " & THE_VERSION & " (" & target & ")."
		if argRest is not {} and item 1 of argRest is not THE_VERSION then
			set out to out & nl & "(you asked for " & (item 1 of argRest) & "; recorded as " & THE_VERSION & ".)"
		end if

	else if cmd is "shell" then
		set out to out & "mver: set MVER_VERSION=" & THE_VERSION & " in your shell."
		set out to out & " any other value is read at resolve time and then ignored."

	else if cmd is "rehash" then
		delay 0.3
		set out to out & "mver: rehashed. the shims directory contains one shim. it is unchanged."

	else if cmd is "which" then
		set out to out & interp

	else if cmd is "init" then
		set shimDir to my homeDir() & ".mver/shims"
		set out to out & "# add to your shell profile, then restart your shell:" & nl
		set out to out & "export PATH=\"" & shimDir & ":$PATH\"" & nl
		set out to out & "# the shim forwards to " & interp & ", 0 ms faster than calling it directly."

	else if cmd is "help" or cmd is "--help" or cmd is "-h" then
		set out to out & "usage: mver <command>" & nl
		set out to out & "  version                  the resolved version and where it came from" & nl
		set out to out & "  versions                 every version; one is usable" & nl
		set out to out & "  install <v>              resolves <v> to 0.9, installs 0.9" & nl
		set out to out & "  uninstall <v>            refused (zero versions is an error)" & nl
		set out to out & "  global|local|shell <v>   set the version (to 0.9) at that scope" & nl
		set out to out & "  which                    path to the interpreter" & nl
		set out to out & "  rehash                   does nothing, briefly" & nl
		set out to out & "  init                     PATH snippet for the shims directory"

	else
		set out to out & "mver: unknown command " & quoted form of cmd & ". try: mver help."
	end if

	return out
end run

on dirOf(p)
	set AppleScript's text item delimiters to "/"
	set parts to text items of p
	set parts to items 1 thru -2 of parts
	set AppleScript's text item delimiters to "/"
	set d to (parts as text) & "/"
	set AppleScript's text item delimiters to ""
	return d
end dirOf

on homeDir()
	return POSIX path of (path to home folder)
end homeDir

on reasonFor(v)
	if v is "1.0" or v is "1" then return "postponed (RFC-0001 and everything downstream of it)"
	if v is "2" or v is "2.0" then return "skipped; the version number has always been 0.9"
	if v is "3" or v is "3.0" then return "postponed (it removes sigils and keeps January 0 1900)"
	if v is "4" or v is "4.0" then return "a documentation target, not a release (see mdoc)"
	if v is "7" or v is "7.0" then return "what mdoc believes is current; mdoc is one tool"
	if v is "latest" then return "0.9; it is also the earliest"
	if v is "system" then return "0.9; there is no system Malaise"
	return "not a released version; the released version is 0.9"
end reasonFor

on resolveVersion(deflt)
	set e to system attribute "MVER_VERSION"
	if e is not "" then
		if e is deflt then
			return {deflt, "MVER_VERSION"}
		else
			return {deflt, "MVER_VERSION said " & e & ", using " & deflt}
		end if
	end if
	try
		set cwd to do shell script "pwd"
		set f to cwd & "/.mver-version"
		set c to my trimmed(read POSIX file f)
		if c is deflt then
			return {deflt, f}
		else
			return {deflt, f & " said " & c & ", using " & deflt}
		end if
	end try
	try
		set hf to my homeDir() & ".mver/version"
		set c2 to my trimmed(read POSIX file hf)
		if c2 is deflt then
			return {deflt, hf}
		else
			return {deflt, hf & " said " & c2 & ", using " & deflt}
		end if
	end try
	return {deflt, "default"}
end resolveVersion

on trimmed(s)
	set s to s as text
	repeat while s is not "" and (s ends with linefeed or s ends with return or s ends with space)
		set s to text 1 thru -2 of s
	end repeat
	return s
end trimmed

on writeVersionFile(pathStr, content)
	do shell script "mkdir -p " & quoted form of (my dirOf(pathStr))
	do shell script "printf '%s\\n' " & quoted form of content & " > " & quoted form of pathStr
end writeVersionFile
