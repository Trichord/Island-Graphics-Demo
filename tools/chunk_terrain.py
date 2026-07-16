"""
Terrain Chunker for Blender
===========================
Select your terrain mesh, run from the scripting tab.
Bins faces by centroid into an NxN grid, then separates
each cell via Separate by Selection. No triangle splitting.
"""

import bpy
import bmesh
from collections import defaultdict

GRID_SIZE = 16
ERROR_LOG = "//chunk_errors.log"
EPSILON = 1e-9


def _log_error(msg):
    print(msg)  # visible in the console when run from the scripting tab
    path = bpy.path.abspath(ERROR_LOG)
    with open(path, "a") as f:
        f.write(msg + "\n")


def chunk_terrain(grid_size=16):

    src = bpy.context.active_object
    if not src or src.type != 'MESH':
        _log_error("chunk_terrain: no mesh object selected")
        return

    if bpy.context.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')

    original_name = src.name
    mesh = src.data

    if len(mesh.polygons) == 0:
        _log_error("chunk_terrain: mesh has no faces")
        return

    world = src.matrix_world

    world_verts = [world @ v.co for v in mesh.vertices]
    min_x = min(v.x for v in world_verts)
    max_x = max(v.x for v in world_verts)
    min_y = min(v.y for v in world_verts)
    max_y = max(v.y for v in world_verts)

    extent_x = max_x - min_x
    extent_y = max_y - min_y

    if extent_x < EPSILON or extent_y < EPSILON:
        _log_error("chunk_terrain: mesh has (near-)zero extent on one axis")
        return

    cell_w = extent_x / grid_size
    cell_h = extent_y / grid_size

    # tag every face with its cell id
    bpy.ops.object.select_all(action='DESELECT')
    bpy.context.view_layer.objects.active = src
    src.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')

    bm = bmesh.from_edit_mesh(mesh)
    bm.faces.ensure_lookup_table()

    tag = bm.faces.layers.int.get('_chunk_cell')
    if tag:
        bm.faces.layers.int.remove(tag)
    tag = bm.faces.layers.int.new('_chunk_cell')

    cells = defaultdict(int)
    for face in bm.faces:
        c = world @ face.calc_center_median()
        gx = min(int((c.x - min_x) / cell_w), grid_size - 1)
        gy = min(int((c.y - min_y) / cell_h), grid_size - 1)
        cell_id = gy * grid_size + gx
        face[tag] = cell_id
        cells[cell_id] += 1

    bmesh.update_edit_mesh(mesh)
    bpy.ops.object.mode_set(mode='OBJECT')

    sorted_cells = sorted(cells.keys())

    # clean up previous run
    col_name = f"{original_name}_chunks"
    if col_name in bpy.data.collections:
        old_col = bpy.data.collections[col_name]
        for ob in list(old_col.objects):
            bpy.data.objects.remove(ob, do_unlink=True)
        bpy.data.collections.remove(old_col)

    col = bpy.data.collections.new(col_name)
    bpy.context.scene.collection.children.link(col)

    # separate each cell, leave last in original
    cells_to_split = sorted_cells[:-1]
    last_cell = sorted_cells[-1]
    chunks = []

    for cell_id in cells_to_split:
        gx = cell_id % grid_size
        gy = cell_id // grid_size

        # Deselect everything at the object level first. Otherwise
        # previously-separated chunks stay selected, mode_set(EDIT)
        # enters multi-object edit, and separate() rips geometry out
        # of the wrong objects.
        bpy.ops.object.select_all(action='DESELECT')
        bpy.context.view_layer.objects.active = src
        src.select_set(True)
        bpy.ops.object.mode_set(mode='EDIT')

        # Clear any stale mesh-level selection, then select in face mode.
        bpy.ops.mesh.select_mode(type='FACE')
        bpy.ops.mesh.select_all(action='DESELECT')

        bm = bmesh.from_edit_mesh(src.data)
        bm.faces.ensure_lookup_table()
        tag = bm.faces.layers.int['_chunk_cell']

        for face in bm.faces:
            face.select = (face[tag] == cell_id)

        # Flush face selection down to edges/verts so separate()
        # takes exactly the tagged faces and nothing else.
        bm.select_flush_mode()

        bmesh.update_edit_mesh(src.data)

        before = set(bpy.data.objects[:])
        bpy.ops.mesh.separate(type='SELECTED')
        bpy.ops.object.mode_set(mode='OBJECT')
        after = set(bpy.data.objects[:])

        new_objs = after - before
        if not new_objs:
            _log_error(f"chunk_terrain: separate produced no object for cell {gx},{gy}")
            continue
        if len(new_objs) > 1:
            _log_error(f"chunk_terrain: separate produced multiple objects for cell {gx},{gy}")

        chunk = new_objs.pop()
        chunk.name = f"chunk_{gx:02d}_{gy:02d}"
        chunk.select_set(False)

        for c in chunk.users_collection:
            c.objects.unlink(chunk)
        col.objects.link(chunk)
        chunks.append(chunk)

    # rename original as the last cell
    last_gx = last_cell % grid_size
    last_gy = last_cell // grid_size
    src.name = f"chunk_{last_gx:02d}_{last_gy:02d}"

    for c in src.users_collection:
        c.objects.unlink(src)
    col.objects.link(src)
    chunks.append(src)

    # strip temp attribute
    for chunk in chunks:
        if '_chunk_cell' in chunk.data.attributes:
            chunk.data.attributes.remove(chunk.data.attributes['_chunk_cell'])

    print(f"chunk_terrain: created {len(chunks)} chunks in collection '{col_name}'")


chunk_terrain(grid_size=GRID_SIZE)
