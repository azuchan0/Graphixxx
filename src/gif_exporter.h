#pragma once
#include <string>
#include <vector>
#include <glad/glad.h>

struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

class GifExporter {
public:
    bool startExport(const std::string& outputPath, int width, int height,
        int sourceWidth, int sourceHeight, int fps, bool isGif);
    void addFrame(GLuint fboTexture, int cropX, int cropY, int cropWidth, int cropHeight);
    void finishExport();
    std::string getLastError() const;

private:
    void releaseResources();

    std::string m_lastError;
    bool m_exporting = false;
    
    AVFormatContext* m_formatCtx = nullptr;
    AVCodecContext* m_codecCtx = nullptr;
    AVFrame* m_frame = nullptr;
    AVPacket* m_packet = nullptr;
    SwsContext* m_swsCtx = nullptr;
    
    std::vector<uint8_t> m_sourceBuffer;
    std::vector<uint8_t> m_cropBuffer;
    
    int m_width = 0;
    int m_height = 0;
    int m_framePtsStep = 1;
    int m_frameCount = 0;
};
