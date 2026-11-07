#!/usr/bin/env python3
"""Stand a placed object as its animation says, for placed.py lot --posed.

Usage: python -I tools/pose.py check <game.iso>

An object's ObjAnimDef names an animation. Its first frame moves bones, and its visibility channel
hides them: one model often holds every variant of a plant, and this picks one. Baking that frame
into the mesh is the simple choice. It needs no skeleton in the port, but a graphic that changes
in play needs another bake. docs/formats/world.md has what's known.
"""
import collections
import copy
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I leaves this out
import anim  # noqa: E402
import arc  # noqa: E402
import gltf  # noqa: E402
import model as mdl  # noqa: E402
import placed  # noqa: E402

FIRST_FRAME = 0
VISIBLE_FROM = 0.5
PARKED = 100.0  # metres: some animations send a part this far away rather than hide it


def hidden_bones(animation):
    """Bones whose visibility channel is a constant 0. No object's animation has a track here."""
    hidden = set()
    for index, node in enumerate(animation.nodes):
        value = node[anim.VISIBILITY]
        if value < 0:
            raise arc.FormatError('%s: a visibility track is not decoded' % animation.name)
        if value and animation.floats[value] < VISIBLE_FROM:
            hidden.add(index)
    return hidden


def blend(moves, joints, weights, vector, turn_only=False):
    """Linear blend skinning of one point or normal."""
    out = [0.0, 0.0, 0.0]
    for joint, weight in zip(joints, weights):
        if weight:
            turn, offset = moves[joint]
            moved = gltf.rotate(turn, vector)
            for axis in range(3):
                out[axis] += weight * (moved[axis] + (0.0 if turn_only else offset[axis]))
    return out


def pose_batch(batch, scale, moves, hidden):
    """Return a copy with positions in metres, and hidden triangles marked as not drawn."""
    posed = copy.copy(batch)
    posed.positions, posed.normals = [], []
    flags = list(batch.flags)
    gone = []
    for index, position in enumerate(batch.positions):
        joints, weights = gltf.vertex_joints(batch, index)
        point = [c * scale for c in position]
        moved = blend(moves, joints, weights, point)
        parked = math.dist(point, moved) > PARKED
        posed.positions.append(tuple(point if parked else moved))  # keeps the mesh's bounds honest
        if batch.normals:
            posed.normals.append(tuple(blend(moves, joints, weights, batch.normals[index], True)))
        gone.append(parked or all(j in hidden for j, w in zip(joints, weights) if w))
    for index in range(2, len(flags)):
        if gone[index] or gone[index - 1] or gone[index - 2]:
            flags[index] |= mdl.NO_DRAW
    posed.flags = flags
    return posed


def pose_meshes(meshes, character, animation):
    """Return copies of a model's meshes at the animation's first frame, in metres."""
    if len(animation.nodes) != len(character.bones):
        raise arc.FormatError('%s does not fit the rig %s' % (animation.name, character.name))
    moves = gltf.bone_moves(character, gltf.bone_channels(animation), FIRST_FRAME)
    hidden = hidden_bones(animation)
    posed = []
    for mesh in meshes:
        copied = copy.copy(mesh)
        copied.batches = [pose_batch(b, mesh.scale, moves, hidden) for b in mesh.batches]
        copied.scale = 1.0
        posed.append(copied)
    return posed


def load_animations(disc):
    return {entry.name: data for entry, data in anim.each_animation(disc)}


def triangle_count(meshes):
    return sum(1 for mesh in meshes for batch in mesh.batches for _ in mdl.triangles(batch))


def shaders_left(meshes, library):
    return {library.shaders[mesh.shader_hash].name for mesh in meshes
            if any(True for batch in mesh.batches for _ in mdl.triangles(batch))}


def check(disc):
    library = gltf.Library(disc)
    table = placed.model_table(disc)
    models = {e.name: raw for e, raw in mdl.each_model(disc, library.shaders)}
    animations = load_animations(disc)
    kinds = collections.Counter()
    for objects in placed.read_houses(disc).values():
        for item in objects:
            name = placed.object_model(item, table, models) if item.on_lot else None
            if name:
                kinds[(name,) + placed.object_pose(item, table)] += 1
    totals = collections.Counter()
    for (name, animation_name, rig), count in kinds.items():
        totals['objects'] += count
        if not animation_name or not rig:
            totals['with no pose'] += count
            continue
        animation = anim.parse_animation(animations[animation_name], animation_name)
        meshes = mdl.parse_model(models[name], library.shaders, name)
        posed = pose_meshes(meshes, library.characters[rig.lower()], animation)
        totals['with parts hidden'] += count * (triangle_count(posed) < triangle_count(meshes))
    for key, value in sorted(totals.items()):
        print('%-22s %5d' % (key, value))

    stalk = mdl.parse_model(models['isStalkVariations'], library.shaders, 'isStalkVariations')
    rig = library.characters['isstalkvariations']

    def left(animation_name):
        animation = anim.parse_animation(animations[animation_name], animation_name)
        return shaders_left(pose_meshes(stalk, rig, animation), library)

    bamboo, sugar = left('o_isStalkVariationsFullBamboo'), left('o_isStalkVariationsFullSugar')
    print('bamboo keeps', sorted(bamboo))
    print('sugarcane keeps', sorted(sugar))
    passed = 'bambooLeaves' in bamboo and 'isSugarCaneLeaves' not in bamboo \
        and 'isSugarCaneLeaves' in sugar and 'bambooLeaves' not in sugar
    print('controls:')
    print('  control: the other plant\'s animation leaves different leaves: %s' % (bamboo != sugar))
    passed &= bamboo != sugar
    wrong = anim.parse_animation(animations['o_isTreeBananaPose'], 'o_isTreeBananaPose')
    try:
        pose_meshes(stalk, rig, wrong)
        print('  CONTROL FAILED: a tree animation fitted the stalk rig')
        passed = False
    except arc.FormatError:
        print('  control: a tree animation on the stalk rig is rejected')
    print('failures: %d' % (not passed))
    return 0 if passed else 1


def main(argv):
    if len(argv) >= 3 and argv[1] == 'check':
        return check(arc.Disc(argv[2]))
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
