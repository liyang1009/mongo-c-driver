/*
 * Copyright 2016 MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MONGOC_STREAM_TLS_SECURE_TRANSPORT_PRIVATE_H
#define MONGOC_STREAM_TLS_SECURE_TRANSPORT_PRIVATE_H

#if !defined (MONGOC_INSIDE) && !defined (MONGOC_COMPILATION)
# error "Only <mongoc.h> can be included directly."
#endif

#ifdef MONGOC_ENABLE_SECURE_TRANSPORT
#include <bson.h>

BSON_BEGIN_DECLS


/**
 * mongoc_stream_tls_secure_transport_t:
 *
 * Private storage for Secure Transport Streams
 */
typedef struct
{
} mongoc_stream_tls_secure_transport_t;


BSON_END_DECLS

#endif /* MONGOC_ENABLE_SECURE_TRANSPORT */
#endif /* MONGOC_STREAM_TLS_SECURE_TRANSPORT_PRIVATE_H */

