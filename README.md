# pagen

pagen is a VERY small and VERY minimal markdown to html converter written in C

## building

` $ cc -std=c99 -Oz -Wall -Wextra pagen.c -o pagen `

or

` $ make `

## usage

pagen converts markdown to html and always writes the result to a file, it
never prints the converted output to the standard output

` $ ./pagen [PATH] `

with no other options this derives the output filename from PATH by
replacing its extension with `.html` (`page.md` becomes `page.html`)

### other options

```
-o, --output       writes html to file (default is derived from input name; required when path is '-')
-r, --recursive    recursively looks for markdown files to convert to html
-s, --style        writes css style to page
-S, --signature    writes message or markdown file on page footer
-t, --title        writes page title to head
-v, --verbose      display where file was written to (best with --recursive)
    --help         output usage information and exit
    --version      output version information and exit
```

#### try converting this page too!
