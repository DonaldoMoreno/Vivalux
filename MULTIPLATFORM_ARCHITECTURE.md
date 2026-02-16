# Arquitectura Multiplataforma Vivalux

## Descripción General

Vivalux utiliza una arquitectura abstracta de renderizado que permite ejecutar el mismo código en méltiples plataformas:

- **Windows/Linux:** OpenGL 4.1
- **macOS M1+:** Vulkan + MoltenVK

## Estructura de Componentes

### Interfaz Base: `Renderer` (`src/rendering/Renderer.h`)

Define una interfaz abstracta que encapsula todas las operaciones gráficas:

```cpp
class Renderer {
    // Inicialización
    virtual bool initialize(uint32_t width, uint32_t height) = 0;
    
    // Gestión de texturas
    virtual TextureHandle createTexture(const TextureSpec& spec) = 0;
    virtual void updateTexture(TextureHandle handle, const uint8_t* data, uint32_t data_size) = 0;
    virtual void deleteTexture(TextureHandle handle) = 0;
    
    // Gestión de shaders
    virtual ShaderHandle createShader(const std::string& vs_code, const std::string& fs_code) = 0;
    
    // Uniforms
    virtual void setUniformFloat(const std::string& name, float value) = 0;
    // ... más uniforms
    
    // Renderizado
    virtual void clear(float r, float g, float b, float a) = 0;
    virtual void drawQuad(const Quad& quad, TextureHandle texture, float opacity = 1.0f, BlendMode blend = BlendMode::Alpha) = 0;
};
```

### Implementaciones Específicas

#### 1. **RendererOpenGL** (`src/rendering/RendererOpenGL.{h,cpp}`)

- Utiliza OpenGL 4.1 core
- Compatible con Windows y Linux
- Usa GLAD como loader de OpenGL
- Implementa la interfaz `Renderer` completamente

**Características:**
- Texturas con `glTexImage2D`
- Shaders compilados con GLSL
- VAO/VBO para geometría de quads
- Blend modes usando `glBlendFunc`

#### 2. **RendererVulkan** (`src/rendering/RendererVulkan.{h,cpp}`)

- Utiliza Vulkan API
- Compatible con macOS via **MoltenVK** (traductora de Vulkan a Metal)
- En desarrollo: implementación completa de Vulkan

**Roadmap:**
- [ ] Inicialización de Vulkan Instance/Device
- [ ] Manejo de Swapchain con MoltenVK
- [ ] Compilación de shaders a SPIR-V
- [ ] Renderpass y pipeline setup
- [ ] Command buffer recording y submission

### Factory Pattern (`src/rendering/RendererFactory.cpp`)

Selecciona automáticamente el backend según la plataforma:

```cpp
std::unique_ptr<Renderer> createPlatformRenderer() {
#ifdef __APPLE__
    return std::make_unique<RendererVulkan>();
#else
    return std::make_unique<RendererOpenGL>();
#endif
}
```

## Flujo de Renderizado

1. **Inicialización:**
   ```cpp
   auto renderer = createPlatformRenderer();
   renderer->initialize(1280, 720);
   ```

2. **Carga de Recursos:**
   ```cpp
   TextureHandle tex = renderer->createTexture(spec);
   ShaderHandle shader = renderer->createShader(glsl_vs, glsl_fs);
   ```

3. **Renderización:**
   ```cpp
   renderer->clear(0, 0, 0, 1);
   renderer->useShader(shader);
   renderer->drawQuad(quad, tex, opacity, blend_mode);
   renderer->present();
   ```

## Compilación Multiplataforma

### Windows/Linux (OpenGL)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/src/VivaLux
```

### macOS M1 (Vulkan + MoltenVK)

**Prerequisites:**
```bash
# Instalar Vulkan SDK desde https://vulkan.lunarg.com/sdk/home
brew install vulkan-headers
bundle install
```

**Compilación:**
```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
./build/src/VivaLux
```

## Dependencias

### Windows/Linux
- GLFW 3.x (window management)
- GLAD (OpenGL loader)
- OpenGL 4.1+

### macOS M1
- GLFW 3.x (window management)
- Vulkan SDK
- MoltenVK (incluido en Vulkan SDK)

### Multiplataforma
- ImGui (UI)
- GLM (math)
- FFmpeg (video decoding)
- nlohmann/json (serialización)
- stb_image (image loading)

## Transición de OpenGL a Vulkan (Hoja de Ruta)

1. **Fase 1 (Actual):** Interfaz abstracta + RendererOpenGL funcional
2. **Fase 2:** Implementar RendererVulkan con inicialización básica
3. **Fase 3:** Compilación de shaders GLSL → SPIR-V
4. **Fase 4:** Renderización de quads con perspective mapping
5. **Fase 5:** Composición de capas (blending)
6. **Fase 6:** Validación en macOS M1

## Consideraciones de Diseño

### ¿Por qué Vulkan en macOS?

- **OpenGL**: Deprecado y removido en macOS Ventura+
- **Metal**: Requeriría reescribir todo el código gráfico
- **Vulkan + MoltenVK**: Mantiene la mayoría del código gráfico portable

### ¿Por qué no usar Metal directamente?

- Mayor complejidad
- Metal es específico de Apple (no portable)
- MoltenVK permite reutilizar código Vulkan en múltiples plataformas

### Manejo de Shaders

- **Entrada:** GLSL.
- **OpenGL:** Se compila directamente a código de máquina nativo
- **Vulkan:** Se compila a SPIR-V (bytecode portable)

## Testing

```bash
# Linux headless (Xvfb)
DISPLAY=:99 LIBGL_ALWAYS_SOFTWARE=1 ./build/src/VivaLux &
sleep 2
killall VivaLux

# macOS
./build/src/VivaLux  # Abre ventana gráfica
```

## Contribuciones Futuras

- [ ] Implementación completa de RendererVulkan
- [ ] Compilaciónautomática GLSL → SPIR-V
- [ ] Sincronización de comandos Vulkan
- [ ] Optimizaciones de rendimiento macOS
- [ ] Soporte para AMD Radeon (extensiones VK_RADV)
