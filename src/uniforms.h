#pragma once
#include <string>
#include <vector>
#include <variant>

struct UniformParam {
    std::string name;
    std::string type;
    std::vector<float> minValues;
    std::vector<float> maxValues;
    std::vector<float> values;
};

class Uniforms {
public:
    void parse(const std::string& shaderCode);
    void renderUI();
    std::vector<UniformParam>& getParams();

private:
    std::vector<UniformParam> m_params;
};
