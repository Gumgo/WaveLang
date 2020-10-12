import json
import math

frequency_min = 20.0
frequency_max = 20000.0
harmonic_count = round(frequency_max / frequency_min)

def triangle_harmonic(i):
	if i % 2 == 0:
		s = 1.0 if (i // 2) % 2 == 0 else -1.0
		sqrt_denom = math.pi * (i + 1)
		return s * 8.0 / (sqrt_denom * sqrt_denom)
	else:
		return 0.0

def sawtooth_harmonic(i):
	return -2.0 / (math.pi * (i + 1))

waveforms = {}
waveforms["sine"] = [1.0]
waveforms["triangle"] = [triangle_harmonic(i) for i in range(harmonic_count)]
waveforms["sawtooth"] = [sawtooth_harmonic(i) for i in range(harmonic_count)]

with open("common_waveforms.json", "w") as file:
	json.dump(waveforms, file, indent = 4)
