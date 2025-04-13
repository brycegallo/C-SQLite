#include <stdbool.h> // Gives us true
#include <stdio.h>  // Gives us size_t, stdin, printf()
#include <stdlib.h> // Gives us malloc()
#include <string.h> // Gives us strcmp()

/*** Type Definitions ***/

// We'll define InputBuffer as a small wrapper around the state we need to store to interact with getline()
typedef struct {
    char* buffer;
    size_t buffer_length;
    ssize_t input_length;
} InputBuffer;

// This function creates a new InputBuffer*
InputBuffer* new_input_buffer() {
    InputBuffer* input_buffer = (InputBuffer*)malloc(sizeof(InputBuffer));
    input_buffer->buffer = NULL;
    input_buffer->buffer_length = 0;
    input_buffer->input_length = 0;

    return input_buffer;
}

// This prints a prompt to the user, we'll call this before reading each line of input
void print_prompt() {
    printf("db > ");
}

void read_input(InputBuffer* input_buffer) {
    /***
     * ssize_t getline(char **lineptr, size_t *n, FILE *stream);
     * lineptr: a pointer to the variable we use to point to the buffer containing the read line
     * // If lineptr is set to NULL, it is m-allocated by getline() and should still be freed by the user even if the command fails
     * n: a pointer to the variable used to save the size of the allocated buffer
     * stream: the input stream we read from (in this case: standard input)
     * return value: the number of bytes read, which may be less than the size of the buffer ***/
    // Here we have getline() store the read line in input_buffer->buffer and the size of the allocated buffer in input_buffer->buffer_length. We store the return value in input_buffer->input_length
    // We initially set buffer to NULL, so getline() allocates enough memory to hold the line of input and makes buffer point to it
    ssize_t bytes_read = getline(&(input_buffer->buffer), &(input_buffer->buffer_length), stdin);

    if (bytes_read <= 0) {
	printf("Error reading input\n");
	exit(EXIT_FAILURE);
    }

    // Ignore trailing newline which is automatically included in the input string
    input_buffer->input_length = bytes_read - 1;
    input_buffer->buffer[bytes_read - 1] = 0;
}

// We need to free the memory allocated for an instance if InputBuffer* and the buffer element of the respective structure. Remember that getline() allocates memory for input_buffer->buffer in read_input
void close_input_buffer(InputBuffer* input_buffer) {
    free(input_buffer->buffer);
    free(input_buffer);
}

int main (int argc, char* argv[]) {
    (void)argc; (void) argv; // prevent compiler warnings
    InputBuffer* input_buffer = new_input_buffer();
    // Print the prompt, get a line of input, then process that line of input
    while (true) {
	print_prompt();
	read_input(input_buffer);

	if (strcmp(input_buffer->buffer, ".exit") == 0) {
	    close_input_buffer(input_buffer);
	    exit(EXIT_SUCCESS);
	} else {
	    printf("Unrecognized command '%s' .\n", input_buffer->buffer);
	}
    }
}
