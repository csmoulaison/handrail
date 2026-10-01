# Sets up a new handrail project in the folder that contains this handrail
# clone. Run it through handrail/new_project.bat:
#   mkdir mygame && cd mygame
#   git clone https://github.com/csmoulaison/handrail
#   sh handrail/new_project.bat
#
# It makes the project folder a git repo, adopts the clone as its handrail
# submodule, and copies in handrail/template/ with the project's name filled in.

fail() {
    printf "%s\n" "$1" >&2
    exit 1
}

# Locate: this script is handrail/scripts/new_project.sh
HANDRAIL_DIR=$(cd "$(dirname "$0")/.." && pwd)
PROJECT_DIR=$(dirname "$HANDRAIL_DIR")
TEMPLATE_DIR="$HANDRAIL_DIR/template"

# Check the setup
command -v git > /dev/null || fail "git isn't on the PATH."
[ -d "$HANDRAIL_DIR/.git" ] || fail "$HANDRAIL_DIR isn't a git clone. Clone handrail with git, then run this again."
HANDRAIL_URL=$(git -C "$HANDRAIL_DIR" remote get-url origin 2> /dev/null) || fail "handrail has no origin remote to record as the submodule's URL."
HANDRAIL_BRANCH=$(git -C "$HANDRAIL_DIR" branch --show-current)
[ -n "$HANDRAIL_BRANCH" ] || fail "handrail is on a detached HEAD. Check out a branch, then run this again."
[ "$(basename "$HANDRAIL_DIR")" = "handrail" ] || fail "The clone must be in a folder named handrail, not $(basename "$HANDRAIL_DIR")."
[ -e "$PROJECT_DIR/.git" ] && fail "$PROJECT_DIR is already a git repo. Clone handrail into a new, empty folder."
for path in $(cd "$TEMPLATE_DIR" && find . -type f); do
    [ -e "$PROJECT_DIR/$path" ] && fail "$PROJECT_DIR/${path#./} already exists, and the template would overwrite it."
done

# Confirm, warning about anything already in the project folder
printf "Set up a new handrail project in:\n  %s\n" "$PROJECT_DIR"
OTHERS=$(ls -A "$PROJECT_DIR" | grep -vx "handrail")
if [ -n "$OTHERS" ]; then
    printf "\nWarning: the folder isn't empty. Besides handrail, it holds:\n"
    printf "%s\n" "$OTHERS" | sed 's/^/  /'
fi
printf "\nContinue? [y/N] "
read -r ANSWER
case "$ANSWER" in
    y|Y) ;;
    *) printf "Nothing was changed.\n"; exit 0 ;;
esac

# Name: used for the executable, the window title, and the README
DEFAULT_NAME=$(basename "$PROJECT_DIR" | sed 's/[^A-Za-z0-9_]/_/g; s/^\([0-9]\)/_\1/')
printf "Project name [%s]: " "$DEFAULT_NAME"
read -r NAME
[ -n "$NAME" ] || NAME=$DEFAULT_NAME
printf "%s" "$NAME" | grep -qx "[A-Za-z_][A-Za-z0-9_]*" || fail "\"$NAME\" isn't a valid name: use letters, digits, and underscores, not starting with a digit."

# Git: a new repo, with the existing clone adopted in place as its submodule
git -C "$PROJECT_DIR" init -q || fail "git init failed."
git -C "$PROJECT_DIR" submodule add -b "$HANDRAIL_BRANCH" "$HANDRAIL_URL" handrail || fail "Adding handrail as a submodule failed."

# Template, with the name filled in
cp -R "$TEMPLATE_DIR/." "$PROJECT_DIR/" || fail "Copying the template failed."
for path in $(cd "$TEMPLATE_DIR" && find . -type f ! -name "*.ttf"); do
    sed -i "s/__NAME__/$NAME/g" "$PROJECT_DIR/$path"
done

# Stage everything, leaving the first commit to the user
git -C "$PROJECT_DIR" add -A || fail "Staging the project failed."

printf "\nCreated %s in %s, with handrail on branch %s.\n" "$NAME" "$PROJECT_DIR" "$HANDRAIL_BRANCH"
printf "Everything is staged; commit when you're ready. To build and run:\n"
printf "  cd %s\n" "$PROJECT_DIR"
printf "  sh build.bat\n"
printf "  cd bin && ./%s\n" "$NAME"
