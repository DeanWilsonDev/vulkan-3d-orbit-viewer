#pragma once

// Camera — an orbit camera. Instead of a free-floating position, it circles a
// focal point (the target) at a given distance, with its angle described in
// spherical coordinates: an azimuth (around the vertical axis) and an elevation
// (up/down). Mouse input nudges those parameters; each frame the camera eases
// toward them so motion stays smooth and frame-rate independent. It still produces
// the view + projection matrices the vertex shader needs.
//
// It owns no GPU resources — it is plain maths built on GLM.
// See Glossary: ORBIT_CAMERA, SPHERICAL_COORDINATES, VIEW_MATRIX, PROJECTION_MATRIX

#include <glm/glm.hpp>

class Camera {
public:
    // --- Goal state ------------------------------------------------------------
    // What input drives. The displayed camera eases toward these values; setting
    // them directly (e.g. for the initial framing) then calling snap() jumps there.

    // The point the camera orbits and looks at. Panning slides it.
    // See Glossary: CAMERA_TARGET
    glm::vec3 target{0.0f};

    // Distance from the target, the azimuth (horizontal angle around +Y), and the
    // elevation (vertical angle above the horizontal plane). Together these are the
    // spherical coordinates of the camera relative to the target.
    // See Glossary: SPHERICAL_COORDINATES, AZIMUTH, ELEVATION
    float distance = 3.0f;
    float azimuthRadians = glm::radians(45.0f);
    float elevationRadians = glm::radians(25.0f);

    glm::vec3 up{0.0f, 1.0f, 0.0f};            // world up
    float fovYRadians = glm::radians(45.0f);   // vertical field of view

    // --- Interaction limits ----------------------------------------------------
    float minDistance = 0.1f;     // closest zoom; keeps the camera outside the mesh
    float maxDistance = 1000.0f;  // farthest zoom
    float sceneRadius = 1.0f;     // model bounding radius; sizes near/far each frame

    // --- Per-frame input (deltas in pixels, scroll in wheel ticks) -------------
    // Each adjusts the goal state; update() then eases the camera toward it.
    void orbit(float dxPixels, float dyPixels);  // left-drag: rotate around target
    void pan(float dxPixels, float dyPixels);    // right-drag: slide the target
    void zoom(float scrollTicks);                // wheel: change distance

    // Ease the displayed state toward the goal over dt seconds and refresh the
    // near/far planes for the current distance. The easing is exponential so it
    // closes the same fraction of the gap per unit time regardless of frame rate.
    // See Glossary: DELTA_TIME, FRAME_RATE_INDEPENDENCE
    void update(float dt);

    // Jump the displayed state to the goal immediately (no easing). Called once
    // after the initial framing so the first rendered frame is already settled.
    void snap();

    // Computed from the *displayed* (eased) state, so they reflect what update()
    // has converged to rather than the raw goal.
    glm::vec3 position() const;
    glm::mat4 viewMatrix() const;
    glm::mat4 projectionMatrix(float aspect) const;

private:
    void refreshClipPlanes();   // set m_near/m_far from m_dispDistance + sceneRadius

    // Displayed (eased) state — what is actually rendered. Synced from the goal by
    // snap() and by the first update().
    glm::vec3 m_dispTarget{0.0f};
    float m_dispDistance = 3.0f;
    float m_dispAzimuth = glm::radians(45.0f);
    float m_dispElevation = glm::radians(25.0f);
    float m_nearPlane = 0.1f;
    float m_farPlane = 100.0f;
    bool m_initialised = false;
};
