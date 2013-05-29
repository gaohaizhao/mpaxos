
#ifndef __MPAXOS_CONFIG_H__
#define __MPAXOS_CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  return 0 for success, negative for error
 */
int mpaxos_load_config(char * config_file_path);

#ifdef __cplusplus
}
#endif

#endif  // __MPAXOS_CONFIG__

