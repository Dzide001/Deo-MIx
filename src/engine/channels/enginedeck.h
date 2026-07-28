#pragma once

#include <QScopedPointer>

#include "engine/channels/enginechannel.h"
#include "preferences/usersettings.h"
#include "soundio/soundmanagerutil.h"
#include "track/track_decl.h"
#include "util/samplebuffer.h"

class EnginePregain;
class EngineBuffer;
class EngineMixer;
class ControlPushButton;
class ControlPotmeter;

class EngineDeck : public EngineChannel, public AudioDestination {
    Q_OBJECT
  public:
    EngineDeck(
            const ChannelHandleAndGroup& handleGroup,
            UserSettingsPointer pConfig,
            EngineMixer* pMixingEngine,
            EffectsManager* pEffectsManager,
            EngineChannel::ChannelOrientation defaultOrientation,
            bool primaryDeck);
    ~EngineDeck() override;

    void process(CSAMPLE* pOutput, const std::size_t bufferSize) override;
    void collectFeatures(GroupFeatureState* pGroupFeatures) const override;

    // postProcessLocalBpm() is called on all decks to update the localBpm after
    // process() is done. Updated localBpms for all decks are required for the
    // postProcess() step, to avoid issues with the order they are processed.
    // It cannot be done during process() because it relies that the localBpm
    // of all decks are on their old values.
    void postProcessLocalBpm() override;

    // Update beat distances, sync modes, and other values that are only known
    // after all other processing is done.
    void postProcess(const std::size_t bufferSize) override;

    // TODO(XXX) This hack needs to be removed.
    EngineBuffer* getEngineBuffer() override;

    EngineChannel::ActiveState updateActiveState() override;

    // This is called by SoundManager whenever there are new samples from the
    // configured input to be processed. This is run in the callback thread of
    // the soundcard this AudioDestination was registered for! Beware, in the
    // case of multiple soundcards, this method is not re-entrant but it may be
    // concurrent with EngineMixer processing.
    void receiveBuffer(const AudioInput& input,
            const CSAMPLE* pBuffer,
            unsigned int nFrames) override;

    // Called by SoundManager whenever the passthrough input is connected to a
    // soundcard input.
    void onInputConfigured(const AudioInput& input) override;

    // Called by SoundManager whenever the passthrough input is disconnected
    // from a soundcard input.
    void onInputUnconfigured(const AudioInput& input) override;

    // Return whether or not passthrough is active
    bool isPassthroughActive() const;

#ifdef __STEM__
    // Clone the stem state (gain and volume) from deckToClone to this. Doesn't
    // check if the loaded track is a stem so this should only be used in case
    // of stem track
    void cloneStemState(const EngineDeck* deckToClone);
    void addStemHandle(const ChannelHandleAndGroup& stemHandleGroup);
    static QString getGroupForStem(QStringView deckGroup, int stemIdx);
#endif

  signals:
    void noPassthroughInputConfigured();

  public slots:
    void slotPassthroughToggle(double v);
    void slotPassthroughChangeRequest(double v);
#ifdef __STEM__
    void slotTrackLoaded(TrackPointer pNewTrack, TrackPointer);
#endif

  private:
#ifdef __STEM__
    // Process multiple channels and mix them together into the passed buffer
    void processStem(CSAMPLE* pOutput, const std::size_t bufferSize);
    // Stem metadata for a freshly loaded track is often not parsed yet at
    // EngineBuffer::trackLoaded time (it's imported asynchronously once the
    // SoundSource is actually opened) -- this re-reads it once
    // Track::stemsUpdated fires for real data, instead of stem_count being
    // permanently stuck at whatever (usually 0) getStemInfo() returned at
    // load time.
    void slotStemsUpdated();
    // M8: registers "stem_acapella"/"stem_instrumental" as real, addressable
    // ControlObjects (rather than the composite mute pattern living only in
    // a QML onClicked handler) so a controller mapping has something to
    // bind a pad/button to -- see milestone_8_controller_support_spec.md's
    // "New custom controls needed" section.
    void slotStemAcapella(double v);
    void slotStemInstrumental(double v);
    // Stem file track order isn't guaranteed by the format; finds the
    // loaded track's stem index whose label contains pattern
    // (case-insensitive), or -1 if not found/no track loaded. Mirrors
    // StemPads.qml's findStemIndex().
    int findStemIndexByLabel(const QString& pattern) const;
#endif

    std::vector<ChannelHandleAndGroup> m_stems;
    std::vector<CSAMPLE_GAIN> m_stemsGainCache;

    UserSettingsPointer m_pConfig;
    EngineBuffer* m_pBuffer;
    EnginePregain* m_pPregain;

#ifdef __STEM__
    // Stem buffer used to retrieve all the channel to mix together
    mixxx::SampleBuffer m_stemBuffer;
    std::unique_ptr<ControlObject> m_pStemCount;
    TrackPointer m_pLoadedTrack;
    std::vector<std::unique_ptr<ControlPotmeter>> m_stemGain;
    std::vector<std::unique_ptr<ControlPushButton>> m_stemMute;
    std::unique_ptr<ControlPushButton> m_pStemAcapella;
    std::unique_ptr<ControlPushButton> m_pStemInstrumental;
    std::vector<std::unique_ptr<EngineVuMeter>> m_stemVuMeter;
    bool m_stemClonedState;
#endif

    // Begin vinyl passthrough fields
    QScopedPointer<ControlObject> m_pInputConfigured;
    ControlPushButton* m_pPassing;
    bool m_bPassthroughIsActive;
    bool m_bPassthroughWasActive;
};
