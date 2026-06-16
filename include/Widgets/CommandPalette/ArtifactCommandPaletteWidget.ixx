module;
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QVariantMap>
#include <vector>
export module Command.Palette;

class QDialog;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QKeyEvent;
class QWidget;

namespace ArtifactCore { class Action; }

export namespace Artifact
{

/**
 * @brief Minimum command-palette UI (experimental, feature-flagged).
 *
 * Pops a small modal dialog with a filter line-edit and a list of actions
 * pulled from ArtifactCore::ActionManager. Pressing Enter executes the
 * highlighted action via ActionManager::executeAction(). This widget is a
 * thin shell on top of the existing ActionManager; it does not introduce
 * new signals/slots and does not own any action registration state.
 *
 * Scope is intentionally tiny: registration of real actions is a TODO and
 * is expected to be wired in by the host (see boot_dummy_command_palette_actions()).
 */
class ArtifactCommandPaletteWidget : public QDialog
{
public:
    explicit ArtifactCommandPaletteWidget(QWidget* parent = nullptr);
    ~ArtifactCommandPaletteWidget() override;

    void setMainWindow(QWidget* mainWindow);
    void setFilterText(const QString& text);
    QString filterText() const;

    void refreshActionList();

    static void bootDummyCommandPaletteActions();

    // ---- MRU (Most Recently Used) ---------------------------------------
    // The palette keeps an in-memory MRU list of action ids. The list is
    // boosted during scoring (recently used items float to the top) and is
    // mutated whenever an action is executed. Persistence to / from the
    // project is exposed as JSON helpers so that a future cycle can wire
    // them through ArtifactProject::setExtensionData() / extensionData()
    // without adding any signal/slot connection here.
    static void recordMruAction(const QString& actionId);
    static int mruScoreFor(const QString& actionId);
    static QJsonObject mruAsJson();
    static void loadMruFromJson(const QJsonObject& data);
    static const QStringList &mruList();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void rebuildFromActions(const std::vector<ArtifactCore::Action*>& actions);
    void executeHighlighted();

    QLineEdit* filterEdit_ = nullptr;
    QListWidget* actionList_ = nullptr;
    QWidget* mainWindow_ = nullptr;
};

} // namespace Artifact
