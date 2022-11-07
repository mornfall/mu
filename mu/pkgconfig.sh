set -e

# args: value, expression
add_config()
{
    if ! [ $# -eq 2 ]; then >&2 echo pkg-config script error; exit 1; fi
    _tmp="$($2)"
    echo "add $1 $_tmp"
}

{
    add_config cxxflags.mu "pkg-config --cflags poppler-glib icu-uc"
    add_config ldflags.svgtex "pkg-config --libs poppler-glib"
    add_config ldflags.mu "pkg-config --libs icu-uc"
} > $1
