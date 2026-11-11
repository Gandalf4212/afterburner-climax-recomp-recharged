import sys, struct, math, re, zipfile
import numpy as np
exec(open('/tmp/rdc/spvrun.py').read().replace("if __name__ == '__main__':\n    main()",""))
class Killed(Exception): pass
# BC3 texture from the capture (mip 0 of image 19866, 2048x2048)
z=zipfile.ZipFile('/tmp/rdc/nx.zip'); tex=z.read('000104'); W=H=2048
def bc3_texel(u,v):
    x=min(W-1,max(0,int(u*W))); y=min(H-1,max(0,int(v*H)))
    bi=(y//4)*(W//4)+(x//4); p=tex[16*bi:16*bi+16]; px=(y%4)*4+(x%4)
    a0,a1=p[0],p[1]; bits=int.from_bytes(p[2:8],'little'); ai=(bits>>(3*px))&7
    if a0>a1: al=[a0,a1]+[((7-i)*a0+i*a1)/7 for i in range(1,7)]
    else: al=[a0,a1]+[((5-i)*a0+i*a1)/5 for i in range(1,5)]+[0,255]
    c0,c1=struct.unpack_from('<HH',p,8); ci=(struct.unpack_from('<I',p,12)[0]>>(2*px))&3
    rgb=lambda c:((c>>11&31)*255/31,(c>>5&63)*255/63,(c&31)*255/31)
    C0,C1=rgb(c0),rgb(c1)
    cols=[C0,C1,tuple((2*a+b)/3 for a,b in zip(C0,C1)),tuple((a+2*b)/3 for a,b in zip(C0,C1))]
    r,g,b=cols[ci]; return [F(r/255),F(g/255),F(b/255),F(al[ai]/255)]
orig_alu=VM.alu
def alu(self,op,t,args):
    g=lambda x:self.vals[x]
    if op=='OpSampledImage': return ('img',)
    if op=='OpImageSampleExplicitLod':
        c=g(args[1]); return bc3_texel(float(c[0]),float(c[1]))
    if op in('OpDPdxCoarse','OpDPdyCoarse'): v=g(args[0]); return [F(0)]*len(v) if isinstance(v,list) else F(0)
    if op=='OpBitFieldUExtract': b=g(args[0]); o=g(args[1]); c=g(args[2]); return (b>>o)&((1<<c)-1)
    if op=='OpBitFieldSExtract':
        b=g(args[0])&0xFFFFFFFF; o=g(args[1]); c=g(args[2]); v=(b>>o)&((1<<c)-1)
        if v&(1<<(c-1)): v-=1<<c
        return v&0xFFFFFFFF
    if op=='OpConvertSToF': v=g(args[0]); return F(v-(1<<32) if v&0x80000000 else v)
    if op=='OpLogicalAnd': return g(args[0]) and g(args[1])
    if op=='OpLogicalNot': return not g(args[0])
    if op=='OpAny': return any(g(args[0]))
    if op=='OpFOrdLessThan':
        a,b=g(args[0]),g(args[1]); return [bool(x<y) for x,y in zip(a,b)] if isinstance(a,list) else bool(a<b)
    if op=='OpFOrdGreaterThanEqual':
        a,b=g(args[0]),g(args[1]); return [bool(x>=y) for x,y in zip(a,b)] if isinstance(a,list) else bool(a>=b)
    if op=='OpFOrdGreaterThan':
        a,b=g(args[0]),g(args[1]); return [bool(x>y) for x,y in zip(a,b)] if isinstance(a,list) else bool(a>b)
    if op=='OpFUnordNotEqual': a,b=g(args[0]),g(args[1]); return bool(not(a==b))
    if op=='OpExtInst' and args[1]=='Ldexp': return F(float(g(args[2]))*2.0**int(g(args[3]) if g(args[3])<2**31 else g(args[3])-2**32))
    return orig_alu(self,op,t,args)
VM.alu=alu
