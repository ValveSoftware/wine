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

int (*pgnutls_cipher_init)(gnutls_cipher_hd_t *, gnutls_cipher_algorithm_t, const gnutls_datum_t *,
                           const gnutls_datum_t *);
void (*pgnutls_cipher_deinit)(gnutls_cipher_hd_t);
void (*pgnutls_perror)(int);
int (*pgnutls_cipher_encrypt2)(gnutls_cipher_hd_t, const void *, size_t, void *, size_t);
int (*pgnutls_cipher_decrypt2)(gnutls_cipher_hd_t, const void *, size_t, void *, size_t);

int (*pgnutls_pubkey_init)(gnutls_pubkey_t *);
void (*pgnutls_pubkey_deinit)(gnutls_pubkey_t);

/* Not present in gnutls version < 3.0 */
int (*pgnutls_cipher_tag)(gnutls_cipher_hd_t, void *, size_t);
int (*pgnutls_cipher_add_auth)(gnutls_cipher_hd_t, const void *, size_t);
int (*pgnutls_pubkey_import_ecc_raw)(gnutls_pubkey_t, gnutls_ecc_curve_t, const gnutls_datum_t *,
                                     const gnutls_datum_t *);
gnutls_sign_algorithm_t (*pgnutls_pk_to_sign)(gnutls_pk_algorithm_t, gnutls_digest_algorithm_t);
int (*pgnutls_pubkey_verify_hash2)(gnutls_pubkey_t, gnutls_sign_algorithm_t, unsigned int, const gnutls_datum_t *,
                                   const gnutls_datum_t *);
/* Not present in gnutls version < 2.11.0 */
int (*pgnutls_pubkey_import_rsa_raw)(gnutls_pubkey_t, const gnutls_datum_t *, const gnutls_datum_t *);
