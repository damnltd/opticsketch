#pragma once

#include <glad/glad.h>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace opticsketch {

class Shader {
public:
    Shader() : id(0) {}
    ~Shader();
    
    // Load and compile shader from source strings
    bool loadFromSource(const std::string& vertexSource, const std::string& fragmentSource);
    
    // Load and compile shader from files
    bool loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);
    
    // Use this shader
    void use() const;
    
    // Set uniform values
    void setBool(const std::string& name, bool value) const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec2(const std::string& name, const glm::vec2& value) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setVec4(const std::string& name, const glm::vec4& value) const;
    void setMat3(const std::string& name, const glm::mat3& value) const;
    void setMat4(const std::string& name, const glm::mat4& value) const;
    
    GLuint getId() const { return id; }
    
    // Explicit cleanup method
    void cleanup();
    
private:
    GLuint id;
    
    std::string readFile(const std::string& filepath);
    GLuint compileShader(GLenum type, const std::string& source);
    GLuint linkProgram(GLuint vertex, GLuint fragment);
};

} // namespace opticsketch
