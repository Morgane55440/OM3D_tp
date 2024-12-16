#include "Scene.h"

#include <TypedBuffer.h>

#include <shader_structs.h>

#include <algorithm>

namespace OM3D {

    Scene::Scene() {
    }

Scene::Scene(std::shared_ptr<StaticMesh> mesh) {
    _ball = std::move(mesh);
}

void Scene::add_object(SceneObject obj) {
    _objects.emplace_back(std::move(obj));
}

void Scene::add_light(PointLight obj) {
      
    auto pos = obj.position();
    auto radius = obj.radius();
    _point_lights.emplace_back(std::move(obj));
    auto obj_light = SceneObject(_ball, std::make_shared<Material>(std::move(Material::light_sphere_material())));
    
    obj_light.set_transform(glm::translate(glm::mat4(1.0), pos) * glm::scale(glm::mat4(1.0), glm::vec3(radius / 3.2) ));
        
    _light_balls.emplace_back(std::move(obj_light));
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

void Scene::render_lights(glm::uvec2 window_size) const
{
    TypedBuffer<shader::FrameData> buffer(nullptr, 1);
    {
        auto mapping = buffer.map(AccessType::WriteOnly);
        mapping[0].camera.view_proj = _camera.view_proj_matrix();
    }
    buffer.bind(BufferUsage::Uniform, 3);

    TypedBuffer<shader::WindowSize> size_buffer(nullptr, 1);
    {
        auto mapping = size_buffer.map(AccessType::WriteOnly);
        mapping[0].inner = window_size;
    }
    size_buffer.bind(BufferUsage::Uniform, 5);

    TypedBuffer<shader::PointLight> light_buffer(nullptr, 1);

    const Frustum& frustum = _camera.build_frustum();
    glm::vec3 cameraOrigin = _camera.position();

    for (int i = 0; i < _point_lights.size(); i++) {
        const SceneObject& obj = _light_balls[i];
        if (isOnFrustum(frustum, obj, _camera)) {
            const PointLight& l = _point_lights[i];
            {
                auto mapping = light_buffer.map(AccessType::WriteOnly);
                mapping[0] = {
                        l.position(),
                        l.radius(),
                        l.color(),
                        0.0f
                 };
            }
            light_buffer.bind(BufferUsage::Storage, 4);
            obj.render();
        }
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
