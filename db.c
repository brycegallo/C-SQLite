#include <stdbool.h> // Gives us true
#include <stdint.h> 
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

typedef enum {
    EXECUTE_SUCCESS,
    EXECUTE_TABLE_FULL
} ExecuteResult;

typedef enum {
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED_COMMAND
} MetaCommandResult;

typedef enum {
    PREPARE_SUCCESS,
    PREPARE_SYNTAX_ERROR,
    PREPARE_UNRECOGNIZED_STATEMENT
} PrepareResult;

typedef enum {
    STATEMENT_INSERT,
    STATEMENT_SELECT
} StatementType;

#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255
typedef struct {
    uint32_t id;
    char username[COLUMN_USERNAME_SIZE];
    char email[COLUMN_EMAIL_SIZE];
} Row;

typedef struct {
    StatementType type;
    Row row_to_insert; // only used by insert statemnet
} Statement;

// a #define that will help us define the compact representation of a row with the consts below
#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute)

// The consts that will let us define the compact representation of a row
const uint32_t ID_SIZE = size_of_attribute(Row, id);
const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);
const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);
const uint32_t ID_OFFSET = 0;
const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
const uint32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
const uint32_t ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;

const uint32_t PAGE_SIZE = 4096; // 4096 bytes, 4kb, is the same size as a page used in the virtual memory system of most computer architectures. One page in our database corresponds to one page used by the operating system. The OS will move pages in and out of memory as whole units instead of breaking them up
#define TABLE_MAX_PAGES 100 // We're setting this limit arbitrarily
const uint32_t ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;
const uint32_t TABLE_MAX_ROWS = ROWS_PER_PAGE * TABLE_MAX_PAGES;

// a Table structure that points to pages of rows and keeps track of how many rows there are
typedef struct {
    uint32_t num_rows;
    void* pages[TABLE_MAX_PAGES];
} Table;

/*** Functions ***/

void print_row(Row* row) {
    printf("(%d , %s, %s)\n", row->id, row->username, row->email);
}

// This function and the one below will convert to and from the compact representation of serialized rows
void serialize_row(Row* source, void* destination) {
    memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
    memcpy(destination + USERNAME_OFFSET, &(source->username), USERNAME_SIZE);
    memcpy(destination + EMAIL_OFFSET, &(source->email), EMAIL_SIZE);
}

void deserialize_row(void* source, Row* destination) {
    memcpy(&(destination->id), source + ID_OFFSET, ID_SIZE);
    memcpy(&(destination->username), source + USERNAME_OFFSET, USERNAME_SIZE);
    memcpy(&(destination->email), source + EMAIL_OFFSET, EMAIL_SIZE);
}

// This function will let us figure out where to read and write in memory for a particular row
void* row_slot(Table* table, uint32_t row_num) {
    uint32_t page_num = row_num / ROWS_PER_PAGE;
    void* page = table->pages[page_num];
    if (page == NULL) {
	// Allocate memory only when we try to access page
	page = table->pages[page_num] = malloc(PAGE_SIZE);
    }
    uint32_t row_offset = row_num % ROWS_PER_PAGE;
    uint32_t byte_offset = row_offset * ROW_SIZE;
    return page + byte_offset;
}

// Initialize a new empty table while allocating the necessary memory for it
Table* new_table(void) {
    Table* table = (Table*)malloc(sizeof(Table));
    table->num_rows = 0;
    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
	table->pages[i] = NULL;
    }
    return table;
}

// Free the memory allocated to a table
void free_table(Table* table) {
    for (int i = 0; table->pages[i]; i++) {
	free(table->pages[i]);
    }
    free(table);
}

// This function creates a new InputBuffer*
InputBuffer* new_input_buffer() {
    InputBuffer* input_buffer = (InputBuffer*)malloc(sizeof(InputBuffer));
    input_buffer->buffer = NULL;
    input_buffer->buffer_length = 0;
    input_buffer->input_length = 0;

    return input_buffer;
}

// This is just a wrapper for existing functionality in main, but leaves room for more commands
MetaCommandResult do_meta_command(InputBuffer* input_buffer) {
    if (strcmp(input_buffer->buffer, ".exit") == 0) {
	exit(EXIT_SUCCESS);
    } else {
	return META_COMMAND_UNRECOGNIZED_COMMAND;
    }
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

// Function we will use to parse arguments, which will be stored into a Row data structure inside the statement object
PrepareResult prepare_statement(InputBuffer* input_buffer, Statement* statement) {
    // We use strncmp() for "insert" because that keyword will be followed by data
    if (strncmp(input_buffer->buffer, "insert", 6) == 0) {
	statement->type = STATEMENT_INSERT;
	int args_assigned = sscanf(
		input_buffer->buffer, "insert %d %s %s", &(statement->row_to_insert.id),
		statement->row_to_insert.username, statement->row_to_insert.email);
	if (args_assigned < 3) {
	    return PREPARE_SYNTAX_ERROR;
	}
	return PREPARE_SUCCESS;
    }
    if (strcmp(input_buffer->buffer, "select") == 0) {
	statement->type = STATEMENT_SELECT;
	return PREPARE_SUCCESS;
    }

    return PREPARE_UNRECOGNIZED_STATEMENT;
}

ExecuteResult execute_insert(Statement* statement, Table* table) {
    if (table->num_rows >= TABLE_MAX_ROWS) {
	return EXECUTE_TABLE_FULL;
    }

    Row* row_to_insert = &(statement->row_to_insert);

    serialize_row(row_to_insert, row_slot(table, table->num_rows));
    table->num_rows += 1;

    return EXECUTE_SUCCESS;
}

ExecuteResult execute_select(Statement* statement, Table* table) {
    Row row;
    for (uint32_t i = 0; i < table->num_rows; i++) {
	deserialize_row(row_slot(table, i), &row);
	print_row(&row);
    }
    return EXECUTE_SUCCESS;
}

// Reads and writes from our table structure
ExecuteResult execute_statement(Statement* statement, Table* table) {
    switch (statement->type) {
	case (STATEMENT_INSERT):
	    return execute_insert(statement, table);
	case (STATEMENT_SELECT):
	    return execute_select(statement, table);
    }
}

int main (int argc, char* argv[]) {
    (void)argc; (void) argv; // prevent compiler warnings
    Table* table = new_table();
    InputBuffer* input_buffer = new_input_buffer();
    // Print the prompt, get a line of input, then process that line of input
    while (true) {
	print_prompt();
	read_input(input_buffer);
	
	// Here we check for non-SQL statements (called "meta-commands") which all start with a . and prepare to handle them in a separate function
	if (input_buffer->buffer[0] == '.') {
	    switch (do_meta_command(input_buffer)) {
		case (META_COMMAND_SUCCESS):
		    continue;
		case (META_COMMAND_UNRECOGNIZED_COMMAND):
		    printf("Unrecognized command '%s' \n", input_buffer->buffer);
		    continue;
	    }
	}
	// Here we convert the line of input into our internal representation of a statement
	Statement statement;
	switch (prepare_statement(input_buffer, &statement)) {
	    case (PREPARE_SUCCESS):
		break;
	    case (PREPARE_SYNTAX_ERROR):
		printf("Syntax error. Could not parse statement.\n");
		continue;
	    case (PREPARE_UNRECOGNIZED_STATEMENT):
		printf("Unrecognized keyword at start of '%s'.\n", input_buffer->buffer);
		continue;
	}

	// We pass the statement output of the above conversion into a function that will act as our virtual machine
	switch (execute_statement(&statement, table)) {
	    case (EXECUTE_SUCCESS):
		printf("Executed.\n");
		break;
	    case (EXECUTE_TABLE_FULL):
		printf("Error: Table full.\n");
		break;
	}
    }
    // exit(EXIT_SUCCESS);
}
