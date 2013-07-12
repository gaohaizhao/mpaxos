/**
 * put it out of use temporarily
 * 
 */

#include <apr_hash.h>
#include <pthread.h>
#include "internal_types.h"
#include "slot_mgr.h"
#include "utils/logger.h"
#include "comm.h"
#include "view.h"

apr_pool_t *slot_pool_;
apr_hash_t *slot_ht_;
pthread_mutex_t slot_mutex;

void slot_mgr_init() {
    apr_pool_create(&slot_pool_, NULL);
    slot_ht_ = apr_hash_make(slot_pool_);
    pthread_mutex_init(&slot_mutex, NULL);
}

void slot_mgr_destroy() {
    apr_pool_destroy(slot_pool_);
    pthread_mutex_destroy(&slot_mutex);
}

nodeid_t get_slot_mgr(groupid_t gid) {
    // who is the slot manager?
    // TODO
    nodeid_t mgr_nid = gid % (get_group_size(gid));
    if (mgr_nid == 0) {
        mgr_nid = get_group_size(gid);
    }
    
    //  assume the nid = gid is the master.
    //nodeid_t mgr_nid = gid;
    LOG_DEBUG("slot manager of gid %u is nid %u", gid, mgr_nid);
    return mgr_nid;
}

bool is_slot_mgr(groupid_t gid) {
    nodeid_t mgr_nid = get_slot_mgr(gid);   
    bool ret = (get_local_nid() == mgr_nid);
    if (ret) {
        LOG_DEBUG("i am the slot manger of group gid: %u.", gid);
    } else {
        LOG_DEBUG("i am not the slot manger of group gid: %u.", gid);
    }
    return ret; 
}

slotid_t alloc_slot(groupid_t gid, nodeid_t nid) {
    pthread_mutex_lock(&slot_mutex);
    slotid_t *sid = apr_hash_get(slot_ht_, &gid, sizeof(gid));
    if (sid == NULL) {
        LOG_DEBUG("first time alloc for group gid:%u", gid);
        // have not alloced any slot for this group.
        groupid_t *k = apr_palloc(slot_pool_, sizeof(gid));
        slotid_t *v = apr_palloc(slot_pool_, sizeof(slotid_t)); 
        *k = gid;
        // TODO [IMPROVE] slot starts from last
        *v = 0;
        apr_hash_set(slot_ht_, k, sizeof(groupid_t), v); 
        sid = v;
    }
    *sid = *sid + 1;
    pthread_mutex_unlock(&slot_mutex);
    LOG_DEBUG("alloc gid:%d sid:%d to nid:%d", gid, *sid, nid);
    return *sid;
}

slotid_t acquire_slot(groupid_t gid, nodeid_t nid) {
    return alloc_slot(gid, nid);
//    if (is_slot_mgr(gid)) {
//        return alloc_slot(gid, nid);
//    } else {
//        // generate a slot message
//        msg_slot_t msg_slot = MPAXOS__MSG_SLOT__INIT;
//        Mpaxos__MsgHeader header = MPAXOS__MSG_HEADER__INIT;
//        Mpaxos__ProcessidT pid = MPAXOS__PROCESSID_T__INIT;
//        msg_slot.h = &header;
//        msg_slot.h->pid = &pid;
//        msg_slot.h->t = MPAXOS__MSG_HEADER__MSGTYPE_T__SLOT;
//        msg_slot.h->pid->gid = gid;
//        msg_slot.h->pid->nid = get_local_nid();
//        
//        nodeid_t mgr_nid = get_slot_mgr(gid);
//        LOG_DEBUG("acquire slot sid from node nid %u, group gid %u", mgr_nid, gid);
//        // send it to remote server, and wait for response
//        size_t sz = mpaxos__msg_slot__get_packed_size(&msg_slot);
//        uint8_t *buf = (uint8_t *)malloc(sz);
//        mpaxos__msg_slot__pack(&msg_slot, buf);
//        slotid_t sid = send_to_slot_mgr(gid, mgr_nid, buf, sz);
//        LOG_DEBUG("slot sid acquired from node nid %u, group gid %u, slot sid %u", mgr_nid, gid, sid);
//        return sid;
//    }
}

