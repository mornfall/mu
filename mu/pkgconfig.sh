set -e

{
    printf "add cxxflags.mu "; pkg-config --cflags poppler-glib icu-uc
    printf "add ldflags.svgtex "; pkg-config --libs poppler-glib
    printf "add ldflags.mu "; pkg-config --libs icu-uc
} > $1
