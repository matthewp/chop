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
    fprintf(stderr, "  add <text>            Emit a new todo line\n");
    fprintf(stderr, "  done [id]             Mark todo as done in stream\n");
    fprintf(stderr, "  status <status> [id]  Set status in stream\n");
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  --todo                Filter: show only pending\n");
    fprintf(stderr, "  --done                Filter: show only done\n");
    fprintf(stderr, "  --in-progress         Filter: show only in-progress\n");
    fprintf(stderr, "  -v, --version         Show version\n");
    fprintf(stderr, "  -h, --help            Show this help message\n");
    fprintf(stderr, "\nExamples:\n");
    fprintf(stderr, "  cat todos.txt | %s                     # format all\n", prog);
    fprintf(stderr, "  cat todos.txt | %s --in-progress       # filter to in-progress\n", prog);
    fprintf(stderr, "  cat todos.txt | %s done 3 | sponge todos.txt\n", prog);
    fprintf(stderr, "  %s add \"Buy milk\" >> todos.txt\n", prog);
}

/* Format/filter stdin to stdout */
static int cmd_filter(int filter, TodoStatus filter_status) {
    TodoList *list = todolist_new();
    if (!list) {
        fprintf(stderr, "Failed to allocate memory\n");
        return 1;
    }

    /* Read from stdin */
    char line[1024];
    while (fgets(line, sizeof(line), stdin)) {
        if (list->count >= list->capacity) {
            /* Grow the list */
            size_t new_cap = list->capacity * 2;
            Todo *new_items = realloc(list->items, sizeof(Todo) * new_cap);
            if (!new_items) {
                fprintf(stderr, "Failed to allocate memory\n");
                todolist_free(list);
                return 1;
            }
            list->items = new_items;
            list->capacity = new_cap;
        }

        Todo *todo = &list->items[list->count];
        memset(todo, 0, sizeof(Todo));
        todo->raw_line = strdup(line);

        /* Try to parse as todo */
        char *p = line;
        while (*p && (*p == ' ' || *p == '\t')) p++;

        /* Skip empty lines */
        size_t len = strlen(p);
        while (len > 0 && (p[len-1] == '\n' || p[len-1] == '\r' || p[len-1] == ' ')) len--;
        if (len == 0) {
            list->count++;
            continue;
        }

        todo->status = STATUS_TODO;
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
                todo->id = list->count + 1;
            }
        }

        list->count++;
    }

    /* Output */
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

/* Emit a new todo line */
static int cmd_add(int argc, char **argv, int arg_start) {
    if (arg_start >= argc) {
        /* Read text from stdin */
        char line[1024];
        while (fgets(line, sizeof(line), stdin)) {
            size_t len = strlen(line);
            while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) len--;
            if (len > 0) {
                printf("- [ ] %.*s\n", (int)len, line);
            }
        }
    } else {
        /* Join args as text */
        char text[1024] = {0};
        for (int i = arg_start; i < argc; i++) {
            if (text[0]) strcat(text, " ");
            strncat(text, argv[i], sizeof(text) - strlen(text) - 1);
        }
        printf("- [ ] %s\n", text);
    }
    return 0;
}

/* Modify status in stream */
static int cmd_status_stream(TodoStatus new_status, int target_id) {
    TodoList *list = todolist_new();
    if (!list) {
        fprintf(stderr, "Failed to allocate memory\n");
        return 1;
    }

    /* Read from stdin */
    char line[1024];
    int id = 1;
    while (fgets(line, sizeof(line), stdin)) {
        if (list->count >= list->capacity) {
            size_t new_cap = list->capacity * 2;
            Todo *new_items = realloc(list->items, sizeof(Todo) * new_cap);
            if (!new_items) {
                fprintf(stderr, "Failed to allocate memory\n");
                todolist_free(list);
                return 1;
            }
            list->items = new_items;
            list->capacity = new_cap;
        }

        Todo *todo = &list->items[list->count];
        memset(todo, 0, sizeof(Todo));
        todo->raw_line = strdup(line);

        /* Try to parse as todo */
        char *p = line;
        while (*p && (*p == ' ' || *p == '\t')) p++;

        /* Skip empty lines */
        size_t len = strlen(p);
        while (len > 0 && (p[len-1] == '\n' || p[len-1] == '\r' || p[len-1] == ' ')) len--;
        if (len == 0) {
            list->count++;
            continue;
        }

        todo->status = STATUS_TODO;
        char *text_start = p;

        if ((*p == '-' || *p == '*' || *p == '+') && p[1] == ' ') {
            p += 2;
            while (*p == ' ') p++;
            text_start = p;

            if (p[0] == '[' && p[2] == ']') {
                if (p[1] == 'x' || p[1] == 'X') todo->status = STATUS_DONE;
                else if (p[1] == '>') todo->status = STATUS_IN_PROGRESS;
                else todo->status = STATUS_TODO;
                p += 3;
                while (*p == ' ') p++;
                text_start = p;
            }
        }

        len = strlen(text_start);
        while (len > 0 && (text_start[len-1] == '\n' || text_start[len-1] == '\r')) len--;

        if (len > 0) {
            todo->text = malloc(len + 1);
            if (todo->text) {
                memcpy(todo->text, text_start, len);
                todo->text[len] = '\0';
                todo->id = id++;

                /* Apply status change if this is the target */
                if (target_id > 0 && todo->id == target_id) {
                    todo->status = new_status;
                }
            }
        }

        list->count++;
    }

    /* Output */
    for (size_t i = 0; i < list->count; i++) {
        Todo *todo = &list->items[i];
        if (todo->text) {
            fprintf(stdout, "- [%c] %s\n", status_to_char(todo->status), todo->text);
        }
    }

    todolist_free(list);
    return 0;
}

int main(int argc, char **argv) {
    int filter = 0;
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

    if (strcmp(cmd, "add") == 0) {
        return cmd_add(argc, argv, i);
    } else if (strcmp(cmd, "done") == 0) {
        int id = 0;
        if (i < argc) {
            id = atoi(argv[i]);
        }
        if (id <= 0) {
            fprintf(stderr, "done requires an ID argument\n");
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
        if (i < argc) {
            id = atoi(argv[i]);
        }
        if (id <= 0) {
            fprintf(stderr, "status requires an ID argument\n");
            return 1;
        }
        return cmd_status_stream(new_status, id);
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        usage(argv[0]);
        return 1;
    }
}
