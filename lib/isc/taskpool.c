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


#include <isc/mem.h>
#include <isc/random.h>
#include <isc/taskpool.h>
#include <isc/util.h>

/***
 *** Types.
 ***/

struct isc_taskpool {
	isc_mem_t *			mctx;
	isc_taskmgr_t *			tmgr;
	unsigned int			ntasks;
	unsigned int			quantum;
	isc_task_t **			tasks;
};

/***
 *** Functions.
 ***/

static isc_result_t
alloc_pool(isc_taskmgr_t *tmgr, isc_mem_t *mctx, unsigned int ntasks,
	   unsigned int quantum, isc_taskpool_t **poolp)
{
	isc_taskpool_t *pool;
	unsigned int i;

	pool = isc_mem_get(mctx, sizeof(*pool));
	if (pool == NULL)
		return (ISC_R_NOMEMORY);

	pool->mctx = NULL;
	isc_mem_attach(mctx, &pool->mctx);
	pool->ntasks = ntasks;
	pool->quantum = quantum;
	pool->tmgr = tmgr;
	pool->tasks = isc_mem_get(mctx, ntasks * sizeof(isc_task_t *));
	if (pool->tasks == NULL) {
		isc_mem_putanddetach(&pool->mctx, pool, sizeof(*pool));
		return (ISC_R_NOMEMORY);
	}
	for (i = 0; i < ntasks; i++)
		pool->tasks[i] = NULL;

	*poolp = pool;
	return (ISC_R_SUCCESS);
}

isc_result_t
isc_taskpool_create(isc_taskmgr_t *tmgr, isc_mem_t *mctx,
		    unsigned int ntasks, unsigned int quantum,
		    isc_taskpool_t **poolp)
{
	unsigned int i;
	isc_taskpool_t *pool = NULL;
	isc_result_t result;

	INSIST(ntasks > 0);

	/* Allocate the pool structure */
	result = alloc_pool(tmgr, mctx, ntasks, quantum, &pool);
	if (result != ISC_R_SUCCESS)
		return (result);

	/* Create the tasks */
	for (i = 0; i < ntasks; i++) {
		result = isc_task_create(tmgr, quantum, &pool->tasks[i]);
		if (result != ISC_R_SUCCESS) {
			isc_taskpool_destroy(&pool);
			return (result);
		}
		isc_task_setname(pool->tasks[i], "taskpool", NULL);
	}

	*poolp = pool;
	return (ISC_R_SUCCESS);
}

void
isc_taskpool_gettask(isc_taskpool_t *pool, isc_task_t **targetp) {
	isc_task_attach(pool->tasks[isc_random_uniform(pool->ntasks)], targetp);
}

int
isc_taskpool_size(isc_taskpool_t *pool) {
	REQUIRE(pool != NULL);
	return (pool->ntasks);
}

isc_result_t
isc_taskpool_expand(isc_taskpool_t **sourcep, unsigned int size,
		    isc_taskpool_t **targetp)
{
	isc_result_t result;
	isc_taskpool_t *pool;

	REQUIRE(sourcep != NULL && *sourcep != NULL);
	REQUIRE(targetp != NULL && *targetp == NULL);

	pool = *sourcep;
	if (size > pool->ntasks) {
		isc_taskpool_t *newpool = NULL;
		unsigned int i;

		/* Allocate a new pool structure */
		result = alloc_pool(pool->tmgr, pool->mctx, size,
				    pool->quantum, &newpool);
		if (result != ISC_R_SUCCESS)
			return (result);

		/* Copy over the tasks from the old pool */
		for (i = 0; i < pool->ntasks; i++) {
			newpool->tasks[i] = pool->tasks[i];
			pool->tasks[i] = NULL;
		}

		/* Create new tasks */
		for (i = pool->ntasks; i < size; i++) {
			result = isc_task_create(pool->tmgr, pool->quantum,
						 &newpool->tasks[i]);
			if (result != ISC_R_SUCCESS) {
				isc_taskpool_destroy(&newpool);
				return (result);
			}
			isc_task_setname(newpool->tasks[i], "taskpool", NULL);
		}

		isc_taskpool_destroy(&pool);
		pool = newpool;
	}

	*sourcep = NULL;
	*targetp = pool;
	return (ISC_R_SUCCESS);
}

void
isc_taskpool_destroy(isc_taskpool_t **poolp) {
	unsigned int i;
	isc_taskpool_t *pool = *poolp;
	for (i = 0; i < pool->ntasks; i++) {
		if (pool->tasks[i] != NULL)
			isc_task_detach(&pool->tasks[i]);
	}
	isc_mem_put(pool->mctx, pool->tasks,
		    pool->ntasks * sizeof(isc_task_t *));
	isc_mem_putanddetach(&pool->mctx, pool, sizeof(*pool));
	*poolp = NULL;
}

void
isc_taskpool_setprivilege(isc_taskpool_t *pool, isc_boolean_t priv) {
	unsigned int i;

	REQUIRE(pool != NULL);

	for (i = 0; i < pool->ntasks; i++) {
		if (pool->tasks[i] != NULL)
			isc_task_setprivilege(pool->tasks[i], priv);
	}
}
