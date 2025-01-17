
// ServerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "Server.h"
#include "ServerDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include <winsock2.h>
#define CLIENT_NUMBER 2
#define PORT 5150			// Порт по умолчанию
#define DATA_BUFSIZE 65536 	// Размер буфера по умолчанию

int  iPort = PORT; 	 // Порт для прослушивания подключений
bool bPrint = false; // Выводить ли сообщения клиентов



long Check(char* Str1, char* Str2)
{
	long len1, len2;
	//for (int i = 0; i < strlen(Str1); i++)
	//{
	//	len1 = len1 * 10 + atoi(Str1[i]);
	//}
	len1 = atoi(Str1);
	len2 = atoi(Str2);

	if (len1 > len2)
		return 1;
	else if (len1 == len2)
		return 0;
	else
		return -1;
}

typedef struct _SOCKET_INFORMATION {
	CHAR Buffer[DATA_BUFSIZE];
	WSABUF DataBuf;
	SOCKET Socket;
	OVERLAPPED Overlapped;
	DWORD BytesSEND;
	DWORD BytesRECV;
} SOCKET_INFORMATION, *LPSOCKET_INFORMATION;

DWORD TotalSockets = 0; // Всего сокетов
DWORD RecvSocket = 0;
DWORD SendSocket = 0;
LPSOCKET_INFORMATION SocketArray[FD_SETSIZE];

HWND   hWnd_LB;  // Для передачи потокам

BOOL CreateSocketInformation(SOCKET s, CListBox * pLB);
void FreeSocketInformation(DWORD Index, CListBox * pLB);
UINT ListenThread(PVOID lpParam);

// CServerDlg dialog

CServerDlg::CServerDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_SERVER_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CServerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LISTBOX, m_ListBox);
}

BEGIN_MESSAGE_MAP(CServerDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_START, &CServerDlg::OnBnClickedStart)
	ON_BN_CLICKED(IDC_PRINT, &CServerDlg::OnBnClickedPrint)
END_MESSAGE_MAP()


// CServerDlg message handlers

BOOL CServerDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here
	char Str[128];

	sprintf_s(Str, sizeof(Str), "%d", iPort);
	GetDlgItem(IDC_PORT)->SetWindowText(Str);

	if (bPrint)
		((CButton *)GetDlgItem(IDC_PRINT))->SetCheck(1);
	else
		((CButton *)GetDlgItem(IDC_PRINT))->SetCheck(0);

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CServerDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CServerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CServerDlg::OnBnClickedStart()
{
	// TODO: Add your control notification handler code here
	char Str[81];

	hWnd_LB = m_ListBox.m_hWnd; // Для ListenThread
	GetDlgItem(IDC_PORT)->GetWindowText(Str, sizeof(Str));
	iPort = atoi(Str);
	if (iPort <= 0 || iPort >= 0x10000)
	{
		AfxMessageBox("Incorrect Port number");
		return;
	}

	AfxBeginThread(ListenThread, NULL);

	GetDlgItem(IDC_START)->EnableWindow(false);
}


UINT ListenThread(PVOID lpParam)
{
	SOCKET	ListenSocket;
	SOCKET	AcceptSocket;
	SOCKADDR_IN	InternetAddr;
	WSADATA	wsaData;
	INT		Ret;
	FD_SET	WriteSet;
	FD_SET	ReadSet;
	DWORD	i;
	DWORD	Total;
	ULONG	NonBlock;
	DWORD	Flags;
	DWORD	SendBytes;
	DWORD	RecvBytes;

	char  Str[200];
	CListBox  * pLB =
		(CListBox *)(CListBox::FromHandle(hWnd_LB));

	if ((Ret = WSAStartup(0x0202, &wsaData)) != 0)
	{
		sprintf_s(Str, sizeof(Str),
			"WSAStartup() failed with error %d", Ret);
		pLB->AddString(Str);
		WSACleanup();
		return 1;
	}

	if ((ListenSocket = WSASocket(AF_INET, SOCK_STREAM, 0,
		NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET)
	{
		sprintf_s(Str, sizeof(Str),
			"WSASocket() failed with error %d",
			WSAGetLastError());
		pLB->AddString(Str);
		return 1;
	}

	InternetAddr.sin_family = AF_INET;
	InternetAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	InternetAddr.sin_port = htons(iPort);

	if (bind(ListenSocket, (PSOCKADDR)&InternetAddr,
		sizeof(InternetAddr)) == SOCKET_ERROR)
	{
		sprintf_s(Str, sizeof(Str),
			"bind() failed with error %d",
			WSAGetLastError());
		pLB->AddString(Str);
		return 1;
	}

	if (listen(ListenSocket, 5))
	{
		sprintf_s(Str, sizeof(Str),
			"listen() failed with error %d",
			WSAGetLastError());
		pLB->AddString(Str);
		return 1;
	}

	NonBlock = 1;
	if (ioctlsocket(ListenSocket, FIONBIO, &NonBlock)
		== SOCKET_ERROR)
	{
		sprintf_s(Str, sizeof(Str),
			"ioctlsocket() failed with error %d",
			WSAGetLastError());
		pLB->AddString(Str);
		return 1;
	}

	while (TRUE)
	{
		FD_ZERO(&ReadSet);
		FD_ZERO(&WriteSet);

		FD_SET(ListenSocket, &ReadSet);

		for (i = 0; i < TotalSockets; i++)
			if (SocketArray[i]->BytesRECV >
				SocketArray[i]->BytesSEND)
				FD_SET(SocketArray[i]->Socket, &WriteSet);
			else
				FD_SET(SocketArray[i]->Socket, &ReadSet);

		if ((Total = select(0, &ReadSet, &WriteSet, NULL,
			NULL)) == SOCKET_ERROR)
		{
			sprintf_s(Str, sizeof(Str),
				"select() returned with error %d",
				WSAGetLastError());
			pLB->AddString(Str);
			return 1;
		}

		if (FD_ISSET(ListenSocket, &ReadSet) && TotalSockets <= CLIENT_NUMBER)
		{
			Total--;
			if ((AcceptSocket = accept(ListenSocket, NULL,
				NULL)) != INVALID_SOCKET)
			{
				// Перевод сокета в неблокирующий режим
				NonBlock = 1;
				if (ioctlsocket(AcceptSocket, FIONBIO,
					&NonBlock) == SOCKET_ERROR)
				{
					sprintf_s(Str, sizeof(Str),
						"ioctlsocket() failed with error %d",
						WSAGetLastError());
					pLB->AddString(Str);
					return 1;
				}
				// Добавление сокет в массив SocketArray
				if (CreateSocketInformation(AcceptSocket, pLB)
					== FALSE)
					return 1;

				char podh[256] = "1";
				char lish[256] = "0";
				int ret;

				if (TotalSockets > CLIENT_NUMBER)
				{
					ret = send(AcceptSocket, lish, strlen(lish), 0);
				}
				else
				{
					ret = send(AcceptSocket, podh, strlen(podh), 0);
				}
			}
			else
			{
				if (WSAGetLastError() != WSAEWOULDBLOCK)
				{
					sprintf_s(Str, sizeof(Str),
						"accept() failed with error %d",
						WSAGetLastError());
					pLB->AddString(Str);
					return 1;
				}
			}
		} // while

		if (TotalSockets >= CLIENT_NUMBER)
		{
			LPSOCKET_INFORMATION SocketInfo1 = SocketArray[0];
			LPSOCKET_INFORMATION SocketInfo2 = SocketArray[1];
			char string[256] = "";

			strcat_s(string, SocketInfo1->Buffer);
			strcat_s(string, SocketInfo2->Buffer);

			for (i = 0; Total > 0 && i < TotalSockets; i++)
			{
				LPSOCKET_INFORMATION SocketInfo = SocketArray[i];

				// Если сокет попадает в множество ReadSet, 
				// выполняем чтение данных.

				if (FD_ISSET(SocketInfo->Socket, &ReadSet))
				{
					Total--;

					SocketInfo->DataBuf.buf = SocketInfo->Buffer;
					SocketInfo->DataBuf.len = DATA_BUFSIZE;

					Flags = 0;
					if (WSARecv(SocketInfo->Socket,
						&(SocketInfo->DataBuf), 1, &RecvBytes,
						&Flags, NULL, NULL) == SOCKET_ERROR)
					{
						if (WSAGetLastError() != WSAEWOULDBLOCK)
						{
							sprintf_s(Str, sizeof(Str),
								"WSARecv() failed with error %d",
								WSAGetLastError());
							pLB->AddString(Str);

							FreeSocketInformation(i, pLB);
						}
						continue;
					}

					else // чтение успешно
					{
						SocketInfo->BytesRECV = RecvBytes;

						// Если получено 0 байтов, значит клиент
						// закрыл соединение.

						if (RecvBytes == 0)
						{
							FreeSocketInformation(i, pLB);
							continue;
						}

						RecvSocket++;

						// Распечатка сообщения, если нужно

						if (bPrint)
						{
							unsigned l = sizeof(Str) - 1;
							if (l > RecvBytes)
								l = RecvBytes;
							strncpy_s(Str, SocketInfo->Buffer, l);
							Str[l] = 0;
							pLB->AddString(Str);
						}
					}
				}

				// Если сокет попадает в множество WriteSet, 
				// выполняем отправку данных.

				if (RecvSocket >= CLIENT_NUMBER)
				{
					if (FD_ISSET(SocketInfo->Socket, &WriteSet))
					{
						Total--;

						SocketInfo->DataBuf.buf =
							SocketInfo->Buffer + SocketInfo->BytesSEND;
						SocketInfo->DataBuf.len =
							SocketInfo->BytesRECV - SocketInfo->BytesSEND;
						LPSOCKET_INFORMATION SocketInf = SocketArray[i];

						if (i == TotalSockets - 1)
						{
							SocketInf = SocketArray[0];
						}
						else
						{
							SocketInf = SocketArray[i + 1];
						}

						int ret;
						char tmp[200];
						int result;

						result = strcmp(SocketInfo->Buffer, SocketInf->Buffer);

						if (result > 0) {

							strcpy_s(tmp, _countof(tmp), "ваше число больше");
							ret = send(SocketInfo->Socket, tmp, strlen(tmp), 0);
						}
						else if (result < 0) {

							strcpy_s(tmp, _countof(tmp), "ваше число меньше");
							ret = send(SocketInfo->Socket, tmp, strlen(tmp), 0);
						}
						else
						{
							strcpy_s(tmp, _countof(tmp), "числа одинаковы одинаково");
							ret = send(SocketInfo->Socket, tmp, strlen(tmp), 0);
						}


						if (ret == SOCKET_ERROR)
						{
							if (WSAGetLastError() != WSAEWOULDBLOCK)
							{
								sprintf_s(Str, sizeof(Str),
									"WSASend() failed with error %d",
									WSAGetLastError());
								pLB->AddString(Str);

								FreeSocketInformation(i, pLB);
							}
							continue;
						}
						else
						{
							/*SocketInfo->BytesSEND += SendBytes;

							if (SocketInfo->BytesSEND ==
								SocketInfo->BytesRECV)*/
							{
								SocketInfo->BytesSEND = 0;
								SocketInfo->BytesRECV = 0;
							}

							SendSocket++;
						}
					}
				}
			}
		}
		if (SendSocket == CLIENT_NUMBER)
		{
			SendSocket = 0;
			RecvSocket = 0;
		}

	}

	return 0;
}



BOOL CreateSocketInformation(SOCKET s, CListBox *pLB)
{
	LPSOCKET_INFORMATION SI;
	char Str[81];

	sprintf_s(Str, sizeof(Str), "Accepted socket number %d", s);
	pLB->AddString(Str);

	if ((SI = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR, sizeof(SOCKET_INFORMATION))) == NULL)
	{
		sprintf_s(Str, sizeof(Str), "GlobalAlloc() failed with error %d", GetLastError());
		pLB->AddString(Str);
		return FALSE;
	}

	// Подготовка структуры SocketInfo для использования.
	SI->Socket = s;
	SI->BytesSEND = 0;
	SI->BytesRECV = 0;

	SocketArray[TotalSockets] = SI;
	TotalSockets++;
	return(TRUE);
}

void FreeSocketInformation(DWORD Index, CListBox *pLB)
{
	LPSOCKET_INFORMATION SI = SocketArray[Index];
	DWORD i;
	char Str[81];

	closesocket(SI->Socket);

	sprintf_s(Str, sizeof(Str), "Closing socket number %d", SI->Socket);
	pLB->AddString(Str);

	GlobalFree(SI);

	// Сдвиг массива сокетов
	for (i = Index; i < TotalSockets; i++)
	{
		SocketArray[i] = SocketArray[i + 1];
	}
	TotalSockets--;
}


void CServerDlg::OnBnClickedPrint()
{
	// TODO: Add your control notification handler code here
	if (((CButton *)GetDlgItem(IDC_PRINT))->GetCheck() == 1)
		bPrint = true;
	else
		bPrint = false;
}
