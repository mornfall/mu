These are the fonts that are used in the default templates shipped with μ. The
xccl font is custom and is generated using potrace and fontforge. The latter
unfortunately has a use-after-free bug that crashes the font merge that we
need to perform; the enclosed fontforge.diff works around the problem (but
leaks memory).
