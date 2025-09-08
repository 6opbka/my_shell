
#define _POSIX_C_SOURCE 200809L


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <fcntl.h>
#include <limits.h>


#define MAX_LINE_LENGTH 256
#define PATH_MAX 4096

char* home_path = NULL;

void ls(char** args);
void cd(char** args);
void pwd(char** args);
void foo2_func(char** args);
void exit_cmd(char** args);
void process_prc(char** args);
void process_redirection(char** cmd);

int redirect_type = 0;
int saved_stdout = -1;

char* g_last_line = NULL;

struct command{
    const char* name;
    void (*func)(char**);
};

struct command command_table[] = {
    {"ls",ls},
    {"cd",cd},
    {"pwd",pwd},
    {"foo2",foo2_func},
    {"exit",exit_cmd},
    {NULL,NULL}
};

int cmpstr(const void* a, const void* b){
    const char *fa = *(const char**)a;
    const char *fb = *(const char**)b;
    return strcmp(fa,fb);
}

char** read_input(){
    ssize_t ret;
    
    size_t len = 0;

    printf("> ");
    ret = getline(&g_last_line,&len, stdin);
    if(!ret){
        printf("Err reading command\n");
    }
  
    int bufsize = 16;
    char **out = malloc(bufsize * sizeof(char*));
    if (!out) {
        perror("malloc");
        free(g_last_line);
        return NULL;
    }
    

    char* token = strtok(g_last_line," \n");
    // printf("command> %s\n",token);

    int pos = 0;
    
    while(token != NULL){
        
        out[pos++] = token;
        if(pos >= bufsize){
            bufsize*=2;
            out = realloc(out,bufsize*sizeof(char*));
            if(!out){
                perror("realloc err\n");
                free(g_last_line);
                return NULL;
            }
        }
        token = strtok(NULL," \n");
        if(token != NULL){
            
        }
    }
    
    out[pos] = NULL;
    
    return out;
}



void process_prc(char** args){
    

    pid_t pid = fork();
    if(pid<0){
        perror("fork\n");
        exit(EXIT_FAILURE);
    }
    else if(pid==0){
        // Redirection logic before exe

        execvp(args[0],args);

        perror("execvp\n");
        exit(EXIT_FAILURE);
    }
    else{
        int status;
        wait(&status);
        if(WIFEXITED(status)){
            printf("Child exited with status %d\n", WEXITSTATUS(status));
        } else {
            printf("Child did not terminate normally\n");
        }
    }

}

void process_redirection(char** args) {
    char* redir_filename = NULL;
    int i;

    for (i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) {
            redirect_type = 1;
            redir_filename = args[i+1];
            args[i] = NULL;  // Truncate command at redirection
            break;
        }
        else if (strcmp(args[i], "<") == 0) {
            redirect_type = 2;
            redir_filename = args[i+1];
            args[i] = NULL;  // Truncate command at redirection
            break;
        }
        else if (strcmp(args[i],">>") == 0){
            redirect_type = 3;
            redir_filename = args[i+1];
            args[i] = NULL;
            break;
        }
    }
    if(redirect_type == 0 || redir_filename == NULL) return;

    printf("filename: %s\n", redir_filename);
    printf("redirection type: %d\n", redirect_type);
    int fd = -1;

    switch (redirect_type)
    {
    case 1:
        fd = open(redir_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            perror("open");
            return;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
        break;

    case 2:
        fd = open(redir_filename, O_RDONLY);
        if (fd < 0) {
            perror("open");
            return;
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
        break;
    case 3:
        fd = open(redir_filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd < 0) {
            perror("open");
            return;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
        break;
    default:
        break;
    }   
}


void process_cmd(char** cmd) {
    // Save original file descriptors
    int saved_stdout = dup(STDOUT_FILENO);
    int saved_stdin = dup(STDIN_FILENO);
    if (saved_stdout == -1 || saved_stdin == -1) {
        perror("dup");
        return;
    }

    // Process redirection (this modifies cmd array)
    redirect_type = 0;
    process_redirection(cmd);

    // Handle built-in commands
    int is_builtin = 0;
    for (size_t i = 0; command_table[i].name != NULL; i++) {
        if (cmd[0] && strcmp(cmd[0], command_table[i].name) == 0) {
            command_table[i].func(cmd + 1);  // Execute with redirection active
            is_builtin = 1;
            break;
        }
    }

    // Handle external commands
    if (!is_builtin && cmd[0]) {
        process_prc(cmd);
    }

    // Restore original file descriptors if redirection occurred
    if (redirect_type != 0) {
        if (dup2(saved_stdout, STDOUT_FILENO) == -1) perror("dup2 stdout");
        if (dup2(saved_stdin, STDIN_FILENO) == -1) perror("dup2 stdin");
    }
    close(saved_stdout);
    close(saved_stdin);
    if(g_last_line) free(g_last_line); //freeing the line buffer.
}

void print_dir(char** dirs, size_t size, unsigned int print_vert) {

    

    if(print_vert){
        for(size_t i = 0; i<size; i++){
            printf("%s\n",dirs[i]);
        }
        return;
    }

    struct winsize w;
    if (isatty(STDOUT_FILENO)) {
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
            perror("ioctl");
            return;
    }
    } else {
        w.ws_col = 80;  // default width for non-terminals (like redirected files)
    }

    size_t maxlen = 0;
    for (size_t i = 0; i < size; ++i) {
        size_t len = strlen(dirs[i]);
        if (len > maxlen) maxlen = len;
    }
    maxlen += 2; // padding

    // Calculate columns and rows
    size_t num_cols = w.ws_col / maxlen;
    if (num_cols == 0) num_cols = 1;
    size_t num_rows = (size + num_cols - 1) / num_cols;

    int* max_col_lengths = malloc(num_cols * sizeof(int));
    for (size_t col_id = 0; col_id < num_cols; col_id++) {  
        max_col_lengths[col_id] = 0; // Initialize
        for (size_t row_id = 0; row_id < num_rows; row_id++) {
            size_t id = col_id * num_rows + row_id;
            if (id >= size) continue; // Don't access out-of-bounds
            int len = strlen(dirs[id])+2;
            if (len > max_col_lengths[col_id])
                max_col_lengths[col_id] = len;
            }
    }
    
    // Print in column-major order
    for (size_t row = 0; row < num_rows; ++row) {
        for (size_t col = 0; col < num_cols; ++col) {
            size_t idx = col * num_rows + row;
            if (idx < size)
                printf("%-*s", max_col_lengths[col], dirs[idx]);
        }
        printf("\n");
    }
}

char* prepend_file_data(const char* file) {
    struct stat st;
    if (stat(file, &st) == -1) {
        perror("stat");
        return NULL;
    }

    // Permissions
    char perms[11];
    perms[0] = S_ISDIR(st.st_mode) ? 'd' :
               S_ISLNK(st.st_mode) ? 'l' : '-';
    perms[1] = (st.st_mode & S_IRUSR) ? 'r' : '-';
    perms[2] = (st.st_mode & S_IWUSR) ? 'w' : '-';
    perms[3] = (st.st_mode & S_IXUSR) ? 'x' : '-';
    perms[4] = (st.st_mode & S_IRGRP) ? 'r' : '-';
    perms[5] = (st.st_mode & S_IWGRP) ? 'w' : '-';
    perms[6] = (st.st_mode & S_IXGRP) ? 'x' : '-';
    perms[7] = (st.st_mode & S_IROTH) ? 'r' : '-';
    perms[8] = (st.st_mode & S_IWOTH) ? 'w' : '-';
    perms[9] = (st.st_mode & S_IXOTH) ? 'x' : '-';
    perms[10] = '\0';

    // User and group
    struct passwd *pw = getpwuid(st.st_uid);
    struct group  *gr = getgrgid(st.st_gid);

    char timebuf[64];
    struct tm *tm = localtime(&st.st_mtime);
    strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", tm);

    // Symlink target
    char link_target[PATH_MAX + 1] = "";
    if (S_ISLNK(st.st_mode)) {
        ssize_t len = readlink(file, link_target, PATH_MAX);
        if (len != -1) {
            link_target[len] = '\0';
        }
    }

    const char *user = pw ? pw->pw_name : "unknown";
    const char *group = gr ? gr->gr_name : "unknown";

    // Compute needed length
    size_t new_len = strlen(perms) + 1 + 10 + strlen(user) + 1 + strlen(group) + 1 +
                     20 + strlen(timebuf) + 1 + strlen(file) + 1;

    // perms + ' ' + user + ':' + group + ' ' + file + '\0'

    char* new_str = malloc(new_len);
    if (!new_str) {
        perror("malloc");
        return NULL;
    }

   snprintf(new_str, new_len, "%s %2lu %s:%s %8ld %s %s",
            perms,
            st.st_nlink,
            user,
            group,
            (long)st.st_size,
            timebuf,
            file
        );

    return new_str;
}


void ls(char** args){
    //ls -a 

    // struct to hold mode values
    struct mode { 
        char* str_val;
        unsigned int value;
    };

    // struct to hold modes
    #define MODES_NUM 3
    struct mode modes[] = {
        {"-1", 0},
        {"-a",0},
        {"-l",0}
    };

    DIR *d;
    struct dirent *dir;
    size_t i = 0;
    char* path = ".";

    while (args[i]!=0)
    {
        if(args[i][0] == '-'){
            for(size_t j = 0; j<MODES_NUM; j++ ){
                // Set the flags based on the parameter 
                if(strcmp(args[i],modes[j].str_val)== 0) modes[j].value = 1;
            }
        }
        else{
            printf("smth-> %s\n",args[i]);
            path = args[i];
        }
        i++;
    }

    d = opendir(path);
    
    unsigned int bufsize = 8;
    char** dirs_to_show = malloc(bufsize * sizeof(char*));
    if (!dirs_to_show) {
        perror("malloc");
        exit(1);
    }
    unsigned int dir_id = 0;

    if(d) {
        while((dir = readdir(d)) != NULL) {
            if(modes[1].value == 0 && dir->d_name[0] == '.') {
                continue;  // Skip hidden files if -a is not set
            }

            // Resize if needed
            if(dir_id == bufsize) {
                bufsize *= 2;
                char** tmp = realloc(dirs_to_show, bufsize * sizeof(char*));
                if(!tmp) {
                    perror("realloc");
                    // free previously allocated strings
                    for(unsigned int j=0; j<dir_id; j++) free(dirs_to_show[j]);
                    free(dirs_to_show);
                    closedir(d);
                    exit(1);
                }
                dirs_to_show = tmp;
            }

            dirs_to_show[dir_id] = strdup(dir->d_name);
            if(modes[2].value==1){
                modes[0].value =1;
                dirs_to_show[dir_id] = prepend_file_data(dirs_to_show[dir_id]);
            }

            if(!dirs_to_show[dir_id]) {
                perror("strdup");
                // free previously allocated strings
                for(unsigned int j=0; j<dir_id; j++) free(dirs_to_show[j]);
                free(dirs_to_show);
                closedir(d);
                exit(1);
            }
            dir_id++;
        }

        qsort(dirs_to_show, dir_id, sizeof(char*), cmpstr);

        print_dir(dirs_to_show, dir_id,modes[0].value);
        fflush(stdout);

        // Free allocated strings
        for(unsigned int j=0; j<dir_id; j++) {
            free(dirs_to_show[j]);
        }
        free(dirs_to_show);
        
        closedir(d);
    } else {
        printf("no such directory\n");
    }


}

void cd(char** args){
    int result = 0;
    if(!args[0]){
        result = chdir(home_path);
    }
    else{
        result = chdir(args[0]);
    }
    
    if (result != 0) {
        perror("cd");
    }

}

void pwd(char** args){
    char buf[PATH_MAX+1];
    if(getcwd(buf,PATH_MAX+1)!=NULL){
        printf("%s\n",buf);
    }
}

void foo2_func(char** args){

}

void exit_cmd(char** args){
    printf("exiting the shell...\n");
    exit(0);
}

void init(){
    home_path = getenv("HOME");
    if(home_path&&home_path[0]!='\0'){
        printf("home is %s\n", home_path);
    }
}




int main() {
    init();
    
    while (1) {
        char** cmd = read_input();
        if (!cmd) continue;  // Skip if read failed
        
        process_cmd(cmd);
     
        
        usleep(1000);  // Small delay to prevent CPU overuse
    }
    return 0;
}