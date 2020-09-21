from enum import Enum
import math

import matplotlib.pyplot as plt
import numpy as np
import scipy.signal
import scipy.signal.windows

# Some references:
# http://www.astro.rug.nl/~vdhulst/SignalProcessing/Hoorcolleges/college10.pdf
# https://www.analog.com/media/en/technical-documentation/dsp-book/dsp_book_Ch16.pdf
# http://yehar.com/blog/wp-content/uploads/2009/08/deip.pdf
# https://ccrma.stanford.edu/~jos/pasp/Windowed_Sinc_Interpolation.html
# https://ccrma.stanford.edu/~jos/resample/

class WindowType(Enum):
	RECTANGULAR = 0
	HANN = 1
	HAMMING = 2
	BLACKMAN = 3
	KAISER = 4

class FilterParams:
	def __init__(self):
		self.upsample_factor = 0
		self.taps_per_phase = 0
		self.cutoff_frequency = 0.0
		self.window_type = WindowType.RECTANGULAR
		self.kaiser_beta = 0.0
		self.notch_frequencies = [] # Two notches at 0.5 is an experiment that isn't working so well

class PolyphaseFir:
	def __init__(self):
		self.phases = []
		self.latency = 0

def build_polyphase_fir(params, plot = False, plot_sample_rate = 1.0):
	# Upsample factor should be a power of 2
	assert math.log2(params.upsample_factor).is_integer()

	# Taps per phase should be a multiple of our SIMD lane count for efficiency
	simd_lanes = 8
	assert params.taps_per_phase % simd_lanes == 0

	# We want each phase of our filter to be taps_per_phase taps long, meaning the total number of taps would be
	# upsample_factor * taps_per_phase. However, our filter must be an odd length in order to have integer latency, so
	# we remove one tap. The coefficient count is one more than the filter order.
	filter_order = params.upsample_factor * params.taps_per_phase - 2

	epsilon = 0.0000001

	notch_filters = np.ones(1)
	for notch_frequency in params.notch_frequencies:
		# Build the notch filter: an FIR of N 1s creates N notches linearly spaced across the spectrum
		upsampled_notch_frequency = notch_frequency / params.upsample_factor
		notch_filter_length = int(round(1.0 / upsampled_notch_frequency))

		# Normalize gain by dividing by the sum, which is equal to the length
		notch_filter = np.repeat(1.0 / notch_filter_length, notch_filter_length)

		# Convolve this notch filter with all the others
		notch_filters = np.convolve(notch_filters, notch_filter)

	# Determine the number of coefficients to generate so that convolving with the notch filters will produce a filter
	# of the correct length
	pre_convolve_filter_order = filter_order - len(notch_filters) + 1

	# Make sure the pre-convolved filter is still of even order so we get an odd number of coefficients
	assert pre_convolve_filter_order % 2 == 0

	# Generate the window
	if params.window_type is WindowType.RECTANGULAR:
		window = np.repeat(1.0, pre_convolve_filter_order + 1)
	elif params.window_type is WindowType.HANN:
		window = scipy.signal.windows.hann(pre_convolve_filter_order + 1)
	elif params.window_type is WindowType.HAMMING:
		window = scipy.signal.windows.hamming(pre_convolve_filter_order + 1)
	elif params.window_type is WindowType.BLACKMAN:
		window = scipy.signal.windows.blackman(pre_convolve_filter_order + 1)
	elif params.window_type is WindowType.KAISER:
		window = scipy.signal.windows.kaiser(pre_convolve_filter_order + 1, params.kaiser_beta)
	else:
		raise Exception("Unknown window type '{}'".format(params.window_type))

	# Make sure window is centered and symmetrical
	assert abs(window[pre_convolve_filter_order // 2] - 1.0) < epsilon
	for i in range(pre_convolve_filter_order):
		assert abs(window[i] - window[-(i + 1)]) < epsilon

	# Analyze the window to determine how we should shift the cutoff to compensate for transition bandwidth
	# Note: why does the side lobe magnitude seem to be less than the attenuation measured after applying the window?
	# Note: do we want to adjust by less than the full lobe width?
	main_lobe_width, side_lobe_magnitude = analyze_window(window)
	transition_bandwidth = main_lobe_width * params.upsample_factor / (2.0 * np.pi)

	# Adjust the cutoff based on window analysis results
	windowed_cutoff = params.cutoff_frequency - transition_bandwidth * 0.5
	upsampled_windowed_cutoff = windowed_cutoff / params.upsample_factor

	# Construct the FFT of an ideal lowpass filter, compensating for the notch filter shape
	fft_factor = 32
	fft_length = params.upsample_factor * params.taps_per_phase * fft_factor
	ideal_filter_fft = np.zeros(fft_length)
	notch_filters_fft = np.fft.fft(notch_filters, fft_length)
	fft_cutoff_line_index = int(round(upsampled_windowed_cutoff * fft_length))
	for i in range(fft_cutoff_line_index):
		ideal_filter_fft[i] = 1.0 / abs(notch_filters_fft[i])

	# Mirror and conjugate to keep the filter real
	ideal_filter_fft[fft_length // 2 + 1 :] = np.conjugate(np.flip(ideal_filter_fft[1 : fft_length // 2]))

	# Transform to time-domain, make sure the filter is real
	ideal_filter_coefficients = np.fft.ifft(ideal_filter_fft)
	assert all(np.isreal(ideal_filter_coefficients))
	ideal_filter_coefficients = np.real(ideal_filter_coefficients)

	# Center the filter
	ideal_filter_coefficients = np.fft.fftshift(ideal_filter_coefficients)

	# Grab a slice of the coefficients
	start_index = fft_length // 2 - pre_convolve_filter_order // 2
	end_index = start_index + pre_convolve_filter_order + 1
	pre_convolve_filter_coefficients = ideal_filter_coefficients[start_index : end_index]

	# Make sure our coefficients are centered and symmetrical (so that the filter is linear phase)
	for i in range(pre_convolve_filter_order):
		assert abs(pre_convolve_filter_coefficients[i] - pre_convolve_filter_coefficients[-(i + 1)]) < epsilon

	# Apply the window
	pre_convolve_filter_coefficients *= window

	# Convolve with the notch filters
	coefficients = np.convolve(pre_convolve_filter_coefficients, notch_filters)

	if plot:
		w, h = fir_freqz_fast(coefficients, 16, params.upsample_factor)

		transition_band_threshold_db = -1.0
		stopband_attenuation = -math.inf
		transition_band_frequency = None
		ripple_min = 1.0
		ripple_max = 1.0
		running_ripple_min = 1.0
		running_ripple_max = 1.0
		previous_mag = -math.inf
		for freq, mag in zip(w, h):
			# Detect the end of the transition band
			if transition_band_frequency is None and db(abs(mag)) < transition_band_threshold_db:
				transition_band_frequency = freq

			# If we're still in the transition band, measure ripple
			if transition_band_frequency is None:
				running_ripple_min = min(running_ripple_min, abs(mag))
				running_ripple_max = max(running_ripple_max, abs(mag))
				if abs(mag) > previous_mag:
					ripple_min = running_ripple_min
					ripple_max = running_ripple_max
				previous_mag = abs(mag)

			# If we're in the stopband, measure attenuation
			if freq > params.cutoff_frequency:
				stopband_attenuation = max(stopband_attenuation, abs(mag))

		print("Max ripple: {}dB".format(max(-db(ripple_min), db(ripple_max))))
		print("Transition start: {} ({}hz @ {}hz)".format(
			transition_band_frequency,
			int(transition_band_frequency * plot_sample_rate),
			plot_sample_rate))
		print("Transition bandwidth: {} ({}hz @ {}hz)".format(
			params.cutoff_frequency - transition_band_frequency,
			int((params.cutoff_frequency - transition_band_frequency) * plot_sample_rate),
			plot_sample_rate))
		print("Stopband attenuation: {}dB".format(db(stopband_attenuation)))

		fig, axes = plt.subplots(1, 2)

		coefficients_axes, response_axes = axes

		coefficients_axes.set_title("FIR coefficients")
		coefficients_axes.plot(coefficients, linewidth = 1)

		response_axes.set_title("FIR response")
		response_axes.plot(w * plot_sample_rate, db(abs(h)), 'b', linewidth = 1)

		analysis_points = [
			[
				(transition_band_frequency, ripple_min),
				(0.0, ripple_min),
				(0.0, ripple_max),
				(params.cutoff_frequency, ripple_max),
				(params.cutoff_frequency, stopband_attenuation)
			],
			[
				(params.upsample_factor * 0.5, stopband_attenuation),
				(transition_band_frequency, stopband_attenuation),
				(transition_band_frequency, ripple_max)
			]
		]

		for points in analysis_points:
			response_axes.plot(
				[x * plot_sample_rate for x, y in points],
				[db(y) for x, y in points],
				"r",
				linewidth = 1)

		response_axes.set_xlim(0, plot_sample_rate * 2) # Don't bother looking much past the cutoff

		plt.show()

	# Finally, multiply by our upsample factor. This is because the filter runs on a signal with (upsample_factor - 1)
	# zeros for every nonzero value, so we must compensate for the loss in gain.
	coefficients *= params.upsample_factor

	# Now we divide the filter up into phases. Let x be a signal with samples (a b c d ...). To upsample x, we
	# insert N zeros between each sample where N = upsample_factor - 1 to obtain:
	# (a 0 0 0 ... b 0 0 0 ... c 0 0 0 ... d 0 0 0 ...)
	# We then run our FIR on the upsampled signal. Most of our FIR coefficients will end up getting multiplied with 0 so
	# we can discard them. The set of samples that don't get discarded depends on the index of the upsampled sample
	# we're computing. Each unique set of coefficients is called a phase. We calculate the phases here.

	# Here is a diagram of how polyphase filtering works using an upsample factor of 4 and 4 taps per phase. Line S
	# contains the upsampled signal history buffer, where each . represents a zero and the numbers represent the
	# negative sample indices. Lines 0-3 represent which coefficients get multiplied with each sample for that phase,
	# and the line below shows only the trimmed down set of coefficients which aren't multiplied with 0. (Coefficient
	# indices only include 1 digit for display purposes, and the x represents the padded 0.)
	# S ...3...2...1...0...
	#      |   |   |   |
	# 0 x432109876543210
	#      2   8   4   0
	# 1  x432109876543210
	#      3   9   5   1
	# 2   x432109876543210
	#      4   0   6   2
	# 3    x432109876543210
	#      x   1   7   3

	# Modification: in practice, we blend between phases i and i+1. This means that sometimes we will need to blend
	# between phase (upsample_count - 1) and phase 0. Sampling from phase 0 would require shifting over by 1 input
	# sample, which means that in the general case we need to make two passes over the input samples. To get around
	# this, we shift all coefficient indices back by 1 (which flips the padded 0 to the beginning) and add a sample of
	# latency:
	# S ...3...2...1...0...
	#      |   |   |   |
	# 0 432109876543210x
	#      1   7   3   x
	# 1  432109876543210x
	#      2   8   4   0
	# 2   432109876543210x
	#      3   9   5   1
	# 3    432109876543210x
	#      4   0   6   2
	# 4     432109876543210x
	#          1   7   3   x
	# Now we can add an additional phase and because the padded zero comes at the beginning of the FIR, the extra sample
	# that we would be multiplying it with can be ignored.

	result = PolyphaseFir()
	for phase_index in range(params.upsample_factor):
		phase_coefficients = []
		result.phases.append(phase_coefficients)
		for tap in range(params.taps_per_phase):
			# Store the phase coefficients in reverse order because we multiply them with the history buffer which is
			# stored in forward order. We subtract 1 to perform the extra phase trick described above.
			coefficient_index = (params.taps_per_phase - tap - 1) * params.upsample_factor + phase_index - 1
			assert coefficient_index >= -1
			coefficient = 0.0 if coefficient_index < 0 else coefficients[coefficient_index]
			phase_coefficients.append(coefficient)

	# Add the extra phase
	extra_phase_coefficients = [0.0] + result.phases[0][: params.taps_per_phase - 1]
	result.phases.append(extra_phase_coefficients)

	# One additional sample of latency is due to the extra phase trick described above
	upsampled_latency = filter_order // 2 + 1

	# We care about the latency at the original sample rate. Make sure it's an integer value (adding the additional
	# sample of latency actually causes this to be the case).
	assert upsampled_latency % params.upsample_factor == 0
	result.latency = upsampled_latency // params.upsample_factor

	return result

def fir_freqz_fast(coefficients, upsample_factor, sample_rate):
	length = (1 << int(math.ceil(math.log2(len(coefficients))))) * upsample_factor
	fft = np.fft.fft(coefficients, length)

	# Include nyquist
	return np.linspace(0, sample_rate * 0.5, length // 2 + 1), fft[: length // 2 + 1]

def analyze_window(window):
	analysis_length = max(len(window), 65536) * 4
	analysis_length = 1 << int(math.ceil(math.log2(analysis_length)))
	w, h = scipy.signal.freqz(window, worN = analysis_length)

	main_lobe_magnitude = max(abs(h))

	# Find the start of the side lobes
	side_lobe_start_index = 0
	y_prev = abs(h[0])
	for i, y in enumerate(h):
		if abs(y) > y_prev:
			side_lobe_start_index = i
			break
		y_prev = abs(y)

	main_lobe_width = 2.0 * w[side_lobe_start_index - 1]
	side_lobe_magnitude = max(abs(x) for x in h[side_lobe_start_index:])

	#fig, ax = plt.subplots()
	#ax.plot(w, db(abs(h)), 'b', linewidth = 1)
	#plt.show()

	return main_lobe_width, side_lobe_magnitude / main_lobe_magnitude

def db(magnitude):
	# This is for display purposes only, so clamp the minimum magnitude so log10 doesn't fail on 0
	return 20.0 * np.log10(np.maximum(magnitude, 1e-10))
