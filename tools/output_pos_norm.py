# Usage: Create a Geometry Node modifier on your target mesh (terrain, etc.) and use weight painting, distribute points on face, and point to vertex.
# After turning points into single vertices, this script will output their position and normal, allowing instanced geometry to either use world up,
# or the underlying mesh.

import bpy
from mathutils import Vector

points_obj = bpy.context.active_object

# Original (unmodified) terrain mesh, needed to calculate normals. Must be the same mesh as the one used to generate the instances.
terrain = bpy.data.objects["island"]  

depsgraph = bpy.context.evaluated_depsgraph_get()

eval_points = points_obj.evaluated_get(depsgraph)
point_mesh = eval_points.to_mesh()
eval_terrain = terrain.evaluated_get(depsgraph)

# Output file
path = bpy.path.abspath("//model.txt")

# Small offset to push geometry down and avoid floating
offset = 0.005

# Toggle using world up or the underlying geometry
use_flat_normal = True

with open(path, "w") as f:
    for v in point_mesh.vertices:
        co = v.co

	# World Up
        if use_flat_normal:
            nx, ny, nz = 0.0, 1.0, 0.0

	# Blender Z-up → Y-up coordinate system
        else:
            result, closest, normal, face_idx = eval_terrain.closest_point_on_mesh(co)
            if result:
                nx, ny, nz = normal.x, normal.z, -normal.y
            else:
                nx, ny, nz = 0.0, 1.0, 0.0
        f.write(
            f"{co.x:.6f} {co.z - offset:.6f} {-co.y:.6f} "
            f"{nx:.6f} {ny:.6f} {nz:.6f}\n"
        )

eval_points.to_mesh_clear()
eval_terrain.to_mesh_clear()
print("Done")