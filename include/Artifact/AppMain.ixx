module;

// Minimal module interface for the application entry point.
// This file provides the `Artifact.AppMain` module so the
// implementation in `src/AppMain.cppm` can compile and link.


export module Artifact.AppMain;




export namespace Artifact {
    int runApplication(int argc, char* argv[]);
}
