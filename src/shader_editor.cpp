#include "shader_editor.h"
#include <imgui.h>
#include <GLFW/glfw3.h>

void ShaderEditor::init() {
    m_editor.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());

    std::string defaultCode = R"(// @param float intensity 0.0 1.0 0.5
// @param float speed 0.0 5.0 1.0
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / u_resolution;
    
    vec4 texColor = texture(iChannel0, uv);
    
    vec3 col = texColor.rgb * intensity;
    col *= 0.5 + 0.5 * sin(u_time * speed);
    
    fragColor = vec4(col, texColor.a);
}
)";
    m_editor.SetText(defaultCode);
    m_lastCompileAttemptCode = defaultCode;
    m_lastObservedCode = defaultCode;
}

void ShaderEditor::render(float height) {
    std::string currentText = m_editor.GetText();
    if (currentText != m_lastObservedCode) {
        m_lastEditTime = std::chrono::steady_clock::now();
        m_needsRecompile = true;
        m_lastObservedCode = currentText;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_S) && (ImGui::GetIO().KeyCtrl)) {
        m_codeChanged = true;
        m_needsRecompile = false;
    }

    m_editor.Render("ShaderEditor", ImVec2(0, height), false);

    if (!m_error.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", m_error.c_str());
    }
}

std::string ShaderEditor::getCode() const {
    return m_editor.GetText();
}

void ShaderEditor::setCode(const std::string& code) {
    m_editor.SetText(code);
    m_lastObservedCode = code;
    m_codeChanged = true;
    m_needsRecompile = false;
}

bool ShaderEditor::hasCodeChanged() const {
    if (m_codeChanged) return true;

    if (m_needsRecompile) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<float, std::milli>(now - m_lastEditTime).count();
        if (elapsed >= m_debounceMs) {
            return true;
        }
    }
    return false;
}

bool ShaderEditor::hasImmediateCompileRequest() const {
    return m_codeChanged;
}

void ShaderEditor::markCompileAttempted() {
    m_lastCompileAttemptCode = m_editor.GetText();
    m_lastObservedCode = m_lastCompileAttemptCode;
    m_codeChanged = false;
    m_needsRecompile = false;
}

void ShaderEditor::setDebounceMs(float milliseconds) {
    m_debounceMs = milliseconds;
}

void ShaderEditor::setError(const std::string& error) {
    m_error = error;
}
