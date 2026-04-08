module;
#include <utility>

#include <wobjectdefs.h>
#include <QDialog>
#include <QWidget>
#include <QString>
#include <QStringList>
#include <QPixmap>
#include <QVector>
export module Artifact.Widgets.LooksPresetBrowser;

export namespace Artifact {

// --- Data model -----------------------------------------------------------

struct LooksPreset {
    QString id;
    QString name;
    QString category;   // "Film" | "TV & Video" | "Photo" | "Creative"
    QString pack;       // "Looks Classic" | "Cinematic" | "Studio Looks" | "カスタム"
    QPixmap thumbnail;
    bool    isFavorite{false};
    int     score{0};         // displayed percentage (0-100)
    float   intensity{1.0f};
    float   exposure{0.0f};
    float   contrast{0.0f};
    float   temperature{0.0f};
};

// --- Dialog ---------------------------------------------------------------

class LooksPresetBrowserDialog : public QDialog {
    W_OBJECT(LooksPresetBrowserDialog)

    class Impl;
    Impl* impl_;

public:
    explicit LooksPresetBrowserDialog(QWidget* parent = nullptr);
    ~LooksPresetBrowserDialog() override;

    // Sets the "before" image shown in the preview panel.
    void setPreviewImage(const QPixmap& before);

    // Returns the id of the currently applied preset, or empty string.
    QString appliedPresetId() const;

signals:
    void presetApplied(const QString& presetId)
        W_SIGNAL(presetApplied, presetId);

    void presetFavorited(const QString& presetId, bool fav)
        W_SIGNAL(presetFavorited, presetId, fav);
};

} // namespace Artifact
