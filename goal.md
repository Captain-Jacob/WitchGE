# GAME_VISION_AND_ENGINE_SPEC.md

## 1. Game Concept & Vision
**Title / Working Title:** Dream of a Witch  
**Genre:** First-Person Spellcaster / Speedrunner Platformer  
**Core Hook:** Real-time gesture drawing under motion momentum. The faster and more aggressively the player moves or bunny hops through the level, the more violently the spellcasting board shakes—forcing players to master precise drawing under chaotic physical forces.

---

## 2. Core Gameplay Mechanics

### 2.1 First-Person 3D Movement
* **Character Representation:** 3D "Bean" collider with first-person camera at head height.
* **Controls:**
  * `W, A, S, D`: Directional movement.
  * `Mouse`: Look around (pitch/yaw camera rotation).
  * `Space`: Jump.
  * `C`: Crouch (lowers camera height and collision profile).
* **Physics & Momentum:**
  * Standard walking speed (~4–8 m/s).
  * **Bunny Hopping:** Air acceleration physics that preserve and stack horizontal momentum when landing consecutive timed jumps.

### 2.2 Spellcasting Slate & Drawing UI
* **Activation:** Pressing `P` or `J` slides a white drawing panel up from the bottom of the screen.
* **Visibility:** The panel covers the lower portion of the screen, leaving the top 35% open so the player maintains situational awareness of the level.
* **Mouse Input:** Drawing strokes are captured using mouse inputs (`Left Click` drag) directly onto the canvas buffer.

### 2.3 Momentum-Proportional Screen Shake
* Drawing difficulty scales directly with horizontal velocity:
  * **Standing Still (0 m/s):** $0\%$ shake. Perfect precision.
  * **Walking (~4–8 m/s):** Mild, subtle trembling.
  * **Bunny Hopping / High Speed (12+ m/s):** Violent canvas shaking, forcing chaotic or skilled precision drawing.

### 2.4 YOLO Gesture Inference & Casting
* Pressing `J` while the panel is active triggers spell evaluation.
* The drawing strokes are converted into a $640 \times 640$ normalized float tensor array and passed to the ONNX Runtime engine.
* The YOLO model outputs the recognized sign class (loaded dynamically from `sign_categories.json`).
* The recognized spell name displays on-screen, the canvas clears, and the panel slides back down.

---

## 3. Technical Engine Architecture

Since we are building a custom, lightweight game engine tailored specifically for this game, the architecture will strip away general-purpose engine overhead in favor of pure performance and low latency.