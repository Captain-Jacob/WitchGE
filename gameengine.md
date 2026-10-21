### 3.1 Tech Stack
* **Primary Language:** C++20
* **Windowing & Input:** GLFW or SDL2 (raw mouse input capture)
* **Graphics API:** OpenGL 4.5 / Vulkan (3D perspective grid, ambient lighting, UI overlays)
* **Math Library:** GLM (OpenGL Mathematics for vectors, matrices, quaternions)
* **Inference Engine:** Microsoft ONNX Runtime C/C++ API
* **Build System:** CMake

### 3.2 Performance & Memory Targets
* **Total RAM Target:** $< 30 \text{ MB}$ at runtime (including the loaded YOLO model).
* **Binary Size:** $< 15 \text{ MB}$ standalone executable.
* **Inference Latency:** $< 2 \text{ ms}$ per gesture frame evaluation.
* **Frame Rate:** Locked 144+ FPS without stutter during evaluation.

---

## 4. Engine & Game Development Roadmap

### Phase 1: Core Engine Skeleton
- [ ] Set up CMake project with C++20 and GLFW/GLM.
- [ ] Create basic windowing, raw mouse input capture, and main game loop.
- [ ] Implement OpenGL 3D perspective camera and basic flat floor rendering.

### Phase 2: 3D Movement & Bunny Hop Physics
- [ ] Build capsule collision against flat/mesh terrain.
- [ ] Program WASD movement, mouse look, and crouching height lerping.
- [ ] Implement air acceleration math for bunny hop velocity stacking.

### Phase 3: Sliding Canvas & Shake System
- [ ] Implement 2D orthographic overlay rendering for the drawing panel.
- [ ] Code vector line drawing algorithms for mouse stroke input.
- [ ] Build momentum-proportional shake matrix ($Shake = f(Velocity)$).

### Phase 4: Native ONNX Integration
- [ ] Link `onnxruntime.dll` / `libonnxruntime.so` directly into the C++ executable.
- [ ] Write GPU/RAM vector-to-tensor preprocessor ($640 \times 640$ CHW normalization).
- [ ] Implement output parsing (Argmax evaluation against `sign_categories.json`).

### Phase 5: Polish & Packaging
- [ ] Add visual feedback for spell casting.
- [ ] Build single self-contained executable distribution package.