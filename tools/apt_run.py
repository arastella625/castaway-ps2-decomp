#!/usr/bin/env python3
"""Run an Apt movie's own ActionScript on stand-in clips: here, how the interaction menu arranges itself.

Usage:
  python -I tools/apt_run.py menu  <game.iso> <out.json>
  python -I tools/apt_run.py check <game.iso>

menu runs tsc5_interaction_menu's scripts as ActionMenu drives them: init, imuShow_local(x, y,
count), updateItemPane, then imuRefresh_local(count, selected, offset) for scrolling. It does so for
each corner (ul, ur, ll, lr), for direct control (dc), for 0 to 12 items and every scroll offset.
It writes where the scripts put each piece: the corner piece and its place, each item bar, the side
panel, the arrows and the pointer. The clips are stand-ins built from apt.py's layout trees, with
_x, _y, _width and _height taken from the shapes' bounds, as Flash measures them; that is the one
assumption (likely). The output is game data: keep it out of the repo.

check runs the cases and tests what the scripts must do: bars 25 apart, the corner picked from the
anchor, up to 6 bars. Its control reads opcode 0xB1 as a plain call, which leaves the corner unset.
"""
import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I leaves this out
import apt  # noqa: E402
import arc  # noqa: E402

MENU = apt.MENU
ROOT_ACTION_FRAME = 0
CORNERS = {'ul': (100, 100), 'ur': (600, 100), 'll': (100, 400), 'lr': (600, 400)}
MAX_ITEMS = 12
PANE_PARTS = ('sidepanel_top_mc', 'sidepanel_mid_mc', 'sidepanel_btm_mc')
PIECE_PARTS = ('arrow_up_mc', 'arrow_dn_mc', 'ptr_mc', 'item_pane_mc', 'titlebar_mc', 'hc_mc')


class Clip:
    """A stand-in MovieClip: properties, named children, the library piece it came from."""

    def __init__(self, name, props=None, children=None, piece=''):
        self.name, self.props, self.children, self.piece = name, dict(props or {}), dict(children or {}), piece
        self.props.setdefault('_x', 0.0)
        self.props.setdefault('_y', 0.0)
        self.frame = None

    def get(self, key):
        return self.children[key] if key in self.children else self.props.get(key)


class Function:
    def __init__(self, name, params, flags, start, end):
        self.name, self.params, self.flags, self.start, self.end = name, params, flags, start, end


def number(v):
    if isinstance(v, bool):
        return 1.0 if v else 0.0
    if isinstance(v, (int, float)):
        return float(v)
    if isinstance(v, str):
        try:
            return float(v)
        except ValueError:
            return math.nan
    return math.nan


def text(v):
    if isinstance(v, bool):
        return 'true' if v else 'false'
    if isinstance(v, float):
        return 'NaN' if math.isnan(v) else str(int(v)) if v == int(v) else repr(v)
    return '' if v is None else str(v)


def truth(v):
    if isinstance(v, bool):
        return v
    if isinstance(v, (Clip, Function)):
        return True
    if isinstance(v, str):
        return v != ''
    n = number(v)
    return not math.isnan(n) and n != 0


class VM:
    """Enough of AVM1 and EA's opcodes for the menu. Unknown opcodes raise, so nothing is skipped."""

    def __init__(self, code, const, game_variables, library, fused_call=True):
        self.code, self.values = code, apt.const_values(const)
        self.game_variables, self.library, self.fused_call = game_variables, library, fused_call
        self.vars, self.log = {}, []
        self.root = Clip('_root')
        self.instructions = {}

    def load(self, entry):
        listing = list(apt.instructions(self.code, entry))
        self.instructions.update({p: (op, o) for p, op, o in listing})
        order = [p for p, _, _ in listing]
        self.next = dict(zip(order, order[1:] + [None]))
        op, operands = listing[0][1], listing[0][2]
        if op != 0x88:
            raise arc.FormatError('%s: the root action has no constant pool' % MENU)
        self.pool = [self.literal(apt.u32(self.code, operands[1] + 4 * i)) for i in range(operands[0])]
        self.run(entry, order[-1] + 1, {}, self.root)

    def literal(self, index, registers=None):
        value = self.values[index]
        if value.startswith("'"):
            return value[1:-1].encode('latin-1').decode('unicode_escape') if '\\' in value else value[1:-1]
        if value.startswith('r'):
            return (registers or {}).get(int(value[1:]))
        raise arc.FormatError('%s: a constant of %s' % (MENU, value))

    def call(self, function, args, this):
        registers = {1: this} if function.flags & 0x01 else {}  # DefineFunction2: preload this
        for (register, _), value in zip(function.params, args + [None] * len(function.params)):
            if register:
                registers[register] = value
        return self.run(function.start, function.end, registers, this)

    def run(self, pos, end, registers, this):
        stack = []
        pop, push = stack.pop, stack.append
        while pos is not None and pos < end:
            op, o = self.instructions[pos]
            following = self.next[pos]
            if op == 0x00:
                return None
            if op == 0x88:
                pass
            elif op == 0x96:
                stack.extend(self.literal(apt.u32(self.code, o[1] + 4 * i), registers) for i in range(o[0]))
            elif op == 0xA2:
                push(self.pool[o[0]])
            elif op == 0xA1:
                push(o[0])
            elif op in (0xB4, 0xB5, 0xB6, 0xB7):
                push(float(o[0]))
            elif op == 0x59:
                push(0.0)
            elif op == 0x5A:
                push(1.0)
            elif op == 0x73:
                push(True)
            elif op == 0x74:
                push(False)
            elif op in (0x75, 0x76):
                push(None)
            elif op == 0x70:
                push(this)
            elif op == 0xAE:
                push(self.variable(self.pool[o[0]], this))
            elif op == 0xAF:
                push(self.member(pop(), self.pool[o[0]]))
            elif op == 0x1C:
                push(self.variable(text(pop()), this))
            elif op in (0x1D, 0x3C):
                value = pop()
                self.vars[text(pop())] = value
            elif op == 0x72:
                self.vars[text(pop())] = 0.0
            elif op == 0x41:
                self.vars.setdefault(text(pop()), None)
            elif op == 0x4E:
                key = pop()
                push(self.member(pop(), text(key)))
            elif op == 0x4F:
                value, key = pop(), pop()
                self.set_member(pop(), text(key), value)
            elif op == 0x87:
                registers[o[0]] = stack[-1]
            elif op == 0xB9:
                push(registers.get(o[0]))
            elif op == 0x17:
                pop()
            elif op == 0x4C:
                push(stack[-1])
            elif op == 0x4D:
                stack[-1], stack[-2] = stack[-2], stack[-1]
            elif op in (0x0A, 0x47):
                b, a = pop(), pop()
                push(text(a) + text(b) if op == 0x47 and (isinstance(a, str) or isinstance(b, str))
                     else number(a) + number(b))
            elif op in (0x0B, 0x0C, 0x0D, 0x3F, 0x0F, 0x48, 0x67):
                b, a = number(pop()), number(pop())
                push({0x0B: lambda: a - b, 0x0C: lambda: a * b,
                      0x0D: lambda: a / b if b else math.nan, 0x3F: lambda: math.fmod(a, b) if b else math.nan,
                      0x0F: lambda: a < b, 0x48: lambda: a < b, 0x67: lambda: a > b}[op]())
            elif op in (0x0E, 0x49, 0x66):
                b, a = pop(), pop()
                if isinstance(a, str) and isinstance(b, str):
                    push(a == b)
                elif op == 0x66 and type(a) is not type(b):
                    push(False)
                elif isinstance(a, (Clip, Function, type(None))) or isinstance(b, (Clip, Function, type(None))):
                    push(a is b)
                else:
                    push(number(a) == number(b))
            elif op == 0x13:
                b = pop()
                push(text(pop()) == text(b))
            elif op == 0x12:
                push(not truth(pop()))
            elif op in (0x10, 0x11):
                b, a = truth(pop()), truth(pop())
                push(a and b if op == 0x10 else a or b)
            elif op == 0x4A:
                push(number(pop()))
            elif op == 0x4B:
                push(text(pop()))
            elif op == 0x18:
                n = number(pop())
                push(n if math.isnan(n) else float(int(n)))
            elif op in (0x50, 0x51):
                push(number(pop()) + (1 if op == 0x50 else -1))
            elif op == 0x99:
                following += o[0]
            elif op == 0x9D:
                if truth(pop()):
                    following += o[0]
            elif op == 0xB8:
                if not truth(pop()):
                    following += o[0]
            elif op == 0x3E:
                return pop() if stack else None
            elif op == 0x40:
                name, count = text(pop()), int(number(pop()))
                for _ in range(count):
                    pop()
                push(Clip('new ' + name))
            elif op in (0x3D, 0xB0, 0xB1):
                name = self.pool[o[0]] if op != 0x3D else text(pop())
                count = int(number(pop()))
                result = self.call_name(name, [pop() for _ in range(count)], this)
                if op == 0xB1 and self.fused_call:  # EA: call, then store into the name beneath the arguments
                    self.vars[text(pop())] = result
                elif op in (0x3D, 0xB1):
                    push(result)
            elif op in (0x52, 0xB2, 0xB3):
                name = self.pool[o[0]] if op != 0x52 else text(pop())
                obj = pop()
                count = int(number(pop()))
                result = self.call_method(obj, name, [pop() for _ in range(count)], this)
                if op == 0xB3 and self.fused_call:
                    self.vars[text(pop())] = result
                elif op in (0x52, 0xB3):
                    push(result)
            elif op in (0x8E, 0x9B):
                size = o[4] if op == 0x8E else o[3]
                if op == 0x8E:
                    params = [(apt.u32(self.code, o[3] + 8 * i), apt.cstring(self.code, apt.u32(self.code, o[3] + 8 * i + 4)))
                              for i in range(o[1])]
                    function = Function(o[0], params, o[2] >> 16, following, following + size)
                else:
                    function = Function(o[0], [(0, '')] * o[1], 0, following, following + size)
                if o[0]:
                    self.vars[o[0]] = function
                else:
                    push(function)
                following += size
            else:
                raise arc.FormatError('%s: the stand-in VM has no %s (at %#x)' % (MENU, apt.OPCODES[op], pos))
            pos = following
        return None

    def variable(self, name, this):
        if name in self.vars:
            return self.vars[name]
        if name == '_root':
            return self.root
        if name == 'Math':
            return Clip('Math')
        if isinstance(this, Clip):
            return this.get(name)
        return None

    def member(self, obj, key):
        if obj is self.root and key in self.vars:  # timeline variables are the root clip's members
            return self.vars[key]
        if isinstance(obj, Clip):
            if key == 'textWidth':
                return 7.0 * len(text(obj.props.get('text', '')))  # a stand-in: no font here
            return obj.get(key)
        if isinstance(obj, str) and key == 'length':
            return float(len(obj))
        return None

    def set_member(self, obj, key, value):
        if isinstance(obj, Clip):
            obj.props[key] = value
            self.log.append(('set', obj.name, key, value))

    def call_name(self, name, args, this):
        function = self.vars.get(name)
        if isinstance(function, Function):
            return self.call(function, args, this)
        if name == 'getVariable':
            return self.game_variables.get(text(args[0]))
        if name in ('attachMovie', 'gotoAndStop', 'gotoAndPlay', 'removeMovieClip') and isinstance(this, Clip):
            return self.call_method(this, name, args, this)
        return None  # playSound, logger, setVariable, reportReady: nothing to lay out

    def call_method(self, obj, name, args, this):
        if isinstance(obj, Clip) and obj.name == 'Math':
            values = [number(v) for v in args]
            if name == 'round':
                return values[0] if math.isnan(values[0]) else float(math.floor(values[0] + 0.5))
            if name in ('max', 'min'):
                return float((max if name == 'max' else min)(values))
            return float({'floor': math.floor, 'ceil': math.ceil, 'abs': math.fabs}[name](values[0]))
        if isinstance(obj, str) and name == 'substr':
            start = int(number(args[0]))
            return obj[start:start + int(number(args[1]))] if len(args) > 1 else obj[start:]
        if not isinstance(obj, Clip):
            return None
        if name == 'attachMovie':
            clip = self.library(text(args[0]), text(args[1]))
            clip.props['_depth'] = number(args[2])
            if len(args) > 3 and isinstance(args[3], Clip):
                clip.props.update({k: v for k, v in args[3].props.items() if k not in ('_x', '_y')})
            obj.children[text(args[1])] = clip
            return clip
        if name in ('gotoAndStop', 'gotoAndPlay'):
            obj.frame = text(args[0])
            return None
        if name == 'getVariable':
            return self.game_variables.get(text(args[0]))
        function = obj.get(name)
        if isinstance(function, Function):
            return self.call(function, args, obj)
        return None


def bounds(node, matrix):
    xs, ys = [], []
    if node['kind'] in ('shape', 'text'):
        left, top, right, bottom = node['bounds']
        for x, y in ((left, top), (right, top), (right, bottom), (left, bottom)):
            px, py = apt.apply(matrix, x, y)
            xs.append(px)
            ys.append(py)
    for child in node.get('children', []):
        cx, cy = bounds(child, apt.combine(matrix, child['matrix']))
        xs += cx
        ys += cy
    return xs, ys


def stand_in(node, name, matrix=(1, 0, 0, 1, 0, 0), piece=''):
    """A Clip from a layout tree: _x and _y from its matrix, _width and _height from its bounds."""
    xs, ys = bounds(node, matrix)
    props = {'_x': matrix[4], '_y': matrix[5]}
    if xs:
        props.update(_width=max(xs) - min(xs), _height=max(ys) - min(ys))
    children = {child['name']: stand_in(child, child['name'], child['matrix'])
                for child in node.get('children', []) if child.get('name')}
    return Clip(name, props, children, piece)


class Menu:
    def __init__(self, disc, fused_call=True):
        _, sheets, parts, movie = apt.read_movie(disc, apt.movie_entry(disc, MENU))
        chars, shapes = apt.read_layout(MENU, parts, movie, sheets)
        info = apt.movie_info(parts[MENU + '.apt'], movie, MENU)
        self.pieces = {}
        for export_name, character in info['exports']:
            tree = apt.node(chars, shapes, character)
            shown = tree['labels'].get('refresh', 0) - 1  # the end of the show animation, as layout draws it
            self.pieces[export_name] = apt.node(chars, shapes, character, shown) if shown > 0 else tree
        self.code, self.const, self.movie = parts[MENU + '.apt'], parts[MENU + '.const'], movie
        self.entry = apt.script_entries(self.code, movie)[ROOT_ACTION_FRAME][1]
        self.fused_call = fused_call

    def run(self, x, y, count, direct=False, selected=0, offset=0):
        game = {'IMU_DirectControl': '1' if direct else '0', 'IMU_IsWideScreen': '0', '_platform': 'PS2',
                'IMU_draw_one': '0', 'IMU_exists': '1'}
        vm = VM(self.code, self.const, game, lambda piece, name: stand_in(self.pieces[piece], name, piece=piece),
                self.fused_call)
        vm.load(self.entry)
        vm.call(vm.vars['init'], [], vm.root)
        vm.call(vm.vars['imuShow_local'], [text(float(x)), text(float(y)), text(float(count))], vm.root)
        im = vm.root.children.get('im')
        if im is None:
            raise arc.FormatError('%s: imuShow attached no piece' % MENU)
        vm.call(vm.vars['updateItemPane'], [im], vm.root)
        if selected or offset:
            vm.call(vm.vars['imuRefresh_local'], [text(float(count)), text(float(selected)), text(float(offset))],
                    vm.root)
        return vm, im


def placed(clip, keys=('_x', '_y', '_height', '_alpha', '_visible')):
    out = {k: clip.props[k] for k in keys if k in clip.props and not
           (isinstance(clip.props[k], float) and math.isnan(clip.props[k]))}
    if clip.frame is not None:
        out['frame'] = clip.frame
    return out


def arrangement(vm, im):
    pane = im.children['item_pane_mc']
    bars = sorted((k for k in pane.children if k.startswith('itembar_')), key=lambda k: int(k.split('_')[1]))
    out = {'piece': im.piece, 'orientation': vm.vars.get('menuOrientation'), 'x': im.props['_x'],
           'y': im.props['_y'], 'pane': {k: placed(pane.children[k]) for k in PANE_PARTS if k in pane.children},
           'parts': {k: placed(im.children[k]) for k in PIECE_PARTS if k in im.children},
           'bars': [dict(placed(pane.children[k]), piece=pane.children[k].piece) for k in bars]}
    bar_text = [e[3] for e in vm.log if e[0] == 'set' and e[2] == 'text' and str(e[3])[11:13].isdigit()
                and str(e[3]).startswith('getloc:IMU_')]
    out['bar_items'] = [int(t[len('getloc:IMU_'):]) for t in bar_text][-len(bars):] if bars else []
    return out


def cases(menu):
    out = []
    for direct, corners in ((False, CORNERS), (True, {'dc': (320, 254)})):
        for corner, (x, y) in corners.items():
            for count in range(MAX_ITEMS + 1):
                offsets = range(max(1, count - 5)) if count > 6 else [0]
                for offset in offsets:
                    vm, im = menu.run(x, y, count, direct, 0, offset)
                    out.append(dict(arrangement(vm, im), corner=corner, count=count, offset=offset, anchor=[x, y],
                                    direct=direct))
    return out


def write(disc, out_path):
    menu = Menu(disc)
    vm, _ = menu.run(*CORNERS['ul'], 0)
    constants = {k: vm.vars[k] for k in ('itembar_height', 'itembar_width', 'itempane_max_width',
                                          'itempane_max_height', 'x_cutoff_max', 'y_cutoff_max', 'max_selections',
                                          'x_direct', 'y_direct')}
    with open(out_path, 'w', encoding='utf-8') as out:
        json.dump({'movie': MENU, 'constants': constants, 'cases': cases(menu)}, out, indent=1)
    print('wrote %s' % out_path)
    return 0


def check(disc):
    passed = True
    menu = Menu(disc)
    for corner, (x, y) in CORNERS.items():
        for count in (3, 12):
            vm, im = menu.run(x, y, count)
            got = arrangement(vm, im)
            ys = [b['_y'] for b in got['bars']]
            ok = (got['orientation'] == corner and im.piece == 'lib_imu_' + corner and len(ys) == min(count, 6)
                  and all(abs(abs(a - b) - 25) < 1e-6 for a, b in zip(ys, ys[1:])))
            print('%s, %2d items: %s at (%g, %g), %d bars at y %s: %s' % (
                corner, count, im.piece, got['x'], got['y'], len(ys), ', '.join('%g' % v for v in ys),
                'ok' if ok else 'FAILED'))
            passed &= ok
    print('controls:')
    try:
        Menu(disc, fused_call=False).run(*CORNERS['ul'], 3)
        print('  CONTROL FAILED: reading 0xB1 as a plain call still attached a corner piece')
        passed = False
    except (arc.FormatError, KeyError) as error:
        print('  control: 0xB1 read as a plain call leaves the corner unset -> %s' % error)
    print('failures: %d' % (not passed))
    return 0 if passed else 1


def main(argv):
    if len(argv) == 3 and argv[1] == 'check':
        return check(arc.Disc(argv[2]))
    if len(argv) == 4 and argv[1] == 'menu':
        return write(arc.Disc(argv[2]), argv[3])
    print(__doc__)
    return 2


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv))
    except (arc.FormatError, OSError) as error:
        print('error: %s' % error, file=sys.stderr)
        sys.exit(1)
