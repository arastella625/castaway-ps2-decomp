#!/usr/bin/env python3
"""Assemble a whole Sim from your own disc image into one rigged, textured glTF file.

Usage:
  python -I tools/sim.py check <game.iso>
  python -I tools/sim.py build <game.iso> <out.glb> <skin> <animation|-> <part> [<part> ...]

skin is a tone such as caucasian_normal_1. animation is substrings split by commas, or - for
none. Each
part is a model name, with an optional colour and variant: af_lb_pcotton_ln:b:1. The first
part's prefix, af_ or am_, picks the rig.

Two more arguments may go among the parts, each four numbers as a CasSimDescription holds them:
--body=3,1,1,1 for latticeMorphValues and --face=1,1,1,1 for morphTargetValues. Those two are the
defaults. The file is game data: keep it out of the repo.
docs/ASSET_FORMATS.md explains how textures are composited.
"""
import collections
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I leaves this out
import anim  # noqa: E402
import arc  # noqa: E402
import gltf  # noqa: E402
import model as mdl  # noqa: E402
import morph  # noqa: E402
import skeleton as skel  # noqa: E402
import texture as tex  # noqa: E402

MAP_UNITS = 128  # GetLayoutMapRect works in units of a 128-pixel map
MAP_SIZE = 256
DEFAULT_COLOUR = 'n'
DEFAULT_VARIANT = '1'
HAIR_CODES = ('ha1', 'ha2', 'ha3', 'hb')
FACE_CODE = 'ft'
FACE_FEATURES = ('chin', 'eyes', 'lips', 'nose')  # offsets for the face's vertices
DEFAULT_BODY = (3.0, 1.0, 1.0, 1.0)
DEFAULT_FACE = (1.0, 1.0, 1.0, 1.0)
NOT_FULL_TEXTURES = ('_game', '_icon', '_rfl')
HEAD_SHADER_ENDING = '_base_face'  # scalp and hairline surfaces read the head's skin
TONES = ('asian', 'brown', 'caucasian', 'darkbrown', 'pale')
TONE_NAME = '%s_normal_%d'
QUADRANTS = ('ul', 'ur', 'lr')

# Quadrant and rectangle from each SimBodyPart class. Rectangles count rows from the bottom,
# which is how the textures are stored. Parts missing here have no skin beneath them.
WHOLE = (0, 0, 128, 128)
HEAD = ('ul', (0, 0, 64, 64))
LAYOUT = {'ti': ('ur', WHOLE), 'lb': ('lr', WHOLE), 'slti': ('ul', (0, 64, 128, 128)),
          'sh': ('ul', (64, 0, 128, 64)), 'ft': HEAD, 'chin': HEAD, 'eyes': HEAD, 'lips': HEAD,
          'nose': HEAD}


def body_code(model_name):
    return model_name.split('_')[1]


def crop(pixels, size, rect):
    """Cut a rectangle out of a top-down RGBA map; return (width, height, pixels)."""
    scale = size // MAP_UNITS
    left, low, right, high = (value * scale for value in rect)
    stride = size * 4
    rows = [bytes(pixels[y * stride + left * 4:y * stride + right * 4])
            for y in range(size - high, size - low)]
    return right - left, high - low, b''.join(rows)


def resized(texture, width, height):
    """Scale a texture's top-down RGBA to a size, by nearest pixel."""
    source = tex.rgba(texture)
    out = bytearray(width * height * 4)
    for y in range(height):
        row = (y * texture.height // height) * texture.width
        for x in range(width):
            at = (row + x * texture.width // width) * 4
            out[(y * width + x) * 4:(y * width + x) * 4 + 4] = source[at:at + 4]
    return out


def paint(base, layer):
    """Lay one RGBA image over another of the same size, by the layer's alpha."""
    out = bytearray(base)
    for i in range(0, len(out), 4):
        alpha = layer[i + 3]
        if alpha:
            for c in range(3):
                out[i + c] = (layer[i + c] * alpha + out[i + c] * (255 - alpha)) // 255
    return out


class Wardrobe:
    """Finds and composites the textures a Sim wears."""

    def __init__(self, library, prefix, skin):
        self.library = library
        self.prefix = prefix
        self.skin = skin
        self.by_name = {e.name: e for e in library.textures.values()}
        self.images = {}

    def load(self, name):
        entry = self.by_name.get(name)
        if entry is None:
            return None
        data = self.library.disc.read(self.library.texture_archive.lba, entry.offset, entry.size)
        return tex.parse_texture(data, entry.name)

    def clothing(self, model_name, shader_name, colour, variant):
        """Try the model's name, its hair family, then the shader's name."""
        ending = '_%s_%s' % (colour, variant)
        hair = model_name
        for code in HAIR_CODES:
            hair = hair.replace('_%s_' % code, '_hh_')
        bases = (model_name, hair, shader_name)
        for base in bases:
            found = self.load(base + ending)
            if found:
                return found
        return self.any_colour(bases, variant)

    def any_colour(self, bases, variant):
        """Some accessories name their colours in words, so take the first that fits."""
        for base in bases:
            matches = sorted(n for n in self.by_name if n.startswith(base + '_')
                             and n.endswith('_' + variant) and not n.endswith(NOT_FULL_TEXTURES))
            if matches:
                return self.load(matches[0])
        return None

    def skin_crop(self, quadrant, rect):
        name = '%s_%s_%s' % (self.prefix, quadrant, self.skin)
        texture = self.load(name)
        if texture is None:
            raise arc.FormatError('no skin texture named %s' % name)
        width, height, pixels = crop(tex.rgba(texture), texture.width, rect)
        solid = bytearray(pixels)
        solid[3::4] = b'\xff' * (len(solid) // 4)  # skin alpha means nothing; one map has 0
        return width, height, solid

    def image_for(self, model_name, shader_name, colour, variant):
        """Return (key, width, height, pixels) for a surface, or None to keep its shader."""
        code = body_code(model_name)
        clothing = self.clothing(model_name, shader_name, colour, variant)
        if shader_name.endswith(HEAD_SHADER_ENDING):
            code, clothing = FACE_CODE, None
        layout = LAYOUT.get(code)
        key = (code, clothing.name if clothing else None)
        if key in self.images:
            return (key,) + self.images[key]
        if layout:
            width, height, pixels = self.skin_crop(*layout)
            if clothing:
                pixels = paint(pixels, resized(clothing, width, height))
        elif clothing:
            width, height, pixels = clothing.width, clothing.height, tex.rgba(clothing)
        else:
            return None
        self.images[key] = (width, height, pixels)
        return (key, width, height, pixels)


def add_composite_material(builder, key, width, height, pixels, cache):
    if key in cache:
        return cache[key]
    gltf_data = builder.gltf
    view = builder.add_view(tex.png_bytes(width, height, pixels))
    name = '_'.join(str(part) for part in key)
    gltf_data.setdefault('images', []).append({'name': name, 'bufferView': view,
                                               'mimeType': 'image/png'})
    gltf_data.setdefault('textures', []).append({'source': len(gltf_data['images']) - 1})
    material = {'name': name, 'doubleSided': True,
                'pbrMetallicRoughness': {'metallicFactor': 0.0, 'roughnessFactor': 1.0,
                                         'baseColorTexture': {
                                             'index': len(gltf_data['textures']) - 1}}}
    if min(pixels[3::4], default=255) < gltf.OPAQUE_ALPHA:
        material['alphaMode'] = 'MASK'
        material['alphaCutoff'] = gltf.ALPHA_CUTOFF
    gltf_data.setdefault('materials', []).append(material)
    cache[key] = len(gltf_data['materials']) - 1
    return cache[key]


def parse_part(text):
    fields = text.split(':')
    return (fields[0], fields[1] if len(fields) > 1 else DEFAULT_COLOUR,
            fields[2] if len(fields) > 2 else DEFAULT_VARIANT)


def read_models(disc, library, wanted):
    """Return {name: meshes} and {name: file data}."""
    found, raw = {}, {}
    for entry, data in mdl.each_model(disc, library.shaders):
        if entry.name in wanted:
            found[entry.name] = mdl.parse_model(data, library.shaders, entry.name)
            raw[entry.name] = data
    missing = sorted(set(wanted) - set(found))
    if missing:
        raise arc.FormatError('no model named %s' % missing[0])
    return found, raw


def reshape(parts, meshes, raw, body, targets):
    """Apply the body lattices to every part, then the face's targets, as the game does."""
    for name, _, _ in parts:
        lattices = morph.read_lattices(raw[name], name)[0]
        morph.apply_lattices(meshes[name], raw[name], lattices, body_code(name), body, name)
        if body_code(name) == FACE_CODE:
            morph.apply_face(meshes[name], [(meshes[t], w) for t, w in targets])


def add_part(builder, library, wardrobe, part, meshes, caches):
    model_name, colour, variant = part
    primitives = []
    for mesh in meshes:
        shader_name = library.shaders[mesh.shader_hash].name
        image = wardrobe.image_for(model_name, shader_name, colour, variant)
        if image:
            material = add_composite_material(builder, *image, caches['composite'])
        else:
            material = gltf.add_material(builder, library, mesh.shader_hash, caches['shader'])
        primitive = gltf.add_primitive(builder, mesh, material, True)
        if primitive:
            primitives.append(primitive)
    return primitives


def shape_values(texts, flag, default):
    for text in texts:
        if text.startswith(flag):
            values = tuple(float(v) for v in text[len(flag):].split(','))
            if len(values) != len(default):
                raise arc.FormatError('%s needs %d numbers' % (flag, len(default)))
            return values
    return default


def build(disc, out_path, skin, wanted_animation, part_texts):
    body = shape_values(part_texts, '--body=', DEFAULT_BODY)
    face_values = shape_values(part_texts, '--face=', DEFAULT_FACE)
    parts = [parse_part(text) for text in part_texts if not text.startswith('--')]
    prefix = parts[0][0][:2]
    library = gltf.Library(disc)
    rig = library.characters[skel.ADULT_RIGS[prefix + '_'].lower()]
    targets = morph.face_targets_wanted(prefix, face_values)
    meshes, raw = read_models(disc, library, [p[0] for p in parts] + [t for t, _ in targets])
    reshape(parts, meshes, raw, body, targets)
    wardrobe = Wardrobe(library, prefix, skin)
    builder = gltf.GlbBuilder()
    roots = gltf.add_skeleton(builder, rig)
    caches = {'composite': {}, 'shader': {}}
    primitives = []
    for part in parts:
        primitives += add_part(builder, library, wardrobe, part, meshes[part[0]], caches)
    builder.gltf['meshes'].append({'name': 'sim', 'primitives': primitives})
    node = builder.add_node({'name': 'sim', 'mesh': 0, 'skin': 0})
    builder.gltf['scenes'][0]['nodes'] = roots + [node]
    count = 0
    if wanted_animation != '-':
        wanted = wanted_animation.split(',')
        for entry, raw in anim.each_animation(disc):
            if any(text in entry.name for text in wanted):
                animation = anim.parse_animation(raw, entry.name)
                if len(animation.nodes) == len(rig.bones):
                    gltf.add_animation(builder, animation, rig)
                    count += 1
    with open(out_path, 'wb') as out:
        out.write(builder.to_bytes())
    print('wrote %s: %d parts, %d animations' % (out_path, len(parts), count))
    return 0


def check(disc):
    library = gltf.Library(disc)
    totals = collections.Counter()
    failures = []
    wardrobes = {prefix: Wardrobe(library, prefix, TONE_NAME % ('pale', 1))
                 for prefix in ('af', 'am')}
    for prefix, wardrobe in wardrobes.items():
        for tone in TONES:
            for number in (1, 2):
                for quadrant in QUADRANTS:
                    name = '%s_%s_%s' % (prefix, quadrant, TONE_NAME % (tone, number))
                    totals['skin maps'] += 1
                    if name not in wardrobe.by_name:
                        failures.append('no skin map %s' % name)
    models = {e.name: mdl.parse_model(raw, library.shaders, e.name)
              for e, raw in mdl.each_model(disc, library.shaders) if e.name[:3] in ('af_', 'am_')}
    for name, meshes in sorted(models.items()):
        code = body_code(name)
        if code in FACE_FEATURES:
            face = models[name[:3] + 'ft_base_face']
            sizes = [[len(b.positions) for m in part for b in m.batches] for part in (face, meshes)]
            totals['face features'] += 1
            if sizes[0] != sizes[1]:
                failures.append('%s does not fit the face' % name)
            continue
        for mesh in meshes:
            shader_name = library.shaders[mesh.shader_hash].name
            found = wardrobes[name[:2]].clothing(name, shader_name, DEFAULT_COLOUR,
                                                 DEFAULT_VARIANT)
            own = library.texture_for(mesh.shader_hash)
            if found:
                totals['surfaces with clothing'] += 1
            elif code in LAYOUT or shader_name.endswith(HEAD_SHADER_ENDING):
                totals['surfaces of skin alone'] += 1
            elif own and own.name != 'white':
                totals['surfaces with their shader texture'] += 1
            else:
                totals['surfaces left white'] += 1
                totals['left white: ' + code] += 1
    for key, value in sorted(totals.items()):
        print('%-36s %6d' % (key, value))
    print('failures: %d' % len(failures))
    for failure in failures[:20]:
        print('   ', failure)
    return 1 if failures else 0


def main(argv):
    if len(argv) == 3 and argv[1] == 'check':
        return check(arc.Disc(argv[2]))
    if len(argv) >= 7 and argv[1] == 'build':
        return build(arc.Disc(argv[2]), argv[3], argv[4], argv[5], argv[6:])
    print(__doc__)
    return 2


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv))
    except (arc.FormatError, OSError) as error:
        print('error: %s' % error, file=sys.stderr)
        print('Check that the path points at an unmodified copy of the game.', file=sys.stderr)
        sys.exit(1)
