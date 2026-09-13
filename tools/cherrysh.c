#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOK_DELIM " \n\t\r\a"
#define BUFF_SIZE 64
int main(int argv, char **argc) {
  while (1) {
    // get the command
    char *line;
    size_t lineSize;
    size_t size = getline(&line, &lineSize, stdin);
    if (size < 0) {
      if (feof(stdin))
        exit(EXIT_SUCCESS); // ctrl+D
      else {
        exit(EXIT_FAILURE);
      }
    }

    // tokenize the command
    int buffsize = BUFF_SIZE;
    char **arguments = malloc(buffsize * sizeof(char *));
    char *token = strtok(line, TOK_DELIM);
    int position = 0;
    while (token != NULL) {
      arguments[position] = token;
      position++;

      if (position >= BUFF_SIZE) {
        buffsize += BUFF_SIZE;
        arguments = realloc(arguments, buffsize * sizeof(char *));
        if (!arguments) {
          printf("reallocation error");
          exit(EXIT_FAILURE);
        }
      }

      token = strtok(NULL, TOK_DELIM);
    }

    arguments[position] = NULL;

    printf("Arguments: ");

    // TODO: pass the Execution to the respective binary(custom command);
  }
}
