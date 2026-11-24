import sys, os
from PIL import Image
import numpy as np
src, dst = sys.argv[1], sys.argv[2]
for f in sorted(os.listdir(src)):
    im = Image.open(os.path.join(src, f)); 
    name = os.path.splitext(f)[0]
    if im.mode in ('RGBA',): 
        a = np.asarray(im)
        if (a[...,3]==255).all(): im = im.convert('RGB')
    if im.mode in ('L',):
        a = np.asarray(im); out = os.path.join(dst, name + '.pgm'); hdr = b'P5\n%d %d\n255\n' % (a.shape[1], a.shape[0])
    elif im.mode == 'RGB':
        a = np.asarray(im); out = os.path.join(dst, name + '.ppm'); hdr = b'P6\n%d %d\n255\n' % (a.shape[1], a.shape[0])
    else:
        print('skip', f, im.mode, im.size); 
        a = np.asarray(im.convert('RGB')); out = os.path.join(dst, name + '.ppm'); hdr = b'P6\n%d %d\n255\n' % (a.shape[1], a.shape[0])
    open(out, 'wb').write(hdr + a.tobytes())
    open(os.path.join(dst, name + '.u8'), 'wb').write(a.tobytes())
    print(out, a.shape, im.mode)
