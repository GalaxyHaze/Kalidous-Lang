param(
    [string]$Version = ""
)

# 1. Setup Global Variables
# Update this if your repository owner/name is different
$Repo = "GalaxyHaze/Kalidous"
$ApiUrl = "https://api.github.com/repos/$Repo/releases/latest"

# 2. Determine Version
if ([string]::IsNullOrWhiteSpace($Version)) {
    Write-Host "No version specified. Fetching latest version..."
    try {
        $Response = Invoke-RestMethod -Uri $ApiUrl
        $Version = $Response.tag_name
        Write-Host "Latest version found: $Version" -ForegroundColor Green
    }
    catch {
        Write-Error "Failed to fetch latest version from GitHub. Check your internet connection."
        exit 1
    }
}
else {
    Write-Host "Installing requested version: $Version" -ForegroundColor Yellow
}

# 3. Detect OS and Architecture
$OS = "windows"
$Arch = "amd64"

if ($env:PROCESSOR_ARCHITECTURE -eq "ARM64") {
    $Arch = "arm64"
}

$FileName = "kalidous-$OS-$Arch.exe"

# 4. Setup Download URL and Destination
$DownloadUrl = "https://github.com/$Repo/releases/download/$Version/$FileName"
$TempPath = "$env:TEMP\kalidous-installer.exe"

# Using Local AppData for user-level installation (no Admin required usually)
$InstallDir = "$env:LOCALAPPDATA\Microsoft\WindowsApps"

# 5. Download the binary
Write-Host "Downloading from $DownloadUrl..." -ForegroundColor Cyan

try {
    Invoke-WebRequest -Uri $DownloadUrl -OutFile $TempPath -UseBasicParsing
}
catch {
    Write-Error "Failed to download file. The version '$Version' or asset '$FileName' might not exist."
    exit 1
}

# 6. Install
Write-Host "Installing Kalidous to $InstallDir..."

try {
    # Ensure the directory exists
    if (-not (Test-Path $InstallDir)) {
        New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
    }

    # Move the file and rename to kalidous.exe
    Move-Item -Path $TempPath -Destination "$InstallDir\kalidous.exe" -Force

    Write-Host "--------------------------------------------------"
    Write-Host "Installation Complete!" -ForegroundColor Green
    Write-Host "Run 'kalidous --help' in a NEW terminal window to get started."
    Write-Host "--------------------------------------------------"
}
catch {
    Write-Error "Failed to move file to $InstallDir."
    Write-Host "You might need to run PowerShell as Administrator, or manually move the file from:"
    Write-Host "$TempPath"
    Write-Host "to a folder in your PATH."
    exit 1
}