#pragma once
#include <string>
#include "GL.h"

class Shader {
private:
    unsigned int programId;
    bool compiled;

    unsigned int compileShader(unsigned int type, const std::string& source);
    int getUniformLocation(const std::string& name);

public:
    Shader();
    ~Shader();

    bool compile(const std::string& vertexSource, const std::string& fragmentSource);
    void use() const;
    void unuse() const;
    unsigned int getProgramId() const { return programId; }
    bool isCompiled() const { return compiled; }

    void setUniform1f(const std::string& name, float value);
    void setUniform2f(const std::string& name, float v0, float v1);
    void setUniform3f(const std::string& name, float v0, float v1, float v2);
    void setUniform4f(const std::string& name, float v0, float v1, float v2, float v3);
    void setUniform1i(const std::string& name, int value);
};
