#include "common/threading/atomics.h"
#include "common/threading/lock_free.h"

void lock_free_list_push(c_lock_free_handle_array &node_storage, s_lock_free_handle &list_head, uint32 handle) {
	// Push the node to the head of the list
	wl_assert(handle != k_lock_free_invalid_handle);
	wl_assert(VALID_INDEX(handle, node_storage.get_count()));

	struct s_update_head {
		s_lock_free_handle *next_handle_pointer;
		uint32 next_tag;
		uint32 new_head_handle;

		int64 operator()(int64 value) const {
			// Read the old head's value
			s_lock_free_handle_data head;
			head.data = value;

			// Point next to the old head
			s_lock_free_handle_data next;
			next.handle = head.handle;
			next.tag = next_tag;
			// Store the updated handle. At this point, this node is still owned by us, so we don't need to sync, and if
			// this value becomes invalid, it is okay to loop and set it again.
			next_handle_pointer->handle = next.data;

			// Assign the new head handle and increment the tag to solve the ABA problem
			head.handle = new_head_handle;
			head.tag++;

			// Return the new data to attempt to update atomically. If we fail, we will loop and reassign both the next
			// handle and the new head.
			return head.data;
		}
	};

	s_update_head update_head;
	s_lock_free_handle_data next;

	// Obtain the next handle - since the node was previously owned by this thread, we don't have to sync its handle
	update_head.next_handle_pointer = &node_storage[handle];
	next.data = update_head.next_handle_pointer->handle;
	update_head.next_tag = next.tag + 1;

	// Point the new head at this node
	update_head.new_head_handle = handle;

	// Perform the update atomically
	execute_atomic(list_head.handle, update_head);
}

uint32 lock_free_list_pop(c_lock_free_handle_array &node_storage, s_lock_free_handle &list_head) {
	// Pop node from the head of the list

	struct s_update_head {
		c_lock_free_handle_array *node_storage;
		uint32 *out_old_head_handle;

		int64 operator()(int64 value) const {
			// Read the head's current value
			s_lock_free_handle_data head;
			head.data = value;

			// Store the current head's handle so we can return it to the user
			*out_old_head_handle = head.handle;

			if (head.handle == k_lock_free_invalid_handle) {
				// List is empty - don't modify
				return head.data;
			}

			// Point the head at the next node's next
			const s_lock_free_handle &next_handle = (*node_storage)[head.handle];
			s_lock_free_handle_data next;
			next.data = next_handle.handle;
			head.handle = next.handle;
			// Increment tag to avoid ABA problem
			head.tag++;

			// Attempt to update the head, looping if we fail
			return head.data;
		}
	};

	s_update_head update_head;
	uint32 old_head_handle;

	update_head.node_storage = &node_storage;
	update_head.out_old_head_handle = &old_head_handle;

	// Perform the update atomically
	execute_atomic(list_head.handle, update_head);

	return old_head_handle;
}

#if IS_TRUE(ASSERTS_ENABLED)
uint32 lock_free_list_count_unsafe(const c_lock_free_handle_array &node_storage, const s_lock_free_handle &list_head) {
	uint32 count = 0;

	s_lock_free_handle_data next;
	next.data = list_head.handle;

	while (next.handle != k_lock_free_invalid_handle) {
		count++;
		next.data = node_storage[next.handle].handle;
	}

	return count;
}
#endif // IS_TRUE(ASSERTS_ENABLED)
