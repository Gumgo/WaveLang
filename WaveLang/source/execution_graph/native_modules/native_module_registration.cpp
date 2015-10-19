#include "execution_graph/native_modules/native_module_registration.h"
#include "execution_graph/native_module_registry.h"

#include "execution_graph/native_modules/native_modules_basic.h"
#include "execution_graph/native_modules/native_modules_utility.h"
#include "execution_graph/native_modules/native_modules_sampler.h"
#include "execution_graph/native_modules/native_modules_delay.h"
#include "execution_graph/native_modules/native_modules_filter.h"

void register_native_modules(bool optimizations_enabled) {
	c_native_module_registry::begin_registration(optimizations_enabled);

	register_native_modules_basic();
	register_native_modules_utility();
	register_native_modules_sampler();
	register_native_modules_delay();
	register_native_modules_filter();

	// $TODO plugin native modules would be registered here

	c_native_module_registry::end_registration();
}