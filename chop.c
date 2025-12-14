#include "chop.h"
#include <string.h>
#include <ctype.h>

#define INITIAL_CAPACITY 16

TodoList *todolist_new(void) {
    TodoList *list = malloc(sizeof(TodoList));
    if (!list) return NULL;

    list->items = malloc(sizeof(Todo) * INITIAL_CAPACITY);
    if (!list->items) {
        free(list);
        return NULL;
    }

    list->count = 0;
    list->capacity = INITIAL_CAPACITY;
    return list;
}

void todolist_free(TodoList *list) {
    if (!list) return;

    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i].text);
        free(list->items[i].raw_line);
    }
    free(list->items);
    free(list);
}

static int todolist_grow(TodoList *list) {
    size_t new_cap = list->capacity * 2;
    Todo *new_items = realloc(list->items, sizeof(Todo) * new_cap);
    if (!new_items) return -1;

    list->items = new_items;
    list->capacity = new_cap;
    return 0;
}

static TodoStatus parse_status_marker(const char *marker) {
    if (strcmp(marker, "[ ]") == 0) return STATUS_TODO;
    if (strcmp(marker, "[x]") == 0 || strcmp(marker, "[X]") == 0) return STATUS_DONE;
    if (strcmp(marker, "[>]") == 0) return STATUS_IN_PROGRESS;
    return STATUS_TODO;
}

static int parse_line(const char *line, Todo *todo, int id) {
    /* Skip leading whitespace */
    while (*line && isspace(*line)) line++;

    /* Skip empty lines */
    if (*line == '\0' || *line == '\n') return -1;

    /* Check for list marker "- " */
    if (line[0] != '-' || line[1] != ' ') return -1;
    line += 2;

    /* Skip whitespace after marker */
    while (*line && isspace(*line)) line++;

    /* Parse status [x], [ ], [>] */
    if (line[0] != '[') return -1;

    char marker[4] = {0};
    strncpy(marker, line, 3);
    marker[3] = '\0';

    todo->status = parse_status_marker(marker);
    line += 3;

    /* Skip whitespace after status */
    while (*line && isspace(*line)) line++;

    /* Rest is the text (trim trailing newline) */
    size_t len = strlen(line);
    while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
        len--;
    }

    todo->text = malloc(len + 1);
    if (!todo->text) return -1;

    strncpy(todo->text, line, len);
    todo->text[len] = '\0';
    todo->id = id;

    return 0;
}

int todolist_parse_file(TodoList *list, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return -1;

    char line[1024];
    int id = 1;

    while (fgets(line, sizeof(line), f)) {
        if (list->count >= list->capacity) {
            if (todolist_grow(list) < 0) {
                fclose(f);
                return -1;
            }
        }

        Todo *todo = &list->items[list->count];
        memset(todo, 0, sizeof(Todo));

        todo->raw_line = strdup(line);

        if (parse_line(line, todo, id) == 0) {
            list->count++;
            id++;
        } else {
            /* Keep raw line for non-todo lines (comments, etc) */
            todo->text = NULL;
            todo->id = 0;
            list->count++;
        }
    }

    fclose(f);
    return 0;
}

int todolist_write_file(TodoList *list, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) return -1;

    for (size_t i = 0; i < list->count; i++) {
        Todo *todo = &list->items[i];

        if (todo->text) {
            fprintf(f, "- [%c] %s\n", status_to_char(todo->status), todo->text);
        } else if (todo->raw_line) {
            /* Preserve non-todo lines as-is */
            fputs(todo->raw_line, f);
        }
    }

    fclose(f);
    return 0;
}

int todolist_add(TodoList *list, const char *text) {
    if (list->count >= list->capacity) {
        if (todolist_grow(list) < 0) return -1;
    }

    /* Find max id */
    int max_id = 0;
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i].id > max_id) {
            max_id = list->items[i].id;
        }
    }

    Todo *todo = &list->items[list->count];
    todo->id = max_id + 1;
    todo->status = STATUS_TODO;
    todo->text = strdup(text);
    todo->raw_line = NULL;

    if (!todo->text) return -1;

    list->count++;
    return todo->id;
}

int todolist_set_status(TodoList *list, int id, TodoStatus status) {
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i].id == id) {
            list->items[i].status = status;
            return 0;
        }
    }
    return -1;
}

Todo *todolist_get(TodoList *list, int id) {
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i].id == id) {
            return &list->items[i];
        }
    }
    return NULL;
}

char status_to_char(TodoStatus status) {
    switch (status) {
        case STATUS_DONE: return 'x';
        case STATUS_IN_PROGRESS: return '>';
        default: return ' ';
    }
}

const char *status_to_str(TodoStatus status) {
    switch (status) {
        case STATUS_DONE: return "done";
        case STATUS_IN_PROGRESS: return "in-progress";
        default: return "todo";
    }
}

TodoStatus status_from_str(const char *str) {
    if (strcmp(str, "done") == 0 || strcmp(str, "x") == 0) return STATUS_DONE;
    if (strcmp(str, "in-progress") == 0 || strcmp(str, "progress") == 0 || strcmp(str, ">") == 0) return STATUS_IN_PROGRESS;
    return STATUS_TODO;
}

void todo_print(Todo *todo, FILE *out) {
    if (!todo || !todo->text) return;
    fprintf(out, "%d\t[%c] %s\n", todo->id, status_to_char(todo->status), todo->text);
}

void todolist_print(TodoList *list, FILE *out) {
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i].text) {
            todo_print(&list->items[i], out);
        }
    }
}

void todolist_print_filtered(TodoList *list, FILE *out, TodoStatus status) {
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i].text && list->items[i].status == status) {
            todo_print(&list->items[i], out);
        }
    }
}
