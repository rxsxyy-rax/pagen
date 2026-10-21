#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERSION "0"

static const struct option opts[] =
{
	{ "help"   , no_argument, NULL, -2 },
	{ "version", no_argument, NULL, -3 },
	{ NULL     , 0          , NULL, 0  },
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
	{ "- "     , "<li>", "</li>" },
	{ ""       , "<p>" , "</p>"  },
};

static const struct tag spans[] =
{
	{ "***", "<b><i>", "</i></b>" },
	{ "**" , "<b>"   , "</b>"     },
	{ "__" , "<b>"   , "</b>"     },
	{ "*"  , "<i>"   , "</i>"     },
	{ "_"  , "<i>"   , "</i>"     },
	{ "~~" , "<del>" , "</del>"   },
	{ NULL , NULL    , NULL       },
};

// TODO: APPLY STYLE IMPLEMENTATION
// static void apply_style();

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

static int convert_markdown(FILE* page)
{
	char* line = NULL;
	char* end;
	size_t cap = 0;
	const struct tag* b;
	int item, ul = 0;

	puts("<html>\n<head>");
	// TODO: GIVE HEAD
	puts("</head>\n<body>");
	while(getline(&line, &cap, page) != -1)
	{
		end = line + strcspn(line, "\r\n");
		*end = '\0';

		b = blocks;
		while(strncmp(line, b->delim, strlen(b->delim)))
			b++;

		item = !strcmp(b->open, "<li>");
		if(ul && !item)
		{
			puts("</ul>");
			ul = 0;
		}
		if(item && !ul)
		{
			puts("<ul>");
			ul = 1;
		}

		if(end == line)
			continue;

		fputs(b->open, stdout);
		inline_markdown(line + strlen(b->delim), end);
		puts(b->close);
	}
	free(line);

	if(ul)
		puts("</ul>");
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
		"	--help       display this help and exit\n"
		"	--version    output version information and exit\n\n"
		"to write the output into a file you need redirect the output into one\n"
		"	$ %s [PATH] > file.html\n",
		prog, prog);
}

int main(int argc, char** argv)
{
	FILE* page = stdin;
	int c, ret;

	while((c = getopt_long(argc, argv, "", opts, NULL)) != -1)
	{
		switch(c)
		{
			case -2:
				usage(stdout, argv[0]);
				return 0;
			case -3:
				printf("%s %s, by rax/rxsxyy\n", argv[0], VERSION);
				return 0;
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

	if(optind < argc && strcmp(argv[optind], "-"))
	{
		page = fopen(argv[optind], "r");
		if(!page)
		{
			fprintf(stderr, "[pagen]: cannot open file '%s': %s\n", argv[optind], strerror(errno));
			return 1;
		}
	}

	ret = convert_markdown(page);

	if(page != stdin)
		fclose(page);

	if(fflush(stdout) || ferror(stdout))
	{
		perror("[pagen]: write error");
		return 1;
	}

	return ret;
}