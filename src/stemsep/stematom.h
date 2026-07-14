#pragma once

#include <string>

namespace stemsep {

// Patches an already-muxed MP4/M4A file (see muxstem.h) to inject a
// moov/udta/stem box containing `manifestJson` as its raw payload -- the
// proprietary atom mixxx::StemInfoImporter (src/track/steminfoimporter.cpp)
// reads on the load side. FFmpeg's muxer API has no public mechanism to
// write an arbitrary named box under udta, so this is a post-processing
// binary patch.
//
// Precondition (checked, throws std::runtime_error if violated): `moov`
// must be the last top-level box in the file, with no bytes following it
// (i.e. the file must not have been muxed with movflags=faststart or any
// fragmented-mp4 flags). This lets the patch be a pure append-and-replace
// of moov's own bytes -- rewriting only moov's (and udta's, if nested) size
// field and truncating/rewriting the file tail from moov's start offset --
// never touching mdat or any stco/co64 chunk-offset table elsewhere in
// moov, which would require a much harder file-wide offset rewrite.
// muxStemContainer() guarantees this precondition by construction; it is
// re-verified here defensively rather than silently falling back to a
// general mid-file-splice algorithm.
void patchStemAtom(const std::string& mp4Path, const std::string& manifestJson);

} // namespace stemsep
