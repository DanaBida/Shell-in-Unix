#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "linux/limits.h"
#include "sys/types.h"
#include "job_control.h"
#include <signal.h>

#include "line_parser.h"

#define PATH_MAX 4096 /*the pathMax defined in linux*/
#define INPUT_MAX 2048

void execute(cmd_line *, int left[] , int right[], job*);
void execute_wrapper(cmd_line *);
char * strsignal(int sig);
pid_t getpgid();
void signal_handler(int);

job** job_list;
struct termios * shell_tmodes;

int main (int argc, char** argv, char ** envp){

	pid_t shell_pgid;
	char input [INPUT_MAX];
	char buffer [PATH_MAX];
	char * working_directory;
	cmd_line * cmd_struct;

	shell_tmodes = malloc(sizeof(struct termios));

	/*Ignore the following signals so they can reach the foreground child process rather than the shell.*/
	signal(SIGTSTP,SIG_IGN);
	signal(SIGTTOU,SIG_IGN);
	signal(SIGTTIN,SIG_IGN);

	/*ignore the following signals by passing them to handler_ptr*/
	void (*handler_ptr) (int) = &signal_handler; /*handler_ptr points to the function signal_handler*/
	signal(SIGQUIT, handler_ptr);
	signal(SIGCHLD, handler_ptr);

	/*Set the process group of the shell to its process id */
	setpgid(getpid(), getpid());

	/*init job_list*/
	job_list = malloc(sizeof(job));
	shell_pgid = getpgid();  /*return the process group ID of the calling process.*/

	/*save terminal attributes to restore them back when a process running in the foreground ends.*/
   /* tcgetattr(STDIN_FILENO, shell_tmodes);*/

	while(1)/*infinite loop*/
	{
		working_directory = getcwd(buffer, PATH_MAX);/*These functions returns the absolute path_name of the current working directory of the calling process.*/
		if(working_directory != NULL)
			printf("%s$ ", working_directory);
    	else 
    		perror("ERROR : can't find the working directory \n");
		fgets(input,INPUT_MAX,stdin); /*reading line from the "user"*/
		if(strcmp(input,"quit\n")==0){/*exit program in normally way if finished*/
			free_job_list(job_list);
			exit(0);
		} 
		if(strcmp(input,"\n")==0) /*if the input is enter*/
			continue;
		cmd_struct = parse_cmd_lines(input);
		if(cmd_struct == NULL)
			printf("ERROR : there is nothing to parse\n");
		else{
				if (strcmp(cmd_struct->arguments[0], "fg") == 0) {
	        		int cont = atoi(cmd_struct->arguments[1]);
	                job *fgJob = find_job_by_index(*job_list, cont);
					run_job_in_foreground(job_list, fgJob, 1, shell_tmodes, shell_pgid);
					free_cmd_lines(cmd_struct);
					continue;
	            }
	            else if (strcmp(cmd_struct->arguments[0], "bg") == 0) {
		        	int cont = atoi(cmd_struct->arguments[1]);/*the number of the job*/
		        	job *bgJob = find_job_by_index(*job_list, cont);
		        	if(bgJob->status == SUSPENDED)
						run_job_in_background(bgJob, 1);
					free_cmd_lines(cmd_struct);
					continue;
    			}  
				else if (strcmp(cmd_struct->arguments[0], "jobs") == 0) {
			        print_jobs(job_list);
			        free_cmd_lines(cmd_struct);
					continue;			      
				}
				execute_wrapper(cmd_struct);
				free_cmd_lines(cmd_struct);
		}
	}

	return 0;
}

/*prints the signal sig that the shell receives with a message saying it was ignored*/
void signal_handler(int sig){
	char* signal_name;
	signal_name =  strsignal(sig);
	printf("%s was ignored \n", signal_name);
}


void execute_wrapper(cmd_line * cmd_struct){

	int* pipe_p = NULL;

	job * job1 = add_job(job_list, cmd_struct->arguments[0]);
    job1->status = RUNNING;	

	if(cmd_struct->next !=NULL){
		int pipe_right[2];
        if(pipe(pipe_right)==-1)
        	exit(1);
		pipe_p = pipe_right;
    }
    execute(cmd_struct,NULL, pipe_p, job1);
	
}

void execute(cmd_line* cmd_struct, int* pipe_left, int* pipe_right, job* job){

	/*suspend the shell process - save the shell terminal attributes*/
    tcgetattr(STDIN_FILENO, shell_tmodes); 

	pid_t pid  = fork();

	if(pid == -1){
		perror("ERROR : fail in forking\n");
        exit(1);
	}

	if(pid == 0){ /*child*/

		if(pipe_right!=NULL){
			close(1); /* Child process closes standart output */
            dup(pipe_right[1]);  /*Duplicate the write-end of the pipe using dup - replace it insted of stdout*/
            close(pipe_right[1]); /*Close the file descriptor that was duplicated.*/
		}
		if(pipe_left!=NULL){
			close(0);    /* Child process closes standart input */
            dup(pipe_left[0]);  /*Duplicate the read-end of the pipe using dup - replace it instad of stdin*/
            close(pipe_left[0]); /*Close the file descriptor that was duplicated.*/
		}

	    if (job->pgid == 0) /*change only the pgid for the first child  to that of the shell*/
			job->pgid = getpid();
		
		setpgid(getpid(), job->pgid); /*change the other child processes to the pgid of the group*/

	    /* set signals back to default */
	    signal(SIGTSTP, SIG_DFL);
		signal(SIGTTOU, SIG_DFL);
		signal(SIGTTIN, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		signal(SIGCHLD, SIG_DFL);

		/*checking if needed to change stdout/in to files*/		
		char const *input_file = cmd_struct->input_redirect;
		char const *output_file = cmd_struct->output_redirect;	
		if(input_file != NULL){
			close(0);
			fopen(input_file,"r");
		}
		if(output_file != NULL){
			close(1);
			fopen(output_file,"w"); /*data is added to end of file.*/
		}
		if(execvp(cmd_struct->arguments[0], cmd_struct->arguments) == -1){
			perror("ERROR : failing execute the file\n");
			_exit(1);
		}
	}

	else{ /*parent*/

		if (job->pgid == 0) /* set the pgid of the new process to be the same as the child process id */
			job->pgid = pid;
		
		setpgid(pid, job->pgid); /*change the parent id to the pgid of the group*/

		if(pipe_right!=NULL)
			close(pipe_right[1]);
		if(pipe_left!=NULL)
			close(pipe_left[0]);

	    /*suspend the shell process - save the shell terminal attributes*/
	    tcgetattr(STDIN_FILENO, shell_tmodes); 

		if(cmd_struct->blocking){/*if a "&" symbol !!isn't!! added */
			run_job_in_foreground(job_list, job, 0, shell_tmodes, getpid());		
    	}
    	else{  /*if a "&" symbol !!is!! added */
    		run_job_in_background(job, 0);
    	}

    	/*recursive call*/

       	int* pipe_p = NULL;

        if(cmd_struct->next == NULL)
            return;
        
        if(cmd_struct->next->next!=NULL)
        {
        	int pipe_right_new[2];
			if(pipe(pipe_right_new)==-1)
				exit(1);
			pipe_p = pipe_right_new;
        }

		execute(cmd_struct->next, pipe_right, pipe_p, job);
		
	}

}