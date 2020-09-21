import matplotlib.pyplot as plt
import numpy as np
import scipy.signal

import math

def generate_waveform_samples(harmonic_weights, upsample_factor):
	max_frequency = len(harmonic_weights)
	sample_count = max_frequency * 4 # Oversample by a factor of 2 so we can represent a sine wave, rather than cosine
	upsampled_sample_count = sample_count * upsample_factor
	buffer = [0.0] * upsampled_sample_count

	for frequency_index, weight in enumerate(harmonic_weights):
		frequency = frequency_index + 1
		frequency_multiplier = 2.0 * math.pi * frequency / upsampled_sample_count
		for i in range(upsampled_sample_count):
			buffer[i] += weight * math.sin(i * frequency_multiplier)

	return buffer

def generate_interpolation_coefficients(samples):
	# Perform least-squares fit of cubic polynomial with constraints at the endpoints

	# Apply first constraint by removing constant term and subtracting constant offset
	constant_offset = samples[0]
	for i in range(len(samples)):
		samples[i] -= constant_offset

	# We want to fit the curve p1*x + p2*x^2 + p3*x^3 (we've already removed the constant factor)
	# Constrained least squares: http://www.seas.ucla.edu/~vandenbe/133A/lectures/cls.pdf
	# Solve [ A^T A   C^T ] [ x ] = [ A^T b  ]
	#       [   C      0  ] [ z ]   [   d    ]
	# where f(x) = |Ax - b|^2 is the function we wish to minimize and C^T_i x = d_i are constraints. Let n be the number
	# of input samples. Let sx_i and sy_i be the x and y coordinates of the ith sample. Because the samples are linearly
	# spaced, sx_i = i/(n-1), and therefore sx_0 = 0 and sx_{n-1} = 1. The matrix A then becomes a matrix of rows (sx_i,
	# sx_i^2, sx_i^3) rows for i from 1 to n-2:
	# [   sx_1       sx_1^2       sx_1^3   ]
	# [   sx_2       sx_2^2       sx_2^3   ]
	# [               ...                  ]
	# [ sx_{n-2}   sx_{n-2}^2   sx_{n-2}^3 ]
	# We exclude samples 0 and n-1 because sample 0 has been accounted for above and we will add a equality constraint
	# for sample n-1. Our vector b is then [ sy_1 sy_2 ... sy_{n-2} ]. We have only one constraint, sample n-1, so our
	# matrix C is simply [ 1 1 1 ] and d is sy_{n-1}.

	sample_count = len(samples)
	sample_xs = [i / (sample_count - 1) for i in range(sample_count)]

	# Construct matrix A:
	a_rows = sample_count - 2
	a_columns = 3
	matrix_a = [0.0] * (a_rows * a_columns)
	for i in range(sample_count - 2):
		sample_x = sample_xs[i + 1]
		sample_x2 = sample_x * sample_x
		matrix_a[i * a_columns] = sample_x
		matrix_a[i * a_columns + 1] = sample_x2
		matrix_a[i * a_columns + 2] = sample_x * sample_x2

	# Construct the matrix
	c_rows = 1
	c_columns = 3
	matrix_rows = a_columns + c_rows
	matrix_columns = a_columns + c_rows

	matrix = [0.0] * (matrix_rows * matrix_columns)

	# Put 2A^T A in the upper left
	for row in range(a_columns):
		for column in range(a_columns):
			sum = 0.0
			for i in range(a_rows):
				sum += matrix_a[i * a_columns + row] * matrix_a[i * a_columns + column]
			matrix[row * matrix_columns + column] = sum

	# Put C^T in the upper right
	for row in range(c_columns):
		column = matrix_columns - 1
		matrix[matrix_columns * row + column] = 1.0

	# Put C in the lower left
	for column in range(c_columns):
		row = matrix_rows - 1
		matrix[matrix_columns * row + column] = 1.0

	# Construct the vector [ A^T b   1 ]
	vector = [0.0] * (a_columns + 1)
	for i in range(a_columns):
		sum = 0.0
		for j in range(sample_count - 2):
			sum += matrix_a[j * a_columns + i] * samples[j + 1]
		vector[i] = sum
	vector[a_columns] = samples[-1]

	result = solve_system(matrix, vector)
	coefficients = [constant_offset] + result[0 : 3]
	return coefficients

def solve_system(matrix, vector):
	# https://en.wikipedia.org/wiki/LU_decomposition
	n = len(vector)
	assert len(matrix) == n * n

	lu = [0.0] * (n * n)
	for i in range(n):
		for j in range(i, n):
			sum = 0.0
			for k in range(i):
				sum += lu[i * n + k] * lu[k * n + j]
			lu[i * n + j] = matrix[i * n + j] - sum
		for j in range(i + 1, n):
			sum = 0.0
			for k in range(i):
				sum += lu[j * n + k] * lu[k * n + i]
			lu[j * n + i] = (1.0 / lu[i * n + i]) * (matrix[j * n + i] - sum)

	# lu = L + U - I
 	# Find solution of Ly = b
	y = [0.0] * n
	for i in range(n):
		sum = 0.0
		for k in range(i):
			sum += lu[i * n + k] * y[k]
		y[i] = vector[i] - sum

	# Find solution of Ux = y
	x = [0.0] * n
	for j in range(n):
		i = n - j - 1
		sum = 0.0
		for k in range(i + 1, n):
			sum += lu[i * n + k] * x[k]
		x[i] = (1.0 / lu[i * n + i]) * (y[i] - sum)

	return x

def interpolate_signal(samples, upsample_factor):
	interpolated_samples = [0.0] * len(samples)
	for i in range(0, len(samples), upsample_factor):
		interpolation_samples = samples[i : i + upsample_factor] + [samples[(i + upsample_factor) % len(samples)]]
		coefficients = generate_interpolation_coefficients(interpolation_samples)
		for t in range(upsample_factor):
			fraction = t / upsample_factor
			value = (coefficients[0] +
				fraction * (coefficients[1] +
				fraction * (coefficients[2] +
				fraction * coefficients[3])))
			interpolated_samples[i + t] = value
	return interpolated_samples

def db(magnitude):
	# This is for display purposes only, so clamp the minimum magnitude so log10 doesn't fail on 0
	return 20.0 * np.log10(np.maximum(magnitude, 1e-10))

def analyze(harmonics, upsample_factor):
	signal = generate_waveform_samples(harmonics, upsample_factor)
	interpolated_signal = interpolate_signal(signal, upsample_factor)

	error = [x - y for x, y in zip(signal, interpolated_signal)]
	max_error = max(abs(x) for x in error)
	rms_error = math.sqrt(sum(x * x for x in error) / len(error))
	print("Max error: {}".format(max_error))
	print("RMS error: {}".format(rms_error))

	repeat_count = 64
	fft_length = len(signal) * repeat_count
	fft_orig = np.fft.fft(signal * repeat_count) / (fft_length * 0.5)
	fft_interp = np.fft.fft(interpolated_signal * repeat_count) / (fft_length * 0.5)
	assert len(fft_orig) == fft_length
	assert len(fft_interp) == fft_length
	fft_x = [x / len(fft_orig) * upsample_factor for x in range(fft_length)]
	fft_slice = slice(0, fft_length // 2 + 1)

	max_aliasing = max(abs(x) for x in fft_interp[fft_length // upsample_factor : fft_length // 2 + 1])
	print("Max aliasing: {}dB".format(db(max_aliasing)))

	fig, axes = plt.subplots(2, 2)

	(signal_axes, error_axes), (original_fft_axes, interpolated_fft_axes) = axes

	signal_axes.set_title("Raw vs. interpolated signals")
	signal_axes.plot(range(len(signal)), signal, linewidth = 1)
	signal_axes.plot(range(0, len(signal), upsample_factor), signal[: : upsample_factor], 'o', markersize = 2)
	signal_axes.plot(range(len(interpolated_signal)), interpolated_signal, linewidth = 1)

	error_axes.set_title("Raw vs. interpolated error")
	error_axes.plot(range(len(error)), error, linewidth = 1)

	original_fft_axes.set_title("Raw signal FFT")
	original_fft_axes.plot(fft_x[fft_slice], db(abs(fft_orig[fft_slice])), linewidth = 1)

	interpolated_fft_axes.set_title("Interpolated signal FFT")
	interpolated_fft_axes.plot(fft_x[fft_slice], db(abs(fft_interp[fft_slice])), linewidth = 1)

	plt.show()

harmonic_count = 16
sine_harmonics = [1.0] + [0.0] * (harmonic_count - 1)
square_harmonics = [0.0 if k % 2 == 1 else 4.0 / (math.pi * (k + 1)) for k in range(harmonic_count)]
sawtooth_harmonics = [math.pow(-1, k + 1) / (k + 1) for k in range(harmonic_count)]
triangle_harmonics = [0.0 if k % 2 == 1 else (8.0 / (math.pi * math.pi)) * math.pow(-1, k // 2) / ((k + 1) * (k + 1))
	for k in range(harmonic_count)]

upsample_factor = 8
analyze(square_harmonics, upsample_factor)
