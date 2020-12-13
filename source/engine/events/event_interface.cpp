#include "engine/events/async_event_handler.h"
#include "engine/events/event_interface.h"

// Initialized to zero
static f_build_event_string g_event_string_builders[256];
static size_t g_next_event_data_type_id = 0;

static constexpr const char *k_event_level_strings[] = {
	"VERBOSE:  ",
	"MESSAGE:  ",
	"WARNING:  ",
	"ERROR:    ",
	"CRITICAL: "
};

STATIC_ASSERT(is_enum_fully_mapped<e_event_level>(k_event_level_strings));

uint8 register_event_data_type_internal(f_build_event_string build_event_string) {
	wl_assert(g_next_event_data_type_id < array_count(g_event_string_builders));
	wl_assert(!g_event_string_builders[g_next_event_data_type_id]);
	g_event_string_builders[g_next_event_data_type_id] = build_event_string;

	uint8 id = static_cast<uint8>(g_next_event_data_type_id);
	g_next_event_data_type_id++;
	return id;
}

void c_event_interface::initialize(c_async_event_handler *event_handler) {
	m_event_handler = event_handler;
}

void c_event_interface::submit(const c_event &e) {
	if (m_event_handler) {
		c_wrapped_array<const uint8> event_data = e.get_event_data();
		m_event_handler->submit_event(event_data.get_count(), event_data.get_pointer());
	}
}

e_event_level c_event_interface::build_event_string(
	size_t event_size,
	const void *event_data,
	c_event_string &event_string_out) {
	e_event_level event_level = e_event_level::k_invalid;
	event_string_out.clear();

	size_t offset = 0;
	const uint8 *event_bytes = static_cast<const uint8 *>(event_data);

	// Read the event level
	if (offset + sizeof(uint8) <= event_size) {
		event_level = static_cast<e_event_level>(*event_bytes);
		wl_assert(valid_enum_index(event_level));
		event_string_out.append_truncate(k_event_level_strings[enum_index(event_level)]);
		offset += sizeof(uint8);
	}

	// Loop through each piece of event data and append to the string
	while (offset < event_size) {
		uint8 event_data_type_id = event_bytes[offset];
		wl_assert(valid_index(event_data_type_id, array_count(g_event_string_builders)));
		wl_assert(g_event_string_builders[event_data_type_id]);
		offset++;

		size_t advance = g_event_string_builders[event_data_type_id](event_string_out, event_bytes + offset);
		offset += advance;
		wl_assert(offset <= event_size);
	}

	return event_level;
}