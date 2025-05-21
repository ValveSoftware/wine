#ifndef _sandbox_H_
#define _sandbox_H_

void* sandbox_apply(void);
void* sandbox_apply_container(void);
void* sandbox_compile_entitlements(void);
void* sandbox_compile_file(void);
void* sandbox_compile_named(void);
void* sandbox_compile_string(void);
void* sandbox_container_paths_iterate_items(void);
void* sandbox_create_params(void);
void* sandbox_free_params(void);
void* sandbox_free_profile(void);
void* sandbox_inspect_pid(void);
void* sandbox_inspect_smemory(void);
void* sandbox_set_param(void);
void* sandbox_set_user_state_item(void);
void* sandbox_user_state_item_buffer_create(void);
void* sandbox_user_state_item_buffer_destroy(void);
void* sandbox_user_state_item_buffer_send(void);
void* sandbox_user_state_iterate_items(void);

#endif
