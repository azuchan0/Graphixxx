#include "gif_exporter.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

namespace {
std::string ffmpegErrorString(int errorCode) {
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(errorCode, buffer, sizeof(buffer));
    return buffer;
}

bool chooseSoftwarePixelFormat(const AVCodec* codec, AVPixelFormat& pixelFormat) {
    const void* supportedConfigs = nullptr;
    int supportedCount = 0;
    const int result = avcodec_get_supported_config(nullptr, codec,
        AV_CODEC_CONFIG_PIX_FORMAT, 0, &supportedConfigs, &supportedCount);

    if (result < 0 || !supportedConfigs) {
        pixelFormat = AV_PIX_FMT_YUV420P;
        return true;
    }

    const auto* formats = static_cast<const AVPixelFormat*>(supportedConfigs);
    const AVPixelFormat preferredFormats[] = {
        AV_PIX_FMT_YUV420P,
        AV_PIX_FMT_NV12,
        AV_PIX_FMT_YUV444P,
        AV_PIX_FMT_YUV422P
    };
    for (AVPixelFormat preferred : preferredFormats) {
        for (int i = 0; i < supportedCount; ++i) {
            if (formats[i] == preferred) {
                pixelFormat = preferred;
                return true;
            }
        }
    }
    return false;
}

}

bool GifExporter::startExport(const std::string& outputPath, int width, int height,
    int sourceWidth, int sourceHeight, int fps, bool isGif) {
    releaseResources();
    m_lastError.clear();

    if (width <= 0 || height <= 0 || sourceWidth <= 0 || sourceHeight <= 0 || fps <= 0) {
        m_lastError = "Invalid export dimensions or frame rate";
        return false;
    }

    m_width = width;
    m_height = height;
    m_frameCount = 0;
    m_framePtsStep = isGif ? std::max(1, 100 / fps) : 1;

    auto fail = [this](const std::string& message) {
        m_lastError = message;
        releaseResources();
        return false;
    };

    const char* formatName = isGif ? "gif" : "mp4";
    int result = avformat_alloc_output_context2(&m_formatCtx, nullptr, formatName, outputPath.c_str());
    if (result < 0 || !m_formatCtx) {
        return fail("Could not create output context: " + ffmpegErrorString(result));
    }

    const AVCodec* codec = nullptr;
    AVPixelFormat outputPixelFormat = AV_PIX_FMT_RGB8;
    if (isGif) {
        codec = avcodec_find_encoder(AV_CODEC_ID_GIF);
    } else {
        const char* softwareEncoderNames[] = { "libx264", "libopenh264", "h264_mf" };
        for (const char* encoderName : softwareEncoderNames) {
            const AVCodec* candidate = avcodec_find_encoder_by_name(encoderName);
            if (candidate && chooseSoftwarePixelFormat(candidate, outputPixelFormat)) {
                codec = candidate;
                break;
            }
        }
    }
    if (!codec) {
        return fail(isGif
            ? "GIF encoder not found"
            : "No compatible software H.264 encoder found. Enable libx264 in the FFmpeg build.");
    }

    m_codecCtx = avcodec_alloc_context3(codec);
    if (!m_codecCtx) {
        return fail("Could not allocate codec context");
    }

    if (!isGif) {
        m_width = (width + 1) & ~1;
        m_height = (height + 1) & ~1;
    }

    m_codecCtx->width = m_width;
    m_codecCtx->height = m_height;
    m_codecCtx->time_base = AVRational{ 1, isGif ? 100 : fps };
    m_codecCtx->framerate = AVRational{ fps, 1 };
    m_codecCtx->pix_fmt = outputPixelFormat;
    m_codecCtx->gop_size = isGif ? 1 : fps * 2;
    m_codecCtx->max_b_frames = 0;
    m_codecCtx->sample_aspect_ratio = AVRational{ 1, 1 };

    if (!isGif) {
        const int64_t targetBitrate = std::max<int64_t>(
            4'000'000,
            static_cast<int64_t>(m_width) * m_height * fps * 12 / 100);
        m_codecCtx->bit_rate = targetBitrate;

        if (codec->name && std::strcmp(codec->name, "libx264") == 0) {
            av_opt_set(m_codecCtx->priv_data, "preset", "medium", 0);
            av_opt_set(m_codecCtx->priv_data, "crf", "17", 0);
        } else {
            m_codecCtx->rc_max_rate = targetBitrate;
            m_codecCtx->rc_buffer_size = static_cast<int>(std::min<int64_t>(
                targetBitrate * 2, std::numeric_limits<int>::max()));
        }
    }
    if (m_formatCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        m_codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    result = avcodec_open2(m_codecCtx, codec, nullptr);
    if (result < 0) {
        char pixelFormatName[64] = {};
        av_get_pix_fmt_string(pixelFormatName, sizeof(pixelFormatName), m_codecCtx->pix_fmt);
        const std::string codecName = codec->name ? codec->name : "unknown";
        return fail("Could not open encoder '" + codecName + "' for " +
            std::to_string(m_width) + "x" + std::to_string(m_height) + " using " +
            pixelFormatName + ": " + ffmpegErrorString(result));
    }

    AVStream* stream = avformat_new_stream(m_formatCtx, nullptr);
    if (!stream) {
        return fail("Could not create output stream");
    }
    stream->time_base = m_codecCtx->time_base;
    result = avcodec_parameters_from_context(stream->codecpar, m_codecCtx);
    if (result < 0) {
        return fail("Could not copy encoder parameters: " + ffmpegErrorString(result));
    }

    if (!(m_formatCtx->oformat->flags & AVFMT_NOFILE)) {
        result = avio_open(&m_formatCtx->pb, outputPath.c_str(), AVIO_FLAG_WRITE);
        if (result < 0) {
            return fail("Could not open output file: " + ffmpegErrorString(result));
        }
    }

    result = avformat_write_header(m_formatCtx, nullptr);
    if (result < 0) {
        return fail("Could not write output header: " + ffmpegErrorString(result));
    }

    m_frame = av_frame_alloc();
    m_packet = av_packet_alloc();
    if (!m_frame || !m_packet) {
        return fail("Could not allocate encoder frame or packet");
    }

    m_frame->format = m_codecCtx->pix_fmt;
    m_frame->width = m_width;
    m_frame->height = m_height;
    result = av_frame_get_buffer(m_frame, 32);
    if (result < 0) {
        return fail("Could not allocate encoder frame data: " + ffmpegErrorString(result));
    }

    try {
        m_sourceBuffer.resize(static_cast<size_t>(sourceWidth) * sourceHeight * 4);
        m_cropBuffer.resize(static_cast<size_t>(width) * height * 4);
    } catch (const std::bad_alloc&) {
        return fail("Could not allocate frame capture buffers");
    }

    m_exporting = true;
    printf("[GifExporter] Recording with %s at %dx%d, %d fps, bitrate %lld to %s\n",
        codec->name ? codec->name : "unknown", m_width, m_height, fps,
        static_cast<long long>(m_codecCtx->bit_rate), outputPath.c_str());
    return true;
}

void GifExporter::addFrame(GLuint fboTexture, int cropX, int cropY, int cropWidth, int cropHeight) {
    if (!m_exporting || !fboTexture || cropWidth <= 0 || cropHeight <= 0) {
        return;
    }

    GLint previousActiveTexture = GL_TEXTURE0;
    GLint previousBinding = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousBinding);
    glBindTexture(GL_TEXTURE_2D, fboTexture);

    GLint sourceWidth = 0;
    GLint sourceHeight = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &sourceWidth);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &sourceHeight);
    if (sourceWidth <= 0 || sourceHeight <= 0) {
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousBinding));
        glActiveTexture(static_cast<GLenum>(previousActiveTexture));
        m_lastError = "Could not read the preview texture dimensions";
        return;
    }

    try {
        m_sourceBuffer.resize(static_cast<size_t>(sourceWidth) * sourceHeight * 4);
    } catch (const std::bad_alloc&) {
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousBinding));
        glActiveTexture(static_cast<GLenum>(previousActiveTexture));
        m_lastError = "Could not allocate a source frame buffer";
        return;
    }
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_sourceBuffer.data());
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousBinding));
    glActiveTexture(static_cast<GLenum>(previousActiveTexture));

    cropX = std::clamp(cropX, 0, sourceWidth - 1);
    cropY = std::clamp(cropY, 0, sourceHeight - 1);
    cropWidth = std::min(cropWidth, sourceWidth - cropX);
    cropHeight = std::min(cropHeight, sourceHeight - cropY);
    if (cropWidth <= 0 || cropHeight <= 0) {
        return;
    }

    try {
        m_cropBuffer.resize(static_cast<size_t>(cropWidth) * cropHeight * 4);
    } catch (const std::bad_alloc&) {
        m_lastError = "Could not allocate a cropped frame buffer";
        return;
    }

    for (int y = 0; y < cropHeight; ++y) {
        const int sourceY = cropY + cropHeight - 1 - y;
        const uint8_t* sourceRow = m_sourceBuffer.data() +
            (static_cast<size_t>(sourceY) * sourceWidth + cropX) * 4;
        uint8_t* cropRow = m_cropBuffer.data() + static_cast<size_t>(y) * cropWidth * 4;
        std::memcpy(cropRow, sourceRow, static_cast<size_t>(cropWidth) * 4);
    }

    m_swsCtx = sws_getCachedContext(m_swsCtx,
        cropWidth, cropHeight, AV_PIX_FMT_RGBA,
        m_width, m_height, m_codecCtx->pix_fmt,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!m_swsCtx) {
        m_lastError = "Could not create the pixel conversion context";
        return;
    }

    int result = av_frame_make_writable(m_frame);
    if (result < 0) {
        m_lastError = "Encoder frame is not writable: " + ffmpegErrorString(result);
        return;
    }

    const uint8_t* sourceData[4] = { m_cropBuffer.data(), nullptr, nullptr, nullptr };
    const int sourceLineSize[4] = { cropWidth * 4, 0, 0, 0 };
    result = sws_scale(m_swsCtx, sourceData, sourceLineSize, 0, cropHeight,
        m_frame->data, m_frame->linesize);
    if (result <= 0) {
        m_lastError = "Pixel conversion failed";
        return;
    }

    m_frame->pts = static_cast<int64_t>(m_frameCount) * m_framePtsStep;
    result = avcodec_send_frame(m_codecCtx, m_frame);
    if (result < 0) {
        m_lastError = "Could not submit frame to encoder: " + ffmpegErrorString(result);
        return;
    }

    while (true) {
        result = avcodec_receive_packet(m_codecCtx, m_packet);
        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) {
            break;
        }
        if (result < 0) {
            m_lastError = "Could not encode frame: " + ffmpegErrorString(result);
            return;
        }

        av_packet_rescale_ts(m_packet, m_codecCtx->time_base, m_formatCtx->streams[0]->time_base);
        m_packet->stream_index = 0;
        result = av_interleaved_write_frame(m_formatCtx, m_packet);
        av_packet_unref(m_packet);
        if (result < 0) {
            m_lastError = "Could not write encoded frame: " + ffmpegErrorString(result);
            return;
        }
    }

    ++m_frameCount;
}

void GifExporter::finishExport() {
    if (!m_exporting) {
        releaseResources();
        return;
    }

    int result = avcodec_send_frame(m_codecCtx, nullptr);
    if (result < 0 && result != AVERROR_EOF) {
        m_lastError = "Could not flush encoder: " + ffmpegErrorString(result);
    }

    while (m_lastError.empty()) {
        result = avcodec_receive_packet(m_codecCtx, m_packet);
        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) {
            break;
        }
        if (result < 0) {
            m_lastError = "Could not finish encoding: " + ffmpegErrorString(result);
            break;
        }

        av_packet_rescale_ts(m_packet, m_codecCtx->time_base, m_formatCtx->streams[0]->time_base);
        m_packet->stream_index = 0;
        result = av_interleaved_write_frame(m_formatCtx, m_packet);
        av_packet_unref(m_packet);
        if (result < 0) {
            m_lastError = "Could not write final encoded frame: " + ffmpegErrorString(result);
            break;
        }
    }

    if (m_formatCtx) {
        result = av_write_trailer(m_formatCtx);
        if (result < 0 && m_lastError.empty()) {
            m_lastError = "Could not finalize output file: " + ffmpegErrorString(result);
        }
    }

    printf("[GifExporter] Finished recording %d frames%s\n", m_frameCount,
        m_lastError.empty() ? "" : " with an error");
    releaseResources();
}

void GifExporter::releaseResources() {
    m_exporting = false;
    if (m_formatCtx && m_formatCtx->pb &&
        !(m_formatCtx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&m_formatCtx->pb);
    }
    if (m_swsCtx) sws_freeContext(m_swsCtx);
    if (m_frame) av_frame_free(&m_frame);
    if (m_packet) av_packet_free(&m_packet);
    if (m_codecCtx) avcodec_free_context(&m_codecCtx);
    if (m_formatCtx) avformat_free_context(m_formatCtx);
    m_swsCtx = nullptr;
    m_frame = nullptr;
    m_packet = nullptr;
    m_codecCtx = nullptr;
    m_formatCtx = nullptr;
    m_sourceBuffer.clear();
    m_cropBuffer.clear();
}

std::string GifExporter::getLastError() const {
    return m_lastError;
}
