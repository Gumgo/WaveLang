module real delay(in real delay_time, in real x) {
	return __native_delay(delay_time, x);
}