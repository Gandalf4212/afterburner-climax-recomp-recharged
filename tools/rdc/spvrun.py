#!/usr/bin/env python3
"""Minimal SPIR-V vertex shader interpreter over spirv-dis text, for one NativeX draw from a RenderDoc capture.
Usage: spvrun.py shader.txt heap.bin mirror.bin sys_off float_off bool_off fetch_off index_addr count [first]"""
import re, struct, sys, math
import numpy as np

F = np.float32

def parse(path):
    insts = []
    for line in open(path):
        line = line.split(';')[0].strip()
        if not line: continue
        m = re.match(r'(%\S+)\s*=\s*(Op\w+)\s*(.*)', line)
        if m: insts.append((m.group(1), m.group(2), m.group(3).split()))
        else:
            p = line.split()
            insts.append((None, p[0], p[1:]))
    return insts

class VM:
    def __init__(self, insts, bufs):
        self.types, self.consts, self.member_off, self.stride, self.builtin = {}, {}, {}, {}, {}
        self.bufs = bufs  # var name -> (bytes, base offset)
        self.names = {}
        self.funcvars = {}
        self.body = []
        in_main = False
        for res, op, a in insts:
            if op == 'OpName': self.names[a[0]] = a[1].strip('"')
            elif op == 'OpMemberDecorate' and a[2] == 'Offset': self.member_off[(a[0], int(a[1]))] = int(a[3])
            elif op == 'OpDecorate' and len(a) > 2 and a[1] == 'ArrayStride': self.stride[a[0]] = int(a[2])
            elif op == 'OpDecorate' and a[1] == 'BuiltIn': self.builtin[a[0]] = a[2]
            elif op == 'OpTypeFloat': self.types[res] = ('f',)
            elif op == 'OpTypeInt': self.types[res] = ('i', a[1] == '1')
            elif op == 'OpTypeBool': self.types[res] = ('b',)
            elif op == 'OpTypeVector': self.types[res] = ('v', a[0], int(a[1]))
            elif op == 'OpTypeArray': self.types[res] = ('a', a[0], a[1])
            elif op == 'OpTypeRuntimeArray': self.types[res] = ('r', a[0])
            elif op == 'OpTypeStruct': self.types[res] = ('s', a)
            elif op == 'OpTypePointer': self.types[res] = ('p', a[0], a[1])
            elif op == 'OpConstant':
                t = self.types[a[0]]
                v = a[1]
                if t[0] == 'f': self.consts[res] = F(float.fromhex(v) if 'x' in v else float(v))
                else: self.consts[res] = int(v, 0) & 0xFFFFFFFF
            elif op == 'OpConstantTrue': self.consts[res] = True
            elif op == 'OpConstantFalse': self.consts[res] = False
            elif op == 'OpConstantComposite': self.consts[res] = [self.consts[x] for x in a[1:]]
            elif op == 'OpConstantNull': self.consts[res] = self.zero(a[0])
            elif op == 'OpFunction': in_main = True
            elif in_main: self.body.append((res, op, a))
        self.vals = dict(self.consts)

    def zero(self, t):
        d = self.types[t]
        if d[0] == 'f': return F(0)
        if d[0] in ('i',): return 0
        if d[0] == 'b': return False
        if d[0] == 'v': return [self.zero(d[1]) for _ in range(d[2])]
        if d[0] == 'a': return [self.zero(d[1]) for _ in range(self.consts[d[2]])]
        if d[0] == 's': return [self.zero(m) for m in d[1]]
        raise Exception(d)

    def elem_type(self, t, idx):
        d = self.types[t]
        if d[0] == 'v': return d[1]
        if d[0] in ('a', 'r'): return d[1]
        if d[0] == 's': return d[1][idx]
        raise Exception(d)

    # Memory (buffer) loads: compute byte offset of a path
    def mem_load(self, var, t, path):
        data, base = self.bufs[var]
        off = base
        cur = self.types[self.types[t][2]][0] if False else None
        # walk from the variable's pointee type
        vt = self.var_type[var]
        for idx in path:
            d = self.types[vt]
            if d[0] == 's':
                off += self.member_off[(vt, idx)]; vt = d[1][idx]
            elif d[0] in ('a', 'r'):
                off += idx * self.stride[vt]; vt = d[1]
            elif d[0] == 'v':
                off += idx * 4; vt = d[1]
        return self.read(data, off, vt)

    def read(self, data, off, t):
        d = self.types[t]
        if d[0] == 'f': return F(struct.unpack_from('<f', data, off)[0])
        if d[0] == 'i': return struct.unpack_from('<I', data, off)[0]
        if d[0] == 'v': return [self.read(data, off + 4 * i, d[1]) for i in range(d[2])]
        if d[0] == 'a':
            return [self.read(data, off + i * self.stride[t], d[1]) for i in range(self.consts[d[2]])]
        if d[0] == 's':
            return [self.read(data, off + self.member_off[(t, i)], m) for i, m in enumerate(d[1])]
        raise Exception(d)

    def run(self, vertex_index, var_decls):
        self.var_type = var_decls
        self.vals = dict(self.consts)
        local = {}   # function/output variables: name -> value tree
        for v, (cls, t) in self.storage.items():
            if cls in ('Function', 'Output', 'Private'): local[v] = self.zero(t)
        labels = {}
        for i, (res, op, a) in enumerate(self.body):
            if op == 'OpLabel': labels[res] = i
        pc = 0; prev = None; cur = None; steps = 0
        def get(x): return self.vals[x]
        while True:
            steps += 1
            if steps > 2000000: raise Exception('too many steps')
            res, op, a = self.body[pc]; pc += 1
            if op == 'OpLabel': prev, cur = cur, res; continue
            if op == 'OpVariable':
                continue
            if op in ('OpSelectionMerge', 'OpLoopMerge'): continue
            if op == 'OpBranch': pc = labels[a[0]]; continue
            if op == 'OpBranchConditional': pc = labels[a[1] if get(a[0]) else a[2]]; continue
            if op == 'OpReturn' or op == 'OpFunctionEnd': break
            if op == 'OpPhi':
                t = a[0]; pairs = a[1:]
                for k in range(0, len(pairs), 2):
                    if pairs[k + 1] == prev: self.vals[res] = self.vals[pairs[k]]; break
                else: raise Exception('phi ' + res)
                continue
            if op == 'OpAccessChain':
                base = a[1]; path = [get(x) for x in a[2:]]
                if base in self.vals and isinstance(self.vals[base], tuple):
                    v0, p0 = self.vals[base]; self.vals[res] = (v0, p0 + path)
                else: self.vals[res] = (base, path)
                continue
            if op == 'OpLoad':
                v, path = self.vals[a[1]] if a[1] in self.vals and isinstance(self.vals[a[1]], tuple) else (a[1], [])
                if v == '%gl_VertexIndex' or self.builtin.get(v) == 'VertexIndex': self.vals[res] = vertex_index; continue
                if v in self.bufs: self.vals[res] = self.mem_load(v, a[0], path); continue
                node = local[v]
                for p in path: node = node[p]
                self.vals[res] = node if not isinstance(node, list) else [x for x in node]
                continue
            if op == 'OpStore':
                v, path = self.vals[a[0]] if a[0] in self.vals and isinstance(self.vals[a[0]], tuple) else (a[0], [])
                val = get(a[1])
                if not path: local[v] = val; continue
                node = local[v]
                for p in path[:-1]: node = node[p]
                node[path[-1]] = val
                continue
            t = a[0]; args = a[1:]
            r = self.alu(op, t, args)
            self.vals[res] = r
        return local

    def alu(self, op, t, args):
        g = lambda x: self.vals[x]
        tt = self.types.get(t)
        def comp(fn, *xs):
            vs = [g(x) for x in xs]
            if isinstance(vs[0], list): return [fn(*e) for e in zip(*vs)]
            return fn(*vs)
        u = lambda x: x & 0xFFFFFFFF
        s32 = lambda x: x - (1 << 32) if x & 0x80000000 else x
        if op == 'OpFMul': return comp(lambda x, y: F(x * y), *args)
        if op == 'OpFAdd': return comp(lambda x, y: F(x + y), *args)
        if op == 'OpFSub': return comp(lambda x, y: F(x - y), *args)
        if op == 'OpFDiv': return comp(lambda x, y: F(x / y) if y != 0 else F(np.inf if x > 0 else (-np.inf if x < 0 else np.nan)), *args)
        if op == 'OpFNegate': return comp(lambda x: F(-x), *args)
        if op == 'OpVectorTimesScalar': return [F(e * g(args[1])) for e in g(args[0])]
        if op == 'OpIAdd': return comp(lambda x, y: u(x + y), *args)
        if op == 'OpIMul': return comp(lambda x, y: u(s32(x) * s32(y)), *args)
        if op == 'OpBitwiseAnd': return comp(lambda x, y: x & y, *args)
        if op == 'OpBitwiseOr': return comp(lambda x, y: x | y, *args)
        if op == 'OpShiftLeftLogical': return comp(lambda x, y: u(x << y), *args)
        if op == 'OpShiftRightLogical': return comp(lambda x, y: x >> y, *args)
        if op == 'OpBitFieldInsert':
            def bfi(b, ins, off, cnt):
                m = ((1 << cnt) - 1) << off
                return (b & ~m & 0xFFFFFFFF) | ((ins << off) & m)
            return comp(lambda b, i: bfi(b, i, g(args[2]), g(args[3])), args[0], args[1])
        if op == 'OpIEqual': return comp(lambda x, y: x == y, *args)
        if op == 'OpINotEqual': return comp(lambda x, y: x != y, *args)
        if op == 'OpFOrdEqual': return comp(lambda x, y: bool(x == y), *args)
        if op == 'OpFOrdGreaterThan': return comp(lambda x, y: bool(x > y), *args)
        if op == 'OpLogicalOr': return comp(lambda x, y: x or y, *args)
        if op == 'OpSelect':
            c = g(args[0]); a1 = g(args[1]); a2 = g(args[2])
            if isinstance(c, list): return [x if cc else y for cc, x, y in zip(c, a1, a2)]
            return a1 if c else a2
        if op == 'OpBitcast':
            v = g(args[0]); tt0 = self.types[t]
            def bc(x):
                base = tt0 if tt0[0] != 'v' else self.types[tt0[1]]
                if base[0] == 'f': return F(struct.unpack('<f', struct.pack('<I', x & 0xFFFFFFFF))[0])
                if isinstance(x, (np.floating, float)): return struct.unpack('<I', struct.pack('<f', float(x)))[0]
                return x & 0xFFFFFFFF
            return [bc(e) for e in v] if isinstance(v, list) else bc(v)
        if op == 'OpConvertFToS': return comp(lambda x: u(int(math.trunc(float(x)))) if np.isfinite(x) else 0x80000000, *args)
        if op == 'OpConvertUToF': return comp(lambda x: F(x), *args)
        if op == 'OpCompositeExtract':
            v = g(args[0])
            for i in args[1:]: v = v[int(i)]
            return v
        if op == 'OpCompositeInsert':
            obj = g(args[0]); comp_ = [x for x in g(args[1])]
            idx = [int(i) for i in args[2:]]
            node = comp_
            for i in idx[:-1]: node[i] = list(node[i]); node = node[i]
            node[idx[-1]] = obj
            return comp_
        if op == 'OpCompositeConstruct':
            out = []
            for x in args:
                v = g(x); out.extend(v if isinstance(v, list) and self.types[t][0] == 'v' else [v])
            return out
        if op == 'OpVectorShuffle':
            v1 = g(args[0]); v2 = g(args[1]); cat = list(v1) + list(v2)
            return [cat[int(i)] for i in args[2:]]
        if op == 'OpExtInst':
            fn = args[1]; xs = args[2:]
            ops = {
                'FAbs': lambda x: F(abs(x)), 'Floor': lambda x: F(math.floor(float(x))) if np.isfinite(x) else x,
                'Trunc': lambda x: F(math.trunc(float(x))) if np.isfinite(x) else x,
                'NMin': lambda x, y: F(min(x, y)) if not (np.isnan(x) or np.isnan(y)) else (y if np.isnan(x) else x),
                'NMax': lambda x, y: F(max(x, y)) if not (np.isnan(x) or np.isnan(y)) else (y if np.isnan(x) else x),
                'NClamp': lambda x, lo, hi: F(min(max(x, lo), hi)) if not np.isnan(x) else lo,
                'UClamp': lambda x, lo, hi: min(max(x, lo), hi),
                'Exp2': lambda x: F(2.0 ** float(x)) if float(x) < 128 else F(np.inf),
                'Log2': lambda x: F(math.log2(x)) if x > 0 else (F(-np.inf) if x == 0 else F(np.nan)),
                'InverseSqrt': lambda x: F(1 / math.sqrt(x)) if x > 0 else (F(np.inf) if x == 0 else F(np.nan)),
            }
            return comp(ops[fn], *xs)
        raise Exception('unhandled ' + op)


def main():
    shader, heap_path, mirror_path = sys.argv[1:4]
    sys_off, float_off, bool_off, fetch_off, index_addr, count = (int(x, 0) for x in sys.argv[4:10])
    first = int(sys.argv[10], 0) if len(sys.argv) > 10 else 0
    insts = parse(shader)
    heap = open(heap_path, 'rb').read()
    mirror = open(mirror_path, 'rb').read()
    vm = VM(insts, {})
    # global variables and their pointee types / storage classes
    storage, var_type = {}, {}
    for res, op, a in insts:
        if op == 'OpVariable':
            ptr = vm.types[a[0]]
            storage[res] = (a[1], ptr[2]); var_type[res] = ptr[2]
    vm.storage = storage
    bufs = {}
    for v in storage:
        n = vm.names.get(v, '')
        if n == 'xe_uniform_system_constants': bufs[v] = (heap, sys_off)
        elif n == 'xe_uniform_float_constants': bufs[v] = (heap, float_off)
        elif n == 'xe_uniform_bool_loop_constants': bufs[v] = (heap, bool_off)
        elif n == 'xe_uniform_fetch_constants': bufs[v] = (heap, fetch_off)
        elif n == 'xe_shared_memory': bufs[v] = (mirror, 0)
    vm.bufs = bufs
    out_pos = [v for v in storage if storage[v][0] == 'Output' and vm.names.get(v, '') in ('', '_')]
    seen = set()
    for i in range(first, first + count):
        hi = struct.unpack_from('<H', mirror, index_addr + 2 * i)[0]
        if hi in seen: continue
        seen.add(hi)
        local = vm.run(hi, var_type)
        pos = None
        for v in storage:
            if storage[v][0] == 'Output' and vm.types[storage[v][1]][0] == 's':
                pos = local[v][0]
        r0 = None
        for v in storage:
            if vm.names.get(v) == 'xe_var_registers': pass
        w = float(pos[3])
        ndc = [float(pos[k]) / w if w else float('nan') for k in range(3)]
        print(f"host index {hi:#06x}: gl_Position {[round(float(x), 3) for x in pos]}  ndc {[round(x, 4) for x in ndc]}")


if __name__ == '__main__':
    main()

def reference(mirror, heap, float_off, fetch_off, guest_index):
    """The guest's math for VS 4CB480: pos(stride 8 at vf95) x c24-26 (a0=0), then c80-c83 as the microcode does."""
    fc = struct.unpack_from('<2I', heap, fetch_off + 4 * (31 * 6 + 2 * 2))
    base = (fc[0] & ~3)
    p = [struct.unpack('>f', mirror[base + 32 * guest_index + 4 * k: base + 32 * guest_index + 4 * k + 4])[0] for k in range(3)] + [1.0]
    c = lambda n: struct.unpack_from('<4f', heap, float_off + 16 * n)
    x = sum(a * b for a, b in zip(c(24), p)); y = sum(a * b for a, b in zip(c(25), p)); z = sum(a * b for a, b in zip(c(26), p))
    c80, c81, c82, c83 = c(80), c(81), c(82), c(83)
    r = [z * c82[i] + c83[i] for i in range(4)]
    s = [c81[0], c81[2], c81[3], c81[1]]; r = [y * s[i] + [r[0], r[2], r[3], r[1]][i] for i in range(4)]
    r = [x * c80[i] + [r[0], r[3], r[1], r[2]][i] for i in range(4)]
    return p, r
