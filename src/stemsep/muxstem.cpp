#include "muxstem.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/opt.h>
} // extern "C"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>

namespace stemsep {

namespace {

constexpr int kNumStreams = 5; // 1 premix + 4 stems, matches SoundSourceSTEM::kNumStreams

struct AVFormatContextDeleter {
    void operator()(AVFormatContext* ctx) const {
        if (ctx) {
            if (ctx->pb) {
                avio_closep(&ctx->pb);
            }
            avformat_free_context(ctx);
        }
    }
};
using AVFormatContextPtr = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;

struct AVCodecContextDeleter {
    void operator()(AVCodecContext* ctx) const {
        avcodec_free_context(&ctx);
    }
};
using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;

struct AVFrameDeleter {
    void operator()(AVFrame* f) const {
        av_frame_free(&f);
    }
};
using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;

struct AVPacketDeleter {
    void operator()(AVPacket* p) const {
        av_packet_free(&p);
    }
};
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

struct EncodeStream {
    AVStream* stream = nullptr; // owned by the AVFormatContext
    AVCodecContextPtr codecCtx;
};

void throwOnError(int ret, const std::string& what) {
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf));
        throw std::runtime_error("stemsep: muxstem: " + what + ": " + errbuf);
    }
}

// Sends `frame` (nullptr to flush/drain) to the encoder and writes out every
// packet it produces. Reused for both the per-chunk encode loop and the
// final flush, so the two share identical error handling.
void encodeAndWrite(AVFormatContext* fmtCtx, EncodeStream& enc, AVFrame* frame) {
    int ret = avcodec_send_frame(enc.codecCtx.get(), frame);
    if (ret < 0 && !(frame == nullptr && ret == AVERROR_EOF)) {
        throwOnError(ret, "avcodec_send_frame");
    }

    AVPacketPtr pkt(av_packet_alloc());
    if (!pkt) {
        throw std::runtime_error("stemsep: muxstem: av_packet_alloc failed");
    }

    while (true) {
        ret = avcodec_receive_packet(enc.codecCtx.get(), pkt.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        throwOnError(ret, "avcodec_receive_packet");

        pkt->stream_index = enc.stream->index;
        av_packet_rescale_ts(pkt.get(), enc.codecCtx->time_base, enc.stream->time_base);
        // av_interleaved_write_frame() always takes ownership of the packet's
        // buffer and unreferences it before returning, success or failure --
        // `pkt` remains a valid, empty AVPacket* for the next iteration.
        ret = av_interleaved_write_frame(fmtCtx, pkt.get());
        throwOnError(ret, "av_interleaved_write_frame");
    }
}

// Copies `count` samples starting at `offset` from `buffer` (2 x N, one row
// per channel) into `frame`'s planar float data, zero-padding the remainder
// of the frame up to `frame->nb_samples` -- so every encoded AAC frame is
// full-length, keeping all 5 streams' encoded sample counts identical.
void fillFrame(AVFrame* frame, const Eigen::MatrixXf& buffer, int64_t offset, int64_t count) {
    auto* left = reinterpret_cast<float*>(frame->data[0]);
    auto* right = reinterpret_cast<float*>(frame->data[1]);
    for (int64_t i = 0; i < count; ++i) {
        left[i] = buffer(0, offset + i);
        right[i] = buffer(1, offset + i);
    }
    for (int64_t i = count; i < frame->nb_samples; ++i) {
        left[i] = 0.0f;
        right[i] = 0.0f;
    }
}

} // namespace

void muxStemContainer(
        const std::string& outPath,
        int sampleRate,
        const Eigen::MatrixXf& premix,
        const std::array<Eigen::MatrixXf, 4>& stems,
        int bitRatePerStreamBps) {
    if (premix.rows() != 2) {
        throw std::runtime_error("stemsep: muxstem: premix must be a 2-row (stereo) buffer");
    }
    const Eigen::Index numSamples = premix.cols();
    for (const auto& stem : stems) {
        if (stem.rows() != 2) {
            throw std::runtime_error("stemsep: muxstem: every stem must be a 2-row (stereo) buffer");
        }
        if (stem.cols() != numSamples) {
            throw std::runtime_error(
                    "stemsep: muxstem: every stem must have the same sample count as the premix");
        }
    }

    const std::array<const Eigen::MatrixXf*, kNumStreams> buffers = {
            &premix, &stems[0], &stems[1], &stems[2], &stems[3]};

    AVFormatContext* fmtCtxRaw = nullptr;
    throwOnError(
            avformat_alloc_output_context2(&fmtCtxRaw, nullptr, "mp4", outPath.c_str()),
            "avformat_alloc_output_context2");
    AVFormatContextPtr fmtCtx(fmtCtxRaw);
    if (!fmtCtx) {
        throw std::runtime_error("stemsep: muxstem: failed to allocate output context");
    }

    const AVCodec* enc = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!enc) {
        throw std::runtime_error("stemsep: muxstem: no AAC encoder available in this FFmpeg build");
    }

    std::array<EncodeStream, kNumStreams> streams;
    for (int i = 0; i < kNumStreams; ++i) {
        AVStream* st = avformat_new_stream(fmtCtx.get(), nullptr);
        if (!st) {
            throw std::runtime_error("stemsep: muxstem: avformat_new_stream failed");
        }

        AVCodecContextPtr cctx(avcodec_alloc_context3(enc));
        if (!cctx) {
            throw std::runtime_error("stemsep: muxstem: avcodec_alloc_context3 failed");
        }
        cctx->sample_rate = sampleRate;
        cctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
        cctx->bit_rate = bitRatePerStreamBps;
        cctx->time_base = AVRational{1, sampleRate};
        av_channel_layout_default(&cctx->ch_layout, 2);
        if (fmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
            cctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        throwOnError(avcodec_open2(cctx.get(), enc, nullptr), "avcodec_open2");
        throwOnError(
                avcodec_parameters_from_context(st->codecpar, cctx.get()),
                "avcodec_parameters_from_context");
        st->time_base = cctx->time_base;

        streams[i].stream = st;
        streams[i].codecCtx = std::move(cctx);
    }

    throwOnError(
            avio_open(&fmtCtx->pb, outPath.c_str(), AVIO_FLAG_WRITE), "avio_open");
    // Deliberately no movflags=faststart / frag_* -- keeps moov as the last
    // top-level box, which stematom.cpp's patcher requires.
    throwOnError(avformat_write_header(fmtCtx.get(), nullptr), "avformat_write_header");

    const int frameSize = streams[0].codecCtx->frame_size;
    if (frameSize <= 0) {
        throw std::runtime_error("stemsep: muxstem: AAC encoder reported an invalid frame_size");
    }

    for (Eigen::Index chunkStart = 0; chunkStart < numSamples; chunkStart += frameSize) {
        const int64_t count = std::min<int64_t>(frameSize, numSamples - chunkStart);
        for (int i = 0; i < kNumStreams; ++i) {
            AVFramePtr frame(av_frame_alloc());
            if (!frame) {
                throw std::runtime_error("stemsep: muxstem: av_frame_alloc failed");
            }
            frame->nb_samples = frameSize;
            frame->format = streams[i].codecCtx->sample_fmt;
            frame->sample_rate = sampleRate;
            throwOnError(
                    av_channel_layout_copy(&frame->ch_layout, &streams[i].codecCtx->ch_layout),
                    "av_channel_layout_copy");
            throwOnError(av_frame_get_buffer(frame.get(), 0), "av_frame_get_buffer");
            throwOnError(av_frame_make_writable(frame.get()), "av_frame_make_writable");

            fillFrame(frame.get(), *buffers[i], chunkStart, count);
            frame->pts = chunkStart;

            encodeAndWrite(fmtCtx.get(), streams[i], frame.get());
        }
    }

    for (auto& enc : streams) {
        encodeAndWrite(fmtCtx.get(), enc, nullptr);
    }

    throwOnError(av_write_trailer(fmtCtx.get()), "av_write_trailer");
}

} // namespace stemsep
