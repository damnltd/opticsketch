#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>

namespace opticsketch {

enum class CameraMode {
    TopDown2D,      // Fixed orthographic top-down, no depth
    Orthographic3D, // Rotatable, no perspective distortion
    Perspective3D    // Full camera control, depth perception
};

class Camera {
public:
    CameraMode mode = CameraMode::Perspective3D;
    
    // Transform
    glm::vec3 position{0.0f, 5.0f, 10.0f};
    glm::vec3 target{0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    
    // Projection params
    float fov = 45.0f;           // Perspective FOV in degrees
    float orthoSize = 10.0f;     // Orthographic size (half-height)
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    float aspectRatio = 16.0f / 9.0f;
    
    // Controls (sensitivity)
    float orbitSpeed = 0.2f;      // Reduced from 0.5f
    float panSpeed = 0.005f;      // Reduced from 0.01f
    float zoomSpeed = 0.05f;      // Reduced from 0.1f
    
    // Get matrices
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;
    
    // Camera controls
    void orbit(float deltaX, float deltaY);
    void pan(float deltaX, float deltaY);
    void zoom(float delta);
    void setAspectRatio(float aspect);
    
    // Preset views
    void setPreset(const std::string& preset); // "top", "front", "side", "isometric"
    
    // Mode switching with state preservation
    void setMode(CameraMode newMode); // Preserves position/rotation/zoom when switching between 3D modes
    
    // Reset to initial view
    void resetView(); // Reset camera to initial Perspective3D state

    // Frame camera on a point with given bounding radius
    void frameOn(const glm::vec3& center, float boundsRadius);
    
private:
    float distance = 10.0f;  // Distance from target
    float azimuth = 0.0f;    // Horizontal angle (radians)
    float elevation = 0.5f;  // Vertical angle (radians)
    
    void updatePosition();
};

} // namespace opticsketch
