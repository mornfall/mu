# μ

μ is a style of writing plain text documents that are reasonably pretty in
themselves and that can be also converted into pretty PDF files for printing
and/or pretty PDF slides for giving presentations or lectures using a beamer.
Goals:

 • pretty plaintext (input files)
 • minimal syntactic ambiguity
 • character-cell diagrams and schemes
 • syntax-colored source code listings
 • tables and display math
 • HTML writer (for web versions of documents)

Non-goals:

 • regex-based parsing
 • ASCII-compatible
 • structural or semantic markup

Future goals:

 • LaTeX writer (for papers)
 • cross-references
 • bibliography support
 • multipart documents

## Current State

There are 2 usable writers: one produces ConTeXt for text and MetaPost for
diagrams, the other produces HTML + MetaPost which can be converted to HTML +
SVG using a bundled helper program (‹svgtex›, a wrapper around context).

On the other hand, there is no documentation, no examples and stuff that I
don't currently use is probably broken. There is no also testsuite. I plan to
address all those problems, eventually. If you want examples or scripts to
actually get a PDF built, I can share those privately: contact me.

## Building

Nothing fancy: you need a C++17 compiler and ICU header files. Run ‹make› to
get a binary.
