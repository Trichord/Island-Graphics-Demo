<!-- Intro -->
<br />
<div align="center">
  <h3 align="center">Island Graphics Demo</h3>

  <p align="center">
    A GPU Driven Walking Simulator
  </p>
</div>

<!-- About -->
## About The Project

<div>
  <img src="images/logo.png" alt="Island Demo Logo" width="250">
</div>
After second year, I decided I needed a summer project and settled on something ambitious: graphics. Throughout the summer I followed the LearnOpenGL tutorials, as evidenced by the repository history. Eventually I settled on the idea of making a small demo. A discrete unit, not intended to be a generic solution, but a polished final product.

Soon after learning instancing, I found my school laptop (running an Intel Iris Xe) was unable to keep up with the workload. Thus began the journey to get decent framerates on a very modest system. Eventually I ended up trying to do as much work as possible on the GPU and arrived at a full glMultiDrawElementsIndirectCount (MDIC) pipeline.

<!-- Features -->
## Features
* Written against 4.6 core spec, no extensions.
* All geometry, including the skybox and all shadows, are run through the same MDIC pipeline.
* GPU frustum culling.
* Hierarchical Z-Buffer occlusion culling.
* Vertex skinning done in compute and written back to the VBOs (vertex shaders never know skinning happened).
* 32 byte packed vertices including vertex color.
* 2 VAOs total.
* Compute particle simulation smuggled inside the 32 byte vertex.
* An homage to the LearnOpenGL geometry shader, done in compute and based off player distance.
* Jolt physics.
* MiniAudio with AABB style audio triggers, one-shot effects, and character sounds.
* Launch window for configuring graphics settings.
* Load-time cubemap generation for water reflections
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


<!-- Markdown links & images -->
[grass-screen]: images/screenshot_1.jpg
[torus-screen]: images/screenshot_2.jpg
[beach-screen]: images/screenshot_3.jpg
[explosion-effect]: images/vertex_explosion.webp
[culling-demo]: https://youtu.be/4OVsnVp6WYw


