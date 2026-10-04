#pragma once
#include "renderer.h"
#include "shader_editor.h"
#include "image_loader.h"
#include "uniforms.h"
#include "gif_exporter.h"
#include <array>
#include <string>

class App {
public:
    void init();
    void render(float time, float mouseX, float mouseY);
    void cleanup();

    void loadImage(int channel);
    void startExport(bool isGif);
    void stopExport();
    void openShader();
    void saveShader(bool saveAs);

private:
    Renderer m_renderer;
    ShaderEditor m_shaderEditor;
    ImageLoader m_imageLoader;
    Uniforms m_uniforms;
    GifExporter m_exporter;

    std::array<GLuint, 4> m_textures = {0, 0, 0, 0};
    std::string m_shaderPath;
    std::string m_statusMessage;
    float m_rightPanelWidth = 500.0f;
    float m_editorHeight = 560.0f;
    float m_debounceMs = 500.0f;
    int m_previewWidth = 1280;
    int m_previewHeight = 720;
    float m_currentTime = 0.0f;
    float m_lastExportCaptureTime = -1.0f;
    bool m_needsRecompile = true;
    bool m_isExporting = false;
    bool m_autoCompile = true;
    bool m_showSettings = false;
    bool m_showAbout = false;
};
