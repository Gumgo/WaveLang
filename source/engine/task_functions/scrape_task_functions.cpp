#include "engine/task_functions/scrape_task_functions.h"

namespace core_task_functions {
	void scrape_task_functions();
}

namespace array_task_functions {
	void scrape_task_functions();
}

namespace math_task_functions {
	void scrape_task_functions();
}

namespace sampler_task_functions {
	void scrape_task_functions();
}

namespace delay_task_functions {
	void scrape_task_functions();
}

namespace filter_task_functions {
	void scrape_task_functions();
}

namespace time_task_functions {
	void scrape_task_functions();
}

namespace controller_task_functions {
	void scrape_task_functions();
}

namespace stream_task_functions {
	void scrape_task_functions();
}

namespace json_task_functions {
	void scrape_task_functions();
}

void scrape_task_functions() {
	core_task_functions::scrape_task_functions();
	array_task_functions::scrape_task_functions();
	math_task_functions::scrape_task_functions();
	sampler_task_functions::scrape_task_functions();
	delay_task_functions::scrape_task_functions();
	filter_task_functions::scrape_task_functions();
	time_task_functions::scrape_task_functions();
	controller_task_functions::scrape_task_functions();
	stream_task_functions::scrape_task_functions();
	json_task_functions::scrape_task_functions();
}
