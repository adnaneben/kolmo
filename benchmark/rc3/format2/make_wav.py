# make_wav.py DATA : DATA/wav/ from the 40 ESC-50 clips in DATA/audio/esc (bench/kaud/fetch_data.sh DATA/audio):
# the clips concatenated (mono), as 20 stereo pairs, and clip 4 with an odd data chunk and a LIST chunk after it.
import wave, sys, glob, struct, array, os
D = sys.argv[1]; os.makedirs(D + '/wav', exist_ok=True); fs = sorted(glob.glob(D + '/audio/esc/*.wav'))
fr = [wave.open(f).readframes(10**9) for f in fs]
w = wave.open(D + '/wav/esc40_mono.wav', 'wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(44100); w.writeframes(b''.join(fr)); w.close()
st = []
for a, b in zip(fr[0::2], fr[1::2]):
    x = array.array('h', a); y = array.array('h', b); n = min(len(x), len(y)); z = array.array('h', [0]) * (2 * n); z[0::2] = x[:n]; z[1::2] = y[:n]; st.append(z.tobytes())
w = wave.open(D + '/wav/esc20_stereo.wav', 'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(44100); w.writeframes(b''.join(st)); w.close()
pcm = fr[3] + b'\x07'
riff = b'WAVE' + b'fmt ' + struct.pack('<IHHIIHH', 16, 1, 1, 44100, 88200, 2, 16) + b'data' + struct.pack('<I', len(pcm)) + pcm + b'\x00' + b'LIST' + struct.pack('<I', 8) + b'INFOtest'
open(D + '/wav/odd_list.wav', 'wb').write(b'RIFF' + struct.pack('<I', len(riff)) + riff)
