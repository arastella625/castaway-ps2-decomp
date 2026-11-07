#!/usr/bin/env python3
"""Decode the game's MODL models from your own disc image into OBJ files.

Usage:
  python -I tools/model.py check  <game.iso>
  python -I tools/model.py export <game.iso> <out_dir> <name_substring>

check decodes every model and reports anything that doesn't fit the format. export writes
an OBJ and MTL for each matching model, with PNGs of its textures. All of that is game
data: keep it out of the repo. docs/ASSET_FORMATS.md has the format.
"""
import bisect
import collections
import os
import struct
import sys
from dataclasses import dataclass, field

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I leaves this out
import arc  # noqa: E402
import gamedata  # noqa: E402
import texture as tex  # noqa: E402

MODEL_ARCHIVE = 'MODELS'
SHADER_ARCHIVE = 'SHADERS'
MODEL_VERSION = 69
MODEL_TAG = b'LDOM'  # "MODL", stored as a little-endian integer
SHADER_TAG = b'RDHS'  # "SHDR"
# Where a shader may name its texture, from the end of its name: layer one, layer two,
# then the two short forms. Every shader that names a texture does so at one of these.
SHADER_TEXTURE_OFFSETS = (0x3F, 0x6F, 0x10, 0x0F)
PREFIX_SIZE = 16
SUBMODEL_HEADER = 12  # a u32, the node count and the mesh count
SUBMODEL_NODE_SIZE = 28

# Every mesh record's type is a mask of what its vertices hold.
MESH_TYPES = (0x16, 0x1A, 0x1E)
HAS_NORMALS = 0x08

# Each batch of vertices opens with these two VIF commands.
BATCH_START = bytes.fromhex('0101000100800160')

VIF_STCYCL = 0x01
VIF_MSCAL = 0x14
UNPACK_S32 = 0x60
UNPACK_V2_16 = 0x65
UNPACK_V4_16 = 0x6D
UNPACK_V4_8 = 0x6E
UNPACK_ITEM_SIZE = {UNPACK_S32: 4, UNPACK_V2_16: 4, UNPACK_V4_16: 8, UNPACK_V4_8: 4}

SLOT_COUNT, SLOT_POSITION, SLOT_NORMAL, SLOT_UV, SLOT_COLOR, SLOT_WEIGHTS = 0, 1, 2, 3, 4, 5
SLOT_COMMANDS = {SLOT_COUNT: UNPACK_S32, SLOT_POSITION: UNPACK_V4_16, SLOT_NORMAL: UNPACK_V4_8,
                 SLOT_UV: UNPACK_V2_16, SLOT_COLOR: UNPACK_V4_8, SLOT_WEIGHTS: UNPACK_V4_8}

NO_DRAW = 0x8000  # in a position's fourth value: this vertex finishes no triangle
TOP_WEIGHT_MASK = 0xFF  # also there: four times the slot of the vertex's largest weight
WEIGHT_TOTAL = 255
FIXED_ONE = 4096  # UVs are stored as 4096ths, and so are most characters' positions
NORMAL_ONE = 127
INTENSITY_ONE = 127  # the normal's fourth byte: VU1 scales a lit vertex's RGB by this over 127
COLOR_ONE = 128  # the GS reads 128 as 1.0 in a vertex colour

GROUP_TAIL_PREFIX = bytes.fromhex('1800000000000000')  # present on all but static groups
PALETTE_ENTRY_FLAG = 1
PALETTE_ENTRY_SIZE = 4

# An SHDR body after its size: docs/formats/world.md has the fields, from ERShader::CopyShedData.
SHADER_LIT_OFFSET = 7  # a word; LIT_FLAG picks normals and intensity over slot 4's colour
SHADER_LIT_FLAG = 0x08
SHADER_PASSES_OFFSET = 0x3B
SHADER_PASS_SIZE = 0x30
PASS_FORMAT = '<IHB2fBBBB'  # texture, flags, a byte, two floats, then ALPHA, FIX, a byte, UV source
PASS_SCALE_OFFSET = 0x20
PASS_BLEND_FLAG = 0x40
SINGLE_SHADER_KIND = 0  # kind 1 is a multi-shader, a list of day and night shaders
ALPHA_FIELD_SCREEN = 1  # in the GS ALPHA register, the colour already on the screen
ALPHA_FIX_MIX = 0x64  # the GS ALPHA register: (source - destination) * FIX + destination
UV_SCALED = 4  # the mesh's own UVs through the pass's scale; 3 is a reflection


@dataclass
class Batch:
    positions: list
    flags: list
    uvs: list
    normals: list
    weights: list
    palette: list = field(default_factory=list)  # bone index for each weight
    intensities: list = field(default_factory=list)  # baked brightness of a lit vertex
    colors: list = field(default_factory=list)  # RGBA of an unlit vertex


@dataclass
class Mesh:
    vertex_mask: int
    shader_hash: int
    batches: list = field(default_factory=list)
    scale: float = 1 / FIXED_ONE  # metres per stored unit


def read_slots(data, pos, name):
    """Walk one batch's VIF commands; return {slot: (count, offset)} and where it ends."""
    slots = {}
    while True:
        if pos + 4 > len(data):
            raise arc.FormatError('%s: a batch runs off the end of the file' % name)
        immediate, count, command = struct.unpack_from('<HBB', data, pos)
        command &= 0x7F
        pos += 4
        if command == VIF_MSCAL:
            return slots, pos
        if command == VIF_STCYCL:
            continue
        slot = immediate & 0x3FF
        if SLOT_COMMANDS.get(slot) != command:
            raise arc.FormatError('%s: unknown VIF command %#x for slot %d'
                                  % (name, command, slot))
        slots[slot] = (count, pos)
        pos += count * UNPACK_ITEM_SIZE[command]


def read_batch(data, pos, name):
    slots, end = read_slots(data, pos, name)
    if end > len(data) or SLOT_COUNT not in slots or SLOT_POSITION not in slots:
        raise arc.FormatError('%s: a batch is incomplete' % name)
    count = struct.unpack_from('<I', data, slots[SLOT_COUNT][1])[0]
    if any(n != count for slot, (n, _) in slots.items() if slot != SLOT_COUNT):
        raise arc.FormatError('%s: a batch disagrees about its vertex count' % name)
    raw = [struct.unpack_from('<4h', data, slots[SLOT_POSITION][1] + i * 8) for i in range(count)]
    uvs = normals = intensities = colors = []
    if SLOT_UV in slots:
        uvs = [struct.unpack_from('<2h', data, slots[SLOT_UV][1] + i * 4) for i in range(count)]
    if SLOT_NORMAL in slots:
        normals = [struct.unpack_from('<3b', data, slots[SLOT_NORMAL][1] + i * 4)
                   for i in range(count)]
        intensities = [data[slots[SLOT_NORMAL][1] + i * 4 + 3] for i in range(count)]
    if SLOT_COLOR in slots:
        offset = slots[SLOT_COLOR][1]
        colors = [tuple(data[offset + i * 4:offset + i * 4 + 4]) for i in range(count)]
    weights = []
    if SLOT_WEIGHTS in slots:
        offset = slots[SLOT_WEIGHTS][1]
        weights = [tuple(data[offset + i * 4:offset + i * 4 + 4]) for i in range(count)]
    flags = [v[3] & 0xFFFF for v in raw]
    check_weights(weights, flags, name)
    return Batch([v[:3] for v in raw], flags, uvs, normals, weights,
                 intensities=intensities, colors=colors), end


def check_weights(weights, flags, name):
    for weight, flag in zip(weights, flags):
        if sum(weight) != WEIGHT_TOTAL:
            raise arc.FormatError('%s: the weights on a vertex do not add up' % name)
        if weight[(flag & TOP_WEIGHT_MASK) // 4 % 4] != max(weight):
            raise arc.FormatError('%s: a vertex does not flag its largest weight' % name)


def read_group_tail(data, end, batch_count, name):
    """Return (unique vertices, total vertices, batches, end of tail) after a group."""
    pos = end + (len(GROUP_TAIL_PREFIX) if data.startswith(GROUP_TAIL_PREFIX, end) else 0)
    pos += 8 * batch_count  # an offset and a size for each batch
    if pos + 12 > len(data):
        raise arc.FormatError('%s: a group has no tail' % name)
    unique, total, batches = struct.unpack_from('<III', data, pos)
    return unique, total, batches, pos + 12


def read_palette(data, low, high, unique, name):
    """Find a group's vertex count, then read the bone list that ends just before it.

    Each entry is a flag of 1, a u16 bone and a u8 slot, and a 0 byte ends the list. The
    count is a small number that can turn up by chance, so a place with a list wins.
    """
    pattern = struct.pack('<I', unique)
    pos = data.find(pattern, low, high)
    while pos >= 0:
        if pos > 0 and data[pos - 1] == 0 and pos + 4 + unique * 8 <= high:
            palette = palette_before(data, pos, low, name)
            if palette:
                return palette
        pos = data.find(pattern, pos + 1, high)
    return []


def palette_before(data, pos, low, name):
    palette = []
    entry = pos - 1 - PALETTE_ENTRY_SIZE
    while entry >= low and data[entry] == PALETTE_ENTRY_FLAG:
        bone, slot = struct.unpack_from('<HB', data, entry + 1)
        palette.insert(0, (slot, bone))
        entry -= PALETTE_ENTRY_SIZE
    if [slot for slot, _ in palette] != list(range(len(palette))):
        raise arc.FormatError('%s: a bone palette is out of order' % name)
    return [bone for _, bone in palette]


def read_groups(data, name, record_offsets=()):
    """Return [(start, batches)]. A group is a run of batches that share a bone palette."""
    groups = []
    low = 0
    pos = data.find(BATCH_START)
    while pos >= 0:
        start = pos
        batches = []
        while data.startswith(BATCH_START, pos):
            batch, pos = read_batch(data, pos, name)
            batches.append(batch)
        unique, total, count, tail_end = read_group_tail(data, pos, len(batches), name)
        if count != len(batches) or total != sum(len(b.positions) for b in batches):
            raise arc.FormatError('%s: a group disagrees with its tail' % name)
        record = bisect.bisect_left(record_offsets, start) - 1
        floor = max(low, record_offsets[record]) if record >= 0 else low
        palette = read_palette(data, floor, start, unique, name)
        for batch in batches:
            batch.palette = palette
        groups.append((start, batches))
        low = tail_end
        pos = data.find(BATCH_START, tail_end)
    return groups


def find_mesh_records(data, shader_hashes):
    """Return (offset, type, shader hash), sorted. A hash ties a record to a real shader."""
    records = []
    for mesh_type in MESH_TYPES:
        pattern = struct.pack('<I', mesh_type)
        pos = data.find(pattern)
        while pos >= 0:
            if pos + 8 <= len(data):
                shader_hash = struct.unpack_from('<I', data, pos + 4)[0]
                if shader_hash in shader_hashes:
                    records.append((pos, mesh_type, shader_hash))
            pos = data.find(pattern, pos + 1)
    return sorted(records)


def parse_model(data, shader_hashes, name='model'):
    """Return a model's meshes. Each batch belongs to the mesh record before it."""
    if len(data) < PREFIX_SIZE:
        raise arc.FormatError('%s is too short for a header' % name)
    version, tag = struct.unpack_from('<I4s', data, 0)
    if version != MODEL_VERSION or tag != MODEL_TAG:
        raise arc.FormatError('%s is not a version 69 MODL file' % name)
    records = find_mesh_records(data, shader_hashes)
    scale = gamedata.read_model_info(data, name).scale
    meshes = [Mesh(mesh_type, shader_hash, scale=scale) for _, mesh_type, shader_hash in records]
    current = -1
    for start, batches in read_groups(data, name, [r[0] for r in records]):
        while current + 1 < len(records) and records[current + 1][0] < start:
            current += 1
        if current < 0:
            raise arc.FormatError('%s: a batch comes before any mesh record' % name)
        for batch in batches:
            if bool(batch.normals) != bool(meshes[current].vertex_mask & HAS_NORMALS):
                raise arc.FormatError('%s: a batch does not match its mesh type' % name)
        meshes[current].batches.extend(batches)
    if any(not mesh.batches for mesh in meshes):
        raise arc.FormatError('%s: a mesh record has no vertices' % name)
    return meshes


def submodels(data, meshes, shader_hashes):
    """Group a model's meshes by submodel. Each is a run of mesh records, in file order.

    ESubModel::Read puts a u32, a counted list of 28-byte quadtree nodes and a u32 mesh count just
    before the run's first record.
    """
    records = find_mesh_records(data, shader_hashes)
    groups = []
    first = 0
    while first < len(records):
        count = submodel_size(data, records[first][0], len(records) - first)
        groups.append(meshes[first:first + count])
        first += count
    return groups


def submodel_size(data, record, most):
    count = struct.unpack_from('<I', data, record - 4)[0]
    nodes = 0
    while record - SUBMODEL_HEADER - nodes * SUBMODEL_NODE_SIZE >= 0:
        at = record - SUBMODEL_HEADER - nodes * SUBMODEL_NODE_SIZE
        if struct.unpack_from('<I', data, at + 4)[0] == nodes:
            if 0 < count <= most:
                return count
            break
        nodes += 1
    raise arc.FormatError('no submodel header before the mesh record at %#x' % record)


def bones_used(batch):
    """Return a batch's bones. With weights but no palette, slot k simply means bone k."""
    if batch.palette or not batch.weights:
        return batch.palette
    return sorted({k for weight in batch.weights for k in range(4) if weight[k]})


def faces_toward(batch, a, b, c):
    """Order a triangle so its face agrees with the stored normals."""
    p, q, r = batch.positions[a], batch.positions[b], batch.positions[c]
    u = [q[i] - p[i] for i in range(3)]
    v = [r[i] - p[i] for i in range(3)]
    face = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
    total = [batch.normals[a][i] + batch.normals[b][i] + batch.normals[c][i] for i in range(3)]
    return sum(face[i] * total[i] for i in range(3)) >= 0


def triangles(batch):
    """Yield index triples from a batch's strip, skipping restarts and collapsed triangles."""
    positions = batch.positions
    for i in range(2, len(positions)):
        if batch.flags[i] & NO_DRAW:
            continue
        a, b, c = i - 2, i - 1, i
        if positions[a] == positions[b] or positions[b] == positions[c] \
                or positions[a] == positions[c]:
            continue
        if batch.normals:
            flipped = not faces_toward(batch, a, b, c)
        else:
            flipped = i % 2 == 1  # strips alternate direction
        yield (a, c, b) if flipped else (a, b, c)


def shader_table(disc):
    archive = arc.find_archive(disc, SHADER_ARCHIVE)
    return archive, {e.name_hash: e for e in arc.read_toc(disc, archive)}


def shader_texture_hash(data, name, texture_hashes):
    """Return the hash of a shader's first texture, or None if it names no texture."""
    if len(data) < PREFIX_SIZE or data[4:8] != SHADER_TAG:
        raise arc.FormatError('%s is not an SHDR file' % name)
    body = PREFIX_SIZE + struct.unpack_from('<I', data, 12)[0]
    for offset in SHADER_TEXTURE_OFFSETS:
        if body + offset + 4 <= len(data):
            candidate = struct.unpack_from('<I', data, body + offset)[0]
            if candidate in texture_hashes:
                return candidate
    return None


@dataclass
class ShaderPass:
    texture_hash: int
    flags: int
    alpha: int  # the GS ALPHA register: four two-bit fields
    fix: int  # ALPHA's constant, where 128 is 1.0
    uv_source: int
    uv_scale: tuple


@dataclass
class Shader:
    lit: bool
    passes: list

    @property
    def is_alpha(self):
        """EShaderRenderPassDef::IsAlphaPass on the first pass: does blending read the screen?"""
        first = self.passes[0]
        fields = (first.alpha & 3, first.alpha >> 2 & 3, first.alpha >> 6 & 3)  # A, B and D
        return bool(first.flags & PASS_BLEND_FLAG) and ALPHA_FIELD_SCREEN in fields


def read_shader(data, name):
    """ERShader::CopyShedData. Return None for a multi-shader, which holds no passes itself."""
    if len(data) < PREFIX_SIZE or data[4:8] != SHADER_TAG:
        raise arc.FormatError('%s is not an SHDR file' % name)
    if struct.unpack_from('<I', data, 8)[0] != SINGLE_SHADER_KIND:
        return None
    body = PREFIX_SIZE + struct.unpack_from('<I', data, 12)[0] + 4
    count = data[body]
    if body + SHADER_PASSES_OFFSET + count * SHADER_PASS_SIZE > len(data):
        raise arc.FormatError('%s: the passes run off the end of the shader' % name)
    passes = []
    for index in range(count):
        pos = body + SHADER_PASSES_OFFSET + index * SHADER_PASS_SIZE
        texture, flags, _, _, _, alpha, fix, _, uv_source = struct.unpack_from(PASS_FORMAT, data, pos)
        scale = struct.unpack_from('<2f', data, pos + PASS_SCALE_OFFSET)
        passes.append(ShaderPass(texture, flags, alpha, fix, uv_source, scale))
    lit = bool(struct.unpack_from('<I', data, body + SHADER_LIT_OFFSET)[0] & SHADER_LIT_FLAG)
    return Shader(lit, passes)


def each_model(disc, shader_hashes):
    archive = arc.find_archive(disc, MODEL_ARCHIVE)
    for entry in sorted(arc.read_toc(disc, archive), key=lambda e: e.offset):
        yield entry, disc.read(archive.lba, entry.offset, entry.size)


def check(disc):
    shader_archive, shaders = shader_table(disc)
    textures = {e.name_hash for e in
                arc.read_toc(disc, arc.find_archive(disc, tex.TEXTURE_ARCHIVE))}
    linked = sum(shader_texture_hash(disc.read(shader_archive.lba, e.offset, e.size), e.name,
                                     textures) is not None for e in shaders.values())
    parsed = {hash_: read_shader(disc.read(shader_archive.lba, e.offset, e.size), e.name)
              for hash_, e in shaders.items()}
    totals = collections.Counter()
    failures = []
    for entry, data in each_model(disc, shaders):
        try:
            meshes = parse_model(data, shaders, entry.name)
        except arc.FormatError as error:
            failures.append(str(error))
            continue
        totals['models'] += 1
        totals['empty models'] += not meshes
        totals['meshes'] += len(meshes)
        for mesh in meshes:
            totals['batches'] += len(mesh.batches)
            totals['triangles'] += sum(len(list(triangles(b))) for b in mesh.batches)
            totals['weighted vertices'] += sum(len(b.weights) for b in mesh.batches)
            count_lighting(totals, mesh, parsed[mesh.shader_hash])
    for key, value in sorted(totals.items()):
        print('%-14s %8d' % (key, value))
    print('shaders with a texture: %d of %d' % (linked, len(shaders)))
    if totals['unlit, no slot 4'] or totals['intensity over 127']:
        failures.append('a mesh breaks the rule VU1 lights by')
    print('failures: %d' % len(failures))
    for failure in failures[:20]:
        print('   ', failure)
    return 1 if failures else 0


def count_lighting(totals, mesh, shader):
    """VU1 reads normals when a shader is lit and slot 4 when not: the mesh must have that one."""
    if shader is None:
        totals['on a multi-shader'] += 1
        return
    normals = bool(mesh.vertex_mask & HAS_NORMALS)
    color = any(batch.colors for batch in mesh.batches)
    totals['lit, normals' if shader.lit and normals else
           'lit, no normals' if shader.lit else
           'unlit, slot 4' if color else 'unlit, no slot 4'] += 1
    totals['intensity over 127'] += sum(value > INTENSITY_ONE for batch in mesh.batches
                                        for value in batch.intensities)


def obj_lines(mesh_list, materials):
    """Build OBJ text; indices run across the whole file and start at 1."""
    lines = []
    base = 1
    for index, mesh in enumerate(mesh_list):
        lines.append('usemtl %s' % materials[index])
        for batch in mesh.batches:
            for x, y, z in batch.positions:
                lines.append('v %.6f %.6f %.6f' % (x * mesh.scale, y * mesh.scale, z * mesh.scale))
            for u, v in batch.uvs:
                lines.append('vt %.6f %.6f' % (u / FIXED_ONE, v / FIXED_ONE))
            for x, y, z in batch.normals:
                lines.append('vn %.4f %.4f %.4f'
                             % (x / NORMAL_ONE, y / NORMAL_ONE, z / NORMAL_ONE))
            for tri in triangles(batch):
                ids = [base + i for i in tri]
                if batch.normals:
                    lines.append('f ' + ' '.join('%d/%d/%d' % (i, i, i) for i in ids))
                else:
                    lines.append('f ' + ' '.join('%d/%d' % (i, i) for i in ids))
            base += len(batch.positions)
    return lines


def write_texture(disc, archive, entry, out_dir):
    file_name = arc.safe_file_name(entry.name) + '.png'
    path = os.path.join(out_dir, file_name)
    if not os.path.exists(path):
        parsed = tex.parse_texture(disc.read(archive.lba, entry.offset, entry.size), entry.name)
        with open(path, 'wb') as out:
            out.write(tex.png_bytes(parsed.width, parsed.height, tex.rgba(parsed)))
    return file_name


def export_model(disc, entry, meshes, out_dir, shader_info, texture_info):
    shader_archive, shaders = shader_info
    texture_archive, textures = texture_info
    base = arc.safe_file_name(entry.name)
    materials = []
    mtl = []
    for mesh in meshes:
        shader = shaders[mesh.shader_hash]
        material = arc.safe_file_name(shader.name)
        materials.append(material)
        if 'newmtl %s' % material in mtl:
            continue
        mtl.append('newmtl %s' % material)
        data = disc.read(shader_archive.lba, shader.offset, shader.size)
        target = textures.get(shader_texture_hash(data, shader.name, textures))
        if target:
            mtl.append('map_Kd %s' % write_texture(disc, texture_archive, target, out_dir))
    with open(os.path.join(out_dir, base + '.mtl'), 'w', newline='\n') as out:
        out.write('\n'.join(mtl) + '\n')
    with open(os.path.join(out_dir, base + '.obj'), 'w', newline='\n') as out:
        out.write('\n'.join(['mtllib %s.mtl' % base] + obj_lines(meshes, materials)) + '\n')


def export(disc, out_dir, wanted):
    os.makedirs(out_dir, exist_ok=True)
    shader_info = shader_table(disc)
    texture_archive = arc.find_archive(disc, tex.TEXTURE_ARCHIVE)
    texture_info = (texture_archive,
                    {e.name_hash: e for e in arc.read_toc(disc, texture_archive)})
    written = 0
    for entry, data in each_model(disc, shader_info[1]):
        if wanted not in entry.name:
            continue
        meshes = parse_model(data, shader_info[1], entry.name)
        if meshes:
            export_model(disc, entry, meshes, out_dir, shader_info, texture_info)
            written += 1
    print('wrote %d models to %s' % (written, out_dir))
    return 0


def main(argv):
    if len(argv) == 3 and argv[1] == 'check':
        return check(arc.Disc(argv[2]))
    if len(argv) == 5 and argv[1] == 'export':
        return export(arc.Disc(argv[2]), argv[3], argv[4])
    print(__doc__)
    return 2


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv))
    except (arc.FormatError, OSError) as error:
        print('error: %s' % error, file=sys.stderr)
        print('Check that the path points at an unmodified copy of the game.', file=sys.stderr)
        sys.exit(1)
