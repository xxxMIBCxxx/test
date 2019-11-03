#include <cstdio>
#include <time.h>
#include "kbhit.h"
#include "CLandryClient.h"

#define ESC						( 27 )


int main()
{
	CLandryClient* pcLandryClient = (CLandryClient*)new CLandryClient();


	timespec		tTimeSpec;
	tTimeSpec.tv_sec = 1;
	tTimeSpec.tv_nsec = 0;


	pcLandryClient->Start();

	printf("-----[ CLandryClient Demo ]-----\n");
	printf(" [Enter] key : Demo End\n");


	while (1)
	{
		if (kbhit())
		{
			break;
		}
		nanosleep(&tTimeSpec, NULL);

	}

	delete pcLandryClient;

	return 0;
}