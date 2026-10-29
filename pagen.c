#define _POSIX_C_SOURCE 200809L
#include "pagen.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define VERSION "2"

static const struct option opts[] =
{
	{"output"   , required_argument, NULL, 'o'},
	{"recursive", no_argument      , NULL, 'r'},
	{"style"    , required_argument, NULL, 's'},
	{"signature", required_argument, NULL, 'S'},
	{"title"    , required_argument, NULL, 't'},
	{"verbose"  , no_argument      , NULL, 'v'},
	{"help"     , no_argument      , NULL, -2 },
	{"version"  , no_argument      , NULL, -3 },
	{NULL       , 0                , NULL, 0  },
};

static const struct tag blocks[] =
{
	{"###### ", "<h6>", "</h6>", 0},
	{"##### " , "<h5>", "</h5>", 0},
	{"#### "  , "<h4>", "</h4>", 0},
	{"### "   , "<h3>", "</h3>", 0},
	{"## "    , "<h2>", "</h2>", 0},
	{"# "     , "<h1>", "</h1>", 0},
	{""       , "<p>" , "</p>" , 0},
};

static const struct tag oneliners[] =
{
	{"---", "<hr>", NULL, 0},
	{"  " , "<br>", NULL, 0},
};

static const struct tag spans[] =
{
	{"`"  , "<code>", "</code>" , 1},
	{"***", "<b><i>", "</b></i>", 0},
	{"___", "<b><i>", "</b></i>", 0},
	{"**" , "<b>"   , "</b>"    , 0},
	{"__" , "<b>"   , "</b>"    , 0},
	{"*"  , "<i>"   , "</i>"    , 0},
	{"_"  , "<i>"   , "</i>"    , 0},
	{"~~" , "<del>" , "</del>"  , 0},
	{NULL , NULL    , NULL      , 0},
};

static void apply_signature(const char* signature, FILE* out);
static void apply_style(const char* css, FILE* out);
static void apply_title(const char* title, FILE* out);
static int convert_markdown(FILE* page, const char* style_path, const char* signature_path, const char* title, FILE* out);
static char* derive_output_name(const char* input);
static const char* find(const char* s, const char* end, const char* pat);
static void html_putc(const char* s, const char* end, FILE* out);
static void inline_markdown(const char* s, const char* end, FILE* out);
static size_t list_marker(const char* s);
static int process_file(const char* path, const char* style_path, const char* signature, const char* title, const char* output_override, int verbose);
static int render(FILE* page, FILE* out);
static void usage(const char* prog, FILE* out);
static void version(const char* prog);
static int walk_directory(const char* dir_path, int recursive, const char* style_path, const char* signature, const char* title, int verbose);

static void apply_signature(const char* signature, FILE* out)
{
	FILE* footer;
	
	fputs("<footer>\n", out);

	footer = fopen(signature, "r");
	if(footer)
	{
		render(footer, out);
		fclose(footer);
	}
	else
	{
		fputs("<p>", out);
		html_putc(signature, signature + strlen(signature), out);
		fputs("</p>\n", out);
	}

	fputs("</footer>\n", out);
}

static void apply_style(const char* css, FILE* out)
{
	FILE* style;
	int c;

	style = fopen(css, "r");
	if(style)
	{
		fputs("<style>\n", out);

		// adds style to html file line by line without changing characters
		while((c = getc(style)) != EOF)
			putc(c, out);

		fputs("</style>\n", out);
		fclose(style);
	}
}

static void apply_title(const char* title, FILE* out)
{
	fputs("<title>", out);
	html_putc(title, title + strlen(title), out);
	fputs("</title>", out);
}

static int convert_markdown(FILE* page, const char* style_path, const char* signature_path, const char* title, FILE* out)
{
	int ret = 0;

	fputs("<html>\n<head>\n", out);

	if(title)
		apply_title(title, out);

	if(style_path)
		apply_style(style_path, out);

	fputs("</head>\n<body>\n", out);

	ret = render(page, out);

	if(signature_path)
		apply_signature(signature_path, out);

	fputs("</body>\n</html>", out);

	if(ret)
	{
		fprintf(stderr, "[pagen]: error reading file\n");
		return 1;
	}

	return 0;
} 

static void html_putc(const char* s, const char* end, FILE* out)
{
	for(; s < end; s++)
	{
		switch(*s)
		{
			case '&':
				fputs("&amp;", out);
				break;
			case '<':
				fputs("&lt;", out);
				break;
			case '>':
				fputs("&gt;", out);
				break;
			case '"':
				fputs("&quot;", out);
				break;
			case '\'':
				fputs("&apos;", out);
				break;
			default:
				putc(*s, out);
		}
	}
}

static const char* find(const char* s, const char* end, const char* pat)
{
	size_t n = strlen(pat);

	for(; s + n <= end; s++)
		if(!memcmp(s, pat, n))
			return s;

	return NULL;
}

static void inline_markdown(const char* s, const char* end, FILE* out)
{
	const char* start = s;
	const char *e, *mid = NULL, *url_end, *title;
	const struct tag* sp;
	size_t n;
	int img;

	while(s < end)
	{
		if(*s == '\\' && s + 1 < end)
		{
			html_putc(s + 1, s + 2, out);
			s += 2;
			continue;
		}

		if(*s == '\\' || (end - s == (ptrdiff_t)strlen(oneliners[1].delim) && !strncmp(s, oneliners[1].delim, strlen(oneliners[1].delim))))
		{
			fputs(oneliners[1].open, out);
			break;
		}

		for(sp = spans; sp->delim; sp++)
		{
			n = strlen(sp->delim);

			if(strncmp(s, sp->delim, n) || s + n >= end || (!sp->raw && isspace((unsigned char)s[n])))
				continue;

			if(*s == '_' && s > start && isalnum((unsigned char)s[-1]))
				continue;

			e = find(s + n, end, sp->delim);
			if(!e || e == s + n)
				continue;

			fputs(sp->open, out);
			if(sp->raw)
				html_putc(s + n, e, out);
			else
				inline_markdown(s + n, e, out);
			fputs(sp->close, out);
			s = e + n;
			break;
		}
		if(sp->delim)
			continue;

		img = !strncmp(s, "![", 2);
		if((img || *s == '[') && (mid = find(s + 1 + img, end, "](")) && (e = find(mid + 2, end, ")")))
		{
			url_end = e;
			title = find(mid + 2, e, " \"");
			if(title && e[-1] == '"')
				url_end = title;
			else
				title = NULL;

			fputs(img ? "<img src=\"" : "<a href=\"", out);
			html_putc(mid + 2, url_end, out);
			if(title)
			{
				fputs("\" title=\"", out);
				html_putc(title + 2, e - 1, out);
			}

			if(img)
			{
				fputs("\" alt=\"", out);
				html_putc(s + 2, mid, out);
				fputs("\">", out);
			}
			else
			{
				fputs("\">", out);
				inline_markdown(s + 1, mid, out);
				fputs("</a>", out);
			}

			s = e + 1;
			continue;
		}

		html_putc(s, s + 1, out);
		s++;
	}
}

static size_t list_marker(const char* s)
{
	size_t n = strspn(s, "0123456789");

	if(!strncmp(s, "- ", 2))
		return 2;

	if(n && !strncmp(s + n, ". ", 2))
		return n + 2;

	return 0;
}

static int render(FILE* page, FILE* out)
{
	char *line = NULL, *next = NULL, *tmp, *end;
	size_t cap = 0, next_cap = 0, tmp_cap, m;
	const char *list = NULL, *kind;
	const struct tag* b;
	int ahead = 0, next_item;

	for(;;)
	{
		if(ahead)
		{
			tmp = line;
			line = next;
			next = tmp;
			tmp_cap = cap;
			cap = next_cap;
			next_cap = tmp_cap;
		}
		else if(getline(&line, &cap, page) == -1)
			break;

		end = line + strcspn(line, "\r\n");
		*end = '\0';

		if(!strncmp(line, "```", 3))
		{
			fputs("<pre><code>", out);
			while(getline(&line, &cap, page) != -1)
			{
				end = line + strcspn(line, "\r\n");
				*end = '\0';

				if(!strncmp(line, "```", 3))
					break;

				html_putc(line, end, out);
				putc('\n', out);
			}
			fputs("</code></pre>\n", out);
			ahead = 0;
			continue;
		}

		m = list_marker(line);
		kind = *line == '-' ? "ul" : "ol";

		ahead = getline(&next, &next_cap, page) != -1;
		next_item = ahead && list_marker(next) && (*next == '-') == (*line == '-');

		if(list && (!m || strcmp(kind, list)))
		{
			fprintf(out, "</%s>\n", list);
			list = NULL;
		}
		if(m && !list && next_item)
		{
			fprintf(out, "<%s>\n", kind);
			list = kind;
		}
		if(!strcmp(line, oneliners[0].delim))
		{
			fputs(oneliners[0].open, out);
			putc('\n', out);
			continue;
		}

		if(end == line)
			continue;

		if(m)
		{
			fputs("<li>", out);
			inline_markdown(line + m, end, out);
			fputs("</li>\n", out);
			continue;
		}

		b = blocks;
		while(strncmp(line, b->delim, strlen(b->delim)))
			b++;

		fputs(b->open, out);
		inline_markdown(line + strlen(b->delim), end, out);
		fputs(b->close, out);
		putc('\n', out);
	}
	free(line);
	free(next);

	if(list)
		fprintf(out, "</%s>\n", list);

	return ferror(page);
}

static void usage(const char* prog, FILE* out)
{
	fprintf(out,
		"usage: %s [OPTION]... [PATH]\n"
		"convert PATH (default or '-': stdin) to html\n"
		"	-o, --output       writes html to file (default: derived from input name; required when PATH is '-')\n"
		"	-r, --recursive    recursively looks for markdown files to convert to html\n"
		"	-s, --style        writes css style to page\n"
		"	-S, --signature    writes message or markdown file on page footer\n"
		"	-t, --title        writes page title to head\n"
		"	-v, --verbose      display where file was written to (best with --recursive)\n"
		"	    --help         output this help and exit\n"
		"	    --version      output version information and exit\n",
		prog);
}

static void version(const char* prog)
{
	printf("%s v%s, by rax/rxsxyy\n", prog, VERSION);
}

static char* derive_output_name(const char* input)
{
	const char* dot = strrchr(input, '.');
	const char* base_end = dot ? dot : input + strlen(input);
	size_t blen = (size_t)(base_end - input);
	char* out = malloc(blen + 6); /* ".html" + '\0' */

	if(!out)
		return NULL;

	memcpy(out, input, blen);
	memcpy(out + blen, ".html", 6);
	return out;
}

static int process_file(const char* path, const char* style_path, const char* signature, const char* title, const char* output_override, int verbose)
{
	FILE *page, *out;
	char* derived = NULL;
	const char* out_path;
	int ret;

	if(!output_override && !strcmp(path, "-"))
	{
		fprintf(stderr, "[pagen]: reading from stdin requires -o/--output to name the output file\n");
		return 1;
	}

	page = strcmp(path, "-") ? fopen(path, "r") : stdin;
	if(!page)
	{
		fprintf(stderr, "[pagen]: cannot open file '%s': %s\n", path, strerror(errno));
		return 1;
	}

	out_path = output_override ? output_override : (derived = derive_output_name(path));

	out = fopen(out_path, "w");
	if(!out)
	{
		fprintf(stderr, "[pagen]: cannot open file '%s': %s\n", out_path, strerror(errno));
		if(page != stdin)
			fclose(page);
		free(derived);
		return 1;
	}

	ret = convert_markdown(page, style_path, signature, title, out);

	if(page != stdin)
		fclose(page);

	if(fflush(out) || ferror(out))
	{
		perror("[pagen]: write error");
		ret = 1;
	}

	fclose(out);

	if(verbose)
		fprintf(stderr, "[pagen]: wrote '%s' -> '%s'\n", path, out_path);

	free(derived);
	return ret;
}

static int walk_directory(const char* dir_path, int recursive, const char* style_path, const char* signature, const char* title, int verbose)
{
	DIR* dir;
	struct dirent* ent;
	char path[PATH_MAX];
	struct stat st;
	int ret = 0;

	dir = opendir(dir_path);
	if(!dir)
	{
		fprintf(stderr, "[pagen]: cannot open directory '%s': %s\n", dir_path, strerror(errno));
		return 1;
	}

	while((ent = readdir(dir)))
	{
		if(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
			continue;

		if(snprintf(path, sizeof(path), "%s/%s", dir_path, ent->d_name) >= (int)sizeof(path))
			continue;

		if(stat(path, &st))
			continue;

		if(S_ISDIR(st.st_mode))
		{
			if(recursive)
				ret |= walk_directory(path, recursive, style_path, signature, title, verbose);
			continue;
		}

		if(!S_ISREG(st.st_mode))
			continue;

		if(!strstr(ent->d_name, ".md") && !strstr(ent->d_name, ".markdown"))
			continue;

		ret |= process_file(path, style_path, signature, title, NULL, verbose);
	}

	closedir(dir);
	return ret;
}

int main(int argc, char** argv)
{
	int c, ret, recursive = 0, verbose = 0;
	char *style_path = NULL, *signature = NULL, *title = NULL, *output_path = NULL;
	struct stat st;
	const char* path;

	while((c = getopt_long(argc, argv, "o:rs:S:t:v", opts, NULL)) != -1)
	{
		switch(c)
		{
			case -2:
				usage(argv[0], stdout);
				return 0;
			case -3:
				version(argv[0]);
				return 0;
			case 'o':
				output_path = optarg;
				break;
			case 'r':
				recursive = 1;
				break;
			case 's':
				style_path = optarg;
				break;
			case 'S':
				signature = optarg;
				break;
			case 't':
				title = optarg;
				break;
			case 'v':
				verbose = 1;
				break;
			default:
				usage(argv[0], stdout);
				return 1;
		}
	}

	if(argc - optind > 1)
	{
		usage(argv[0], stderr);
		return 1;
	}

	path = optind < argc ? argv[optind] : "-";

	if(recursive)
	{
		if(output_path)
			fprintf(stderr, "[pagen]: --output is ignored with --recursive\n");

		if(stat(path, &st) || !S_ISDIR(st.st_mode))
		{
			fprintf(stderr, "[pagen]: --recursive requires PATH to be a directory\n");
			return 1;
		}

		ret = walk_directory(path, recursive, style_path, signature, title, verbose);
	}
	else
		ret = process_file(path, style_path, signature, title, output_path, verbose);

	return ret;
}