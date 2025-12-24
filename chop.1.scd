chop(1)

# NAME

chop - stream filter for todo lists

# SYNOPSIS

*chop* [options]

# DESCRIPTION

A stream filter for todo lists. Reads stdin, writes stdout. Designed to be
composable with unix tools like fzf, grep, sort, and sponge.

Any text piped to chop becomes a todo item. Output format matches input format,
so files stay hand-editable.

# OPTIONS

*-i*, *--include*=_STATUS_
	Only include items with the given status.

*-x*, *--exclude*=_STATUS_
	Exclude items with the given status.

*-m*, *--mark*=_STATUS_
	Change matching items to the given status.

*--fzf*
	Use fzf for interactive selection before applying marks.

*-f* _FILE_
	Read from FILE instead of stdin.

*-w*
	Write back to FILE (requires -f).

*-h*, *--help*
	Show help message.

*-V*, *--version*
	Show version.

# STATUS VALUES

*t*, *todo*
	Pending task: _- [ ] text_

*d*, *done*
	Completed task: _- [x] text_

*ip*, *in-progress*
	In-progress task: _- [>] text_

# EXAMPLES

Normalize any text to todo format:

	echo "Buy milk" | chop
	# outputs: - [ ] Buy milk

Filter by status:

	cat todos.txt | chop -it      # pending only
	cat todos.txt | chop -id      # done only
	cat todos.txt | chop -xd      # exclude done

Mark items:

	cat todos.txt | chop -md | sponge todos.txt      # mark all done
	cat todos.txt | chop -mip | sponge todos.txt     # mark all in-progress

Interactive selection with fzf:

	cat todos.txt | chop -md --fzf | sponge todos.txt

# FILE FORMAT

	- [ ] Pending task
	- [x] Completed task
	- [>] In-progress task

# SEE ALSO

*fzf*(1), *sponge*(1)

# AUTHORS

Matthew Phillips
