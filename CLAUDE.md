# chop - Unix-philosophy CLI todo manager

A stream filter for todo lists. Reads stdin, writes stdout. Designed to be composable with unix tools like `fzf`, `grep`, `sort`, `sponge`.

Tip: Install `moreutils` for `sponge` (`apt install moreutils` / `brew install moreutils`).

## Build

```bash
make        # build
make clean  # clean build artifacts
```

## Usage

```bash
# Filter/format (stdin → stdout)
cat todos.txt | chop                    # normalize to todo format
cat todos.txt | chop --todo             # filter: pending only
cat todos.txt | chop --done             # filter: done only
cat todos.txt | chop --in-progress      # filter: in-progress only

# Any text becomes todos
echo "Buy milk" | chop                  # outputs: - [ ] Buy milk

# Add new todos
echo "Buy milk" | chop >> todos.txt

# Mark all items in stream
cat todos.txt | chop done | sponge todos.txt   # all done
cat todos.txt | chop start | sponge todos.txt  # all in-progress

# Or select interactively with fzf
cat todos.txt | chop done --fzf | sponge todos.txt
cat todos.txt | chop start --fzf | sponge todos.txt
```

## Pipe-friendly

Output format is `- [status] text` (same as input), so files stay hand-editable.

## File format

```
- [ ] Pending task
- [x] Completed task
- [>] In-progress task
```

## Code structure

- `main.c` - CLI entry point, stream processing
- `chop.c` / `chop.h` - Core library: parsing, status utilities
- `Makefile` - Build system (gcc, C99 + POSIX)
