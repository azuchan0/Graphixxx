#include <glad/glad.h>
#include "renderer.h"
#include "uniforms.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <regex>

void Renderer::init() {
    printf("[Renderer] Creating quad VAO/VBO...\n");
	float quad[]{
		-1.0f, -1.0f,
		1.0f, -1.0f,
		-1.0f, 1.0f,
		-1.0f, 1.0f,
		1.0f, -1.0f,
		1.0f, 1.0f
	};

	glGenVertexArrays(1, &m_vao);
	glGenBuffers(1, &m_vbo);

	glBindVertexArray(m_vao);
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);
	printf("[Renderer] Quad created OK\n");

	createFramebuffer(1280, 720);
}

void Renderer::createFramebuffer(int width, int height) {
    if (m_fbo) deleteFramebuffer();

    std::vector<unsigned char> emptyFrame(static_cast<size_t>(width) * height * 4, 0);

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    glGenTextures(1, &m_fboTexture);
    glBindTexture(GL_TEXTURE_2D, m_fboTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_fboTexture, 0);

    glGenTextures(1, &m_feedbackTexture);
    glBindTexture(GL_TEXTURE_2D, m_feedbackTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, emptyFrame.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        printf("[Renderer] ERROR: Framebuffer not complete!\n");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    printf("[Renderer] Framebuffer created: %dx%d\n", width, height);
}

void Renderer::deleteFramebuffer() {
    if (m_fboTexture) {
        glDeleteTextures(1, &m_fboTexture);
        m_fboTexture = 0;
    }
    if (m_fbo) {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
    if (m_feedbackTexture) {
        glDeleteTextures(1, &m_feedbackTexture);
        m_feedbackTexture = 0;
    }
}

void Renderer::clearFeedbackHistory() {
    if (!m_feedbackTexture || m_width <= 0 || m_height <= 0) return;

    std::vector<unsigned char> emptyFrame(static_cast<size_t>(m_width) * m_height * 4, 0);
    GLint previousActiveTexture = GL_TEXTURE0;
    GLint previousBinding = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    glActiveTexture(GL_TEXTURE4);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousBinding);
    glBindTexture(GL_TEXTURE_2D, m_feedbackTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height,
        GL_RGBA, GL_UNSIGNED_BYTE, emptyFrame.data());
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousBinding));
    glActiveTexture(static_cast<GLenum>(previousActiveTexture));
}

GLuint Renderer::getOutputTexture() const {
    return m_fboTexture;
}

void Renderer::getRenderRegion(int& x, int& y, int& width, int& height) const {
    x = m_renderX;
    y = m_renderY;
    width = m_renderWidth;
    height = m_renderHeight;
}

void Renderer::render(float time, float mouseX, float mouseY) {
    if (!m_program) {
        printf("[Renderer] No shader program, skipping render\n");
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_width, m_height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    m_renderX = 0;
    m_renderY = 0;
    m_renderWidth = m_width;
    m_renderHeight = m_height;

    if (m_textureWidths[0] > 0 && m_textureHeights[0] > 0) {
        const float scale = std::min({
            1.0f,
            static_cast<float>(m_width) / static_cast<float>(m_textureWidths[0]),
            static_cast<float>(m_height) / static_cast<float>(m_textureHeights[0])
        });
        m_renderWidth = std::max(1, static_cast<int>(std::round(m_textureWidths[0] * scale)));
        m_renderHeight = std::max(1, static_cast<int>(std::round(m_textureHeights[0] * scale)));
        m_renderX = (m_width - m_renderWidth) / 2;
        m_renderY = (m_height - m_renderHeight) / 2;
    }

    glViewport(m_renderX, m_renderY, m_renderWidth, m_renderHeight);

    glUseProgram(m_program);

    glUniform1f(glGetUniformLocation(m_program, "u_time"), time);
    glUniform2f(glGetUniformLocation(m_program, "u_resolution"),
        static_cast<float>(m_renderWidth), static_cast<float>(m_renderHeight));
    glUniform2f(glGetUniformLocation(m_program, "u_targetResolution"),
        static_cast<float>(m_width), static_cast<float>(m_height));
    glUniform2f(glGetUniformLocation(m_program, "u_viewportOrigin"),
        static_cast<float>(m_renderX), static_cast<float>(m_renderY));
    glUniform2f(glGetUniformLocation(m_program, "u_mouse"), mouseX, mouseY);

    for (int i = 0; i < 4; ++i) {
        std::string name = "iChannel" + std::to_string(i);
        GLint loc = glGetUniformLocation(m_program, name.c_str());
        glUniform1i(loc, i);
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, m_textures[i]);
    }

    glUniform1i(glGetUniformLocation(m_program, "u_previousFrame"), 4);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, m_feedbackTexture);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, m_feedbackTexture);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, m_width, m_height);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glActiveTexture(GL_TEXTURE0);
}

void Renderer::cleanup() {
    if (m_program) glDeleteProgram(m_program);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    deleteFramebuffer();
    m_program = 0;
    m_vbo = 0;
    m_vao = 0;
}

bool Renderer::compileShader(const std::string& userCode) {
    printf("[Renderer] Compiling shader...\n");
    const char* vertSrc = R"(
        #version 330 core
        layout(location = 0) in vec2 position;
        out vec2 fragCoord;
        void main() {
            fragCoord = position * 0.5 + 0.5;
            gl_Position = vec4(position, 0.0, 1.0);
        }
    )";

    std::string customUniforms;
    std::istringstream stream(userCode);
    std::string line;
    while (std::getline(stream, line)) {
        std::regex paramRegex(R"(@param\s+(\w+)\s+(\w+))");
        std::smatch match;
        if (std::regex_search(line, match, paramRegex)) {
            std::string type = match[1].str();
            std::string name = match[2].str();
            customUniforms += "uniform " + type + " " + name + ";\n";
        }
    }

    std::string fragSrc = R"(
        #version 330 core
        in vec2 fragCoord;
        out vec4 outColor;
        uniform float u_time;
        uniform vec2 u_resolution;
        uniform vec2 u_targetResolution;
        uniform vec2 u_viewportOrigin;
        uniform vec2 u_mouse;
        uniform sampler2D u_previousFrame;
        uniform sampler2D iChannel0;
        uniform sampler2D iChannel1;
        uniform sampler2D iChannel2;
        uniform sampler2D iChannel3;
    )" + customUniforms + userCode + R"(
        void main() {
            vec4 fragColor;
            mainImage(fragColor, gl_FragCoord.xy - u_viewportOrigin);
            outColor = fragColor;
        }
    )";

    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &vertSrc, nullptr);
    glCompileShader(vert);

    GLint success;
    glGetShaderiv(vert, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(vert, 512, nullptr, log);
        m_lastError = "Vertex shader: " + std::string(log);
        printf("[Renderer] Vertex shader ERROR: %s\n", log);
        glDeleteShader(vert);
        return false;
    }
    printf("[Renderer] Vertex shader OK\n");

    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
    const char* fragCStr = fragSrc.c_str();
    glShaderSource(frag, 1, &fragCStr, nullptr);
    glCompileShader(frag);

    glGetShaderiv(frag, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(frag, 512, nullptr, log);
        m_lastError = "Fragment shader: " + std::string(log);
        printf("[Renderer] Fragment shader ERROR: %s\n", log);
        glDeleteShader(vert);
        glDeleteShader(frag);
        return false;
    }
    printf("[Renderer] Fragment shader OK\n");

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);

    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(prog, 512, nullptr, log);
        m_lastError = "Link: " + std::string(log);
        printf("[Renderer] Link ERROR: %s\n", log);
        glDeleteShader(vert);
        glDeleteShader(frag);
        glDeleteProgram(prog);
        return false;
    }

    glDeleteShader(vert);
    glDeleteShader(frag);

    if (m_program) glDeleteProgram(m_program);
    m_program = prog;
    m_lastError.clear();
    clearFeedbackHistory();
    printf("[Renderer] Shader program linked OK\n");
    return true;
}

std::string Renderer::getLastError() const {
    return m_lastError;
}

void Renderer::setTexture(int channel, GLuint texId) {
    if (channel < 0 || channel >= 4) return;

    m_textures[channel] = texId;
    m_textureWidths[channel] = 0;
    m_textureHeights[channel] = 0;
    if (!texId) return;

    GLint previousActiveTexture = GL_TEXTURE0;
    GLint previousBinding = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousBinding);
    glBindTexture(GL_TEXTURE_2D, texId);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &m_textureWidths[channel]);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &m_textureHeights[channel]);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousBinding));
    glActiveTexture(static_cast<GLenum>(previousActiveTexture));
    if (channel == 0) {
        clearFeedbackHistory();
    }
}

void Renderer::setResolution(int width, int height) {
    if (width != m_width || height != m_height) {
        m_width = width;
        m_height = height;
        createFramebuffer(width, height);
    }
}

void Renderer::setCustomUniforms(const std::vector<UniformParam>& params) {
    if (!m_program) return;
    glUseProgram(m_program);
    for (const auto& param : params) {
        GLint loc = glGetUniformLocation(m_program, param.name.c_str());
        if (loc == -1) continue;
        if (param.type == "float" && param.values.size() >= 1) {
            glUniform1f(loc, param.values[0]);
        } else if (param.type == "vec2" && param.values.size() >= 2) {
            glUniform2f(loc, param.values[0], param.values[1]);
        } else if (param.type == "vec3" && param.values.size() >= 3) {
            glUniform3f(loc, param.values[0], param.values[1], param.values[2]);
        } else if (param.type == "vec4" && param.values.size() >= 4) {
            glUniform4f(loc, param.values[0], param.values[1], param.values[2], param.values[3]);
        }
    }
}



