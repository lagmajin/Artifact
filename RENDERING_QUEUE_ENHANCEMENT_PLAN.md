# レンダリングキュー管理強化計画

## プロジェクト現状
- 基本的なレンダリングキューサービスの枠組みが存在（`ArtifactRenderQueueService`）
- UIウィジェットの基礎が実装（`RenderQueueManagerWidget`, `RenderQueueJobPanel`）
- メニュー項目が作成されているが機能は未実装（`addToRenderQueueAction`）
- 全体的に機能が空の状態で、実際のレンダリングキュー管理が未実装

## 実装方針

### 1. レンダリングキューデータモデルの設計

#### 1.1 レンダリングジョブクラス
```cpp
class ArtifactRenderJob {
public:
    enum class Status {
        Pending,       // 待機中
        Rendering,     // レンダリング中
        Completed,     // 完了
        Failed,        // 失敗
        Canceled       // キャンセル
    };

    QString compositionName;      // コンポジション名
    QString outputPath;           // 出力パス
    QString outputFormat;         // 出力形式 (MP4, PNG sequence等)
    QString codec;                // コーデック
    int resolutionWidth;          // 解像度幅
    int resolutionHeight;         // 解像度高さ
    double frameRate;             // フレームレート
    int bitrate;                  // ビットレート (kbps)
    int startFrame;               // 開始フレーム
    int endFrame;                 // 終了フレーム
    Status status;                // ステータス
    int progress;                 // 進捗率 (0-100)
    QString errorMessage;         // エラーメッセージ
};
```

#### 1.2 レンダリングキューマネージャ
```cpp
class ArtifactRenderQueueManager {
public:
    void addJob(const ArtifactRenderJob& job);
    void removeJob(int index);
    void removeAllJobs();
    void moveJob(int fromIndex, int toIndex);
    void startRendering(int index);
    void pauseRendering(int index);
    void cancelRendering(int index);
    void startAllJobs();
    void pauseAllJobs();
    void cancelAllJobs();
    
    ArtifactRenderJob getJob(int index) const;
    int jobCount() const;
    int getTotalProgress() const;
    
    // シグナル
    void jobAdded(int index);
    void jobRemoved(int index);
    void jobUpdated(int index);
    void jobStatusChanged(int index, ArtifactRenderJob::Status status);
    void jobProgressChanged(int index, int progress);
    void allJobsCompleted();
};
```

### 2. レンダリングキューサービスの実装

#### 2.1 ArtifactRenderQueueServiceの強化
```cpp
class ArtifactRenderQueueService::Impl {
public:
    QList<ArtifactRenderJob> jobs;
    ArtifactRenderQueueManager queueManager;
    QThread* renderingThread;
    bool isRendering;
    
    void handleJobAdded(int index);
    void handleJobRemoved(int index);
    void handleJobUpdated(int index);
    void handleJobStatusChanged(int index, ArtifactRenderJob::Status status);
    void handleJobProgressChanged(int index, int progress);
};

// 主要なメソッドの実装
void ArtifactRenderQueueService::addRenderQueue() {
    // 現在のコンポジションをレンダリングキューに追加
    auto& projectManager = ArtifactProjectManager::getInstance();
    auto currentComp = projectManager.currentComposition();
    
    if (currentComp) {
        ArtifactRenderJob job;
        job.compositionName = currentComp->getName().toQString();
        job.status = ArtifactRenderJob::Status::Pending;
        job.outputPath = QDir::homePath() + "/Desktop/" + job.compositionName + ".mp4";
        job.outputFormat = "MP4";
        job.codec = "H.264";
        job.resolutionWidth = currentComp->getSettings().width;
        job.resolutionHeight = currentComp->getSettings().height;
        job.frameRate = currentComp->getSettings().frameRate;
        job.bitrate = 8000; // 8Mbps
        job.startFrame = 0;
        job.endFrame = currentComp->getDurationFrames();
        
        impl_->queueManager.addJob(job);
    }
}

void ArtifactRenderQueueService::removeRenderQueue() {
    // 選択されたジョブを削除
}

void ArtifactRenderQueueService::removeAllRenderQueues() {
    impl_->queueManager.removeAllJobs();
}
```

### 3. UIウィジェットの実装

#### 3.1 RenderQueueManagerWidgetの強化
```cpp
class RenderQueueManagerWidget::Impl {
public:
    QListWidget* jobListWidget;
    QPushButton* addButton;
    QPushButton* removeButton;
    QPushButton* clearButton;
    QPushButton* startButton;
    QPushButton* pauseButton;
    QPushButton* cancelButton;
    QProgressBar* totalProgressBar;
    QLabel* statusLabel;
    
    void updateJobList();
    void handleJobSelected();
    void handleJobAdded(int index);
    void handleJobRemoved(int index);
    void handleJobUpdated(int index);
    void handleJobStatusChanged(int index, ArtifactRenderJob::Status status);
    void handleJobProgressChanged(int index, int progress);
};

RenderQueueManagerWidget::RenderQueueManagerWidget(QWidget* parent) : QWidget(parent), impl_(new Impl()) {
    // レイアウトの作成
    auto mainLayout = new QVBoxLayout(this);
    
    // ジョブリスト
    impl_->jobListWidget = new QListWidget(this);
    mainLayout->addWidget(impl_->jobListWidget);
    
    // コントロールボタン
    auto controlLayout = new QHBoxLayout();
    impl_->addButton = new QPushButton("Add", this);
    impl_->removeButton = new QPushButton("Remove", this);
    impl_->clearButton = new QPushButton("Clear All", this);
    impl_->startButton = new QPushButton("Start", this);
    impl_->pauseButton = new QPushButton("Pause", this);
    impl_->cancelButton = new QPushButton("Cancel", this);
    
    controlLayout->addWidget(impl_->addButton);
    controlLayout->addWidget(impl_->removeButton);
    controlLayout->addWidget(impl_->clearButton);
    controlLayout->addWidget(impl_->startButton);
    controlLayout->addWidget(impl_->pauseButton);
    controlLayout->addWidget(impl_->cancelButton);
    
    mainLayout->addLayout(controlLayout);
    
    // 進捗表示
    impl_->totalProgressBar = new QProgressBar(this);
    mainLayout->addWidget(impl_->totalProgressBar);
    
    impl_->statusLabel = new QLabel("Ready", this);
    mainLayout->addWidget(impl_->statusLabel);
    
    // シグナル接続
    connect(impl_->addButton, &QPushButton::clicked, this, []() {
        auto& renderQueueService = ArtifactRenderQueueService::getInstance();
        renderQueueService.addRenderQueue();
    });
    
    connect(impl_->removeButton, &QPushButton::clicked, this, []() {
        auto& renderQueueService = ArtifactRenderQueueService::getInstance();
        renderQueueService.removeRenderQueue();
    });
    
    connect(impl_->clearButton, &QPushButton::clicked, this, []() {
        auto& renderQueueService = ArtifactRenderQueueService::getInstance();
        renderQueueService.removeAllRenderQueues();
    });
    
    connect(impl_->startButton, &QPushButton::clicked, this, []() {
        auto& renderQueueService = ArtifactRenderQueueService::getInstance();
        renderQueueService.startAllJobs();
    });
    
    connect(impl_->pauseButton, &QPushButton::clicked, this, []() {
        auto& renderQueueService = ArtifactRenderQueueService::getInstance();
        renderQueueService.pauseAllJobs();
    });
    
    connect(impl_->cancelButton, &QPushButton::clicked, this, []() {
        auto& renderQueueService = ArtifactRenderQueueService::getInstance();
        renderQueueService.cancelAllJobs();
    });
}
```

#### 3.2 RenderQueueJobWidgetの実装
```cpp
class RenderQueueJobWidget::Impl {
public:
    QToolButton* toggleButton;
    EditableLabel* compositionNameLabel;
    QToolButton* renderingStartButton;
    EditableLabel* directoryLabel;
    QProgressBar* jobProgressBar;
    QLabel* statusLabel;
    QWidget* headerDetailPanel;
    QWidget* outputDetailPanel;
    
    void updateJobStatus(ArtifactRenderJob::Status status);
    void updateJobProgress(int progress);
};

RenderQueueJobWidget::RenderQueueJobWidget(QWidget* parent) : QWidget(parent), impl_(new Impl()) {
    // 既存の実装に進捗バーとステータス表示を追加
    impl_->jobProgressBar = new QProgressBar(this);
    impl_->jobProgressBar->setRange(0, 100);
    impl_->jobProgressBar->setValue(0);
    
    impl_->statusLabel = new QLabel("Pending", this);
    
    // レイアウトに追加
    // ...
}
```

### 4. レンダリングエンジンの連携

#### 4.1 バックグラウンドレンダリング
```cpp
class ArtifactRenderWorker : public QObject {
    Q_OBJECT
public:
    ArtifactRenderWorker(ArtifactRenderJob job);
    
public slots:
    void process();
    
signals:
    void finished();
    void progress(int value);
    void statusChanged(ArtifactRenderJob::Status status);
    void error(QString errorMessage);
    
private:
    ArtifactRenderJob job;
    bool isPaused;
    bool isCanceled;
};

void ArtifactRenderWorker::process() {
    try {
        emit statusChanged(ArtifactRenderJob::Status::Rendering);
        
        for (int frame = job.startFrame; frame <= job.endFrame && !isCanceled; frame++) {
            if (isPaused) {
                emit statusChanged(ArtifactRenderJob::Status::Pending);
                // 一時停止処理
            }
            
            // フレームレンダリング処理
            renderFrame(frame);
            
            int progress = static_cast<int>((static_cast<double>(frame - job.startFrame) / 
                                           (job.endFrame - job.startFrame)) * 100);
            emit progress(progress);
        }
        
        if (isCanceled) {
            emit statusChanged(ArtifactRenderJob::Status::Canceled);
        } else {
            emit statusChanged(ArtifactRenderJob::Status::Completed);
        }
    } catch (const std::exception& e) {
        emit error(QString::fromStdString(e.what()));
        emit statusChanged(ArtifactRenderJob::Status::Failed);
    }
    
    emit finished();
}
```

### 5. エクスポート設定の管理

#### 5.1 レンダリング設定ダイアログ
```cpp
class ArtifactRenderOutputSettingDialog : public QDialog {
    Q_OBJECT
public:
    enum class OutputFormat {
        MP4,
        MOV,
        AVI,
        PNGSequence,
        JPEGSequence
    };
    
    explicit ArtifactRenderOutputSettingDialog(QWidget* parent = nullptr);
    
    QString outputPath() const;
    OutputFormat outputFormat() const;
    QString codec() const;
    int resolutionWidth() const;
    int resolutionHeight() const;
    double frameRate() const;
    int bitrate() const;
    int startFrame() const;
    int endFrame() const;
    
private:
    void setupUI();
    
    QLineEdit* outputPathEdit;
    QComboBox* outputFormatCombo;
    QComboBox* codecCombo;
    QSpinBox* resolutionWidthSpin;
    QSpinBox* resolutionHeightSpin;
    QDoubleSpinBox* frameRateSpin;
    QSpinBox* bitrateSpin;
    QSpinBox* startFrameSpin;
    QSpinBox* endFrameSpin;
};
```

### 6. メニュー機能の実装

#### 6.1 ArtifactCompositionMenuの強化
```cpp
void ArtifactCompositionMenu::Impl::handleAddRenderQueueRequest() {
    // レンダリングキューに現在のコンポジションを追加
    auto& renderQueueService = ArtifactRenderQueueService::getInstance();
    renderQueueService.addRenderQueue();
}

void ArtifactCompositionMenu::Impl::handleSaveAsImageRequest() {
    // 現在のフレームを画像として保存
    auto& previewController = ArtifactPreviewController::getInstance();
    QImage currentFrame = previewController.getCurrentFrameImage();
    
    if (!currentFrame.isNull()) {
        QString fileName = QFileDialog::getSaveFileName(mainWindow_, "Save Frame As",
                                                      QDir::homePath() + "/Desktop/frame.png",
                                                      "Images (*.png *.jpg *.jpeg)");
        
        if (!fileName.isEmpty()) {
            currentFrame.save(fileName);
        }
    }
}
```

### 7. エラー処理とロギング

#### 7.1 レンダリングログの管理
```cpp
class ArtifactRenderLogger {
public:
    static ArtifactRenderLogger& getInstance();
    
    void logJobStarted(const QString& jobName);
    void logJobCompleted(const QString& jobName, int duration);
    void logJobFailed(const QString& jobName, const QString& errorMessage);
    void logJobCanceled(const QString& jobName);
    void logFrameRendered(const QString& jobName, int frame, double time);
    
    QString getLogContent() const;
    void saveLogToFile(const QString& filePath) const;
    
private:
    QList<QString> logs;
};
```

## 実装スケジュール

### Phase 1: 基礎機能の実装 (1-2週間)
1. レンダリングキューデータモデルの実装
2. ArtifactRenderQueueServiceの機能完成
3. RenderQueueManagerWidgetのUI実装
4. メニュー機能の実装

### Phase 2: レンダリングエンジン連携 (1-2週間)
1. バックグラウンドレンダリングWorkerの実装
2. レンダリング進捗の表示と更新
3. レンダリングステータスの管理

### Phase 3: エクスポート機能の強化 (1週間)
1. レンダリング設定ダイアログの実装
2. 出力形式とコーデックの設定
3. ファイル保存ダイアログの実装

### Phase 4: エラー処理とロギング (1週間)
1. 例外処理の実装
2. レンダリングログの管理
3. エラーメッセージの表示

### Phase 5: 最適化とテスト (1週間)
1. レンダリングパフォーマンスの最適化
2. メモリ管理の改善
3. ユニットテストの作成

## 技術的な考慮点

### スレッド管理
- バックグラウンドレンダリングはQThreadを使用
- メインスレッドとの安全な通信のために信号とスロットを使用
- レンダリング進捗の更新はQMetaObject::invokeMethodを使用

### データの永続化
- レンダリングキューの状態をJSONファイルとして保存
- アプリケーション起動時にキュー状態を復元
- レンダリング設定の保存と管理

### 性能最適化
- マルチスレッドレンダリングの実装
- GPU資源の効率的な管理
- レンダリングキャッシュシステムの導入

## ユーザーインターフェースの改善

### 視覚的な改善
- レンダリング進捗のグラフィカル表示
- ジョブステータスの色分け表示
- レンダリング速度と残り時間の推定

### 操作性の改善
- ジョブのドラッグアンドドロップによる並べ替え
- バッチ処理の実装
- キーボードショートカットの追加

## リリース計画

### 最初のリリース
- 基本的なレンダリングキュー管理機能
- MP4出力のサポート
- バックグラウンドレンダリング
- 進捗表示とキャンセル機能

### 次のバージョン
- 追加の出力形式のサポート
- コーデックとビットレートの調整
- レンダリング設定のプリセット
- ネットワークレンダリングの基礎機能

## 結論

レンダリングキュー管理の強化は、Artifactのプロフェッショナルレベルの機能として非常に重要です。実装計画に基づいて段階的に開発を進めることで、ユーザーが複数のコンポジションを効率的にレンダリングできるようになり、作業効率が大幅に向上します。