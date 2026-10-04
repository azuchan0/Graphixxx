#include "uniforms.h"
#include <imgui.h>
#include <sstream>
#include <regex>

void Uniforms::parse(const std::string& shaderCode) {
    m_params.clear();

    std::istringstream stream(shaderCode);
    std::string line;

    while (std::getline(stream, line)) {
        std::regex paramRegex(R"(@param\s+(\w+)\s+(\w+)(.*))");
        std::smatch match;

        if (std::regex_search(line, match, paramRegex)) {
            UniformParam param;
            param.type = match[1].str();
            param.name = match[2].str();

            std::istringstream valuesStream(match[3].str());
            std::vector<float> numbers;
            float num;
            while (valuesStream >> num) {
                numbers.push_back(num);
            }

            int valueCount = 0;
            if (param.type == "float") valueCount = 1;
            else if (param.type == "vec2") valueCount = 2;
            else if (param.type == "vec3") valueCount = 3;
            else if (param.type == "vec4") valueCount = 4;

            int requiredValues = valueCount * 3;
            
            if (numbers.size() >= requiredValues) {
                for (int i = 0; i < valueCount; ++i) {
                    param.minValues.push_back(numbers[i]);
                    param.maxValues.push_back(numbers[valueCount + i]);
                    param.values.push_back(numbers[valueCount * 2 + i]);
                }
            } else if (numbers.size() >= valueCount * 2) {
                for (int i = 0; i < valueCount; ++i) {
                    param.minValues.push_back(numbers[i]);
                    param.maxValues.push_back(numbers[valueCount + i]);
                    param.values.push_back(0.5f);
                }
            } else if (numbers.size() >= valueCount) {
                for (int i = 0; i < valueCount; ++i) {
                    param.minValues.push_back(0.0f);
                    param.maxValues.push_back(1.0f);
                    param.values.push_back(numbers[i]);
                }
            } else {
                for (int i = 0; i < valueCount; ++i) {
                    param.minValues.push_back(0.0f);
                    param.maxValues.push_back(1.0f);
                    param.values.push_back(0.5f);
                }
            }

            m_params.push_back(param);
        }
    }
}

void Uniforms::renderUI() {
    for (auto& param : m_params) {
        if (param.type == "float") {
            ImGui::SliderFloat(param.name.c_str(), &param.values[0], param.minValues[0], param.maxValues[0]);
        } else if (param.type == "vec2") {
            ImGui::SliderFloat2(param.name.c_str(), param.values.data(), param.minValues[0], param.maxValues[0]);
        } else if (param.type == "vec3") {
            if (param.name.find("color") != std::string::npos || param.name.find("tint") != std::string::npos) {
                ImGui::ColorEdit3(param.name.c_str(), param.values.data());
            } else {
                ImGui::SliderFloat3(param.name.c_str(), param.values.data(), param.minValues[0], param.maxValues[0]);
            }
        } else if (param.type == "vec4") {
            ImGui::ColorEdit4(param.name.c_str(), param.values.data());
        }
    }
}

std::vector<UniformParam>& Uniforms::getParams() {
    return m_params;
}
