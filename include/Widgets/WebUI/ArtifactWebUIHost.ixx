module;
#include <QWidget>
#include <QVBoxLayout>
#include <wobjectdefs.h>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Artifact.Widgets.WebUIHost;





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
