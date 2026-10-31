# The "Ask AI" window, without the real sites.
#
# This build is rebuilt with the test hook (MZGRAM_TEST_HOOKS, mzgram_ai.cpp):
# with MZGRAM_TEST_AI set, it opens the AI window of every site in turn, the
# same window "Ask AI" opens, in its own Webview profile. Instead of the site
# it loads a local page with an input field like the site's and a Send
# button, puts a test message and prompt there with the script it uses on
# the site, and reads the field back. Only mzgram_ai.cpp is compiled again,
# in the folder CMake made.
#
# Passes when the window of each of the five sites was created and its
# field got the text while Send was never pressed. Signing in and the real
# sites are not checked here.
param(
	# The CMake build folder of the Build step
	[Parameter(Mandatory = $true)][string]$Out,
	[Parameter(Mandatory = $true)][string]$Arch
)

$ErrorActionPreference = 'Stop'
$work = Join-Path $env:RUNNER_TEMP 'mzgram-ai-window'
if (Test-Path $work) { Remove-Item -Recurse -Force $work }
New-Item -ItemType Directory $work | Out-Null

# Forward slashes: a backslash before the closing quote would escape it
function WorkdirArgument([string]$folder) {
	return '"' + $folder.Replace([char]92, [char]47) + '/"'
}

# The folder as CMake created it: TBUILD is a link to the build drive, and
# the same build reached through another path is generated anew and compiled
# from scratch
$cacheDir = Get-Content (Join-Path $Out 'CMakeCache.txt') | Where-Object { $_ -like 'CMAKE_CACHEFILE_DIR:INTERNAL=*' } | Select-Object -First 1
if (-not $cacheDir) { throw "no CMAKE_CACHEFILE_DIR in $Out" }
$Out = ($cacheDir -split '=', 2)[1]
Write-Host "AI window: the build folder $Out, windows-$Arch"

cmake -D MZGRAM_TEST_HOOKS=ON $Out
if ($LASTEXITCODE) { throw 'cmake could not configure the test build' }
cmake --build $Out --config Debug --target Telegram
if ($LASTEXITCODE) { throw 'the test build failed' }
$exe = Join-Path $Out 'Debug\MZGram.exe'

$data = Join-Path $work 'data'
New-Item -ItemType Directory $data | Out-Null

$env:MZGRAM_TEST_AI = '1'
$process = Start-Process $exe -ArgumentList @('-debug', '-workdir', (WorkdirArgument $data)) -PassThru

$sites = 'gemini', 'claude', 'chatgpt', 'deepseek', 'grok'
$lines = @()
$deadline = (Get-Date).AddSeconds(300)
while ((Get-Date) -lt $deadline) {
	Start-Sleep -Seconds 3
	$logs = Get-ChildItem $data -Filter 'log*.txt' -ErrorAction SilentlyContinue
	if ($logs) {
		$lines = @(Select-String -Path $logs.FullName -Pattern 'MZGram AI test: ' | ForEach-Object { $_.Line })
		if ($lines | Where-Object { $_ -like '*MZGram AI test: done*' }) { break }
	}
	if ($process.HasExited) { break }
}
Get-Process MZGram -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $exe } | Stop-Process -Force
Start-Sleep -Seconds 2

foreach ($line in $lines) { Write-Host $line }
$problems = @()
if (-not ($lines | Where-Object { $_ -like '*MZGram AI test: done*' })) { $problems += 'the test did not finish' }
foreach ($site in $sites) {
	if (-not ($lines | Where-Object { $_ -like "*MZGram AI test: $site window created*" })) { $problems += "$site`: no window" }
	if (-not ($lines | Where-Object { $_ -like "*MZGram AI test: PASS $site*" })) { $problems += "$site`: no pass" }
}
foreach ($line in $lines | Where-Object { $_ -like '*MZGram AI test: FAIL*' }) { $problems += $line }

if ($problems.Count) {
	foreach ($log in Get-ChildItem $data -Recurse -Filter 'log*.txt' -ErrorAction SilentlyContinue) {
		Write-Host "--- $($log.FullName)"
		Get-Content $log.FullName | Where-Object { $_ -match 'AI|Webview|WebView' } | Select-Object -Last 60
	}
	foreach ($problem in $problems) { Write-Host "FAIL: $problem" }
	exit 1
}
Write-Host "PASS: the AI window of each of the five sites was created and got the text, nothing sent, windows-$Arch"
# GitHub's pwsh step ends with "exit $LASTEXITCODE", the code of the last
# native command
exit 0
