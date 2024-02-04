#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/personality.h>

void greet(void);
int transform_stack(void);

const char *synopsis = "\
Synopsis: ropchain <stack|greet>.\n\
If \"stack\" has been passed, the program tries to open \"stack.txt\" and parse the lines as hexadecimal values and puts the converted values into \"stack.bin\".\n\
If \"greet\" has been passed, the program will totally safely ask you for your name to greet you ;)\n\
";

int main(int argc, char **argv) {
    if (argc == 2) {
        if (!strcmp(argv[1], "greet")) {
            greet();
        } else if (!strcmp(argv[1], "stack")) {
            if (transform_stack()) {
                printf("Error opening file: %s\n", strerror(errno));
            }
        } else {
            printf("%s", synopsis);
        }
    } else {
        printf("%s", synopsis);
    }
    return 0;
}

__attribute__((noinline))
void greet(void) {
    char name[32];
    printf("Please write your name: ");
    scanf("%s", name);
    printf("Hello %s!\n", name);
}

int transform_stack(void) {
    FILE *input = fopen("stack.txt", "r");
    if (!input) {
        return -1;
    }
    FILE *output = fopen("stack.bin", "w");
    if (!output) {
        fclose(input);
        return -1;
    }
    
    // initialize array and current line for loop
    size_t *bytes = malloc(sizeof(size_t));
    size_t capacity = 1;
    size_t element_num = 0;
    char *current_line = NULL;
    size_t line_length = 0;

    // convert strings to bytes
    while (getline(&current_line, &line_length, input) > 0) {
        // actual parsing
        size_t current;
        sscanf(current_line, " 0x%lx", &current);

        // resize array if to small
        if (element_num >= capacity) {
            capacity *= 2;
            bytes = realloc(bytes, sizeof(size_t) * capacity);
        }

        // adding value to bytes
        bytes[element_num] = current;
        element_num++;
    }

    // write output
    fwrite(bytes, sizeof(size_t), element_num, output);
    
    // cleanup
    fclose(input);
    fclose(output);
    free(current_line);
    free(bytes);
    return 0;
}
