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

extern int do_fsync(void);
extern void fsync_init(void);
extern unsigned int fsync_alloc_shm( int low, int high );
extern void fsync_free_shm_idx( int shm_idx );
extern void fsync_wake_futex( unsigned int shm_idx );
extern void fsync_clear_futex( unsigned int shm_idx );
extern void fsync_wake_up( struct object *obj );
extern void fsync_clear( struct object *obj );

struct fsync;

extern const struct object_ops fsync_ops;
extern void fsync_set_event( struct fsync *fsync );
extern void fsync_reset_event( struct fsync *fsync );
extern void fsync_abandon_mutexes( struct thread *thread );
extern void fsync_cleanup_process_shm_indices( process_id_t id );
