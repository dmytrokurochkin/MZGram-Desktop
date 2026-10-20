# Self-update from the latest release to this build, the way a user gets it.
#
# The latest MZGram release (its Windows zip, checked against SHA256SUMS)
# starts with an update prepared in its working folder exactly as its own
# Update button leaves it (mzgram_self_update.cpp, verifyAndUnpack): the
# files of the new version in tupdates/temp, tdata/mzgram_version with the
# code of the next release and "ready". The release itself then installs it
# with its Updater. Passes when the new MZGram.exe and Updater.exe replaced
# the old ones, the new MZGram.exe runs from the app folder and the data in
# the working folder is still there.
#
# First the new build starts alone, as from its zip on a computer with
# nothing but Windows: no PostgreSQL in PATH (mzgram_check_imports.py).
# MZGram x64 up to 1.0.4 imports libpq.dll and starts only where
# PostgreSQL's bin folder is in PATH; that release gets the runner's.
param(
	[Parameter(Mandatory = $true)][string]$Build,
	[Parameter(Mandatory = $true)][string]$Arch,
	# The release to update from; the latest one by default
	[string]$From
)

$ErrorActionPreference = 'Stop'
$repo = $env:GITHUB_REPOSITORY
$work = Join-Path $env:RUNNER_TEMP 'mzgram-self-update'
if (Test-Path $work) { Remove-Item -Recurse -Force $work }
New-Item -ItemType Directory $work | Out-Null

# Forward slashes: a backslash before the closing quote would escape it
function WorkdirArgument([string]$folder) {
	return '"' + $folder.Replace([char]92, [char]47) + '/"'
}
function StopTestProcesses {
	Get-Process MZGram, Updater -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$work*" } | Stop-Process -Force
	Start-Sleep -Seconds 2
}
$plainPath = ($env:PATH -split ';' | Where-Object { $_ -and $_ -notmatch 'PostgreSQL' }) -join ';'

# The new build alone, with Windows only
$clean = Join-Path $work 'clean'
New-Item -ItemType Directory (Join-Path $clean 'app'), (Join-Path $clean 'data') | Out-Null
foreach ($name in 'MZGram.exe', 'Updater.exe') {
	Copy-Item (Join-Path $Build $name) (Join-Path $clean 'app')
}
$env:PATH = $plainPath
Start-Process (Join-Path $clean 'app\MZGram.exe') -ArgumentList @('-workdir', (WorkdirArgument (Join-Path $clean 'data')))
$started = $false
$deadline = (Get-Date).AddSeconds(120)
while ((Get-Date) -lt $deadline) {
	Start-Sleep -Seconds 3
	$logs = Get-ChildItem (Join-Path $clean 'data') -Recurse -Filter 'log*.txt' -ErrorAction SilentlyContinue
	if ($logs -and (Select-String -Path $logs.FullName -Pattern 'Launched version' -Quiet)) {
		$started = $true
		break
	}
}
StopTestProcesses
if (-not $started) {
	Write-Host "FAIL: the new MZGram.exe does not start with Windows only (no log in $clean)"
	exit 1
}
Write-Host 'PASS: the new MZGram.exe starts with Windows only'

$from = $From
if (-not $from) {
	$from = gh release view -R $repo --json tagName -q .tagName
	if ($LASTEXITCODE) { throw 'no latest release' }
}
if ($from -notmatch '^v(\d+)\.(\d+)\.(\d+)$') { throw "latest release $from is not vX.Y.Z" }
$to = "v$($Matches[1]).$($Matches[2]).$([int]$Matches[3] + 1)"
$toCode = [int]$Matches[1] * 1000000 + [int]$Matches[2] * 10000 + ([int]$Matches[3] + 1) * 100 + 99
Write-Host "self-update: $from -> $to (code $toCode), windows-$Arch"

$zip = "MZGram-Desktop-$($from.Substring(1))-windows-$Arch.zip"
gh release download $from -R $repo -p $zip -p SHA256SUMS -D $work
if ($LASTEXITCODE) { throw "could not download $zip of $from" }
$sums = Get-Content (Join-Path $work 'SHA256SUMS') | Where-Object { $_ -match " \*?$([regex]::Escape($zip))$" }
$expected = ($sums -split '\s+')[0].ToLower()
$actual = (Get-FileHash (Join-Path $work $zip) -Algorithm SHA256).Hash.ToLower()
if (-not $expected -or $expected -ne $actual) { throw "$zip does not match SHA256SUMS" }
Expand-Archive (Join-Path $work $zip) -DestinationPath (Join-Path $work 'install')
$app = Join-Path $work 'install\MZGram'

# The working folder with data the update must keep
$data = Join-Path $work 'data'
New-Item -ItemType Directory (Join-Path $data 'tdata') | Out-Null
$marker = Join-Path $data 'tdata\mzgram_self_update_test'
[IO.File]::WriteAllText($marker, 'kept')

# The update, as the Update button of the release prepares it
$ready = Join-Path $data 'tupdates\temp'
New-Item -ItemType Directory (Join-Path $ready 'tdata') | Out-Null
foreach ($name in 'MZGram.exe', 'Updater.exe') {
	Copy-Item (Join-Path $Build $name) $ready
}
[IO.File]::WriteAllText((Join-Path $ready 'tdata\mzgram_version'), "$toCode")
[IO.File]::WriteAllText((Join-Path $ready 'ready'), '1')

$newHash = (Get-FileHash (Join-Path $Build 'MZGram.exe')).Hash
$newUpdater = (Get-FileHash (Join-Path $Build 'Updater.exe')).Hash
if ($newHash -eq (Get-FileHash (Join-Path $app 'MZGram.exe')).Hash) { throw 'the build is the release itself' }

# A release that imports libpq.dll gets PostgreSQL's bin folder in PATH
$oldImports = python (Join-Path $PSScriptRoot 'mzgram_check_imports.py') (Join-Path $app 'MZGram.exe')
if ($oldImports -match 'libpq\.dll') {
	$pgBin = $env:PGBIN
	if (-not $pgBin) {
		$pgBin = Get-ChildItem 'C:\Program Files\PostgreSQL\*in' -Directory -ErrorAction SilentlyContinue | Select-Object -Last 1 -ExpandProperty FullName
	}
	if (-not $pgBin -or -not (Test-Path (Join-Path $pgBin 'libpq.dll'))) { throw "$from imports libpq.dll and the runner has no PostgreSQL" }
	Write-Host "$from imports libpq.dll: started with $pgBin in PATH"
	$env:PATH = "$pgBin;$plainPath"
}
Start-Process (Join-Path $app 'MZGram.exe') -ArgumentList @('-debug', '-workdir', (WorkdirArgument $data))
$env:PATH = $plainPath

$exe = Join-Path $app 'MZGram.exe'
$installed = $false
$deadline = (Get-Date).AddSeconds(180)
while ((Get-Date) -lt $deadline) {
	Start-Sleep -Seconds 3
	$running = Get-Process MZGram -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $exe }
	if ($running -and (Get-FileHash $exe).Hash -eq $newHash) {
		$installed = $true
		break
	}
}

$problems = @()
if (-not $installed) { $problems += "the new MZGram.exe is not running from $app" }
if ((Get-FileHash (Join-Path $app 'Updater.exe')).Hash -ne $newUpdater) { $problems += 'Updater.exe is not the new one' }
if (-not (Test-Path $marker) -or (Get-Content $marker) -ne 'kept') { $problems += 'the data in the working folder is gone' }
if (Test-Path (Join-Path $ready 'ready')) { $problems += 'the update is still waiting in tupdates/temp' }

StopTestProcesses

if ($problems.Count) {
	Get-ChildItem $work -Recurse -Exclude '*.dll' | Select-Object -First 60 | ForEach-Object { Write-Host $_.FullName }
	foreach ($log in Get-ChildItem $work -Recurse -Filter '*.txt' -ErrorAction SilentlyContinue) {
		Write-Host "--- $($log.FullName)"
		Get-Content $log.FullName -Tail 40
	}
	foreach ($problem in $problems) { Write-Host "FAIL: $problem" }
	exit 1
}
Write-Host "PASS: $from installed $to over itself, the data stayed"
