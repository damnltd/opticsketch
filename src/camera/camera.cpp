#include "camera/camera.h"
#include <cmath>
#include <algorithm>

namespace opticsketch {

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(position, target, up);
}

glm::mat4 Camera::getProjectionMatrix() const {
    switch (mode) {
        case CameraMode::TopDown2D:
        case CameraMode::Orthographic3D: {
            float halfHeight = orthoSize;
            float halfWidth = halfHeight * aspectRatio;
            return glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, nearPlane, farPlane);
        }
        case CameraMode::Perspective3D:
        default:
            return glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
    }
}

void Camera::orbit(float deltaX, float deltaY) {
    // Orbit works for all 3D modes (Orthographic3D and Perspective3D)
    // TopDown2D doesn't use orbit (fixed top-down view)
    if (mode == CameraMode::TopDown2D) {
        return; // No orbit in 2D mode
    }
    
    // Blender/Maya style: horizontal mouse movement rotates around vertical axis (azimuth)
    // Vertical mouse movement rotates around horizontal axis (elevation)
    // Positive deltaX (mouse right) = rotate right (increase azimuth)
    // Positive deltaY (mouse down) = rotate down (decrease elevation)
    azimuth -= deltaX * orbitSpeed * 0.01f;  // Reversed X
    elevation += deltaY * orbitSpeed * 0.01f; // Reversed Y
    
    // Clamp elevation to avoid gimbal lock
    elevation = std::clamp(elevation, -1.5f, 1.5f);
    
    updatePosition();
}

void Camera::pan(float deltaX, float deltaY) {
    // Calculate camera basis vectors
    // forward points from camera to target
    glm::vec3 forward = glm::normalize(target - position);
    
    // Calculate right vector: cross product of forward and world up
    // In right-handed system: right = forward × up
    glm::vec3 right = glm::normalize(glm::cross(forward, up));
    
    // Calculate camera's up vector: right × forward (orthonormal basis)
    glm::vec3 cameraUp = glm::normalize(glm::cross(right, forward));
    
    // Blender/Maya style panning:
    // When mouse moves right (deltaX > 0), we want to pan right
    // Panning right means moving the target to the right relative to camera
    // This is equivalent to moving camera left, or moving target right
    // So: target += right * deltaX (positive deltaX = move right)
    //
    // When mouse moves down (deltaY > 0), we want to pan down
    // Panning down means moving target down relative to camera
    // So: target -= cameraUp * deltaY (positive deltaY = move down)
    
    float panAmount = panSpeed * distance;
    
    if (mode == CameraMode::TopDown2D) {
        // In 2D mode, pan in XZ plane (world space)
        // Mouse right = pan right (positive X)
        // Mouse down = pan forward (positive Z)
        target += glm::vec3(1.0f, 0.0f, 0.0f) * deltaX * panAmount;
        target += glm::vec3(0.0f, 0.0f, 1.0f) * deltaY * panAmount;
    } else {
        // In 3D modes, pan along camera's right and up vectors
        // Blender/Maya style: 
        // - Mouse drag RIGHT (deltaX > 0) = pan RIGHT
        // - Mouse drag DOWN (deltaY > 0) = pan DOWN
        // When panning right, the view shifts right, which means target moves LEFT relative to camera
        // When panning down, the view shifts down, which means target moves UP relative to camera
        target -= right * deltaX * panAmount;   // Negative = pan right when mouse moves right
        target += cameraUp * deltaY * panAmount; // Positive = pan down when mouse moves down
    }
    
    updatePosition();
}

void Camera::zoom(float delta) {
    if (mode == CameraMode::TopDown2D || mode == CameraMode::Orthographic3D) {
        orthoSize -= delta * zoomSpeed;
        orthoSize = std::max(0.1f, std::min(orthoSize, 1000.0f));
        // For Orthographic3D, also update distance to keep them in sync
        if (mode == CameraMode::Orthographic3D) {
            distance = orthoSize; // Keep distance and orthoSize synchronized
            updatePosition();
        }
    } else {
        distance -= delta * zoomSpeed * distance;
        distance = std::max(0.1f, std::min(distance, 1000.0f));
        updatePosition();
    }
}

void Camera::setAspectRatio(float aspect) {
    aspectRatio = aspect;
}

void Camera::setPreset(const std::string& preset) {
    if (preset == "top") {
        mode = CameraMode::TopDown2D;
        position = glm::vec3(0.0f, 10.0f, 0.0f);
        target = glm::vec3(0.0f);
        up = glm::vec3(0.0f, 0.0f, -1.0f);
        orthoSize = 10.0f;
    } else if (preset == "front") {
        mode = CameraMode::Orthographic3D;
        azimuth = 0.0f;
        elevation = 0.0f;
        distance = 10.0f;
        updatePosition();
    } else if (preset == "side") {
        mode = CameraMode::Orthographic3D;
        azimuth = 1.5708f; // 90 degrees
        elevation = 0.0f;
        distance = 10.0f;
        updatePosition();
    } else if (preset == "isometric") {
        mode = CameraMode::Orthographic3D;
        azimuth = 0.785f; // 45 degrees
        elevation = 0.615f; // ~35 degrees
        distance = 10.0f;
        updatePosition();
    }
}

void Camera::setMode(CameraMode newMode) {
    // When switching between Perspective3D and Orthographic3D, preserve state
    if ((mode == CameraMode::Perspective3D && newMode == CameraMode::Orthographic3D) ||
        (mode == CameraMode::Orthographic3D && newMode == CameraMode::Perspective3D)) {
        // Preserve azimuth, elevation, and target
        // Convert zoom: distance <-> orthoSize
        if (newMode == CameraMode::Orthographic3D) {
            // Switching to orthographic: use current distance as orthoSize
            orthoSize = distance;
        } else {
            // Switching to perspective: use current orthoSize as distance
            distance = orthoSize;
        }
        // Update position to reflect the new mode
        updatePosition();
    } else if (newMode == CameraMode::TopDown2D) {
        // Switching to 2D: reset to top-down preset
        setPreset("top");
        return;
    }
    
    mode = newMode;
}

void Camera::resetView() {
    // Reset to initial Perspective3D state
    mode = CameraMode::Perspective3D;
    target = glm::vec3(0.0f);
    up = glm::vec3(0.0f, 1.0f, 0.0f);
    distance = 10.0f;
    azimuth = 0.0f;
    elevation = 0.5f;
    fov = 45.0f;
    orthoSize = 10.0f;
    updatePosition();
}

void Camera::frameOn(const glm::vec3& center, float boundsRadius) {
    float r = std::max(boundsRadius, 0.1f);
    target = center;

    if (mode == CameraMode::TopDown2D) {
        position = glm::vec3(center.x, 10.0f, center.z);
        orthoSize = r * 1.2f;
    } else if (mode == CameraMode::Orthographic3D) {
        orthoSize = r * 1.2f;
        distance = orthoSize;
        updatePosition();
    } else {
        // Perspective: distance so bounding sphere fits in the FOV
        float halfFovRad = glm::radians(fov * 0.5f);
        distance = (r * 1.2f) / std::sin(halfFovRad);
        distance = std::clamp(distance, 0.1f, 1000.0f);
        updatePosition();
    }
}

void Camera::updatePosition() {
    if (mode == CameraMode::TopDown2D) {
        return; // Position is set directly in 2D mode
    }
    
    // Calculate position from spherical coordinates
    float x = distance * std::cos(elevation) * std::sin(azimuth);
    float y = distance * std::sin(elevation);
    float z = distance * std::cos(elevation) * std::cos(azimuth);
    
    position = target + glm::vec3(x, y, z);
}

} // namespace opticsketch
