#include "run_trace.h"
#include "data_utils.h"
#include "process_descriptor.h"
#include "xbt.h"
#include "sockets.h"
#include "simdag/simdag.h" /* For SD_get_clock() */

void init_global_data()
{
  global_data->child_amount = 0;
  global_data->flops_per_second = 0.0;
  global_data->micro_s_per_flop = 0.0;
  global_data->launching_time = xbt_dynar_new(sizeof(time_desc*), NULL);
  global_data->list_station = xbt_dict_new_homogeneous(&destroy_simterpose_station);
  global_data->list_ip = xbt_dict_new_homogeneous(&free);
  global_data->init_time = time(NULL);
  
  int i;
  for(i=0; i<MAX_PID; ++i)
  {
    global_data->process_desc[i]=NULL;
  }
}

void destroy_global_data()
{
  xbt_dynar_free(&(global_data->launching_time));
  xbt_dict_free(&(global_data->list_station));
  xbt_dict_free(&(global_data->list_ip));
  free(global_data);
}

void destroy_simterpose_station(void *data)
{
  simterpose_station* station = (simterpose_station*)data;
  xbt_dict_free(&(station->port));
  free(station);
}

double get_next_start_time()
{
  if(xbt_dynar_is_empty(global_data->launching_time))
    return -1;
  
  time_desc** t = (time_desc**)xbt_dynar_get_ptr(global_data->launching_time, 0);
//   printf("Next start_time %lf\n", (*t)->start_time);
  return (*t)->start_time;
}

pid_t pop_next_pid()
{
  time_desc* t = NULL;
  xbt_dynar_shift(global_data->launching_time, &t);
  int res = t->pid;
  
  process_descriptor* proc = process_get_descriptor(res);
  if(proc->in_timeout == PROC_IN_TIMEOUT)
    proc->in_timeout = PROC_TIMEOUT_EXPIRE;
  proc->timeout = NULL;
  
  free(t);
  return res;
}

void add_launching_time(pid_t pid, double start_time)
{
  time_desc* t = malloc(sizeof(time_desc));
  t->pid = pid;
  t->start_time = start_time;
  
  process_descriptor* proc = process_get_descriptor(pid);
  proc->timeout = t;
  
  xbt_dynar_push(global_data->launching_time, &t);
}

void set_next_launchment(pid_t pid)
{
  time_desc* t = malloc(sizeof(time_desc));
  t->pid = pid;
  t->start_time = SD_get_clock();
  
  process_descriptor* proc = process_get_descriptor(pid);
  proc->timeout = t;
  
  xbt_dynar_unshift(global_data->launching_time, &t);
}

int has_sleeping_to_launch()
{
  return !xbt_dynar_is_empty(global_data->launching_time);
}

void add_timeout(pid_t pid, double start_time)
{
  
  if(start_time == SD_get_clock())
    start_time += 0.0001;
//   printf("Add new timeout of %lf for %d\n", start_time, pid);
  time_desc* t = malloc(sizeof(time_desc));
  t->pid = pid;
  t->start_time = start_time;
  
  process_descriptor* proc = process_get_descriptor(pid);
  proc->timeout = t;
  proc->in_timeout = PROC_IN_TIMEOUT;
  
  int i=0;
  while( i < xbt_dynar_length(global_data->launching_time))
  {
    time_desc** t = xbt_dynar_get_ptr(global_data->launching_time, i);
    if( start_time < (*t)->start_time)
      break;
    ++i;
  }
  xbt_dynar_insert_at(global_data->launching_time, i, &t);
}

void remove_timeout(pid_t pid)
{
  process_descriptor* proc = process_get_descriptor(pid);
  time_desc* t = proc->timeout;
  proc->timeout = NULL;
  proc->in_timeout = PROC_NO_TIMEOUT;
  
  xbt_ex_t e;
  TRY{
    int i= xbt_dynar_search(global_data->launching_time, &t);
    xbt_dynar_remove_at(global_data->launching_time, i, NULL);
  }
  CATCH(e){
    printf("Timeout not found %d\n", xbt_dynar_is_empty(global_data->launching_time));
  }
  free(t);
}

int is_port_in_use(SD_workstation_t station, int port)
{
  simterpose_station *temp = (simterpose_station*)xbt_dict_get(global_data->list_station, SD_workstation_get_name(station));
  char buff[6];
  sprintf(buff, "%d", port);
  return (xbt_dict_get_or_null(temp->port, buff) != NULL);
}

void register_port(SD_workstation_t station, int port)
{
  //try to see if port isn't already use.
  simterpose_station *station_desc = (simterpose_station*)xbt_dict_get(global_data->list_station, SD_workstation_get_name(station));
  char buff[6];
  sprintf(buff, "%d", port);
  port_desc *temp = NULL;
  
  
  if((temp = (port_desc*)xbt_dict_get_or_null(station_desc->port, buff)))
  {
    ++(temp->amount_socket);
  }
  else
  {
    temp = malloc(sizeof(port_desc));
    temp->port_num = port;
    temp->option=0;
    temp->amount_socket=1;
    temp->bind_socket=NULL;
    
    xbt_dict_set(station_desc->port, buff, temp, NULL);
  }
  
  
}

int get_port_option(SD_workstation_t station, int port)
{
  simterpose_station *temp = (simterpose_station*)xbt_dict_get(global_data->list_station, SD_workstation_get_name(station));
  char buff[6];
  sprintf(buff, "%d", port);
  
  port_desc* desc = xbt_dict_get_or_null(temp->port, buff);
  if(!desc)
    return 0;
  else
    return desc->option;
}

void set_port_option(SD_workstation_t station, int port, int option)
{
  simterpose_station *temp = (simterpose_station*)xbt_dict_get(global_data->list_station, SD_workstation_get_name(station));
  char buff[6];
  sprintf(buff, "%d", port);
  
  port_desc* desc = xbt_dict_get_or_null(temp->port, buff);
  if(desc)
    desc->option = option;
}

void set_port_on_binding(SD_workstation_t station, int port, struct infos_socket* is, int device)
{
  simterpose_station *temp = (simterpose_station*)xbt_dict_get(global_data->list_station, SD_workstation_get_name(station));
  char buff[6];
  sprintf(buff, "%d", port);
  
  port_desc* desc = xbt_dict_get_or_null(temp->port, buff);
  if(!desc)
    return;
  desc->option = desc->option | PORT_BIND | device;
  desc->bind_socket = is;
}

struct infos_socket *get_binding_socket(unsigned int ip, int port, int nature)
{
  struct in_addr in = {ip};
  char *ip_dot = inet_ntoa(in);
  
  char* station_name = xbt_dict_get(global_data->list_ip, ip_dot);
  simterpose_station *temp = (simterpose_station*)xbt_dict_get(global_data->list_station, station_name);
  char buff[6];
  sprintf(buff, "%d", port);
  port_desc* desc = xbt_dict_get_or_null(temp->port, buff);
  
  
  if(desc == NULL || !(desc->option & PORT_BIND))
    return NULL;
  
  if(!(nature & desc->option))
    return NULL;
  
  return desc->bind_socket;
}

struct infos_socket *get_binding_socket_workstation(SD_workstation_t station , int port, int device)
{
  simterpose_station *temp = (simterpose_station*)xbt_dict_get(global_data->list_station, SD_workstation_get_name(station));
  char buff[6];
  sprintf(buff, "%d", port);
  port_desc* desc = xbt_dict_get_or_null(temp->port, buff);
  
  if(desc == NULL || !(desc->option & PORT_BIND))
    return NULL;
  
  if(!(device & desc->option))
    return NULL;
  
  return desc->bind_socket;
}

unsigned int get_ip_of_station(SD_workstation_t station)
{
  simterpose_station *temp = (simterpose_station*)xbt_dict_get(global_data->list_station, SD_workstation_get_name(station));
  return temp->ip;
}

SD_workstation_t get_station_by_ip(unsigned int ip)
{
  struct in_addr in = {ip};
  char *name = xbt_dict_get_or_null(global_data->list_ip, inet_ntoa(in));
  if(!name)
    return NULL;
  
  return SD_workstation_get_by_name(name);
}

int get_random_port(SD_workstation_t station)
{
  simterpose_station *temp = (simterpose_station*)xbt_dict_get(global_data->list_station, SD_workstation_get_name(station));
  unsigned short port = 0;
  --port;
  
  while(1)
  {
    char buff[6];
    sprintf(buff, "%d", port);
    if(xbt_dict_get_or_null(temp->port, buff))
      --port;
    else
      break;
  }
  
  return port;
}

void unset_socket(pid_t pid, struct infos_socket* is)
{
  process_descriptor *proc = process_get_descriptor(pid);
  SD_workstation_t station = proc->station;
  simterpose_station *temp = (simterpose_station*)xbt_dict_get(global_data->list_station, SD_workstation_get_name(station));
  
  char buff[6];
  sprintf(buff, "%d", is->port_local);
  
  port_desc* desc = xbt_dict_get_or_null(temp->port, buff);
  if(!desc)
    return;
  
  if(is == desc->bind_socket)
    desc->bind_socket = NULL;
  
  --desc->amount_socket;
  if(desc->amount_socket)
    return;
  
  //if this is the last socket to use te port, we have to remove it from dict
  xbt_dict_remove(temp->port, buff);
}


time_t get_simulated_timestamp()
{
  return global_data->init_time + SD_get_clock();
}
