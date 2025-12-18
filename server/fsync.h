/*
 * futex-based synchronization objects
 *
 * Copyright (C) 2018 Zebediah Figura
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

extern void fsync_init(void);
extern unsigned int fsync_alloc_shm( int low, int high );
extern void fsync_free_shm_idx( int shm_idx );
int fsync_grab_shm_idx( unsigned int shm_idx );

extern const struct object_ops fsync_ops;
extern void fsync_set_event( unsigned int shm_idx );
extern void fsync_reset_event( unsigned int shm_idx );
extern void fsync_abandon_mutex( unsigned int shm_idx, thread_id_t tid );
extern void fsync_cleanup_process_shm_indices( process_id_t id );

extern int fsync_check_support(void);
extern int do_fsync_cached;

static inline int do_fsync(void)
{
    if (do_fsync_cached != -1) return do_fsync_cached;
    return (do_fsync_cached = fsync_check_support());
}
