#include "camera.h"

// matrix_transform.hpp brings in lookAt and perspective. GLM models GLSL, so the
// types and functions read the same here as they do in the shader.
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace {

// How much each unit of input moves the camera. Orbit and pan are displacement
// based (driven by how far the mouse moved, in pixels), which makes them naturally
// frame-rate independent: the total motion equals the total drag whatever the
// frame rate. Zoom is multiplicative so it feels even at any scale.
constexpr float kOrbitSensitivity = 0.008f;   // radians of rotation per pixel dragged
constexpr float kPanSensitivity   = 0.0015f;  // fraction of `distance` per pixel dragged
constexpr float kZoomStep         = 1.1f;     // distance multiplier per wheel tick

// Easing responsiveness, in 1/seconds: higher snaps faster. This is the only place
// delta time is needed — see update(). See Glossary: DELTA_TIME
constexpr float kSmoothing = 20.0f;

// Stop just short of straight up/down so the view never flips over the pole and the
// lookAt up-vector never becomes degenerate — the orbit camera's defence against
// gimbal lock. See Glossary: GIMBAL_LOCK
const float kMaxElevation = glm::radians(89.0f);

// The unit direction from the target to the camera for a spherical (azimuth,
// elevation) pair. At elevation 0 it lies in the horizontal plane; at +90° it points
// straight up. See Glossary: SPHERICAL_COORDINATES
glm::vec3 sphericalDirection(float azimuth, float elevation) {
    const float ce = std::cos(elevation);
    return glm::vec3(ce * std::sin(azimuth), std::sin(elevation), ce * std::cos(azimuth));
}

} // namespace

void Camera::orbit(float dxPixels, float dyPixels) {
    // Dragging horizontally sweeps the azimuth; dragging vertically the elevation.
    // Signs give a "grab" feel: drag right and the model appears to follow, drag up
    // and you rise to look down on it. See Glossary: AZIMUTH, ELEVATION
    azimuthRadians   -= dxPixels * kOrbitSensitivity;
    elevationRadians -= dyPixels * kOrbitSensitivity;
    elevationRadians = std::clamp(elevationRadians, -kMaxElevation, kMaxElevation);
}

void Camera::pan(float dxPixels, float dyPixels) {
    // Slide the focal point within the camera's own screen plane, scaled by distance
    // so the model tracks the cursor at any zoom level. Right and up are derived from
    // the current view direction so panning is always screen-aligned.
    // See Glossary: CAMERA_TARGET
    const glm::vec3 toCamera = sphericalDirection(azimuthRadians, elevationRadians);
    const glm::vec3 forward = -toCamera;
    const glm::vec3 right = glm::normalize(glm::cross(forward, up));
    const glm::vec3 camUp = glm::normalize(glm::cross(right, forward));
    const float worldPerPixel = distance * kPanSensitivity;
    // Drag right → target moves left (scene follows the cursor right); drag down →
    // target moves up.
    target += (-right * dxPixels + camUp * dyPixels) * worldPerPixel;
}

void Camera::zoom(float scrollTicks) {
    // Multiplicative so each tick changes distance by the same proportion. Scrolling
    // up (positive ticks) moves closer; the clamp keeps the camera outside the mesh
    // (minDistance) and within range (maxDistance).
    distance *= std::pow(kZoomStep, -scrollTicks);
    distance = std::clamp(distance, minDistance, maxDistance);
}

void Camera::update(float dt) {
    if (!m_initialised) {
        snap();
        return;
    }
    // Exponential smoothing: alpha = 1 − e^(−k·dt) closes the same fraction of the
    // remaining gap per unit time no matter the frame rate, so motion looks identical
    // at 30 or 120 fps and never jitters. A naive `disp += (goal − disp) · k·dt` would
    // move faster the higher the frame rate. See Glossary: DELTA_TIME, FRAME_RATE_INDEPENDENCE
    const float alpha = 1.0f - std::exp(-kSmoothing * dt);
    m_dispTarget    += (target - m_dispTarget) * alpha;
    m_dispDistance  += (distance - m_dispDistance) * alpha;
    m_dispAzimuth   += (azimuthRadians - m_dispAzimuth) * alpha;
    m_dispElevation += (elevationRadians - m_dispElevation) * alpha;
    refreshClipPlanes();
}

void Camera::snap() {
    m_dispTarget = target;
    m_dispDistance = distance;
    m_dispAzimuth = azimuthRadians;
    m_dispElevation = elevationRadians;
    m_initialised = true;
    refreshClipPlanes();
}

void Camera::refreshClipPlanes() {
    // Fit the near/far planes snugly around the model at the current distance: this
    // keeps it from being clipped and concentrates depth precision where it matters.
    // near is floored at a small fraction of the distance so it stays positive even
    // when zoomed right up to the surface. See Glossary: PROJECTION_MATRIX
    m_nearPlane = std::max(m_dispDistance * 0.01f, m_dispDistance - sceneRadius * 1.5f);
    m_farPlane = m_dispDistance + sceneRadius * 3.0f;
}

glm::vec3 Camera::position() const {
    // Reconstruct the Cartesian position from the displayed spherical state.
    // See Glossary: SPHERICAL_COORDINATES
    return m_dispTarget + sphericalDirection(m_dispAzimuth, m_dispElevation) * m_dispDistance;
}

glm::mat4 Camera::viewMatrix() const {
    // lookAt builds the matrix that moves the world so the camera sits at the origin
    // looking down its −Z axis. See Glossary: VIEW_MATRIX, VIEW_SPACE
    return glm::lookAt(position(), m_dispTarget, up);
}

glm::mat4 Camera::projectionMatrix(float aspect) const {
    // perspective builds the view frustum; proj[1][1] is negated to flip Y for
    // Vulkan's clip space (see the longer note retained from Chunk 8), and the build
    // defines GLM_FORCE_DEPTH_ZERO_TO_ONE for Vulkan's [0,1] depth range.
    // See Glossary: PERSPECTIVE_PROJECTION, CLIP_SPACE
    glm::mat4 proj = glm::perspective(fovYRadians, aspect, m_nearPlane, m_farPlane);
    proj[1][1] *= -1.0f;
    return proj;
}
