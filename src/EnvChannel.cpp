
#include "gbsynth.hpp"

#include "tables.h"

#include <cmath>

namespace gbsynth {

    EnvChannel::EnvChannel() : 
        envCounter(0),
        envelope(DEFAULT_ENV_STEPS),
        envLength(DEFAULT_ENV_LENGTH),
        envMode(static_cast<EnvMode>(DEFAULT_ENV_MODE)),
        Channel() 
    {
    }

    float EnvChannel::getCurrentVolume() {
        return Channel::getCurrentVolume() * ENV_TABLE[envelope];
    }

    void EnvChannel::setEnvLength(uint8_t length) {
        if (length > MAX_ENV_LENGTH) {
            length = MAX_ENV_LENGTH;
        }
        envLength = length;
    }

    void EnvChannel::setEnvMode(EnvMode mode) {
        this->envMode = mode;
    }

    void EnvChannel::setEnvStep(uint8_t step) {
        if (step > MAX_ENV_STEPS) {
            step = MAX_ENV_STEPS;
        }
        envelope = step;
    }

    void EnvChannel::envStep() {
        if (envLength) {
            // do nothing if envLength == 0
            if (envCounter == envLength) {
                envCounter = 0;
                if (envMode == EnvMode::amplify) {
                    if (envelope < SAMPLE_MAX) {
                        ++envelope;
                    }
                } else {
                    if (envelope > SAMPLE_MIN) {
                        --envelope;
                    }
                }
            } else {
                ++envCounter;
            }
        }
    }

    void EnvChannel::reset() {
        Channel::reset();
        envCounter = 0;
    }

}