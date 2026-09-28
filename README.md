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


