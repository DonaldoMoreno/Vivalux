# Guía Rápida de Inicio - Vivalux Setup Script

## Descripción

El script `setup_project.py` automatiza todo el proceso de configuración y compilación de Vivalux para **Windows**, **macOS** y **Linux**.

## Requisitos Previos

### Todos los Sistemas
- **Git**: Para controlar versiones
- **CMake**: 3.20+
- **Python**: 3.7+

### Windows
- **Visual Studio 2022 Community** (con workload "Desktop development with C++")

### macOS
- **Xcode Command Line Tools**
- **Vulkan SDK**: [https://vulkan.lunarg.com/sdk/home](https://vulkan.lunarg.com/sdk/home)
- **Homebrew**: [https://brew.sh](https://brew.sh)

### Linux
- **GCC/Clang**: Compilador C++20 compatible
- **Build-essential**: `apt install build-essential cmake git`

## Uso

### Opción 1: Python Directo (Multiplataforma)

```bash
# En la raíz del proyecto
python3 setup_project.py
```

### Opción 2: Script Shell (macOS/Linux)

```bash
# Crear un alias opcional
chmod +x setup_project.py
./setup_project.py
```

### Opción 3: PowerShell (Windows)

```powershell
python setup_project.py

# O directamente
python .\setup_project.py
```

## Qué hace el Script

1. **Detección de Plataforma** 
   - Identifica el SO (Windows, macOS, Linux)
   - Muestra información del sistema

2. **Verificación de Dependencias**
   - Comprueba que CMake y Git estén instalados
   - Valida herramientas específicas de plataforma

3. **Inicialización de vcpkg** (si es necesario)
   - Descarga y configura vcpkg
   - Instala dependencias automáticamente

4. **Limpieza**
   - Elimina el directorio `build` anterior
   - Prepara un entorno limpio

5. **Configuración con CMake**
   - Adapta la configuración según la plataforma
   - Usa toolchain de vcpkg cuando está disponible

6. **Compilación**
   - Compila el proyecto en modo Release
   - Muestra el progreso

7. **Resumen Final**
   - Indica ubicación del ejecutable
   - Sugiere próximos pasos

## Salida Esperada

```
======================================================================
  VIVALUX PROJECT SETUP
======================================================================

Project root: /ruta/al/proyecto

======================================================================
  PLATFORM DETECTION
======================================================================
OS: Darwin
Architecture: arm64
Python Version: 3.11.7
macOS Version: 14.2.1

======================================================================
  DEPENDENCY CHECK
======================================================================
✓ Found cmake
✓ Found git
✓ Found brew

✓ All required tools found

➜ Creating build directory
✓ Build directory created

[... CMake configuration output ...]

[... Build output ...]

======================================================================
  SETUP COMPLETE
======================================================================
✓ Project configured and built successfully!

======================================================================
  NEXT STEPS
======================================================================

1. Run the application:
   /ruta/al/proyecto/build/src/VivaLux

2. For development, use:
   cd /ruta/al/proyecto/build
   cmake --build . --config Release

3. To clean and rebuild:
   python3 /ruta/al/proyecto/setup_project.py
```

## Troubleshooting

### Error: "cmake: command not found"
- **Windows**: Instala CMake desde [cmake.org](https://cmake.org)
- **macOS**: `brew install cmake`
- **Linux**: `apt install cmake`

### Error: "vcpkg toolchain not found"
- El script intentará inicializar vcpkg automáticamente
- Si falla, clona manualmente: `git clone https://github.com/microsoft/vcpkg.git`

### Error: "Failed to bootstrap vcpkg"
- En macOS/Linux: Asegúrate que tengas permisos de escritura
- En Windows: Ejecuta PowerShell como administrador

### Error: "Depends on X but finding package X is not enabled"
- En Linux: Instala dependencias del sistema: `sudo apt install libxrandr-dev libxinerama-dev libxi-dev libxext-dev libxcursor-dev`

### Compilación lenta en macOS
- Normal: La compilación Vulkan en la primera pasada es lenta
- Subsecuentes: Usa `cmake --build build` directamente en el directorio build

## Solución Manualrápida si el Script falla

Si el script no funciona, puedes compilar manualmente:

```bash
# Linux/macOS
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Windows (PowerShell)
Remove-Item -Recurse -Force build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Parámetros del Script (Futuro)

Se planea agregar parámetros opcionales:

```bash
python3 setup_project.py --clean-only          # Solo limpiar
python3 setup_project.py --no-build            # Config sin compilar
python3 setup_project.py --verbose             # Salida detallada
python3 setup_project.py --configure-only      # Solo CMake config
```

## Contacto y Soporte

Para reportar problemas con el script, abre un issue en el repositorio con:
- Resultado de `python3 setup_project.py`
- Tu SO y versión
- Salida de errores completa

---

**Última actualización**: 2026-02-16  
**Compatible con**: Python 3.7+, CMake 3.20+
