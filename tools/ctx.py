import pickle,sys
P=pickle.load(open(sys.argv[1],'rb')); ins=P['ins']
idx={o[0]:i for i,o in enumerate(ins)}
def fa(a):
    t,v=a
    return {'g':'$%d','l':'%d@','i':'%d','f':'%.2f','s':"'%s'"}[t]%v
B=int(sys.argv[2]); A=int(sys.argv[3])
for p in sys.argv[4:]:
    i=idx[int(p)]; print('==',p)
    for j in range(i-B,i+A+1):
        o=ins[j]; print(' ',o[0],('!' if o[3] else '')+o[4],' '.join(fa(a) for a in o[5]))
