# Engine Architecture & Rendering Pipeline
This project is a high-performance, multi-threaded 3D software renderer written entirely from scratch in C. It does not rely on hardware acceleration (OpenGL/Vulkan/DirectX) for graphics processing. Instead, it implements the entire graphics pipeline—from vertex transformation to rasterization—on the CPU, utilizing data-oriented design, dynamic thread pools, and cache-friendly memory access patterns.

Below is the detailed step-by-step breakdown of a single frame's journey through the engine.

# 1. Asset Management & Data Layout (SoA)
Before the pipeline even begins, 3D models are parsed and loaded into memory using a Structure of Arrays (SoA) layout.
Instead of storing vertices as an array of structs `([xyz, xyz, xyz])`, the engine stores discrete arrays for each attribute `([x, x, x], [y, y, y], [z, z, z])`. This maximizes CPU cache-line efficiency and aligns the data perfectly for potential SIMD vectorization.

# 2. Scene Traversal & High-Level Culling (Main Thread)
The frame begins in the Scene module. To prevent the rendering pipeline from choking on unnecessary data, the engine aggressively culls objects and lights before generating Draw Calls.

- **Object-Level Frustum Culling**: The engine calculates the center point of every entity's bounding volume, transforms it into Clip Space, and checks it against the camera's viewport margins. If the entity is completely off-screen or behind the camera, it is instantly discarded.

- **Per-Object Light Culling** (Forward Light Binning): Instead of looping through all 500+ lights in the fragment shader per pixel, the CPU performs spatial distance checks between the active lights and the entity. A highly optimized `Uniforms` payload is created containing only a lightweight `uint16_t` array of the indices of lights that actually touch the object. This reduces memory bandwidth across threads from dozens of megabytes per frame down to a few kilobytes.

- **Draw Call Submission**: Surviving entities copy their local uniforms to a thread-safe uniform pool and record a Draw Call into the geometry batch.

# 3. Vertex Processing Phase (`STAGE_VERTEX`)
A custom thread pool wakes up. Worker threads dynamically grab batches of vertices from the submitted Draw Calls using lock-free atomic counters (`atomic_fetch_add`) to perfectly load-balance the work.

- **Vertex Shading**: Vertices are multiplied by their MVP (Model-View-Projection) matrices to transform them from Local Space -> World Space -> Clip Space.

- **Perspective-Correct Setup**: For vertices that pass the near-plane, the engine calculates the inverse depth (`inv_w = 1.0 / w`). Crucially, vertex attributes (like world position and normals) are pre-multiplied by `inv_w `to prepare them for perspective-correct interpolation later in the pipeline.

- **Screen Mapping**: NDC (Normalized Device Coordinates) are mapped to actual 2D screen pixel coordinates.

# 4. Primitive Assembly & Culling (`STAGE_ASSEMBLE`)
With vertices processed, the threads move on to assemble them into triangles. This stage filters out invisible geometry at the granular triangle level.

- Near-Plane Culling: If any vertex of a triangle is behind the camera (`w < 0`), the triangle is discarded.

- Screen-Space Frustum Culling: A quick 2D bounding box is calculated. If the triangle is entirely outside the screen boundaries, it is discarded.

- Backface Culling: The engine calculates the signed area of the triangle using 2D edge-cross products.

- Assembly: Surviving triangles are safely appended to a massive, globally pre-allocated triangle array.

# 5. Spatial Binning (Main Thread)
To allow for multi-threaded rasterization without race conditions on the depth buffer, the screen space is divided into a grid of 2D Tiles (e.g., 100x100 pixels).

- The main thread loops through all active assembled triangles.

- It calculates an exact bounding box for each triangle (using `floorf` and `ceilf` to prevent edge-truncation artifacts).

- Triangles are assigned to every tile their bounding box overlaps, building a list of triangle indices per tile (`tile_tri_indices`).

# 6. Rasterization Phase (`STAGE_RASTER`)
The worker threads wake up again, this time dynamically claiming specific screen Tiles via atomic fetching. Because each thread owns a distinct sector of the screen, there are no lock contentions on the pixel/depth buffers.

- **Edge Equations**: For every pixel in the triangle's bounding box within the tile, 2D edge functions compute the Barycentric Coordinates (`l0, l1, l2`).

- **Raster Rules**: Only pixels with positive barycentric weights (strictly inside the triangle) are evaluated.

- **Depth Testing (Z-Buffer**): The fragment's depth is interpolated and checked against the 1D depth buffer array. If the pixel is occluded, the shader is skipped.

# 7. Fragment Shading & Lighting
For visible pixels, the custom fragment shaders (e.g., `fs_multi_light_smooth`) are executed.

- **Perspective-Correct Interpolation**: The engine reconstructs the true perspective depth (`w_true`) using the barycentric weights and the `inv_w` values prepared in the vertex shader. All vertex attributes (world position, normals) are multiplied by `w_true` to undo the affine texture warping effect, guaranteeing mathematically perfect 3D perspective.

- **Flat vs. Smooth Normals**: Depending on the material, the shader either interpolates vertex normals (for curved surfaces) or reconstructs flat normals using 3D cross-products of the un-warped world positions (for hard surfaces).

- **Blinn-Phong Illumination**: The shader iterates only through the pre-culled active lights assigned to this object. It computes diffuse lambertian scattering, attenuation (with a smooth distance fade-out), and uses a fast sequential-squaring power approximation for specular highlights (avoiding expensive `libm` `powf` calls).

- Color Output: Final RGB values are clamped and packed into a `uint32_t` color buffer.

# 8. Presentation
Once all tiles are rasterized, the worker threads sleep. The main thread takes the finalized `uint32_t `color buffer and uploads it directly to an SDL Streaming Texture to be presented to the window.