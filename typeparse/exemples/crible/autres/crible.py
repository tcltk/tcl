import time, sys
N=1000000
def best(f,*a):
    b=1e18;r=None
    for _ in range(5):
        t=time.perf_counter(); r=f(*a); b=min(b,time.perf_counter()-t)
    return b*1000,r
# boucle (liste)
def c1(): return [1]*N
def k1(s):
    s=s[:]; s[0]=s[1]=0
    i=2
    while i*i<N:
        if s[i]:
            for j in range(i*i,N,i): s[j]=0
        i+=1
    return s
def n1(s):
    c=0
    for v in s: c+=v
    return c
# tranches (bytearray)
def c2(): return bytearray(b'\x01')*N
def k2(s):
    s=bytearray(s); s[0]=s[1]=0
    i=2
    while i*i<N:
        if s[i]: s[i*i::i]=bytes(len(range(i*i,N,i)))
        i+=1
    return s
def n2(s): return sum(s)
try:
    import numpy as np
except ImportError:
    np = None
def c3(): return np.ones(N,dtype=np.uint8)
def k3(s):
    s=s.copy(); s[:2]=0
    i=2
    while i*i<N:
        if s[i]: s[i*i::i]=0
        i+=1
    return s
def n3(s): return int(s.sum(dtype=np.int64))
variantes=[("Python liste + boucle",c1,k1,n1),("Python bytearray + tranches",c2,k2,n2)]
if np is not None: variantes.append(("Python NumPy + tranches",c3,k3,n3))
for nom,c,k,n in variantes:
    tc,s=best(c); tk,s=best(k,s); tn,r=best(n,s)
    print(f"{nom:32s} {tc:7.1f} {tk:7.1f} {tn:7.1f} {tc+tk+tn:7.1f} -> {r}")
