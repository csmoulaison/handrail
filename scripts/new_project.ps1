# Sets up a new handrail project in the folder that contains this handrail
# clone. Run it through handrail\new_project.bat:
#   mkdir mygame; cd mygame
#   git clone https://github.com/csmoulaison/handrail
#   handrail\new_project.bat
#
# It makes the project folder a git repo, adopts the clone as its handrail
# submodule, and copies in handrail\template\ with the project's name filled in.

function Fail {
    Write-Host $args[0] -ForegroundColor Red
    exit 1
}

# Locate: this script is handrail\scripts\new_project.ps1
$HANDRAIL_DIR = (Resolve-Path "$PSScriptRoot\..").Path
$PROJECT_DIR  = Split-Path $HANDRAIL_DIR -Parent
$TEMPLATE_DIR = Join-Path $HANDRAIL_DIR "template"

# Check the setup
if(!(Get-Command git -ErrorAction SilentlyContinue)) { Fail "git isn't on the PATH." }
if(!(Test-Path "$HANDRAIL_DIR\.git")) { Fail "$HANDRAIL_DIR isn't a git clone. Clone handrail with git, then run this again." }
$HANDRAIL_URL = git -C $HANDRAIL_DIR remote get-url origin 2> $null
if(!$HANDRAIL_URL) { Fail "handrail has no origin remote to record as the submodule's URL." }
$HANDRAIL_BRANCH = git -C $HANDRAIL_DIR branch --show-current
if(!$HANDRAIL_BRANCH) { Fail "handrail is on a detached HEAD. Check out a branch, then run this again." }
if((Split-Path $HANDRAIL_DIR -Leaf) -ne "handrail") { Fail "The clone must be in a folder named handrail, not $(Split-Path $HANDRAIL_DIR -Leaf)." }
if(Test-Path "$PROJECT_DIR\.git") { Fail "$PROJECT_DIR is already a git repo. Clone handrail into a new, empty folder." }
$TEMPLATE_FILES = Get-ChildItem $TEMPLATE_DIR -Recurse -File -Force | ForEach-Object { $_.FullName.Substring($TEMPLATE_DIR.Length + 1) }
foreach($path in $TEMPLATE_FILES) {
    if(Test-Path (Join-Path $PROJECT_DIR $path)) { Fail "$(Join-Path $PROJECT_DIR $path) already exists, and the template would overwrite it." }
}

# Confirm, warning about anything already in the project folder
Write-Host "Set up a new handrail project in:"
Write-Host "  $PROJECT_DIR"
$OTHERS = Get-ChildItem $PROJECT_DIR -Force | Where-Object { $_.Name -ne "handrail" }
if($OTHERS) {
    Write-Host ""
    Write-Host "Warning: the folder isn't empty. Besides handrail, it holds:" -ForegroundColor Yellow
    $OTHERS | ForEach-Object { Write-Host "  $($_.Name)" }
}
Write-Host ""
$ANSWER = Read-Host "Continue? [y/N]"
if($ANSWER -ne "y" -and $ANSWER -ne "Y") {
    Write-Host "Nothing was changed."
    exit 0
}

# Name: used for the executable, the window title, and the README
$DEFAULT_NAME = (Split-Path $PROJECT_DIR -Leaf) -replace "[^A-Za-z0-9_]", "_" -replace "^([0-9])", "_`$1"
$NAME = Read-Host "Project name [$DEFAULT_NAME]"
if(!$NAME) { $NAME = $DEFAULT_NAME }
if($NAME -cnotmatch "^[A-Za-z_][A-Za-z0-9_]*$") { Fail "`"$NAME`" isn't a valid name: use letters, digits, and underscores, not starting with a digit." }

# Git: a new repo, with the existing clone adopted in place as its submodule
git -C $PROJECT_DIR init -q
if(!$?) { Fail "git init failed." }
git -C $PROJECT_DIR submodule add -b $HANDRAIL_BRANCH $HANDRAIL_URL handrail
if(!$?) { Fail "Adding handrail as a submodule failed." }

# Template, with the name filled in. WriteAllText keeps the files' LF line
# endings and writes no byte order mark, which would break build.bat.
Copy-Item "$TEMPLATE_DIR\*" $PROJECT_DIR -Recurse -Force
foreach($path in $TEMPLATE_FILES) {
    if($path -like "*.ttf") { continue }
    $file = Join-Path $PROJECT_DIR $path
    $text = [System.IO.File]::ReadAllText($file)
    [System.IO.File]::WriteAllText($file, $text.Replace("__NAME__", $NAME))
}

# Stage everything, leaving the first commit to the user
git -C $PROJECT_DIR add -A
if(!$?) { Fail "Staging the project failed." }

Write-Host ""
Write-Host "Created $NAME in $PROJECT_DIR, with handrail on branch $HANDRAIL_BRANCH."
Write-Host "Everything is staged; commit when you're ready. To build and run:"
Write-Host "  cd $PROJECT_DIR"
Write-Host "  build.bat"
Write-Host "  cd bin"
Write-Host "  .\$NAME.exe"
