# Installe Freeze Shimmer (VST3) dans le dossier VST3 systeme de Windows.
# Utilisation : clic droit > "Executer avec PowerShell" (les droits administrateur sont demandes).
$ErrorActionPreference = 'Stop'

$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Start-Process powershell -Verb RunAs -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`""
    exit
}

$src  = Join-Path $PSScriptRoot 'Freeze Shimmer.vst3'
$dest = Join-Path $env:CommonProgramFiles 'VST3'
if (-not (Test-Path $src)) { throw "Dossier 'Freeze Shimmer.vst3' introuvable a cote du script." }

New-Item -ItemType Directory -Force -Path $dest | Out-Null
$target = Join-Path $dest 'Freeze Shimmer.vst3'
if (Test-Path $target) { Remove-Item -Recurse -Force $target }
Copy-Item -Recurse -Force $src $dest

Write-Host "Freeze Shimmer installe dans : $target" -ForegroundColor Green
Write-Host "Relancez le scan des plugins de votre DAW."
Read-Host "Appuyez sur Entree pour fermer"
