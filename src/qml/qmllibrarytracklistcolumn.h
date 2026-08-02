#pragma once

#include <QAbstractItemModel>
#include <QObject>
#include <QQmlEngine>
#include <QQmlListProperty>
#include <QQmlParserStatus>
#include <QQuickItem>
#include <QVariant>

#include "library/columncache.h"
#include "qml/qml_owned_ptr.h"

namespace mixxx {
namespace qml {

class QmlLibraryTrackListColumn : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString label MEMBER m_label FINAL)
    Q_PROPERTY(int fillSpan MEMBER m_fillSpan FINAL)
    Q_PROPERTY(int columnIdx MEMBER m_columnIdx FINAL)
    Q_PROPERTY(double preferredWidth MEMBER m_preferredWidth FINAL)
    Q_PROPERTY(double autoHideWidth MEMBER m_autoHideWidth FINAL)
    Q_PROPERTY(QQmlComponent* delegate READ delegate WRITE setDelegate FINAL)
    Q_PROPERTY(Role role MEMBER m_role FINAL)
    // M7/column-visibility: whether this column is currently hidden --
    // TrackList.qml's updateColumnSize()/columnWidthProvider already read
    // this (collapsing a hidden column to 0 width), but until now nothing
    // ever set it, since this property didn't exist. A plain MEMBER
    // property (no NOTIFY) is consistent with how preferredWidth is
    // already mutated after construction by TrackList.qml's own
    // column-resize handler -- callers that mutate `hidden` at runtime
    // are expected to also call view.updateColumnSize()/forceLayout()
    // themselves, same as that existing resize path already does.
    Q_PROPERTY(bool hidden MEMBER m_hidden FINAL)
    QML_NAMED_ELEMENT(TrackListColumn)
  public:
    enum class SQLColumns {
        Album = ColumnCache::COLUMN_LIBRARYTABLE_ALBUM,
        Artist = ColumnCache::COLUMN_LIBRARYTABLE_ARTIST,
        Title = ColumnCache::COLUMN_LIBRARYTABLE_TITLE,
        Year = ColumnCache::COLUMN_LIBRARYTABLE_YEAR,
        Bpm = ColumnCache::COLUMN_LIBRARYTABLE_BPM,
        Key = ColumnCache::COLUMN_LIBRARYTABLE_KEY,
        FileType = ColumnCache::COLUMN_LIBRARYTABLE_FILETYPE,
        Bitrate = ColumnCache::COLUMN_LIBRARYTABLE_BITRATE,
        // M7/column-visibility: 7 more real columns surfaced to QML for
        // the first time -- all already exist in ColumnCache::Column and
        // are shown by the legacy skin today, just never wired up here.
        Genre = ColumnCache::COLUMN_LIBRARYTABLE_GENRE,
        Comment = ColumnCache::COLUMN_LIBRARYTABLE_COMMENT,
        Composer = ColumnCache::COLUMN_LIBRARYTABLE_COMPOSER,
        Duration = ColumnCache::COLUMN_LIBRARYTABLE_DURATION,
        DateAdded = ColumnCache::COLUMN_LIBRARYTABLE_DATETIMEADDED,
        TimesPlayed = ColumnCache::COLUMN_LIBRARYTABLE_TIMESPLAYED,
        Rating = ColumnCache::COLUMN_LIBRARYTABLE_RATING,
    };
    Q_ENUM(SQLColumns)
    enum class Role {
        Location,
        Artist,
        Title,
        Cover,
    };
    Q_ENUM(Role)
    explicit QmlLibraryTrackListColumn(QObject* parent = nullptr)
            : QObject(parent) {
    }
    explicit QmlLibraryTrackListColumn(QObject* parent,
            const QString& label,
            int fillSpan,
            int columnIdx,
            double preferredWidth,
            double autoHideWidth,
            QQmlComponent* delegate,
            Role role,
            bool hidden);
    const QString& label() const {
        return m_label;
    }
    Role role() const {
        return m_role;
    }
    int fillSpan() const {
        return m_fillSpan;
    }
    int columnIdx() const {
        return m_columnIdx;
    }
    double preferredWidth() const {
        return m_preferredWidth;
    }
    double autoHideWidth() const {
        return m_autoHideWidth;
    }
    QQmlComponent* delegate() const {
        return m_pDelegate;
    }
    void setDelegate(QQmlComponent* delegate) {
        m_pDelegate = qml_owned_ptr(delegate);
    }
    bool hidden() const {
        return m_hidden;
    }

  private:
    QString m_label;
    Role m_role;
    int m_fillSpan{0};
    int m_columnIdx{-1};
    double m_preferredWidth{-1};
    double m_autoHideWidth{-1};
    bool m_hidden{false};
    qml_owned_ptr<QQmlComponent> m_pDelegate;
};
} // namespace qml
} // namespace mixxx
