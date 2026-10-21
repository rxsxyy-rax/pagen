#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERSION "0"

static struct option const opts[] =
{
	{ "help"   , no_argument, NULL, -2 },
	{ "version", no_argument, NULL, -3 },
	{ NULL     , 0          , NULL, 0  },
};

static void replace_special_chars(const char* s)
{
	for(; *s; s++)
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
			default:
				putchar(*s);
		}
	}
}

static void tag(const char* open, const char* text, const char* close)
{
	fputs(open, stdout);
	replace_special_chars(text);
	puts(close);
}

static FILE* read_source(const char* path)
{
	FILE* file;

	if(!strcmp(path, "-"))
		return stdin;

	file = fopen(path, "r");
	if(!file)
		fprintf(stderr, "[pagen]: cannot open file '%s': %s\n", path,
			strerror(errno));

	return file;
}

static int convert_markdown(FILE* page)
{
	char* line = NULL;
	size_t cap = 0;
	ssize_t len;
	int ul = 0;

	puts("<html>\n<head></head>\n<body>");
	while((len = getline(&line, &cap, page)) != -1)
	{
		line[strcspn(line, "\r\n")] = '\0';

		int item = !strncmp(line, "- ", 2);
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

		if(!*line)
			continue;
		else if(!strncmp(line, "### ", 4))
			tag("<h3>", line + 4, "</h3>");
		else if(!strncmp(line, "## ", 3))
			tag("<h2>", line + 3, "</h2>");
		else if(!strncmp(line, "# ", 2))
			tag("<h1>", line + 2, "</h1>");
		else if(item)
			tag("<li>", line + 2, "</li>");
		else
			tag("<p>", line, "</p>");
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

void version(char* prog, char* ver)
{
	printf("%s %s, by rax/rxsxyy\n", prog, ver);
}

int main(int argc, char** argv)
{
	FILE* page;
	int c, ret;

	while((c = getopt_long(argc, argv, "", opts, NULL)) != -1)
	{
		switch(c)
		{
			case -2:
				usage(stdout, argv[0]);
				return 0;
			case -3:
				version(argv[0], VERSION);
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

	page = read_source(optind < argc ? argv[optind] : "-");
	if(!page)
		return 1;

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