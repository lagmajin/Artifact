module;
#include <QObject>
#include <wobjectdefs.h>
#include "DockManager.h"

export module Widgets.Dock.StyleManager;

import std;

export namespace Artifact {

class DockStyleManager : public QObject {
    W_OBJECT(DockStyleManager)
private:
    class Impl;
    Impl* impl_;
    
public:
    explicit DockStyleManager(ads::CDockManager* dockManager, QObject* parent = nullptr);
    ~DockStyleManager();
    
    void setGlowEnabled(bool enabled);
    void setGlowColor(const QColor& color);
    void setGlowWidth(int width);
    void setGlowIntensity(float intensity);
    void applyStyle();
};

}
