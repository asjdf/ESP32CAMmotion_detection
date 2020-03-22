// filetoarray.cpp : Defines the entry point for the console application.
//

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int main(int argc, char *argv[]) {
	FILE * fp;
	char *buffer;
	long flen;
	char *fname;
	char pname[1024];

	if (argc == 2) {
		fname = argv[1];
		strcpy(pname, fname);
		char * dot = strchr(pname, '.');
		while (dot != NULL) {
			*dot = '_';
			dot = strchr(pname, '.');
		}
	}
	else {
		printf("Filename not supplied\n");
		return 1;
	}

	fp = fopen(fname, "rb");
	fseek(fp, 0, SEEK_END);
	flen = ftell(fp);
	rewind(fp);

	buffer = (char *)malloc((flen + 1) * sizeof(char));
	fread(buffer, flen, 1, fp);
	fclose(fp);

	printf("\n//File: %s, Size: %lu\n", fname, flen);
	printf("#define %s_len %lu\n", pname, flen);
	printf("const uint8_t %s[] PROGMEM = {\n", pname);
	long i;
	for (i = 0; i<flen; i++) {
		printf(" 0x%02X", (uint8_t)buffer[i]);
		if (i<(flen - 1)) {
			printf(",");
		}
		if ((i % 16) == 15) {
			printf("\n");
		}
	}
	printf("\n};\n\n");
	free(buffer);
	return 0;
}
