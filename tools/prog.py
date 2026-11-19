import pickle,sys
P=pickle.load(open(sys.argv[1],'rb')); ins=P['ins']
tot=0
for i,(pos,mi,op,neg,name,args) in enumerate(ins):
    if op==0x030D: print('TOTAL',args,pos)
    if op!=0x030C: continue
    amt=args[0][1]; tot+=amt
    keys=[]; sets=[]; scr=''
    for j in range(max(0,i-25),min(len(ins),i+25)):
        o=ins[j]
        if o[1]!=mi: continue
        if o[2]==0x0318: keys.append(o[5][0][1])
        if o[2]==0x0004 and o[5][0][0]=='g' and o[5][1][1]==1 and abs(j-i)<=10: sets.append('%d%+d'%(o[5][0][1],j-i))
    # script name: last 03A4 before in same mission
    for j in range(i,max(0,i-6000),-1):
        if ins[j][1]!=mi: break
        if ins[j][2]==0x03A4: scr=ins[j][5][0][1]; break
    print(mi,pos,amt,scr,'|',','.join(keys),'|',' '.join(sets))
print('sum',tot)
