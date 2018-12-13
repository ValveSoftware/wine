/*
 * Copyright 2018 Hans Leidekker for CodeWeavers
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
 *
 */

gnutls_alert_description_t (*pgnutls_alert_get)(gnutls_session_t session);
const char * (*pgnutls_alert_get_name)(gnutls_alert_description_t alert);
int (*pgnutls_certificate_allocate_credentials)(gnutls_certificate_credentials_t *);
void (*pgnutls_certificate_free_credentials)(gnutls_certificate_credentials_t);
const gnutls_datum_t * (*pgnutls_certificate_get_peers)(gnutls_session_t, unsigned int *);
gnutls_cipher_algorithm_t (*pgnutls_cipher_get)(gnutls_session_t);
size_t (*pgnutls_cipher_get_key_size)(gnutls_cipher_algorithm_t);
int (*pgnutls_credentials_set)(gnutls_session_t, gnutls_credentials_type_t, void *);
void (*pgnutls_deinit)(gnutls_session_t);
int (*pgnutls_handshake)(gnutls_session_t);
int (*pgnutls_init)(gnutls_session_t *, unsigned int);
gnutls_kx_algorithm_t (*pgnutls_kx_get)(gnutls_session_t);
gnutls_mac_algorithm_t (*pgnutls_mac_get)(gnutls_session_t);
size_t (*pgnutls_mac_get_key_size)(gnutls_mac_algorithm_t);
void (*pgnutls_perror)(int);
gnutls_protocol_t (*pgnutls_protocol_get_version)(gnutls_session_t);
int (*pgnutls_priority_set_direct)(gnutls_session_t, const char *, const char **);
size_t (*pgnutls_record_get_max_size)(gnutls_session_t);
ssize_t (*pgnutls_record_recv)(gnutls_session_t, void *, size_t);
ssize_t (*pgnutls_record_send)(gnutls_session_t, const void *, size_t);
int (*pgnutls_server_name_set)(gnutls_session_t, gnutls_server_name_type_t, const void *, size_t);
gnutls_transport_ptr_t (*pgnutls_transport_get_ptr)(gnutls_session_t);
void (*pgnutls_transport_set_errno)(gnutls_session_t, int);
void (*pgnutls_transport_set_ptr)(gnutls_session_t, gnutls_transport_ptr_t);
void (*pgnutls_transport_set_pull_function)(gnutls_session_t, gnutls_pull_func);
void (*pgnutls_transport_set_push_function)(gnutls_session_t, gnutls_push_func);

/* Not present in gnutls version < 2.9.10. */
int (*pgnutls_cipher_get_block_size)(gnutls_cipher_algorithm_t algorithm);
