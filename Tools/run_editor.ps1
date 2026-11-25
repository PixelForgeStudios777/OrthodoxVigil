# Runs a UE Python script inside a full (rendering) editor session, then prints tagged log lines.
param([string]$Script, [string]$Tag = "VIGIL")
$p = Split-Path -Parent $PSScriptRoot
$log = "run_" + [IO.Path]::GetFileNameWithoutExtension($Script) + ".log"
Start-Process -FilePath "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList "`"$p\OrthodoxVigil.uproject`"","-ExecutePythonScript=`"$Script`"","-unattended","-nosplash","-FullStdOutLogOutput","-log=$log" -Wait
Select-String -Path "$p\Saved\Logs\$log" -Pattern "$Tag|Error:|Traceback|Exception" | ForEach-Object { $_.Line } | Select-Object -First 200
