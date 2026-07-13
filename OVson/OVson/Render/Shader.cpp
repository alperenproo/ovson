#include "Shader.h"
#include "../Utils/Logger.h"
#include <vector>
#include <fstream>
#include <string>

static void SH_Log(const std::string& msg) {
    std::ofstream ofs("liquidglass_debug.log", std::ios::app);
    if (ofs.is_open()) {
        ofs << "[Shader] " << msg << "\n";
    }
}

Shader::Shader() : programId(0), compiled(false) {}

Shader::~Shader() {
    if (programId != 0 && glDeleteProgram != nullptr) {
        glDeleteProgram(programId);
    }
}

unsigned int Shader::compileShader(unsigned int type, const std::string& source) {
    if (!glCreateShader || !glShaderSource || !glCompileShader || !glGetShaderiv || !glGetShaderInfoLog) {
        SH_Log("ERROR: OpenGL shader functions are not loaded!");
        Logger::error("OpenGL shader functions are not loaded!");
        return 0;
    }

    unsigned int id = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);

    int result;
    glGetShaderiv(id, GL_COMPILE_STATUS, &result);
    if (result == 0) {
        int length;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> message(length + 1);
        glGetShaderInfoLog(id, length, &length, &message[0]);
        SH_Log("ERROR: Failed to compile shader type " + std::to_string(type) + ": " + std::string(&message[0]));
        Logger::error(("Failed to compile shader (" + std::to_string(type) + "): " + std::string(&message[0])).c_str());
        glDeleteShader(id);
        return 0;
    }

    return id;
}

bool Shader::compile(const std::string& vertexSource, const std::string& fragmentSource) {
    if (!glCreateProgram || !glAttachShader || !glLinkProgram || !glGetProgramiv || !glGetProgramInfoLog || !glDeleteShader) {
        SH_Log("ERROR: OpenGL program/link functions are not loaded!");
        Logger::error("OpenGL program/link functions are not loaded!");
        return false;
    }

    programId = glCreateProgram();
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexSource);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

    if (vs == 0 || fs == 0) {
        SH_Log("ERROR: Vertex or fragment shader failed to compile.");
        if (vs != 0) glDeleteShader(vs);
        if (fs != 0) glDeleteShader(fs);
        return false;
    }

    glAttachShader(programId, vs);
    glAttachShader(programId, fs);
    glLinkProgram(programId);

    int programStatus;
    glGetProgramiv(programId, GL_LINK_STATUS, &programStatus);
    if (programStatus == 0) {
        int length;
        glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> message(length + 1);
        glGetProgramInfoLog(programId, length, &length, &message[0]);
        SH_Log("ERROR: Failed to link program: " + std::string(&message[0]));
        Logger::error(("Failed to link program: " + std::string(&message[0])).c_str());
        glDeleteShader(vs);
        glDeleteShader(fs);
        return false;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    compiled = true;
    SH_Log("Program linked successfully.");
    return true;
}

void Shader::use() const {
    if (compiled && glUseProgram) {
        glUseProgram(programId);
    }
}

void Shader::unuse() const {
    if (glUseProgram) {
        glUseProgram(0);
    }
}

int Shader::getUniformLocation(const std::string& name) {
    if (!compiled || !glGetUniformLocation) return -1;
    return glGetUniformLocation(programId, name.c_str());
}

void Shader::setUniform1f(const std::string& name, float value) {
    int loc = getUniformLocation(name);
    if (loc != -1 && glUniform1f) {
        glUniform1f(loc, value);
    }
}

void Shader::setUniform2f(const std::string& name, float v0, float v1) {
    int loc = getUniformLocation(name);
    if (loc != -1 && glUniform2f) {
        glUniform2f(loc, v0, v1);
    }
}

void Shader::setUniform3f(const std::string& name, float v0, float v1, float v2) {
    int loc = getUniformLocation(name);
    if (loc != -1 && glUniform3f) {
        glUniform3f(loc, v0, v1, v2);
    }
}

void Shader::setUniform4f(const std::string& name, float v0, float v1, float v2, float v3) {
    int loc = getUniformLocation(name);
    if (loc != -1 && glUniform4f) {
        glUniform4f(loc, v0, v1, v2, v3);
    }
}

void Shader::setUniform1i(const std::string& name, int value) {
    int loc = getUniformLocation(name);
    if (loc != -1 && glUniform1i) {
        glUniform1i(loc, value);
    }
}
