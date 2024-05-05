// Copyright (c) 2024 Horia-Valentin MOROIANU

#ifndef _UTILS_H
#define _UTILS_H

#include <stdlib.h>
#include <errno.h>

#define DEB_DIE(assertion, call_description)	\
	do {										\
		if (assertion) {						\
			fprintf(stderr, "(%s, %d): ",		\
					__FILE__, __LINE__);		\
			perror(call_description);			\
			exit(errno);						\
		}										\
	} while (0)

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "%s\n",			\
					call_description);		\
			exit(errno);					\
		}									\
	} while (0)

#endif
