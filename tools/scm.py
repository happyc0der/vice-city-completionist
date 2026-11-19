import json,struct,sys,pickle
REF=sys.argv[1]; SCM=sys.argv[2]; OUT=sys.argv[3]
lib=json.load(open(REF+'/vc.json',encoding='utf8'))
cmds={}
for e in lib['extensions']:
    if e['name']!='default': continue
    for c in e['commands']:
        ps=c.get('input',[])+c.get('output',[])
        cmds[int(c['id'],16)]=(c['name'],[p['type'] for p in ps])
STR={'gxt_key','zone_key','string'}
d=open(SCM,'rb').read()
def rd(pos,t=None):
    b=d[pos]
    if t in STR and b>6:
        return ('s',d[pos:pos+8].split(b'\0')[0].decode('latin1')),pos+8
    if b==1: return ('i',struct.unpack_from('<i',d,pos+1)[0]),pos+5
    if b==2: return ('g',struct.unpack_from('<H',d,pos+1)[0]),pos+3
    if b==3: return ('l',struct.unpack_from('<H',d,pos+1)[0]),pos+3
    if b==4: return ('i',struct.unpack_from('<b',d,pos+1)[0]),pos+2
    if b==5: return ('i',struct.unpack_from('<h',d,pos+1)[0]),pos+3
    if b==6: return ('f',struct.unpack_from('<f',d,pos+1)[0]),pos+5
    if b>6: return ('s',d[pos:pos+8].split(b'\0')[0].decode('latin1')),pos+8
    raise ValueError('type %d at %d'%(b,pos))
# header
def jmp(pos):
    assert d[pos:pos+3]==b'\x02\x00\x01',pos
    return struct.unpack_from('<i',d,pos+3)[0]
s2=jmp(0); s3=jmp(s2); 
nobj=struct.unpack_from('<I',d,s2+8)[0]
codestart=jmp(s3)
p=s3+8
mainsize,largest,nmis,nexcl=struct.unpack_from('<IIHH',d,p); p+=12
moffs=list(struct.unpack_from('<%dI'%nmis,d,p))
print('globals end',s2,'objs',nobj,'main',mainsize,'missions',nmis,'code',codestart)
bounds=[codestart]+moffs+[len(d)]
ins=[]  # (off, mission(-1 main), op, neg, name, args)
errs=0
for bi in range(len(bounds)-1):
    pos=bounds[bi]; end=bounds[bi+1]; mi=bi-1
    while pos<end:
        op=struct.unpack_from('<H',d,pos)[0]; neg=op>>15; op&=0x7fff
        if op not in cmds:
            errs+=1; print('unknown op %04X at %d mission %d'%(op,pos,mi)); break
        name,types=cmds[op]; q=pos+2; args=[]
        try:
            for t in types:
                if t=='arguments':
                    while d[q]!=0:
                        a,q=rd(q); args.append(a)
                    q+=1
                else:
                    a,q=rd(q,t); args.append(a)
        except Exception as ex:
            errs+=1; print('err',ex,'op %04X'%op,'mission',mi); break
        ins.append((pos,mi,op,neg,name,args)); pos=q
print('instructions',len(ins),'errors',errs)
pickle.dump({'ins':ins,'moffs':moffs,'s2':s2},open(OUT,'wb'))
