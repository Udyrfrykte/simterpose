#include <unistd.h>

#include "args_trace.h"
#include "peek_data.h"
#include "sysdep.h"
#include "calc_times_proc.h"
#include "times_proc.h"
#include "sockets.h"
#include "insert_trace.h"
#include "syscalls_io.h"
#include "run_trace.h"
#include "benchmark.h"
#include "syscall_process.h"
#include "xbt/fifo.h"
#include "replay.h"
#include "data_utils.h"

#define BUFFER_SIZE 512


void usage(char* progName) {
  printf("usage : %s platform_file.xml deployment_file.xml [-fp flops_power]\n", progName);
}

void print_trace_header(FILE* trace)
{
  fprintf(trace,"%8s %12s %8s %10s %10s %21s %21s\n","pidX", "syscall", "pidY", "return","diff_cpu","local_addr:port", "remote_addr:port");
}


int main(int argc, char *argv[]) { 
  
  global_data = malloc(sizeof(simterpose_data_t));

  int status;
  int stoppedpid;
  global_data->launcherpid=0;
  global_data->child_amount=0;
  global_data->time_to_next= xbt_fifo_new();
  global_data->last_clock=0;
  global_data->process_launch=0;
  global_data->idle_amount=0;
  int manual_flop =0;

  int sockfd; 

  int i;

  for(i=0; i<MAX_PID; ++i)
  {
    global_data->process_desc[i]=NULL;
  }
  
  //TODO mettre un vrai gestionnaire d'option et gérer les extensions des fichiers passés en paramètre
  if(argc>1)
  {
    for(i=1; i<argc; ++i)
    {
      if(!strcmp(argv[i], "-fp"))
      {
	if(argv[i+1] == NULL)
	{
	  usage(argv[0]); 
	}
	else
	{
	  char* endptr = argv[i+1]+strlen(argv[i+1])-1;
	  global_data->flops_per_second = strtod(argv[i+1], &endptr);
	  if(endptr == argv[i+1])
	    usage(argv[0]);
	  else
	  {
	    global_data->micro_s_per_flop  = 1000000/global_data->flops_per_second;
	    manual_flop = 1;
	  }
	}
      }
    }
  }
  
  if(!manual_flop)
    benchmark_matrix_product(&(global_data->flops_per_second), &(global_data->micro_s_per_flop));
  
  
  init_replay (argc, argv);
  
  struct user_regs_struct regs;
  

  char ret_trace[SIZE_PARAM_TRACE];

  unsigned long reg_orig;
  unsigned long ret;
  unsigned long arg1;
  unsigned long arg2;
  unsigned long arg3;
  
  int comm_launcher[2];
  pipe(comm_launcher);

  init_socket_gestion();
  global_data->launcherpid = fork();
  
  if (global_data->launcherpid == 0) {
    
    close(comm_launcher[0]);
    dup2(comm_launcher[1],3);
    close(comm_launcher[1]);
    if (ptrace(PTRACE_TRACEME, 0, NULL, NULL)==-1) {
      perror("ptrace traceme");
      exit(1);
    }
    if (execl("launcher", "launcher", argv[2], NULL)==-1) {
      perror("execl 1");
      exit(1);
    }
  
  } else {
    
    
    close(comm_launcher[1]);
    global_data->launcher_com = comm_launcher[0];
    
    //We enter name of processus in array
    printf("launcher pid %d\n", global_data->launcherpid);
    global_data->process_desc[global_data->launcherpid]= process_descriptor_new("launcher", global_data->launcherpid);
    
    // We wait for the child to be blocked by ptrace in the first exec()
    wait(&status);
    
    if (ptrace(PTRACE_SETOPTIONS,global_data->launcherpid,NULL,PTRACE_O_TRACECLONE | PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK | PTRACE_O_TRACEVFORKDONE)==-1) {
      perror("Error setoptions 1");
      exit(1);
    }

    ++global_data->child_amount;
    ++global_data->not_assigned;
    // Resume the child
    if (ptrace(PTRACE_SYSCALL, global_data->launcherpid, 0, 0)==-1) {
      perror("ptrace syscall 1");
      exit(1);
    }

   

    if (init_cputime()) {
      perror("Error init cputime");
      exit(1);
    }
    
	  
      while(global_data->child_amount) {
	
	//The first loop consist on advance each processus until found task for each
	//TODO metttre un place un sstème pour ne pas casser la boucle si les seuls fils restant sont idle.
	while(global_data->not_assigned)
	{
	  printf("New tour : left %d not assigned\n", global_data->not_assigned);
	  int task_found = 0;
	  // __WALL to follow all children
	  //TODO parcour de tous les pid dans l'ordre en traitant l'appel système s'il y en a ou en passant à un autre sinon option WNOHANG
	  stoppedpid = waitpid(-1, &status, __WALL);

	  
	  if (WIFEXITED(status)) {
	    printf("[%d] Child is dead\n",stoppedpid);
	    if (stoppedpid != global_data->launcherpid)
	      global_data->launcherpid=0;
	    --global_data->child_amount;
	    --global_data->not_assigned;
	    printf("Left %d child\n", global_data->child_amount);
	    continue;
	  }
	  
	  
	  int stat16=status >> 16;

	  if (stat16== PTRACE_EVENT_FORK || stat16 == PTRACE_EVENT_VFORK || stat16== PTRACE_EVENT_CLONE) {
	    if(!global_data->process_launch)
	    {
	      process_fork_call(stoppedpid);
	      global_data->process_launch = global_data->last_pid_create;
	      ++global_data->not_assigned;
	    }
	    else
	    {
	      task_found = process_fork_call(stoppedpid);
	      if(task_found)
		++global_data->not_assigned;
	    }
	  } 
	  
	  else if(stoppedpid != global_data->launcherpid){
	    
	    /*If this is the interrupt of the syscall and not the return, we print computation time */
	    if (in_syscall(stoppedpid)==0) {
	      set_in_syscall(stoppedpid);
	      if (ptrace(PTRACE_GETREGS, stoppedpid,NULL, &regs)==-1) {
		perror("ptrace getregs");
		exit(1);
	      }
	      /* ---- test archi for registers ---- */
	      
	      #if defined(__x86_64) || defined(amd64)
	      reg_orig=regs.orig_rax;
	      ret=regs.rax;
	      arg1=regs.rdi;
	      arg2=regs.rsi;
	      arg3=regs.rdx;
	      #elif defined(i386)
	      reg_orig=regs.orig_eax;
	      ret=regs.eax;
	      arg1=regs.ebx;
	      arg2=regs.ecx;
	      arg3=regs.edx;
	      #endif
	      
	      if(reg_orig == SYS_accept)
	      {
		printf("[%d] accept_in( ");
		--global_data->not_assigned;
		process_descriptor_set_idle(stoppedpid, 1);
	      }
	      
	      if(reg_orig == SYS_recvfrom)
	      {
		printf("[%d] recvfrom_in",stoppedpid);
		sockfd=get_args_sendto_recvfrom(stoppedpid,2,ret_trace,&regs);
		process_descriptor_set_idle(stoppedpid, 1);
		if(!is_communication_received(stoppedpid, sockfd))
		{
		  task_found=1;
		  socket_wait_for_sending(stoppedpid, sockfd);
		}
		else
		  --global_data->not_assigned;
	      }
	    }
	    else
	    {
	      //if the process was in idle state, we need to increment not_assigned
	      
	      
	      if (ptrace(PTRACE_GETREGS, stoppedpid,NULL, &regs)==-1) {
		perror("ptrace getregs");
		exit(1);
	      }
	      /* ---- test archi for registers ---- */

	      #if defined(__x86_64) || defined(amd64)
		reg_orig=regs.orig_rax;
		ret=regs.rax;
		arg1=regs.rdi;
		arg2=regs.rsi;
		arg3=regs.rdx;
	      #elif defined(i386)
		reg_orig=regs.orig_eax;
		ret=regs.eax;
		arg1=regs.ebx;
		arg2=regs.ecx;
		arg3=regs.edx;
	      #endif

	      /*------------------*/

	      switch (reg_orig) {
	      case SYS_write:
		printf("[%d] write(%ld, ... , %d) = %ld\n",stoppedpid,arg1,(int)arg3, ret);
		if (socket_registered(stoppedpid,arg1) != -1) {
		  if ((int)ret>0 && socket_incomplete(stoppedpid,arg1)) 
		    update_socket(stoppedpid,(int)arg1);
		  insert_trace_comm(stoppedpid,(int)arg1,"write",(int)ret);
		}
		break;

	      case SYS_read:
		printf("[%d] read(%ld, ..., %ld) = %ld\n",stoppedpid, arg1,arg3, ret);
		if (socket_registered(stoppedpid,arg1) != -1) {
		  if ((int)ret>0 && socket_incomplete(stoppedpid,arg1)) 
		    update_socket(stoppedpid,(int)arg1);
		  insert_trace_comm(stoppedpid,(int)arg1,"read",(int)ret);
		}
		break;

	      case SYS_fork: 
		printf("[%d] fork = %ld\n", stoppedpid,ret);
		break;
	      
	      case SYS_poll:
		get_args_poll(stoppedpid,(void *)arg1, (nfds_t)arg2);
		printf(" = %d \n",(int)ret);
		break;

	      case SYS_open:
	      {
		char *flags = malloc(9);
		switch (arg2) {
		case 0: strcpy(flags,"O_RDONLY"); break;
		case 1: strcpy(flags,"O_WRONLY"); break;
		case 2: strcpy(flags,"O_RDWR"); break;
		}
		if (strlen(flags)>0)
		  printf("[%d] open(\"...\", %s) = %ld\n",stoppedpid,flags, ret);
		else
		  printf("[%d] open(\"...\", no_flags) = %ld\n",stoppedpid, ret);
	      }
	      break;

	      case SYS_clone:
		printf("[%d] clone() ?= %ld\n",stoppedpid,ret);
		break;

	      case SYS_close: 
		printf("[%d] close(%ld) = %ld\n",stoppedpid,arg1,ret);
		close_sockfd(stoppedpid,(int)arg1);
		break;

	      case SYS_dup:
		printf("[%d] dup(%ld) = %ld\n",stoppedpid,arg1,ret);
		break;

	      case SYS_dup2:
		printf("[%d] dup2(%ld, %ld) = %ld\n",stoppedpid,arg1,arg2,ret);
		break;

	      case SYS_exit_group:
		printf("[%d] exit_group(%ld) called \n",stoppedpid,arg1);
		insert_trace_fork_exit(stoppedpid,"exit_group",(int)arg1);
		break;

	      case SYS_exit:
		printf("[%d] exit(%ld) called \n",stoppedpid,arg1);
		insert_trace_fork_exit(stoppedpid,"exit",(int)arg1);
		break;

	      case SYS_execve:
		printf("[%d] execve called\n",stoppedpid);
		process_descriptor* proc = process_descriptor_get(stoppedpid);
		if(global_data->process_launch != stoppedpid)
		{
		  printf("%p %d \n", proc,proc->execve_call_before_start);
		  if(!proc->execve_call_before_start)
		    task_found=1;
		  else
		    --proc->execve_call_before_start;
		}
		else
		{
		  if(!proc->execve_call_before_start)
		    global_data->process_launch = 1;//Normally, we have no chance to see an exec of init
		  else
		    --proc->execve_call_before_start;
		}
		break;
		
		
      #if defined(__x86_64)  

	      case SYS_select: 
		get_args_select(stoppedpid,&regs);
		break;

	      case SYS_socket: 
		printf("[%d] socket( ",stoppedpid);
		get_args_socket(stoppedpid,(int)ret, &regs);
		printf(" ) = %ld\n",ret);
		break;

	      case SYS_bind:
		printf("[%d] bind( ",stoppedpid);
		get_args_bind_connect(stoppedpid,(int)ret,0,&regs);
		printf(" ) = %ld\n",ret);
		break;

	      case SYS_connect:
		printf("[%d] connect( ",stoppedpid);
		get_args_bind_connect(stoppedpid,(int)ret,1,&regs);
		printf(" ) = %ld\n",ret);
		break;

	      case SYS_accept:
		printf("[%d] accept( ",stoppedpid);
		get_args_accept(stoppedpid,(int)ret,&regs);
		++global_data->not_assigned;
		process_descriptor_set_idle(stoppedpid, 0);
		printf(" ) = %ld\n",ret);
		break;

	      case SYS_listen:
		printf("[%d] listen( ", stoppedpid); 
		get_args_listen(stoppedpid,&regs);
		printf(" ) = %ld\n", ret);
		break;

	      case SYS_sendto:
		printf("[%d] sendto( ",stoppedpid);
		sockfd=get_args_sendto_recvfrom(stoppedpid,1,ret_trace,&regs);
		printf(" ) = %ld\n",ret);
		task_found = process_send_call(stoppedpid,sockfd,(int)ret);   
		break;

	      case SYS_recvfrom:
		printf("[%d] recvfrom( ",stoppedpid);
		sockfd=get_args_sendto_recvfrom(stoppedpid,2,ret_trace,&regs);
		if(process_descriptor_get_idle(stoppedpid))
		  ++global_data->not_assigned;
		process_descriptor_set_idle(stoppedpid, 0);
		printf(" ) = %ld\n",ret);
		task_found = process_recv_call(stoppedpid,sockfd,(int)ret);
		break;
	      
	      case SYS_sendmsg:
		printf("[%d] sendmsg( ",stoppedpid);
		sockfd=get_args_send_recvmsg(stoppedpid,1,ret_trace,&regs);
		printf(" ) = %ld\n",ret); 
		task_found = process_send_call(stoppedpid,sockfd,(int)ret);
		break;

	      case SYS_recvmsg:
		printf("[%d] recvmsg( ",stoppedpid);
		sockfd=get_args_send_recvmsg(stoppedpid,2,ret_trace,&regs);
		printf(" ) = %ld\n",ret);
		task_found = process_recv_call(stoppedpid,sockfd,(int)ret);
		break;

	      case SYS_shutdown:
		printf("[%d] shutdown( %ld, ",stoppedpid, arg1);
		char *how=malloc(10);;
		switch(arg2){
		case 0: strcpy(how,"SHUT_RD"); break;
		case 1: strcpy(how,"SHUT_WR"); break;
		case 2: strcpy(how,"SHUT_RDWR"); break;
		}
		printf("%s) = %ld\n",how,ret);;
		break;

	      case SYS_getsockopt:
		printf("[%d] getsockopt(",stoppedpid);
		get_args_get_setsockopt(stoppedpid, 1, &regs);
		printf("%d\n",(int)ret);
		break;

	      case SYS_setsockopt:
		printf("[%d] setsockopt(",stoppedpid);
		get_args_get_setsockopt(stoppedpid, 1, &regs);
		printf("%d\n",(int)ret);
		break;

      #else

	      case SYS__newselect:
		get_args_select(stoppedpid,&regs);
		break;

	      case SYS_socketcall:
		switch (arg1) {
		
		case 1:
		  printf("[%d] socket( ",stoppedpid);
		  get_args_socket(stoppedpid, (int)ret, (void *)arg2,NULL);
		  printf(" ) = %ld\n",ret);
		  break;

		case 2:
		  printf("[%d] bind( ",stoppedpid);
		  get_args_bind_connect(stoppedpid,(int)ret,0,(void *)arg2);
		  printf(" ) = %ld\n",ret);
		  break;

		case 3:
		  printf("[%d] connect( ",stoppedpid);
		  get_args_bind_connect(stoppedpid,(int)ret,1,(void *)arg2);
		  printf(" ) = %ld\n",ret);
		  if (ret<0)
		    printf("%s\n",strerror(-ret));
		  break;

		case 4: 
		  printf("[%d] listen( ", stoppedpid); 
		  get_args_listen(stoppedpid,(void *)arg2);
		  printf(" ) = %ld\n", ret);
		  break;

		case 5:
		  printf("[%d] accept( ",stoppedpid);
		  get_args_accept(stoppedpid,(int)ret, (void *)arg2);
		  printf(" ) = %ld\n",ret);
		  break;

		case 9:
		  printf("[%d] send( ",stoppedpid);
		  sockfd=get_args_send_recv(stoppedpid,1,ret_trace,(void *)arg2);
		  printf(" ) = %ld\n",ret);
		  task_found = process_send_call(stoppedpid,sockfd,(int)ret);
		  break;

		case 10:
		  printf("[%d] recv( ",stoppedpid);
		  sockfd=get_args_send_recv(stoppedpid,2,ret_trace,(void *)arg2);
		  printf(" ) = %ld\n",ret);
		  task_found = process_recv_call(stoppedpid,sockfd,(int)ret);
		  break;

		case 11:
		  printf("[%d] sendto(",stoppedpid);
		  sockfd=get_args_sendto_recvfrom(stoppedpid,1,ret_trace, (void *)arg2);
		  printf(" ) = %ld\n", ret); 
		  task_found = process_send_call(stoppedpid,sockfd,(int)ret);
		  break;

		case 12:
		  printf("[%d] recvfrom(",stoppedpid);
		  sockfd=get_args_sendto_recvfrom(stoppedpid,2,ret_trace,(void *)arg2);
		  printf(" ) = %ld\n", ret);
		  task_found = process_recv_call(stoppedpid,sockfd,(int)ret);
		  break;

		case 13:
		  printf("shutdown\n");
		  break;

		case 14:
		  printf("[%d] setsockopt(",stoppedpid);
		  get_args_get_setsockopt(stoppedpid, 2, (void *)arg2);
		  printf("%d\n",(int)ret);
		  break;

		case 15:
		  printf("[%d] getsockopt(",stoppedpid);
		  get_args_get_setsockopt(stoppedpid, 1, (void *)arg2);
		  printf("%d\n",(int)ret);
		  break;

		case 16:
		  printf("[%d] sendmsg(",stoppedpid);
		  sockfd=get_args_send_recvmsg(stoppedpid,1,ret_trace,(void *)arg2);
		  printf(" ) = %ld\n", ret);
		  task_found = process_send_call(stoppedpid,sockfd,(int)ret);
		  break;

		case 17:
		  printf("[%d] recvmsg(",stoppedpid);
		  sockfd=get_args_send_recvmsg(stoppedpid,2,ret_trace,(void *)arg2);
		  printf(" ) = %ld\n", ret);
		  task_found = process_recv_call(stoppedpid,sockfd,(int)ret);
		  break;

	      
		}
		break;

      #endif

	      default :
		  printf("[%d] Unknown syscall %ld ?= %ld\n", stoppedpid,reg_orig,ret);
		  break;

	      }
	      set_out_syscall(stoppedpid);
	    
	    }
	  }
	  else
	  {
	    if(in_syscall(stoppedpid))
	      set_out_syscall(stoppedpid);
	    else
	      set_in_syscall(stoppedpid);
    // 	printf("\t\t\t\t launcher syscall\n");
	  }
	  
	  //if the syscalls we trap doesn't lead to a task we resume child to found the next one
	  if(!task_found)
	  {
	    if (ptrace(PTRACE_SYSCALL, stoppedpid, NULL, NULL)==-1) {
	      perror("ptrace syscall");
	      exit(1);
	    }
	    
	  }
	  else
	  {
	    --(global_data->not_assigned);
	    printf("(left %d) New task found for pid %d\n",global_data->not_assigned, stoppedpid);
	  }
	}
	if(!global_data->child_amount)
	  break;
	
	double* next_time = xbt_fifo_shift(global_data->time_to_next);
	
	xbt_dynar_t arr;
	if(next_time==NULL)
	{
	  printf("\t\t\t\t\t NEW SIMULATION TURN with time %lf\n", -1.);
	  arr = SD_simulate(-1);
	}
	else
	{
	  printf("Process to simulation\n");
	  printf("\t\t\t\t\t NEW SIMULATION TURN with time %lf\n", *next_time);
	  arr = SD_simulate(*next_time);
	}
	//now all processus have got a task to execute we can run simulation.
	
	printf("End of Simulation\n");
	//Now there is two case.
	//	1: there no processus in arr and we have to launch the next processus.
	//	2: there's processus and we have to substract time and resume these processus.
	if(xbt_dynar_is_empty(arr))
	{
	  printf("New simulation time %lf\n", update_simulation_clock());
	  if(global_data->launcherpid)
	  {
	    if (ptrace(PTRACE_SYSCALL, global_data->launcherpid, NULL, NULL)==-1) {
	      printf("%d\n", global_data->launcherpid);
	      perror("ptrace syscall");
	      exit(1);
	    }
	  }
	  if (ptrace(PTRACE_SYSCALL, global_data->last_pid_create, NULL, NULL)==-1) {
	    printf("%d\n", global_data->launcherpid);
	    perror("ptrace syscall");
	    exit(1);
	  }
	  global_data->not_assigned +=2;
	}
	else
	{
	  if(next_time != NULL)
	  {
	    *next_time -= update_simulation_clock();
	    xbt_fifo_unshift(global_data->time_to_next, next_time);
	  }
	  SD_task_t temp_task;
	  unsigned int cpt;
	  xbt_dynar_foreach(arr, cpt, temp_task){
	    if(SD_task_get_state(temp_task) == SD_DONE)
	    {
	      int* data = (int *)SD_task_get_data(temp_task);
	      if(data!=NULL)
	      {
		printf("Simulation task ending for process %d \n", *data);
		if (ptrace(PTRACE_SYSCALL, *data, NULL, NULL)==-1) {
		  perror("ptrace syscall");
		  exit(1);
		}
		++global_data->not_assigned;
	      }
	    }
	  }
	}
      }

  }
  return 0;

}
