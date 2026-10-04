#include "image_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

GLuint ImageLoader::loadTexture(const std::string& path) {
    stbi_set_flip_vertically_on_load(true);
    
    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);

    if (!data) {
        m_lastError = "Failed to load image: " + path;
        return 0;
    }

    GLuint texId;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);
    m_lastError.clear();
    return texId;
}

void ImageLoader::freeTexture(GLuint texId) {
    if (texId) {
        glDeleteTextures(1, &texId);
    }
}

std::string ImageLoader::getLastError() const {
    return m_lastError;
}
