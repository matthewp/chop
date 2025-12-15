#include "chop.h"
#include <string.h>
#include <unistd.h>

#ifndef VERSION
#define VERSION "devel"
#endif

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options] [command] [args]\n", prog);
    fprintf(stderr, "\nStream filter for todo lists. Reads stdin, writes stdout.\n");
    fprintf(stderr, "\nCommands:\n");
    fprintf(stderr, "  (none)                Filter/format todos from stdin\n");
    fprintf(stderr, "  start [id]            Mark todo as in-progress\n");
    fprintf(stderr, "  done [id]             Mark todo as done\n");
    fprintf(stderr, "  status <status> [id]  Set status (todo, done, in-progress)\n");
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  --todo                Filter: show only pending\n");
    fprintf(stderr, "  --done                Filter: show only done\n");
    fprintf(stderr, "  --in-progress         Filter: show only in-progress\n");
    fprintf(stderr, "  --fzf                 Select interactively with fzf\n");
    fprintf(stderr, "  -v, --version         Show version\n");
    fprintf(stderr, "  -h, --help            Show this help message\n");
    fprintf(stderr, "\nExamples:\n");
    fprintf(stderr, "  cat todos.txt | %s                     # format all\n", prog);
    fprintf(stderr, "  cat todos.txt | %s --in-progress       # filter to in-progress\n", prog);
    fprintf(stderr, "  cat todos.txt | %s done --fzf | sponge todos.txt\n", prog);
    fprintf(stderr, "  echo \"Buy milk\" | %s >> todos.txt\n", prog);
}

/* Helper to parse a line into a todo */
static void parse_todo_line(char *line, Todo *todo, int id) {
    todo->raw_line = strdup(line);
    todo->status = STATUS_TODO;
    todo->text = NULL;
    todo->id = 0;

    char *p = line;
    while (*p && (*p == ' ' || *p == '\t')) p++;

    /* Skip empty lines */
    size_t len = strlen(p);
    while (len > 0 && (p[len-1] == '\n' || p[len-1] == '\r' || p[len-1] == ' ')) len--;
    if (len == 0) return;

    char *text_start = p;

    /* Check for list marker */
    if ((*p == '-' || *p == '*' || *p == '+') && p[1] == ' ') {
        p += 2;
        while (*p == ' ') p++;
        text_start = p;

        /* Check for status marker */
        if (p[0] == '[' && p[2] == ']') {
            if (p[1] == 'x' || p[1] == 'X') todo->status = STATUS_DONE;
            else if (p[1] == '>') todo->status = STATUS_IN_PROGRESS;
            else todo->status = STATUS_TODO;
            p += 3;
            while (*p == ' ') p++;
            text_start = p;
        }
    }

    /* Extract text */
    len = strlen(text_start);
    while (len > 0 && (text_start[len-1] == '\n' || text_start[len-1] == '\r')) len--;

    if (len > 0) {
        todo->text = malloc(len + 1);
        if (todo->text) {
            memcpy(todo->text, text_start, len);
            todo->text[len] = '\0';
            todo->id = id;
        }
    }
}

/* Helper to grow list */
static int grow_list(TodoList *list) {
    size_t new_cap = list->capacity * 2;
    Todo *new_items = realloc(list->items, sizeof(Todo) * new_cap);
    if (!new_items) return -1;
    list->items = new_items;
    list->capacity = new_cap;
    return 0;
}

/* Read todos from stdin into list */
static TodoList *read_todos_from_stdin(void) {
    TodoList *list = todolist_new();
    if (!list) return NULL;

    char line[1024];
    int id = 1;
    while (fgets(line, sizeof(line), stdin)) {
        if (list->count >= list->capacity) {
            if (grow_list(list) < 0) {
                todolist_free(list);
                return NULL;
            }
        }

        Todo *todo = &list->items[list->count];
        memset(todo, 0, sizeof(Todo));
        parse_todo_line(line, todo, id);
        if (todo->text) id++;
        list->count++;
    }

    return list;
}

/* Output all todos */
static void output_todos(TodoList *list) {
    for (size_t i = 0; i < list->count; i++) {
        Todo *todo = &list->items[i];
        if (todo->text) {
            fprintf(stdout, "- [%c] %s\n", status_to_char(todo->status), todo->text);
        }
    }
}

/* Format/filter stdin to stdout */
static int cmd_filter(int filter, TodoStatus filter_status) {
    TodoList *list = read_todos_from_stdin();
    if (!list) {
        fprintf(stderr, "Failed to allocate memory\n");
        return 1;
    }

    for (size_t i = 0; i < list->count; i++) {
        Todo *todo = &list->items[i];
        if (todo->text) {
            if (!filter || todo->status == filter_status) {
                fprintf(stdout, "- [%c] %s\n", status_to_char(todo->status), todo->text);
            }
        }
    }

    todolist_free(list);
    return 0;
}

/* Modify status in stream by ID */
static int cmd_status_stream(TodoStatus new_status, int target_id) {
    TodoList *list = read_todos_from_stdin();
    if (!list) {
        fprintf(stderr, "Failed to allocate memory\n");
        return 1;
    }

    /* Apply status change */
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i].id == target_id) {
            list->items[i].status = new_status;
            break;
        }
    }

    output_todos(list);
    todolist_free(list);
    return 0;
}

/* Modify status with fzf selection */
static int cmd_status_fzf(TodoStatus new_status) {
    TodoList *list = read_todos_from_stdin();
    if (!list) {
        fprintf(stderr, "Failed to allocate memory\n");
        return 1;
    }

    /* Build list of selectable todos */
    FILE *fzf = popen("fzf", "w+");
    if (!fzf) {
        /* Try without popen read - use temp approach */
        fzf = popen("fzf > /tmp/chop_fzf_out", "w");
        if (!fzf) {
            fprintf(stderr, "Failed to run fzf\n");
            todolist_free(list);
            return 1;
        }

        /* Write todos to fzf */
        for (size_t i = 0; i < list->count; i++) {
            Todo *todo = &list->items[i];
            if (todo->text) {
                fprintf(fzf, "- [%c] %s\n", status_to_char(todo->status), todo->text);
            }
        }
        pclose(fzf);

        /* Read selection */
        FILE *out = fopen("/tmp/chop_fzf_out", "r");
        if (out) {
            char selected[1024];
            while (fgets(selected, sizeof(selected), out)) {
                /* Find matching todo by text */
                size_t sel_len = strlen(selected);
                while (sel_len > 0 && (selected[sel_len-1] == '\n' || selected[sel_len-1] == '\r')) {
                    selected[--sel_len] = '\0';
                }

                for (size_t i = 0; i < list->count; i++) {
                    Todo *todo = &list->items[i];
                    if (todo->text) {
                        char line[1024];
                        snprintf(line, sizeof(line), "- [%c] %s", status_to_char(todo->status), todo->text);
                        if (strcmp(line, selected) == 0) {
                            todo->status = new_status;
                            break;
                        }
                    }
                }
            }
            fclose(out);
            unlink("/tmp/chop_fzf_out");
        }
    } else {
        pclose(fzf);
        fprintf(stderr, "Failed to run fzf\n");
        todolist_free(list);
        return 1;
    }

    output_todos(list);
    todolist_free(list);
    return 0;
}

int main(int argc, char **argv) {
    int filter = 0;
    int use_fzf = 0;
    TodoStatus filter_status = STATUS_TODO;

    /* Parse options */
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("%s\n", VERSION);
            return 0;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--todo") == 0) {
            filter = 1;
            filter_status = STATUS_TODO;
        } else if (strcmp(argv[i], "--done") == 0) {
            filter = 1;
            filter_status = STATUS_DONE;
        } else if (strcmp(argv[i], "--in-progress") == 0) {
            filter = 1;
            filter_status = STATUS_IN_PROGRESS;
        } else if (strcmp(argv[i], "--fzf") == 0) {
            use_fzf = 1;
        } else if (argv[i][0] == '-' && strcmp(argv[i], "-") != 0) {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        } else {
            break;
        }
    }

    /* No command - filter/format stdin */
    if (i >= argc) {
        return cmd_filter(filter, filter_status);
    }

    const char *cmd = argv[i++];

    if (strcmp(cmd, "start") == 0) {
        int id = 0;
        while (i < argc) {
            if (strcmp(argv[i], "--fzf") == 0) {
                use_fzf = 1;
            } else {
                id = atoi(argv[i]);
            }
            i++;
        }
        if (use_fzf) {
            return cmd_status_fzf(STATUS_IN_PROGRESS);
        }
        if (id <= 0) {
            fprintf(stderr, "start requires an ID argument (or use --fzf)\n");
            return 1;
        }
        return cmd_status_stream(STATUS_IN_PROGRESS, id);
    } else if (strcmp(cmd, "done") == 0) {
        int id = 0;
        while (i < argc) {
            if (strcmp(argv[i], "--fzf") == 0) {
                use_fzf = 1;
            } else {
                id = atoi(argv[i]);
            }
            i++;
        }
        if (use_fzf) {
            return cmd_status_fzf(STATUS_DONE);
        }
        if (id <= 0) {
            fprintf(stderr, "done requires an ID argument (or use --fzf)\n");
            return 1;
        }
        return cmd_status_stream(STATUS_DONE, id);
    } else if (strcmp(cmd, "status") == 0) {
        if (i >= argc) {
            fprintf(stderr, "status requires a status argument (todo, done, in-progress)\n");
            return 1;
        }
        TodoStatus new_status = status_from_str(argv[i++]);
        int id = 0;
        while (i < argc) {
            if (strcmp(argv[i], "--fzf") == 0) {
                use_fzf = 1;
            } else {
                id = atoi(argv[i]);
            }
            i++;
        }
        if (use_fzf) {
            return cmd_status_fzf(new_status);
        }
        if (id <= 0) {
            fprintf(stderr, "status requires an ID argument (or use --fzf)\n");
            return 1;
        }
        return cmd_status_stream(new_status, id);
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        usage(argv[0]);
        return 1;
    }
}
