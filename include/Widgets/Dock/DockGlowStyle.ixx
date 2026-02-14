module;
#include <QProxyStyle>
#include <wobjectdefs.h>

export module Widgets.Dock.GlowStyle;

import std;

export namespace Artifact {

class DockGlowStyle : public QProxyStyle {
    W_OBJECT(DockGlowStyle)
private:
    class Impl;
    Impl* impl_;
    
public:
    explicit DockGlowStyle(QStyle* baseStyle = nullptr);
    ~DockGlowStyle();
    
    void setGlowEnabled(bool enabled);
    void setGlowColor(const QColor& color);
    void setGlowWidth(int width);
    void setGlowIntensity(float intensity);
    
    // QStyle overrides
    void drawControl(ControlElement element, const QStyleOption* option,
                     QPainter* painter, const QWidget* widget = nullptr) const override;
    void drawPrimitive(PrimitiveElement element, const QStyleOption* option,
                      QPainter* painter, const QWidget* widget = nullptr) const override;
    void drawComplexControl(ComplexControl control, const QStyleOptionComplex* option,
                           QPainter* painter, const QWidget* widget = nullptr) const override;
    
private:
    void drawDockWidgetGlow(const QStyleOption* option, QPainter* painter,
                           const QWidget* widget) const;
    void drawDockTabGlow(const QStyleOption* option, QPainter* painter,
                        const QWidget* widget) const;
    bool isDockWidgetActive(const QWidget* widget) const;
    bool isDockTabActive(const QWidget* widget) const;
};

}
