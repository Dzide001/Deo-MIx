#pragma once

#include <qobject.h>

#include <QAbstractItemModel>
#include <QObject>
#include <QQmlEngine>
#include <QQmlListProperty>
#include <QQmlParserStatus>
#include <QQuickItem>
#include <QVariant>
#include <memory>

#include "library/analysis/analysisfeature.h"
#include "library/autodj/autodjfeature.h"
#include "library/browse/browsefeature.h"
#include "library/itunes/itunesfeature.h"
#include "library/libraryfeature.h"
#include "library/recording/recordingfeature.h"
#include "library/sidebarmodel.h"
#include "library/trackset/crate/cratefeature.h"
#include "library/trackset/playlistfeature.h"
#include "library/trackset/setlogfeature.h"
#include "library/treeitem.h"
#include "qmlconfigproxy.h"
#include "qmllibrarytracklistmodel.h"
#include "util/parented_ptr.h"

class LibraryTableModel;
class TreeItemModel;
class AllTrackLibraryFeature final : public LibraryFeature {
    Q_OBJECT
  public:
    AllTrackLibraryFeature(Library* pLibrary,
            UserSettingsPointer pConfig);
    ~AllTrackLibraryFeature() override = default;

    QVariant title() override {
        return tr("All...");
    }
    TreeItemModel* sidebarModel() const override {
        return m_pSidebarModel;
    }

    bool hasTrackTable() override {
        return true;
    }

    LibraryTableModel* trackTableModel() const {
        return m_pLibraryTableModel;
    }

    void searchAndActivate(const QString& query);

  public slots:
    void activate() override;

  private:
    LibraryTableModel* m_pLibraryTableModel;

    parented_ptr<TreeItemModel> m_pSidebarModel;
};

namespace mixxx {
namespace qml {

class QmlLibraryTrackListColumn;

class QmlLibrarySource : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString label MEMBER m_label)
    Q_PROPERTY(QString icon MEMBER m_icon)
    Q_PROPERTY(QQmlListProperty<QmlLibraryTrackListColumn> columns READ columnsQml)
    Q_CLASSINFO("DefaultProperty", "columns")
    QML_NAMED_ELEMENT(LibrarySource)
    QML_UNCREATABLE("Only accessible via its specialization")
  public:
    explicit QmlLibrarySource(QObject* parent = nullptr,
            const QList<QmlLibraryTrackListColumn*>& columns = {});

    QQmlListProperty<QmlLibraryTrackListColumn> columnsQml() {
        return {this, &m_columns};
    }

    const QList<QmlLibraryTrackListColumn*>& columns() const {
        return m_columns;
    }
    virtual LibraryFeature* internal() = 0;
  public slots:
    void slotShowTrackModel(QAbstractItemModel* pModel);

  signals:
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    void requestTrackModel(std::shared_ptr<QmlLibraryTrackListModel> pModel);
#else
    void requestTrackModel(std::shared_ptr<mixxx::qml::QmlLibraryTrackListModel> pModel);
#endif

  protected:
    QString m_label;
    QString m_icon;
    QList<QmlLibraryTrackListColumn*> m_columns;
};

class QmlLibraryAllTrackSource : public QmlLibrarySource {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryAllTrackSource)
  public:
    explicit QmlLibraryAllTrackSource(QObject* parent = nullptr,
            const QList<QmlLibraryTrackListColumn*>& columns = {});

    LibraryFeature* internal() override {
        return m_pLibraryFeature.get();
    }

  private:
    std::unique_ptr<AllTrackLibraryFeature> m_pLibraryFeature;
};

// M7: Playlists/Crates/Browse sidebar sections, following the exact same
// adapter pattern as QmlLibraryAllTrackSource above, but wrapping the real
// PlaylistFeature/CrateFeature/BrowseFeature (rather than a hand-rolled
// LibraryFeature) so the sidebar gets their actual child-item trees
// (individual playlists/crates/folders), not just a flat track list.
class QmlLibraryPlaylistsSource : public QmlLibrarySource {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryPlaylistsSource)
  public:
    explicit QmlLibraryPlaylistsSource(QObject* parent = nullptr,
            const QList<QmlLibraryTrackListColumn*>& columns = {});

    LibraryFeature* internal() override {
        return m_pFeature.get();
    }

  private:
    std::unique_ptr<PlaylistFeature> m_pFeature;
};

class QmlLibraryCratesSource : public QmlLibrarySource {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryCratesSource)
  public:
    explicit QmlLibraryCratesSource(QObject* parent = nullptr,
            const QList<QmlLibraryTrackListColumn*>& columns = {});

    LibraryFeature* internal() override {
        return m_pFeature.get();
    }

  private:
    std::unique_ptr<CrateFeature> m_pFeature;
};

class QmlLibraryBrowseSource : public QmlLibrarySource {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryBrowseSource)
  public:
    explicit QmlLibraryBrowseSource(QObject* parent = nullptr,
            const QList<QmlLibraryTrackListColumn*>& columns = {});

    LibraryFeature* internal() override {
        return m_pFeature.get();
    }

  private:
    std::unique_ptr<BrowseFeature> m_pFeature;
};

// Rough-sketch sidebar sources for the previously QML-unsurfaced backends
// (M10 Recordings, M7 History/Auto DJ/Analyze). Same adapter shape as the
// four sources above -- each just wraps the real, already-registered
// LibraryFeature subclass. RecordingFeature/AutoDJFeature need an extra
// manager pointer that isn't reachable through QmlLibraryProxy/QmlConfigProxy
// alone, so those two pull it from the matching singleton proxy's static
// shared_ptr (QmlRecordingProxy::s_pRecordingManager,
// QmlPlayerManagerProxy::s_pPlayerManager), same objects CoreServices already
// seeded at startup for the Recording/PlayerManager singletons.
class QmlLibraryRecordingSource : public QmlLibrarySource {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryRecordingSource)
  public:
    explicit QmlLibraryRecordingSource(QObject* parent = nullptr,
            const QList<QmlLibraryTrackListColumn*>& columns = {});

    LibraryFeature* internal() override {
        return m_pFeature.get();
    }

  private:
    std::unique_ptr<RecordingFeature> m_pFeature;
};

class QmlLibraryHistorySource : public QmlLibrarySource {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryHistorySource)
  public:
    explicit QmlLibraryHistorySource(QObject* parent = nullptr,
            const QList<QmlLibraryTrackListColumn*>& columns = {});

    LibraryFeature* internal() override {
        return m_pFeature.get();
    }

  private:
    std::unique_ptr<SetlogFeature> m_pFeature;
};

class QmlLibraryAnalyzeSource : public QmlLibrarySource {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryAnalyzeSource)
  public:
    explicit QmlLibraryAnalyzeSource(QObject* parent = nullptr,
            const QList<QmlLibraryTrackListColumn*>& columns = {});

    LibraryFeature* internal() override {
        return m_pFeature.get();
    }

  private:
    std::unique_ptr<AnalysisFeature> m_pFeature;
};

class QmlLibraryAutoDJSource : public QmlLibrarySource {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryAutoDJSource)
  public:
    explicit QmlLibraryAutoDJSource(QObject* parent = nullptr,
            const QList<QmlLibraryTrackListColumn*>& columns = {});

    LibraryFeature* internal() override {
        return m_pFeature;
    }

  private:
    // Non-owning: unlike every other QmlLibrary*Source, AutoDJFeature must
    // not be constructed twice (its AutoDJProcessor registers global
    // "[AutoDJ]"-keyed ControlObjects, and a second instance collides with
    // Library's own always-created one -- this used to construct a second
    // AutoDJFeature here, which triggered
    // "ControlObject already created" DEBUG_ASSERTs at startup). Reuses
    // Library::getAutoDJFeature() instead.
    AutoDJFeature* m_pFeature;
};

// Representative External Libraries entry (M7) -- establishes the pattern
// for the rest (Rhythmbox/Traktor/Serato/Rekordbox/Banshee all share the
// same BaseExternalLibraryFeature(Library*, UserSettingsPointer) + static
// isSupported() shape as ITunesFeature and can be added the same way later).
// ITunesFeature::isSupported() unconditionally returns true (it just checks
// for an XML library path at import time, not at construction time), so no
// availability gating is needed here, unlike a real per-platform check.
class QmlLibraryITunesSource : public QmlLibrarySource {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryITunesSource)
  public:
    explicit QmlLibraryITunesSource(QObject* parent = nullptr,
            const QList<QmlLibraryTrackListColumn*>& columns = {});

    LibraryFeature* internal() override {
        return m_pFeature.get();
    }

  private:
    std::unique_ptr<ITunesFeature> m_pFeature;
};

} // namespace qml
} // namespace mixxx
