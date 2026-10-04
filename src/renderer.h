#pragma once
#include <glad/glad.h>
#include <string>
#include <array>
#include <vector>

struct UniformParam;

class Renderer {
public:
    void init();
    void render(float time, float mouseX, float mouseY);
    void cleanup();

    bool compileShader(const std::string& userCode);
    std::string getLastError() const;

    void setTexture(int channel, GLuint texId);
    void setResolution(int width, int height);
    void setCustomUniforms(const std::vector<UniformParam>& params);

    GLuint getOutputTexture() const;
    void getRenderRegion(int& x, int& y, int& width, int& height) const;

private:
    void createFramebuffer(int width, int height);
    void deleteFramebuffer();
    void clearFeedbackHistory();

    GLuint m_program = 0;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    std::array<GLuint, 4> m_textures = {0, 0, 0, 0};
    std::array<int, 4> m_textureWidths = {0, 0, 0, 0};
    std::array<int, 4> m_textureHeights = {0, 0, 0, 0};
    int m_width = 0;
    int m_height = 0;
    std::string m_lastError;

    GLuint m_fbo = 0;
    GLuint m_fboTexture = 0;
    GLuint m_feedbackTexture = 0;
    int m_renderX = 0;
    int m_renderY = 0;
    int m_renderWidth = 0;
    int m_renderHeight = 0;
};
