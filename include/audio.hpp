
#pragma once

#include <stdexcept>
#include <memory>

#include "portaudio.h"
#include "pa_ringbuffer.h"

namespace audio {


class PaException : public std::runtime_error {
    PaError err;
public:
    PaException(PaError err);

    PaError getError();
};


class PlaybackQueue {

    PaStream *stream;
    //PaUtilRingBuffer ringLeft;
    //PaUtilRingBuffer ringRight;
    PaUtilRingBuffer ringbuf;
    std::unique_ptr<float[]> framedata;
    //std::unique_ptr<float[]> framedataLeft;
    //std::unique_ptr<float[]> framedataRight;

    // frame settings
    size_t samplesPerFrame; // spf = framerate / samplingRate
    float samplingRate;

    friend PaStreamCallback playbackCallback;

public:

    PlaybackQueue(float samplingRate);
    ~PlaybackQueue();

    bool canWrite();

    size_t framesize();

    void setSamplingRate(float samplingRate);

    void start();

    void stop(bool wait);

    void writeFrame(float frame[]);

};



}

