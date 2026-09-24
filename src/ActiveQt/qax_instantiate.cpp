// qax_instantiate is a Qt ActiveQt extension point.  Keep this as a regular
// translation unit so Qt6AxServerd.lib can resolve its external symbol.
// Returning nullptr is safe because qAxFactory() provides a fallback factory.

class QAxFactory;

QAxFactory* qax_instantiate()
{
    return nullptr;
}
