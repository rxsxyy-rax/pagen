#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define VERSION "1"

static const struct option opts[] =
{
	{ "style"    , required_argument, NULL, 'c' },
	{ "signature", required_argument, NULL, 'S' },
	{ "help"     , no_argument      , NULL, -2  },
	{ "version"  , no_argument      , NULL, -3  },
	{ NULL       , 0                , NULL, 0   },
};

struct tag
{
	const char* delim;
	const char* open;
	const char* close;
};

static const struct tag blocks[] =
{
	{ "###### ", "<h6>", "</h6>" },
	{ "##### " , "<h5>", "</h5>" },
	{ "#### "  , "<h4>", "</h4>" },
	{ "### "   , "<h3>", "</h3>" },
	{ "## "    , "<h2>", "</h2>" },
	{ "# "     , "<h1>", "</h1>" },
	{ ""       , "<p>" , "</p>"  },
};

static const struct tag spans[] =
{
	{ "***", "<b><i>", "</i></b>" },
	{ "___", "<b><i>", "</i></b>" },
	{ "**" , "<b>"   , "</b>"     },
	{ "__" , "<b>"   , "</b>"     },
	{ "*"  , "<i>"   , "</i>"     },
	{ "_"  , "<i>"   , "</i>"     },
	{ "~~" , "<del>" , "</del>"   },
	{ NULL , NULL    , NULL       },
};

static void apply_style(FILE* css)
{
	int c;

	puts("<style>");
	while((c = getc(css)) != EOF)
		putchar(c);
	puts("</style>");
}

static void replace_special_chars(const char* s, const char* end)
{
	for(; s < end; s++)
	{
		switch(*s)
		{
			case '&':
				fputs("&amp;", stdout);
				break;
			case '<':
				fputs("&lt;", stdout);
				break;
			case '>':
				fputs("&gt;", stdout);
				break;
			case '"':
				fputs("&quot;", stdout);
				break;
			case '\'':
				fputs("&apos;", stdout);
				break;
			default:
				putchar(*s);
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

static void inline_markdown(const char* s, const char* end)
{
	const char* start = s;
	const char *e, *mid = NULL, *url_end, *title;
	const struct tag* sp;
	size_t n;
	int img;

	while(s< end)
	{
		if(*s == '\\' && s + 1 < end)
		{
			replace_special_chars(s + 1, s + 2);
			s += 2;
			continue;
		}

		if(*s == '\\' || (end - s == 2 && !strncmp(s, "  ", 2)))
		{
			fputs("<br>", stdout);
			break;
		}

		if(*s == '`' && (e = find(s + 1, end, "`")) && e > s + 1)
		{
			fputs("<code>", stdout);
			replace_special_chars(s + 1, e);
			fputs("</code>", stdout);
			s = e + 1;
			continue;
		}

		img = !strncmp(s, "![", 2);
		if((img || *s == '[') && (mid = find(s + 1 + img, end, "](")) && (e = find(mid + 2, end, ")")))
		{
			url_end = e;
			title = find(mid + 2, e, " \"");
			if(title && e[-1] == '"')
				url_end = title;
			else
				title = NULL;

			fputs(img ? "<img src=\"" : "<a href=\"", stdout);
			replace_special_chars(mid + 2, url_end);
			if(title)
			{
				fputs("\" title=\"", stdout);
				replace_special_chars(title + 2, e - 1);
			}

			if(img)
			{
				fputs("\" alt=\"", stdout);
				replace_special_chars(s + 2, mid);
				fputs("\">", stdout);
			}
			else
			{
				fputs("\">", stdout);
				inline_markdown(s + 1, mid);
				fputs("</a>", stdout);
			}

			s = e + 1;
			continue;
		}

		for(sp = spans; sp->delim; sp++)
		{
			n = strlen(sp->delim);

			if(strncmp(s, sp->delim, n) || s + n >= end || isspace((unsigned char)s[n]))
				continue;

			if(*s == '_' && s > start && isalnum((unsigned char)s[-1]))
				continue;

			e = find(s + n, end, sp->delim);
			if(!e || e == s + n)
				continue;

			fputs(sp->open, stdout);
			inline_markdown(s + n, e);
			fputs(sp->close, stdout);
			s = e + n;
			break;
		}
		if(sp->delim)
			continue;

		replace_special_chars(s, s + 1);
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

static int convert_markdown(FILE* page, FILE* style, const char* signature)
{
	char *line = NULL, *next = NULL, *tmp, *end;
	size_t cap = 0, next_cap = 0, tmp_cap, m;
	const char *list = NULL, *kind;
	const struct tag* b;
	int ahead = 0, next_item, interactive = isatty(fileno(page));

	puts("<html>\n<head>");

	if(style)
		apply_style(style);
	puts("</head>\n<body>");
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

		m = list_marker(line);
		kind = *line == '-' ? "ul" : "ol";

		ahead = 0;
		next_item = 1;
		if(!interactive)
		{
			ahead = getline(&next, &next_cap, page) != -1;
			next_item = ahead && list_marker(next) && (*next == '-') == (*line == '-');
		}

		if(list && (!m || strcmp(kind, list)))
		{
			printf("</%s>\n", list);
			list = NULL;
		}
		if(m && !list && next_item)
		{
			printf("<%s>\n", kind);
			list = kind;
		}
		if(!strcmp(line, "---"))
		{
			puts("<hr>");
			continue;
		}

		if(end == line)
			continue;

		if(m)
		{
			fputs("<li>", stdout);
			inline_markdown(line + m, end);
			puts("</li>");
			continue;
		}

		b = blocks;
		while(strncmp(line, b->delim, strlen(b->delim)))
			b++;

		fputs(b->open, stdout);
		inline_markdown(line + strlen(b->delim), end);
		puts(b->close);
	}
	free(line);
	free(next);

	if(list)
		printf("</%s>\n", list);
	if(signature)
	{
		fputs("<footer>\n<p>", stdout);
		replace_special_chars(signature, signature + strlen(signature));
		puts("</p>\n</footer>");
	}
	puts("</body>\n</html>");

	if(ferror(page))
	{
		fprintf(stderr, "[pagen]: error reading file\n");
		return 1;
	}

	return 0;
}

static void usage(FILE* out, const char* prog)
{
	fprintf(out,
		"usage: %s [OPTION]... [PATH]\n"
		"convert PATH (default or '-': stdin) to html on stdout\n"
		"	-c, --style        writes css style to page\n"
		"	-S, --signature    writes message on page footer\n"
		"	--help             display this help and exit\n"
		"	--version          output version information and exit\n",
		prog);
}

int main(int argc, char** argv)
{
	FILE* page = stdin, *style = NULL;
	int c, ret;
	char *style_path = NULL, *signature = NULL;

	while((c = getopt_long(argc, argv, "c:S:", opts, NULL)) != -1)
	{
		switch(c)
		{
			case -2:
				usage(stdout, argv[0]);
				return 0;
			case -3:
				printf("%s %s, by rax/rxsxyy\n", argv[0], VERSION);
				return 0;
			case 'c':
				style_path = optarg;
				break;
			case 'S':
				signature = optarg;
				break;
			default:
				usage(stdout, argv[0]);
				return 1;
		}
	}

	if(argc - optind > 1)
	{
		usage(stderr, argv[0]);
		return 1;
	}

	if(style_path)
	{
		style = fopen(style_path, "r");
		if(!style)
		{
			fprintf(stderr, "[pagen]: cannot open style file '%s': %s\n", style_path, strerror(errno));
			return 1;
		}
	}

	if(optind < argc && strcmp(argv[optind], "-"))
	{
		page = fopen(argv[optind], "r");
		if(!page)
		{
			fprintf(stderr, "[pagen]: cannot open file '%s': %s\n", argv[optind], strerror(errno));
			return 1;
		}
	}

	ret = convert_markdown(page, style, signature);

	if(page != stdin)
		fclose(page);

	if(fflush(stdout) || ferror(stdout))
	{
		perror("[pagen]: write error");
		return 1;
	}

	return ret;
}