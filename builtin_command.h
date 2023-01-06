// renvoie 1 si la command est un des builtin listé ci dessous
int is_builtin_command(struct cmd *cmd)
{
  return (strcmp(cmd->args[0], "cd") == 0 ||
          strcmp(cmd->args[0], "history") == 0 ||
          strcmp(cmd->args[0], "jobs") == 0 ||
          strcmp(cmd->args[0], "fg") == 0 ||
          strcmp(cmd->args[0], "bg") == 0);
}

// execute la commande cd
int cd_command(struct cmd *cmd)
{
  //chdir
  if (cmd->args[2] != NULL) {
    fprintf(stderr, "cd : too many arguments\n");
    return -1;
  }
  char* dir = cmd->args[1];
  if (dir == NULL) {
    dir = getenv("HOME");
  }
  if (chdir(dir)) {
    fprintf(stderr, "cannot move to %s\n", cmd->args[1]);
    return -1;
  }
  return 0;
}

// exeecute la command history
int history_command(struct cmd *cmd)
{
  //check error
  if (cmd->args[2] != NULL) {
    fprintf(stderr, "history : too many arguments\n");
    return -1;
  } 
  if (cmd-> args[1] != NULL && atoi(cmd->args[1]) == 0 && strcmp(cmd->args[1], "0") != 0) {
    fprintf(stderr, "history : numeric argument requiered\n");
    return -1;
  }

  //display history
  HISTORY_STATE *hist = history_get_history_state ();
  HIST_ENTRY **histlst = history_list ();

  int nb_disp = hist->length;
  if (cmd->args[1] != NULL) {
    nb_disp = atoi(cmd->args[1]);
  }
  for (int i = hist->length - nb_disp; i < hist->length; i++) {
    printf(" %d  %s\n", i + 1, histlst[i]->line);
  }
  return 0;
}

// execute la command jobs
int jobs_command(struct cmd *cmd)
{
  if (cmd->args[1] == NULL) {
    refresh_jobstk(main_jobs_stack, 1);
  }
  else {
    int i = 1;
    while (cmd->args[i] != NULL) {
      int proc_id = atoi(cmd->args[i]);
      if (proc_id == 0 && strcmp(cmd->args[1], "0") != 0) {
        fprintf(stderr, "jobs : numeric argument required\n");
        return -1;
      }
      struct process* proc = find_jobstk(main_jobs_stack, proc_id);
      if (proc == NULL) {
        fprintf(stderr, "jobs : %d : no such job\n", proc_id);
        return -1;
      }
      refresh_process(main_jobs_stack, proc, 0);
      print_process(main_jobs_stack, proc);
    }
  }
  return 0;
}

//execute la command fg
int fg_command(struct cmd *cmd)
{
  if (main_jobs_stack->nb_proc == 0) {
    fprintf(stderr, "jobs : no job under control\n");
    return -1;
  }
  if (cmd->args[2] != NULL) {
    fprintf(stderr, "jobs : fg : too many arguments\n");
    return -1;
  }
  struct process* proc = NULL;
  if (find_from_name_jobstk(main_jobs_stack, &proc, cmd->args[1]) < 0) {
    return -1;
  }
  if (continue_fg_proc(main_jobs_stack, proc) < 0) {
    fprintf(stderr, "jobs : failing restarting processus\n");
    return -1;
  }
  int status;
  wait_for_proc(main_jobs_stack, proc, &status);
  return WEXITSTATUS(status);
}

//execute la command bg
int bg_command(struct cmd *cmd)
{
  if (main_jobs_stack->nb_proc == 0) {
    fprintf(stderr, "jobs : no job under control\n");
    return -1;
  }
  if (cmd->args[2] != NULL) {
    fprintf(stderr, "jobs : fg : too many arguments\n");
    return -1;
  }
  struct process* proc = NULL;
  if (find_from_name_jobstk(main_jobs_stack, &proc, cmd->args[1]) < 0) {
    return -1;
  }
  return continue_bg_proc(proc);
}

//execute une commande 'builtin'
int builtin_command(struct cmd *cmd)
{
  int save_stdin = dup(0);
  int save_stdout = dup(1);
  int save_stderr = dup(2);
  int fd = apply_redirects(cmd);
  int res;
  if (strcmp(cmd->args[0], "cd") == 0) {
    res = cd_command(cmd);
  }
  else if (strcmp(cmd->args[0], "history") == 0) {
    res = history_command(cmd);
  }
  else if (strcmp(cmd->args[0], "jobs") == 0) {
    res = jobs_command(cmd);
  }
  else if (strcmp(cmd->args[0], "fg") == 0) {
    res = fg_command(cmd);
  }
  else if (strcmp(cmd->args[0], "bg") == 0) {
    res = bg_command(cmd);
  }
  else {
    fprintf(stderr, "%s : command not found\n", cmd->args[0]);
    res = -1;
  }
  restore_redirects(save_stdin, save_stdout, save_stderr);
  if (fd >= 0) {
    close(fd);
  }
  return res;
}
