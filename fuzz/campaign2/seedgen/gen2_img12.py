import random, struct, sys, os, math
R=random.Random(99); O=sys.argv[1]
def img(wd, ht, big, mx=4095, noise=8):
    b=bytearray(); f='>H' if big else '<H'
    for y in range(ht):
        for x in range(wd):
            v=int(mx/2+mx/3*math.sin(x/37.0)*math.cos(y/23.0)+R.randint(-noise,noise)); v=max(0,min(mx,v))
            b+=struct.pack(f,v)
    return bytes(b)
open(O+'/img12_640x480.pgm','wb').write(b'P5\n640 480\n4095\n'+img(640,480,True))
open(O+'/img12_1024x768.pgm','wb').write(b'P5\n1024 768\n4095\n'+img(1024,768,True))
open(O+'/raw12le_hdr.bin','wb').write(struct.pack('<4sIII',b'RAW2',700,500,12)+img(700,500,False))
open(O+'/raw12be_hdr.bin','wb').write(struct.pack('>4sIII',b'RAW2',700,500,12)+img(700,500,True))
open(O+'/raw10le_noh.bin','wb').write(img(512,600,False,1023,3))
