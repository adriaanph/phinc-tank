# Some basic debugging on recorded wav files. Created for spy-ear project.
# Record wav files as follows:
# bash$  wget 192.168.1.44 -o aud.wav
from scipy.io import wavfile
from scipy.signal import medfilt
import numpy as np
import sounddevice as sd
import pylab as plt


# AnalogAudio(channels=1) & ConverterAutoCenter(channels=2), using 3.3V
fn = './aud_2.wav' # Three whistles
# Now sample rate is *4
fn = './aud_1.wav' # Three whistles
# Now sample rate is *8
fn = './aud.wav' # Spoken words

samplerate, data = wavfile.read(fn)
print(samplerate, np.shape(data))

data_ = 10*(data-np.median(data))
data_ = medfilt(data_, 7)
sd.play(data_, samplerate)


axes = plt.subplots(2, 1)[1]
axes[0].plot(data)
axes[1].plot(data_)
plt.show()