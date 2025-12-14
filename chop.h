#ifndef CHOP_H
#define CHOP_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef enum {
    STATUS_TODO,
    STATUS_DONE,
    STATUS_IN_PROGRESS
} TodoStatus;

typedef struct {
    int id;
    TodoStatus status;
    char *text;
    char *raw_line;
} Todo;

typedef struct {
    Todo *items;
    size_t count;
    size_t capacity;
} TodoList;

/* Parsing */
TodoList *todolist_new(void);
void todolist_free(TodoList *list);
int todolist_parse_file(TodoList *list, const char *filename);
int todolist_write_file(TodoList *list, const char *filename);

/* Manipulation */
int todolist_add(TodoList *list, const char *text);
int todolist_set_status(TodoList *list, int id, TodoStatus status);
Todo *todolist_get(TodoList *list, int id);

/* Output */
void todo_print(Todo *todo, FILE *out);
void todolist_print(TodoList *list, FILE *out);
void todolist_print_filtered(TodoList *list, FILE *out, TodoStatus status);

/* Utilities */
const char *status_to_str(TodoStatus status);
TodoStatus status_from_str(const char *str);
char status_to_char(TodoStatus status);

#endif
