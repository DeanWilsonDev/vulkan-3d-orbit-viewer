#pragma once

// Camera — describes a viewpoint and turns it into the two matrices the vertex
// shader needs: the view matrix (where the camera is and which way it looks) and
// the projection matrix (how 3D space is flattened onto the 2D screen).
//
// It owns no GPU resources — it is plain maths built on GLM. The renderer reads
// the matrices each frame and uploads them through the uniform buffer.
// See Glossary: VIEW_MATRIX, PROJECTION_MATRIX, FIELD_OF_VIEW

#include <glm/glm.hpp>

class Camera {
public:
    // Where the camera sits, the point it is aimed at, and which way is "up". The
    // view matrix is fully determined by these three. Chunk 10 will drive
    // position/target from mouse input to orbit; for now they are static.
    // See Glossary: VIEW_MATRIX, WORLD_SPACE
    glm::vec3 position{1.2f, 0.9f, -1.0f};
    glm::vec3 target{0.0f, 0.0f, 0.5f};   // the cube's centre (it sits at z≈0.5)
    glm::vec3 up{0.0f, 1.0f, 0.0f};

    // Perspective parameters. The vertical field of view sets how wide a slice of
    // the world the camera takes in; the near/far planes bound the depth range
    // that gets rendered. See Glossary: FIELD_OF_VIEW, PERSPECTIVE_PROJECTION
    float fovYRadians = glm::radians(45.0f);
    float nearPlane = 0.1f;
    float farPlane = 100.0f;

    // The view matrix: transforms world space into the camera's own space (the
    // camera at the origin looking down −Z). See Glossary: VIEW_MATRIX, VIEW_SPACE
    glm::mat4 viewMatrix() const;

    // The projection matrix for the given viewport aspect ratio (width / height):
    // transforms view space into clip space, applying perspective foreshortening.
    // Aspect is a parameter, not a field, because it changes with every resize.
    // See Glossary: PROJECTION_MATRIX, PERSPECTIVE_PROJECTION, CLIP_SPACE
    glm::mat4 projectionMatrix(float aspect) const;
};
