# chop

A stream filter for todo lists. Like `sed` or `sort`, but for todos.

```bash
echo "Buy milk" | chop
# - [ ] Buy milk

chop -ft < todos.txt
# - [ ] Pending item 1
# - [ ] Pending item 2
```

Chop normalizes any text into todo format and filters by status. It reads from stdin (via `<` or pipe), writes to stdout, and composes with standard unix tools.

## Install

```bash
make
sudo make install  # copies to /usr/local/bin
```

## Usage

```bash
# Normalize text to todos
chop < notes.txt > todos.txt

# Filter by status
chop -ft < todos.txt            # pending only
chop -fd < todos.txt            # completed only
chop -fip < todos.txt           # in-progress only

# Add new items
echo "Buy milk" | chop >> todos.txt

# Mark all items
chop -md < todos.txt | sponge todos.txt   # all done
chop -mip < todos.txt | sponge todos.txt  # all in-progress

# Mark items interactively (with fzf)
chop -md --fzf < todos.txt | sponge todos.txt
chop -mip --fzf < todos.txt | sponge todos.txt
```

Both `chop < file` and `cat file | chop` work - use whichever you prefer.

Long forms: `--filter=todo`, `--filter=done`, `--filter=in-progress`, `--mark=todo`, `--mark=done`, `--mark=in-progress`

## File format

Standard markdown checkboxes:

```
- [ ] Pending task
- [x] Completed task
- [>] In-progress task
```

Any plain text piped through chop becomes `- [ ] text`.

## Composing with other tools

```bash
# Browse with fzf
cat todos.txt | chop -ft | fzf

# Sort alphabetically
cat todos.txt | chop | sort

# Count pending items
cat todos.txt | chop -ft | wc -l
```

Install `moreutils` for `sponge`: `apt install moreutils` or `brew install moreutils`

## License

BSD 3-Clause. See [LICENSE](LICENSE).
