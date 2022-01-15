/*
* Encapsulates the properties of the data-package.
*/
typedef struct data_package {
	char s_num[8];
	char message[1024];
	char buffer[1032];
	int pt; //pointer to iterate through message[]
} package;