import struct
def load(path):
    d=open(path,'rb').read()
    # VC gxt: TABL size entries(12: name8 off4) ; each table: (name8 for non-MAIN) TKEY size entries(12: off4 key8) TDAT size data
    assert d[:4]==b'TABL'; n=struct.unpack_from('<I',d,4)[0]//12
    out={}
    for i in range(n):
        name=d[8+i*12:16+i*12].split(b'\0')[0]; off=struct.unpack_from('<I',d,16+i*12)[0]
        p=off if name==b'MAIN' else off+8
        assert d[p:p+4]==b'TKEY',(name,p)
        ks=struct.unpack_from('<I',d,p+4)[0]; kp=p+8; tdat=kp+ks; assert d[tdat:tdat+4]==b'TDAT'
        base=tdat+8
        for j in range(ks//12):
            o=struct.unpack_from('<I',d,kp+j*12)[0]; key=d[kp+j*12+4:kp+j*12+12].split(b'\0')[0].decode('latin1')
            q=base+o; s=''
            while True:
                c=struct.unpack_from('<H',d,q)[0]; q+=2
                if c==0: break
                s+=chr(c)
            out.setdefault(key,s)
    return out
