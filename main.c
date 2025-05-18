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


#define MAX_LINE_LENGTH 256
#define PATH_MAX 4096

void ls(char** args);
void cd(char** args);
void pwd(char** args);
void foo2_func(char** args);
void exit_cmd(char** args);



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
    char* line = NULL;
    size_t len = 0;

    printf("> ");
    ret = getline(&line,&len, stdin);
    if(!ret){
        printf("Err reading command\n");
    }
  
    int bufsize = 16;
    char **out = malloc(bufsize * sizeof(char*));
    if (!out) {
        perror("malloc");
        free(line);
        return NULL;
    }
    

    char* token = strtok(line," \n");
    // printf("command> %s\n",token);

    int pos = 0;
    
    while(token != NULL){
        
        out[pos++] = token;
        if(pos >= bufsize){
            bufsize*=2;
            out = realloc(out,bufsize*sizeof(char*));
            if(!out){
                perror("realloc err\n");
                free(line);
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

void process_cmd(char** cmd){
    size_t i = 0;
    i = 0;
    while(command_table[i].name!=NULL){
        if(strcmp(cmd[0],command_table[i].name) == 0){
            // printf("calling %s\n", command_table[i].name);
            command_table[i].func(cmd+1);
        }
        i++;
    }

    // process_prc(cmd);
}

void print_dir(char** dirs, size_t size, unsigned int print_vert) {

    if(print_vert){
        for(size_t i = 0; i<size; i++){
            printf("%s\n",dirs[i]);
        }
        return;
    }

    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
        perror("ioctl");
        return;
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
    int result = chdir(args[0]);
    if (result != 0) {
        perror("cd");
    }

}

void pwd(char** args){

}

void foo2_func(char** args){

}

void exit_cmd(char** args){
    printf("exiting the shell...\n");
    exit(0);
}




int main(){
    
    
    while(1){
        char** cmd = read_input();
        process_cmd(cmd);
        usleep(1);
    }
    return 0;

}