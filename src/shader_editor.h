#pragma once
#include <string>
#include <chrono>
#include "TextEditor.h"

class ShaderEditor {
public:
    void init();
    void render(float height = 300.0f);

    std::string getCode() const;
    void setCode(const std::string& code);
    bool hasCodeChanged() const;
    bool hasImmediateCompileRequest() const;
    void markCompileAttempted();
    void setDebounceMs(float milliseconds);

    void setError(const std::string& error);

private:
    TextEditor m_editor;
    std::string m_lastCompileAttemptCode;
    std::string m_lastObservedCode;
    bool m_codeChanged = false;

    std::chrono::steady_clock::time_point m_lastEditTime;
    float m_debounceMs = 500.0f;
    bool m_needsRecompile = false;

    std::string m_error;
};
