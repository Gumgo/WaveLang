#include "engine/events/event_data_types.h"

uint8 s_event_data_type_definition<bool>::id;
uint8 s_event_data_type_definition<int8>::id;
uint8 s_event_data_type_definition<uint8>::id;
uint8 s_event_data_type_definition<int16>::id;
uint8 s_event_data_type_definition<uint16>::id;
uint8 s_event_data_type_definition<int32>::id;
uint8 s_event_data_type_definition<uint32>::id;
uint8 s_event_data_type_definition<int64>::id;
uint8 s_event_data_type_definition<uint64>::id;
uint8 s_event_data_type_definition<real32>::id;
uint8 s_event_data_type_definition<real64>::id;
uint8 s_event_data_type_definition<const char *>::id;
uint8 s_event_data_type_definition<c_dstr>::id;

void initialize_event_data_types() {
	register_event_data_type<bool>();
	register_event_data_type<int8>();
	register_event_data_type<uint8>();
	register_event_data_type<int16>();
	register_event_data_type<uint16>();
	register_event_data_type<int32>();
	register_event_data_type<uint32>();
	register_event_data_type<int64>();
	register_event_data_type<uint64>();
	register_event_data_type<real32>();
	register_event_data_type<real64>();
	register_event_data_type<const char *>();
	register_event_data_type<c_dstr>();
}