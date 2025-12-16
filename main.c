#include "chop.h"
#include <string.h>
#include <unistd.h>

#ifndef VERSION
#define VERSION "devel"
#endif

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "\nStream filter for todo lists. Reads stdin, writes stdout.\n");
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  --include=STATUS  Include only STATUS (todo, done, in-progress)\n");
    fprintf(stderr, "  --exclude=STATUS  Exclude STATUS (todo, done, in-progress)\n");
    fprintf(stderr, "  --mark=STATUS     Mark all with status (todo, done, in-progress)\n");
    fprintf(stderr, "  --fzf             With --mark: select interactively\n");
    fprintf(stderr, "  -f, --file=FILE   Read from FILE instead of stdin\n");
    fprintf(stderr, "  -w, --write       Write back to FILE (requires -f)\n");
    fprintf(stderr, "  -v, --version     Show version\n");
    fprintf(stderr, "  -h, --help        Show this help message\n");
    fprintf(stderr, "\nShort forms:\n");
    fprintf(stderr, "  -it, -id, -iip    Include: todo, done, in-progress\n");
    fprintf(stderr, "  -xt, -xd, -xip    Exclude: todo, done, in-progress\n");
    fprintf(stderr, "  -mt, -md, -mip    Mark: todo, done, in-progress\n");
    fprintf(stderr, "\nExamples:\n");
    fprintf(stderr, "  cat todos.txt | %s                # format all\n", prog);
    fprintf(stderr, "  cat todos.txt | %s -it            # include pending only\n", prog);
    fprintf(stderr, "  cat todos.txt | %s -xd            # exclude done (clear finished)\n", prog);
    fprintf(stderr, "  cat todos.txt | %s -md | sponge todos.txt  # mark all done\n", prog);
    fprintf(stderr, "  %s -f todos.txt -xd -w            # clear done items in-place\n", prog);
    fprintf(stderr, "  echo \"Buy milk\" | %s >> todos.txt\n", prog);
}

/* Parse short status code: t=todo, d=done, ip=in-progress */
static int parse_status_code(const char *code, TodoStatus *status) {
    if (strcmp(code, "t") == 0 || strcmp(code, "todo") == 0) {
        *status = STATUS_TODO;
        return 0;
    } else if (strcmp(code, "d") == 0 || strcmp(code, "done") == 0) {
        *status = STATUS_DONE;
        return 0;
    } else if (strcmp(code, "ip") == 0 || strcmp(code, "in-progress") == 0) {
        *status = STATUS_IN_PROGRESS;
        return 0;
    }
    return -1;
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

/* Read todos from a file handle into list */
static TodoList *read_todos(FILE *in) {
    TodoList *list = todolist_new();
    if (!list) return NULL;

    char line[1024];
    int id = 1;
    while (fgets(line, sizeof(line), in)) {
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

/* Output all todos to a file handle */
static void output_todos(TodoList *list, FILE *out) {
    for (size_t i = 0; i < list->count; i++) {
        Todo *todo = &list->items[i];
        if (todo->text) {
            fprintf(out, "- [%c] %s\n", status_to_char(todo->status), todo->text);
        }
    }
}

/* Format/filter input to output */
static int cmd_filter(FILE *in, FILE *out, int do_include, TodoStatus include_status,
                      int do_exclude, TodoStatus exclude_status) {
    TodoList *list = read_todos(in);
    if (!list) {
        fprintf(stderr, "Failed to allocate memory\n");
        return 1;
    }

    for (size_t i = 0; i < list->count; i++) {
        Todo *todo = &list->items[i];
        if (todo->text) {
            int show = 1;
            if (do_include && todo->status != include_status) show = 0;
            if (do_exclude && todo->status == exclude_status) show = 0;
            if (show) {
                fprintf(out, "- [%c] %s\n", status_to_char(todo->status), todo->text);
            }
        }
    }

    todolist_free(list);
    return 0;
}

/* Modify status in stream - all items or specific ID */
static int cmd_status_stream(FILE *in, FILE *out, TodoStatus new_status, int target_id) {
    TodoList *list = read_todos(in);
    if (!list) {
        fprintf(stderr, "Failed to allocate memory\n");
        return 1;
    }

    /* Apply status change - all items if target_id is 0, otherwise specific ID */
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i].text) {
            if (target_id == 0 || list->items[i].id == target_id) {
                list->items[i].status = new_status;
            }
        }
    }

    output_todos(list, out);
    todolist_free(list);
    return 0;
}

/* Modify status with fzf selection */
static int cmd_status_fzf(FILE *in, FILE *out, TodoStatus new_status) {
    TodoList *list = read_todos(in);
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
        FILE *fzf_out = fopen("/tmp/chop_fzf_out", "r");
        if (fzf_out) {
            char selected[1024];
            while (fgets(selected, sizeof(selected), fzf_out)) {
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
            fclose(fzf_out);
            unlink("/tmp/chop_fzf_out");
        }
    } else {
        pclose(fzf);
        fprintf(stderr, "Failed to run fzf\n");
        todolist_free(list);
        return 1;
    }

    output_todos(list, out);
    todolist_free(list);
    return 0;
}

int main(int argc, char **argv) {
    int do_include = 0;
    int do_exclude = 0;
    int do_mark = 0;
    int use_fzf = 0;
    int do_write = 0;
    const char *file_path = NULL;
    TodoStatus include_status = STATUS_TODO;
    TodoStatus exclude_status = STATUS_TODO;
    TodoStatus mark_status = STATUS_TODO;

    /* Parse options */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("%s\n", VERSION);
            return 0;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--fzf") == 0) {
            use_fzf = 1;
        } else if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--write") == 0) {
            do_write = 1;
        } else if (strcmp(argv[i], "-f") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Missing argument for -f\n");
                return 1;
            }
            file_path = argv[++i];
        } else if (strncmp(argv[i], "--file=", 7) == 0) {
            file_path = argv[i] + 7;
        } else if (strncmp(argv[i], "--include=", 10) == 0) {
            if (parse_status_code(argv[i] + 10, &include_status) < 0) {
                fprintf(stderr, "Invalid include status: %s\n", argv[i] + 10);
                return 1;
            }
            do_include = 1;
        } else if (strncmp(argv[i], "--exclude=", 10) == 0) {
            if (parse_status_code(argv[i] + 10, &exclude_status) < 0) {
                fprintf(stderr, "Invalid exclude status: %s\n", argv[i] + 10);
                return 1;
            }
            do_exclude = 1;
        } else if (strncmp(argv[i], "--mark=", 7) == 0) {
            if (parse_status_code(argv[i] + 7, &mark_status) < 0) {
                fprintf(stderr, "Invalid mark status: %s\n", argv[i] + 7);
                return 1;
            }
            do_mark = 1;
        } else if (strcmp(argv[i], "-it") == 0) {
            do_include = 1;
            include_status = STATUS_TODO;
        } else if (strcmp(argv[i], "-id") == 0) {
            do_include = 1;
            include_status = STATUS_DONE;
        } else if (strcmp(argv[i], "-iip") == 0) {
            do_include = 1;
            include_status = STATUS_IN_PROGRESS;
        } else if (strcmp(argv[i], "-xt") == 0) {
            do_exclude = 1;
            exclude_status = STATUS_TODO;
        } else if (strcmp(argv[i], "-xd") == 0) {
            do_exclude = 1;
            exclude_status = STATUS_DONE;
        } else if (strcmp(argv[i], "-xip") == 0) {
            do_exclude = 1;
            exclude_status = STATUS_IN_PROGRESS;
        } else if (strcmp(argv[i], "-mt") == 0) {
            do_mark = 1;
            mark_status = STATUS_TODO;
        } else if (strcmp(argv[i], "-md") == 0) {
            do_mark = 1;
            mark_status = STATUS_DONE;
        } else if (strcmp(argv[i], "-mip") == 0) {
            do_mark = 1;
            mark_status = STATUS_IN_PROGRESS;
        } else if (argv[i][0] == '-' && strcmp(argv[i], "-") != 0) {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        } else {
            fprintf(stderr, "Unexpected argument: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    /* Validate mutually exclusive flags */
    if (do_include && do_exclude) {
        fprintf(stderr, "Cannot use --include and --exclude together\n");
        return 1;
    }
    if (do_write && !file_path) {
        fprintf(stderr, "--write requires --file\n");
        return 1;
    }

    /* Set up input/output */
    FILE *in = stdin;
    FILE *out = stdout;
    int result;

    if (file_path) {
        in = fopen(file_path, "r");
        if (!in) {
            fprintf(stderr, "Cannot open file: %s\n", file_path);
            return 1;
        }
    }

    /* For write mode, we need to buffer output then write to file */
    FILE *out_buffer = NULL;
    if (do_write) {
        out_buffer = tmpfile();
        if (!out_buffer) {
            fprintf(stderr, "Cannot create temporary file\n");
            if (file_path) fclose(in);
            return 1;
        }
        out = out_buffer;
    }

    /* Execute based on flags */
    if (do_mark) {
        if (use_fzf) {
            result = cmd_status_fzf(in, out, mark_status);
        } else {
            result = cmd_status_stream(in, out, mark_status, 0);
        }
    } else {
        result = cmd_filter(in, out, do_include, include_status, do_exclude, exclude_status);
    }

    if (file_path) fclose(in);

    /* Write buffer back to file if requested */
    if (do_write && result == 0) {
        rewind(out_buffer);
        FILE *out_file = fopen(file_path, "w");
        if (!out_file) {
            fprintf(stderr, "Cannot write to file: %s\n", file_path);
            fclose(out_buffer);
            return 1;
        }
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), out_buffer)) > 0) {
            fwrite(buf, 1, n, out_file);
        }
        fclose(out_file);
    }

    if (out_buffer) fclose(out_buffer);
    return result;
}
