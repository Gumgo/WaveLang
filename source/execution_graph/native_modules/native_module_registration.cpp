#include "native_module_registration_inputs.h"

#include "execution_graph/native_module.h"
#include "execution_graph/native_module_registry.h"
#include "execution_graph/native_modules/native_module_registration.h"

#include "generated/registration_generated.inl"

void register_native_modules(bool optimizations_enabled) {
	c_native_module_registry::begin_registration(optimizations_enabled);

	// Register all operators first
	for (uint32 op = 0; op < k_native_operator_count; op++) {
		e_native_operator native_operator = static_cast<e_native_operator>(op);
		c_native_module_registry::register_native_operator(
			native_operator, get_native_operator_native_module_name(native_operator));
	}

	//bool result = // $TODO $PLUGIN handle failure
	register_native_modules();

	// $TODO plugin native modules would be registered here

	c_native_module_registry::end_registration();
}
