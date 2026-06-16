#include "camera.h"

// matrix_transform.hpp brings in lookAt and perspective. GLM models GLSL, so the
// types and functions read the same here as they do in the shader.
#include <glm/gtc/matrix_transform.hpp>

glm::mat4 Camera::viewMatrix() const {
    // lookAt builds the matrix that moves the world so the camera ends up at the
    // origin looking down its −Z axis. GLM is right-handed, matching the world
    // space our cube lives in. See Glossary: VIEW_MATRIX, VIEW_SPACE
    return glm::lookAt(position, target, up);
}

glm::mat4 Camera::projectionMatrix(float aspect) const {
    // perspective builds a frustum: a truncated pyramid that maps nearer, larger
    // slices and farther, smaller slices onto the same clip volume — which is what
    // makes distant things look smaller. See Glossary: PERSPECTIVE_PROJECTION
    glm::mat4 proj = glm::perspective(fovYRadians, aspect, nearPlane, farPlane);

    // GLM was born for OpenGL, whose clip space has Y pointing up and depth in
    // [−1, 1]. Vulkan's clip space has Y pointing *down* (and we compile GLM with
    // GLM_FORCE_DEPTH_ZERO_TO_ONE so depth already lands in [0, 1]). Negating the
    // Y scale flips the image the right way up; without it the cube renders
    // upside-down — and the apparent triangle winding inverts with it, which is
    // why the pipeline now treats counter-clockwise as front-facing.
    // See Glossary: CLIP_SPACE, NDC_SPACE, WINDING_ORDER
    proj[1][1] *= -1.0f;
    return proj;
}
