# pagen

pagen is a VERY small and VERY minimal markdown to html converter written in C

## building

```
$ make
$ make install    # installs to /usr/local/bin, change PREFIX to move it
```

or by hand:

```
$ cc -std=c99 -Wall -Wextra pagen.c -o pagen
```

## usage

pagen converts markdown to html and always writes the result to a file, it never prints the converted output to the standard output

```
$ ./pagen [OPTION]... [PATH]
```

with no other options the output filename is derived from PATH by replacing its extension with `.html` (`page.md` becomes `page.html`, `README` becomes `README.html`)

when PATH is missing or `-`, markdown is read from stdin and `-o` is required

### options

```
-o, --output       writes html to file, or to a directory when --recursive is passed
-r, --recursive    recursively looks for .md and .markdown files to convert to html
-s, --style        writes css style to page
-S, --signature    writes message or markdown file on page footer
-t, --title        writes page title to head
-v, --verbose      display where file was written to (best with --recursive)
    --help         output usage information and exit
    --version      output version information and exit
```

### examples

```
$ ./pagen -t "my page" -s style.css page.md
$ ./pagen -r -v -o site/ notes/
$ cat page.md | ./pagen -o page.html
```

## supported markdown

- headings, from `#` to `######`
- paragraphs, one per line
- **bold**, *italic*, ***both***, ~~strikethrough~~ and `inline code`
- links and images, with an optional title
- lists, both `-` and `1.` style
- fenced code blocks
- horizontal rules with `---`
- line breaks with two trailing spaces or a trailing backslash
- inline html, passed through as is

## license

MIT, see LICENSE

#### try converting this page too!
