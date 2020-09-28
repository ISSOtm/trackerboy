
#include "ConfigDialog.hpp"

#include "portaudio.h"

#include <algorithm>
#include <cmath>

#pragma warning(push, 0)
#include "ui_ConfigDialog.h"
#pragma warning(pop)

static const char *SAMPLING_RATE_STR[] = {
    "11,025 Hz",
    "22,050 Hz",
    "44,100 Hz",
    "48,000 Hz",
    "96,000 Hz"
};


ConfigDialog::ConfigDialog(Config &config, QWidget *parent) :
    mUi(new Ui::ConfigDialog()),
    mConfig(config),
    mDeviceManager(),
    mIgnoreSelections(false),
    QDialog(parent)
{
    mUi->setupUi(this);

    // populate the host combo with all available host apis
    // we only need to do this once
    auto &deviceTable = audio::DeviceTable::instance();
    auto &hosts = deviceTable.hosts();
    for (auto &host : hosts) {
        mUi->mHostApiCombo->addItem(QString::fromLatin1(host.info->name));
    }

    connect(mUi->mBufferSizeSlider, &QSlider::valueChanged, this, &ConfigDialog::bufferSizeSliderChanged);
    connect(mUi->mVolumeSlider, &QSlider::valueChanged, this, &ConfigDialog::volumeSliderChanged);

    connect(mUi->mGainSlider1, &QSlider::valueChanged, this, [this](int value) { gainChanged(0, value); });
    connect(mUi->mGainSlider2, &QSlider::valueChanged, this, [this](int value) { gainChanged(1, value); });
    connect(mUi->mGainSlider3, &QSlider::valueChanged, this, [this](int value) { gainChanged(2, value); });
    connect(mUi->mGainSlider4, &QSlider::valueChanged, this, [this](int value) { gainChanged(3, value); });

    connect(mUi->mHostApiCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ConfigDialog::hostApiSelected);
    connect(mUi->mDeviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ConfigDialog::deviceSelected);
    connect(mUi->mSamplerateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ConfigDialog::samplerateSelected);

    // reset all controls to our Config's settings
    resetControls();

}

ConfigDialog::~ConfigDialog() {
    delete mUi;
}

void ConfigDialog::accept() {
    // update all changes to the Config object
    mConfig.setDeviceId(mDeviceManager.portaudioDevice());
    mConfig.setSamplerate(mDeviceManager.samplerates()[mDeviceManager.currentSamplerate()]);
    mConfig.setBuffersize(mUi->mBufferSizeSlider->value());
    mConfig.setVolume(mUi->mVolumeSlider->value());
    mConfig.setGain(trackerboy::ChType::ch1, mUi->mGainSlider1->value());
    mConfig.setGain(trackerboy::ChType::ch2, mUi->mGainSlider2->value());
    mConfig.setGain(trackerboy::ChType::ch3, mUi->mGainSlider3->value());
    mConfig.setGain(trackerboy::ChType::ch4, mUi->mGainSlider4->value());


    QDialog::accept();
}

void ConfigDialog::reject() {
    // reset all settings
    resetControls();

    QDialog::reject();
}

void ConfigDialog::showEvent(QShowEvent *evt) {
    mUi->mTabWidget->setCurrentIndex(0);
    QDialog::showEvent(evt);
}

void ConfigDialog::bufferSizeSliderChanged(int value) {
    QString text("%1 ms");
    mUi->mBufferSizeLabel->setText(text.arg(QString::number(value)));
}

void ConfigDialog::volumeSliderChanged(int value) {
    QString text("%1%");
    mUi->mVolumeLabel->setText(text.arg(QString::number(value)));
}

void ConfigDialog::hostApiSelected(int index) {
    if (!mIgnoreSelections) {
        mDeviceManager.setCurrentApi(index);

        // populate the device combo box with devices for this host
        mIgnoreSelections = true;
        fillDeviceCombo(index);
        mIgnoreSelections = false;

        mUi->mDeviceCombo->setCurrentIndex(mDeviceManager.currentDevice());
    }
}

void ConfigDialog::deviceSelected(int index) {
    
    // ignore this signal when items are being added to the combobox
    if (!mIgnoreSelections) {
        mIgnoreSelections = true;
        mDeviceManager.setCurrentDevice(index);
        fillSamplerateCombo();
        mUi->mSamplerateCombo->setCurrentIndex(mDeviceManager.currentSamplerate());
        mIgnoreSelections = false;
    }
}

void ConfigDialog::samplerateSelected(int index) {
    if (!mIgnoreSelections) {
        mDeviceManager.setCurrentSamplerate(index);
    }
}

void ConfigDialog::gainChanged(int channel, int value) {
    QLabel *gainLabel;
    switch (channel) {
        case 0:
            gainLabel = mUi->mGainLabel1;
            break;
        case 1:
            gainLabel = mUi->mGainLabel2;
            break;
        case 2:
            gainLabel = mUi->mGainLabel3;
            break;
        default:
            gainLabel = mUi->mGainLabel4;
            break;
    }
    QString text = QString("%1%2.%3 dB")
                    .arg(value < 0 ? '-' : '+')
                    .arg(value / 10)
                    .arg(abs(value) % 10);
    gainLabel->setText(text);
}

void ConfigDialog::fillDeviceCombo(int hostIndex) {
    mUi->mDeviceCombo->clear();

    auto &deviceTable = audio::DeviceTable::instance();
    auto devicesBegin = deviceTable.devicesBegin(hostIndex);
    auto devicesEnd = deviceTable.devicesEnd(hostIndex);
    for (auto iter = devicesBegin; iter != devicesEnd; ++iter) {
        mUi->mDeviceCombo->addItem(QString::fromLatin1(iter->info->name));
    }
}

void ConfigDialog::fillSamplerateCombo() {
    mUi->mSamplerateCombo->clear();
    auto &samplerates = mDeviceManager.samplerates();
    for (auto rate : samplerates) {
        mUi->mSamplerateCombo->addItem(QString::fromLatin1(SAMPLING_RATE_STR[rate]));
    }
}


void ConfigDialog::resetControls() {
    mIgnoreSelections = true;
    mDeviceManager.setPortaudioDevice(mConfig.deviceId());

    int host = mDeviceManager.currentHost();
    mUi->mHostApiCombo->setCurrentIndex(host);
    fillDeviceCombo(host);

    mUi->mDeviceCombo->setCurrentIndex(mDeviceManager.currentDevice());

    // reset samplerate
    fillSamplerateCombo();
    int samplerate = mConfig.samplerate();
    int samplerateIndex = 0;
    for (auto rate : mDeviceManager.samplerates()) {
        if (rate >= samplerate) {
            break;
        }
        ++samplerateIndex;
    }

    mDeviceManager.setCurrentSamplerate(samplerateIndex);
    mUi->mSamplerateCombo->setCurrentIndex(samplerateIndex);

    mIgnoreSelections = false;

    mUi->mBufferSizeSlider->setValue(mConfig.buffersize());
    mUi->mVolumeSlider->setValue(mConfig.volume());
    mUi->mGainSlider1->setValue(mConfig.gain(trackerboy::ChType::ch1));
    mUi->mGainSlider2->setValue(mConfig.gain(trackerboy::ChType::ch2));
    mUi->mGainSlider3->setValue(mConfig.gain(trackerboy::ChType::ch3));
    mUi->mGainSlider4->setValue(mConfig.gain(trackerboy::ChType::ch4));


}



