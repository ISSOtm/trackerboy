
#pragma once

#include "model/SongListModel.hpp"
#include "widgets/grid/PatternGridHeader.hpp"
#include "widgets/grid/PatternPainter.hpp"
#include "ColorTable.hpp"

#include "trackerboy/data/Pattern.hpp"

#include <QWidget>
#include <QPaintEvent>
#include <QString>
#include <QBitmap>
#include <QRect>
#include <QSize>

#include <cstdint>
#include <optional>


class PatternGrid : public QWidget {

    Q_OBJECT

public:

    explicit PatternGrid(SongListModel &model, PatternGridHeader &header, QWidget *parent = nullptr);
    ~PatternGrid() = default;

    // Settings

    //
    // force a full redraw of the pattern
    //
    void redraw();

    //
    // Returns the current row of the cursor
    //
    int row() const;

    //
    // set color scheme, grid is redrawn afterwards
    //
    void setColors(ColorTable const& colors);

    // show pattern data for the previous and next patterns
    void setPreviewEnable(bool previews);

    // if true then flats will be shown instead of sharps
    void setShowFlats(bool showFlats);

signals:

    void cursorColumnChanged(int column);

    // emitted when the user changes the current row via keyboard, scroll wheel or mouse
    void cursorRowChanged(int row);

public slots:

    void setCursorTrack(int track);
    void setCursorColumn(int column);
    void setCursorRow(int row);
    void setCursorPattern(int pattern);

    void moveCursorRow(int amount);
    void moveCursorColumn(int amount);


protected:

    void changeEvent(QEvent *evt) override;

    void leaveEvent(QEvent *evt) override;

    void mouseMoveEvent(QMouseEvent *evt) override;

    void mousePressEvent(QMouseEvent *evt) override;

    void mouseReleaseEvent(QMouseEvent *evt) override;

    void paintEvent(QPaintEvent *evt) override;

    void resizeEvent(QResizeEvent *evt) override;

private slots:

    void onSongChanged(int index);

private:

    enum ColumnType {
        COLUMN_NOTE,

        // high is the upper nibble (bits 4-7)
        // low is the lower nibble (bits 0-3)

        COLUMN_INSTRUMENT_HIGH,
        COLUMN_INSTRUMENT_LOW,

        COLUMN_EFFECT1_TYPE,
        COLUMN_EFFECT1_ARG_HIGH,
        COLUMN_EFFECT1_ARG_LOW,

        COLUMN_EFFECT2_TYPE,
        COLUMN_EFFECT2_ARG_HIGH,
        COLUMN_EFFECT2_ARG_LOW,

        COLUMN_EFFECT3_TYPE,
        COLUMN_EFFECT3_ARG_HIGH,
        COLUMN_EFFECT3_ARG_LOW

    };

    //
    // Called when appearance settings have changed, recalculates metrics and redraws
    // all rows.
    //
    void fontChanged();

    void calcOffset();

    //
    // Calculates the number of rows we can draw on this widget
    //
    unsigned getVisibleRows();

    //
    // Calculates the x position of the given column
    //
    int columnLocation(int column);

    //
    // Converts translated mouse coordinates on the grid to a row and column coordinate
    //
    void getCursorFromMouse(int x, int y, unsigned &outRow, unsigned &outCol);

    //
    // Paints rows using the given painter. Row indices range form 0 to mVisibleRows.
    // Row 0 is the first visible row on the widget.
    // 0 <= rowStart < rowEnd <= mVisibleRows
    //
    void paintRows(QPainter &painter, int rowStart, int rowEnd);

    //
    // Scrolls displayed rows and paints new ones. If rows >= mVisibleRows then
    // no scrolling will occur and the entire display will be repainted
    //
    void scroll(int rows);

    //
    // Get pattern data for the given pattern index
    //
    void setPatterns(int pattern);

    void setPatternRect();


    SongListModel &mModel;
    PatternGridHeader &mHeader;
    PatternPainter mPainter;

    // display image, rows get painted here when needed as opposed to every
    // paintEvent
    QPixmap mDisplay;

    // if true all rows will be redrawn on next paint event
    bool mRepaintImage;

    int mCursorRow;
    int mCursorCol;
    int mCursorPattern;    // the current pattern

    int mOffset;

    std::optional<trackerboy::Pattern> mPatternPrev;
    trackerboy::Pattern mPatternCurr;
    std::optional<trackerboy::Pattern> mPatternNext;

    // rectangle of the current rendered pattern
    QRect mPatternRect;

    bool mSelecting;

    // settings
    bool mSettingShowPreviews;


    // variables here are dependent on appearance settings
    // treat them as constants, only fontChanged() can modify them

    unsigned mVisibleRows; // number of rows visible on the widget
    // mVisibleRows * mRowHeight is always >= height()


};

