# Graphixxx

A C++ shader playground using OpenGL, GLFW, Dear ImGui, and FFmpeg. Build the `x64` configuration with vcpkg integration enabled.

## Use

Write `mainImage(out vec4 fragColor, in vec2 fragCoord)` in the editor. Load images into `iChannel0`–`iChannel3`; built-ins include `u_time`, `u_resolution`, and `u_mouse`. Shaders recompile after typing pauses; **Ctrl+S** compiles immediately. Use **File** to open/save shaders and export GIF/MP4.

## Custom parameters

Add an annotation above `mainImage` to create a UI control:

```glsl
// @param float intensity 0.0 1.0 0.5
// @param vec3 tint 0.0 0.0 0.0 1.0 1.0 1.0 1.0 0.8 0.6
```

Format: `// @param type name min... max... default...`. Vectors list one value per component for each group; `vec3` uses 3 minimums, 3 maximums, and 3 defaults.
