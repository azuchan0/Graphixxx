#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "app.h"
#include <cstdio>

int main() {
    printf("[1] Starting GLFW init...\n");
    if (!glfwInit()) {
        printf("ERROR: GLFW init failed\n");
        return -1;
    }
    printf("[2] GLFW init OK\n");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1920, 1080, "Graphixxx", nullptr, nullptr);
    if (!window) {
        printf("ERROR: Window creation failed\n");
        glfwTerminate();
        return -1;
    }
    printf("[3] Window created OK\n");

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        printf("ERROR: GLAD load failed\n");
        return -1;
    }
    printf("[4] GLAD loaded OK, GL version: %s\n", glGetString(GL_VERSION));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    printf("[5] ImGui init OK\n");

    App app;
    app.init();
    printf("[6] App init OK\n");

    float startTime = (float)glfwGetTime();
    printf("[7] Entering main loop...\n");

    int frameCount = 0;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        float time = (float)glfwGetTime() - startTime;
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        app.render(time, (float)mouseX, (float)mouseY);

        ImGui::Render();
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);

        frameCount++;
        if (frameCount == 1) {
            printf("[8] First frame rendered\n");
        }
    }

    app.cleanup();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}
