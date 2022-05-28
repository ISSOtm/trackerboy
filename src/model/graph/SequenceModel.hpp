
#pragma once

#include "model/graph/GraphModel.hpp"

#include "trackerboy/data/Sequence.hpp"

#include <cstdint>
#include <vector>


class SequenceModel : public GraphModel {

    W_OBJECT(SequenceModel)

public:
    explicit SequenceModel(Module &mod, QObject *parent = nullptr);

    virtual int count() override;

    virtual DataType dataAt(int index) override;

    virtual void setData(int index, DataType data) override;

    //
    // Sets the sequence data source for the model. The caller is responsible
    // for the lifetime of the given sequence.
    //
    void setSequence(trackerboy::Sequence *seq);

    void setSize(int size);

    void setLoop(uint8_t pos);

    void removeLoop();

    void replaceData(std::vector<uint8_t> const& data);

    trackerboy::Sequence* sequence() const;

private:
    trackerboy::Sequence *mSequence;

};
