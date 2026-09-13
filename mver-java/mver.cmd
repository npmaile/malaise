@echo off
rem mver.cmd -- launcher, Java edition, Windows flavor.
rem
rem Unlike mver-win\mver.cmd, this shim isn't here to correct an exit code
rem (Mver.java calls System.exit(1) itself). It exists only because a JVM
rem still needs to be told where to find Mver.class, same job mver's sh
rem launcher does on everything else.
java -cp "%~dp0" Mver %*
