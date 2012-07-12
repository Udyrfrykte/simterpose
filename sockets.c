#include "sockets.h"
#include "insert_trace.h"
#include "xbt.h"
#include "task.h"
#include "sysdep.h"
#include "simdag/simdag.h"
#include "xbt.h"

#define LOCAL 1
#define REMOTE 2

#define TCP_PROTOCOL 0
#define UDP_PROTOCOL 1
#define RAW_PROTOCOL 2

xbt_dynar_t all_sockets;
int nb_sockets = 0;

int get_addr_port_sock(pid_t pid, int fd, int addr);
void print_infos_socket(struct infos_socket *is);

void init_socket_gestion()
{
  all_sockets = xbt_dynar_new(sizeof(struct infos_socket*), NULL);
}

recv_information* recv_information_new()
{
  recv_information* res = malloc(sizeof(recv_information));
  res->quantity_recv=0;
  res->send_fifo = xbt_fifo_new();
  res->recv_task = xbt_fifo_new();
  
  return res;
}

void recv_information_destroy(recv_information *recv)
{
  xbt_fifo_free(recv->send_fifo);
  xbt_fifo_free(recv->recv_task);
  free(recv);
}

struct infos_socket* confirm_register_socket(pid_t pid, int sockfd, int domain, int protocol) {

  process_descriptor* proc = global_data->process_desc[pid];
  
  struct infos_socket *is = malloc(sizeof(struct infos_socket));
  proc->fd_list[sockfd]=is;
  is->fd= sockfd;
  
  is->comm = NULL;
  is->proc=proc;
  is->domain=domain;
  is->protocol=protocol;
  is->ip_local=-1;
  is->port_local=0;

  xbt_dynar_push(all_sockets, &is);
  
  return is;
}

void delete_socket(pid_t pid, int fd) {
  xbt_ex_t e;
  TRY{
    int i= xbt_dynar_search(all_sockets, get_infos_socket(pid, fd));
    xbt_dynar_remove_at(all_sockets, i, NULL);
  }
  CATCH(e){
    printf("Socket not found\n");
  } 
}

void socket_close(pid_t pid, int fd)
{
  struct infos_socket* is = get_infos_socket(pid, fd);
  if(is!=NULL)
  {
    if(socket_network(pid, fd))
      comm_close(is);
    delete_socket(pid, fd);
    process_descriptor* proc = global_data->process_desc[pid];
    proc->fd_list[fd]=NULL;
  }
}

struct infos_socket* register_socket(pid_t pid, int sockfd, int domain, int protocol) {
//   printf("Registering socket %d for processus %d\n", sockfd, pid);
  if (socket_registered(pid,sockfd)==1) {
    xbt_die("Inconsistence found in model. Socket already exist");
  }else{
    return confirm_register_socket(pid,sockfd,domain,protocol);
  }

}


void update_socket(pid_t pid, int fd) {

  struct infos_socket* is = get_infos_socket(pid, fd);

  if (is->domain == 2) { // PF_INET
    get_addr_port_sock(pid, fd, LOCAL);
    print_infos_socket(is);
  }
}



void set_localaddr_port_socket(pid_t pid, int fd, char *ip, int port) {
  struct infos_socket* is = get_infos_socket(pid, fd);
  struct in_addr t;
  is->ip_local = inet_aton(ip, &t);
  is->port_local = port;
  print_infos_socket(is);
}



void get_localaddr_port_socket(pid_t pid, int fd) {
  struct infos_socket* is = get_infos_socket(pid, fd);

  if (is->domain == 2) { // PF_INET
    if (!get_addr_port_sock(pid, fd, 1))
      printf("Failed reading locale addr:port after bind\n");
    print_infos_socket(is);
  }
}


void print_infos_socket(struct infos_socket *is) {
  fprintf(stdout,"\n%5s %5s %10s %10s %21s\n","pid","fd","domain","protocol","locale_ip:port");
    if(is->proc != NULL){

  fprintf(stdout,"%5d %5d %10d %10d %15d:%5d\n",
	  is->proc->pid,
	  is->fd,
	  is->domain,
	  is->protocol,
	  is->ip_local,
	  is->port_local);
  }
}

#define INODE_OFFSET 91

int get_addr_port(int type, int num_sock, struct sockaddr_in *addr_port, int addr ) {

  FILE *file;
  
  
  //printf("Socket number : %d\n", num_sock);
  if (type == TCP_PROTOCOL) // TCP
    file=fopen("/proc/net/tcp","r");
  else if (type == UDP_PROTOCOL) // UDP
    file=fopen("/proc/net/udp","r");
  else // RAW
    file=fopen("/proc/net/raw","r");
  if (file == NULL) {
    perror("Open file /proc/net/...");
    exit(1);
  }
  char buff[512];
  char *loc_addr_hex;
  char *rem_addr_hex;
  char addrs[27];
  char part_inode[512 - INODE_OFFSET]; // size buff = 512 - all informations before inode
  char *inode;
  while (fgets(buff,sizeof(buff),file)) {
    strncpy(part_inode,buff+INODE_OFFSET,512-INODE_OFFSET);
    inode = strtok(part_inode," ");
    if (atoi(inode) == num_sock) {
      strncpy(addrs,buff+6,27);
      loc_addr_hex=strtok(addrs," ");
      rem_addr_hex=strtok(NULL," ");
      if (addr==LOCAL) // 1 -> locale
	sscanf(loc_addr_hex, "%X:%hX", &addr_port->sin_addr.s_addr, &addr_port->sin_port);
      else // remote
	sscanf(rem_addr_hex, "%X:%hX", &addr_port->sin_addr.s_addr, &addr_port->sin_port);
      fclose(file);
      return 1;
    }
  }
  fclose(file);
  return -1;

}

int socket_get_remote_addr(pid_t pid, int fd, struct sockaddr_in* addr_port)
{
  struct infos_socket* is = get_infos_socket(pid, fd);
  int protocol = is->protocol;
  
  char path[512];
  char dest[512];
  sprintf(path,"/proc/%d/fd/%d", pid, fd);
  if (readlink(path, dest, 512) == -1) {
    printf("Failed reading /proc/%d/fd/%d",pid,fd);
    return -1;
  }
  
  char *token;
  token = strtok(dest,"["); // left part before socket id
  token = strtok(NULL,"]"); // socket id 
  int num_socket = atoi(token);
  
  int res=0;
  
  if (protocol == 1) { // case IPPROTO_ICMP -> protocol unknown -> test TCP
    res = get_addr_port(TCP_PROTOCOL, num_socket, addr_port, REMOTE);
    if (res == -1) { // not tcp -> test UDP
      res = get_addr_port(UDP_PROTOCOL, num_socket, addr_port, REMOTE);
      if (res == -1) // not udp -> test RAW
        res = get_addr_port(RAW_PROTOCOL, num_socket, addr_port, REMOTE);
    }
  }
  
  if (protocol == 6 || protocol == 0) // case IPPROTO_TCP ou IPPROTO_IP
    res=get_addr_port(TCP_PROTOCOL, num_socket, addr_port, REMOTE);
  if (protocol == 17 ) // case IPPROTO_UDP 
    res=get_addr_port(UDP_PROTOCOL, num_socket, addr_port, REMOTE);
  if (protocol == 255 ) // case IPPROTO_RAW 
    res=get_addr_port(RAW_PROTOCOL, num_socket, addr_port, REMOTE);
  
  return res;
}


int get_addr_port_sock(pid_t pid, int fd, int addr_type) {

  struct infos_socket* is = get_infos_socket(pid, fd);
  int protocol = is->protocol;
  
  char path[512];
  char dest[512];
  sprintf(path,"/proc/%d/fd/%d", pid, fd);
  if (readlink(path, dest, 512) == -1) {
    printf("Failed reading /proc/%d/fd/%d",pid,fd);
    return -1;
  }

  char *token;
  token = strtok(dest,"["); // left part before socket id
  token = strtok(NULL,"]"); // socket id 
  int num_socket = atoi(token);
  
  struct sockaddr_in addr_port;
  
  int res=0;

  if (protocol == 1) { // case IPPROTO_ICMP -> protocol unknown -> test TCP
    res = get_addr_port(TCP_PROTOCOL, num_socket, &addr_port, LOCAL);
    if (res == -1) { // not tcp -> test UDP
      res = get_addr_port(UDP_PROTOCOL, num_socket, &addr_port, LOCAL);
      if (res == -1) // not udp -> test RAW
	res = get_addr_port(RAW_PROTOCOL, num_socket, &addr_port, LOCAL);
    }
  }

  if (protocol == 6 || protocol == 0) // case IPPROTO_TCP ou IPPROTO_IP
    res=get_addr_port(TCP_PROTOCOL, num_socket, &addr_port, LOCAL);
  if (protocol == 17 ) // case IPPROTO_UDP 
    res=get_addr_port(UDP_PROTOCOL, num_socket, &addr_port, LOCAL);
  if (protocol == 255 ) // case IPPROTO_RAW 
    res=get_addr_port(RAW_PROTOCOL, num_socket, &addr_port, LOCAL);
      

  if (res==1) {
    is->ip_local= addr_port.sin_addr.s_addr;
    is->port_local = addr_port.sin_port;
  }

  return res;
  
}


int get_domain_socket(pid_t pid, int fd) {
  struct infos_socket* is = get_infos_socket(pid, fd);
  
  if (is != NULL )
    return is->domain;
  return -1;
}

int socket_registered(pid_t pid, int fd) {
  struct infos_socket* is = get_infos_socket(pid, fd);
  
  if(is != NULL)
      return 1;
  return -1;
}


struct infos_socket* get_infos_socket(pid_t pid, int fd) {
  //printf("Info socket %d %d\n", pid, fd);
  return global_data->process_desc[pid]->fd_list[fd];
}


int get_protocol_socket(pid_t pid, int fd) {
  struct infos_socket* is = get_infos_socket(pid, fd);
  
  if (is != NULL )
    return is->protocol;
  return -1;
}

int socket_netlink(pid_t pid, int fd) {
  struct infos_socket* is = get_infos_socket(pid, fd);
  
  if (is != NULL )
  {
    printf("Socket %d of %d : domain %d\n", fd, pid, is->domain);
    return is->domain == 16;
  }
  return 0;
}

int socket_network(pid_t pid, int fd)
{
  struct infos_socket* is = get_infos_socket(pid, fd);
  if (is != NULL )
  {
    printf("Socket %d of %d : domain %d\n", fd, pid, is->domain);
    return is->domain != 16 && is->domain != 2;
  }
  return 0;
}


struct infos_socket* getSocketInfoFromContext(unsigned int ip_local, int port_local)
{
  struct infos_socket* temp_is;
  unsigned int cpt=0;
  
  xbt_dynar_foreach(all_sockets, cpt, temp_is){
    print_infos_socket(temp_is);
    if ((temp_is->ip_local == ip_local) && (temp_is->port_local==port_local))
    {
      return temp_is;
    }
  }
  return NULL;
}

//FIXME what happen when there's to sending akwnowledgement in the same turn
//maybe use an global ghost task which have every recv task in dependencies and use last_computation task mecanism
int handle_communication_stat(struct infos_socket* is, pid_t pid)
{
  int result=0;
  recv_information* recv = comm_get_own_recv(is);
  int *size = (int*)xbt_fifo_shift(recv->send_fifo);
  if(size==NULL)
    return 0;
  
  //if size == -1 , that mean that we start a new recv and we have to make a new task
  if(*size == -1)
  {
//     insert_trace_comm(is->proc->pid, is->fd, "recv", 0);
    task_schedule_receive(is, pid);
    free(size);
    result = 1;
    size = (int*)xbt_fifo_shift(recv->send_fifo);
  }
  
  if(recv->quantity_recv < *size)
  {
    xbt_fifo_unshift(recv->send_fifo, size);
  }
  else
  {
    recv->quantity_recv -= *size;
    free(size);
    //til we acknowledge sending, we continue
    if(recv->quantity_recv > 0)
    {
      result = 1;
      handle_communication_stat(is, pid);
    }
  }
  
  return result;
}


int handle_new_receive(int pid, int sockfd, int length)
{
  struct infos_socket* is = get_infos_socket(pid, sockfd);

  recv_information* recv = comm_get_own_recv(is);
  recv->quantity_recv += length;
  
  return handle_communication_stat(is, pid);
}


//TODO simplify handling 
void handle_new_send(struct infos_socket *is,  int length)
{
  recv_information* recv = comm_get_peer_recv(is);
  int *size = malloc(sizeof(int));
  *size=length;
  
  int *buf = malloc(sizeof(int));
  *buf=-1;

  xbt_fifo_push(recv->send_fifo, buf);
  xbt_fifo_push(recv->send_fifo, size);
}


int close_all_communication(int pid){
  process_descriptor* proc = global_data->process_desc[pid];
  int i=0;
  int result=0;
  for(i=0; i<MAX_FD ; ++i)
  {
    if (proc->fd_list[i]!=NULL)
    {
      recv_information* recv = comm_get_own_recv(proc->fd_list[i]);
      
      xbt_fifo_t sl = recv->send_fifo;
      int *size;
      while(xbt_fifo_size(sl))
      {
        size = (int*)xbt_fifo_shift(sl);
        free(size);
      }
      
      xbt_fifo_t tl = recv->recv_task;
      task_comm_info* tci;
      while(xbt_fifo_size(tl))
      {
        tci = (task_comm_info*) xbt_fifo_shift(tl);
        SD_task_destroy(tci->task);
        free(tci);
      }
      socket_close(pid, i);
      proc->fd_list[i]=NULL;
    }
  }
  return result;
}

int socket_read_event(pid_t pid, int fd)
{
  struct infos_socket* is = get_infos_socket(pid, fd);
  int res = comm_get_socket_state(is);
  
  return res;
}

int socket_get_state(struct infos_socket* is)
{
  return comm_get_socket_state(is);
}

