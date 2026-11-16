import random, struct, sys
R=random.Random(7); O=sys.argv[1]
def wide(rows, cols, sep=',', dsep='.'):
    L=[sep.join('c%d'%j for j in range(cols))]
    base=[R.uniform(0,100) for _ in range(cols)]
    for i in range(rows):
        f=[]
        for j in range(cols):
            k=j%5
            if k==0: f.append(str(i*cols+j))
            elif k==1: f.append(('%.2f'%(base[j]+R.uniform(-1,1))).replace('.',dsep))
            elif k==2: f.append(str(R.randint(-1000,1000)))
            elif k==3: f.append(R.choice(['x','y','z','']))
            else: f.append(('%.3f'%R.uniform(0,10)).replace('.',dsep) if R.random()<.9 else 'NA')
        L.append(sep.join(f))
    return '\n'.join(L)+'\n'
open(O+'/wide300_1m.csv','w').write(wide(600,300))
open(O+'/wide300_200k.csv','w').write(wide(110,300))
open(O+'/wide400_semi.csv','w').write(wide(300,400,';',','))
open(O+'/wide280_tab.tsv','w').write(wide(400,280,'\t'))
open(O+'/wide260_pipe.txt','w').write(wide(400,260,'|'))
b=bytearray()
for i in range(60000): b+=struct.pack('<Idd', i, round(R.uniform(0,500),2), R.randint(-50,50)*0.125)
open(O+'/recq_1m.bin','wb').write(bytes(b))
b=bytearray()
p=100.0
for i in range(50000): p+=R.uniform(-1,1); b+=struct.pack('<dHH', round(p,2), R.randint(0,9), 7)
open(O+'/recq12_600k.bin','wb').write(bytes(b))
