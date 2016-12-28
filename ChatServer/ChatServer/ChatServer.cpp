#include<WinSock2.h>
#include<stdio.h>
#include<stdlib.h>
#include<process.h>
#pragma comment(lib,"ws2_32.lib")

#define SEND_OVER 1 //已转发
#define SEND_YET 0   //未转发

int g_iStatus = SEND_YET;
SOCKET g_ServerSocket = INVALID_SOCKET;		 //服务端套接字
SOCKADDR_IN g_ClientAddr = { 0 };			 //客户端地址
int g_iClientAddrLen = sizeof(g_ClientAddr);
bool g_bCheckConnect = false;                //检查连接情况

//客户端信息结构体
typedef struct _Client
{
	SOCKET sClient;      //客户端套接字
	char buf[128];		 //数据缓冲区
	char userName[16];   //客户端用户名
	char IP[20];		 //客户端IP
	UINT_PTR flag;       //标记客户端，用来区分不同的客户端
}Client;

Client g_Client[10] = { 0 };    


//发送数据
unsigned __stdcall ThreadSend(void *param)
{
	int ret = 0;
	int flag = *(int *)param;
	SOCKET client = INVALID_SOCKET;
	char temp[128] = { 0 };
	char buffer[128] = { 0 };
	memcpy(temp, g_Client[flag].buf, sizeof(temp));
	sprintf(buffer, "%s: %s", g_Client[flag].userName, temp);
	if (strlen(temp) != 0 && g_iStatus == SEND_YET)
	{	
		for (int i = 0; i < 10; i++)
		{
			if (i != flag)
				ret = send(g_Client[i].sClient, buffer, sizeof(buffer), 0);
		}
	}
	if (ret == SOCKET_ERROR)
		return 1;
	g_iStatus = SEND_OVER;   //转发成功后设置状态为已转发
	return 0;
}
//接收数据
unsigned __stdcall ThreadRecv(void * param)
{
	SOCKET client = INVALID_SOCKET;
	int flag = 0;
	for (int i = 0; i < 10; i++)
	{
		if (*(int *)param == g_Client[i].flag)
		{
			//printf("%d", *(int*)param);
			client = g_Client[i].sClient;
			flag = i;
		}
	}

	char temp[128] = { 0 };
	while (1)
	{
		memset(temp, 0, sizeof(temp));
		int ret = recv(client, temp, sizeof(temp), 0);//接收数据
		if (ret == SOCKET_ERROR)
			continue;
		g_iStatus = SEND_YET; //设置转发状态为未转发
		memcpy(g_Client[flag].buf, temp, sizeof(g_Client[flag].buf));
		_beginthreadex(NULL, 0, ThreadSend, &flag, 0, NULL);
	}
	return 0;

}


//线程连接管理
unsigned __stdcall ThreadManager(void *param)
{
	while (1)
	{
		for (int i = 0; i < 10; i++)
		{
			if (send(g_Client[i].sClient, "", sizeof(""), 0) == SOCKET_ERROR)
			{
				if (g_Client[i].sClient != 0)
				{
					printf("Disconnect from IP: %s,UserName: %s\n", g_Client[i].IP, g_Client[i].userName);
					closesocket(g_Client[i].sClient);
					g_Client[i] = { 0 };
				}
			}
		}
		Sleep(2000); //2s检查一次
	}
	return 0;
}


unsigned __stdcall ThreadAccept(void *param)
{
	int i = 0;
	int temp1 = 0, temp2 = 0;
	_beginthreadex(NULL, 0, ThreadManager, NULL, 0, NULL);
	while (1){
		while (i < 10)
		{
			if (g_Client[i].flag != 0)
			{
				++i;
				continue;
			}

			if ((g_Client[i].sClient = accept(g_ServerSocket, (SOCKADDR*)&g_ClientAddr, &g_iClientAddrLen)) == INVALID_SOCKET)
			{
				printf("accept failed with error code: %d\n", WSAGetLastError());
				closesocket(g_ServerSocket);
				WSACleanup();
				return -1;
			}
			recv(g_Client[i].sClient, g_Client[i].userName, sizeof(g_Client[i].userName), 0); //接收用户名
			printf("Successfuuly got a connection from IP:%s ,Port: %d,UerName: %s\n",
				inet_ntoa(g_ClientAddr.sin_addr), htons(g_ClientAddr.sin_port), g_Client[i].userName);
			memcpy(g_Client[i].IP, inet_ntoa(g_ClientAddr.sin_addr), sizeof(g_Client[i].IP)); //记录客户端IP
			g_Client[i].flag = g_Client[i].sClient;//不同的socket有不同的UINT数字标识
			_beginthreadex(NULL, 0, ThreadRecv, &g_Client[i].flag, 0, NULL);

			i++;
		}
		i = 0;
		/*for (int i = 0; i < 10; i++)
		{
			if (g_Client[i].flag != temp1)
			{
				_beginthreadex(NULL, 0, ThreadRecv, &g_Client[i].flag, 0, NULL);
				printf("%d", i);
			}

		}*/

		Sleep(2000);
	}
	return 0;
}

int StartServer()
{
	WSADATA wsaData = { 0 };
	SOCKADDR_IN ServerAddr = { 0 };
	USHORT uPort = 18000;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		printf("WSAStartup failed with error code:%d\n", WSAGetLastError());
		return -1;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("wVersion was not 2.2\n");
		return -1;
	}

	//创建套接字
	g_ServerSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (g_ServerSocket == INVALID_SOCKET)
	{
		printf("socket failed with error code:%d\n", WSAGetLastError());
		return -1;
	}

	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port = htons(uPort);
	ServerAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	if (SOCKET_ERROR == bind(g_ServerSocket, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr)))
	{
		printf("bind failed with error code: %d\n", WSAGetLastError());
		closesocket(g_ServerSocket);
		return -1;
	}

	if (SOCKET_ERROR == listen(g_ServerSocket, 20000))
	{
		printf("listen failed with error code: %d\n", WSAGetLastError());
		closesocket(g_ServerSocket);
		WSACleanup();
		return -1;
	}

	_beginthreadex(NULL, 0, ThreadAccept, NULL, 0, 0);

	for (int k = 0; k < 100; k++)
	{
		Sleep(10000000);
	}

	for (int j = 0; j < 10; j++)
	{
		if (g_Client[j].sClient != INVALID_SOCKET)
			closesocket(g_Client[j].sClient);
	}
	closesocket(g_ServerSocket);
	WSACleanup();
	return 0;
}


int main()
{
	StartServer();//开启服务器
	return 0;
}

