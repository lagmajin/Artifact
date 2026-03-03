module;

// Minimal module interface for the application entry point.
// This file provides the `Artifact.AppMain` module so the
// implementation in `src/AppMain.cppm` can compile and link.

export module Artifact.AppMain;

import std;

export namespace Artifact {
    // Intentionally empty. The implementation (.cppm) defines
    // the application entry point (main) and test helpers.
}
