#include "effects/backends/audiounit/audiounitbackend.h"

#import <AVFAudio/AVFAudio.h>
#import <AudioToolbox/AudioToolbox.h>
#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>

#include <QDebug>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QString>
#include <memory>

#include "effects/backends/audiounit/audiounitbackend.h"
#include "effects/backends/audiounit/audiouniteffectprocessor.h"
#include "effects/backends/audiounit/audiounitmanifest.h"
#include "effects/defs.h"
#include "util/compatibility/qmutex.h"

/// An effects backend for Audio Unit (AU) plugins. macOS-only.
class AudioUnitBackend : public EffectsBackend {
  public:
    AudioUnitBackend()
            : m_componentsById([NSMutableDictionary dictionary]),
              m_loadQueue([[NSOperationQueue alloc] init]) {
        m_loadQueue.name = @"AudioUnitBackend manifest loader";
        m_loadQueue.maxConcurrentOperationCount = kMaxConcurrentLoads;
        loadAudioUnits();
    }

    ~AudioUnitBackend() override {
        // A block still running when this destructor starts must not be
        // allowed to keep running past it -- it captures `this` and writes
        // into m_manifestsById/m_mutex, both about to be freed (confirmed
        // via a real crash: a straggler from an earlier, already-destroyed
        // AudioUnitBackend dereferenced its now-freed `this`, SIGSEGV
        // inside a QSharedPointer's atomic refcount). cancelAllOperations
        // drops anything not yet started (the usual case: loadAudioUnits()
        // already gave up waiting on most of a large backlog after
        // kLoadTimeoutMs), so this only ever blocks for however long the
        // handful of operations already mid-flight take to finish --
        // bounded by AudioUnitManifest's own ~2s internal timeout, not by
        // the full backlog size the way waiting on m_loadQueue itself
        // (rather than cancelling first) would be.
        [m_loadQueue cancelAllOperations];
        [m_loadQueue waitUntilAllOperationsAreFinished];
    }

    EffectBackendType getType() const override {
        return EffectBackendType::AudioUnit;
    };

    const QList<QString> getEffectIds() const override {
        // Only report IDs whose manifest has actually finished loading.
        // loadAudioUnits() registers every discovered component in
        // m_componentsById synchronously but loads manifests asynchronously
        // with a timeout, so m_componentsById can contain IDs with no
        // corresponding entry in m_manifestsById yet.
        return m_manifestsById.keys();
    }

    EffectManifestPointer getManifest(const QString& effectId) const override {
        return m_manifestsById[effectId];
    }

    const QList<EffectManifestPointer> getManifests() const override {
        return m_manifestsById.values();
    }

    bool canInstantiateEffect(const QString& effectId) const override {
        return [m_componentsById objectForKey:effectId.toNSString()] != nil;
    }

    std::unique_ptr<EffectProcessor> createProcessor(
            const EffectManifestPointer pManifest) const override {
        AVAudioUnitComponent* component =
                m_componentsById[pManifest->id().toNSString()];
        return std::make_unique<AudioUnitEffectProcessor>(component);
    }

  private:
    // Limit concurrent manifest loads: each one blocks its thread in
    // waitForAudioUnit for up to 2 seconds, and m_loadQueue spins up a real
    // OS thread per concurrent operation, so an unbounded count here would
    // still spin up as many threads as there are AUs (64+ is common) all
    // at once.
    static constexpr NSInteger kMaxConcurrentLoads = 8;

    NSMutableDictionary<NSString*, AVAudioUnitComponent*>* m_componentsById;
    QHash<QString, EffectManifestPointer> m_manifestsById;
    QMutex m_mutex;
    // A dedicated NSOperationQueue, not GCD's shared global concurrent
    // queue -- this is the whole reason this uses NSOperationQueue instead
    // of the dispatch_group/dispatch_semaphore combination the rest of
    // this codebase's Apple-API code otherwise prefers. Every
    // AudioUnitBackend constructed over this process's lifetime (once per
    // test fixture in the full test suite: 137+ times) used to queue its
    // manifest-load blocks onto the one shared global queue, each block
    // blocking its worker thread for up to 2s; confirmed via a real hang
    // that this exhausts the OS's shared worker-thread pool once enough
    // instances' backlogs overlap -- every worker ends up parked waiting on
    // a semaphore with no thread left free to ever signal it, a permanent
    // deadlock. A private NSOperationQueue gets its own dedicated pool
    // (explicitly designed by Apple to tolerate long-blocking operations),
    // so blocking here never competes with any other AudioUnitBackend
    // instance's queue or the rest of the app for the same limited shared
    // pool.
    NSOperationQueue* m_loadQueue;

    void loadAudioUnits() {
        qDebug() << "Loading audio units...";

        // See
        // https://developer.apple.com/documentation/audiotoolbox/audio_unit_v3_plug-ins/incorporating_audio_effects_and_instruments?language=objc

        // Discover all AU components of both types first, then load all
        // manifests in a single parallel batch. This avoids the performance
        // penalty of two sequential discovery passes each with their own
        // blocking wait.
        auto manager =
                [AVAudioUnitComponentManager sharedAudioUnitComponentManager];

        NSMutableArray<AVAudioUnitComponent*>* allComponents =
                [NSMutableArray array];

        for (OSType componentType :
                {kAudioUnitType_Effect, kAudioUnitType_MusicEffect}) {
            AudioComponentDescription description = {
                    .componentType = componentType,
                    .componentSubType = 0,
                    .componentManufacturer = 0,
                    .componentFlags = 0,
                    .componentFlagsMask = 0,
            };
            auto components =
                    [manager componentsMatchingDescription:description];
            [allComponents addObjectsFromArray:components];
        }

        // Load component manifests (parameters etc.) concurrently since this
        // requires instantiating the corresponding Audio Units. m_loadQueue
        // is a private NSOperationQueue (not GCD's shared global queue --
        // see its declaration comment for why), with its own
        // maxConcurrentOperationCount already capping concurrency, so no
        // separate semaphore is needed here the way a shared GCD queue
        // would require.
        dispatch_group_t group = dispatch_group_create();

        for (AVAudioUnitComponent* component in allComponents) {
            qDebug() << "Found audio unit" << [component name];

            QString effectId = QString::fromNSString(
                    [NSString stringWithFormat:@"%@~%@~%@",
                            [component manufacturerName],
                            [component name],
                            [component versionString]]);

            // Register component
            m_componentsById[effectId.toNSString()] = component;

            dispatch_group_enter(group);
            [m_loadQueue addOperationWithBlock:^{
                // Load manifest (potentially slow blocking operation)
                auto manifest = EffectManifestPointer(
                        new AudioUnitManifest(effectId, component));

                // Register manifest
                auto locker = lockMutex(&m_mutex);
                m_manifestsById[effectId] = manifest;
                locker.unlock();

                dispatch_group_leave(group);
            }];
        }

        const int64_t TIMEOUT_MS = 6000;

        qDebug() << "Waiting for audio unit manifests to be loaded...";
        if (dispatch_group_wait(group,
                    dispatch_time(DISPATCH_TIME_NOW, TIMEOUT_MS * 1000000)) ==
                0) {
            qDebug() << "Successfully loaded audio unit manifests";
        } else {
            qWarning() << "Timed out while loading audio unit manifests";
        }
    }
};

EffectsBackendPointer createAudioUnitBackend() {
    return EffectsBackendPointer(new AudioUnitBackend());
}
