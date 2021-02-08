#include "instrument/native_modules/scrape_native_modules.h"

namespace core_native_modules {
	void scrape_native_modules();
}

namespace array_native_modules {
	void scrape_native_modules();
}

namespace math_native_modules {
	void scrape_native_modules();
}

namespace sampler_native_modules {
	void scrape_native_modules();
}

namespace delay_native_modules {
	void scrape_native_modules();
}

namespace filter_native_modules {
	void scrape_native_modules();
}

namespace time_native_modules {
	void scrape_native_modules();
}

namespace controller_native_modules {
	void scrape_native_modules();
}

namespace stream_native_modules {
	void scrape_native_modules();
}

namespace json_native_modules {
	void scrape_native_modules();
}

namespace resampler_native_modules {
	void scrape_native_modules();
}

void scrape_native_modules() {
	core_native_modules::scrape_native_modules();
	array_native_modules::scrape_native_modules();
	math_native_modules::scrape_native_modules();
	sampler_native_modules::scrape_native_modules();
	delay_native_modules::scrape_native_modules();
	filter_native_modules::scrape_native_modules();
	time_native_modules::scrape_native_modules();
	controller_native_modules::scrape_native_modules();
	stream_native_modules::scrape_native_modules();
	json_native_modules::scrape_native_modules();
	resampler_native_modules::scrape_native_modules();
}
