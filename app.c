// Hakan Duran 150200091

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/wait.h>


// I've implemented a linked list for my code

struct node* head = NULL;
struct node
{
	char** pcom;
	struct node* next;
    struct node* prev;
    int* in;
    int* out;
};

void create(char** pcom)
{
	struct node* temp;

	temp = (struct node*)malloc(sizeof(struct node));
	temp->next = NULL;
    temp->prev = NULL;
    temp->pcom = pcom;
    temp->in = NULL;
    temp->out = NULL;

	if(head==NULL)  {
		head = temp;
	}

	else{
		struct node* ptr = head;
		while(ptr->next!=NULL)
		{
			ptr = ptr->next;
		}
		ptr->next = temp;
        temp->prev = ptr; //inserting at end of linked list
	}
}

int main(int argc, char *argv[])
{
    if (argc != 2){
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "r");
    if (file == NULL){
        printf("Error: Unable to open file %s\n", argv[1]);
        return 1;
    }

    char* line = NULL; // first line one string
    size_t len;
    char* tokens[250]; // Each word is element of line string
    int numTokens = 0;


    // for history command

    int histfd = open("history.txt", O_CREAT | O_TRUNC | O_RDWR , 0644 );


    // Read each line of script.sh

    while (getline(&line, &len, file) != -1){


        // Echoing back to the user and writing line to history.txt

        printf("Command:\n%s\nOutput:\n", line);
        write(histfd, line, strlen(line));


        // Tokenizing

        char* token = strtok(line, " ");
        while(token!= NULL){
            tokens[numTokens] = strdup(token);
            numTokens++;
            token = strtok(NULL, " ");
        }
        tokens[numTokens] = "!";
        
        int len = strlen(tokens[numTokens - 1]);
        if (len > 0 && tokens[numTokens - 1][len - 1] == '\n') {
            tokens[numTokens - 1][len - 1] = '\0'; // Replace '\n' with '\0'
        }

    
        // Creating command nodes

        char* queue[100]; // ";;|;!" It will consist of ; | and !
        int q = 0;
        int z = -1;
        for (int i=0 ; i < numTokens + 1 ; i++ ){
            if (strcmp(tokens[i], ";") == 0 | strcmp(tokens[i], "|") == 0 | strcmp(tokens[i], "!") == 0){
                queue[q] = tokens[i];
                q++;

                // pcom

                char ** newarr = malloc((i - z) * sizeof(char*));
                for (int j = 0; j < i - z - 1; j++){
                    newarr[j] = strdup(tokens[z + j + 1]);
                }
                newarr[i-z-1] = NULL;

                // create

                create(newarr);

                z = i;
            }
        }


        // In order to find pipes and terminator

        int pipefinder[100]; // "3,6,8", the last element is for !
        int countpipe = 0;

        for (int i = 0; i < q; i++)
        {
            if (strcmp(queue[i], "|") == 0 | strcmp(queue[i], "!") == 0){

                pipefinder[countpipe] = i + 1;
                countpipe++;

            }
        }
        

        // Piping nodes each other

        struct node* nowptr = head;
        int npipes[10][2];

        for ( int i=0 ; i < countpipe-1 ; i++ ){

            pipe(npipes[i]);

            if (i == 0){
                for (int j=0; j < pipefinder[i]; j++){
                    nowptr->out = &npipes[i][1];
                    nowptr = nowptr->next;
                }
            }
            else {
                for (int j=0; j < pipefinder[i]-pipefinder[i-1]; j++){
                    nowptr->out = &npipes[i][1];
                    nowptr = nowptr->next;
                }
            }

            struct node* sarptr = nowptr;

            for (int j=0; j < pipefinder[i+1]-pipefinder[i] ; j++){
                nowptr->in = &npipes[i][0];
                nowptr = nowptr->next;
            }

            nowptr = sarptr;
            
        }


        // Pre-adjustments

        nowptr = head;
        int counter = 0;


        // buffer.txt is for commands which reads pipe 

        int fd = open("buffer.txt", O_RDWR | O_CREAT, 0644 );

        while(1){

            int rc = fork();

            if (rc < 0) {
                fprintf(stderr, "fork failed\n");
                exit(1);
            } 
            
            else if (rc == 0) {


                // If child do not have to pipe

                if (nowptr->in == NULL && nowptr->out == NULL ){

                    execvp(nowptr->pcom[0], nowptr->pcom );
                    return 0;

                }
                

                // If child only writes to pipe's write end

                else if (nowptr->in == NULL && nowptr->out != NULL){

                    close( *(nowptr->out - 1) );

                    if (dup2( *(nowptr->out) , STDOUT_FILENO ) == -1){
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }

                    execvp(nowptr->pcom[0], nowptr->pcom );
                    return 0;

                }


                // If child only reads pipe's read end

                else if (nowptr->in != NULL && nowptr->out == NULL){
                    
                    int sd = open("buffer.txt", O_RDWR | O_CREAT, 0644 );

                    if (dup2( sd , STDIN_FILENO ) == -1){
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }

                    execvp(nowptr->pcom[0], nowptr->pcom );
                    return 0;
                    
                }


                //If child is in middle of two pipe operator 

                else {

                    close( *(nowptr->out - 1) );
                    if (dup2( *(nowptr->out) , STDOUT_FILENO ) == -1){
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }

                    close( *(nowptr->in + 1) );
                    if (dup2( *(nowptr->in) , STDIN_FILENO ) == -1){
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }

                    execvp(nowptr->pcom[0], nowptr->pcom );
                    return 0;
                }
            } 
            
            else {
                

                // quit operation

                if ((nowptr->next) && (strcmp(nowptr->next->pcom[0], "quit") == 0)){
                    pid_t wpid;
                    int status = 0;
                    while((wpid = wait(&status)) > 0);
                    return 0;
                }
                

                // If next operator is ;

                if (strcmp(queue[counter],";") == 0){
                    nowptr = nowptr->next;
                }


                // If next operator is |

                else if (strcmp(queue[counter], "|") == 0){

                    pid_t wpid;
                    int status = 0;
                    while((wpid = wait(&status)) > 0); // Wait for pre-pipe processes

                    nowptr = nowptr->next;

                    // Some annoying adjustments to read from pipe and write to buffer.txt

                    int flags = fcntl(*(nowptr->in), F_GETFL, 0);
                    fcntl(*(nowptr->in), F_SETFL, flags | O_NONBLOCK);

                    char buffer[1024];
                    ssize_t byte_read;
                    
                    byte_read = read(*(nowptr->in), buffer, sizeof(buffer));
                    if (byte_read > 0){
                        write(fd, buffer, byte_read);
                    }

                }


                // If next operator is ! (which means the end of line)

                else {

                    pid_t wpid;
                    int status = 0;
                    while((wpid = wait(&status)) > 0); // Wait for all childs to finish

                    if (strcmp(nowptr->pcom[0], "quit") == 0){

                        return 0; // Simply quit

                    }

                    if (strcmp(nowptr->pcom[0], "cd") == 0){
                        
                        chdir(nowptr->pcom[1]); // Simply change current directory

                    }

                    if (strcmp(nowptr->pcom[0], "history") == 0){

                        int histtfd = open("history.txt", O_RDONLY, 0644 );
                        ssize_t bytes_read;
                        char hbuffer[256];

                        while ((bytes_read = read(histtfd, hbuffer, sizeof(hbuffer))) > 0) {
                            write(STDOUT_FILENO, hbuffer, bytes_read); // Read history and write it to standard output
                        }

                    }

                    break; // Exit the while
                }

                counter++;
                
            }
        }

        printf("--------------------\n");
        for (int i = 0 ; i < numTokens ; i++) {
            free(tokens[i]);
        }

        numTokens = 0;
        head = NULL;
    }

    return 0;
}