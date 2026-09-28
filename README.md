<!-- Intro -->
<br />
<div align="center">
  <h3 align="center">Island Graphics Demo</h3>

  <p align="center">
    A GPU-Driven Walking Simulator
  </p>
</div>

<!-- About -->
## About The Project

<div>
  <img src="images/logo.png" alt="Island Demo Logo" width="250">
</div>
<p>
After second year, I wanted a summer project and chose something ambitious: graphics. Throughout the summer I followed the LearnOpenGL tutorials, as evidenced by the repository history. Sometime before the PBR chapter, I decided to make a small demo, not intended to be a generic solution, but a polished final product.
</p>
<p>
Soon after learning instancing, I found my school laptop (running an Intel Iris Xe) was unable to keep up with the workload. Thus began the journey to get decent framerates on a very modest system. I tried to do as much work as possible on the GPU and arrived at a full glMultiDrawElementsIndirectCount (MDIC) pipeline.
</p>

<!-- Features -->
## Features
* Written against the OpenGL 4.6 Core spec, with no extensions.
* All geometry, including the skybox and shadows, is run through the MDIC pipeline.
* GPU frustum culling.
* GPU Hi-Z culling.
* Compute vertex skinning, written back to the main VBOs.
* 32-byte packed vertices including vertex color.
* 2 VAOs total.
* Compute particle simulation smuggled inside the 32-byte vertex.
* Compute vertex explosion (an homage to the LearnOpenGL geometry shader).
* Jolt Physics.
* miniaudio with AABB-style audio triggers, one-shot effects, and character sounds.
* Launch window for configuring graphics settings.
* Load-time cubemap generation for water reflections.
* KTX2 compressed textures, loaded via thread pool.

<!-- Gallery -->
## Gallery

### Screenshots
![Grassy Field Screenshot][grass-screen]
![Space Torus Screenshot][torus-screen]
![Beach Screenshot][beach-screen]

### Vertex Explosion Effect
![Vertex Explosion][explosion-effect]

### Culling Demo
[![GPU Culling Demo][culling-thumb]][culling-video]

<!-- Markdown links & images -->
[grass-screen]: images/screenshot_1.jpg
[torus-screen]: images/screenshot_2.jpg
[beach-screen]: images/screenshot_3.jpg
[explosion-effect]: images/vertex_explosion.webp
[culling-thumb]: https://img.youtube.com/vi/4OVsnVp6WYw/maxresdefault.jpg
[culling-video]: https://www.youtube.com/watch?v=4OVsnVp6WYw

## Performance

### Intel Iris Xe
Average 30-45 FPS

* 1920x1080 resolution
* 2x MSAA 
* 2x anisotropic filtering 
* 1024x1024 shadow maps 
* 512x512 cubemaps

Crouched in heavy grass: 25 FPS

### 4080 Super
Average 600-900 FPS

* 2560x1440 resolution
* 8x MSAA 
* 16x anisotropic filtering 
* 4096x4096 shadow maps 
* 2048x2048 cubemaps

Crouched in heavy grass: ~550 FPS

## Fun Bugs

* Broke the MSVC compiler. Started a [Visual Studio Developer Thread](https://developercommunity.visualstudio.com/t/Fatal-error-C1001:-Internal-compiler-err/11055388) to report it, worked for months to catch it, and got it moved to "Under Consideration". It appears to be a race condition in link time code generation under partial recompilation. The script to reproduce it commented out a single line of code, compiled, uncommented, then recompiled, in a loop.
* Hi-Z. All of it. Including finding out the debug camera was sending its depth information to the Hi-Z buffer. I thought it was broken - turns out when the depth map contains the entire island, and you're testing against that, not much gets culled.

## Build Requirements

## Build Instructions

## AI Use Disclaimer
The following code is 100% AI generated:
* Build scripts (CMake, vcpkg)
* Blender tools (terrain chunking, position generation)

AI was used heavily for debugging, walking through new algorithms, OpenGL API usage, and providing guidance on architectural decisions. Ultimately, the best design came from not consulting AI at all and fixing the code that bothered me.
