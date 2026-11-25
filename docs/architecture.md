# How Nagus is built

## The original first

Nagus aims to play like the 1999 game with its latest official patch
(1.0.2): the same rules, screens, computer players and numbers. Anything new,
such as gamepad controls or scaling to modern screens, is added on top
without changing how the game plays or looks. When the original's behaviour
is unclear, we observe the original program and write down what it does (see
the clean-room rules in `CONTRIBUTING.md`); where we could only make a best
guess so far, `docs/rules` says so.

## Parts

- **Library** (`game_files.hpp`). On first start the player's own files are
  copied into Nagus's library: the game's files from the CD image, zip or
  folder into `library/base`, and the patch's files into `library/base/patch`.
  See [formats/library.md](formats/library.md).
- **Resources** (`data/resources.hpp`). Opens the main resource archive and
  lets the patch's loose files take the place of the archive's.
- **Readers** (`data/*.hpp`). One per file format: pictures, screen layouts,
  fonts, texts, pointers. Each checks every size and offset (`SECURITY.md`).
- **Screens** (`ui/*.hpp`). A screen draws itself into an 800×600 `Canvas`
  (the original's screen size) and reacts to pointer presses and to
  `Command`s, which the app makes from keys and gamepad buttons. A `Panel`
  draws a layout's background and buttons and works out which button the
  pointer is on, and which button the d-pad should jump to.
- **Shell** (`ui/shell.hpp`). Holds the screens, moves between them and
  draws the pointer.
- **App** (`src/app`). The only part that uses SDL: window, scaling,
  input, the import screen and the background import thread.

The core never touches the screen or the operating system, so everything up
to the finished picture can be tested; `ngs_snapshot` draws the screens to
files for checking by eye.
