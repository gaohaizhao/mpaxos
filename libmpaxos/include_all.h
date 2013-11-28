/* 
 * File:   include_all.h
 * Author: shuai
 *
 * Created on November 11, 2013, 3:02 PM
 */

#ifndef INCLUDE_ALL_H
#define	INCLUDE_ALL_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#include <stdio.h>
#include <errno.h>
#include <memory.h>
#include <pthread.h>
#include <apr_hash.h>
#include <apr_thread_mutex.h>
#include <apr_atomic.h>

#include "comm.h"
#include "proposer.h"
#include "mpaxos/mpaxos.h"
#include "utils/logger.h"
#include "utils/mtime.h"
#include "utils/mpr_hash.h"
#include "utils/mpr_dag.h"
#include "rpc/rpc.h"
#include "log_helper.h"
#include "view.h"
#include "async.h"
#include "internal_types.h"
#include "slot_mgr.h"
#include "recorder.h"
#include "controller.h"


#ifdef	__cplusplus
}
#endif

#endif	/* INCLUDE_ALL_H */

