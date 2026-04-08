module;
#include <utility>

// qax_instantiate must be defined in the global module fragment so it is
// emitted as a regular external symbol that Qt6AxServerd.lib can resolve.
// Returning nullptr is safe because qAxFactory() has a FallbackQAxFactory fallback.

class QAxFactory; // minimal forward declaration; full header not needed here

QAxFactory *qax_instantiate()
{
    return nullptr;
}

export module ActiveQt.QAxStub;
// This named module is intentionally empty.
// The only purpose of this file is to provide qax_instantiate() above.
