module;
#include <utility>
#include <wobjectimpl.h>
#include <QVBoxLayout>
#include <QDebug>
#include <QUrl>

// QWebEngine includes — require `QT += webenginewidgets webchannel` in build
#ifdef USE_WEBENGINE
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebChannel>
#endif

module Artifact.Widgets.WebUIHost;

import Artifact.Widgets.WebBridge;

namespace Artifact {

    W_OBJECT_IMPL(ArtifactWebUIHost)

    class ArtifactWebUIHost::Impl {
    public:
        QWebEngineView* webView = nullptr;
        QWebChannel* channel = nullptr;
        ArtifactWebBridge* bridge = nullptr;
    };

    ArtifactWebUIHost::ArtifactWebUIHost(QWidget* parent)
        : QWidget(parent), impl_(new Impl())
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);

        // ---- WebEngineView の作成 ----
        impl_->webView = new QWebEngineView(this);
        layout->addWidget(impl_->webView);

        // ---- WebChannel (C++ <-> JS ブリッジ) の作成 ----
        impl_->channel = new QWebChannel(this);
        impl_->bridge = new ArtifactWebBridge(this);

        // "bridge" という名前で JS 側からアクセス可能にする
        // JS側: new QWebChannel(qt.webChannelTransport, function(channel) {
        //            var bridge = channel.objects.bridge;
        //            bridge.selectLayer("...");
        //        });
        impl_->channel->registerObject(QStringLiteral("bridge"), impl_->bridge);
        impl_->webView->page()->setWebChannel(impl_->channel);

        qDebug() << "[WebUIHost] Initialized with QWebChannel bridge";
    }

    ArtifactWebUIHost::~ArtifactWebUIHost()
    {
        delete impl_;
    }

    void ArtifactWebUIHost::loadUrl(const QString& url)
    {
        qDebug() << "[WebUIHost] Loading URL:" << url;
        impl_->webView->load(QUrl(url));
    }

    void ArtifactWebUIHost::connectToDevServer(int port)
    {
        QString devUrl = QString("http://localhost:%1").arg(port);
        qDebug() << "[WebUIHost] Connecting to dev server:" << devUrl;
        loadUrl(devUrl);
    }

    void ArtifactWebUIHost::loadProductionUI()
    {
        // qrc にバンドルされたプロダクションビルドのHTMLを読み込む
        QString prodUrl = QStringLiteral("qrc:/webui/index.html");
        qDebug() << "[WebUIHost] Loading production UI:" << prodUrl;
        loadUrl(prodUrl);
    }

}
