#ifndef DATA_UTILS_MSG_H
#define DATA_UTILS_MSG_H

#include <sys/types.h>
#include "simterpose_msg.h"
#include "sockets_msg.h"

#define MAX_PID 32768

void simterpose_globals_init(float msec_per_flop);
void simterpose_globals_exit(void);

int get_nb_peek(void);

int get_nb_poke(void);

int get_nb_getregs(void);

int get_nb_setregs(void);

int get_nb_syscall(void);

int get_nb_setoptions(void);

int get_nb_detach(void);

int get_nb_geteventmsg(void);

void increment_nb_peek(void);

void increment_nb_poke(void);

void increment_nb_getregs(void);

void increment_nb_setregs(void);

void increment_nb_syscall(void);

void increment_nb_setoptions(void);

void increment_nb_detach(void);

void increment_nb_geteventmsg(void);

double simterpose_get_msec_per_flop(void);

xbt_dict_t simterpose_get_host_list(void);
xbt_dict_t simterpose_get_ip_list(void);

double update_simulation_clock(void);

void destroy_simterpose_host(void *host);

int is_port_in_use(msg_host_t host, int port);

void register_port(msg_host_t host, int port);

int get_port_option(msg_host_t host, int port);

void set_port_option(msg_host_t host, int port, int option);

void set_port_on_binding(msg_host_t host, int port, struct infos_socket *is, int device);

struct infos_socket *get_binding_socket(unsigned int ip, int port, int nature);

struct infos_socket *get_binding_socket_host(msg_host_t host, int port, int device);

unsigned int get_ip_of_host(msg_host_t host);

msg_host_t get_host_by_ip(unsigned int ip);

int get_random_port(msg_host_t host);

void unset_socket(pid_t pid, struct infos_socket *is);

time_t get_simulated_timestamp(void);

void set_real_port(msg_host_t host, int port, int real_port);

void add_new_translation(int real_port, int translated_port, unsigned int translated_ip);

translate_desc_t *get_translation(int real_port);

int get_real_port(process_descriptor_t *proc, unsigned int ip, int port);

void benchmark_matrix_product(float *msec_per_flop);

void init_host_list(void);

#endif                          /* DATA_UTILS_MSG_H */