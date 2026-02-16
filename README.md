# Vivalux

Advanced Visual Projection Architecture

## 📁 Estructura del Proyecto

```
Vivalux/
│
├── Core/                          # Motor principal de la aplicación
│   ├── Render Engine/             # Motor de renderizado
│   ├── Mapping Engine/            # Motor de mapeo de proyecciones
│   ├── Media Manager/             # Gestor de contenido multimedia
│   └── Scene System/              # Sistema de escenas y elementos
│
├── UI/                            # Interfaces de usuario
│   ├── Surface Editor/            # Editor de superficies
│   ├── Media Library/             # Biblioteca de medios
│   ├── Live Control Panel/        # Panel de control en vivo
│   └── Settings/                  # Configuración general
│
├── IO/                            # Entrada/Salida de datos
│   ├── MIDI Input/                # Controladores MIDI
│   ├── OSC Input/                 # Mensajes OSC en red
│   └── Audio Analysis/            # Análisis de audio
│
└── Projects/                      # Gestión de proyectos
	└── JSON Configurations/       # Archivos de configuración
```

## 🎯 Módulos Principales

### 🎨 **Core** - Motor Central
Contiene los componentes fundamentales del sistema:
- **Render Engine**: Renderizado de contenido visual
- **Mapping Engine**: Mapeo dinámico de proyecciones
- **Media Manager**: Gestión centralizada de media
- **Scene System**: Organización de elementos visuales

### 🖥️ **UI** - Interfaz de Usuario
Herramientas visuales para usuarios:
- **Surface Editor**: Edición visual de superficies
- **Media Library**: Biblioteca de contenido
- **Live Control Panel**: Control en tiempo real
- **Settings**: Configuración de aplicación

### 🎛️ **IO** - Entrada/Salida
Integración con dispositivos externos:
- **MIDI Input**: Controladores MIDI
- **OSC Input**: Mensajes OSC en red
- **Audio Analysis**: Análisis reactivo de audio

### 📁 **Projects** - Proyectos
- **JSON Configurations**: Persistencia de proyectos

## 🚀 Requisitos Generales

- CMake 3.20+
- C++20 compatible compiler
- vcpkg (para gestión de dependencias)

### Windows
- Visual Studio 2022 (Community, Professional o Enterprise)
- Windows 10/11

### macOS
- Xcode 14+ (Command Line Tools)
- macOS 12+
- Apple Silicon (M1/M2/M3) o Intel (x86_64)

## 📖 Instalación y Configuración

### 🪟 Windows (OpenGL)

#### Requisitos previos
1. Instalar **Visual Studio 2022**:
   - Descargar desde https://www.visualstudio.com/
   - Seleccionar workload: "Desktop development with C++"

2. Instalar **CMake** (si no está incluido en Visual Studio):
   - Descargar desde https://cmake.org/download/
   - Agregar a PATH durante instalación

3. Instalar **vcpkg**:
   ```bash
   git clone https://github.com/microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   ```

#### Compilación

1. **Clonar el repositorio**:
   ```bash
   git clone https://github.com/DonaldoMoreno/Vivalux.git
   cd Vivalux
   ```

2. **Configurar CMake con vcpkg**:
   ```powershell
   # Abrir PowerShell o Command Prompt en el directorio del proyecto
   $VCPKG_PATH = "C:/path/to/vcpkg"  # Cambiar con tu ruta de vcpkg
   
   cmake -S . -B build -A x64 `
     -DCMAKE_TOOLCHAIN_FILE="$VCPKG_PATH/scripts/buildsystems/vcpkg.cmake"
   ```

3. **Compilar**:
   ```bash
   cmake --build build --config Release
   ```

4. **Ejecutar**:
   ```bash
   .\build\src\Release\VivaLux.exe
   ```

#### Solución de problemas (Windows)
- Si falta vcpkg: asegúrate de que la variable de entorno `CMAKE_TOOLCHAIN_FILE` apunte a `vcpkg.cmake`
- Si hay errores de compilación: verifica que tienes Visual Studio 2022 instalado y actualizado
- Para depuración: usa `cmake --build build --config Debug`

---

### 🍎 macOS (Vulkan + MoltenVK)

#### Requisitos previos

1. **Instalar Xcode Command Line Tools**:
   ```bash
   xcode-select --install
   ```

2. **Instalar Homebrew** (si no lo tienes):
   ```bash
   /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
   ```

3. **Instalar dependencias del sistema**:
   ```bash
   brew install cmake pkg-config
   ```

4. **Instalar vcpkg**:
   ```bash
   git clone https://github.com/microsoft/vcpkg.git
   cd vcpkg
   ./bootstrap-vcpkg.sh
   export VCPKG_ROOT="$(pwd)"
   ```

#### Compilación

1. **Clonar el repositorio**:
   ```bash
   git clone https://github.com/DonaldoMoreno/Vivalux.git
   cd Vivalux
   ```

2. **Configurar CMake con vcpkg**:
   ```bash
   export VCPKG_PATH="/path/to/vcpkg"  # Tu ruta de vcpkg
   
   cmake -S . -B build \
     -DCMAKE_TOOLCHAIN_FILE="$VCPKG_PATH/scripts/buildsystems/vcpkg.cmake" \
     -DCMAKE_BUILD_TYPE=Release
   ```

3. **Compilar**:
   ```bash
   cmake --build build --config Release
   ```

4. **Ejecutar**:
   ```bash
   ./build/src/VivaLux
   ```

#### Instalación como aplicación macOS (Opcional)

Para crear un `.app` Bundle:
```bash
mkdir -p build/VivaLux.app/Contents/MacOS
mkdir -p build/VivaLux.app/Contents/Resources

cp ./build/src/VivaLux build/VivaLux.app/Contents/MacOS/
cp ./src/resources/Info.plist build/VivaLux.app/Contents/  # si existe

# Ejecutar como aplicación
open build/VivaLux.app
```

#### Hardware requerido (macOS)

- **Recomendado**: Apple Silicon (M1/M2/M3+)
  - Vulkan vía MoltenVK (mejor rendimiento)
  - Compilación nativa ARM64

- **Compatible**: Intel Mac (x86_64)
  - Vulkan vía MoltenVK
  - Compilación Intel compatible

El software selecciona automáticamente Vulkan en macOS.

#### Solución de problemas (macOS)

- **Error "MoltenVK not found"**: Instala mediante vcpkg en manifest mode (automático)
- **Error de compilación de shaders**: Asegúrate de que `glslangValidator` esté disponible:
  ```bash
  which glslangValidator
  # Si no existe: brew install glslang
  ```
- **Permisos de ejecución**: 
  ```bash
  chmod +x ./build/src/VivaLux
  ```

---

## 📊 Arquitectura Multiplatforma

Vivalux usa una arquitectura renderer-agnóstica:

```
┌────────────────────────────────────────┐
│         main.cpp (UI + Lógica)         │
├────────────────────────────────────────┤
│        Unified Renderer Interface      │
│      (TextureHandle, BlendMode, etc)   │
├────────────────────────────────────────┤
│  RendererOpenGL (Windows/Linux)        │  RendererVulkan (macOS)
│  - GLSL shaders                        │  - SPIR-V shaders
│  - GL texture management               │  - Vulkan image/sampler
│  - Direct OpenGL calls                 │  - Descriptor sets
└────────────────────────────────────────┘
```

**Ventajas**:
- ✅ Un único código fuente
- ✅ Selección automática de backend
- ✅ Rendimiento nativo en cada plataforma
- ✅ Fácil mantenimiento

---

## 🎮 Uso Básico

### Interfaz Principal

1. **Quad Mapping**:
   - Crea quads haciendo clic en "Place Quad Corners"
   - Arrastra las esquinas para ajustar
   - Asigna texturas desde la librería

2. **Media Library**:
   - Carga imágenes (PNG, JPG)
   - Carga videos (MP4, MKV, etc.)
   - Vista previa en tiempo real

3. **Composer**:
   - Crea capas (Layers)
   - Configura opacidad y blend modes
   - Ordena por z-order

4. **Show Mode** (Ctrl+Shift+P):
   - Visualiza en pantalla completa
   - Controla con teclado:
     - **Espacio**: Play/Pause
     - **Flechas**: Seek/Brightness
     - **+/-**: Opacidad global
     - **1-9**: Toggle capas
     - **ESC**: Salir

### Guardado de Proyectos

Los proyectos se guardan en formato JSON:
```json
{
  "version": 1,
  "quads": [...],
  "layers": [...],
  "textures": [...]
}
```

---

## 🛠️ Desarrollo

### Compilación con Debug

```bash
# Windows
cmake -S . -B build -A x64 -DCMAKE_TOOLCHAIN_FILE="..." -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug

# macOS
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="..." -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

### Estructura de Código

- `src/main.cpp`: Aplicación principal + UI
- `src/rendering/Renderer.h`: Interfaz abstracta
- `src/rendering/RendererOpenGL.cpp`: Implementación OpenGL
- `src/rendering/RendererVulkan.cpp`: Implementación Vulkan
- `src/rendering/RendererFactory.cpp`: Selección de backend

### Contribuir

1. Fork el repositorio
2. Crea rama feature: `git checkout -b feature/nombre`
3. Commit cambios: `git commit -m "feat: descripción"`
4. Push: `git push origin feature/nombre`
5. Pull Request

---

## 📚 Documentación Adicional

- [MULTIPLATFORM_ARCHITECTURE.md](MULTIPLATFORM_ARCHITECTURE.md) - Detalles técnicos de arquitectura
- [Fases de Desarrollo](phases.md) - Roadmap y status de features

---

## 📄 Licencia

[Especificar licencia del proyecto]

## 👨‍💻 Autor

**Donaldo Moreno**  
Proyecto: Vivalux - Advanced Visual Projection Architecture

---

## 📞 Soporte

Para reportar bugs o solicitar features:
1. Abre un issue en GitHub
2. Describe el problema/solicitud
3. Incluye screenshots o logs si es relevante
