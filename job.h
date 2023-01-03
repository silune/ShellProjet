#include <stdio.h>
#include <string.h>
#include <termios.h>

struct process {
  int group_id;
  int proc_id;
  char* proc_name;
  int status;
  struct termios *terminal_state;
  struct process* next;
};

struct jobstk {
  int nb_proc;
  struct process* top;
};

// ---------- GESTION DE LA PILE DE PROCESS (JOBSTK) ----------

// creation d'une nouvelle pile
struct jobstk* new_jobstk (void)
{
  struct jobstk* jobs_stack = malloc(sizeof(struct jobstk));
  jobs_stack->nb_proc = 0;
  jobs_stack->top = NULL;
  return jobs_stack;
}

// retire le processus sur le haut de la pile
struct process* pop_jobstk (struct jobstk *jobs_stack)
{
  struct process* proc = jobs_stack->top;
  if (proc != NULL) {
    jobs_stack->top = proc->next;
    jobs_stack->nb_proc = jobs_stack->nb_proc - 1;
  }
  return proc;
}

// met un processus sur le sommet de la pile
// renvoie -1 si il n'existe pas
int up_proc (struct jobstk *jobs_stack, struct process *proc)
{
  //recherche du processus
  struct process* prev_of_proc = jobs_stack->top;
  if (prev_of_proc == proc) {
    //deja sur le dessus de la pile
    return 0;
  }
  while (prev_of_proc != NULL && prev_of_proc->next != proc) {
    prev_of_proc = prev_of_proc->next;
  }
  if (prev_of_proc == NULL) {
    //la pile est vide ou ne contient pas le processus
    return -1;
    //add_new_stopped_proc_jobstk(jobs_stack, group_id, proc_name);
  }
  prev_of_proc->next = proc->next;
  proc->next = jobs_stack->top;
  jobs_stack->top = proc;
  return 0;
}

// liberation d'une pile
void free_jobstk (struct jobstk *jobs_stack)
{
  struct process* proc = pop_jobstk(jobs_stack);
  while (proc != NULL) {
    free(proc->terminal_state);
    free(proc);
    proc = pop_jobstk(jobs_stack);
  }
  free(jobs_stack);
}

// retire un processus donné de la pile
void delete_proc_jobstk (struct jobstk *jobs_stack, struct process *proc)
{
  //chercher le processus
  struct process *prev_of_proc = jobs_stack->top;
  if (prev_of_proc == proc) {
    //si c'est le haut de la pile
    pop_jobstk(jobs_stack);
    return;
  }
  while (prev_of_proc != NULL && prev_of_proc->next != proc) {
    prev_of_proc = prev_of_proc->next;
  }
  if (prev_of_proc == NULL) {
    //si on ne l'a pas trouvé
    return;
  }
  prev_of_proc->next = proc->next;
  jobs_stack->nb_proc = jobs_stack->nb_proc - 1;
  free(proc);
}

// trouve un processus à partir de son identifiant
struct process* find_jobstk (struct jobstk *jobs_stack, int proc_id)
{
  struct process* proc = jobs_stack->top;
  while (proc != NULL && proc->proc_id != proc_id) {
    proc = proc->next;
  }
  return proc;
}

int find_from_name_jobstk (struct jobstk *jobs_stack, struct process **proc, char* name)
{
  if ((name == NULL || strcmp(name, "+") == 0)) {
    // Premier processus sur la pile
    *proc = jobs_stack->top;
    if (*proc == NULL) {
      fprintf(stderr, "jobs : no job under control, cannot find one\n");
      return -1;
    }
    return 0;
  }
  if (strcmp(name, "-") == 0) {
    if (jobs_stack->top->next == NULL) {
      *proc = jobs_stack->top;
    }
    *proc = jobs_stack->top->next;
    return 0;
  }
  int proc_id = atoi(name);
  if (proc_id == 0 && strcmp(name, "0") != 0) {
    fprintf(stderr, "jobs : fg : numeric argument required\n");
    return -1;
  }
  *proc = find_jobstk(jobs_stack, proc_id); // n-ième processus sur la pile
  if (*proc == NULL) {
    fprintf(stderr, "jobs : no such job %d\n", proc_id);
    return -1;
  }
  return 0;
}

// print un processus donné 
void print_process (struct jobstk* jobs_stack, struct process *proc) {
  char* status_name = malloc(8 * sizeof(char));
  switch (proc->status)
  {
    case 0 :
      status_name = "Init...";
      break;
    case 1 :
      status_name = "Running";
      break;
    case 2 :
      status_name = "Stopped";
      break;
    case 3 :
      status_name = "Done   ";
      break;
    default:
      status_name = "Unkonwn";
      break;
  }
  char stack_status = ' ';
  if (proc == jobs_stack->top) {
    stack_status = '+';
  }
  else if (proc == jobs_stack->top->next) {
    stack_status = '-';
  }
  char is_bakground = ' ';
  if (proc->status == 1) {
    is_bakground = '&';
  }
  printf(" [%d]%c %s\t\t %s %c\n", proc->proc_id, stack_status, status_name, proc->proc_name, is_bakground);
}

void refresh_process(struct jobstk* jobs_stack, struct process *proc, int print_ended)
{
  int status;
  int pid = waitpid(proc->group_id, &status, WUNTRACED | WNOHANG);
  if (pid != 0 && WIFEXITED(status)) {
    proc->status = 3;
    if (print_ended) {
      print_process(jobs_stack, proc);
    }
  }
}

void refresh_jobstk(struct jobstk *jobs_stack, int printing)
{
  int visited = 0;
  int i = 1;
  while (visited < jobs_stack->nb_proc) {
    struct process* proc = find_jobstk(jobs_stack, i);
    if (proc != NULL) {
      refresh_process(jobs_stack, proc, !printing);
      if (printing) {
        print_process(jobs_stack, proc);
      }
      if (proc->status == 3) {
        delete_proc_jobstk(jobs_stack, proc);
      }
      else {
        // si on le supprime il n'est pas visite mais il y en a moins
        visited++;
      }
    }
    i++;
  }
}

// ---------- GESTION DES PROCESSUS ----------

// ajoute un processus en tant que "Init..." à la pile
struct process* add_new_proc_jobstk (struct jobstk *jobs_stack, int group_id, char* proc_name)
{
  struct process* new_proc = malloc(sizeof(struct process));
  new_proc->group_id = group_id;
  int proc_id = 1;
  int found = 0;
  while (found < jobs_stack->nb_proc) {
    if (find_jobstk(jobs_stack, proc_id) != NULL) {
      found++;
    }
    proc_id++;
  }
  new_proc->proc_id = proc_id;
  new_proc->proc_name = proc_name;
  new_proc->status = 0;
  new_proc->terminal_state = malloc(sizeof(struct termios));
  tcgetattr(0, new_proc->terminal_state);
  new_proc->next = jobs_stack->top;

  jobs_stack->nb_proc = jobs_stack->nb_proc + 1;
  jobs_stack->top = new_proc;

  return new_proc;
}

// stop un processus de la pile et le met sur le dessus
// renvoie -1 s'il est déjà stopé ou qu'il ne le trouve pas
int stop_proc_jobstk (struct jobstk *jobs_stack, struct process* proc, int end_status)
{
  if (proc->status == 2) {
    return -1;
  }
  if (up_proc(jobs_stack, proc) < 0) {
    return -1;
  }
  proc->status = end_status;
  tcgetattr(0, proc->terminal_state);
  return 0;
}

// relance un processus et le met sur le dessus de la pile
// renvoie -1 si le processus est en cours ou si il n'existe pas
int continue_fg_proc(struct jobstk *jobs_stack, struct process *proc)
{
  if (up_proc(jobs_stack, proc) < 0) {
      return -1;
  }
  printf("%s\n", proc->proc_name);
  push_foreground(proc->group_id, *proc->terminal_state);
  kill(- proc->group_id, SIGCONT);
  proc->status = 1;
  return 0;
}

int continue_bg_proc(struct process *proc)
{
  if (proc->status == 1) {
    fprintf(stderr, "jobs : already running in background");
    return -1;
  }
  printf(" [%d] %s &\n", proc->proc_id, proc->proc_name);
  proc->status = 1;
  kill(- proc->group_id, SIGCONT);
  return 0;
}

//attends un processus
void wait_for_proc (struct jobstk* jobs_stack, struct process* proc, int *status)
{
  waitpid(proc->group_id, status, WUNTRACED);
  if (WIFSIGNALED(*status)) {
    printf("\n");
  }
  if (WIFSTOPPED(*status)) {
    if (stop_proc_jobstk(jobs_stack, proc, 2) < 0) {
      fprintf(stderr, "jobs : cannot stop process\n");
    }
    printf("\n");
    print_process(jobs_stack, proc);
  }
  else {
    if (stop_proc_jobstk(jobs_stack, proc, 3) < 0) {
      fprintf(stderr, "jobs : cannot stop process\n");
    }
    pop_jobstk(jobs_stack);
  }
  tcsetpgrp(0, terminal_fd);
  tcsetattr(0, TCSADRAIN, &terminal_attr);
}

struct jobstk *main_jobs_stack;
