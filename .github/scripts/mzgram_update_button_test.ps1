# The Update button, the way a user presses it.
#
# This build is rebuilt with the code just below the latest MZGram release
# and the test hook (MZGRAM_TEST_HOOKS, mzgram_update.cpp): with
# MZGRAM_TEST_PRESS_UPDATE set it installs the first release the bar offers
# as if Update was pressed. Only mzgram_update.cpp is compiled again, in
# the folder CMake made.
#
# Then it runs like a user's copy: it asks GitHub for the latest release,
# downloads SHA256SUMS and the archive of its architecture through GitHub's
# redirects, checks the SHA-256, unpacks the zip, restarts, and its Updater
# installs the release. Passes when the MZGram.exe and Updater.exe of the
# release zip replaced the build in the app folder, the release runs from
# there and the data in the working folder is still there. Fails at the
# first "MZGram Update Error" in log.txt and prints it.
#
# MZGram up to 1.0.7 never got past GitHub's redirect: it refused the
# address of release-assets (update_failed_network).
param(
	# The CMake build folder of the Build step
	[Parameter(Mandatory = $true)][string]$Out,
	[Parameter(Mandatory = $true)][string]$Arch
)

$ErrorActionPreference = 'Stop'
$repo = $env:GITHUB_REPOSITORY
$work = Join-Path $env:RUNNER_TEMP 'mzgram-update-button'
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

$tag = gh release view -R $repo --json tagName -q .tagName
if ($LASTEXITCODE -or $tag -notmatch '^v\d+\.\d+\.\d+$') { throw "no stable latest release: $tag" }
$code = ('' | python (Join-Path $PSScriptRoot 'mzgram_version.py') release $tag --upstream test --floor 0 | Where-Object { $_ -like 'code=*' }) -replace '^code=', ''
if ($LASTEXITCODE -or -not $code) { throw "no code for $tag" }
$below = [long]$code - 1
Write-Host "Update button: this build as code $below, the release $tag (code $code), windows-$Arch"

# The folder as CMake created it: TBUILD is a link to the build drive, and
# the same build reached through another path is generated anew and compiled
# from scratch
$cacheDir = Get-Content (Join-Path $Out 'CMakeCache.txt') | Where-Object { $_ -like 'CMAKE_CACHEFILE_DIR:INTERNAL=*' } | Select-Object -First 1
if (-not $cacheDir) { throw "no CMAKE_CACHEFILE_DIR in $Out" }
$Out = ($cacheDir -split '=', 2)[1]
Write-Host "Update button: the build folder $Out"

$started = Get-Date
cmake -D "MZGRAM_VERSION_CODE=$below" -D MZGRAM_TEST_HOOKS=ON $Out
if ($LASTEXITCODE) { throw 'cmake could not configure the test build' }
cmake --build $Out --config Debug --target Telegram Updater
if ($LASTEXITCODE) { throw 'the test build failed' }
$build = Join-Path $Out 'Debug'
$rebuilt = (Get-Item (Join-Path $build 'MZGram.exe')).LastWriteTime
if ($rebuilt -lt $started) { throw 'MZGram.exe was not linked again' }

# What the update must bring: the files of the release zip
$zip = "MZGram-Desktop-$($tag.Substring(1))-windows-$Arch.zip"
$release = Join-Path $work 'release'
gh release download $tag -R $repo -p $zip -p SHA256SUMS -D $release
if ($LASTEXITCODE) { throw "could not download $zip of $tag" }
$sums = Get-Content (Join-Path $release 'SHA256SUMS') | Where-Object { $_ -match " \*?$([regex]::Escape($zip))$" }
$expected = ($sums -split '\s+')[0].ToLower()
if (-not $expected -or $expected -ne (Get-FileHash (Join-Path $release $zip) -Algorithm SHA256).Hash.ToLower()) { throw "$zip does not match SHA256SUMS" }
Expand-Archive (Join-Path $release $zip) -DestinationPath (Join-Path $release 'unpacked')
$releaseExe = (Get-FileHash (Join-Path $release 'unpacked\MZGram\MZGram.exe')).Hash
$releaseUpdater = (Get-FileHash (Join-Path $release 'unpacked\MZGram\Updater.exe')).Hash

$app = Join-Path $work 'app'
New-Item -ItemType Directory $app | Out-Null
foreach ($name in 'MZGram.exe', 'Updater.exe') {
	Copy-Item (Join-Path $build $name) $app
}
$exe = Join-Path $app 'MZGram.exe'
if ((Get-FileHash $exe).Hash -eq $releaseExe) { throw 'the test build is the release itself' }

$data = Join-Path $work 'data'
New-Item -ItemType Directory (Join-Path $data 'tdata') | Out-Null
$marker = Join-Path $data 'tdata\mzgram_update_button_test'
[IO.File]::WriteAllText($marker, 'kept')

$env:MZGRAM_TEST_PRESS_UPDATE = '1'
Start-Process $exe -ArgumentList @('-debug', '-workdir', (WorkdirArgument $data))

$installed = $false
$failed = $null
$deadline = (Get-Date).AddSeconds(480)
while ((Get-Date) -lt $deadline) {
	Start-Sleep -Seconds 3
	$logs = Get-ChildItem $data -Filter 'log*.txt' -ErrorAction SilentlyContinue
	if ($logs) {
		$failed = Select-String -Path $logs.FullName -Pattern 'MZGram Update Error' | Select-Object -First 1
		if ($failed) { break }
	}
	$running = Get-Process MZGram -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $exe }
	# The Updater may be writing the file right now: then the next round
	try { $hash = (Get-FileHash $exe -ErrorAction Stop).Hash } catch { $hash = $null }
	if ($running -and $hash -eq $releaseExe) {
		$installed = $true
		break
	}
}

$problems = @()
if ($failed) { $problems += "the app stopped: $($failed.Line)" }
elseif (-not $installed) { $problems += "$tag is not running from $app" }
if ((Get-FileHash (Join-Path $app 'Updater.exe')).Hash -ne $releaseUpdater) { $problems += "Updater.exe is not the one of $tag" }
if (-not (Test-Path $marker) -or (Get-Content $marker) -ne 'kept') { $problems += 'the data in the working folder is gone' }

StopTestProcesses

if ($problems.Count) {
	Get-ChildItem $data -Recurse -ErrorAction SilentlyContinue | Where-Object { $_.FullName -notlike '*\tdata\*' } | Select-Object -First 40 | ForEach-Object { Write-Host $_.FullName }
	foreach ($log in Get-ChildItem $data -Recurse -Filter '*.txt' -ErrorAction SilentlyContinue) {
		Write-Host "--- $($log.FullName)"
		Get-Content $log.FullName | Where-Object { $_ -match 'Update|Launched' } | Select-Object -Last 40
	}
	foreach ($problem in $problems) { Write-Host "FAIL: $problem" }
	exit 1
}
Write-Host "PASS: the Update button installed $tag over code $below, windows-$Arch, the data stayed"
# GitHub's pwsh step ends with "exit $LASTEXITCODE", the code of the last
# native command
exit 0
