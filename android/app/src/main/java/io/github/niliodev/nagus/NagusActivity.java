// SPDX-License-Identifier: GPL-3.0-or-later
package io.github.niliodev.nagus;

import org.libsdl.app.SDLActivity;

// SDL runs the game: it loads libSDL3.so and libmain.so (Nagus) and calls
// SDL_main. Nothing else is needed on the Java side.
public class NagusActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] {"SDL3", "main"};
    }
}
