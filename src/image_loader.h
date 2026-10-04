#pragma once
#include <glad/glad.h>
#include <string>

class ImageLoader {
public:
    GLuint loadTexture(const std::string& path);
    void freeTexture(GLuint texId);
    std::string getLastError() const;

private:
    std::string m_lastError;
};
