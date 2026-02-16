#include <iostream>
#include <chrono>
#include <thread>
#include <cstring>
#include <map>
#include <filesystem>
#include <fstream>
#include <algorithm>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <vector>
#include <string>
#include <sstream>
#include <glm/glm.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
}

// Phase 5: Video decoder using FFmpeg
class VideoDecoder {
public:
    AVFormatContext* fmt_ctx = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    SwsContext* sws_ctx = nullptr;
    AVFrame* frame = nullptr;
    AVFrame* frame_rgb = nullptr;
    int video_stream_idx = -1;
    int width = 0, height = 0;
    int total_frames = 0;
    int current_frame = 0;
    uint8_t* buffer = nullptr;
    
    ~VideoDecoder() { cleanup(); }
    
    void cleanup() {
        if (buffer) av_free(buffer);
        if (frame) av_frame_free(&frame);
        if (frame_rgb) av_frame_free(&frame_rgb);
        if (sws_ctx) sws_freeContext(sws_ctx);
        if (codec_ctx) avcodec_free_context(&codec_ctx);
        if (fmt_ctx) avformat_close_input(&fmt_ctx);
    }
    
    bool open(const std::string& path) {
        cleanup();
        
        // Open file
        if (avformat_open_input(&fmt_ctx, path.c_str(), nullptr, nullptr) < 0) {
            std::cerr << "Cannot open file: " << path << "\n";
            return false;
        }
        
        // Find stream info
        if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
            std::cerr << "Cannot find stream info\n";
            return false;
        }
        
        // Find video stream
        for (unsigned i = 0; i < fmt_ctx->nb_streams; i++) {
            if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                video_stream_idx = i;
                break;
            }
        }
        
        if (video_stream_idx < 0) {
            std::cerr << "No video stream found\n";
            return false;
        }
        
        // Get codec and open
        const AVCodec* codec = avcodec_find_decoder(fmt_ctx->streams[video_stream_idx]->codecpar->codec_id);
        if (!codec) {
            std::cerr << "Codec not found\n";
            return false;
        }
        
        codec_ctx = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(codec_ctx, fmt_ctx->streams[video_stream_idx]->codecpar);
        
        if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
            std::cerr << "Cannot open codec\n";
            return false;
        }
        
        width = codec_ctx->width;
        height = codec_ctx->height;
        
        // Allocate frames
        frame = av_frame_alloc();
        frame_rgb = av_frame_alloc();
        if (!frame || !frame_rgb) {
            std::cerr << "Cannot allocate frames\n";
            return false;
        }
        
        // Set up sws for RGBA conversion
        sws_ctx = sws_getContext(width, height, codec_ctx->pix_fmt,
                                 width, height, AV_PIX_FMT_RGBA,
                                 SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (!sws_ctx) {
            std::cerr << "Cannot create sws context\n";
            return false;
        }
        
        // Allocate buffer for RGBA
        int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, width, height, 1);
        buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
        av_image_fill_arrays(frame_rgb->data, frame_rgb->linesize, buffer, AV_PIX_FMT_RGBA, width, height, 1);
        
        // Calculate total frames
        AVStream* stream = fmt_ctx->streams[video_stream_idx];
        if (stream->nb_frames > 0) {
            total_frames = stream->nb_frames;
        } else if (stream->duration > 0 && stream->avg_frame_rate.num > 0) {
            total_frames = (int)(stream->duration * stream->avg_frame_rate.num / (stream->avg_frame_rate.den * stream->time_base.den));
        }
        
        std::cout << "Video opened: " << path << " (" << width << "x" << height << ", " << total_frames << " frames)\n";
        return true;
    }
    
    bool get_frame(uint8_t*& rgba_data, int& w, int& h) {
        if (!frame_rgb || !sws_ctx) return false;
        
        AVPacket* packet = av_packet_alloc();
        if (!packet) return false;
        
        while (av_read_frame(fmt_ctx, packet) >= 0) {
            if (packet->stream_index == video_stream_idx) {
                avcodec_send_packet(codec_ctx, packet);
                if (avcodec_receive_frame(codec_ctx, frame) == 0) {
                    // Convert to RGBA
                    sws_scale(sws_ctx, frame->data, frame->linesize, 0, height,
                              frame_rgb->data, frame_rgb->linesize);
                    
                    rgba_data = frame_rgb->data[0];
                    w = width;
                    h = height;
                    current_frame++;
                    av_packet_free(&packet);
                    return true;
                }
            }
            av_packet_unref(packet);
        }
        
        av_packet_free(&packet);
        return false;
    }
    
    void seek_to_frame(int frame_idx) {
        if (!fmt_ctx || video_stream_idx < 0) return;
        
        AVStream* stream = fmt_ctx->streams[video_stream_idx];
        int64_t ts = (int64_t)frame_idx * stream->avg_frame_rate.den / stream->avg_frame_rate.num;
        av_seek_frame(fmt_ctx, video_stream_idx, ts, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(codec_ctx);
        current_frame = frame_idx;
    }
};

// Phase 4: Texture/Media management structure
struct TextureAsset {
    GLuint gl_texture = 0;
    int width = 0, height = 0;
    int channels = 0;
    char filepath[256] = {};
    
    TextureAsset() = default;
    
    ~TextureAsset() {
        if (gl_texture) glDeleteTextures(1, &gl_texture);
    }
    
    bool load_from_file(const std::string& path) {
        int w, h, c;
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &c, 4);  // Force RGBA
        if (!data) {
            std::cerr << "Failed to load image: " << path << "\n";
            return false;
        }
        
        width = w;
        height = h;
        channels = 4;
        strncpy(filepath, path.c_str(), sizeof(filepath) - 1);
        filepath[sizeof(filepath) - 1] = '\0';
        
        // Create GL texture
        if (gl_texture) glDeleteTextures(1, &gl_texture);
        glGenTextures(1, &gl_texture);
        glBindTexture(GL_TEXTURE_2D, gl_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glBindTexture(GL_TEXTURE_2D, 0);
        
        stbi_image_free(data);
        std::cout << "Loaded texture: " << path << " (" << width << "x" << height << ")\n";
        return true;
    }
};

// Phase 4: Media/Project asset management
struct MediaLibrary {
    std::map<std::string, TextureAsset> textures;  // name -> texture
    std::string selected_texture;
    VideoDecoder video_decoder;
    GLuint video_texture = 0;
    bool is_video_loaded = false;
    
    ~MediaLibrary() {
        if (video_texture) glDeleteTextures(1, &video_texture);
    }
    
    bool add_texture(const std::string& path) {
        TextureAsset asset;
        if (!asset.load_from_file(path)) return false;
        
        std::string name = std::filesystem::path(path).filename().string();
        textures[name] = std::move(asset);
        selected_texture = name;
        return true;
    }
    
    bool load_video(const std::string& path) {
        if (!video_decoder.open(path)) return false;
        
        // Create initial video texture
        if (video_texture) glDeleteTextures(1, &video_texture);
        glGenTextures(1, &video_texture);
        glBindTexture(GL_TEXTURE_2D, video_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, video_decoder.width, video_decoder.height, 
                    0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);
        
        is_video_loaded = true;
        selected_texture = std::filesystem::path(path).filename().string();
        return true;
    }
    
    bool update_video_frame() {
        if (!is_video_loaded || !video_texture) return false;
        
        uint8_t* rgba_data = nullptr;
        int w, h;
        if (!video_decoder.get_frame(rgba_data, w, h)) return false;
        
        // Update texture with new frame
        glBindTexture(GL_TEXTURE_2D, video_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba_data);
        glBindTexture(GL_TEXTURE_2D, 0);
        return true;
    }
    
    TextureAsset* get_selected() {
        if (selected_texture.empty() || textures.find(selected_texture) == textures.end()) {
            return nullptr;
        }
        return &textures[selected_texture];
    }
};

// Phase 3: Quad mapping structure
struct Quad {
    ImVec2 corners[4];  // 0=TL, 1=TR, 2=BR, 3=BL
    char name[64];
    bool selected = false;
    
    Quad(const std::string& n = "") : selected(false) {
        strncpy(name, n.c_str(), sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        corners[0] = ImVec2(100, 100);
        corners[1] = ImVec2(300, 100);
        corners[2] = ImVec2(300, 300);
        corners[3] = ImVec2(100, 300);
    }
};

// Phase 6: Layer management structure
struct Layer {
    char name[64] = {};
    int quad_idx = -1;  // Reference to quad
    int texture_idx = -1;  // Index into media library textures
    float opacity = 1.0f;
    int blend_mode = 0;  // 0=Alpha, 1=Add, 2=Multiply
    bool visible = true;
    int z_order = 0;  // Higher = on top
    
    Layer(const std::string& n = "") {
        strncpy(name, n.c_str(), sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
    }
};

// Phase 6: Layer composition system
struct LayerCompositor {
    std::vector<Layer> layers;
    int selected_layer_idx = -1;
    
    void add_layer(const std::string& name) {
        Layer l(name);
        l.z_order = (int)layers.size();
        layers.push_back(l);
        selected_layer_idx = (int)layers.size() - 1;
    }
    
    void remove_layer(int idx) {
        if (idx >= 0 && idx < (int)layers.size()) {
            layers.erase(layers.begin() + idx);
            selected_layer_idx = std::min(selected_layer_idx, (int)layers.size() - 1);
        }
    }
    
    void move_layer_up(int idx) {
        if (idx > 0 && idx < (int)layers.size()) {
            std::swap(layers[idx], layers[idx - 1]);
            std::swap(layers[idx].z_order, layers[idx - 1].z_order);
        }
    }
    
    void move_layer_down(int idx) {
        if (idx >= 0 && idx < (int)layers.size() - 1) {
            std::swap(layers[idx], layers[idx + 1]);
            std::swap(layers[idx].z_order, layers[idx + 1].z_order);
        }
    }
};

// Phase 7: Scene persistence structure
struct Scene {
    char name[64] = {};
    char description[256] = {};
    int version = 1;
    std::vector<Quad> quads;
    std::vector<Layer> layers;
    
    Scene(const std::string& n = "Untitled") {
        strncpy(name, n.c_str(), sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
    }
    
    json to_json() const {
        json j;
        j["name"] = name;
        j["description"] = description;
        j["version"] = version;
        
        // Serialize quads
        j["quads"] = json::array();
        for (const auto& q : quads) {
            json quad_obj;
            quad_obj["name"] = q.name;
            quad_obj["corners"] = json::array();
            for (int i = 0; i < 4; ++i) {
                quad_obj["corners"].push_back({
                    {"x", q.corners[i].x},
                    {"y", q.corners[i].y}
                });
            }
            j["quads"].push_back(quad_obj);
        }
        
        // Serialize layers
        j["layers"] = json::array();
        for (const auto& l : layers) {
            json layer_obj;
            layer_obj["name"] = l.name;
            layer_obj["quad_idx"] = l.quad_idx;
            layer_obj["texture_idx"] = l.texture_idx;
            layer_obj["opacity"] = l.opacity;
            layer_obj["blend_mode"] = l.blend_mode;
            layer_obj["visible"] = l.visible;
            layer_obj["z_order"] = l.z_order;
            j["layers"].push_back(layer_obj);
        }
        
        return j;
    }
    
    bool from_json(const json& j) {
        try {
            strncpy(name, j.value("name", "Untitled").c_str(), sizeof(name) - 1);
            strncpy(description, j.value("description", "").c_str(), sizeof(description) - 1);
            version = j.value("version", 1);
            
            // Deserialize quads
            quads.clear();
            if (j.contains("quads")) {
                for (const auto& quad_obj : j["quads"]) {
                    Quad q(quad_obj.value("name", "Quad"));
                    if (quad_obj.contains("corners")) {
                        const auto& corners = quad_obj["corners"];
                        for (int i = 0; i < 4 && i < (int)corners.size(); ++i) {
                            q.corners[i] = ImVec2(corners[i]["x"], corners[i]["y"]);
                        }
                    }
                    quads.push_back(q);
                }
            }
            
            // Deserialize layers
            layers.clear();
            if (j.contains("layers")) {
                for (const auto& layer_obj : j["layers"]) {
                    Layer l(layer_obj.value("name", "Layer"));
                    l.quad_idx = layer_obj.value("quad_idx", -1);
                    l.texture_idx = layer_obj.value("texture_idx", -1);
                    l.opacity = layer_obj.value("opacity", 1.0f);
                    l.blend_mode = layer_obj.value("blend_mode", 0);
                    l.visible = layer_obj.value("visible", true);
                    l.z_order = layer_obj.value("z_order", 0);
                    layers.push_back(l);
                }
            }
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "JSON deserialization error: " << e.what() << "\n";
            return false;
        }
    }
};

// Phase 8: Simple projection/composition renderer
class ProjectionRenderer {
public:
    GLuint quad_vao = 0, quad_vbo = 0, quad_ebo = 0;
    GLuint shader_program = 0;
    bool is_initialized = false;
    
    ProjectionRenderer() = default;
    
    ~ProjectionRenderer() { cleanup(); }
    
    void cleanup() {
        if (quad_vao) glDeleteVertexArrays(1, &quad_vao);
        if (quad_vbo) glDeleteBuffers(1, &quad_vbo);
        if (quad_ebo) glDeleteBuffers(1, &quad_ebo);
        if (shader_program) glDeleteProgram(shader_program);
    }
    
    bool init() {
        // Simple vertex shader
        const char* vs_src = R"(
            #version 410 core
            layout(location = 0) in vec2 pos;
            layout(location = 1) in vec2 uv;
            
            out vec2 frag_uv;
            
            uniform vec2 corners[4];
            uniform vec2 screen_size;
            
            void main() {
                vec2 quad_corner = mix(mix(corners[3], corners[2], uv.x),
                                       mix(corners[0], corners[1], uv.x), uv.y);
                
                vec2 ndc = (quad_corner / screen_size) * 2.0 - 1.0;
                ndc.y = -ndc.y;  // Flip Y
                
                gl_Position = vec4(ndc, 0.0, 1.0);
                frag_uv = uv;
            }
        )";
        
        // Simple fragment shader
        const char* fs_src = R"(
            #version 410 core
            in vec2 frag_uv;
            out vec4 color;
            
            uniform sampler2D tex;
            uniform float opacity;
            uniform int blend_mode;  // 0=alpha, 1=add, 2=multiply
            uniform float brightness;
            
            void main() {
                vec4 tex_color = texture(tex, frag_uv);
                tex_color.rgb *= brightness;
                tex_color.a *= opacity;
                
                if (blend_mode == 1) {
                    // Additive blend
                    color = vec4(tex_color.rgb, 1.0);
                } else if (blend_mode == 2) {
                    // Multiply blend
                    color = vec4(tex_color.rgb, tex_color.a);
                } else {
                    // Alpha blend (default)
                    color = tex_color;
                }
            }
        )";
        
        // Compile shaders
        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vs_src, nullptr);
        glCompileShader(vs);
        
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fs_src, nullptr);
        glCompileShader(fs);
        
        // Link program
        shader_program = glCreateProgram();
        glAttachShader(shader_program, vs);
        glAttachShader(shader_program, fs);
        glLinkProgram(shader_program);
        
        glDeleteShader(vs);
        glDeleteShader(fs);
        
        // Create quad mesh (unit quad 0-1)
        float vertices[] = {
            0.0f, 0.0f, 0.0f, 0.0f,  // TL
            1.0f, 0.0f, 1.0f, 0.0f,  // TR
            1.0f, 1.0f, 1.0f, 1.0f,  // BR
            0.0f, 1.0f, 0.0f, 1.0f,  // BL
        };
        
        unsigned int indices[] = {0, 1, 2, 0, 2, 3};
        
        glGenVertexArrays(1, &quad_vao);
        glGenBuffers(1, &quad_vbo);
        glGenBuffers(1, &quad_ebo);
        
        glBindVertexArray(quad_vao);
        glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
        
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        
        is_initialized = true;
        return true;
    }
    
    void render_quad(const Quad& q, GLuint texture, float opacity, int blend_mode, float brightness = 1.0f) {
        if (!is_initialized || !texture) return;
        
        glUseProgram(shader_program);
        
        // Set up uniforms
        ImVec2 corners[4] = {q.corners[0], q.corners[1], q.corners[2], q.corners[3]};
        int corners_loc = glGetUniformLocation(shader_program, "corners");
        glUniform2fv(corners_loc, 4, (float*)corners);
        
        int screen_size_loc = glGetUniformLocation(shader_program, "screen_size");
        glUniform2f(screen_size_loc, ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);
        
        int opacity_loc = glGetUniformLocation(shader_program, "opacity");
        glUniform1f(opacity_loc, opacity);
        
        int blend_mode_loc = glGetUniformLocation(shader_program, "blend_mode");
        glUniform1i(blend_mode_loc, blend_mode);
        
        int brightness_loc = glGetUniformLocation(shader_program, "brightness");
        glUniform1f(brightness_loc, brightness);
        
        // Bind texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        int tex_loc = glGetUniformLocation(shader_program, "tex");
        glUniform1i(tex_loc, 0);
        
        // Render
        glBindVertexArray(quad_vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
};

// Phase 9: Show Mode live controls and OSD
struct ShowModeController {
    bool show_osd = true;
    float brightness = 1.0f;
    float global_opacity = 1.0f;
    float seek_offset = 0.0f;  // Frame offset
    std::vector<bool> layer_overrides;  // Track per-layer visibility overrides
    
    void update_layer_visibility(int layer_count) {
        if ((int)layer_overrides.size() != layer_count) {
            layer_overrides.assign(layer_count, false);  // false = use layer's visibility, true = force hidden
        }
    }
    
    bool is_layer_visible(int layer_idx, bool original_visible) {
        if (layer_idx < 0 || layer_idx >= (int)layer_overrides.size()) {
            return original_visible;
        }
        return original_visible && !layer_overrides[layer_idx];
    }
    
    void render_osd(const LayerCompositor& compositor, const MediaLibrary& media_lib) {
        if (!show_osd) return;
        
        ImDrawList* draw_list = ImGui::GetForegroundDrawList();
        ImVec2 screen_size = ImGui::GetIO().DisplaySize;
        ImU32 text_color = ImGui::GetColorU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        ImU32 bg_color = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.7f));
        
        float pad = 20.0f;
        ImVec2 pos(pad, pad);
        
        // Title
        draw_list->AddText(pos, text_color, "=== SHOW MODE ===");
        pos.y += 25;
        
        // Brightness
        std::string brightness_str = "Brightness: " + std::to_string((int)(brightness * 100)) + "%";
        draw_list->AddText(pos, text_color, brightness_str.c_str());
        pos.y += 20;
        
        // Global opacity
        std::string opacity_str = "Global Opacity: " + std::to_string((int)(global_opacity * 100)) + "%";
        draw_list->AddText(pos, text_color, opacity_str.c_str());
        pos.y += 20;
        
        // Video info if playing
        if (media_lib.is_video_loaded) {
            std::string video_str = "Video Frame: " + std::to_string(media_lib.video_decoder.current_frame) + 
                                   "/" + std::to_string(media_lib.video_decoder.total_frames);
            draw_list->AddText(pos, text_color, video_str.c_str());
            pos.y += 20;
        }
        
        // Layer visibility
        std::string layer_str = "Layers (press 1-" + std::to_string(std::min(9, (int)compositor.layers.size())) + " to toggle):";
        draw_list->AddText(pos, text_color, layer_str.c_str());
        pos.y += 20;
        
        for (int i = 0; i < std::min(9, (int)compositor.layers.size()); ++i) {
            const Layer& layer = compositor.layers[i];
            bool visible = is_layer_visible(i, layer.visible);
            std::string layer_info = std::string(visible ? "[V]" : "[H]") + " " + std::to_string(i+1) + ": " + layer.name;
            draw_list->AddText(pos, text_color, layer_info.c_str());
            pos.y += 18;
        }
        
        // Controls help
        pos.y += 10;
        ImU32 help_color = ImGui::GetColorU32(ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
        draw_list->AddText(pos, help_color, "SPACE: Play/Pause | Arrows: Seek/Adjust | +/-: Brightness | ESC: Exit");
        pos.y += 18;
        draw_list->AddText(pos, help_color, "H: Toggle OSD | O: Toggle All Layers");
    }
};

int main(int argc, char** argv)
{
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    // Request OpenGL 4.1 core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(1280, 720, "VivaLux - Phase 1", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable VSync -> stable 60 FPS on most displays

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410");

    bool show_demo = true;
    // Phase 2: monitor selection state
    int selected_monitor = 0;
    bool is_fullscreen = false;
    int prev_x = 100, prev_y = 100, prev_w = 1280, prev_h = 720;

    // Phase 3: quad mapping state
    std::vector<Quad> quads;
    int selected_quad_idx = -1;
    bool is_placing_quad = false;
    int quad_placement_corner = 0;  // which corner we're placing (0-3)
    float snap_distance = 10.0f;    // pixels

    // Phase 4: media/texture management
    MediaLibrary media_library;
    char file_input_buffer[256] = {};  // For file path input
    bool is_playing = false;
    float playback_time = 0.0f;

    // Phase 6: layer composition
    LayerCompositor compositor;

    // Phase 7: scene management
    Scene current_scene("Default");
    char scene_save_path[256] = {};
    char scene_load_path[256] = {};

    // Phase 8: show mode and composition rendering
    bool show_mode = false;
    ProjectionRenderer projection_renderer;
    projection_renderer.init();

    // Phase 9: Show Mode live controls
    ShowModeController show_controller;

    auto refresh_monitors = [&]() -> std::vector<GLFWmonitor*> {
        int count = 0;
        GLFWmonitor** mons = glfwGetMonitors(&count);
        std::vector<GLFWmonitor*> out;
        for (int i = 0; i < count; ++i) out.push_back(mons[i]);
        return out;
    };

    auto get_monitor_name = [&](GLFWmonitor* m)->std::string {
        if (!m) return "<none>";
        const char* name = glfwGetMonitorName(m);
        return name ? std::string(name) : std::string("Unknown");
    };

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Phase 8: Keyboard shortcuts
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS && 
            glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS && 
            glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
            show_mode = !show_mode;
            std::cout << (show_mode ? "Entering Show Mode" : "Exiting Show Mode") << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(200));  // Debounce
        }

        // Update monitor list each frame (cheap): keep selection if possible
        std::vector<GLFWmonitor*> monitors = refresh_monitors();
        if (selected_monitor >= (int)monitors.size()) selected_monitor = (int)monitors.size() - 1;
        if (selected_monitor < 0) selected_monitor = 0;

        // Phase 3: Handle mouse clicks for quad placement (only if not over ImGui and not in show mode)
        if (!show_mode && is_placing_quad && !ImGui::GetIO().WantCaptureMouse) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                ImVec2 mouse_pos = ImGui::GetMousePos();
                if (selected_quad_idx >= 0 && selected_quad_idx < (int)quads.size()) {
                    quads[selected_quad_idx].corners[quad_placement_corner] = mouse_pos;
                    quad_placement_corner = (quad_placement_corner + 1) % 4;
                    if (quad_placement_corner == 0) {
                        is_placing_quad = false;  // Done placing all 4 corners
                        std::cout << "Quad placement complete: " << quads[selected_quad_idx].name << "\n";
                    }
                }
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (!show_mode && show_demo)
            ImGui::ShowDemoWindow(&show_demo);

        // --- Phase 2 UI: Monitor enumeration & fullscreen control ---
        {
            ImGui::Begin("Output / Display");

            ImGui::Text("Detected monitors: %d", (int)monitors.size());

            // Create a combo listing monitors
            std::vector<std::string> names;
            for (size_t i = 0; i < monitors.size(); ++i) {
                const GLFWvidmode* vm = glfwGetVideoMode(monitors[i]);
                std::ostringstream ss;
                ss << get_monitor_name(monitors[i]);
                if (vm) ss << " (" << vm->width << "x" << vm->height << " @" << vm->refreshRate << "Hz)";
                names.push_back(ss.str());
            }

            if (ImGui::BeginCombo("Monitor", (names.empty() ? "<none>" : names[selected_monitor].c_str()))) {
                for (int n = 0; n < (int)names.size(); ++n) {
                    bool is_selected = (selected_monitor == n);
                    if (ImGui::Selectable(names[n].c_str(), is_selected)) selected_monitor = n;
                    if (is_selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            if (ImGui::Button("Refresh Monitors")) {
                monitors = refresh_monitors();
                if (selected_monitor >= (int)monitors.size()) selected_monitor = (int)monitors.size() - 1;
            }

            ImGui::Separator();

            if (!is_fullscreen) {
                if (ImGui::Button("Go Fullscreen on Selected Monitor")) {
                    if (monitors.empty()) {
                        std::cout << "No monitors available to go fullscreen.\n";
                    } else if (selected_monitor < 0 || selected_monitor >= (int)monitors.size()) {
                        std::cout << "Selected monitor index invalid.\n";
                    } else {
                        // Save previous windowed state
                        glfwGetWindowPos(window, &prev_x, &prev_y);
                        glfwGetWindowSize(window, &prev_w, &prev_h);

                        GLFWmonitor* target = monitors[selected_monitor];
                        const GLFWvidmode* mode = glfwGetVideoMode(target);
                        int mx, my;
                        glfwGetMonitorPos(target, &mx, &my);
                        if (mode) {
                            glfwSetWindowMonitor(window, target, mx, my, mode->width, mode->height, mode->refreshRate);
                        } else {
                            // fallback: use primary monitor video mode
                            GLFWmonitor* primary = glfwGetPrimaryMonitor();
                            const GLFWvidmode* pm = glfwGetVideoMode(primary);
                            int px, py; glfwGetMonitorPos(primary, &px, &py);
                            if (pm) glfwSetWindowMonitor(window, primary, px, py, pm->width, pm->height, pm->refreshRate);
                        }
                        is_fullscreen = true;
                    }
                }
            } else {
                if (ImGui::Button("Restore Windowed Mode")) {
                    // Restore previous windowed position/size
                    glfwSetWindowMonitor(window, nullptr, prev_x, prev_y, prev_w, prev_h, 0);
                    is_fullscreen = false;
                }
            }

            ImGui::End();
        }

        // --- Phase 3 UI: Quad mapping control ---
        {
            ImGui::Begin("Surface Mapping");

            ImGui::Text("Quads: %d", (int)quads.size());

            if (ImGui::Button("Add New Quad")) {
                std::ostringstream ss;
                ss << "Quad_" << quads.size();
                quads.emplace_back(ss.str());
                selected_quad_idx = (int)quads.size() - 1;
            }

            ImGui::SameLine();
            if (ImGui::Button("Delete Selected") && selected_quad_idx >= 0 && selected_quad_idx < (int)quads.size()) {
                quads.erase(quads.begin() + selected_quad_idx);
                selected_quad_idx = -1;
            }

            ImGui::Separator();

            // List of quads
            for (int i = 0; i < (int)quads.size(); ++i) {
                bool is_selected = (selected_quad_idx == i);
                std::string label = std::string(quads[i].name) + "##quad" + std::to_string(i);
                if (ImGui::Selectable(label.c_str(), is_selected)) {
                    selected_quad_idx = i;
                }
            }

            ImGui::Separator();

            if (selected_quad_idx >= 0 && selected_quad_idx < (int)quads.size()) {
                Quad& q = quads[selected_quad_idx];

                ImGui::InputText("Quad Name", q.name, sizeof(q.name));

                ImGui::Text("Corners:");
                for (int i = 0; i < 4; ++i) {
                    float corners[2] = {q.corners[i].x, q.corners[i].y};
                    std::string corner_label = "Corner " + std::to_string(i);
                    ImGui::SliderFloat2(corner_label.c_str(), corners, 0.0f, 1280.0f);
                    q.corners[i] = ImVec2(corners[0], corners[1]);
                }

                if (!is_placing_quad) {
                    if (ImGui::Button("Place Quad Corners (Click on canvas)")) {
                        is_placing_quad = true;
                        quad_placement_corner = 0;
                        std::cout << "Start placing quad: " << q.name << " - Click to place 4 corners\n";
                    }
                } else {
                    ImGui::Text("Placing corner %d/4 - Click on canvas", quad_placement_corner + 1);
                    if (ImGui::Button("Cancel Placement")) {
                        is_placing_quad = false;
                    }
                }
            }

            ImGui::End();
        }

        // --- Render quads on canvas using ImGui draw list ---
        {
            ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
            ImU32 quad_color = ImGui::GetColorU32(ImVec4(0.0f, 1.0f, 0.0f, 0.8f));
            ImU32 selected_color = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 0.0f, 0.8f));
            ImU32 corner_color = ImGui::GetColorU32(ImVec4(1.0f, 0.5f, 0.0f, 1.0f));

            for (int i = 0; i < (int)quads.size(); ++i) {
                const Quad& q = quads[i];
                ImU32 color = (i == selected_quad_idx) ? selected_color : quad_color;

                // Draw quad outline
                for (int j = 0; j < 4; ++j) {
                    int next = (j + 1) % 4;
                    draw_list->AddLine(q.corners[j], q.corners[next], color, 2.0f);
                }

                // Draw corner points
                for (int j = 0; j < 4; ++j) {
                    draw_list->AddCircleFilled(q.corners[j], 4.0f, corner_color);
                }
            }

            // Draw placement helper
            if (is_placing_quad && selected_quad_idx >= 0 && selected_quad_idx < (int)quads.size()) {
                const Quad& q = quads[selected_quad_idx];
                ImU32 help_color = ImGui::GetColorU32(ImVec4(1.0f, 0.0f, 0.0f, 0.5f));
                
                // Highlight the corner being placed
                draw_list->AddCircleFilled(q.corners[quad_placement_corner], 6.0f, help_color);
                
                // Draw a crosshair at mouse position
                ImVec2 mouse = ImGui::GetMousePos();
                draw_list->AddLine(ImVec2(mouse.x - 10, mouse.y), ImVec2(mouse.x + 10, mouse.y), help_color, 1.0f);
                draw_list->AddLine(ImVec2(mouse.x, mouse.y - 10), ImVec2(mouse.x, mouse.y + 10), help_color, 1.0f);
            }
        }

        // --- Phase 4 UI: Media Library (Images/Videos) ---
        {
            ImGui::Begin("Media Library");

            ImGui::Text("Loaded Textures: %d | Video: %s", (int)media_library.textures.size(), 
                       media_library.is_video_loaded ? "YES" : "NO");

            ImGui::InputText("File Path##media", file_input_buffer, sizeof(file_input_buffer));
            ImGui::SameLine();
            if (ImGui::Button("Load Image")) {
                std::string path(file_input_buffer);
                if (!path.empty()) {
                    if (media_library.add_texture(path)) {
                        memset(file_input_buffer, 0, sizeof(file_input_buffer));
                    } else {
                        std::cerr << "Failed to load image: " << path << "\n";
                    }
                }
            }
            
            ImGui::InputText("Video Path##video", file_input_buffer, sizeof(file_input_buffer));
            ImGui::SameLine();
            if (ImGui::Button("Load Video")) {
                std::string path(file_input_buffer);
                if (!path.empty()) {
                    if (media_library.load_video(path)) {
                        memset(file_input_buffer, 0, sizeof(file_input_buffer));
                    } else {
                        std::cerr << "Failed to load video: " << path << "\n";
                    }
                }
            }

            ImGui::Separator();

            // List loaded textures
            if (!media_library.textures.empty()) {
                const char* combo_preview = media_library.selected_texture.empty() ? "<none>" : media_library.selected_texture.c_str();
                if (ImGui::BeginCombo("Texture##select", combo_preview)) {
                    for (auto& pair : media_library.textures) {
                        bool is_selected = (media_library.selected_texture == pair.first);
                        if (ImGui::Selectable(pair.first.c_str(), is_selected)) {
                            media_library.selected_texture = pair.first;
                        }
                        if (is_selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }

            ImGui::Separator();

            // Display video if loaded
            if (media_library.is_video_loaded && media_library.video_texture) {
                ImGui::Text("Video: %s", media_library.selected_texture.c_str());
                ImGui::Text("Resolution: %dx%d | Frames: %d", 
                           media_library.video_decoder.width, 
                           media_library.video_decoder.height,
                           media_library.video_decoder.total_frames);

                // Display video preview
                float preview_size = 200.0f;
                float aspect = (float)media_library.video_decoder.width / (float)media_library.video_decoder.height;
                float preview_w = preview_size;
                float preview_h = preview_size / aspect;
                if (preview_h > preview_size) {
                    preview_h = preview_size;
                    preview_w = preview_size * aspect;
                }

                ImGui::Image((ImTextureID)(intptr_t)media_library.video_texture, ImVec2(preview_w, preview_h),
                             ImVec2(0, 1), ImVec2(1, 0));  // Flip Y for OpenGL

                // Video playback controls
                ImGui::Checkbox("Playing##video", &is_playing);
                
                int frame_slider = media_library.video_decoder.current_frame;
                if (ImGui::SliderInt("Frame##video", &frame_slider, 0, media_library.video_decoder.total_frames - 1)) {
                    media_library.video_decoder.seek_to_frame(frame_slider);
                }
                
                ImGui::Text("Frame: %d / %d", media_library.video_decoder.current_frame, media_library.video_decoder.total_frames);
            } else if (!media_library.is_video_loaded) {
                TextureAsset* selected = media_library.get_selected();
                if (selected && selected->gl_texture) {
                    ImGui::Text("Selected: %s", selected->filepath);
                    ImGui::Text("Resolution: %dx%d", selected->width, selected->height);

                    // Display texture preview (scaled to fit in UI)
                    float preview_size = 200.0f;
                    float aspect = (float)selected->width / (float)selected->height;
                    float preview_w = preview_size;
                    float preview_h = preview_size / aspect;
                    if (preview_h > preview_size) {
                        preview_h = preview_size;
                        preview_w = preview_size * aspect;
                    }

                    ImGui::Image((ImTextureID)(intptr_t)selected->gl_texture, ImVec2(preview_w, preview_h),
                                 ImVec2(0, 1), ImVec2(1, 0));  // Flip Y for OpenGL

                    // Playback controls
                    ImGui::Checkbox("Playing##media", &is_playing);
                    ImGui::SliderFloat("Playback Time##media", &playback_time, 0.0f, 10.0f);
                    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
                } else {
                    ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "No texture selected");
                }
            }

            ImGui::End();
        }

        // --- Phase 5: Update video playback ---
        if (media_library.is_video_loaded && is_playing) {
            media_library.update_video_frame();
        }

        // --- Phase 6 UI: Layer Composition ---
        {
            ImGui::Begin("Layers");

            ImGui::Text("Total Layers: %d", (int)compositor.layers.size());

            if (ImGui::Button("Add Layer")) {
                std::ostringstream ss;
                ss << "Layer_" << compositor.layers.size();
                compositor.add_layer(ss.str());
            }

            ImGui::SameLine();
            if (ImGui::Button("Delete Selected") && compositor.selected_layer_idx >= 0 && compositor.selected_layer_idx < (int)compositor.layers.size()) {
                compositor.remove_layer(compositor.selected_layer_idx);
            }

            ImGui::Separator();

            // Layer list with z-order indicator
            ImGui::Text("Layer Stack (Top to Bottom):");
            for (int i = 0; i < (int)compositor.layers.size(); ++i) {
                Layer& layer = compositor.layers[i];
                bool is_selected = (compositor.selected_layer_idx == i);
                
                std::string display = std::string(layer.visible ? "[V] " : "[H] ") + layer.name;
                if (ImGui::Selectable(display.c_str(), is_selected)) {
                    compositor.selected_layer_idx = i;
                }
                if (is_selected) ImGui::SetItemDefaultFocus();
            }

            ImGui::Separator();

            // Layer properties
            if (compositor.selected_layer_idx >= 0 && compositor.selected_layer_idx < (int)compositor.layers.size()) {
                Layer& layer = compositor.layers[compositor.selected_layer_idx];

                ImGui::InputText("Layer Name##layer", layer.name, sizeof(layer.name));

                ImGui::Checkbox("Visible##layer", &layer.visible);

                ImGui::SliderFloat("Opacity##layer", &layer.opacity, 0.0f, 1.0f);

                // Blend mode dropdown
                const char* blend_modes[] = {"Alpha", "Add", "Multiply"};
                ImGui::Combo("Blend Mode##layer", &layer.blend_mode, blend_modes, 3);

                // Quad assignment dropdown
                if (!quads.empty()) {
                    std::vector<std::string> quad_names;
                    quad_names.push_back("<None>");
                    for (size_t j = 0; j < quads.size(); ++j) {
                        quad_names.push_back(quads[j].name);
                    }

                    const char* quad_preview = (layer.quad_idx < 0 || layer.quad_idx >= (int)quads.size()) ? "<None>" : quads[layer.quad_idx].name;
                    if (ImGui::BeginCombo("Target Quad##layer", quad_preview)) {
                        if (ImGui::Selectable("<None>", layer.quad_idx < 0)) {
                            layer.quad_idx = -1;
                        }
                        for (int q = 0; q < (int)quads.size(); ++q) {
                            bool is_sel = (layer.quad_idx == q);
                            if (ImGui::Selectable(quads[q].name, is_sel)) {
                                layer.quad_idx = q;
                            }
                            if (is_sel) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                } else {
                    ImGui::TextDisabled("No quads available");
                }

                ImGui::Text("Z-Order: %d", layer.z_order);

                // Layer reordering buttons
                if (ImGui::Button("Move Up##layer")) {
                    compositor.move_layer_up(compositor.selected_layer_idx);
                }

                ImGui::SameLine();
                if (ImGui::Button("Move Down##layer")) {
                    compositor.move_layer_down(compositor.selected_layer_idx);
                }
            }

            ImGui::End();
        }

        // --- Phase 7 UI: Scene Management (Save/Load) ---
        {
            ImGui::Begin("Scene Management");

            ImGui::InputText("Scene Name##scene", current_scene.name, sizeof(current_scene.name));
            ImGui::InputTextMultiline("Description##scene", current_scene.description, sizeof(current_scene.description), ImVec2(-1, 50));

            ImGui::Separator();
            ImGui::Text("Save/Load Project File:");

            ImGui::InputText("Save Path##scene", scene_save_path, sizeof(scene_save_path));
            ImGui::SameLine();
            if (ImGui::Button("Save to JSON")) {
                std::string path(scene_save_path);
                if (!path.empty()) {
                    // Sync current state to scene
                    current_scene.quads = quads;
                    current_scene.layers = compositor.layers;
                    
                    json scene_json = current_scene.to_json();
                    try {
                        std::ofstream file(path);
                        file << scene_json.dump(2);
                        file.close();
                        std::cout << "Scene saved to: " << path << "\n";
                    } catch (const std::exception& e) {
                        std::cerr << "Failed to save scene: " << e.what() << "\n";
                    }
                }
            }

            ImGui::InputText("Load Path##scene", scene_load_path, sizeof(scene_load_path));
            ImGui::SameLine();
            if (ImGui::Button("Load from JSON")) {
                std::string path(scene_load_path);
                if (!path.empty()) {
                    try {
                        std::ifstream file(path);
                        json scene_json;
                        file >> scene_json;
                        file.close();
                        
                        if (current_scene.from_json(scene_json)) {
                            // Restore from scene
                            quads = current_scene.quads;
                            compositor.layers = current_scene.layers;
                            compositor.selected_layer_idx = -1;
                            std::cout << "Scene loaded from: " << path << "\n";
                        } else {
                            std::cerr << "Failed to parse scene JSON\n";
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "Failed to load scene: " << e.what() << "\n";
                    }
                }
            }

            ImGui::Separator();
            ImGui::Text("Project Statistics:");
            ImGui::Text("Quads: %d", (int)quads.size());
            ImGui::Text("Layers: %d", (int)compositor.layers.size());
            ImGui::Text("Name: %s", current_scene.name);

            ImGui::End();
        }

        // --- Phase 8 UI: Show Mode Control ---
        if (!show_mode) {
            ImGui::Begin("Show Mode");

            if (ImGui::Button("Enter Show Mode (Ctrl+Shift+P)", ImVec2(-1, 30))) {
                show_mode = true;
            }

            ImGui::Separator();
            ImGui::Text("Show Mode Info:");
            ImGui::Text("Quads to render: %d", (int)quads.size());
            ImGui::Text("Visible layers: %d", (int)compositor.layers.size());
            ImGui::TextDisabled("Press Ctrl+Shift+P to toggle");

            ImGui::End();
        }

        // Rendering
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (show_mode) {
            // Phase 9: Handle Show Mode input
            show_controller.update_layer_visibility((int)compositor.layers.size());
            
            // Spacebar: Play/pause
            static bool space_pressed_last = false;
            bool space_pressed = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
            if (space_pressed && !space_pressed_last) {
                is_playing = !is_playing;
                std::cout << (is_playing ? "Playing" : "Paused") << "\n";
            }
            space_pressed_last = space_pressed;
            
            // Arrow keys: seek or adjust brightness
            if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
                if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
                    show_controller.brightness = std::max(0.1f, show_controller.brightness - 0.01f);
                } else {
                    show_controller.seek_offset = std::max(-10.0f, show_controller.seek_offset - 0.1f);
                }
            }
            if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
                if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
                    show_controller.brightness = std::min(2.0f, show_controller.brightness + 0.01f);
                } else {
                    show_controller.seek_offset = std::min(10.0f, show_controller.seek_offset + 0.1f);
                }
            }
            
            // +/- keys: Global opacity
            if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS) {
                show_controller.global_opacity = std::min(1.0f, show_controller.global_opacity + 0.01f);
            }
            if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS) {
                show_controller.global_opacity = std::max(0.0f, show_controller.global_opacity - 0.01f);
            }
            
            // Number keys 1-9: Toggle layer visibility
            for (int i = 0; i < 9; ++i) {
                int key = GLFW_KEY_1 + i;
                if (glfwGetKey(window, key) == GLFW_PRESS && i < (int)show_controller.layer_overrides.size()) {
                    static std::array<bool, 9> num_pressed_last = {};
                    if (!num_pressed_last[i]) {
                        show_controller.layer_overrides[i] = !show_controller.layer_overrides[i];
                        std::cout << "Layer " << (i+1) << " toggled\n";
                    }
                    num_pressed_last[i] = true;
                } else {
                    static std::array<bool, 9> num_pressed_last = {};
                    num_pressed_last[i] = false;
                }
            }
            
            // H: Toggle OSD
            static bool h_pressed_last = false;
            bool h_pressed = glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS;
            if (h_pressed && !h_pressed_last) {
                show_controller.show_osd = !show_controller.show_osd;
            }
            h_pressed_last = h_pressed;
            
            // O: Toggle all layers
            static bool o_pressed_last = false;
            bool o_pressed = glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS;
            if (o_pressed && !o_pressed_last) {
                bool all_hidden = true;
                for (int i = 0; i < (int)show_controller.layer_overrides.size(); ++i) {
                    if (!show_controller.layer_overrides[i]) all_hidden = false;
                }
                for (int i = 0; i < (int)show_controller.layer_overrides.size(); ++i) {
                    show_controller.layer_overrides[i] = !all_hidden;
                }
            }
            o_pressed_last = o_pressed;
            
            // Phase 8: Render composition to quads
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            // Sort and render layers by z-order
            std::vector<int> layer_indices;
            for (int i = 0; i < (int)compositor.layers.size(); ++i) {
                layer_indices.push_back(i);
            }
            std::sort(layer_indices.begin(), layer_indices.end(),
                     [&](int a, int b) { return compositor.layers[a].z_order < compositor.layers[b].z_order; });

            // Render each layer on its assigned quad
            for (int layer_idx : layer_indices) {
                const Layer& layer = compositor.layers[layer_idx];
                bool layer_visible = show_controller.is_layer_visible(layer_idx, layer.visible);
                if (!layer_visible || layer.quad_idx < 0 || layer.quad_idx >= (int)quads.size()) {
                    continue;
                }

                const Quad& quad = quads[layer.quad_idx];
                GLuint texture = 0;

                // Get texture from media library (simplified: use video if playing, else selected)
                if (media_library.is_video_loaded && media_library.video_texture) {
                    texture = media_library.video_texture;
                } else {
                    TextureAsset* asset = media_library.get_selected();
                    if (asset && asset->gl_texture) texture = asset->gl_texture;
                }

                if (texture) {
                    float final_opacity = layer.opacity * show_controller.global_opacity;
                    projection_renderer.render_quad(quad, texture, final_opacity, layer.blend_mode, show_controller.brightness);
                }
            }

            glDisable(GL_BLEND);
            
            // Phase 9: Render OSD overlay
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            show_controller.render_osd(compositor, media_library);
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            // ESC to exit show mode
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                show_mode = false;
                std::cout << "Exiting Show Mode\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        } else {
            // Render ImGui UI
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }

        // (Platform windows / multi-viewport disabled in this build)

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
