

void detach_txn_info(txn_info_t *tinfo);

txn_info_t* get_txn_info(txnid_t tid);

int mpaxos_start_request(mpaxos_req_t *req);

void controller_init();

void controller_destroy();

void group_info_create(group_info_t **g, txn_info_t *tinfo, roundid_t *rid);

void group_info_destroy(group_info_t *g);
