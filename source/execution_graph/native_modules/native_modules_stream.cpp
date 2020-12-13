#include "execution_graph/instrument_globals.h"
#include "execution_graph/native_modules/native_modules_stream.h"

namespace stream_native_modules {

	void get_sample_rate(const s_native_module_context &context, real32 &result) {
		if (context.instrument_globals->sample_rate == 0) {
			context.diagnostic_interface->error(
				"Sample rate is only available in targeted builds; "
				"it can be specified using the #sample_rate synth global");
			result = 0.0f;
		} else {
			result = static_cast<real32>(context.instrument_globals->sample_rate);
		}
	}

}
