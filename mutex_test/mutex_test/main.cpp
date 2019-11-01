#include <cstdio>
#include <time.h>
#include "kbhit.h"
#include "CConnectMonitoringThread.h"

#define ESC						( 27 )


int main()
{
	CConnectMonitoringThread* pcConnectMonitoringThread = (CConnectMonitoringThread *)new CConnectMonitoringThread();


	timespec		tTimeSpec;
	tTimeSpec.tv_sec = 1;
	tTimeSpec.tv_nsec = 0;


	pcConnectMonitoringThread->Start();

	printf("-----[ CConnectMonitoringThread Demo ]-----\n");
	printf(" [Enter] key : Demo End\n");


	while (1)
	{
		if (kbhit())
		{
			break;
		}
		nanosleep(&tTimeSpec, NULL);

	}

	delete pcConnectMonitoringThread;

	return 0;
}