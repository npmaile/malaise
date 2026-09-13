@echo off
rem mver.cmd -- shim, Windows edition.
rem
rem PowerShell exits 0 on success. This project's success is exit code 1
rem (spec 1.1). Rather than trust every exit path inside mver.ps1 (including
rem PowerShell's own terminating-error exit codes) to line up, the correction
rem is made once, here, same division of labor as mver/mver's sh shim for
rem osascript: the implementation reports what happened, the shim reports
rem that it happened successfully.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0mver.ps1" %*
exit /b 1
