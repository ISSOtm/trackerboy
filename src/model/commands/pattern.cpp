
#include "model/commands/pattern.hpp"
#include "model/PatternModel.hpp"

#define TU commandsPatternTU

SelectionCmd::SelectionCmd(PatternModel &model) :
    mModel(model),
    mPattern((uint8_t)model.mCursorPattern),
    mClip()
{
    mClip.save(model.mPatternCurr, model.mSelection);
}

void SelectionCmd::restore(bool update) {
    auto pattern = mModel.source()->getPattern(mPattern);
    {
        auto ctx = mModel.mModule.edit();
        mClip.restore(pattern);
    }

    mModel.invalidate(mPattern, update);
}

EraseCmd::EraseCmd(PatternModel &model) :
    SelectionCmd(model)
{
}

void EraseCmd::redo() {
    {
        auto ctx = mModel.mModule.edit();
        // clear all set data in the selection
        auto const iter = mClip.selection().iterator();

        for (auto track = iter.trackStart(); track <= iter.trackEnd(); ++track) {
            auto tmeta = iter.getTrackMeta(track);
            auto &data = mModel.getTrack(mPattern, track);

            for (auto row = iter.rowStart(); row <= iter.rowEnd(); ++row) {
                auto &rowdata = data[row];
                if (tmeta.hasColumn<PatternAnchor::SelectNote>()) {
                    rowdata.note = 0;
                }

                if (tmeta.hasColumn<PatternAnchor::SelectInstrument>()) {
                    rowdata.instrumentId = 0;
                }

                if (tmeta.hasColumn<PatternAnchor::SelectEffect1>()) {
                    rowdata.effects[0] = trackerboy::NO_EFFECT;
                }

                if (tmeta.hasColumn<PatternAnchor::SelectEffect2>()) {
                    rowdata.effects[1] = trackerboy::NO_EFFECT;
                }

                if (tmeta.hasColumn<PatternAnchor::SelectEffect3>()) {
                    rowdata.effects[2] = trackerboy::NO_EFFECT;
                }
            }
        }

    }

    mModel.invalidate(mPattern, true);
}

void EraseCmd::undo() {
    restore(true);
}

PasteCmd::PasteCmd(
    PatternModel &model,
    PatternClip const& clip,
    PatternCursor pos,
    bool mix
) :
    QUndoCommand(),
    mModel(model),
    mSrc(clip),
    mPast(),
    mPos(pos),
    mPattern((uint8_t)model.mCursorPattern),
    mMix(mix)
{
    auto region = mSrc.selection();
    region.moveTo(pos);
    region.clamp(model.mPatternCurr.size() - 1);
    mPast.save(model.mPatternCurr, region);
}

void PasteCmd::redo() {
    {
        auto ctx = mModel.mModule.edit();
        auto pattern = mModel.source()->getPattern(mPattern);
        mSrc.paste(pattern, mPos, mMix);
    }

    mModel.invalidate(mPattern, true);
}

void PasteCmd::undo() {
    {
        auto ctx = mModel.mModule.edit();
        auto pattern = mModel.source()->getPattern(mPattern);
        mPast.restore(pattern);
    }

    mModel.invalidate(mPattern, true);
}

ReverseCmd::ReverseCmd(PatternModel &model) :
    mModel(model),
    mSelection(model.mSelection),
    mPattern((uint8_t)model.mCursorPattern)
{
}

void ReverseCmd::redo() {
    reverse();
}

void ReverseCmd::undo() {
    // same as redo() since reversing is an involutory function
    reverse();
}

void ReverseCmd::reverse() {
    {
        auto ctx = mModel.mModule.edit();
        auto const iter = mSelection.iterator();
        auto const midpoint = iter.rowStart() + (iter.rows() / 2);

        for (auto track = iter.trackStart(); track <= iter.trackEnd(); ++track) {
            auto tmeta = iter.getTrackMeta(track);

            auto &data = mModel.getTrack(mPattern, track);
            for (auto row = iter.rowStart(), lastRow = iter.rowEnd(); row < midpoint; ++row, --lastRow) {
                auto &first = data[row];
                auto &last = data[lastRow];

                // inefficient, but works

                if (tmeta.hasColumn<PatternAnchor::SelectNote>()) {
                    std::swap(first.note, last.note);
                }

                if (tmeta.hasColumn<PatternAnchor::SelectInstrument>()) {
                    std::swap(first.instrumentId, last.instrumentId);
                }

                if (tmeta.hasColumn<PatternAnchor::SelectEffect1>()) {
                    std::swap(first.effects[0], last.effects[0]);
                }

                if (tmeta.hasColumn<PatternAnchor::SelectEffect2>()) {
                    std::swap(first.effects[1], last.effects[1]);
                }

                if (tmeta.hasColumn<PatternAnchor::SelectEffect3>()) {
                    std::swap(first.effects[2], last.effects[2]);
                }
            }
        }
    }
    mModel.invalidate(mPattern, true);
}

ReplaceInstrumentCmd::ReplaceInstrumentCmd(PatternModel &model, int instrument) :
    SelectionCmd(model),
    mInstrument(instrument)
{

}

void ReplaceInstrumentCmd::redo() {
    {
        auto ctx = mModel.mModule.edit();
        auto const iter = mClip.selection().iterator();

        for (auto track = iter.trackStart(); track <= iter.trackEnd(); ++track) {
            auto tmeta = iter.getTrackMeta(track);
            auto &data = mModel.getTrack(mPattern, track);

            if (tmeta.hasColumn<PatternAnchor::SelectInstrument>()) {
                for (auto row = iter.rowStart(); row <= iter.rowEnd(); ++row) {
                    auto &rowdata = data[row];
                    if (rowdata.queryInstrument().has_value()) {
                        rowdata.setInstrument((uint8_t)mInstrument);
                    }
                }
            }
        }
    }

    mModel.invalidate(mPattern, false);
}

void ReplaceInstrumentCmd::undo() {
    restore(false);
}

GrowCmd::GrowCmd(PatternModel& model) :
    SelectionCmd(model)
{
}

namespace TU {

// grows a track, in place, for the given span of rows.
static void grow(trackerboy::Track &track, int rowStart, int rowEnd) {
    // we have to modify data backwards, from rowEnd to rowStart

    auto const srcOff = (rowEnd - rowStart) / 2;

    auto dst = rowStart + (srcOff * 2); // destination index of move
    auto src = rowStart + srcOff;       // source index of move

    // move rows
    while (dst > rowStart) {
        track[dst] = track[src];
        dst -= 2;
        --src;
    }
    // rows in between get cleared
    for (auto i = rowStart + 1; i <= rowEnd; i += 2) {
        track[i] = {};
    }
}

// shrinks a track, in place, for the given span of rows
static void shrink(trackerboy::Track &track, int rowStart, int rowEnd) {
    // unlike grow, data is modified from rowStart to rowEnd
    auto dst = rowStart + 1;
    auto src = rowStart + 2;
    // move rows
    while (src <= rowEnd) {
        track[dst] = track[src];
        ++dst;
        src += 2;
    }
    // clear the space that is now available from shrinking
    while (dst <= rowEnd) {
        track[dst] = {};
        ++dst;
    }
}

}

void GrowCmd::redo() {
    {
        auto ctx = mModel.mModule.edit();
        auto const iter = mClip.selection().iterator();
        for (auto track = iter.trackStart(); track <= iter.trackEnd(); ++track) {
            TU::grow(mModel.getTrack(mPattern, track), iter.rowStart(), iter.rowEnd());
        }
    }
    mModel.invalidate(mPattern, true);
}

void GrowCmd::undo() {
    restore(true);
}

ShrinkCmd::ShrinkCmd(PatternModel& model) :
    SelectionCmd(model)
{
}

void ShrinkCmd::redo() {
    {
        auto ctx = mModel.mModule.edit();
        auto const iter = mClip.selection().iterator();
        for (auto track = iter.trackStart(); track <= iter.trackEnd(); ++track) {
            TU::shrink(mModel.getTrack(mPattern, track), iter.rowStart(), iter.rowEnd());
        }
    }
    mModel.invalidate(mPattern, true);
}

void ShrinkCmd::undo() {
    restore(true);
}

TrackEditCmd::TrackEditCmd(
    PatternModel &model,
    uint8_t dataNew,
    uint8_t dataOld,
    QUndoCommand *parent
) :
    QUndoCommand(parent),
    mModel(model),
    mTrack((uint8_t)(model.mCursor.track)),
    mPattern((uint8_t)model.mCursorPattern),
    mRow((uint8_t)model.mCursor.row),
    mNewData(dataNew),
    mOldData(dataOld)
{
}

void TrackEditCmd::redo() {
    setData(mNewData);
}

void TrackEditCmd::undo() {
    setData(mOldData);
}

trackerboy::TrackRow& TrackEditCmd::getRow() {
    return mModel.source()->getRow(
        static_cast<trackerboy::ChType>(mTrack),
        mPattern,
        (uint16_t)mRow
    );
}

void TrackEditCmd::setData(uint8_t data) {
    auto &rowdata = mModel.source()->getRow(
        static_cast<trackerboy::ChType>(mTrack),
        mPattern,
        (uint16_t)mRow
    );

    bool update;
    {
        auto ctx = mModel.mModule.edit();
        update = edit(rowdata, data);
    }

    mModel.invalidate(mPattern, update);

}

// ===

bool NoteEditCmd::edit(trackerboy::TrackRow &rowdata, uint8_t data) {
    rowdata.note = data;
    return false;
}

// ===

bool InstrumentEditCmd::edit(trackerboy::TrackRow &rowdata, uint8_t data) {
    rowdata.instrumentId = data;
    return false;
}

// ===

EffectEditCmd::EffectEditCmd(
    PatternModel &model,
    uint8_t effectNo,
    uint8_t newData,
    uint8_t oldData,
    QUndoCommand *parent
) :
    TrackEditCmd(model, newData, oldData, parent),
    mEffectNo(effectNo)
{
}

bool EffectTypeEditCmd::edit(trackerboy::TrackRow &rowdata, uint8_t data)  {
    auto &effect = rowdata.effects[mEffectNo];
    auto oldtype = effect.type;
    auto type = static_cast<trackerboy::EffectType>(data);
    effect.type = type;
    return trackerboy::effectTypeShortensPattern(type) || trackerboy::effectTypeShortensPattern(oldtype);
}

// ===

bool EffectParamEditCmd::edit(trackerboy::TrackRow &rowdata, uint8_t data) {
    rowdata.effects[mEffectNo].param = data;
    return false;
}

TransposeCmd::TransposeCmd(PatternModel &model, int8_t transposeAmount) :
    SelectionCmd(model),
    mTransposeAmount(transposeAmount)
{
}

void TransposeCmd::redo()  {
    {
        auto ctx = mModel.mModule.edit();
        auto const iter = mClip.selection().iterator();

        for (auto track = iter.trackStart(); track <= iter.trackEnd(); ++track) {
            auto tmeta = iter.getTrackMeta(track);
            if (!tmeta.hasColumn<PatternAnchor::SelectNote>()) {
                continue;
            }

            auto &data = mModel.getTrack(mPattern, track);
            for (auto row = iter.rowStart(); row <= iter.rowEnd(); ++row) {
                data[row].transpose(mTransposeAmount);
            }
        }
    }

    mModel.invalidate(mPattern, false);
}

void TransposeCmd::undo() {
    restore(false);
}

BackspaceCmd::BackspaceCmd(PatternModel &model, QUndoCommand *parent) :
    QUndoCommand(parent),
    mModel(model),
    mPattern(model.mCursorPattern),
    mTrack(model.mCursor.track),
    mRow(model.mCursor.row),
    mDeleted(model.currentPattern()[mRow - 1][mTrack])
{
}

// 0 a
// 1 b <- backspace here
// 2 c
// 3 d

// redo
// 0 b
// 1 c
// 2 d
// 3 . <- empty row

// undo
// 0 a <- restored from mDeleted
// 1 b
// 2 c
// 3 d


void BackspaceCmd::redo() {
    {
        auto editor = mModel.mModule.edit();
        auto &dest = mModel.getTrack(mPattern, mTrack);
        auto const rows = (int)dest.size() - 1;
        for (int i = mRow - 1; i < rows; ++i) {
            dest[i] = dest[i + 1];
        }
        dest[rows] = {};

    }
    mModel.invalidate(mPattern, true);
}

void BackspaceCmd::undo() {

    {
        auto editor = mModel.mModule.edit();
        auto &dest = mModel.getTrack(mPattern, mTrack);
        auto const restoredRow = mRow - 1;
        for (int i = (int)dest.size() - 1; i > restoredRow; --i) {
            dest[i] = dest[i - 1];
        }
        dest[restoredRow] = mDeleted;
    }
    mModel.invalidate(mPattern, true);
}

InsertRowCmd::InsertRowCmd(PatternModel& model, QUndoCommand* parent) :
    QUndoCommand(parent),
    mModel(model),
    mPattern(model.mCursorPattern),
    mTrack(model.mCursor.track),
    mRow(model.mCursor.row),
    mLastRow(model.source()->patterns().length() - 1),
    mTruncated(model.currentPattern()[mLastRow][mTrack])
{
}

void InsertRowCmd::redo() {
    {
        auto editor = mModel.mModule.edit();
        auto &track = mModel.getTrack(mPattern, mTrack);
        // shift down
        for (auto i = mLastRow; i > mRow; --i) {
            track[i] = track[i - 1];
        }
        track[mRow] = {};
    }
    mModel.invalidate(mPattern, true);
}

void InsertRowCmd::undo() {
    {
        auto editor = mModel.mModule.edit();
        auto &track = mModel.getTrack(mPattern, mTrack);
        // shift up
        for (auto i = mRow; i < mLastRow; ++i) {
            track[i] = track[i + 1];
        }
        track[mLastRow] = mTruncated;
    }
    mModel.invalidate(mPattern, true);
}

#undef TU
