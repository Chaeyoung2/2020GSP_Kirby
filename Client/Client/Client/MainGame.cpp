#include "framework.h"
#include "MainGame.h"

mutex pl;
int scrollX = 0;
int scrollY = 0;
int myid = -1;

SOCKET serverSocket;
WSABUF send_wsabuf;
WSABUF recv_wsabuf;
char	packet_buffer[BUF_SIZE];
char 	send_buffer[BUF_SIZE] = "";
char	recv_buffer[BUF_SIZE] = "";
DWORD		in_packet_size = 0;
int		saved_packet_size = 0;
DWORD send_last_time = 0; // ���������� send�ϰ� �󸶳� ��������


void ProcessPacket(char* ptr, MainGame* maingame)
{
	int packet_size = ptr[0];
	int packet_type = ptr[1];
	OBJ* pPlayers = maingame->getPlayers();
	switch (packet_type) {
	case SC_LOGIN_OK:
	{
		sc_packet_login_ok* p = reinterpret_cast<sc_packet_login_ok*>(ptr);
		int packet_id = p->id;
		if (pPlayers[packet_id].connected == true) break;
		short ptX = p->x;
		short ptY = p->y;
		myid = packet_id;
		pl.lock();
		pPlayers[packet_id].id = packet_id;
		pPlayers[packet_id].ptX = ptX;
		pPlayers[packet_id].ptY = ptY;
		pPlayers[packet_id].connected = true;
		HBITMAP* hBitmaps = maingame->getBitmaps();
		pPlayers[packet_id].bitmap = hBitmaps[2];
		maingame->setObjectPoint(packet_id, (float)ptX, (float)ptY);
		maingame->setObjectRect(packet_id);
		// �α��� ok ���� ������
		int t_id = GetCurrentProcessId();
		char tempBuffer[MAX_NICKNAME] = "";
		sprintf_s(tempBuffer, "P%03d", t_id % 1000);
		pPlayers[packet_id].name = new char[MAX_NICKNAME];
		ZeroMemory(pPlayers[packet_id].name, MAX_NICKNAME);
		memcpy(pPlayers[packet_id].name, tempBuffer, strlen(tempBuffer));
		pl.unlock();
		// ��ǥ�� ���� ��ũ�� ����
		maingame->setScroll(ptX, ptY);
	}
	break;
	case SC_MOVEPLAYER:
	{
		sc_packet_move* p = reinterpret_cast<sc_packet_move*>(ptr);
		int packet_id = p->id;
		short ptX = p->x;
		short ptY = p->y;
		pl.lock();
		pPlayers[packet_id].ptX = ptX;
		pPlayers[packet_id].ptY = ptY;
		pl.unlock();
		maingame->setObjectPoint(packet_id, (float)ptX, (float)ptY);
		maingame->setObjectRect(packet_id);
		if(myid == packet_id)
			maingame->setScroll(ptX, ptY); // ��ǥ�� ���� ��ũ�� ����

		// cout << "Recv Type[" << SC_MOVEPLAYER << "] Player's X[" << ptX << "] Player's Y[" << ptY << "]\n";
	}
	break;
	case SC_ENTER:
	{
		sc_packet_enter* packet = reinterpret_cast<sc_packet_enter*>(ptr);
		int packet_id = packet->id;
		pl.lock();
		pPlayers[packet_id].connected = true;
		//pPlayers[packet_id].o_type = packet->o_type;
		//memcpy(pPlayers[packet_id].name, packet->name, strlen(packet->name));
		pPlayers[packet_id].id = packet_id;
		pPlayers[packet_id].ptX = packet->x;
		pPlayers[packet_id].ptY = packet->y;
		HBITMAP* hBitmaps = maingame->getBitmaps();
		if (packet_id < MAX_USER) // player bitmap
			pPlayers[packet_id].bitmap = hBitmaps[2];
		else // NPC bitmap
			pPlayers[packet_id].bitmap = hBitmaps[3]; 
		pPlayers[packet_id].name = new char[MAX_NICKNAME];
		ZeroMemory(pPlayers[packet_id].name, MAX_NICKNAME);
		memcpy(pPlayers[packet_id].name, packet->name, strlen(packet->name));
		pl.unlock();
		maingame->setObjectPoint(packet_id, packet->x, packet->y);
		maingame->setObjectRect(packet_id);
	}
	break;
	case SC_LEAVE:
	{
		sc_packet_leave* my_packet = reinterpret_cast<sc_packet_leave*>(ptr);
		int packet_id = my_packet->id;
		pl.lock();
		delete pPlayers[packet_id].name; // Enter �� �� new �������
		ZeroMemory(&(pPlayers[packet_id]), sizeof(pPlayers[packet_id]));
		pPlayers[packet_id].connected = false;
		pl.unlock();
	}
	break;
	case SC_CHAT: 
	{
		sc_packet_chat* p = reinterpret_cast<sc_packet_chat*>(ptr);
		int id = p->id;
		WCHAR* wmess;
		int nChars = MultiByteToWideChar(CP_ACP, 0, p->message, -1, NULL, 0);
		wmess = new WCHAR[nChars];
		MultiByteToWideChar(CP_ACP, 0, p->message, -1, (LPWSTR)wmess, nChars);
		wcscpy_s(pPlayers[id].chat_buf, wmess);
		pPlayers[id].timeout = high_resolution_clock::now() + 1s;
	}
	break;
	default:
	{
		printf("Unknown Packet\n");
	}
	break;
	}

}
void RecvPacket(LPVOID classPtr) {
	MainGame* maingame = (MainGame*)classPtr;

	DWORD num_recv;
	DWORD flag = 0;

	while (true) {
		int ret = WSARecv(serverSocket, &recv_wsabuf, 1, &num_recv, &flag, NULL, NULL);
		if (ret) {
			int err_code = WSAGetLastError();
			printf("Recv Error [%d]\n", err_code);
			break;
		}
		char* ptr = reinterpret_cast<char*>(recv_buffer);
		while (0 != num_recv) {
			if (0 == in_packet_size)
				in_packet_size = ptr[0];
			if (num_recv + saved_packet_size >= in_packet_size) {
				memcpy(packet_buffer + saved_packet_size, ptr, in_packet_size - saved_packet_size);
				ProcessPacket(packet_buffer, maingame);
				ptr += in_packet_size - saved_packet_size;
				num_recv -= in_packet_size - saved_packet_size;
				in_packet_size = 0;
				saved_packet_size = 0;
			}
			else {
				memcpy(packet_buffer + saved_packet_size, ptr, num_recv);
				saved_packet_size += num_recv;
				num_recv = 0;
			}
		}

	}
}


MainGame::MainGame()
{
}

MainGame::~MainGame()
{
	Release();
}

void MainGame::Start()
{
	hdc = GetDC(g_hwnd);
	LoadBitmaps();

	setObjectPoint(0, 0, 0);
	setObjectRect(0);

	InitNetwork();

}

void MainGame::Update()
{
	//InputKeyState();
	send_last_time += 1;

	//// 0: up, 1:down, 2:left, 3:right
	//if (GetAsyncKeyState(VK_UP))
	//	InputKeyState(0);
	//if (GetAsyncKeyState(VK_DOWN))
	//	InputKeyState(1);
	//if (GetAsyncKeyState(VK_LEFT))
	//	InputKeyState(2);
	//if (GetAsyncKeyState(VK_RIGHT))
	//	InputKeyState(3);
}

void MainGame::Render()
{
	HDC hdcMain = GetDC(g_hwnd);
	HDC hdcBuffer = CreateCompatibleDC(hdcMain);
	HDC hdcBackGround = CreateCompatibleDC(hdcMain);
	HDC hdcPlayer = CreateCompatibleDC(hdcMain);
	HDC hdcNPC = CreateCompatibleDC(hdcMain);

	SelectObject(hdcBuffer, bitmaps[0]);
	SelectObject(hdcBackGround, bitmaps[1]);
	SelectObject(hdcPlayer, bitmaps[2]);
	SelectObject(hdcNPC, bitmaps[3]);

	int playerptX = 0, playerptY = 0;

	// render BackGround at BackBuffer
	BitBlt(hdcBuffer, 0, 0, WINCX, WINCY, hdcBackGround, 0, 0, SRCCOPY);
	// render Line
	for (int i = 0; i < TILESCREENMAX + 1; i++) {
		MoveToEx(hdcBuffer, TILESIZE * (i), 0, NULL);
		LineTo(hdcBuffer, TILESIZE * (i) , TILESIZE * (TILESCREENMAX));
		MoveToEx(hdcBuffer, 0 , TILESIZE * (i) , NULL);
		LineTo(hdcBuffer, TILESIZE * (TILESCREENMAX), TILESIZE*(i) );
	}
	// render Players
	for (int i = 0; i < MAX_USER + NUM_NPC; ++i) {
		if (players[i].connected == false) continue;
		if (i < MAX_USER) { // Player
			TransparentBlt(hdcBuffer, /// �̹��� ����� ��ġ �ڵ�
				players[i].rect.left + scrollX, players[i].rect.top + scrollY, /// �̹����� ����� ��ġ x,y
				PLAYERCX, PLAYERCY, /// ����� �̹����� �ʺ�, ����
				hdcPlayer, /// �̹��� �ڵ�
				0, 0, /// ������ �̹����� �������� x,y 
				30, 30, /// ���� �̹����κ��� �߶� �̹����� �ʺ�,����
				RGB(0, 255, 255) /// �����ϰ� �� ����
			);
			// render Text(info)
			//if (i == myid) {
			//	// �� ĳ���� ����
			//	TextOut(hdcBuffer, players[i].rect.left + scrollX, players[i].y + 3 + scrollY, L"It's me!", lstrlen(L"It's me"));
			//	playerptX = players[i].ptX; playerptY = players[i].ptY; // �迭 ��� �д°� ����
			//}
			// ���� ���
			//TCHAR lpOut[128]; // ��ǥ
			//wsprintf(lpOut, TEXT("(%d, %d)"), (int)(players[i].ptX), (int)(players[i].ptY));
			//TextOut(hdcBuffer, players[i].rect.left + scrollX, players[i].y - 40 + scrollY, lpOut, lstrlen(lpOut));
			if (players[i].name != nullptr) {
				TCHAR lpNickname[MAX_NICKNAME];	// �г���
				size_t convertedChars = 0;
				size_t newsize = strlen(players[i].name) + 1;
				mbstowcs_s(&convertedChars, lpNickname, newsize, players[i].name, _TRUNCATE);
				TextOut(hdcBuffer, players[i].rect.left + scrollX, players[i].y - 25 + scrollY, (wchar_t*)lpNickname, lstrlen(lpNickname));
			}
		}
		else { // NPC
			TransparentBlt(hdcBuffer, /// �̹��� ����� ��ġ �ڵ�
				players[i].rect.left + scrollX, players[i].rect.top + scrollY, /// �̹����� ����� ��ġ x,y
				MON1CX, MON1CY, /// ����� �̹����� �ʺ�, ����
				hdcNPC, /// �̹��� �ڵ�
				0, 0, /// ������ �̹����� �������� x,y 
				20, 30, /// ���� �̹����κ��� �߶� �̹����� �ʺ�,����
				RGB(0, 255, 255) /// �����ϰ� �� ����
			);
			// ���� ���
			//TCHAR lpOut[128]; // ��ǥ
			//wsprintf(lpOut, TEXT("(%d, %d)"), (int)(players[i].ptX), (int)(players[i].ptY));
			//TextOut(hdcBuffer, players[i].rect.left + 10 + scrollX, players[i].y - 40 + scrollY, lpOut, lstrlen(lpOut));
			if (players[i].name != nullptr) {
				TCHAR lpNickname[MAX_NICKNAME];	// �г���
				size_t convertedChars = 0;
				size_t newsize = strlen(players[i].name) + 1;
				mbstowcs_s(&convertedChars, lpNickname, newsize, players[i].name, _TRUNCATE);
				TextOut(hdcBuffer, players[i].rect.left + 5 + scrollX, players[i].y - 25 + scrollY, (wchar_t*)lpNickname, lstrlen(lpNickname));
			}
		}
		// chat message
		if (high_resolution_clock::now() < players[i].timeout) {
			TextOut(hdcBuffer, players[i].rect.left + scrollX, players[i].y + scrollY, players[i].chat_buf, lstrlen(players[i].chat_buf));
		}
	}
	// render Coordinates
	int pointsCount = WORLD_WIDTH / 8; // 0, 8, 16, 24, .. 
	for (int i = 0; i < pointsCount; ++i) {
		for (int j = 0; j < pointsCount; ++j) {
			int ptX = j * 8;
			int ptY = i * 8;
			int posX = TILESIZE * ptX /*+ TILESIZE * 0.5*/;
			int posY = TILESIZE * ptY + TILESIZE * 0.25;
			if ( (ptX >= playerptX - TILESCREENMAX * 0.5 && ptX <= playerptX + TILESCREENMAX * 0.5) 
			&& (ptY >= playerptY - TILESCREENMAX * 0.5 && ptY <= playerptY + TILESCREENMAX * 0.5) ){
				TCHAR lpOut[128]; // ��ǥ
				wsprintf(lpOut, TEXT("(%d, %d)"), ptX, ptY);
				TextOut(hdcBuffer,
					posX + scrollX, posY + scrollY, lpOut, lstrlen(lpOut));
			}
		}
	}
	// render BackBuffer at MainDC (Double Buffering)
	BitBlt(hdcMain, 0, 0, WINCX, WINCY, hdcBuffer, 0, 0, SRCCOPY);

	DeleteDC(hdcNPC);
	DeleteDC(hdcPlayer);
	DeleteDC(hdcBackGround);
	DeleteDC(hdcBuffer);

	ReleaseDC(g_hwnd, hdcMain);
}


void MainGame::InputKeyState(int key)
{
	if (send_last_time < 10) return;
	send_last_time = 0;

	int x = 0, y = 0;

	// 0: up, 1:down, 2:left, 3:right
	switch (key) {
	case 0: 
		y -= 1;
		//if (players[myid].ptY >= 0 && players[myid].ptY <= TILEMAX - 1) 
		//	scrollY += 70;
		//else g_scrollY = TILESIZE * players[myid].ptY;
		break;
	case 1: 
		y += 1;
		//if (players[myid].ptY >= 0 && players[myid].ptY <= TILEMAX - 1) 
		//	scrollY -= 70;
		//else g_scrollY = TILESIZE * players[myid].ptY;
		break;
	case 2: 
		x -= 1;
		//if (players[myid].ptX >= 0 && players[myid].ptX <= TILEMAX - 1) 
		//	scrollX += 70;
		//else g_scrollX = TILESIZE * players[myid].ptX;
		break;
	case 3: 
		x += 1;
		//if (players[myid].ptX >= 0 && players[myid].ptX <= TILEMAX - 1) 
		//	scrollX -= 70;
		//else g_scrollX = TILESIZE * players[myid].ptX;
		break;
	}


	// �̶� �� �ѹ� ������ ����.
	cs_packet_move* p = reinterpret_cast<cs_packet_move*>(send_buffer);
	p->size = sizeof(p);
	send_wsabuf.len = sizeof(p);
	DWORD iobyte;
	p->type = CS_MOVE;
	if (0 != x) {
		if (1 == x)
			p->direction = MV_RIGHT;
		else
			p->direction = MV_LEFT;
	}
	else if (0 != y) {
		if (1 == y)
			p->direction = MV_DOWN;
		else
			p->direction = MV_UP;
	}
	int ret = WSASend(serverSocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
	if (ret) {
		int error_code = WSAGetLastError();
		printf("Error while send packet [%d]", error_code);
	}


	//-----------
	//SendPacket(&p);

	//InvalidateRect(g_hwnd, NULL, TRUE);

}


void MainGame::setObjectPoint(int id, float ptX, float ptY)
{
	// ���� ��Ƽ�÷��̾�ϱ� �迭�� ���� !
	players[id].ptX = ptX;
	players[id].ptY = ptY;
	players[id].x = ptX * TILESIZE + TILESIZE * 0.5f;
	players[id].y = ptY * TILESIZE + TILESIZE * 0.5f;
	//// ü���ǿ� �°� // point�� ���� ��ġ ����
	//player.x = player.ptX * TILESIZE + TILESIZE * 0.5f;
	//player.y = player.ptY * TILESIZE + TILESIZE * 0.5f;
}

void MainGame::setObjectRect(int id)
{
	// ���� ��Ƽ�÷��̾�ϱ� �迭�� ���� !
	players[id].rect.left = long(players[id].x - PLAYERCX * 0.5f);
	players[id].rect.right = long(players[id].x + PLAYERCX * 0.5f);
	players[id].rect.top = long(players[id].y - PLAYERCY * 0.5f);
	players[id].rect.bottom = long(players[id].y + PLAYERCY * 0.5f);
	//// rect ������ �θ� TranasparentBlt �� �� ������
	//player.rect.left = long(player.x - PLAYERCX * 0.5f);
	//player.rect.right = long(player.x + PLAYERCX * 0.5f);
	//player.rect.top = long(player.y - PLAYERCY * 0.5f);
	//player.rect.bottom = long(player.y + PLAYERCY * 0.5f);
}

void MainGame::setScroll(int ptX, int ptY)
{

	scrollX = -(ptX * TILESIZE) + (WINCX * 0.5);
	scrollY = -(ptY * TILESIZE) + (WINCY * 0.5);


	// ��ũ�� ó��
	if (scrollX < WINCX - (TILESIZE * TILEMAX) - (WINCX * 0.5))
		scrollX = WINCX - (TILESIZE * TILEMAX) - (WINCX * 0.5);
	if (scrollX > WINCX * 0.5)
		scrollX = WINCX * 0.5;
	if (scrollY < WINCY - (TILESIZE * TILEMAX) - (WINCY * 0.5))
		scrollY = WINCY - (TILESIZE * TILEMAX) - (WINCY * 0.5);
	if (scrollY > WINCY * 0.5)
		scrollY = WINCY * 0.5;


	//cout << "scrollX : " << scrollX << endl;
	//cout << "scrollY : " << scrollY << endl;
}

bool MainGame::isNear(int x1, int y1, int x2, int y2, int viewlimit)
{
	int dist = (x1 - x2) * (x1 - x2);
	dist += (y1 - y2) * (y1 - y2);
	return dist <= viewlimit * viewlimit;
}



void MainGame::LoadBitmaps()
{
	// BackBuffer
	HBITMAP tempBitmap = (HBITMAP)LoadImage(NULL, L"Image/BackBuffer.bmp", IMAGE_BITMAP, WINCX, WINCY, LR_LOADFROMFILE | LR_CREATEDIBSECTION); // ��Ʈ�� �ε� ���2
	if (NULL == tempBitmap) {
		MessageBox(g_hwnd, L"Image/BackBuffer.bmp", L"Failed (LoadImage)", MB_OK);
		return;
	}
	else
		bitmaps[0] = tempBitmap;

	// BackGround
	tempBitmap = (HBITMAP)LoadImage(NULL, L"Image/BackGround.bmp", IMAGE_BITMAP, WINCX, WINCY, LR_LOADFROMFILE | LR_CREATEDIBSECTION); // ��Ʈ�� �ε� ���2
	if (NULL == tempBitmap) {
		MessageBox(g_hwnd, L"Image/BackGround.bmp", L"Failed (LoadImage)", MB_OK);
		return;
	}
	else
		bitmaps[1] = tempBitmap;

	// Player
	tempBitmap = (HBITMAP)LoadImage(NULL, L"Image/Kirby.bmp", IMAGE_BITMAP, 150, 90, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
	if (NULL == tempBitmap) {
		MessageBox(g_hwnd, L"Image/Kirby.bmp", L"Failed (LoadImage)", MB_OK);
		return;
	}
	else
		bitmaps[2] = tempBitmap;

	// Waddle
	tempBitmap = (HBITMAP)LoadImage(NULL, L"Image/Waddle.bmp", IMAGE_BITMAP, 150, 90, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
	if (NULL == tempBitmap) {
		MessageBox(g_hwnd, L"Image/Waddle.bmp", L"Failed (LoadImage)", MB_OK);
		return;
	}
	else
		bitmaps[3] = tempBitmap;
}

void MainGame::InitNetwork()
{
	WSADATA WSAdata;
	WSAStartup(MAKEWORD(2, 0), &WSAdata);
	serverSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	SOCKADDR_IN serverAddress;
	memset(&serverAddress, 0, sizeof(SOCKADDR_IN));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(PORT);
	inet_pton(AF_INET, SERVER_IP, &serverAddress.sin_addr);
	if (connect(serverSocket, (sockaddr*)&serverAddress, sizeof(SOCKADDR_IN))) {
		MessageBox(g_hwnd, L"WSAConnect", L"Failed", MB_OK);
		return;
	}

	//WSAAsyncSelect(serverSocket, g_hwnd, WM_SOCKET, FD_CLOSE | FD_READ); // ������ �޽����ε� ���о������.. ����/..
	send_wsabuf.buf = send_buffer;
	send_wsabuf.len = BUF_SIZE;
	recv_wsabuf.buf = recv_buffer;
	recv_wsabuf.len = BUF_SIZE;

	recvThread = thread{ RecvPacket, this };
	// ReadPacket(); -> ���� �����忡�� �ޱ��

	// �ʱ⿡ login �õ� ��Ŷ ����
	//cs_packet_login login_packet;
	//login_packet.size = sizeof(login_packet);
	//login_packet.type = CS_LOGIN;
	int t_id = GetCurrentProcessId();
	//sprintf_s(login_packet.name, "P%03d", t_id % 1000);

	// �̶� �� �ѹ� ������ ����.
	cs_packet_login* p = reinterpret_cast<cs_packet_login*>(send_buffer);
	p->size = sizeof(p);
	send_wsabuf.len = sizeof(p);
	DWORD iobyte;
	p->type = CS_LOGIN;
	sprintf_s(p->name, "P%03d", t_id % 1000);

	int ret = WSASend(serverSocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
	if (ret) {
		int error_code = WSAGetLastError();
		printf("Error while send packet [%d]", error_code);
	}
	//---
	//SendPacket(&login_packet);

	// ������� mynickname�� �г��� ���� (char to wchar_t)
	char tempBuffer[MAX_NICKNAME] = "";
	strcpy_s(tempBuffer, p->name);
	size_t convertedChars = 0;
	size_t newsize = strlen(tempBuffer) + 1;
	mbstowcs_s(&convertedChars, mynickname, newsize, tempBuffer, _TRUNCATE);

	//strcpy_s(mynickname, login_packet.name);
	//mynickname = new wchar_t;
}

void MainGame::SendPacket(void* packet)
{
	char* p = reinterpret_cast<char*>(packet);
	send_wsabuf.buf = p;
	send_wsabuf.len = p[0];
	DWORD num_sent;
	int ret = WSASend(serverSocket, &send_wsabuf, 1, &num_sent, 0, NULL, NULL);
	if (ret) {
		int error_code = WSAGetLastError();
		//printf("Error while sending packet [%d]", error_code);
	}
}


void MainGame::Release()
{
	DeleteObject(bitmaps[0]);
	DeleteObject(bitmaps[1]);
	DeleteObject(bitmaps[2]);

	
	closesocket(serverSocket);
	WSACleanup();

	recvThread.join();
}
