set -e

{
    echo "add cxxflags.mu $(pkg-config --cflags poppler-glib icu-uc)"
    echo "add ldflags.svgtex $(pkg-config --libs poppler-glib)"
    echo "add ldflags.mu $(pkg-config --libs icu-uc)"
} > $1
