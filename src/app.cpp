#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include "app.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>

#pragma comment(lib, "Comdlg32.lib")

namespace {
std::string ansiPathToUtf8(const char* path) {
    if (!path || !*path) {
        return {};
    }

    const int wideLength = MultiByteToWideChar(CP_ACP, 0, path, -1, nullptr, 0);
    if (wideLength <= 0) {
        return path;
    }

    std::wstring widePath(static_cast<size_t>(wideLength), L'\0');
    MultiByteToWideChar(CP_ACP, 0, path, -1, widePath.data(), wideLength);

    const int utf8Length = WideCharToMultiByte(CP_UTF8, 0, widePath.c_str(), -1,
        nullptr, 0, nullptr, nullptr);
    if (utf8Length <= 0) {
        return path;
    }

    std::string utf8Path(static_cast<size_t>(utf8Length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, widePath.c_str(), -1,
        utf8Path.data(), utf8Length, nullptr, nullptr);
    utf8Path.pop_back();
    return utf8Path;
}
}

void App::init() {
    printf("[App] Initializing renderer...\n");
    m_renderer.init();
    printf("[App] Renderer initialized\n");

    printf("[App] Initializing shader editor...\n");
    m_shaderEditor.init();
    printf("[App] Shader editor initialized\n");

    printf("[App] Parsing initial uniforms...\n");
    m_uniforms.parse(m_shaderEditor.getCode());
    printf("[App] Initial uniforms parsed\n");
}

void App::render(float time, float mouseX, float mouseY) {
    m_currentTime = time;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open Shader...", "Ctrl+O")) {
                openShader();
            }
            if (ImGui::MenuItem("Save Shader", "Ctrl+S")) {
                saveShader(false);
            }
            if (ImGui::MenuItem("Save Shader As...", "Ctrl+Shift+S")) {
                saveShader(true);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Export GIF...")) {
                startExport(true);
            }
            if (ImGui::MenuItem("Export MP4...")) {
                startExport(false);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                glfwSetWindowShouldClose(glfwGetCurrentContext(), GLFW_TRUE);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Settings")) {
            if (ImGui::MenuItem("Preferences...")) {
                m_showSettings = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("About")) {
            if (ImGui::MenuItem("About Graphixxx...")) {
                m_showAbout = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O)) {
        openShader();
    }
    if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S)) {
        saveShader(true);
    } else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
        saveShader(false);
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 workPos = viewport->WorkPos;
    ImVec2 workSize = viewport->WorkSize;
    const float maxPanelWidth = std::max(250.0f, workSize.x - 180.0f);
    m_rightPanelWidth = std::clamp(m_rightPanelWidth, 250.0f, maxPanelWidth);

    ImGui::SetNextWindowPos(ImVec2(workPos.x + workSize.x - m_rightPanelWidth, workPos.y));
    ImGui::SetNextWindowSize(ImVec2(m_rightPanelWidth, workSize.y));
    ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    const float maxEditorHeight = std::max(250.0f, workSize.y - 220.0f);
    m_editorHeight = std::clamp(m_editorHeight, 250.0f, maxEditorHeight);
    m_shaderEditor.render(m_editorHeight);

    ImVec2 editorSplitterSize = ImGui::GetContentRegionAvail();
    editorSplitterSize.y = 8.0f;
    ImGui::InvisibleButton("##EditorHeightSplitter", editorSplitterSize);
    const bool editorSplitterHovered = ImGui::IsItemHovered();
    const bool editorSplitterActive = ImGui::IsItemActive();
    const ImVec2 editorSplitterMin = ImGui::GetItemRectMin();
    const ImVec2 editorSplitterMax = ImGui::GetItemRectMax();
    if (editorSplitterHovered || editorSplitterActive) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
    if (editorSplitterActive) {
        m_editorHeight = std::clamp(m_editorHeight + ImGui::GetIO().MouseDelta.y,
            250.0f, maxEditorHeight);
    }
    ImGui::GetWindowDrawList()->AddRectFilled(editorSplitterMin, editorSplitterMax,
        editorSplitterActive ? IM_COL32(110, 175, 235, 255) :
        editorSplitterHovered ? IM_COL32(85, 95, 110, 255) : IM_COL32(55, 60, 70, 255));

    if (m_needsRecompile || m_shaderEditor.hasImmediateCompileRequest() ||
        (m_autoCompile && m_shaderEditor.hasCodeChanged())) {
        const std::string code = m_shaderEditor.getCode();
        const bool compiled = m_renderer.compileShader(code);
        m_shaderEditor.markCompileAttempted();
        if (compiled) {
            m_shaderEditor.setError("");
            m_uniforms.parse(code);
            m_needsRecompile = false;
            m_statusMessage = "Shader compiled";
        } else {
            m_shaderEditor.setError(m_renderer.getLastError());
            m_statusMessage = "Shader compilation failed";
            printf("[App] Shader compile ERROR: %s\n", m_renderer.getLastError().c_str());
        }
    }

    ImGui::Separator();
    m_uniforms.renderUI();

    ImGui::Separator();
    for (int i = 0; i < 4; ++i) {
        std::string label = "Load Image (iChannel" + std::to_string(i) + ")";
        if (ImGui::Button(label.c_str())) {
            loadImage(i);
        }
        if (m_textures[i]) {
            ImGui::SameLine();
            ImGui::Text("Loaded");
        }
    }
    ImGui::Separator();
    if (m_isExporting) {
        if (ImGui::Button("Stop Recording")) {
            stopExport();
        }
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Recording...");
    } else {
        if (ImGui::Button("Export GIF")) {
            startExport(true);
        }
        ImGui::SameLine();
        if (ImGui::Button("Export MP4")) {
            startExport(false);
        }
    }

    if (!m_statusMessage.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", m_statusMessage.c_str());
    }

    ImGui::End();

    ImGui::SetNextWindowPos(workPos);
    ImGui::SetNextWindowSize(ImVec2(workSize.x - m_rightPanelWidth, workSize.y));
    ImGui::Begin("Preview", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    ImVec2 previewSize = ImGui::GetContentRegionAvail();
    m_previewWidth = std::max(2, static_cast<int>(previewSize.x)) & ~1;
    m_previewHeight = std::max(2, static_cast<int>(previewSize.y)) & ~1;
    m_renderer.setResolution(m_previewWidth, m_previewHeight);
    m_renderer.setCustomUniforms(m_uniforms.getParams());
    m_renderer.render(time, mouseX, mouseY);

    const GLuint texId = m_renderer.getOutputTexture();
    if (texId) {
        int regionX, regionY, regionWidth, regionHeight;
        m_renderer.getRenderRegion(regionX, regionY, regionWidth, regionHeight);

        const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
        const ImVec2 canvasMax(canvasMin.x + previewSize.x, canvasMin.y + previewSize.y);
        ImGui::GetWindowDrawList()->AddRectFilled(canvasMin, canvasMax, IM_COL32(0, 0, 0, 255));

        const ImVec2 imageSize(static_cast<float>(regionWidth), static_cast<float>(regionHeight));
        const ImVec2 imagePos(
            canvasMin.x + (previewSize.x - imageSize.x) * 0.5f,
            canvasMin.y + (previewSize.y - imageSize.y) * 0.5f);
        const ImVec2 uv0(
            static_cast<float>(regionX) / static_cast<float>(m_previewWidth),
            static_cast<float>(regionY + regionHeight) / static_cast<float>(m_previewHeight));
        const ImVec2 uv1(
            static_cast<float>(regionX + regionWidth) / static_cast<float>(m_previewWidth),
            static_cast<float>(regionY) / static_cast<float>(m_previewHeight));

        ImGui::SetCursorScreenPos(imagePos);
        ImGui::Image(reinterpret_cast<void*>(static_cast<intptr_t>(texId)), imageSize, uv0, uv1);
        ImGui::SetCursorScreenPos(canvasMin);
        ImGui::Dummy(previewSize);
        if (m_isExporting && (m_lastExportCaptureTime < 0.0f ||
            time - m_lastExportCaptureTime >= 1.0f / 30.0f)) {
            m_exporter.addFrame(texId, regionX, regionY, regionWidth, regionHeight);
            m_lastExportCaptureTime = time;
        }
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(workPos.x + workSize.x - m_rightPanelWidth - 4.0f, workPos.y));
    ImGui::SetNextWindowSize(ImVec2(8.0f, workSize.y));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const ImGuiWindowFlags splitterFlags = ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNavFocus;
    ImGui::Begin("##PreviewControlsSplitter", nullptr, splitterFlags);
    ImGui::InvisibleButton("##DragPreviewControls", ImVec2(8.0f, workSize.y));
    const bool panelSplitterHovered = ImGui::IsItemHovered();
    const bool panelSplitterActive = ImGui::IsItemActive();
    const ImVec2 panelSplitterMin = ImGui::GetItemRectMin();
    const ImVec2 panelSplitterMax = ImGui::GetItemRectMax();
    if (panelSplitterHovered || panelSplitterActive) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    if (panelSplitterActive) {
        m_rightPanelWidth = std::clamp(m_rightPanelWidth - ImGui::GetIO().MouseDelta.x,
            250.0f, maxPanelWidth);
    }
    ImGui::GetWindowDrawList()->AddRectFilled(panelSplitterMin, panelSplitterMax,
        panelSplitterActive ? IM_COL32(110, 175, 235, 255) :
        panelSplitterHovered ? IM_COL32(85, 95, 110, 255) : IM_COL32(55, 60, 70, 255));
    ImGui::End();
    ImGui::PopStyleVar();

    if (m_showSettings) {
        ImGui::SetNextWindowSize(ImVec2(380.0f, 190.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Settings", &m_showSettings)) {
            ImGui::Checkbox("Auto-compile shader", &m_autoCompile);
            ImGui::BeginDisabled(!m_autoCompile);
            ImGui::SliderFloat("Compile delay (ms)", &m_debounceMs, 100.0f, 2000.0f, "%.0f");
            m_shaderEditor.setDebounceMs(m_debounceMs);
            ImGui::EndDisabled();
            ImGui::SliderFloat("Right panel width", &m_rightPanelWidth, 250.0f,
                std::max(250.0f, workSize.x - 180.0f), "%.0f px");
        }
        ImGui::End();
    }

    if (m_showAbout) {
        ImGui::SetNextWindowSize(ImVec2(340.0f, 120.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("About Graphixxx", &m_showAbout, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Graphixxx");
            ImGui::Separator();
            ImGui::Text("by Abdel-Aziz El Kihal (azuchan0)");
            ImGui::Text("Heaven Software Foundation");
        }
        ImGui::End();
    }
}

void App::cleanup() {
    if (m_isExporting) {
        stopExport();
    }
    for (int i = 0; i < 4; ++i) {
        if (m_textures[i]) {
            m_imageLoader.freeTexture(m_textures[i]);
            m_textures[i] = 0;
        }
    }
    m_renderer.cleanup();
}

void App::loadImage(int channel) {
    char filename[MAX_PATH] = "";
    
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "Image Files\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = "Load Image";
    
    if (GetOpenFileNameA(&ofn)) {
        if (m_textures[channel]) {
            m_imageLoader.freeTexture(m_textures[channel]);
        }
        
        GLuint texId = m_imageLoader.loadTexture(filename);
        if (texId) {
            m_textures[channel] = texId;
            m_renderer.setTexture(channel, texId);
            printf("[App] Loaded image to channel %d: %s\n", channel, filename);
        } else {
            printf("[App] Failed to load image: %s\n", filename);
        }
    }
}

void App::startExport(bool isGif) {
    char filename[MAX_PATH] = "";
    
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = isGif ? "GIF Files\0*.gif\0" : "MP4 Files\0*.mp4\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = isGif ? "gif" : "mp4";
    ofn.lpstrTitle = "Export Video";
    
    if (GetSaveFileNameA(&ofn)) {
        const std::string ffmpegPath = ansiPathToUtf8(filename);
        int cropX = 0;
        int cropY = 0;
        int cropWidth = m_previewWidth;
        int cropHeight = m_previewHeight;
        m_renderer.getRenderRegion(cropX, cropY, cropWidth, cropHeight);

        if (m_exporter.startExport(ffmpegPath, cropWidth, cropHeight,
            m_previewWidth, m_previewHeight, 30, isGif)) {
            m_isExporting = true;
            m_lastExportCaptureTime = m_currentTime - (1.0f / 30.0f);
            m_statusMessage = std::string("Recording to ") + filename;
        } else {
            m_statusMessage = std::string("Export failed: ") + m_exporter.getLastError();
        }
    }
}

void App::stopExport() {
    if (m_isExporting) {
        m_exporter.finishExport();
        m_isExporting = false;
        m_lastExportCaptureTime = -1.0f;
        m_statusMessage = m_exporter.getLastError().empty()
            ? "Export finished"
            : std::string("Export error: ") + m_exporter.getLastError();
    }
}

void App::openShader() {
    char filename[MAX_PATH] = "";
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "GLSL Shader Files\0*.glsl;*.frag;*.txt\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = "Open Shader";

    if (!GetOpenFileNameA(&ofn)) {
        return;
    }

    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        m_statusMessage = "Could not open shader file";
        return;
    }

    const std::string code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    m_shaderEditor.setCode(code);
    m_shaderPath = filename;
    m_needsRecompile = true;
    m_statusMessage = std::string("Loaded ") + filename;
}

void App::saveShader(bool saveAs) {
    if (m_shaderPath.empty() || saveAs) {
        char filename[MAX_PATH] = "";
        if (!m_shaderPath.empty()) {
            const size_t copyLength = std::min(m_shaderPath.size(), sizeof(filename) - 1);
            std::memcpy(filename, m_shaderPath.data(), copyLength);
            filename[copyLength] = '\0';
        }

        OPENFILENAMEA ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFilter = "GLSL Shader Files\0*.glsl\0All Files\0*.*\0";
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
        ofn.lpstrDefExt = "glsl";
        ofn.lpstrTitle = "Save Shader As";

        if (!GetSaveFileNameA(&ofn)) {
            return;
        }
        m_shaderPath = filename;
    }

    std::ofstream file(m_shaderPath, std::ios::binary);
    if (!file) {
        m_statusMessage = "Could not save shader file";
        return;
    }

    const std::string code = m_shaderEditor.getCode();
    file.write(code.data(), static_cast<std::streamsize>(code.size()));
    if (!file) {
        m_statusMessage = "Error writing shader file";
        return;
    }

    m_statusMessage = std::string("Saved ") + m_shaderPath;
}
