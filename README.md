# chop

A stream filter for todo lists. Like `sed` or `sort`, but for todos.

```bash
echo "Buy milk" | chop
# - [ ] Buy milk

cat todos.txt | chop --todo
# - [ ] Pending item 1
# - [ ] Pending item 2
```

Chop normalizes any text into todo format and filters by status. It reads from stdin, writes to stdout, and composes with standard unix tools.

## Install

```bash
make
sudo make install  # copies to /usr/local/bin
```

## Usage

```bash
# Normalize text to todos
cat notes.txt | chop > todos.txt

# Filter by status
cat todos.txt | chop --todo          # pending only
cat todos.txt | chop --done          # completed only
cat todos.txt | chop --in-progress   # in-progress only

# Add new items
echo "Buy milk" | chop >> todos.txt

# Mark items (interactive with fzf)
cat todos.txt | chop start --fzf | sponge todos.txt  # in-progress
cat todos.txt | chop done --fzf | sponge todos.txt   # done

# Or by line number
cat todos.txt | chop done 3 | sponge todos.txt
```

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
cat todos.txt | chop --todo | fzf

# Sort alphabetically
cat todos.txt | chop | sort

# Count pending items
cat todos.txt | chop --todo | wc -l
```

Install `moreutils` for `sponge`: `apt install moreutils` or `brew install moreutils`

## License

BSD 3-Clause. See [LICENSE](LICENSE).
