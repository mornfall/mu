# μ

μ is a style of writing plain text documents that are pretty in themselves and
that can be also converted into pretty PDF files for printing and/or pretty
PDF slides for giving presentations or lectures using a beamer. Goals:

 • pretty plaintext (input files)
 • minimal syntactic ambiguity
 • character-cell diagrams and schemes
 • syntax-colored source code listings
 • tables and display math
 
Non-goals:

 • regex-based parsing
 • ASCII-compatible
 • structural or semantic markup

Possible future goals:

 • HTML writer (for web)
 • LaTeX writer (for papers)
 • cross-references
 • bibliography support
 • multipart documents

## Current State

At the moment, this is a custom tool for my personal use. There is exactly one
usable writer and it produces ConTeXt for text and MetaPost for diagrams.
There is no documentation, no examples and stuff that I don't currently use is
probably broken. There is no testsuite. I plan to address all those problems,
eventually. If you want examples or {makefile} fragments to actually get a PDF
built, I can share those privately: contact me.

## Building

Nothing fancy: you need a C++17 compiler and ICU header files. Run {make}.
