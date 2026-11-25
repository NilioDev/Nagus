# gamedata/

Put your own copy of the original game here. Everything in this folder except
this README is ignored by Git and must never be committed.

Any of these works:

- the CD image (`.bin` and `.cue`, or `.iso`), or the `.zip` it came in;
- a copy of the CD's files, or the installed game folder;
- and the official 1.0.2 patch, `botf102e.zip` (or its unpacked files).

When the program starts and has not imported a game yet, it imports this
folder into its own library by itself (from the repository root, or with a
`gamedata` folder next to the executable). Tests that need the original files
skip when they are not here.
