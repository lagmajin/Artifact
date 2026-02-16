module;
#include <QWidget>
#include <wobjectdefs.h>
export module Widgets.AIChatWidget;

import std;
import Utils.String.UniString;
import AI.Client;

export namespace Artifact {

class AIChatWidget : public QWidget {
 W_OBJECT(AIChatWidget)
private:
    class Impl;
    Impl* impl_;
public:
    AIChatWidget(QWidget* parent = nullptr);
    ~AIChatWidget();

    void sendUserMessage(const UniString& msg);

    void setProvider(const UniString& provider);
};

}
