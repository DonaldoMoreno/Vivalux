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

## 🚀 Requisitos

- Python 3.8+
- Dependencias específicas por módulo

## 📚 Desarrollo

Consulta los archivos README.md en cada carpeta de módulo para más información específica.

## 🛠️ Build (Windows - Visual Studio 2022 + vcpkg manifest)

1. Install vcpkg and bootstrap it (see vcpkg docs).
2. From the project root run in a VS developer prompt or configure CMake to use vcpkg toolchain file:

```bash
# Example (PowerShell)
cmake -S . -B build -A x64 -DCMAKE_TOOLCHAIN_FILE="C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

The project uses the `vcpkg.json` manifest. Ensure you have the required libraries installed by letting CMake + vcpkg handle dependencies automatically in manifest mode.

## ✅ Phase 1 status

- Window + render loop with ImGui docking and multi-viewport: implemented in `src/main.cpp`.
