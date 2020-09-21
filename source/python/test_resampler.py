import matplotlib.pyplot as plt
import numpy as np

import polyphase_fir_generator

plot_results = True
plot_sample_rate = 44100

params = polyphase_fir_generator.FilterParams()
params.upsample_factor = 512
params.taps_per_phase = 256
params.cutoff_frequency = 0.5
params.window_type = polyphase_fir_generator.WindowType.KAISER
params.kaiser_beta = 15.0

filter = polyphase_fir_generator.build_polyphase_fir(params, plot_results, plot_sample_rate)
history_length = params.taps_per_phase - 1

sample_count = 2048
frequency = 16
signal = np.sin(np.linspace(0, frequency * 2.0 * np.pi, sample_count, endpoint = False))

# Pad the beginning with zeros to compensate for latency
padded_signal = np.concatenate((np.zeros(history_length - filter.latency), signal, np.zeros(filter.latency)))

def resample(params, filter, samples, sample_index, fractional_sample_index):
	assert fractional_sample_index >= 0.0 and fractional_sample_index < 1.0
	upsampled_fractional_sample_index = fractional_sample_index * params.upsample_factor
	phase_a_index = int(upsampled_fractional_sample_index)
	phase_b_index = phase_a_index + 1
	phase_fraction = upsampled_fractional_sample_index - phase_a_index

	# The last tap lands on the sample at sample_index, so we add 1 to the starting index after subtracting tap count
	assert sample_index < len(samples)
	assert sample_index >= params.taps_per_phase - 1
	start_sample_index = sample_index - (params.taps_per_phase - 1)

	phase_a_coefficients = filter.phases[phase_a_index]
	phase_b_coefficients = filter.phases[phase_b_index]

	phase_a_result = 0.0
	phase_b_result = 0.0

	# Apply the two phase FIRs
	for tap_index in range(params.taps_per_phase):
		phase_a_coefficient = phase_a_coefficients[tap_index]
		phase_b_coefficient = phase_b_coefficients[tap_index]
		sample = samples[start_sample_index + tap_index]

		phase_a_result += phase_a_coefficient * sample
		phase_b_result += phase_b_coefficient * sample

	return phase_a_result + (phase_b_result - phase_a_result) * phase_fraction

upsample_factor = 16
upsampled_signal = np.zeros(sample_count * upsample_factor)
for index in range(sample_count):
	for upsample_index in range(upsample_factor):
		upsampled_signal[index * upsample_factor + upsample_index] = resample(
			params,
			filter,
			padded_signal,
			index + history_length,
			upsample_index / upsample_factor)

fig, axes = plt.subplots(2, 1)

signal_axes, resampled_signal_axes = axes

signal_axes.set_title("Original signal")
signal_axes.plot(signal, linewidth = 1)

resampled_signal_axes.set_title("Resampled signal")
resampled_signal_axes.plot(upsampled_signal, linewidth = 1)

plt.show()
