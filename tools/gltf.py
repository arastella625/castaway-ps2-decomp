#!/usr/bin/env python3
"""Export the game's models from your own disc image as binary glTF, with their rigs.

Usage:
  python -I tools/gltf.py export  <game.iso> <out_dir> <name_substring>
  python -I tools/gltf.py animate <game.iso> <out_dir> <model_name> <animation_substring>

Each matching model becomes one .glb holding its meshes and textures, and its skeleton and
skin weights when its rig is known. animate writes one model with every matching animation
that fits its rig. These files are game data: keep them out of the repo.
docs/ASSET_FORMATS.md has the conventions used here.
"""
import json
import math
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I leaves this out
import anim  # noqa: E402
import arc  # noqa: E402
import model as mdl  # noqa: E402
import skeleton as skel  # noqa: E402
import texture as tex  # noqa: E402

GLB_MAGIC = 0x46546C67  # "glTF"
GLB_VERSION = 2
JSON_CHUNK = 0x4E4F534A
BIN_CHUNK = 0x004E4942
FLOAT, UNSIGNED_SHORT, UNSIGNED_INT = 5126, 5123, 5125
ARRAY_BUFFER, ELEMENT_ARRAY_BUFFER = 34962, 34963
ALPHA_CUTOFF = 0.5
OPAQUE_ALPHA = 128
FRAMES_PER_SECOND = 30


def to_y_up(vector):
    """Turn the game's Z-up axes into glTF's Y-up. This is a rotation, so winding holds."""
    x, y, z = vector
    return (x, z, -y)


def quaternion_to_y_up(rotation):
    x, y, z, w = rotation
    return (x, z, -y, w)


def conjugate(q):
    return (-q[0], -q[1], -q[2], q[3])


def multiply(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw,
            aw * bw - ax * bx - ay * by - az * bz)


def rotate(q, v):
    return multiply(multiply(q, (v[0], v[1], v[2], 0.0)), conjugate(q))[:3]


def normalised(q):
    length = math.sqrt(sum(c * c for c in q))
    return tuple(c / length for c in q)


def inverse_bind_matrix(position, rotation):
    """Return the column-major inverse of a joint's world transform."""
    x, y, z, w = conjugate(rotation)
    r = ((1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)),
         (2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)),
         (2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)))
    t = [-sum(r[i][k] * position[k] for k in range(3)) for i in range(3)]
    return (r[0][0], r[1][0], r[2][0], 0.0, r[0][1], r[1][1], r[2][1], 0.0,
            r[0][2], r[1][2], r[2][2], 0.0, t[0], t[1], t[2], 1.0)


class GlbBuilder:
    """Collects binary views and accessors for one file."""

    def __init__(self):
        self.binary = bytearray()
        self.gltf = {'asset': {'version': '2.0', 'generator': 'castaway tools/gltf.py'},
                     'scene': 0, 'scenes': [{'nodes': []}], 'nodes': [], 'meshes': [],
                     'buffers': [{}], 'bufferViews': [], 'accessors': []}

    def add_view(self, data, target=None):
        while len(self.binary) % 4:
            self.binary.append(0)
        view = {'buffer': 0, 'byteOffset': len(self.binary), 'byteLength': len(data)}
        if target:
            view['target'] = target
        self.binary += data
        self.gltf['bufferViews'].append(view)
        return len(self.gltf['bufferViews']) - 1

    def add_accessor(self, rows, kind, component, code, target, bounds=False):
        """Pack rows of numbers into a view and describe it."""
        flat = [value for row in rows for value in row]
        data = struct.pack('<%d%s' % (len(flat), code), *flat)
        accessor = {'bufferView': self.add_view(data, target), 'componentType': component,
                    'count': len(rows), 'type': kind}
        if bounds:
            accessor['min'] = [min(row[i] for row in rows) for i in range(len(rows[0]))]
            accessor['max'] = [max(row[i] for row in rows) for i in range(len(rows[0]))]
        self.gltf['accessors'].append(accessor)
        return len(self.gltf['accessors']) - 1

    def add_indices(self, indices):
        data = struct.pack('<%dI' % len(indices), *indices)
        view = self.add_view(data, ELEMENT_ARRAY_BUFFER)
        self.gltf['accessors'].append({'bufferView': view, 'componentType': UNSIGNED_INT,
                                       'count': len(indices), 'type': 'SCALAR'})
        return len(self.gltf['accessors']) - 1

    def add_node(self, node):
        self.gltf['nodes'].append(node)
        return len(self.gltf['nodes']) - 1

    def to_bytes(self):
        while len(self.binary) % 4:
            self.binary.append(0)
        self.gltf['buffers'][0]['byteLength'] = len(self.binary)
        text = json.dumps(self.gltf, separators=(',', ':'), sort_keys=True).encode('utf-8')
        text += b' ' * (-len(text) % 4)
        total = 12 + 8 + len(text) + 8 + len(self.binary)
        return (struct.pack('<III', GLB_MAGIC, GLB_VERSION, total)
                + struct.pack('<II', len(text), JSON_CHUNK) + text
                + struct.pack('<II', len(self.binary), BIN_CHUNK) + bytes(self.binary))


def add_skeleton(builder, character):
    """Add a joint node per bone; return the node of each, and the skin's index."""
    worlds = [(to_y_up(b.position), normalised(quaternion_to_y_up(b.rotation)))
              for b in character.bones]
    nodes = []
    for bone, (position, rotation) in zip(character.bones, worlds):
        if bone.parent < 0:
            local_position, local_rotation = position, rotation
        else:
            parent_position, parent_rotation = worlds[bone.parent]
            inverse = conjugate(parent_rotation)
            offset = [position[i] - parent_position[i] for i in range(3)]
            local_position = rotate(inverse, offset)
            local_rotation = normalised(multiply(inverse, rotation))
        nodes.append(builder.add_node({'name': bone.name,
                                       'translation': list(local_position),
                                       'rotation': list(local_rotation)}))
    for index, bone in enumerate(character.bones):
        if bone.children:
            builder.gltf['nodes'][nodes[index]]['children'] = [nodes[c] for c in bone.children]
    matrices = [inverse_bind_matrix(p, r) for p, r in worlds]
    skin = {'joints': nodes,
            'inverseBindMatrices': builder.add_accessor(matrices, 'MAT4', FLOAT, 'f', None)}
    builder.gltf['skins'] = [skin]
    return [nodes[i] for i, b in enumerate(character.bones) if b.parent < 0]


def vertex_joints(batch, index):
    """Return four joints and four weights for one vertex."""
    palette = batch.palette
    if batch.weights and not palette:
        palette = (0, 1, 2, 3)  # weights with no list: slot k is bone k
    if not palette:
        return (0, 0, 0, 0), (1.0, 0.0, 0.0, 0.0)  # a static part stays with the root
    if not batch.weights:
        return (palette[0], 0, 0, 0), (1.0, 0.0, 0.0, 0.0)
    weights = batch.weights[index]
    total = sum(weights[:len(palette)])
    if total == 0:
        return (0, 0, 0, 0), (1.0, 0.0, 0.0, 0.0)
    joints = [palette[k] if k < len(palette) and weights[k] else 0 for k in range(4)]
    values = [weights[k] / total if k < len(palette) else 0.0 for k in range(4)]
    return tuple(joints), tuple(values)


def unit_normal(normal):
    x, y, z = to_y_up(normal)
    length = math.sqrt(x * x + y * y + z * z)
    return (x / length, y / length, z / length) if length else (0.0, 1.0, 0.0)


def vertex_colors(batch, shader):
    """What VU1 hands the GS per vertex: lit RGB's scale, or an unlit vertex's own RGBA."""
    if shader.lit:
        return [(v / mdl.INTENSITY_ONE,) * 3 + (1.0,) for v in batch.intensities]
    return [tuple(min(c / mdl.COLOR_ONE, 1.0) for c in color) for color in batch.colors]


def add_primitive(builder, mesh, material, skinned, shader=None):
    """Give shader to also write vertex colours, as COLOR_0."""
    positions, uvs, normals, joints, weights, indices = [], [], [], [], [], []
    colors = []
    for batch in mesh.batches:
        base = len(positions)
        if shader:
            colors.extend(vertex_colors(batch, shader))
        for index, position in enumerate(batch.positions):
            positions.append(to_y_up([c * mesh.scale for c in position]))
            uvs.append((batch.uvs[index][0] / mdl.FIXED_ONE,
                        1 - batch.uvs[index][1] / mdl.FIXED_ONE))
            if batch.normals:
                normals.append(unit_normal(batch.normals[index]))
            if skinned:
                joint, weight = vertex_joints(batch, index)
                joints.append(joint)
                weights.append(weight)
        for triangle in mdl.triangles(batch):
            indices.extend(base + i for i in triangle)
    if not indices:
        return None
    attributes = {
        'POSITION': builder.add_accessor(positions, 'VEC3', FLOAT, 'f', ARRAY_BUFFER, True),
        'TEXCOORD_0': builder.add_accessor(uvs, 'VEC2', FLOAT, 'f', ARRAY_BUFFER)}
    if normals:
        attributes['NORMAL'] = builder.add_accessor(normals, 'VEC3', FLOAT, 'f', ARRAY_BUFFER)
    if colors and len(colors) == len(positions):
        attributes['COLOR_0'] = builder.add_accessor(colors, 'VEC4', FLOAT, 'f', ARRAY_BUFFER)
    if skinned:
        attributes['JOINTS_0'] = builder.add_accessor(joints, 'VEC4', UNSIGNED_SHORT, 'H',
                                                      ARRAY_BUFFER)
        attributes['WEIGHTS_0'] = builder.add_accessor(weights, 'VEC4', FLOAT, 'f',
                                                       ARRAY_BUFFER)
    return {'attributes': attributes, 'indices': builder.add_indices(indices),
            'material': material, 'mode': 4}


def channel_value(channel, frame, default):
    if channel is None:
        return default
    return channel.sample(frame) if isinstance(channel, anim.Track) else channel


def bone_channels(animation):
    return [(animation.rotation_track(i), animation.vector_track(i, anim.POSITION))
            for i in range(len(animation.nodes))]


def bone_moves(character, channels, frame):
    """Return each bone's (turn, offset), which map rest-pose points to posed ones.

    The game rotates a bone about its rest position in world axes, adds the position
    channel, then applies the parent's result.
    """
    moves = []
    for bone, (rotation, position) in zip(character.bones, channels):
        turn = channel_value(rotation, frame, anim.IDENTITY)
        shift = channel_value(position, frame, (0.0, 0.0, 0.0))
        rest = bone.position
        spun = rotate(turn, rest)
        offset = [rest[i] + shift[i] - spun[i] for i in range(3)]
        if bone.parent >= 0:
            parent_turn, parent_offset = moves[bone.parent]
            moved = rotate(parent_turn, offset)
            offset = [moved[i] + parent_offset[i] for i in range(3)]
            turn = multiply(parent_turn, turn)
        moves.append((turn, offset))
    return moves


def posed_bones(character, channels, frame):
    """Return each bone's world (position, rotation) at a frame, in the game's axes."""
    posed = []
    for bone, (turn, offset) in zip(character.bones, bone_moves(character, channels, frame)):
        placed = rotate(turn, bone.position)
        posed.append(([placed[i] + offset[i] for i in range(3)],
                      normalised(multiply(turn, bone.rotation))))
    return posed


def local_pose(character, posed):
    """Turn world poses into glTF's local joint transforms, in glTF's axes."""
    worlds = [(to_y_up(p), quaternion_to_y_up(r)) for p, r in posed]
    local = []
    for bone, (position, rotation) in zip(character.bones, worlds):
        if bone.parent < 0:
            local.append((position, rotation))
            continue
        parent_position, parent_rotation = worlds[bone.parent]
        inverse = conjugate(parent_rotation)
        offset = [position[i] - parent_position[i] for i in range(3)]
        local.append((rotate(inverse, offset), normalised(multiply(inverse, rotation))))
    return local


def steady(rotations):
    """Flip quaternions so neighbours stay on the same side, for clean blending."""
    out = [rotations[0]]
    for rotation in rotations[1:]:
        flip = sum(a * b for a, b in zip(out[-1], rotation)) < 0
        out.append(tuple(-c for c in rotation) if flip else rotation)
    return out


def add_animation(builder, animation, character):
    channels = bone_channels(animation)
    frames = list(range(animation.frames))
    poses = [local_pose(character, posed_bones(character, channels, f)) for f in frames]
    times = [(f / FRAMES_PER_SECOND,) for f in frames]
    time_accessor = builder.add_accessor(times, 'SCALAR', FLOAT, 'f', None, True)
    samplers, targets = [], []
    joints = builder.gltf['skins'][0]['joints']
    for index, node in enumerate(joints):
        for path, kind, rows in (('translation', 'VEC3', [p[index][0] for p in poses]),
                                 ('rotation', 'VEC4', steady([p[index][1] for p in poses]))):
            output = builder.add_accessor(rows, kind, FLOAT, 'f', None)
            samplers.append({'input': time_accessor, 'output': output,
                             'interpolation': 'LINEAR'})
            targets.append({'sampler': len(samplers) - 1,
                            'target': {'node': node, 'path': path}})
    builder.gltf.setdefault('animations', []).append(
        {'name': animation.name, 'samplers': samplers, 'channels': targets})


class Library:
    """The archives a model's materials draw from."""

    def __init__(self, disc):
        self.disc = disc
        self.shader_archive, self.shaders = mdl.shader_table(disc)
        self.texture_archive = arc.find_archive(disc, tex.TEXTURE_ARCHIVE)
        self.textures = {e.name_hash: e for e in arc.read_toc(disc, self.texture_archive)}
        self.characters = skel.read_characters(disc)

    def texture_for(self, shader_hash):
        shader = self.shaders[shader_hash]
        data = self.disc.read(self.shader_archive.lba, shader.offset, shader.size)
        target = mdl.shader_texture_hash(data, shader.name, self.textures)
        if target is None:
            return None
        return self.read_texture(target)

    def read_texture(self, texture_hash):
        entry = self.textures[texture_hash]
        raw = self.disc.read(self.texture_archive.lba, entry.offset, entry.size)
        return tex.parse_texture(raw, entry.name)

    def shader(self, shader_hash):
        """Return a shader's passes, or None for a multi-shader."""
        entry = self.shaders[shader_hash]
        data = self.disc.read(self.shader_archive.lba, entry.offset, entry.size)
        return mdl.read_shader(data, entry.name)


def add_texture(builder, texture):
    gltf = builder.gltf
    view = builder.add_view(tex.png_bytes(texture.width, texture.height, tex.rgba(texture)))
    gltf.setdefault('images', []).append({'name': texture.name, 'bufferView': view,
                                          'mimeType': 'image/png'})
    gltf.setdefault('textures', []).append({'source': len(gltf['images']) - 1})
    return len(gltf['textures']) - 1


def add_detail_pass(builder, library, material, shader):
    """Write a shader's second pass, if it fades a texture over the first by a constant.

    glTF has no such layer. The texture rides in occlusionTexture, where a reader that knows
    nothing of it only darkens the surface a little, and extras says how to use it.
    """
    if shader is None or len(shader.passes) < 2:
        return
    layer = shader.passes[1]
    if (layer.alpha != mdl.ALPHA_FIX_MIX or layer.uv_source != mdl.UV_SCALED
            or layer.texture_hash not in library.textures):
        return
    material['occlusionTexture'] = {
        'index': add_texture(builder, library.read_texture(layer.texture_hash))}
    material['extras'].update({'detail_mix': layer.fix / mdl.COLOR_ONE,
                               'detail_scale': list(layer.uv_scale)})


def add_second_pass(builder, library, material, shader):
    """Write an additive second pass, like the sea's water_ocean over its caustics.

    Its texture rides in emissiveTexture with a zero emissiveFactor, so a reader that knows
    nothing of it adds nothing, and extras.gs_blend2 says how the GS mixes it.
    """
    if len(shader.passes) < 2:
        return
    layer = shader.passes[1]
    if (not layer.flags & mdl.PASS_BLEND_FLAG or layer.alpha != mdl.ALPHA_ADD or not layer.fix
            or layer.uv_source != shader.passes[0].uv_source
            or layer.texture_hash not in library.textures):
        return
    material['emissiveTexture'] = {
        'index': add_texture(builder, library.read_texture(layer.texture_hash))}
    material['emissiveFactor'] = [0.0, 0.0, 0.0]
    material['extras']['gs_blend2'] = {'alpha': layer.alpha, 'fix': layer.fix}


def add_material(builder, library, shader_hash, cache, layers=False):
    if shader_hash in cache:
        return cache[shader_hash]
    gltf = builder.gltf
    material = {'name': library.shaders[shader_hash].name, 'doubleSided': True,
                'pbrMetallicRoughness': {'metallicFactor': 0.0, 'roughnessFactor': 1.0}}
    texture = library.texture_for(shader_hash)
    if texture:
        material['pbrMetallicRoughness']['baseColorTexture'] = {
            'index': add_texture(builder, texture)}
        if min(tex.rgba(texture)[3::4], default=255) < OPAQUE_ALPHA:
            material['alphaMode'] = 'MASK'
            material['alphaCutoff'] = ALPHA_CUTOFF
    blending = library.shader(shader_hash)
    if blending and blending.passes and blending.passes[0].flags & mdl.PASS_BLEND_FLAG:
        # How the GS mixes the first pass with the screen: its ALPHA register and FIX, as stored.
        first = blending.passes[0]
        material.setdefault('extras', {})['gs_blend'] = {'alpha': first.alpha, 'fix': first.fix}
        add_second_pass(builder, library, material, blending)
    if layers:
        shader = library.shader(shader_hash)
        # The game draws opaque shaders at once and alpha ones later, so alpha wins an exact tie.
        material.setdefault('extras', {})['draw_rank'] = \
            len(cache) + 1 if shader and shader.is_alpha else 0
        # Lit: COLOR_0 is a brightness that VU1 multiplies by its lights. Unlit: the colour itself.
        material['extras']['lit'] = bool(shader and shader.lit)
        add_detail_pass(builder, library, material, shader)
    gltf.setdefault('materials', []).append(material)
    cache[shader_hash] = len(gltf['materials']) - 1
    return cache[shader_hash]


def usable_rig(name, meshes, characters):
    """Return the model's rig if every bone it names exists there."""
    used = [bone for mesh in meshes for batch in mesh.batches
            for bone in mdl.bones_used(batch)]
    rig = skel.rig_for(name, characters)
    return rig if rig and used and max(used) < len(rig.bones) else None


def build_glb(name, meshes, library, animations=()):
    builder = GlbBuilder()
    rig = usable_rig(name, meshes, library.characters)
    roots = add_skeleton(builder, rig) if rig else []
    cache = {}
    primitives = []
    for mesh in meshes:
        material = add_material(builder, library, mesh.shader_hash, cache)
        primitive = add_primitive(builder, mesh, material, rig is not None)
        if primitive:
            primitives.append(primitive)
    builder.gltf['meshes'].append({'name': name, 'primitives': primitives})
    node = {'name': name, 'mesh': 0}
    if rig:
        node['skin'] = 0
    builder.gltf['scenes'][0]['nodes'] = roots + [builder.add_node(node)]
    for animation in animations:
        add_animation(builder, animation, rig)
    return builder.to_bytes(), rig


def export(disc, out_dir, wanted):
    os.makedirs(out_dir, exist_ok=True)
    library = Library(disc)
    written = rigged = 0
    for entry, data in mdl.each_model(disc, library.shaders):
        if wanted not in entry.name:
            continue
        meshes = mdl.parse_model(data, library.shaders, entry.name)
        if not meshes:
            continue
        glb, rig = build_glb(entry.name, meshes, library)
        with open(os.path.join(out_dir, arc.safe_file_name(entry.name) + '.glb'), 'wb') as out:
            out.write(glb)
        written += 1
        rigged += rig is not None
    print('wrote %d models to %s, %d of them rigged' % (written, out_dir, rigged))
    return 0


def export_animated(disc, out_dir, model_name, wanted):
    os.makedirs(out_dir, exist_ok=True)
    library = Library(disc)
    for entry, data in mdl.each_model(disc, library.shaders):
        if entry.name != model_name:
            continue
        meshes = mdl.parse_model(data, library.shaders, entry.name)
        rig = usable_rig(entry.name, meshes, library.characters)
        if rig is None:
            raise arc.FormatError('%s has no known rig to animate' % model_name)
        animations = [a for a in (anim.parse_animation(raw, e.name)
                                  for e, raw in anim.each_animation(disc) if wanted in e.name)
                      if len(a.nodes) == len(rig.bones)]
        glb, _ = build_glb(entry.name, meshes, library, animations)
        with open(os.path.join(out_dir, arc.safe_file_name(entry.name) + '.glb'), 'wb') as out:
            out.write(glb)
        print('wrote %s with %d animations' % (entry.name, len(animations)))
        return 0
    raise arc.FormatError('%s is not on this disc' % model_name)


def main(argv):
    if len(argv) == 5 and argv[1] == 'export':
        return export(arc.Disc(argv[2]), argv[3], argv[4])
    if len(argv) == 6 and argv[1] == 'animate':
        return export_animated(arc.Disc(argv[2]), argv[3], argv[4], argv[5])
    print(__doc__)
    return 2


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv))
    except (arc.FormatError, OSError) as error:
        print('error: %s' % error, file=sys.stderr)
        print('Check that the path points at an unmodified copy of the game.', file=sys.stderr)
        sys.exit(1)
