module;
#include <QWidget>
#include <QVBoxLayout>
#include <wobjectdefs.h>

export module Artifact.Widgets.WebUIHost;

import std;

// Forward declarations for Qt WebEngine types
// These require `QT += webenginewidgets webchannel` in the .pro / CMakeLists
class QWebEngineView;
class QWebChannel;

export namespace Artifact {

    // ===================================================================
    // ArtifactWebUIHost
    // -------------------------------------------------------------------
    // QWidget内にChromiumベースのWebビュー（QWebEngineView）を埋め込み、
    // QWebChannel経由でC++バックエンドとWeb UIを双方向通信させるホスト。
    //
    // 使い方:
    //   auto* webHost = new ArtifactWebUIHost();
    //   webHost->loadUrl(QUrl("qrc:/webui/index.html"));
    //   // or
    //   webHost->loadUrl(QUrl("http://localhost:3000"));
    //   someLayout->addWidget(webHost);
    // ===================================================================
    class ArtifactWebUIHost : public QWidget {
        W_OBJECT(ArtifactWebUIHost)

    private:
        class Impl;
        Impl* impl_;

    public:
        explicit ArtifactWebUIHost(QWidget* parent = nullptr);
        ~ArtifactWebUIHost();

        // URLをロード（ローカルHTMLファイル、qrcリソース、またはdev serverのURL）
        void loadUrl(const QString& url);

        // 開発モード: Vite/Webpack dev server に直接接続
        void connectToDevServer(int port = 5173);

        // プロダクションモード: qrc内のビルド済みアセットをロード
        void loadProductionUI();

        // WebBridge への直接アクセス（テスト/デバッグ用）
        // ArtifactWebBridge* bridge() const;
    };

}
