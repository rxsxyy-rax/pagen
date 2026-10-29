#ifndef PAGEN_H_
#define PAGEN_H_

struct tag
{
	const char* delim;
	const char* open;
	const char* close;
	// literal content
	int raw;
};

#endif /* PAGEN_H_ */
