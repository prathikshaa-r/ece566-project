#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <time.h>
#include <string.h>


extern int errno;

void usage() 
{
  fprintf(stderr, "Usage: Directory[mountdir or nasdir] fileSize (Kb)\n");
  abort();
}

int main(int argc, char *argv[])
{
	if(argc != 3)
	{
		usage();
	}		

	char directory[9];//up to mountdir/ (10)

	int parseRet = sscanf(argv[argc-2], "%s", directory);

	printf("%s\n", directory);

	FILE* fileNames[10];

	FILE* filePointers[10];
	FILE* resultsPointer;
	for(int create_index = 0; create_index < 10; create_index++)
	{
		char* fileName;
		if(strcmp("mountdir", directory) == 0)
		{
			fileName = malloc(68);//see below snprintf comment for length of string 27
			snprintf(fileName, 61, "%s", "/home/wjb24/ECE566/ece566-project/example/mountdir/benchFile");//total string length of 27 (19 %s+ 1%d+6%s+1\0)
			snprintf(fileName+60, 2, "%d", create_index);
			snprintf(fileName+61, 7, "%s", ".wjb24");
		}
		else if(strcmp("nasdir", directory) == 0)
		{
			fileName = malloc(66);//see below snprintf comment for length of string 27
			snprintf(fileName, 59, "%s", "/home/wjb24/ECE566/ece566-project/example/nasdir/benchFile");//total string length of 27 (19 %s+ 1%d+6%s+1\0)
			snprintf(fileName+58, 2, "%d", create_index);
			snprintf(fileName+59, 7, "%s", ".wjb24");
		}
		else
		{
			fprintf(stderr,"Direcotry parse error");
			abort();
		}
		printf("%s\n", fileName);
		fileNames[create_index] = fileName;
		filePointers[create_index] = fopen(fileName, "w+");
		if(filePointers[create_index] == NULL)
		{
			perror("Error opening file: ");
		}
	}

	size_t fileSize;
	parseRet = sscanf(argv[argc-1], "%lu", &fileSize);
	if(parseRet == EOF)
	{
		fprintf(stderr, "No fileSize parsed ---> Defaulting to 100 Kb.\n");
	}
	else
	{
		fprintf(stderr, "fileSize parsed ---> Setting to %lu Kb.\n", fileSize);
	}


	//--------------------Begin Testing-------------------\\

	clock_t timerStart, timerEnd;
	double cpuTimeUsed;

	//---------Write Testing---------\\
	
	char* dataSource;

	dataSource = malloc(fileSize*1024);

	for(int byte_index = 0; byte_index < fileSize*1024; byte_index++)//write 1 to 9 repeatedly to dataSource
	{
		int toSet = (byte_index%10);
		if(toSet != 0)
		{
			memset(dataSource+byte_index, toSet+48, 1);//ASCII value of 0 is 48
		}
		else
		{
			memset(dataSource+byte_index, 10, 1);//line break
		}
	}

	/*
		Files should already be open for writing.
	*/
	for(int write_index = 0; write_index < 10; write_index++)
	{
		size_t writeRes;
		timerStart = clock();

		writeRes = fwrite(dataSource, fileSize*1024, 1, filePointers[write_index]);

		timerEnd = clock();
		cpuTimeUsed = ((double)(timerEnd-timerStart)) / CLOCKS_PER_SEC;

		fprintf(stderr, "Write to file %d took %lf milliseconds\n", write_index, cpuTimeUsed*1000);

		{
      		fclose(filePointers[write_index]);
   		}
	}

	//---------Read Testing---------\\



	for(int read_index = 0; read_index < 10; read_index++)
	{
		FILE* readOpenRes;
		readOpenRes = fopen((const char *)fileNames[read_index], "r");
		if(readOpenRes == NULL)
		{
			perror("Error opening file: ");
		}

		size_t readRes;
		timerStart = clock();

		readRes = fread(dataSource, fileSize*1024, 1, filePointers[read_index]);

		timerEnd = clock();
		cpuTimeUsed = ((double)(timerEnd-timerStart)) / CLOCKS_PER_SEC;

		fprintf(stderr, "Read to file %d took %f milliseconds\n", read_index, cpuTimeUsed*1000);

		{
      		fclose(filePointers[read_index]);
   		}
	}

	return 0;
}