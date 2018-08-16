/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * See the COPYRIGHT file distributed with this work for additional
 * information regarding copyright ownership.
 */

/*! \file */

#include <config.h>

#include <stdbool.h>
#include <stddef.h>

#include <isc/hash.h>
#include <isc/mem.h>
#include <isc/msgcat.h>
#include <isc/mutex.h>
#include <isc/once.h>
#include <isc/refcount.h>
#include <isc/util.h>

#include <dns/db.h>
#include <dns/ecdb.h>
#include <dns/lib.h>
#include <dns/result.h>

#include <dst/dst.h>


/***
 *** Globals
 ***/

LIBDNS_EXTERNAL_DATA unsigned int			dns_pps = 0U;
LIBDNS_EXTERNAL_DATA isc_msgcat_t *			dns_msgcat = NULL;


/***
 *** Private
 ***/

static isc_once_t		msgcat_once = ISC_ONCE_INIT;


/***
 *** Functions
 ***/

static void
open_msgcat(void) {
	isc_msgcat_open("libdns.cat", &dns_msgcat);
}

void
dns_lib_initmsgcat(void) {

	/*
	 * Initialize the DNS library's message catalog, dns_msgcat, if it
	 * has not already been initialized.
	 */

	RUNTIME_CHECK(isc_once_do(&msgcat_once, open_msgcat) == ISC_R_SUCCESS);
}

static isc_once_t init_once = ISC_ONCE_INIT;
static isc_mem_t *dns_g_mctx = NULL;
static dns_dbimplementation_t *dbimp = NULL;
static bool initialize_done = false;
static isc_mutex_t reflock;
static isc_refcount_t references;

static void
initialize(void) {
	isc_result_t result;

	REQUIRE(initialize_done == false);

	result = isc_mem_create(0, 0, &dns_g_mctx);
	if (result != ISC_R_SUCCESS)
		return;
	dns_result_register();
	result = dns_ecdb_register(dns_g_mctx, &dbimp);
	if (result != ISC_R_SUCCESS)
		goto cleanup_mctx;

	result = dst_lib_init(dns_g_mctx, NULL);
	if (result != ISC_R_SUCCESS)
		goto cleanup_db;

	result = isc_mutex_init(&reflock);
	if (result != ISC_R_SUCCESS)
		goto cleanup_dst;

	isc_refcount_init(&references, 0);

	initialize_done = true;
	return;

  cleanup_dst:
	dst_lib_destroy();
  cleanup_db:
	if (dbimp != NULL)
		dns_ecdb_unregister(&dbimp);
  cleanup_mctx:
	if (dns_g_mctx != NULL)
		isc_mem_detach(&dns_g_mctx);
}

isc_result_t
dns_lib_init(void) {
	isc_result_t result;

	/*
	 * Since this routine is expected to be used by a normal application,
	 * it should be better to return an error, instead of an emergency
	 * abort, on any failure.
	 */
	result = isc_once_do(&init_once, initialize);
	if (result != ISC_R_SUCCESS) {
		return (result);
	}

	if (!initialize_done) {
		return (ISC_R_FAILURE);
	}

	isc_refcount_increment0(&references, NULL);

	return (ISC_R_SUCCESS);
}

void
dns_lib_shutdown(void) {
	int_fast32_t refs;

	isc_refcount_decrement(&references, &refs);
	if (refs > 0) {
		return;
	}
	isc_refcount_destroy(&references);

	dst_lib_destroy();

	if (dbimp != NULL) {
		dns_ecdb_unregister(&dbimp);
	}
	if (dns_g_mctx != NULL) {
		isc_mem_detach(&dns_g_mctx);
	}
}
