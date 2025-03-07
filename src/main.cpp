
#include <glad/gl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <graphics.h>
#include <Scene.h>
#include <Texture.h>
#include <Framebuffer.h>
#include <TimestampQuery.h>
#include <ImGuiRenderer.h>

#include <imgui/imgui.h>

#include <iostream>
#include <vector>
#include <filesystem>

using namespace OM3D;


static float delta_time = 0.0f;
static std::unique_ptr<Scene> scene;
static float exposure = 1.0;
static std::vector<std::string> scene_files;

namespace OM3D {
extern bool audit_bindings_before_draw;
}

void parse_args(int argc, char** argv) {
    for(int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];

        if(arg == "--validate") {
            OM3D::audit_bindings_before_draw = true;
        } else {
            std::cerr << "Unknown argument \"" << arg << "\"" << std::endl;
        }
    }
}

void glfw_check(bool cond) {
    if(!cond) {
        const char* err = nullptr;
        glfwGetError(&err);
        std::cerr << "GLFW error: " << err << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

void update_delta_time() {
    static double time = 0.0;
    const double new_time = program_time();
    delta_time = float(new_time - time);
    time = new_time;
}

void process_inputs(GLFWwindow* window, Camera& camera) {
    static glm::dvec2 mouse_pos;

    glm::dvec2 new_mouse_pos;
    glfwGetCursorPos(window, &new_mouse_pos.x, &new_mouse_pos.y);

    {
        glm::vec3 movement = {};
        if(glfwGetKey(window, 'W') == GLFW_PRESS) {
            movement += camera.forward();
        }
        if(glfwGetKey(window, 'S') == GLFW_PRESS) {
            movement -= camera.forward();
        }
        if(glfwGetKey(window, 'D') == GLFW_PRESS) {
            movement += camera.right();
        }
        if(glfwGetKey(window, 'A') == GLFW_PRESS) {
            movement -= camera.right();
        }

        float speed = 10.0f;
        if(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
            speed *= 10.0f;
        }

        if(movement.length() > 0.0f) {
            const glm::vec3 new_pos = camera.position() + movement * delta_time * speed;
            camera.set_view(glm::lookAt(new_pos, new_pos + camera.forward(), camera.up()));
        }
    }

    if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        const glm::vec2 delta = glm::vec2(mouse_pos - new_mouse_pos) * 0.01f;
        if(delta.length() > 0.0f) {
            glm::mat4 rot = glm::rotate(glm::mat4(1.0f), delta.x, glm::vec3(0.0f, 1.0f, 0.0f));
            rot = glm::rotate(rot, delta.y, camera.right());
            camera.set_view(glm::lookAt(camera.position(), camera.position() + (glm::mat3(rot) * camera.forward()), (glm::mat3(rot) * camera.up())));
        }
    }

    {
        int width = 0;
        int height = 0;
        glfwGetWindowSize(window, &width, &height);
        camera.set_ratio(float(width) / float(height));
    }

    mouse_pos = new_mouse_pos;
}

void gui(ImGuiRenderer& imgui, int& debug_opt) {
    const ImVec4 error_text_color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
    const ImVec4 warning_text_color = ImVec4(1.0f, 0.8f, 0.4f, 1.0f);

    static bool open_gpu_profiler = false;

    const char* displayOptions[] = { "albedo", "normals", "depth" };

    PROFILE_GPU("GUI");

    imgui.start();
    DEFER(imgui.finish());

    //ImGui::ShowDemoWindow();

        bool open_scene_popup = false;
        if(ImGui::BeginMainMenuBar()) {
            if(ImGui::BeginMenu("File")) {
                if(ImGui::MenuItem("Open Scene")) {
                    open_scene_popup = true;
                }
                ImGui::EndMenu();
            }

            if(ImGui::BeginMenu("Exposure")) {
                ImGui::DragFloat("Exposure", &exposure, 0.25f, 0.01f, 100.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
                if(exposure != 1.0f && ImGui::Button("Reset")) {
                    exposure = 1.0f;
                }
                ImGui::EndMenu();
            }

            if(scene && ImGui::BeginMenu("Scene Info")) {
                ImGui::Text("%u objects", u32(scene->objects().size()));
                ImGui::Text("%u point lights", u32(scene->point_lights().size()));
                ImGui::EndMenu();
            }

        if (ImGui::BeginMenu("Debug Display")) {
            if (ImGui::BeginCombo("##dropdown", displayOptions[debug_opt])) {
                for (int i = 0; i < IM_ARRAYSIZE(displayOptions); i++) {
                    const bool isSelected = (debug_opt == i);
                    if (ImGui::Selectable(displayOptions[i], isSelected)) {
                        debug_opt = i;
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::EndMenu();
        }

        if(ImGui::MenuItem("GPU Profiler")) {
            open_gpu_profiler = true;
        }

        ImGui::Separator();
        ImGui::TextUnformatted(reinterpret_cast<const char*>(glGetString(GL_RENDERER)));

        ImGui::Separator();
        ImGui::Text("%.2f ms", delta_time * 1000.0f);

#ifdef OM3D_DEBUG
        ImGui::Separator();
        ImGui::TextColored(warning_text_color, ICON_FA_BUG " (DEBUG)");
#endif

        if(!bindless_enabled()) {
            ImGui::Separator();
            ImGui::TextColored(error_text_color, ICON_FA_EXCLAMATION_TRIANGLE " Bindless textures not supported");
        }
        ImGui::EndMainMenuBar();
    }

    if(open_scene_popup) {
        ImGui::OpenPopup("###openscenepopup");

        scene_files.clear();
        for(auto&& entry : std::filesystem::directory_iterator(data_path)) {
            if(entry.status().type() == std::filesystem::file_type::regular) {
                const auto ext = entry.path().extension();
                if(ext == ".gltf" || ext == ".glb") {
                    scene_files.emplace_back(entry.path().string());
                }
            }
        }
    }

    if(ImGui::BeginPopup("###openscenepopup", ImGuiWindowFlags_AlwaysAutoResize)) {
        auto load_scene = [](const std::string path) {
            auto result = Scene::from_gltf(path);
            if(!result.is_ok) {
                std::cerr << "Unable to load scene (" << path << ")" << std::endl;
            } else {
                scene = std::move(result.value);
            }
            ImGui::CloseCurrentPopup();
        };

        char buffer[1024] = {};
        if(ImGui::InputText("Load scene", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
            load_scene(buffer);
        }

        if(!scene_files.empty()) {
            for(const std::string& p : scene_files) {
                const auto abs = std::filesystem::absolute(p).string();
                if(ImGui::MenuItem(abs.c_str())) {
                    load_scene(p);
                    break;
                }
            }
        }

        ImGui::EndPopup();
    }

    if(open_gpu_profiler) {
        if(ImGui::Begin(ICON_FA_CLOCK " GPU Profiler")) {
            const ImGuiTableFlags table_flags =
                ImGuiTableFlags_SortTristate |
                ImGuiTableFlags_NoSavedSettings |
                ImGuiTableFlags_SizingFixedFit |
                ImGuiTableFlags_BordersInnerV |
                ImGuiTableFlags_Resizable |
                ImGuiTableFlags_RowBg;

            ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(1, 1, 1, 0.01f));
            DEFER(ImGui::PopStyleColor());

            if(ImGui::BeginTable("##timetable", 3, table_flags)) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("CPU (ms)", ImGuiTableColumnFlags_NoResize, 70.0f);
                ImGui::TableSetupColumn("GPU (ms)", ImGuiTableColumnFlags_NoResize, 70.0f);
                ImGui::TableHeadersRow();

                std::vector<u32> indents;
                for(const auto& zone : retrieve_profile()) {
                    auto color_from_time = [](float time) {
                        const float t = std::min(time / 0.008f, 1.0f); // 8ms = red
                        return ImVec4(t, 1.0f - t, 0.0f, 1.0f);
                    };

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(zone.name.data());

                    ImGui::TableSetColumnIndex(1);
                    ImGui::PushStyleColor(ImGuiCol_Text, color_from_time(zone.cpu_time));
                    ImGui::Text("%.2f", zone.cpu_time * 1000.0f);

                    ImGui::TableSetColumnIndex(2);
                    ImGui::PushStyleColor(ImGuiCol_Text, color_from_time(zone.gpu_time));
                    ImGui::Text("%.2f", zone.gpu_time * 1000.0f);

                    ImGui::PopStyleColor(2);

                    if(!indents.empty() && --indents.back() == 0) {
                        indents.pop_back();
                        ImGui::Unindent();
                    }

                    if(zone.contained_zones) {
                        indents.push_back(zone.contained_zones);
                        ImGui::Indent();
                    }
                }

                ImGui::EndTable();
            }
        }
        ImGui::End();
    }
}




std::unique_ptr<Scene> create_default_scene() {
    auto scene = std::make_unique<Scene>();

    // Load default cube model
    auto result = Scene::from_gltf(std::string(data_path) + "cube.glb");
    ALWAYS_ASSERT(result.is_ok, "Unable to load default scene");
    scene = std::move(result.value);

    scene->set_sun(glm::vec3(0.2f, 1.0f, 0.1f), glm::vec3(1.0f));

    // Add lights
    {
        PointLight light;
        light.set_position(glm::vec3(1.0f, 2.0f, 4.0f));
        light.set_color(glm::vec3(0.0f, 50.0f, 0.0f));
        light.set_radius(100.0f);
        scene->add_light(std::move(light));
    }
    {
        PointLight light;
        light.set_position(glm::vec3(1.0f, 2.0f, -4.0f));
        light.set_color(glm::vec3(50.0f, 0.0f, 0.0f));
        light.set_radius(50.0f);
        scene->add_light(std::move(light));
    }

    return scene;
}

struct RendererState {
    static RendererState create(glm::uvec2 size) {
        RendererState state;

        state.size = size;

        if(state.size.x > 0 && state.size.y > 0) {
            state.depth_texture = Texture(size, ImageFormat::Depth32_FLOAT);
            state.lit_hdr_texture = Texture(size, ImageFormat::RGBA8_sRGB);
            state.tone_mapped_texture = Texture(size, ImageFormat::RGBA8_UNORM);
            state.prepass_framebuffer = Framebuffer(&state.depth_texture, std::array<Texture*, 0>{});
            state.color_texture = Texture(size, ImageFormat::RGBA8_sRGB);
            state.sunlight_texture = Texture(size, ImageFormat::RGBA8_sRGB);
            state.normal_texture = Texture(size, ImageFormat::RGBA8_UNORM);
            state.indirect_light_texture = Texture(size, ImageFormat::RGBA8_sRGB);
            state.full_light_texture = Texture(size, ImageFormat::RGBA8_sRGB);
            state.main_framebuffer = Framebuffer(nullptr, std::array{&state.lit_hdr_texture});
            state.g_buffer = Framebuffer(&state.depth_texture, std::array{&state.color_texture, &state.normal_texture});
            state.tone_map_framebuffer = Framebuffer(nullptr, std::array{&state.tone_mapped_texture});
            state.indirect_lights_framebuffer = Framebuffer(nullptr, std::array{&state.indirect_light_texture});
            state.blur_framebuffer = Framebuffer(nullptr, std::array{&state.full_light_texture});
        }

        return state;
    }

    glm::uvec2 size = {};

    Texture depth_texture;
    Texture lit_hdr_texture;
    Texture tone_mapped_texture;
    Texture color_texture;
    Texture normal_texture;
    Texture sunlight_texture;
    Texture full_light_texture;
    Texture indirect_light_texture;

    Framebuffer prepass_framebuffer;
    Framebuffer sunlight_framebuffer;
    Framebuffer main_framebuffer;
    Framebuffer tone_map_framebuffer;
    Framebuffer indirect_lights_framebuffer;
    Framebuffer g_buffer;
    Framebuffer blur_framebuffer;
};





int main(int argc, char** argv) {
    DEBUG_ASSERT([] { std::cout << "Debug asserts enabled" << std::endl; return true; }());

    parse_args(argc, argv);

    glfw_check(glfwInit());
    DEFER(glfwTerminate());

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);


    GLFWwindow* window = glfwCreateWindow(1600, 900, "OM3D", nullptr, nullptr);
    glfw_check(window);
    DEFER(glfwDestroyWindow(window));

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    init_graphics();

    ImGuiRenderer imgui(window);

    scene = create_default_scene();

    auto tonemap_program = Program::from_files("tonemap.frag", "screen.vert");
    auto gbuffer_program = Program::from_files("gbuffer.frag", "basic.vert");
    auto gbuffer_choice_program = Program::from_files("gbufferchoice.frag", "screen.vert");
    auto indirect_lights_program = Program::from_files("indirect_lights.frag", "screen.vert");
    auto blur_program = Program::from_files("blur.frag", "screen.vert");
    RendererState renderer;

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    int debug_opt = 0;

    for(;;) {
        glfwPollEvents();
        if(glfwWindowShouldClose(window) || glfwGetKey(window, GLFW_KEY_ESCAPE)) {
            break;
        }

        process_profile_markers();

        {
            int width = 0;
            int height = 0;
            glfwGetWindowSize(window, &width, &height);

            if(renderer.size != glm::uvec2(width, height)) {
                renderer = RendererState::create(glm::uvec2(width, height));
            }
        }

        update_delta_time();

        if(const auto& io = ImGui::GetIO(); !io.WantCaptureMouse && !io.WantCaptureKeyboard) {
            process_inputs(window, scene->camera());
        }

        // Draw everything
        {
            PROFILE_GPU("Frame");

            //z prepass
            {
                PROFILE_GPU("Z-Prepass");
                renderer.prepass_framebuffer.bind(true,true);
                scene->zprepass();
            }
            // Render the scene
            {
                PROFILE_GPU("GBuffer pass");
                //renderer.main_framebuffer.bind(false, true);
                renderer.g_buffer.bind(true, true);
                gbuffer_program->bind();
                scene->render();
            }

            // Compute light using g buffer and ssao
            {
                PROFILE_GPU("GBuffer blitting pass");
                if (debug_opt != 0) { // just blit
                    renderer.main_framebuffer.bind(true, true);
                    gbuffer_choice_program->bind();
                    gbuffer_choice_program->set_uniform(HASH("outputtype"), OM3D::u32(debug_opt));
                    renderer.color_texture.bind(0);
                    renderer.normal_texture.bind(1);
                    renderer.depth_texture.bind(2);
                    glDrawArrays(GL_TRIANGLES, 0, 3);
                }
                else { // render lights

                    glCullFace(GL_FRONT);
                    glDepthMask(GL_TRUE);
                    renderer.main_framebuffer.bind(true, true);
                    renderer.color_texture.bind(0);
                    renderer.normal_texture.bind(1);
                    renderer.depth_texture.bind(2);
                    scene->render_lights(renderer.size);
                    glCullFace(GL_BACK);
                }
                //renderer.g_buffer.blit();
            }

            {
                std::cout << "Executing Indirect Lighting Pass" << std::endl;
                PROFILE_GPU("Indirect Lightning pass");
                // glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "indirect pass");
                renderer.indirect_lights_framebuffer.bind(false, true);
                indirect_lights_program->bind();
                renderer.lit_hdr_texture.bind(0);
                renderer.normal_texture.bind(1);
                renderer.depth_texture.bind(2);
                glDrawArrays(GL_TRIANGLES, 0, 3);
                // glPopDebugGroup();
            }

            {
                PROFILE_GPU("Blur pass");
                renderer.blur_framebuffer.bind(false, true);
                blur_program->bind();
                renderer.indirect_light_texture.bind(0);
                renderer.lit_hdr_texture.bind(1);
                renderer.depth_texture.bind(2);
                glDrawArrays(GL_TRIANGLES, 0, 3);
            }

            // Apply a tonemap in compute shader
            {
                PROFILE_GPU("Tonemap pass");
                renderer.tone_map_framebuffer.bind(false, true);
                tonemap_program->bind();
                tonemap_program->set_uniform(HASH("exposure"), exposure);
                renderer.full_light_texture.bind(0);
                glDrawArrays(GL_TRIANGLES, 0, 3);
            }   

            // Blit tonemap result to screen
            {
                PROFILE_GPU("Blit pass");

                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                renderer.tone_map_framebuffer.blit();
            }

            glClear(GL_DEPTH_BUFFER_BIT);
            // Draw GUI on top

            glDisable(GL_CULL_FACE);
            gui(imgui, debug_opt);
            glEnable(GL_CULL_FACE);
        }

        glfwSwapBuffers(window);
    }

    scene = nullptr; // destroy scene and child OpenGL objects
}
