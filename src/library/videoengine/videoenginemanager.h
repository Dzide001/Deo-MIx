#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QObject>
#include <QString>
#include <functional>
#include <memory>

class ControlPushButton;
class ControlProxy;
class QThread;
class QTimer;

// GStreamer's own public headers typedef these as opaque struct pointers
// (gst/gstelement.h, gst/gstpad.h) -- forward-declaring here avoids pulling
// GStreamer headers into this header at all, matching the pimpl-lite
// pattern already used by QmlVideoPreviewItem.
typedef struct _GstElement GstElement;
typedef struct _GstPad GstPad;
// Stage 7: same opaque-pointer treatment for the GL-interop types and
// GstSample -- see the class doc comment's Stage 7 entry.
typedef struct _GstGLDisplay GstGLDisplay;
typedef struct _GstGLContext GstGLContext;
typedef struct _GstSample GstSample;

namespace mixxx {

#ifdef __VIDEO_ENGINE_NDI_OUTPUT__
class NdiOutputSender;
#endif

/// M12 Stage 3b/3c/3d: the real mixxx-lib integration of the video engine
/// researched/validated in milestone_12_video_spec_addendum.md.
///
/// Stage 3d adds real per-deck video: each of the two decks ("[Channel1]"/
/// "[Channel2]") can have an independent video clip loaded via loadVideo(),
/// decoupled from whatever audio track that deck is playing (per the
/// user's explicit decision -- a VJ-style clip-deck model, not a
/// music-video-follows-the-track model). The two loaded sources are
/// composited together with a GStreamer `compositor`, and the blend
/// between them mirrors the real `[Master]/crossfader` ControlObject
/// (range -1.0..1.0) using the same linear alpha mapping proven correct by
/// the standalone Stage 3d-1 CLI (mixxx-videoengine-crossfade-test):
/// alphaA = (1-position)/2, alphaB = (1+position)/2.
///
/// Deliberately still minimal: exactly two fixed compositor slots (matching
/// the skin's two decks), no looping of a loaded clip once it reaches EOS,
/// no camera input (deferred to its own later stage), no third-party
/// layer/z-order UI. Per-layer position/opacity/z-order ControlObjects and
/// camera input are still open follow-up work -- see the addendum doc's
/// "Next" section.
///
/// Stage 3i: the whole shared pipeline's PLAYING/PAUSED state follows
/// whether EITHER deck's own "play" CO is active, so a freshly loaded clip
/// no longer auto-plays the instant it's loaded onto a paused deck. This
/// is a real, documented compromise, not full independent per-deck
/// play/pause: with the compositor's two branches sharing one pipeline, if
/// deck A is playing and deck B is paused, deck B's video still visibly
/// advances too (only its audio stays correctly paused, via Mixxx's
/// separate, unaffected audio engine). True independent per-branch
/// pause/resume was investigated (GStreamer's `gst_element_set_locked_state`
/// technique) and confirmed NOT reliably resumable in this pipeline
/// shape -- see milestone_12_video_spec_addendum.md's Stage 3i writeup.
/// Genuinely independent per-deck video pause needs two separate
/// pipelines blended in application code instead of one shared
/// `compositor`, which is a bigger follow-up, not attempted here.
/// SUPERSEDED by Stage 4 below -- kept as historical rationale for why
/// the two-pipeline rewrite happened.
///
/// Stage 3j: play-state sync alone wasn't enough -- each branch's own
/// internal GStreamer position kept advancing (or sitting frozen wherever
/// it happened to be) independently of the audio deck's actual
/// `[Channel]/playposition`, so repeated pause/resume/cue cycles visibly
/// drifted video and audio apart. Fixed by seeking that deck's branch
/// (`gst_element_seek_simple` on its `decodebin`, a normal per-element
/// GStreamer seek -- much simpler and more reliable than the locked-state
/// technique that failed for resume) to match the deck's current
/// `playposition` every time that deck's `play` CO transitions to active,
/// and once when a clip is first loaded. This does not handle a seek/hot
/// cue fired while ALREADY playing (only re-anchors on the next
/// play-start), which is a smaller, real, separately-documented gap.
///
/// Stage 3k: two follow-up fixes once the above was confirmed working but
/// visibly imperfect. First, `gst_element_seek_simple()` returns as soon
/// as a seek is *accepted*, not once the pipeline has actually settled
/// into the new position -- letting playback resume immediately after
/// issuing a seek made the video visibly "fast-forward" to catch up
/// rather than jump instantly; "fixed" by blocking on
/// `gst_element_get_state()` right after seeking (later found to be
/// wrong -- see Stage 3m). Second, each branch's `decodebin` didn't have
/// a bounded `queue` after it (unlike the standalone CLI that first
/// proved this whole seek approach), so it could buffer well ahead of
/// what was actually displayed, making a seek's target frame fight
/// through a backlog of stale buffers -- same visible symptom, different
/// cause; fixed by adding `queue max-size-buffers=2 leaky=downstream`.
/// Third, re-sync is no longer limited to play/pause transitions:
/// `[Channel]/playposition` is now watched continuously (throttled, not
/// on every tick), comparing the video's actual GStreamer position
/// against where the audio says it should be, and only correcting when
/// they've drifted past a tolerance -- this covers hotcues, scratching,
/// and seeks fired while a deck is already playing, which Stage 3j's
/// play-transition-only re-anchor could not.
///
/// Stage 3l: attempted to fix reported choppy ("~3-5fps") playback by
/// loosening two Stage 3k parameters suspected of being too aggressive --
/// the queue's `max-size-buffers=2` (widened to a ~500ms time-bounded
/// `max-size-time`) and the drift-correction tolerance/throttle (widened
/// 200ms/250ms -> 1s/1s). Reported by the user as NOT fixed ("still
/// same, like 1fps") -- both changes were real improvements but treated
/// the wrong root cause.
///
/// Stage 3m: found the actual root cause. `correctDriftIfNeeded()` (and
/// its caller, `slotDeckA/BPositionChanged`) run on VideoEngineManager's
/// own thread -- the GUI/main thread -- because
/// `ControlProxy::connectValueChanged()`'s default `Qt::AutoConnection`
/// resolves to a *queued* connection when `[Channel]/playposition` is set
/// from the audio engine thread. Stage 3k's blocking
/// `gst_element_get_state(..., GST_SECOND)` after every seek therefore
/// blocked the whole Qt event loop -- including
/// `QmlVideoPreviewItem`'s poll timer and all QML repainting -- for up to
/// a full second on every correction. That is what actually produced the
/// "1fps" symptom; Stage 3l's parameter tuning only changed how often
/// corrections fired, not how long each one froze the UI. Fixed (at the
/// time) by reverting seeks to fire-and-forget, trading back to a brief
/// visible catch-up artifact (Stage 3j's original behavior) instead of
/// periodic UI freezes. Restored in Stage 3p once the periodic caller
/// itself was gone -- see below.
///
/// Stage 3n: a second, distinct choppiness bug, this one in
/// `sourceBranchFor()`'s per-branch `queue` (see videoenginemanager.cpp):
/// `leaky=downstream` meant that once the queue filled -- which happens
/// almost immediately during ordinary playback, since `decodebin` decodes
/// as fast as the CPU allows while the compositor/appsink downstream is
/// clock-paced to real playback speed -- it discarded decoded frames
/// continuously rather than applying backpressure. Fixed by removing
/// `leaky=downstream`, letting the queue block instead (GStreamer's
/// default), which throttles decode to real-time with no frames lost;
/// `GST_SEEK_FLAG_FLUSH` still empties the queue on every seek regardless
/// of leaky mode, so this doesn't reopen Stage 3k's original seek-backlog
/// problem.
///
/// Stage 3o: Stage 3k's third sub-fix -- continuous drift correction via
/// `correctDriftIfNeeded()`, watching `[Channel]/playposition` throughout
/// playback rather than only at play transitions -- turned out to be
/// fundamentally broken and was removed. `expectedNs` was computed as
/// `audioPosition * videoDurationNs`, i.e. the audio deck's normalized
/// position applied to the *video* branch's own queried duration. Since
/// Stage 3h, the audio (Mixxx's own FFmpeg-based engine) and video
/// (GStreamer's `decodebin`) are two independently decoding the same
/// file -- if their reported durations differ by even a small amount
/// (common for real files: edit lists, container duration rounding,
/// slightly different demuxer behavior), `expectedNs` carries a
/// persistent, systematic offset from the video's true position, not a
/// transient one. Once that offset exceeds the 1-second tolerance, *every*
/// throttle window (~1s) finds "drift" and fires a flush-seek
/// (`GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT`) -- a disruptive
/// flush/reseek roughly once a second, forever, during completely normal
/// playback. That's what surfaced as a periodic flash/warp/shake, at
/// almost exactly the throttle interval. Rather than trying to calibrate
/// out a per-file duration mismatch (fragile, and every attempt at tuning
/// this specific feature has produced a new regression), continuous
/// correction was removed entirely; Stage 3j's play-transition re-anchor
/// (fires only when a deck's `play` CO actually changes, never
/// periodically) is kept and remains the only re-sync mechanism. This
/// reopens Stage 3j's original, smaller, documented gap: a hotcue or seek
/// fired while a deck is *already* playing won't be caught until the next
/// play-state transition. Stage 4's continuous drift correction (see
/// below) revisits this with an anchor-and-delta design specifically
/// meant to avoid this exact failure mode -- see its own writeup.
///
/// Stage 3p: with Stage 3o's periodic caller gone, Stage 3m's fire-and-
/// forget seeks brought back exactly the artifact Stage 3k originally
/// fixed -- confirmed by the user ("the regular flash is gone now but
/// the fast forward thingy is back"). `seekElementBlocking()`'s post-seek
/// `gst_element_get_state()` wait is restored: its only remaining callers
/// are `seekDeckToCurrentPosition()`, itself only invoked from a deck's
/// play/pause CO transition and from `rebuildPipeline()` on load -- rare,
/// discrete, user-initiated events, not a periodic loop. A bounded block
/// there now risks at most a brief stall right when the user presses
/// play/pause/cue or loads a clip, never a recurring freeze during
/// ordinary playback, since nothing calls this method on a timer or a
/// continuous signal anymore.
///
/// Stage 3q: restoring the wait wasn't enough either -- the fast-forward
/// look was still there specifically on resume/cue. Cause:
/// `GST_SEEK_FLAG_KEY_UNIT` deliberately lands on the nearest keyframe at
/// or before the target rather than the exact frame, trading accuracy for
/// speed; for real compressed video with multi-second keyframe spacing,
/// that can land genuinely seconds behind the true target, and playing
/// forward from there through the (non-leaky, since Stage 3n) queue's
/// backlog is a visible catch-up burst. Switched to
/// `GST_SEEK_FLAG_ACCURATE`, which resolves the exact frame internally as
/// part of the seek -- no intervening frames are ever pushed downstream,
/// so there's nothing to flash through. Costs more decode time per seek,
/// acceptable since this only runs on rare discrete events already inside
/// a bounded wait.
///
/// Stage 3r: scrubbing the position while a deck is paused (dragging the
/// waveform/position indicator, not pressing play) previously did nothing
/// to the video at all -- only a play/pause CO transition re-seeks the
/// video branch, so the preview stayed frozen on whatever frame it last
/// showed until the user actually pressed play. Fixed by watching
/// `[Channel]/playposition` again (Stage 3o deleted the previous
/// subscription along with the broken continuous-correction feature, but
/// this is a different, simpler use), this time with logic that cannot
/// reproduce Stage 3o's bug: `slotDeckA/BPositionChanged()` only acts
/// while that deck is NOT playing (`m_deckA/BPlaying == false`) -- it
/// never compares GStreamer's own position against an audio-derived
/// expectation (the thing that carried Stage 3o's systematic-offset bug),
/// it just seeks directly to match the paused deck's current position.
/// Throttled to 100ms (short enough to feel responsive while dragging,
/// long enough not to queue up a seek storm) and fire-and-forget with
/// `GST_SEEK_FLAG_KEY_UNIT` rather than blocking+`ACCURATE`
/// (`seekElementBlocking()`/Stage 3q's precision is for the rare,
/// discrete play/cue moment; a scrub can fire many times a second while
/// dragging, where a blocking accurate seek per tick would make the drag
/// itself feel frozen -- approximate, keyframe-snapped preview during the
/// drag is the standard tradeoff most scrub-preview UIs make). Once the
/// deck actually starts playing, `seekDeckToCurrentPosition()`'s existing
/// accurate blocking seek takes over and lands exactly.
///
/// Stage 3s: Stage 3r's scrub preview didn't work at all, and the
/// resume/cue fast-forward from Stage 3k/3m/3p/3q survived even the
/// switch to `GST_SEEK_FLAG_ACCURATE` -- both traced to the same root
/// cause. `compositor` (like other GStreamer aggregator-based mixers)
/// only produces a new composited output frame when the pipeline clock
/// is actually advancing, i.e. while `PLAYING`. While `PAUSED`, it
/// renders exactly one preroll frame and then sits idle; seeking one
/// upstream branch (decodebin) doesn't make the compositor recomposite
/// on its own -- the branch's own position advances, but nothing tells
/// the compositor to look again. That explains both symptoms: scrubbing
/// while paused seeks the branch correctly but the composited output
/// appsink pulls from never updates (nothing forces a new composite), and
/// resuming has to visibly "catch up" through whatever the compositor's
/// stale internal state was once the clock starts advancing again.
/// `GST_SEEK_FLAG_ACCURATE` (Stage 3q) couldn't have fixed this -- it was
/// never a seek-precision problem.
///
/// Fixed with GStreamer's frame-stepping mechanism
/// (`gst_event_new_step()`), the standard technique for "seek while
/// paused, see the result immediately" -- sent to the whole pipeline
/// (only while it's actually `PAUSED`; if the pipeline is `PLAYING`
/// because the *other* deck is active, new frames already flow
/// naturally and stepping would disrupt that deck's playback) after
/// every seek, both the accurate blocking one in
/// `seekElementBlocking()` and the throttled scrub one (renamed
/// `commitScrubSeek()` in Stage 3t). This forces exactly one fresh composited
/// frame through to appsink immediately, so: scrubbing now actually
/// updates the preview, and by the time a play transition's
/// `applyPlayState()` flips the pipeline to `PLAYING`, the compositor is
/// already sitting at the correct, current frame rather than something
/// stale it needs to catch up from.
///
/// Stage 3t: fast-forward-on-resume confirmed fixed, but two more real
/// bugs surfaced. First, a single, isolated position change (a waveform
/// click, a cue jump) could be silently swallowed: Stage 3r's
/// `QElapsedTimer`-based throttle just dropped any tick arriving within
/// 100ms of the last one, which is correct for coalescing a *continuous*
/// drag (later ticks keep arriving, one eventually gets through) but
/// wrong for a single discrete jump with no follow-up tick -- if that one
/// tick landed inside the throttle window, nothing ever re-tried it.
/// Replaced with a proper trailing-edge debounce: a real `QTimer`
/// (single-shot) is (re)started on every tick while paused, so a lone
/// click still fires ~100ms later, while a rapid drag keeps restarting
/// the timer and only the final quiet moment commits a seek -- the
/// commit (`commitScrubSeek()`) always re-reads the *current* position at
/// fire time rather than whatever was captured when the timer started.
/// This also explains the cue-then-play bug: cue's position jump (while
/// paused) could itself be a swallowed single tick, but that shouldn't
/// have mattered since `seekDeckToCurrentPosition()` independently
/// re-reads the current position on the *play* transition regardless --
/// the debounce fix is what actually matters here, not a cue-specific
/// gap.
///
/// Second, seeking while a deck is already playing was still a known,
/// accepted gap (Stage 3j/3o) -- nothing reacted to it at all. Closed
/// without reintroducing Stage 3o's failure mode: `handlePositionChanged()`
/// now tracks each deck's own previously-seen audio position
/// (`m_deckA/BLastAudioPosition`) and, while playing, compares the *new*
/// audio position only against that -- never against GStreamer's own
/// queried position, which is what carried Stage 3o's systematic
/// audio/video duration-mismatch bug. Ordinary per-tick playback
/// advancement is tiny (a small fraction of a percent of track length
/// per tick); a genuine seek/hotcue/click moves position by a large,
/// unambiguous jump in a single tick. A >=1%-of-track-length threshold
/// distinguishes the two reliably, and reuses the existing, already-safe
/// `seekDeckToCurrentPosition()` (accurate, blocking, rare/discrete-event
/// safe per Stage 3p's reasoning) rather than inventing new seek logic.
///
/// Stage 3v: the fast-forward-on-resume symptom returned specifically
/// when resuming after a cue press, even with everything above in place.
/// Diagnostic logging (kLogger.info(), tagged "[diag]") revealed a real,
/// reproducible signal-delivery bug rather than a seek/sync-logic
/// problem: `slotDeckAPlayChanged()` never fired at all for the QML deck
/// Play button's click (confirmed the button visibly flipped state and
/// audio genuinely resumed, but zero corresponding log line, repeatedly,
/// across separate test runs) -- while the audio-thread-originated,
/// cue-triggered `play` transition (`CueControl::cueCDJ()`'s
/// `m_pPlay->set(0.0)`) always fired correctly and logged as expected.
/// The difference: `VideoEngineManager` and the QML Play button both live
/// on the GUI thread, so that write is genuinely same-thread, and
/// `ControlProxy::connectValueChanged()`'s default `Qt::AutoConnection`
/// resolves a same-thread connection between `ControlDoublePrivate` and
/// `ControlProxy` to a *direct*, synchronous connection -- a different
/// code path than the proven-reliable cross-thread queued one the
/// audio-thread-originated write took. Fixed by explicitly requesting
/// `Qt::QueuedConnection` for all four play/position CO subscriptions
/// (`m_pDeckAPlay`, `m_pDeckBPlay`, `m_pDeckAPosition`,
/// `m_pDeckBPosition`), removing same-thread direct delivery as a
/// variable entirely and converging on the one mechanism already proven
/// to work. This likely also explains Stage 3t's still-unconfirmed
/// "click to seek" symptom, since a waveform click is the same kind of
/// same-thread QML-originated write.
///
/// Stage 3w: Stage 3v fixed signal delivery, but the user still saw two
/// related symptoms -- a cue press (while paused, staying paused) just
/// visibly holding in place rather than jumping to the cue point, and
/// click-to-seek landing at the right position but with a visible
/// fast-forward look. Both trace to the same design tension in
/// `commitScrubSeek()`: it fires a *non-blocking*
/// `GST_SEEK_FLAG_KEY_UNIT` seek and *immediately* calls
/// `stepOneFrameIfPaused()` afterward -- deliberately non-blocking so an
/// active drag doesn't feel frozen (Stage 3t). But for a single discrete
/// tick (a click, a cue jump), nothing is dragging, so there's no reason
/// to avoid blocking: the forced compositor step can race ahead of the
/// still-in-flight seek and push through a stale, pre-seek frame, which
/// is exactly what both symptoms look like (a jump that never visibly
/// happens, or a genuine jump followed by catch-up once the real,
/// delayed frames arrive). Fixed by distinguishing the two cases:
/// `handlePositionChanged()` now counts ticks accumulated since the last
/// commit (`m_deckA/BScrubTicksSinceLastCommit`) before (re)starting the
/// debounce timer; `commitScrubSeek()` checks that count when it fires --
/// exactly one tick (a lone click or cue jump, no drag in between) uses
/// `seekElementBlocking()` (the same accurate, blocking seek already
/// proven correct for play/pause/cue transitions), while more than one
/// tick (an active drag) keeps the original fast, non-blocking
/// `KEY_UNIT` path so dragging still feels responsive.
///
/// Stage 3x: clip looping. Loaded clips previously played once and held on
/// the last frame at EOS (explicitly out of scope per this class's
/// original doc comment above). A `compositor` sink pad only marks its
/// own stream finished once it sees an EOS event arrive on that pad -- a
/// pad probe (GST_PAD_PROBE_TYPE_EVENT_BOTH) on each of the two
/// compositor sink pads (m_pCompositorPad0/1) intercepts that EOS before
/// the compositor ever processes it, drops it (GST_PAD_PROBE_DROP) so the
/// pad never gets marked finished, and queues (Qt::QueuedConnection, off
/// the GStreamer streaming thread the probe runs on) a call back onto
/// this object's own thread to re-seek that branch's decodebin to 0.
/// (Stage 4 relocates this probe to each deck's own appsink pad, since
/// the compositor is gone -- same technique, different attachment
/// point.) The placeholder `videotestsrc` branch never reaches EOS on its
/// own, so a deck without a real clip loaded is unaffected.
///
/// Stage 3y: camera input, deferred at this class's original writing
/// ("no camera input") to its own stage. `avfvideosrc` (macOS camera
/// capture) is provided by GStreamer's "applemedia" plugin, which this
/// class already statically registers (see the GST_PLUGIN_STATIC_*
/// calls in the .cpp) -- it was linked in for other applemedia elements
/// already in use, so no new plugin/link work was needed, only the
/// pipeline-integration code. setCameraSource() swaps which source feeds
/// an EXISTING deck slot (camera vs. loaded clip vs. black placeholder).
/// Defaults to the system's first camera (`device-index=0`); a device
/// picker for machines with more than one camera is a smaller follow-up,
/// not attempted here.
///
/// Stage 3z: the long-standing "cue/loop reset just holds on the old
/// frame instead of jumping to the right position" bug, confirmed real
/// via user reports across the whole time this class has existed.
/// Diagnosed via real log data end to end: (1) `handlePositionChanged()`'s
/// forward-jump threshold (1% of track length) was also the only test
/// applied to BACKWARD jumps, but a beatloop's loop-out->loop-in reset is
/// routinely well under 1% of the full track -- fixed with a separate,
/// much smaller backward-specific threshold, since ordinary forward
/// playback never decreases position (no threshold is needed for that
/// direction) but scratching does move backward frequently in small
/// increments (a threshold IS still needed to avoid triggering an
/// expensive seek on every scratch-jitter tick). (2) `applyPlayState()`
/// requests PAUSED via a non-blocking `gst_element_set_state()` call and
/// never waits for it to land, so the immediately-following frame-step
/// call could read a stale pre-transition state and silently skip --
/// fixed with a short bounded wait before checking. (3) The confirmed
/// root cause: `compositor` (an Aggregator subclass) never produced a
/// new composited frame after GStreamer's documented
/// `gst_event_new_step()` "seek while paused" technique, verified
/// directly via a bounded post-step `grabPreviewFrame()` pull that
/// returned null every single time, regardless of whether the step was
/// sent to the whole pipeline or directly to the appsink, and regardless
/// of whether the other deck's placeholder branch was a live or
/// non-live `videotestsrc`. Rather than continue debugging why the
/// textbook STEP technique doesn't work in this specific pipeline shape,
/// `stepOneFrameIfPaused()` (name kept despite no longer stepping) now
/// uses a brief real PLAYING pulse instead -- a mechanism independently
/// proven reliable (it's how the whole crossfade feature was validated)
/// -- to force at least one genuine frame through, then returns to
/// PAUSED, leaving that frame in the appsink's queue for the real
/// preview's own independent poll to pick up.
///
/// Stage 3z-9/3z-10: seeks moved off the calling (GUI) thread entirely --
/// first onto independent detached threads (3z-9), then onto one shared,
/// serialized worker thread (3z-10) once concurrent detached threads were
/// found to race on `m_pPipeline`'s state with no ordering. See
/// `seekElementAsync()`'s comment (still accurate as of Stage 4, just
/// re-scoped to a per-deck worker -- see below) for the full history.
///
/// Stage 4: a genuine architectural rewrite, not another tuning pass.
/// Real testing (comparing [diag] logs against what the user actually
/// saw) surfaced two structural problems the incremental fixes above
/// could not solve without this rewrite:
///
/// (a) No true independent per-deck pause. Stage 3i's shared-pipeline
/// compromise means deck A's video can keep animating even while deck A
/// itself is genuinely paused with no audio playing at all, whenever
/// anything else keeps the one shared pipeline in `PLAYING` (or, in one
/// confirmed case, a stray leftover `stepOneFrameIfPaused()` PLAYING
/// pulse from an older, already-superseded background task -- itself
/// fixed by Stage 3z-10's serialized worker, but the underlying
/// shared-pipeline coupling remained).
///
/// (b) No continuous audio/video lock. Every fix above (Stage 3j onward)
/// is reactive: it corrects video position only at specific discrete
/// events (play/pause/cue, a big jump, a beatloop reset). Between those
/// events, each deck's video free-runs on GStreamer's own wall-clock and
/// drifts from the actual audio position during ordinary playback -- by
/// design, this architecture cannot deliver "audio and video stay in
/// sync at all times."
///
/// Both are fixed by splitting the single shared pipeline (one
/// `compositor` blending two branches) into two fully independent
/// per-deck `GstElement*` pipelines (`DeckVideoContext::pPipeline`), each
/// ending at its own `appsink` instead of a shared compositor pad.
/// Blending for display moves out of GStreamer entirely and into C++
/// (`blendFrames()`, called from `grabPreviewFrame()`), using the same
/// linear alpha mapping the old `applyCrossfaderAlpha()` used to write
/// into the compositor's pad properties. Each deck's own `playing` flag
/// now drives only that deck's own pipeline's PLAYING/PAUSED state
/// (`applyPlayState(DeckVideoContext&)`) -- there is no shared state left
/// for one deck's video to keep animating through the other's pause.
/// EOS-loop pad probes (Stage 3x) relocate from the (now-gone)
/// compositor's sink pads to each deck's own appsink pad, same
/// intercept-and-drop technique. Each deck gets its own serialized seek
/// worker thread (`DeckVideoContext::pSeekThread`/`pSeekWorkerContext`,
/// same Stage 3z-10 mechanism, just no longer shared across decks --
/// there is no longer a shared `GstElement` for two decks' workers to
/// race over) so a burst of corrections on one deck never queues behind
/// the other's.
///
/// On top of that structural split, `handlePositionChanged()` gains a
/// continuous drift-correction path for ordinary playback (not just big
/// jumps), using an anchor-and-delta design specifically shaped to avoid
/// Stage 3o's failure mode: rather than recomputing an absolute expected
/// position from `audioPosition * videoDuration` every check (which
/// carries forward any persistent per-file audio/video duration mismatch
/// as a permanent, non-decaying offset -- Stage 3o's actual bug), this
/// tracks how far each side has moved *since the last known-good sync
/// point* and only ever compares that small, recent delta. The anchor
/// resets on every correction (continuous or discrete), so a structural
/// duration mismatch can only ever contaminate one recent window, never
/// accumulate. See `maybeCorrectContinuousDrift()`'s own comment.
///
/// Stage 4's known limitation as originally written here -- continuous
/// drift correction alone assumed 1x video playback, so a deck played at a
/// different tempo would drift and get repeatedly re-corrected rather than
/// genuinely locking -- is addressed by Stage 5 below.
///
/// Stage 5: rate-matching. `handleRateChanged()` subscribes each deck to
/// `"[ChannelN]/rate_ratio"` (a normalized speed multiplier already written
/// by the pitch/tempo slider, sync-lock, and vinyl control -- NOT
/// `"[ChannelN]/rate"`, the raw untransformed slider position) and slaves
/// that deck's video playback rate to match, so a deck played faster or
/// slower than 1x now genuinely plays its video at the matching speed
/// instead of relying purely on `maybeCorrectContinuousDrift()` to
/// repeatedly re-seek a video that's structurally always trying to play at
/// 1x. A standalone spike (`videoengine_rateseek_cli.cpp`, since removed --
/// see `src/videoengine/CMakeLists.txt`'s history comment) confirmed
/// `GST_SEEK_FLAG_INSTANT_RATE_CHANGE` -- the mechanism that would let this
/// happen with zero flushing/glitch cost at all -- is outright rejected by
/// this pipeline's `decodebin` chain (`gst_element_seek()` returned FALSE
/// every time, not merely "accepted but ignored"), confirming GStreamer's
/// own documented "can't work in all cases" caveat for that flag applies
/// here. `handleRateChanged()` therefore uses an ordinary flushing
/// `gst_element_seek()` per rate change instead, throttled/deduped
/// (skipped unless the new rate differs from the last-applied one past a
/// small epsilon) so it doesn't reseek on every tiny CO tick -- the same
/// cost/tradeoff as this class's other flushing seeks. Deliberately does
/// NOT attempt to track a sign change (a direction change, e.g. through a
/// backspin/reverse excursion): GStreamer's rate-seek mechanisms are
/// unreliable across a direction change regardless of flag, and reverse
/// decode support is inconsistent per-codec; a direction change instead
/// falls back to the existing position-based correction paths.
/// `maybeCorrectContinuousDrift()`'s role shrank accordingly, from "the
/// only mechanism ever catching non-1x drift" to a coarse safety net for
/// what rate-tracking can't see (vinyl's throttled/direction-stripped
/// `rate_ratio` echo, the async gap between a rate change and its queued
/// rate-seek landing, transient scratch/backspin) -- its tolerance/throttle
/// were widened to match.
///
/// Stage 6: NDI output (`#ifdef __VIDEO_ENGINE_NDI_OUTPUT__`, a separate
/// build-time option gated on a separately-downloaded, free-but-proprietary
/// SDK -- see the root `CMakeLists.txt`'s `VIDEO_ENGINE_NDI_OUTPUT` block).
/// `NdiOutputSender` (`src/library/videoengine/ndioutputsender.{h,cpp}`) is
/// fed the exact same composited `QImage` `grabPreviewFrame()` already
/// produces for the in-app preview window -- no GStreamer pipeline changes,
/// no duplicate blending work. Since Stage 4 moved compositing entirely out
/// of GStreamer into C++, there was never a GStreamer pipeline segment to
/// tee an NDI sink off of in the first place, so this calls the NDI SDK's
/// own raw C send API directly from this class rather than integrating a
/// GStreamer NDI plugin (which would additionally have meant a Rust/Cargo
/// build dependency with documented arm64-macOS build-maturity risk, a
/// mismatch with this project's plain-C/C++ static-plugin convention).
///
/// Stage 7: GPU-upload preview path. `grabPreviewFrame()`'s CPU path
/// (GStreamer appsink -> `QImage` copy -> `blendFrames()` -> `QPainter`
/// blit) stays exactly as-is and remains the default/fallback -- this adds
/// a second, opt-in path alongside it rather than replacing it, since
/// there's no safe *partial* version of "avoid the CPU copy": routing
/// through GStreamer's GL elements but still ending at a CPU-mapped frame
/// would pay for a GPU upload AND a GPU download for zero benefit, worse
/// than the existing CPU path, not a smaller version of the real feature.
/// `setSharedGlContext()` is called once, by a new companion QML item
/// (`Mixxx.VideoPreviewGl`, `src/qml/qmlvideopreviewglitem.{h,cpp}`) as
/// soon as its own `QQuickWindow`'s scene graph/RHI OpenGL context is
/// ready -- VideoEngineManager can't do this eagerly at construction time
/// (Stage 3g's reasoning for building pipelines in the constructor) because
/// no QML window/GL context exists yet that early. On success, both decks'
/// pipelines are rebuilt to route through `glupload`/`glcolorconvert`
/// instead of plain `videoconvert`, ending in GL-memory
/// (`memory:GLMemory`) caps instead of system-memory RGB -- the decoded
/// frame lands directly in a GPU texture, no CPU copy. `isGlPreviewAvailable()`
/// reports whether that succeeded, mainly for logging/diagnostics -- QML
/// itself doesn't need to reactively branch on it at all:
/// `VideoPreviewPanel.qml` stacks `Mixxx.VideoPreviewGl` directly on top of
/// the original, always-live `Mixxx.VideoPreview` (`QmlVideoPreviewItem`,
/// completely untouched) at the same geometry. Before GL sharing succeeds
/// (or if it never does -- wrong platform, a Qt RHI API returning null,
/// etc.) the GL item's `updatePaintNode()` simply returns a null node, so
/// the CPU item underneath shows through with no broken intermediate
/// state; once GL frames start arriving, the GL item's own opaque
/// composite covers the CPU one every tick, with no visible seam since
/// both draw the exact same crossfader-driven blend. This sidesteps
/// needing any cross-thread "which path is ready" signal between C++ and
/// QML, which would otherwise be its own small source of races given
/// `setSharedGlContext()` runs on the render thread.
///
/// The actual GL-context-sharing mechanism follows GStreamer's own
/// documented app-integration pattern: each deck's pipeline bus gets a
/// synchronous handler (`gst_bus_set_sync_handler()`, not the normal async
/// bus-watch dispatch -- the `GST_MESSAGE_NEED_CONTEXT` query this responds
/// to must be answered before the requesting element proceeds past its own
/// state change) that answers `GST_GL_DISPLAY_CONTEXT_TYPE`/
/// `"gst.gl.app_context"` context requests with `m_pGlDisplay`/
/// `m_pGlContext`, letting `glupload` create its own internal GStreamer GL
/// context *shared* with the app-provided one (`gst_gl_context_new_wrapped()`,
/// wrapping the native `CGLContextObj` Qt Quick's own OpenGL RHI backend is
/// using -- extracted via `QNativeInterface::QCocoaGLContext`, verified
/// against the actually-installed Qt 6.8.3 headers before writing this, in
/// a small Objective-C++ helper file since that extraction needs ObjC,
/// `src/qml/qmlvideopreviewglitem_mac.mm`). A texture created by either
/// side is then valid on the other, with no copy.
///
/// `Mixxx.VideoPreviewGl` renders the two decks as two `QSGSimpleTextureNode`
/// children (deck A opaque, deck B drawn on top with its opacity set to
/// the same crossfader-derived alpha `blendFrames()` already uses) rather
/// than a custom shader/material -- since both sources are always fully
/// opaque RGB with no alpha of their own, and the two alpha weights always
/// sum to 1.0, "draw A, then draw B at opacity alphaB" is mathematically
/// identical to `blendFrames()`'s `A*alphaA + B*alphaB` mix, achieved with
/// a plain, already-public Qt Quick API instead of hand-written GLSL --
/// deliberately the simpler, lower-risk choice given this whole stage's
/// real risk is the GL-context-sharing plumbing, not the blend math.
///
/// Each deck's `GstSample` (which owns the actual GL texture's memory) is
/// kept alive for two frames past the one it was pulled for (a small
/// ring, not released the instant `updatePaintNode()` returns) since Qt
/// Quick's actual GPU read of a texture happens asynchronously, on its own
/// render schedule, not synchronously within that call.
///
/// **Genuinely novel territory for this codebase, not fully verified**:
/// this is the one piece of the whole video engine that can't be checked
/// by reading logs or querying element state -- only by looking at the
/// screen. Every API used here (`QRhiTexture::createFrom()`,
/// `QQuickWindow::createTextureFromRhiTexture()`, `QNativeInterface::
/// QCocoaGLContext`, GStreamer's GL context-sharing calls) was verified to
/// exist with the expected signature against the actual installed Qt
/// 6.8.3 and GStreamer headers before being used, which rules out "wrong
/// API name" as a failure mode -- it does NOT rule out a genuine logic or
/// synchronization bug in code this complex that's never been exercised
/// for real. Real-world verification (does the video actually render, any
/// corruption/tearing, does it stay stable under extended real use) needs
/// the user's own hands-on test.
class VideoEngineManager : public QObject {
    Q_OBJECT
  public:
    explicit VideoEngineManager(QObject* parent);
    ~VideoEngineManager() override;

    /// False if GStreamer failed to initialize (e.g. this build wasn't
    /// compiled with VIDEO_ENGINE, or gst_init() itself failed) -- QML
    /// should treat the preview item as unavailable rather than attempting
    /// to construct a pipeline in that case.
    bool isAvailable() const {
        return m_available;
    }

    /// Loads a video file for the given deck group. Only "[Channel1]" and
    /// "[Channel2]" are recognized (the two independent per-deck pipelines
    /// wired up so far). Rebuilds only that deck's own pipeline from
    /// scratch rather than dynamically relinking pads on a running one --
    /// simpler and more robust, and loading a clip is a rare, deliberate
    /// action, not a hot path. Since Stage 4, this no longer touches the
    /// OTHER deck's pipeline at all (previously, loading a clip on deck A
    /// also rebuilt -- and briefly glitched -- deck B's video, since both
    /// shared one pipeline). Returns false if unavailable, the deck group
    /// isn't recognized, or the rebuilt pipeline failed to reach PLAYING.
    bool loadVideo(const QString& deckGroup, const QString& filePath);

    /// Stage 3y: switches a deck's video source to a live macOS camera feed
    /// (GStreamer's `avfvideosrc`, provided by the "applemedia" plugin
    /// already statically registered above -- no new plugin linking
    /// needed) instead of whatever clip is loaded. The camera is just an
    /// alternate SOURCE for one deck's own pipeline, still blended by the
    /// same real `[Master]/crossfader`-driven alpha math
    /// (`blendFrames()`) as before. Disabling it (enabled=false) reverts
    /// that deck to its loaded clip (or the black placeholder if none).
    /// Rebuilds only that deck's own pipeline (see loadVideo()'s comment).
    /// Returns false if unavailable or the deck group isn't recognized.
    bool setCameraSource(const QString& deckGroup, bool enabled);

#ifdef __VIDEO_ENGINE_NDI_OUTPUT__
    /// Stage 6: starts or stops advertising the composited preview as an
    /// NDI source named `sourceName` (see the class doc comment's Stage 6
    /// entry and `NdiOutputSender`). Only declared/compiled when built
    /// with `-DVIDEO_ENGINE_NDI_OUTPUT=ON` (and a valid `-DNDI_ROOT=<path>`
    /// pointing at the NDI SDK) -- callers (QmlVideoEngineProxy) must
    /// `#ifdef` around any use of this method the same way. Returns false
    /// if unavailable or if the NDI SDK's own send-instance creation fails
    /// (e.g. the NDI runtime isn't actually installed on this machine).
    bool enableNdiOutput(bool enabled, const QString& sourceName);
#endif

    /// Pulls the latest available frame from each deck's own appsink and
    /// blends them in C++ using the current crossfader value, returning
    /// one composited QImage. Returns a null QImage if neither deck has
    /// ever produced a frame. Exists primarily so the crossfade math can
    /// be verified end-to-end (real files, real CO-driven alpha) without a
    /// QML preview widget in the loop; a future thumbnail/snapshot feature
    /// could reuse it too.
    QImage grabPreviewFrame(int timeoutMs = 2000);

    /// Stage 7: one deck's currently-available GL texture, ready for direct
    /// use by Qt Quick's scene graph -- no CPU copy. `textureId == 0` means
    /// no frame is available (deck not loaded/rebuilding). `pOwningSample`
    /// is the GstSample that owns the texture's actual GPU memory; the
    /// caller MUST eventually pass it to releaseGlFrameSample() (not
    /// immediately -- see the class doc comment's Stage 7 entry on why a
    /// short-lived ring, not instant release, is needed) or the
    /// underlying GStreamer buffer pool can't reclaim it.
    struct GlFrame {
        unsigned int textureId = 0;
        int width = 0;
        int height = 0;
        GstSample* pOwningSample = nullptr;
    };
    /// Both decks' current GL frames plus the crossfader value needed to
    /// blend them, in one call (avoids two separate calls racing against a
    /// crossfader value that could change in between). Returns default-
    /// constructed (textureId == 0 for both) if !isGlPreviewAvailable().
    struct GlPreviewFrames {
        GlFrame deckA;
        GlFrame deckB;
        double crossfaderValue = 0.0;
    };
    GlPreviewFrames grabPreviewGlFrames();
    /// Releases a GstSample previously handed out via grabPreviewGlFrames()
    /// -- a thin wrapper so callers (QmlVideoPreviewGlItem) never need to
    /// include a GStreamer header themselves just to call gst_sample_unref().
    void releaseGlFrameSample(GstSample* pSample);

    /// Stage 7: called once by QmlVideoPreviewGlItem as soon as its own
    /// QQuickWindow's OpenGL RHI context exists, handing over the native
    /// CGLContextObj (as a raw quintptr -- see
    /// qmlvideopreviewglitem_mac.mm) so GStreamer's GL elements can create
    /// their own context genuinely shared with Qt Quick's. Rebuilds both
    /// decks' pipelines to route through GL on success. Returns false (and
    /// leaves both decks on the existing, proven CPU pipeline untouched)
    /// if `nativeCglContextHandle` is 0 or GL context creation otherwise
    /// fails -- safe to call defensively; failure here is not fatal to the
    /// rest of the video engine.
    /// Which GL flavour `nativeCglContextHandle` actually is. Must be
    /// reported by the caller (which can see Qt's QSurfaceFormat) rather
    /// than assumed here: declaring a legacy 2.1 context as OpenGL3 makes
    /// gst_gl_context_fill_info() fail and silently disables this whole
    /// path, which is exactly what happened on the first re-enable
    /// attempt. Mixxx sets no version/profile, so macOS gives Qt a legacy
    /// compatibility context -- OpenGlLegacy is the real-world case here.
    enum class GlApi {
        OpenGlLegacy,
        OpenGl3,
        Gles2,
    };
    bool setSharedGlContext(quintptr nativeCglContextHandle, GlApi api);
    bool isGlPreviewAvailable() const {
        return m_pGlContext != nullptr;
    }

  private slots:
    void slotEnabledChanged(double value);
    // Stage 3x: invoked via QMetaObject::invokeMethod(..., Qt::QueuedConnection)
    // from a GStreamer pad-probe callback running on the streaming thread
    // (see the class doc comment's Stage 3x entry) -- must stay a plain
    // Qt slot reachable by name for that to work. Both are one-line
    // forwarders to handleDeckLooped(DeckVideoContext&); kept as two
    // distinct named slots (rather than one deck-parametrized slot)
    // because the string-based invokeMethod dispatch needs a real,
    // concretely-named Q_SLOT to look up.
    void slotDeckALooped();
    void slotDeckBLooped();

  private:
    // Stage 4: everything that used to be a pair of parallel m_deckA*/
    // m_deckB* members (there were about 15 such pairs, plus several
    // near-duplicate slot bodies) is now one struct, instantiated twice.
    // Splitting the single shared pipeline into two independent ones would
    // have doubled that parallel-member burden again (pipeline, appsink,
    // appsink pad, decode element, seek thread, seek worker context,
    // cached last frame) without this.
    struct DeckVideoContext {
        GstElement* pPipeline = nullptr;
        GstElement* pAppSink = nullptr;
        // EOS-loop probe attachment point (Stage 4 relocates it here from
        // the old shared compositor's sink pad -- see the class doc
        // comment's Stage 4 entry).
        GstPad* pAppSinkPad = nullptr;
        // Null when this deck has no real file loaded (the
        // videotestsrc/camera branches have no decodebin to seek).
        GstElement* pDecode = nullptr;
        // Stage 3z-10's serialized-worker-queue mechanism, now one per
        // deck instead of shared -- see seekElementAsync()'s comment for
        // why a shared queue would reintroduce needless coupling once
        // each deck has its own pipeline.
        QThread* pSeekThread = nullptr;
        QObject* pSeekWorkerContext = nullptr;
        std::unique_ptr<ControlProxy> pPlayControl;
        std::unique_ptr<ControlProxy> pPositionControl;
        // Stage 5: "[ChannelN]/rate_ratio" -- a normalized playback-speed
        // multiplier (1.0 = normal speed), already written by the pitch/
        // tempo slider, sync-lock, and (throttled, direction-stripped)
        // vinyl control. Deliberately not "[ChannelN]/rate", which is the
        // raw, untransformed slider position and means nothing on its own.
        std::unique_ptr<ControlProxy> pRateControl;
        // The last rate this deck's video was actually re-seeked to match
        // -- lets handleRateChanged() dedupe/throttle (skip reseeking on
        // every tiny CO tick) and detect a sign change (direction change,
        // e.g. through a backspin), which rate-tracking deliberately
        // doesn't attempt to follow.
        double lastAppliedRate = 1.0;
        QString videoPath;
        // Stage 3y: when true, this deck's pipeline uses a live
        // avfvideosrc camera feed instead of videoPath -- the loaded clip
        // path is deliberately NOT cleared when this is set, so turning
        // the camera back off (enabled=false) reverts to whatever clip
        // was already loaded rather than losing it.
        bool useCamera = false;
        bool playing = false;
        // Tracks this deck's own previously-seen audio position, purely
        // to detect a large single-tick jump (a click/hotcue/seek fired
        // while already playing) -- deliberately never compared against
        // GStreamer's own position (that comparison was Stage 3o's
        // mistake); this is only ever compared against itself, so it
        // can't carry the same systematic-offset failure mode.
        double lastAudioPosition = 0.0;
        std::unique_ptr<QTimer> pScrubDebounceTimer;
        // Stage 3w: counts position ticks accumulated since the last
        // commitScrubSeek(), so it can tell a single discrete tick (a
        // click, a cue jump -- exactly one tick before the debounce timer
        // fires) apart from an active drag (many ticks restarting the
        // timer before it finally fires).
        int scrubTicksSinceLastCommit = 0;
        // Stage 4: each deck's own pipeline only hands back a *fresh*
        // sample from its appsink when something new has actually landed
        // -- while genuinely paused, that's just the one preroll frame,
        // then nothing until stepOneFrameIfPaused() pulses it. Caching
        // the last real frame here (refreshed only when a new sample is
        // actually available) is what keeps grabPreviewFrame()'s blend
        // stable instead of flickering toward "100% other deck" on every
        // poll tick that doesn't get a fresh sample from this deck.
        QImage lastFrame;
        // Stage 4 (continuous drift correction): the video position and
        // audio position at the last known-good sync point -- reset every
        // time ANY re-sync happens (play-start re-anchor, a big-jump or
        // beatloop correction, initial clip load, or a continuous
        // correction itself). maybeCorrectContinuousDrift() only ever
        // compares how far each side has moved *since this anchor*, never
        // an absolute `audioPosition * videoDuration` recomputation -- see
        // that method's comment for why (Stage 3o's failure mode).
        qint64 anchorVideoPositionNs = 0;
        double anchorAudioPosition = 0.0;
        QElapsedTimer driftCheckThrottle;
    };

    void initDeckControls(DeckVideoContext& deck, const QString& channelGroup);
    void initDeckSeekWorker(DeckVideoContext& deck, const QString& threadName);
    void teardownDeckSeekWorker(DeckVideoContext& deck);
    void rebuildDeckPipeline(
            DeckVideoContext& deck, const QString& decodeName, const QString& sinkName);
    void teardownDeckPipeline(DeckVideoContext& deck);
    void applyPlayState(DeckVideoContext& deck);
    void handlePlayChanged(DeckVideoContext& deck, double value);
    // onLanded runs on this object's own thread (Qt::QueuedConnection, not
    // called directly from the background seek thread) once the seek
    // referenced by seekElementAsync() below has actually landed -- see
    // that method's comment for why this had to become callback-based
    // instead of the caller just continuing synchronously afterward.
    // Callers that don't need to run anything once the seek lands (most
    // of them) can omit it. waitNs/stepFrameAfter forward directly to
    // seekElementAsync() -- see its comment; handlePositionChanged()'s
    // beatloop-reset case (a caller that repeats far more often than the
    // "rare, discrete event" this method was originally written for, and
    // never touches applyPlayState() itself) is the one caller that
    // overrides either.
    void seekDeckToCurrentPosition(DeckVideoContext& deck,
            std::function<void()> onLanded = nullptr,
            qint64 waitNs = 1000000000,
            bool stepFrameAfter = true);
    // qint64, not GStreamer's own gint64 (binary-identical, both a 64-bit
    // signed int, but using gint64 here would require this header to
    // include a GLib/GStreamer header, breaking the opaque-pointer
    // pattern the rest of this header deliberately follows).
    // 1000000000 = 1 second in nanoseconds (GStreamer's own GST_SECOND
    // constant, spelled out numerically since this header deliberately
    // doesn't include any GStreamer header -- see the opaque-pointer
    // comment above).
    //
    // Stage 3z-9: async, not blocking. This used to block whatever thread
    // called it for up to waitNs via gst_element_get_state() -- and every
    // caller in this class runs on Qt's main/GUI thread (every
    // ControlProxy::connectValueChanged() callback and QTimer timeout in
    // this class resolves there). Real testing showed that had a much
    // bigger blast radius than intended: blocking that thread also blocks
    // the QML preview's own poll timer (so composited frames stop being
    // picked up -- looks like "the video doesn't loop/cue" even when the
    // seek itself landed correctly) and delays the *next* audio-engine
    // position-change delivery (so a scrub drag's own rapid small ticks
    // pile up behind the block and land discontinuously once it clears --
    // looks like "click/drag doesn't seek to where I actually clicked").
    // Both symptoms were independently reported by real testing and
    // traced, via [diag] logs, to this same root cause -- not a seek-flag
    // or wait-duration problem, a threading one. The fix: do the actual
    // seek + settle-wait + stepOneFrameIfPaused() on this deck's own
    // serialized worker thread (pure GstElement/GLib calls -- none of
    // them touch a Qt object, so thread affinity doesn't matter), then
    // hop back to this object's own thread via Qt::QueuedConnection only
    // to invoke onLanded, since that callback (e.g. applyPlayState())
    // does need to run where this class's other state is read/written.
    // The decode element and pipeline this call touches are
    // gst_object_ref()'d for the duration of the queued work, so a
    // concurrent rebuildDeckPipeline() call (which tears down and
    // replaces a deck's pipeline/decode element with fresh ones) can't
    // leave a torn-down element dangling underneath an in-flight seek.
    //
    // Stage 4: queues onto deck.pSeekWorkerContext (one per deck) rather
    // than a single shared worker -- see the DeckVideoContext member
    // comment for why sharing one across two now-fully-independent
    // pipelines would reintroduce needless coupling (e.g. deck B's
    // frequent continuous drift corrections queuing behind deck A's rapid
    // scrub-drag ticks for no structural reason).
    void seekElementAsync(DeckVideoContext& deck,
            qint64 targetNs,
            qint64 waitNs = 1000000000,
            bool stepFrameAfter = true,
            std::function<void()> onLanded = nullptr);
    void handlePositionChanged(DeckVideoContext& deck, double newPosition);
    // Stage 5: slaves this deck's video playback rate to the audio's own
    // tempo/pitch rate ("[ChannelN]/rate_ratio"), so a deck played faster
    // or slower than 1x genuinely plays its video at the matching speed
    // instead of relying on maybeCorrectContinuousDrift() to repeatedly
    // re-seek a video that's structurally always trying to play at 1x. A
    // standalone spike (see src/videoengine/CMakeLists.txt's Stage 5
    // history comment) confirmed GST_SEEK_FLAG_INSTANT_RATE_CHANGE -- the
    // mechanism that would let this happen with no flushing/glitch at all
    // -- is outright rejected by this pipeline's decodebin chain, so this
    // uses an ordinary flushing gst_element_seek() per rate change instead,
    // same cost tradeoff as this class's other flushing seeks, made
    // acceptable by throttling/deduping (skip re-seeking on every tiny CO
    // tick, only act on a change past a small epsilon). Deliberately does
    // NOT attempt to track a sign change (direction change, e.g. through a
    // backspin/reverse excursion) -- GStreamer's rate-seek mechanisms are
    // unreliable across a direction change regardless of flag, and reverse
    // decode support is inconsistent per-codec; a direction change falls
    // back to the existing position-based correction paths instead.
    void handleRateChanged(DeckVideoContext& deck, double newRate);
    // Stage 4: continuous, small-drift correction for ordinary playback --
    // handlePositionChanged()'s big-jump/backward-jump branches only ever
    // caught large, discrete position changes; between those events, a
    // deck's video free-ran on GStreamer's own wall-clock with nothing
    // correcting it. Anchor-and-delta design (see the DeckVideoContext
    // member comment): deliberately does NOT recompute an absolute
    // expected position from `audioPosition * videoDuration` on every
    // check -- that was Stage 3o's actual bug (a persistent per-file
    // audio/video duration mismatch became a permanent, non-decaying
    // offset that fired a disruptive re-seek roughly once a second,
    // forever, even during perfectly healthy playback). Comparing only
    // the delta since the last known-good anchor means a structural
    // duration mismatch can only ever contaminate one recent, bounded
    // window, never accumulate -- the anchor resets every time this (or
    // any other) correction lands.
    void maybeCorrectContinuousDrift(DeckVideoContext& deck);
    void commitScrubSeek(DeckVideoContext& deck);
    // Stage 4: takes the pipeline explicitly (rather than reading a class
    // member) now that each deck has its own -- the caller is responsible
    // for gst_object_ref()'ing it for the duration of this call (both
    // current callers already do, as part of the same ref taken for the
    // overall queued seek/step task -- see seekElementAsync() and
    // commitScrubSeek()'s drag branch).
    void stepOneFrameIfPaused(GstElement* pPipeline);
    void handleDeckLooped(DeckVideoContext& deck);

    bool m_available;
    std::unique_ptr<ControlPushButton> m_pEnabled;
    // Read on demand (in grabPreviewFrame()/blendFrames()), not subscribed
    // to -- since Stage 4 blends in C++ at grab time instead of writing
    // into a GStreamer compositor pad's "alpha" property, there's no
    // GStreamer object left to push a value into on every change, so a
    // live subscription serves no purpose anymore.
    std::unique_ptr<ControlProxy> m_pCrossfader;

    DeckVideoContext m_deckA;
    DeckVideoContext m_deckB;

    // Stage 7: null until setSharedGlContext() succeeds -- both null is the
    // permanent, safe default (GL preview simply unavailable, CPU path
    // used) if it's never called or fails. One shared pair for the whole
    // app, not per-deck, matching Qt Quick's own single top-level shared
    // GL context per process.
    GstGLDisplay* m_pGlDisplay = nullptr;
    /// GStreamer's OWN GL context, created sharing with (never equal to)
    /// the wrapped Qt one below. This is what the pipelines get.
    GstGLContext* m_pGlContext = nullptr;
    /// A non-owning GStreamer wrapper around Qt Quick's render-thread CGL
    /// context, used ONLY as the share-group source when creating
    /// m_pGlContext. It must never be handed to a pipeline: doing so
    /// makes GStreamer's streaming thread activate Qt's own context while
    /// Qt's render thread is also using it, and a CGL context is not safe
    /// to have current on two threads at once -- that was the cause of
    /// the GPU-level fault (a hard crash producing no macOS crash report,
    /// since it's not a Mach exception) seen when this path was first
    /// attempted.
    GstGLContext* m_pGlWrappedQtContext = nullptr;

#ifdef __VIDEO_ENGINE_NDI_OUTPUT__
    // Stage 6. Owned as a unique_ptr (rather than a plain value member) so
    // this header only needs a forward declaration, not NdiOutputSender's
    // own header, keeping the same "don't pull dependency headers into
    // videoenginemanager.h" discipline this file already follows for
    // GStreamer.
    std::unique_ptr<NdiOutputSender> m_pNdiSender;
#endif
};

} // namespace mixxx
