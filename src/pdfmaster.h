// SPDX-FileCopyrightText: 2022 Valentin Bruch <software@vbruch.eu>
// SPDX-License-Identifier: GPL-3.0-or-later OR AGPL-3.0-or-later

#ifndef PDFMASTER_H
#define PDFMASTER_H

#include <utility>
#include <map>
#include <QObject>
#include <QList>
#include <QMap>
#include <QRectF>
#include <QString>
#include "src/config.h"
#include "src/enumerates.h"
#include "src/rendering/pdfdocument.h"
#include "src/drawing/pathcontainer.h"

class SlideScene;
class QGraphicsItem;
class QBuffer;
class QXmlStreamReader;
class QXmlStreamWriter;
class AbstractGraphicsPath;
class TextGraphicsItem;
struct SlideTransition;

namespace drawHistory {
    struct DrawToolDifference;
    struct TextPropertiesDifference;
    struct ZValueChange;
    struct Step;
}

/**
 * Full document including PDF and paths / annotations added by user.
 * This should also manage drawings and multimedia content of the PDF.
 */
class PdfMaster : public QObject
{
    Q_OBJECT

public:
    /// Flags for different kinds of unsaved changes.
    enum Flags
    {
        UnsavedDrawings = 1 << 0,
        UnsavedTimes = 1 << 1,
        UnsavedNotes = 1 << 2,
        FullPageUsed = LeftHalf >> 1,
        LeftHalfUsed = LeftHalf,
        RightHalfUsed = RightHalf,
    };

private:
    /// Document representing the PDF
    PdfDocument *document {nullptr};

    /// Graphics scenes of this application. For each combination of PDF file
    /// and page shift one scene is created.
    /// Master scene is the first scene in the list.
    QList<SlideScene*> scenes;

    /// Path to file in which drawings are saved.
    QString drawings_path;

    /// Map page (part) numbers to containers of paths.
    /// page (part) numbers are given as (page | page_part)
    /// Paths can be drawn per slide label by creating references to the main
    /// path list from other slide numbers.
    QMap<int, PathContainer*> paths;

    /// Time at which a slide should be finished.
    QMap<int, quint32> target_times;

    /// Flags for unsaved changes.
    unsigned char _flags = 0;

    /// Search results (currently only one results)
    std::pair<int, QList<QRectF>> search_results;

    /// make sure paths[page] is a PathContainer*
    void assertPageExists(const int page) noexcept
    {
        if (!paths.contains(page) || !paths[page])
            paths[page] = new PathContainer(this);
    }

public:
    /// Create empty, uninitialized PdfMaster.
    explicit PdfMaster() {}

    /// Destructor. Deletes paths and document.
    ~PdfMaster();

    /// get function for search_results
    const std::pair<int, QList<QRectF>> &searchResults() const noexcept
    {return search_results;}

    /// get function for _flags
    unsigned char &flags() noexcept
    {return _flags;}

    /// Load PDF file.
    void loadDocument(const QString &filename);

    /// Load or reload the file. Return true if the file was updated and false
    /// otherwise.
    bool loadDocument();

    /// Get path to PDF file.
    const QString &getFilename() const
    {return document->getPath();}

    /// Get the list of SlideScenes connected to this PDF.
    QList<SlideScene*> &getScenes()
    {return scenes;}

    /// Get size of page in points (floating point precision).
    const QSizeF getPageSize(const int page_number) const
    {return document->pageSize(page_number);}

    /// Get PdfDocument.
    PdfDocument *getDocument() const
    {return document;}

    /// Return true if document contains pages of different size.
    bool flexiblePageSizes() const noexcept
    {return document->flexiblePageSizes();}

    /// Clear history of given page.
    void clearHistory(const int page, const int remaining_entries) const
    {
        PathContainer *container = paths.value(page, nullptr);
        if (container)
            container->clearHistory(remaining_entries);
    }

    /// Slide transition when reaching the given page number.
    const SlideTransition transition(const int page) const
    {return document->transition(page);}

    /// Number of pages in the document.
    int numberOfPages() const
    {return document->numberOfPages();}

    /// Get page number of start shifted by shift_overlay.
    /// Here in shift_overlay the bits of ShiftOverlay::FirstOverlay and
    /// ShiftOverlay::LastOverlay control the interpretation of the shift.
    /// Shifting with overlays means that every page with a different page
    /// label starts a new "real" side.
    int overlaysShifted(const int start, const int shift_overlay) const
    {return document->overlaysShifted(start, shift_overlay);}

    /// Write page (part) to image, including drawings.
    QPixmap exportImage(const int page, const qreal resolution) const noexcept;

    /// Read a page element from XML stream
    void readPageFromStream(QXmlStreamReader &reader);
    /// Load drawings from XML reader, must be in element <layer>
    void readDrawingsFromStream(QXmlStreamReader &reader, const int page);

    /// Get path container at given page. If overlay_mode==Cumulative, this may
    /// create and return a copy of a previous path container.
    /// page (part) number is given as (page | page_part).
    PathContainer *pathContainerCreate(int page);

    /// Get path container at given page.
    /// page (part) number is given as (page | page_part).
    PathContainer *pathContainer(int page) const
    {return paths.value(page, nullptr);}

    /// Get file path at which drawings are saved.
    const QString &drawingsPath() const noexcept
    {return drawings_path;}

    /// Clear all drawings including history.
    void clearAllDrawings();

    /// Check if page currently contains any drawings (ignoring history).
    bool hasDrawings() const noexcept;

    /// Write pages objects to XML
    void writePages(QXmlStreamWriter &writer, const bool save_bp_specific);

public slots:
    /// Handle the given action.
    void receiveAction(const Action action);

    /// Add a new path (or QGraphicsItem) to paths[page].
    /// Page (part) number is given as (page | page_part).
    /// If item is nullptr: create the container if it does not exist yet.
    void receiveNewPath(int page, QGraphicsItem *item)
    {replacePath(page, nullptr, item);}

    /// Replace an existing path (or QGraphicsItem) in paths[page] by the gievn new one.
    /// Old or new item can be nullptr, then only a new item will be created or an
    /// existing one will be removed, respectively.
    /// Page (part) number is given as (page | page_part).
    /// If both items are nullptr, only the container is created (if it doesn't exist yet).
    void replacePath(int page, QGraphicsItem *olditem, QGraphicsItem *newitem);

    /// Add history step with transformations, tool changes, and text
    /// property changes (not text content changes!).
    /// Page (part) number is given as (page | page_part).
    void addHistoryStep(int page,
            std::map<QGraphicsItem*, QTransform> *transforms,
            std::map<AbstractGraphicsPath*, drawHistory::DrawToolDifference> *tools,
            std::map<TextGraphicsItem*, drawHistory::TextPropertiesDifference> *texts);

    /// Add new paths.
    void addItemsForeground(int page, const QList<QGraphicsItem*> &items);
    /// Remove paths.
    void removeItems(int page, const QList<QGraphicsItem*> &items);

    /// Send navigation events to all SlideScenes reading from this document.
    /// This is done centrally via PdfMaster because it may be necessary
    /// to reconnect SlideViews and SlideScenes if multiple scenes would
    /// show the same page.
    void distributeNavigationEvents(const int page) const;

    /// Get path container at given page. If overlay_mode==Cumulative, this may
    /// create and return a copy of a previous path container.
    /// page (part) number is given as (page | page_part).
    void requestNewPathContainer(PathContainer **container, int page)
    {*container = pathContainerCreate(page);}

    /// Get path container at given page. Always create a new container if it
    /// does not exist yet.
    void createPathContainer(PathContainer **container, int page);

    /// Get target_times map reference
    QMap<int, quint32> &targetTimes() noexcept
    {return target_times;}

    /// Set time for page and write it to target_times.
    void setTimeForPage(const int page, const quint32 time) noexcept
    {
        target_times[page] = time;
        _flags |= UnsavedTimes;
    }

    /// Get time for given page and write it to time.
    void getTimeForPage(const int page, quint32 &time) const noexcept;

    /// Set UnsavedDrawings flag.
    void newUnsavedDrawings() noexcept
    {_flags |= UnsavedDrawings;}

    /// Bring given items to foreground and add history step.
    void bringToForeground(int page, const QList<QGraphicsItem*> &to_foreground);
    /// Bring given items to background and add history step.
    void bringToBackground(int page, const QList<QGraphicsItem*> &to_background);

    /// Handle the given action.
    void search(const QString &text, const int &page, const bool forward);

    void setDrawingsPath(const QString &filename)
    {drawings_path = filename;}

signals:
    /// Write notes from notes widgets to stream writer.
    void writeNotes(QXmlStreamWriter &writer) const;
    /// Read notes in notes widgets from stream reader.
    void readNotes(QXmlStreamReader &reader) const;
    /// Set total time of presentation (preferences().total_time).
    void setTotalTime(const QTime time) const;
    /// Send navigation signal to master.
    void navigationSignal(const int page);
    /// Tell slides to update search results.
    void updateSearch();
};

/// Unzip file to buffer.
QBuffer *loadZipToBuffer(const QString &filename);

#endif // PDFMASTER_H
