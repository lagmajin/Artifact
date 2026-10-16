# Dock Foundation

`Dock::Foundation` is a reusable Qt Widgets docking surface and portable layout contract.

## Requirements

- CMake 3.28 or later
- C++23 module support
- Qt 6.6 or later (`Core`, `Gui`, `Widgets`)

## Build from source

Add this directory to a Qt application with `add_subdirectory`, then link `Dock::Foundation`:

```cmake
add_subdirectory(path/to/Artifact/DockFoundation)
target_link_libraries(MyApp PRIVATE Dock::Foundation)
```

Import `Dock.Manager` and `Dock.NativeDockSurface` from C++ modules. The old `Artifact.DockManager` and `Artifact.NativeDockSurface` module names remain as re-export wrappers for migration. The surface takes a `QPalette` from the host application so it does not depend on a particular theme system. Call `setDockPalette()` when the host theme changes to update existing and future tab surfaces:

```cpp
import Dock.Manager;
import Dock.NativeDockSurface;

auto *surface = new Dock::NativeDockSurface(qApp->palette());
surface->addDockWidget("project", "Project", projectWidget,
                       Dock::DockArea::Left);
```

## Install and consume

Set `CMAKE_INSTALL_PREFIX` when configuring, install the project, then locate it with:

```cmake
find_package(DockFoundation 1 CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE Dock::Foundation)
```

The package exports the C++ module file set and depends only on Qt. The source currently lives in the `Artifact` repository; it has not been moved to a separately versioned repository.
