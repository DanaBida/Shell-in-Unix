#include "job_control.h"

 int kill(pid_t pid, int sig);

/**
* Receive a pointer to a job list and a new command to add to the job list and adds it to it.
* Create a new job list if none exists.
**/
job* add_job(job** job_list, char* cmd){
	job* job_to_add = initialize_job(cmd);
	
	if (*job_list == NULL){
		*job_list = job_to_add;
		job_to_add -> idx = 1;
	}	
	else{
		int counter = 2;
		job* list = *job_list;
		while (list -> next !=NULL){
	/*		printf("adding %d\n", list->idx);*/
			list = list -> next;
			counter++;
		}
		job_to_add ->idx = counter;
		list -> next = job_to_add;
	}
	return job_to_add;
}


/**
* Receive a pointer to a job list and a pointer to a job and removes the job from the job list 
* freeing its memory.
**/
void remove_job(job** job_list, job* tmp){
	if (*job_list == NULL)
		return;
	job* tmp_list = *job_list;
	if (tmp_list == tmp){
		*job_list = tmp_list -> next;
		free_job(tmp);
		return;
	}
		
	while (tmp_list->next != tmp){
		tmp_list = tmp_list -> next;
	}
	tmp_list -> next = tmp -> next;
	free_job(tmp);
	
}

/**
* receives a status and prints the string it represents.
**/
char* status_to_str(int status)
{
  static char* strs[] = {"Done", "Suspended", "Running"};
  return strs[status + 1];
}


/**
*   Receive a job list, and print it in the following format:<code>[idx] \t status \t\t cmd</code>, where:
    cmd: the full command as typed by the user.
    status: Running, Suspended, Done (for jobs that have completed but are not yet removed from the list).
  
**/
void print_jobs(job** job_list){

	job* tmp = *job_list;
	update_job_list(job_list, FALSE);
	while (tmp != NULL){
		printf("[%d]\t %s \t\t %s", tmp->idx, status_to_str(tmp->status),tmp -> cmd); 
		
		if (tmp -> cmd[strlen(tmp -> cmd)-1]  != '\n')
			printf("\n");
		job* job_to_remove = tmp;
		tmp = tmp -> next;
		if (job_to_remove->status == DONE)
			remove_job(job_list, job_to_remove);
		
	}
 
}


/**
* Receive a pointer to a list of jobs, and delete all of its nodes and the memory allocated for each of them.
*/
void free_job_list(job** job_list){
	while(*job_list != NULL){
		job* tmp = *job_list;
		*job_list = (*job_list) -> next;
		free_job(tmp);
	}
	
}


/**
* receives a pointer to a job, and frees it along with all memory allocated for its fields.
**/
void free_job(job* job_to_remove){
	free(job_to_remove->tmodes);
	free(job_to_remove->cmd);
	free(job_to_remove);
}



/**
* Receive a command (string) and return a job pointer. 
* The function needs to allocate all required memory for: job, cmd, tmodes
* to copy cmd, and to initialize the rest of the fields to NULL: next, pigd, status 
**/

job* initialize_job(char* cmd){
	
	job * job1 = (job *) malloc(sizeof(job)); 
	job1->cmd = malloc(strlen(cmd)+1);
   	strcpy( job1->cmd, cmd);
   	job1->tmodes = malloc (1000);
   	job1->next = NULL;
   	job1->idx = 0;
   	job1->pgid = 0;
   	job1->status = 0;
   	return job1;
}


/**
* Receive a job list and and index and return a pointer to a job with the given index, according to the idx field.
* Print an error message if no job with such an index exists.
**/
job* find_job_by_index(job* job_list, int idx){
  
  if (job_list == NULL) /*empty list*/
		perror("ERROR : no job with this index exists \n");

	job* tmp_list = job_list;

	if (idx == 1){ /*if the first corellates*/
		return tmp_list;
	}
	/*search idx in the list*/
	while (tmp_list->next != NULL){
		tmp_list = tmp_list -> next;
		if (tmp_list->idx == idx)
			return tmp_list;
	}

	perror("ERROR : no job with this index exists \n");
	return NULL;
}


/**
* Receive a pointer to a job list, and a boolean to decide whether to remove done
* jobs from the job list or not. 
**/
void update_job_list(job **job_list, int remove_done_jobs){
	
	job* tmp_list = *job_list;
	pid_t pid;
	int status;
	while(tmp_list != NULL) {
		pid = waitpid(-tmp_list->pgid, &status, WNOHANG);
		if(tmp_list->status == RUNNING && pid > 0)
			tmp_list->status = DONE;
		if(tmp_list->status == DONE && remove_done_jobs){
			printf("[%d]\t %s \t\t %s", tmp_list->idx, status_to_str(tmp_list->status),tmp_list -> cmd);
			if (tmp_list -> cmd[strlen(tmp_list -> cmd)-1]  != '\n')
				printf("\n");
			remove_job(job_list,tmp_list);
		}
		tmp_list = tmp_list->next;
	}
}


/** 
* Put job j in the foreground.  If cont is nonzero, restore the saved terminal modes and send the process group a
* SIGCONT signal to wake it up before we block.  Run update_job_list to print DONE jobs.
**/

void run_job_in_foreground (job** job_list, job *j, int cont, struct termios* shell_tmodes, pid_t shell_pgid){

    int status;
    pid_t pid = waitpid(-j->pgid, &status, WNOHANG);
    /*If it fails , prints a Done message in the same format as in printJobs and remove the job from the job list*/
    if(pid == -1){
        printf("[%d]\t %s \t\t %s", j->idx, status_to_str(j->status), j->cmd); 
        remove_job(job_list, j);
        return;
    }
    /*one or more child(ren) specified by pid exist, but have not yet changed state*/
    if(pid == 0) {
        tcsetpgrp(STDIN_FILENO,j->pgid);/*put the job in the foreground */
    } 
    if(cont == 1 && j->status == SUSPENDED) {
        tcsetattr(STDIN_FILENO, TCSADRAIN, j->tmodes); /*set the attributes of the terminal to that of the job's*/
        kill(j->pgid, SIGCONT); /*send SIGCONT signal to the process group of the job*/ 
    }
    
    waitpid(-j->pgid,&status,WUNTRACED);    
    if(WIFSTOPPED(status)){
    	if (WSTOPSIG(status)== SIGTSTP){/* Ctrl-Z*/
			j->status = SUSPENDED;
   		}
	    if(WSTOPSIG(status)== SIGINT){/* Ctrl-C*/
			j->status = DONE;
		}
	}
	else
		j->status = DONE;

	tcsetpgrp(STDIN_FILENO, shell_pgid);/* put the shell back in the foreground.*/
  	tcgetattr (STDIN_FILENO,j->tmodes);/*save the terminal attributes in the job tmodes.*/
  	tcsetattr (STDIN_FILENO,TCSADRAIN,shell_tmodes);/*restore the shell's terminal modes.*/

	update_job_list(job_list,1);
}


/** 
* Put a job in the background.  If the cont argument is nonzero, send
* the process group a SIGCONT signal to wake it up.  
**/

void run_job_in_background (job *j, int cont){	

    j->status = RUNNING; /*Put a job in the background*/
	if(cont){
	    kill(j->pgid, SIGCONT);
	}
}

