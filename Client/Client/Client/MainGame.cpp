#include "framework.h"
#include "MainGame.h"

void RecvPacket(LPVOID classPtr) {
	MainGame* maingame = (MainGame*)classPtr;

	DWORD num_recv;
	DWORD flag = 0;

	const SOCKET* serverSocket = maingame->getServerSocket();
	WSABUF* recv_wsabuf = maingame->getRecvWsabuf();

	while (true) {
		int ret = WSARecv(*serverSocket, recv_wsabuf, 1, &num_recv, &flag, NULL, NULL);
		if (ret) {
			int err_code = WSAGetLastError();
			printf("Recv Error [%d]\n", err_code);
			break;
		}
		else {
			BYTE* ptr = reinterpret_cast<BYTE*>(recv_wsabuf->buf);
			int packet_size = ptr[0];
			int packet_type = ptr[1];
			OBJ* pPlayers = maingame->getPlayers();
			switch (packet_type) {
			case SC_LOGIN_OK:
			{
				sc_packet_login_ok* p = reinterpret_cast<sc_packet_login_ok*>(recv_wsabuf->buf);
				int packet_id = p->id;
				short ptX = p->x;
				short ptY = p->y;
				maingame->setMyid(packet_id);
				pPlayers[packet_id].id = packet_id;
				pPlayers[packet_id].ptX = ptX;
				pPlayers[packet_id].ptY = ptY;
				pPlayers[packet_id].connected = true;
				HBITMAP* hBitmaps = maingame->getBitmaps();
				pPlayers[packet_id].bitmap = hBitmaps[2];
				maingame->setObjectPoint(packet_id, (float)ptX, (float)ptY);
				maingame->setObjectRect(packet_id);

				//int packet_id;
				//memcpy(&packet_id, &(ptr[1]) + sizeof(char), sizeof(int));
				///*int*/short ptX, ptY;
				//memcpy(&ptX, &(ptr[2]) + sizeof(int), sizeof(short));
				//memcpy(&ptY, &(ptr[2]) + sizeof(int) + sizeof(short), sizeof(short));
				//maingame->setMyid(packet_id);
				//pPlayers[packet_id].id = packet_id;
				//pPlayers[packet_id].ptX = ptX;
				//pPlayers[packet_id].ptY = ptY;
				//pPlayers[packet_id].connected = true; // ���̵� �ο� �޾����ϱ� Ŀ��Ʈ �Ȱ���!
				//HBITMAP* hBitmaps = maingame->getBitmaps();
				//pPlayers[packet_id].bitmap = hBitmaps[2]; // �̶� ��Ʈ���� �־�����
				//maingame->setObjectPoint(packet_id, (float)ptX, (float)ptY);
				//maingame->setObjectRect(packet_id);
			}
			break;
			case SC_MOVEPLAYER:
			{
				sc_packet_move_player* p = reinterpret_cast<sc_packet_move_player*>(recv_wsabuf->buf);
				int packet_id = p->id;
				short ptX = p->x;
				short ptY = p->y;
				pPlayers[packet_id].ptX = ptX;
				pPlayers[packet_id].ptY = ptY;
				pPlayers[packet_id].connected = true;
				maingame->setObjectPoint(packet_id, (float)ptX, (float)ptY);
				maingame->setObjectRect(packet_id);

				//int packet_id = 0;
				//memcpy(&packet_id, &(ptr[2]), sizeof(int));

				//short ptX=0, ptY=0;
				//memcpy(&ptX, &(ptr[2]) + sizeof(int), sizeof(short));
				//memcpy(&ptY, &(ptr[2]) + sizeof(int) + sizeof(short), sizeof(short));
				//
				//// ��Ƽ �÷��̾��� ���� �迭�� �����ؾ� ��
				//maingame->setObjectPoint(packet_id, ptX, ptY);
				//maingame->setObjectRect(packet_id);

				//// �̱� �÷��̾��� ���� �̷��� ��
				////pPlayer->ptX = ptX;
				////pPlayer->ptY = ptY;
				////maingame->setObjectPoint();
				////maingame->setObjectRect();

				cout << "Recv Type[" << SC_MOVEPLAYER << "] Player's X[" << ptX << "] Player's Y[" << ptY << "]\n";
			}
			break;
			case SC_ENTER:
			{
				sc_packet_enter* packet = reinterpret_cast<sc_packet_enter*>(ptr);
				int packet_id = packet->id;
				pPlayers[packet_id].connected = true;
				//pPlayers[packet_id].o_type = packet->o_type;
				//memcpy(pPlayers[packet_id].name, packet->name, strlen(packet->name));
				pPlayers[packet_id].id = packet_id;
				pPlayers[packet_id].ptX = packet->x;
				pPlayers[packet_id].ptY = packet->y;
				HBITMAP* hBitmaps = maingame->getBitmaps();
				pPlayers[packet_id].bitmap = hBitmaps[2];
				maingame->setObjectPoint(packet_id, packet->x, packet->y);
				maingame->setObjectRect(packet_id);
			}
			break;
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
}

void MainGame::Render()
{
	HDC hdcMain = GetDC(g_hwnd);
	HDC hdcBuffer = CreateCompatibleDC(hdcMain);
	HDC hdcBackGround = CreateCompatibleDC(hdcMain);
	HDC hdcPlayer = CreateCompatibleDC(hdcMain);

	SelectObject(hdcBuffer, bitmaps[0]);
	SelectObject(hdcBackGround, bitmaps[1]);
	SelectObject(hdcPlayer, players[myid].bitmap);

	// render BackGround at BackBuffer
	BitBlt(hdcBuffer, 0, 0, WINCX, WINCY, hdcBackGround, 0, 0, SRCCOPY);
	// render Line
	for (int i = 0; i < 8; i++) {
		MoveToEx(hdcBuffer, TILESIZE * (i+1), 0, NULL);
		LineTo(hdcBuffer, TILESIZE * (i+1), WINCX);
		MoveToEx(hdcBuffer, 0, TILESIZE * (i+1), NULL);
		LineTo(hdcBuffer, WINCX, TILESIZE*(i+1));
	}
	// render Players
	for (int i = 0; i < MAX_USER; ++i) {
		if (players[i].connected == false) continue;
		TransparentBlt(hdcBuffer, /// �̹��� ����� ��ġ �ڵ�
			players[i].rect.left, players[i].rect.top, /// �̹����� ����� ��ġ x,y
			PLAYERCX, PLAYERCY, /// ����� �̹����� �ʺ�, ����
			hdcPlayer, /// �̹��� �ڵ�
			0, 0, /// ������ �̹����� �������� x,y 
			30, 30, /// ���� �̹����κ��� �߶� �̹����� �ʺ�,����
			RGB(0, 255, 255) /// �����ϰ� �� ����
		);
	}
	// render BackBuffer at MainDC (Double Buffering)
	BitBlt(hdcMain, 0, 0, WINCX, WINCY, hdcBuffer, 0, 0, SRCCOPY);

	DeleteDC(hdcPlayer);
	DeleteDC(hdcBackGround);
	DeleteDC(hdcBuffer);

	ReleaseDC(g_hwnd, hdcMain);
}


void MainGame::InputKeyState(int key)
{
	int x = 0, y = 0;

	// -> �ټ� Ŭ�󿡼� �ߺ� �Է��� ��.. ������!
	//if (pKeyMgr->OnceKeyUp(VK_UP)) {
	//	y -= 1;
	//}
	//else if (pKeyMgr->OnceKeyUp(VK_DOWN)) {
	//	y += 1;
	//}
	//else if (pKeyMgr->OnceKeyUp(VK_LEFT)) {
	//	x -= 1;
	//}
	//else if (pKeyMgr->OnceKeyUp(VK_RIGHT)) {
	//	x += 1;
	//}

	// 0: up, 1:down, 2:left, 3:right
	switch (key) {
	case 0: y -= 1; break;
	case 1: y += 1; break;
	case 2: x -= 1; break;
	case 3: x += 1; break;
	}

	// �̶� �� �ѹ� ������ ����.
	cs_packet_up *myPacket = reinterpret_cast<cs_packet_up*>(send_buffer);
	myPacket->size = sizeof(myPacket);
	myPacket->id = players[myid].id;
	send_wsabuf.len = sizeof(myPacket);

	if (0 != x) {
		if (1 == x) myPacket->type = CS_INPUTRIGHT;
		else myPacket->type = CS_INPUTLEFT;
		DWORD num_sent;
		int ret = WSASend(serverSocket, &send_wsabuf, 1, &num_sent, 0, NULL, NULL);
		cout << "Sent " << send_wsabuf.len << "Bytes\n";
		if (ret) {
			int error_code = WSAGetLastError();
			//printf("Error while sending packet [%d]", error_code);
		}
		//ReadPacket();
	}
	if (0 != y) {
		if (1 == y) myPacket->type = CS_INPUTDOWN;
		else myPacket->type = CS_INPUTUP;
		DWORD num_sent;
		int ret = WSASend(serverSocket, &send_wsabuf, 1, &num_sent, 0, NULL, NULL);
		cout << "Sent " << send_wsabuf.len << "Bytes\n";
		if (ret) {
			int error_code = WSAGetLastError();
			//printf("Error while sending packet [%d]", error_code);
		}
		//ReadPacket();
	}

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
	}

	//WSAAsyncSelect(serverSocket, g_hwnd, WM_SOCKET, FD_CLOSE | FD_READ); // ������ �޽����ε� ���о������.. ����/..
	send_wsabuf.buf = send_buffer;
	send_wsabuf.len = BUF_SIZE;
	recv_wsabuf.buf = recv_buffer;
	recv_wsabuf.len = BUF_SIZE;

	recvThread = thread{ RecvPacket, this };
	// ReadPacket(); -> ���� �����忡�� �ޱ��
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
