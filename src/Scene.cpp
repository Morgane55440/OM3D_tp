#include "Scene.h"

#include <TypedBuffer.h>

#include <shader_structs.h>

#include <algorithm>

namespace OM3D {

Scene::Scene() {
}

void Scene::add_object(SceneObject obj) {
    _objects.emplace_back(std::move(obj));
}

void Scene::add_light(PointLight obj) {
    _point_lights.emplace_back(std::move(obj));
}

Span<const SceneObject> Scene::objects() const {
    return _objects;
}

Span<const PointLight> Scene::point_lights() const {
    return _point_lights;
}

Camera& Scene::camera() {
    return _camera;
}

const Camera& Scene::camera() const {
    return _camera;
}

void Scene::set_sun(glm::vec3 direction, glm::vec3 color) {
    _sun_direction = direction;
    _sun_color = color;
}

float getSignedDistanceToPlane(const glm::vec3& plane, const glm::vec3& point) {
    return glm::dot(plane, point);
}

bool isOnForwardPlane(glm::vec3& plane, glm::vec3 globalCenter, const float globalRadius) {
    return getSignedDistanceToPlane(plane, globalCenter) >= -globalRadius;
}

bool isOnFrustum(Frustum frustum, SceneObject obj, Camera camera) {

    glm::mat4 transform = obj.transform();

    glm::vec3 surfacePoint = obj.getMesh()->getCenter() + glm::vec3(1.0f) * obj.getMesh()->getRadius();

    glm::vec4 globalCenter = transform * glm::vec4(obj.getMesh()->getCenter(), 1.0f);

    glm::vec4 globalSurfacePoint = transform * glm::vec4(surfacePoint, 1.0f);

    glm::vec4 diag = (globalSurfacePoint - globalCenter);
    const float globalRadius = std::sqrt((diag.x * diag.x + diag.y * diag.y + diag.z * diag.z)) * 0.5f;

    glm::vec3 globalCameraCenter = glm::vec3(globalCenter) - camera.position();

    return (isOnForwardPlane(frustum._bottom_normal, globalCameraCenter, globalRadius) &&
            isOnForwardPlane(frustum._top_normal, globalCameraCenter, globalRadius) &&
            isOnForwardPlane(frustum._left_normal, globalCameraCenter, globalRadius) &&
            isOnForwardPlane(frustum._right_normal, globalCameraCenter, globalRadius) &&
            isOnForwardPlane(frustum._near_normal, globalCameraCenter, globalRadius));
}

void Scene::render() const {
    // Fill and bind frame data buffer
    TypedBuffer<shader::FrameData> buffer(nullptr, 1);
    {
        auto mapping = buffer.map(AccessType::WriteOnly);
        mapping[0].camera.view_proj = _camera.view_proj_matrix();
        mapping[0].point_light_count = u32(_point_lights.size());
        mapping[0].sun_color = _sun_color;
        mapping[0].sun_dir = glm::normalize(_sun_direction);
    }
    buffer.bind(BufferUsage::Uniform, 0);

    // Fill and bind lights buffer
    TypedBuffer<shader::PointLight> light_buffer(nullptr, std::max(_point_lights.size(), size_t(1)));
    {
        auto mapping = light_buffer.map(AccessType::WriteOnly);
        for(size_t i = 0; i != _point_lights.size(); ++i) {
            const auto& light = _point_lights[i];
            mapping[i] = {
                light.position(),
                light.radius(),
                light.color(),
                0.0f
            };
        }
    }
    light_buffer.bind(BufferUsage::Storage, 1);

    const Frustum& frustum = _camera.build_frustum();
    glm::vec3 cameraOrigin = _camera.position();

    // Render every object
    for(const SceneObject& obj : _objects) {
        if (isOnFrustum(frustum, obj, _camera))
            obj.render();
    }
}

void Scene::zprepass() const {
    // Fill and bind frame data buffer
    TypedBuffer<shader::FrameData> buffer(nullptr, 1);
    {
        auto mapping = buffer.map(AccessType::WriteOnly);
        mapping[0].camera.view_proj = _camera.view_proj_matrix();
        mapping[0].point_light_count = u32(0);
        mapping[0].sun_color = glm::vec3(0.0,0.0,0.0);
        mapping[0].sun_dir = glm::normalize(_sun_direction);
    }
    buffer.bind(BufferUsage::Uniform, 0);

    const Frustum& frustum = _camera.build_frustum();
    glm::vec3 cameraOrigin = _camera.position();

    // Render every object
    for(const SceneObject& obj : _objects) {
        if (isOnFrustum(frustum, obj, _camera))
            obj.render();
    }
}

}
