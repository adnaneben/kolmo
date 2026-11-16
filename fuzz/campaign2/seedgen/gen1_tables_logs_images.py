import random, struct, json, os, sys
R=random.Random(1234); O=sys.argv[1]
def w(name, data):
    open(os.path.join(O,name),'wb').write(bytes(data) if isinstance(data,(bytes,bytearray)) else data.encode())
# 1. CSV / TSV with integer and decimal columns
def csv(rows, sep=',', dec=2, hdr=True, quoted=False):
    out=[]
    if hdr: out.append(sep.join(['id','ts','qty','price','temp','code','name']))
    t=1700000000; p=100.0; tmp=20.0
    for i in range(rows):
        t+=R.randint(1,60); p+=R.uniform(-1,1); tmp+=R.uniform(-.3,.3)
        name=R.choice(['alpha','beta','gamma','delta','eps'])
        if quoted: name='"'+name+'"'
        out.append(sep.join([str(i), str(t), str(R.randint(0,500)), '%.*f'%(dec,p), '%.1f'%tmp, 'C%04d'%R.randint(0,9999), name]))
    return '\n'.join(out)+'\n'
w('tbl_4m.csv', csv(70000)); w('tbl_1m.csv', csv(18000)); w('tbl_300k.csv', csv(5000))
w('tbl_4m.tsv', csv(70000, '\t', 3)); w('tbl_q_2m.csv', csv(35000, quoted=True))
# numeric-only CSV (decimal graph)
def numcsv(rows, cols, dec):
    out=[]; v=[R.uniform(0,1000) for _ in range(cols)]
    for i in range(rows):
        for c in range(cols): v[c]+=R.uniform(-2,2)
        out.append(','.join('%.*f'%(dec,x) for x in v))
    return '\n'.join(out)+'\n'
w('num_dec_3m.csv', numcsv(60000,6,2)); w('num_dec_600k.csv', numcsv(12000,6,3))
w('num_int_2m.csv', '\n'.join(','.join(str(R.randint(0,100000)) for _ in range(5)) for _ in range(50000))+'\n')
# one decimal column per line
x=50.0; L=[]
for i in range(200000): x+=R.uniform(-.5,.5); L.append('%.2f'%x)
w('dec_col_1m.txt','\n'.join(L)+'\n')
L=[str(R.randint(-30000,30000)) for _ in range(200000)]; w('int_col_1m.txt','\n'.join(L)+'\n')
# 2. fixed-width record text
L=[]
for i in range(60000):
    L.append('%08d %-12s %10.2f %6d %s' % (i, R.choice(['NORTH','SOUTH','EAST','WEST']), R.uniform(0,99999), R.randint(0,999999), R.choice('ABCD')*3))
w('fixed_3m.txt','\n'.join(L)+'\n'); w('fixed_500k.txt','\n'.join(L[:10000])+'\n')
# 3. 16-bit images
def pgm16(wd, ht, big=True):
    b=bytearray(); 
    for y in range(ht):
        for xx in range(wd):
            v=int(30000+12000*((xx/wd)-(y/ht))+R.randint(-200,200))&0xffff
            b+=struct.pack('>H' if big else '<H', v)
    return b
w('img16_512.pgm', b'P5\n512 512\n65535\n'+pgm16(512,512)); w('img16_256.pgm', b'P5\n256 256\n65535\n'+pgm16(256,256))
w('img16_1024x700.pgm', b'P5\n1024 700\n65535\n'+pgm16(1024,700))
w('raw16le_hdr.bin', struct.pack('<4sIII', b'IMG1', 640, 480, 16)+pgm16(640,480,False))
w('raw16be_hdr.bin', struct.pack('>4sIII', b'IMG1', 640, 480, 16)+pgm16(640,480,True))
w('raw16le_noh.bin', pgm16(800,600,False))
# 16-bit sensor arrays
b=bytearray(); v=0
for i in range(600000): v+=R.randint(-50,50); b+=struct.pack('<h', max(-32768,min(32767,v)))
w('s16le_1m.bin', bytes(b))
# 4. fixed-length binary records
b=bytearray(struct.pack('<8sI', b'RECFILE1', 48))
for i in range(40000):
    b+=struct.pack('<IHhfd8sQ', i, R.randint(0,1000), R.randint(-100,100), R.uniform(0,1), 1000+i*0.25, R.choice([b'alpha\0\0\0',b'beta\0\0\0\0']), 1600000000000+i*1000+R.randint(0,999))[:48].ljust(48,b'\0')
w('rec48_2m.bin', bytes(b))
b=bytearray()
for i in range(80000): b+=struct.pack('<IHBBi', i, R.randint(0,60000), R.randint(0,3), 0xAA, R.randint(-5000,5000))
w('rec12_1m.bin', bytes(b))
# 5. JSON with numbers
L=[json.dumps({'id':i,'ts':1700000000+i*7,'lat':round(48+R.uniform(-1,1),6),'lon':round(2+R.uniform(-1,1),6),'v':round(R.uniform(0,100),2),'ok':R.random()<.5,'tag':R.choice(['a','b','c'])}) for i in range(40000)]
w('ndjson_3m.json','\n'.join(L)+'\n')
w('jsonarr_2m.json', json.dumps([{'x':R.randint(0,10**6),'y':round(R.uniform(-1e3,1e3),3),'z':[R.randint(0,99) for _ in range(4)]} for i in range(30000)], indent=1))
# 6. logs
ips=['10.0.%d.%d'%(R.randint(0,255),R.randint(0,255)) for _ in range(300)]
L=[]
for i in range(30000):
    L.append('%s - - [16/Nov/2026:%02d:%02d:%02d +0000] "GET /api/v1/item/%d HTTP/1.1" %d %d "-" "curl/8.%d"'%(R.choice(ips),(i//3600)%24,(i//60)%60,i%60,R.randint(1,99999),R.choice([200,200,200,404,500]),R.randint(100,99999),R.randint(0,9)))
w('access_3m.log','\n'.join(L)+'\n'); w('access_600k.log','\n'.join(L[:6000])+'\n')
L=[]
for i in range(40000):
    L.append('2026-11-16T%02d:%02d:%02d.%03dZ %s [%s] worker-%d: request id=%08x took %dms status=%d'%((i//3600)%24,(i//60)%60,i%60,R.randint(0,999),R.choice(['INFO','INFO','WARN','ERROR','DEBUG']),R.choice(['http','db','cache']),R.randint(1,16),R.getrandbits(32),R.randint(1,5000),R.choice([0,1,2])))
w('app_3m.log','\n'.join(L)+'\n')
L=[]
for i in range(40000):
    L.append('Nov 16 %02d:%02d:%02d host%d sshd[%d]: Accepted publickey for user%d from 192.168.%d.%d port %d ssh2'%((i//3600)%24,(i//60)%60,i%60,R.randint(1,4),R.randint(1000,65000),R.randint(1,50),R.randint(0,255),R.randint(0,255),R.randint(1024,65535)))
w('syslog_3m.log','\n'.join(L)+'\n')
# lines: repetitive line classes
L=[]
for i in range(80000):
    k=R.randint(0,3)
    L.append(['BEGIN TRANSACTION;','INSERT INTO t VALUES(%d,%d);'%(i,R.randint(0,9999)),'-- checkpoint %d'%(i//100),'COMMIT;'][k])
w('lines_2m.sql','\n'.join(L)+'\n')
