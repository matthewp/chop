# chop

A stream filter for todo lists. Like `sed` or `sort`, but for todos.

```bash
echo "Buy milk" | chop
# - [ ] Buy milk

chop -it < todos.txt
# - [ ] Pending item 1
# - [ ] Pending item 2
```

Chop normalizes any text into todo format and filters by status. It reads from stdin (via `<` or pipe), writes to stdout, and composes with standard unix tools.

## Install

| Platform | Binary | Package Manager |
|----------|--------|-----------------|
| Linux | `chop-linux-amd64` | `apt install chop` |
| FreeBSD | `chop-freebsd-amd64` | `pkg install chop` |
| macOS | `chop-macos-arm64` | - |

### Debian/Ubuntu (apt)

```bash
curl -fsSL https://codeberg.org/api/packages/mphillips/debian/repository.key | sudo tee /etc/apt/keyrings/chop.asc
echo "deb [signed-by=/etc/apt/keyrings/chop.asc] https://codeberg.org/api/packages/mphillips/debian stable main" | sudo tee /etc/apt/sources.list.d/chop.list
sudo apt update && sudo apt install chop
```

### FreeBSD (pkg)

```bash
sudo mkdir -p /usr/local/etc/pkg/repos
cat <<'EOF' | sudo tee /usr/local/etc/pkg/repos/chop.conf
chop: {
  url: "https://matthewp.github.io/chop/freebsd/",
  enabled: yes
}
EOF
sudo pkg update
sudo pkg install chop
```

### Binary download

Grab a binary from [Releases](https://codeberg.org/mphillips/chop/releases):

```bash
# Linux
curl -LO https://codeberg.org/mphillips/chop/releases/latest/download/chop-linux-amd64

# macOS (Apple Silicon)
curl -LO https://codeberg.org/mphillips/chop/releases/latest/download/chop-macos-arm64

# FreeBSD
curl -LO https://codeberg.org/mphillips/chop/releases/latest/download/chop-freebsd-amd64

chmod +x chop-*
sudo mv chop-* /usr/local/bin/chop
```

### Build from source

```bash
make
sudo make install  # copies to /usr/local/bin
```

## Usage

```bash
# Normalize text to todos
chop < notes.txt > todos.txt

# Include by status
chop -it < todos.txt            # pending only
chop -id < todos.txt            # completed only
chop -iip < todos.txt           # in-progress only

# Exclude by status
chop -xd < todos.txt            # exclude done (clear finished)
chop -xd < todos.txt | sponge todos.txt  # clear done and save

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

Long forms: `--include=STATUS`, `--exclude=STATUS`, `--mark=STATUS` (STATUS: todo, done, in-progress)

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
chop -it < todos.txt | fzf

# Sort alphabetically
chop < todos.txt | sort

# Count pending items
chop -it < todos.txt | wc -l
```

Install `moreutils` for `sponge`: `apt install moreutils` or `brew install moreutils`

## File mode

For convenience, `-f FILE` reads from a file and `-w` writes back to it:

```bash
chop -f todos.txt -iip          # view in-progress
chop -f todos.txt -xd -w        # clear done items
chop -f todos.txt -md --fzf -w  # interactive mark done
```

This is useful for shell aliases:

```bash
alias t="chop -f ~/todos.txt"
t                    # view all
t -iip               # view in-progress
t -xd -w             # clear done
t -mip --fzf -w      # mark in-progress interactively
```

## License

BSD 3-Clause. See [LICENSE](LICENSE).
