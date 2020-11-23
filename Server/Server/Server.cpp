#include <iostream>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <stdlib.h>
#include <thread>
#include <vector>
#include <mutex>
#include <unordered_set>
#include "protocol.h"

using namespace std;

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "MSWSock.lib")

constexpr int KEY_SERVER = 1000000; // Ŭ���̾�Ʈ ���̵�� �򰥸��� �ʰ� ū ������

constexpr char OP_MODE_RECV = 0;
constexpr char OP_MODE_SEND = 1;
constexpr char OP_MODE_ACCEPT = 2;

constexpr int VIEW_LIMIT = 3;

int cur_playerCnt = 0;

struct OVER_EX {
	WSAOVERLAPPED wsa_over;
	char op_mode;
	WSABUF wsa_buf;
	unsigned char iocp_buf[MAX_BUFFER];
};


struct client_info {
	client_info() {}
	client_info(int _id, short _x, short _y, SOCKET _sock, bool _connected) {
		id = _id; x = _x; y = _y; sock = _sock; connected = _connected;
	}
	OVER_EX m_recv_over;
	mutex c_lock;
	int id;
	char name[MAX_ID_LEN];
	short x, y;
	SOCKET sock;
	bool connected = false;
	//char oType;
	unsigned char* m_packet_start;
	unsigned char* m_recv_start;
	mutex vl;
	unordered_set<int> view_list;
	int move_time;
};

mutex id_lock;//���̵� �־��ٶ� ��ȣ�������� ��ŷ
client_info g_clients[MAX_USER];
HANDLE h_iocp;
SOCKET g_listenSocket;
OVER_EX g_accept_over; // accept�� overlapped ����ü

void error_display(const char* msg, int err_no);
void ProcessPacket(int id);
void ProcessRecv(int id, DWORD iosize);
void ProcessMove(int id, char dir);
void WorkerThread();
void AddNewClient(SOCKET ns);
void DisconnectClient(int id);
void SendPacket(int id, void* p);
void SendLeavePacket(int to_id, int id);
void SendLoginOK(int id);
void SendEnterPacket(int to_id, int new_id);
void SendMovePacket(int to_id, int id);
bool IsNear(int p1, int p2);


int main()
{
	for (auto& cl : g_clients) {
		cl.connected = false;
	}

	wcout.imbue(std::locale("korean"));
	WSADATA WSAdata;
	WSAStartup(MAKEWORD(2, 0), &WSAdata);
	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
	g_listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_listenSocket), h_iocp, KEY_SERVER, 0); // iocp�� ���� ���� ���

	SOCKADDR_IN serverAddress;
	memset(&serverAddress, 0, sizeof(SOCKADDR_IN));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(SERVER_PORT);
	serverAddress.sin_addr.s_addr = INADDR_ANY;
	::bind(g_listenSocket, (sockaddr*)&serverAddress, sizeof(serverAddress));
	listen(g_listenSocket, 5);

	// Accept
	SOCKET cSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	g_accept_over.op_mode = OP_MODE_ACCEPT;
	g_accept_over.wsa_buf.len = static_cast<int>(cSocket); // ���� integer���� �׳�.. �־���.... ??
	ZeroMemory(&g_accept_over.wsa_over, sizeof(WSAOVERLAPPED));
	AcceptEx(g_listenSocket, cSocket, g_accept_over.iocp_buf, 0, 32, 32, NULL, &g_accept_over.wsa_over); // accept ex�� ������ ���� ���ڶ� Ŭ���̾�Ʈ�� ���� ���ϴ� ���� ������ (1006)

	vector <thread> workerthreads;
	for (int i = 0; i < 6; ++i) {
		workerthreads.emplace_back(WorkerThread);
	}
	for (auto& t : workerthreads)
		t.join();

	closesocket(g_listenSocket);
	WSACleanup();
	
	return 0;
}


void ProcessPacket(int id)
{
	char p_type = g_clients[id].m_packet_start[1]; // ��Ŷ Ÿ��
	switch (p_type) {
	case CS_LOGIN:
	{
		cs_packet_login* p = reinterpret_cast<cs_packet_login*>(g_clients[id].m_packet_start);
		g_clients[id].c_lock.lock();
		strcpy_s(g_clients[id].name, p->name);
		g_clients[id].c_lock.unlock();
		SendLoginOK(id);
		for (int i = 0; i < MAX_USER; ++i) {
			if (false == g_clients[i].connected) continue;
			if (id == i) continue;
			if (false == IsNear(i, id)) continue;
			if (0 == g_clients[i].view_list.count(id)) {
				g_clients[i].view_list.insert(id);
				SendEnterPacket(i, id);
			}
			if (0 == g_clients[id].view_list.count(i)) {
				g_clients[id].view_list.insert(i);
				SendEnterPacket(id, i);
			}
		}
	}
	break;
	case CS_MOVE:
	{
		cs_packet_move* p = reinterpret_cast<cs_packet_move*>(g_clients[id].m_packet_start);
		g_clients[id].move_time = p->move_time;
		ProcessMove(id, p->direction);
	}
	break;
	default:
	{
#ifdef DEBUG
		cout << "Unkown Packet type[" << p_type << "] from Client [" << id << "]\n";
#endif
		while (true);
	}
	break;
	}

}

void ProcessRecv(int id, DWORD iosize)
{
	// ��Ŷ ����.
	unsigned char p_size = g_clients[id].m_packet_start[0]; // ��Ŷ ������
	unsigned char* next_recv_ptr = g_clients[id].m_recv_start + iosize; // ������ ���� ptr
	while (p_size <= next_recv_ptr - g_clients[id].m_packet_start) { // ����
		ProcessPacket(id); // ��Ŷ ó��
		g_clients[id].m_packet_start += p_size; // ��Ŷ ó�������� ���� ��Ŷ �ּҷ�
		if (g_clients[id].m_packet_start < next_recv_ptr)
			p_size = g_clients[id].m_packet_start[0]; // ���� ��Ŷ�� ����� p_size�� ������Ʈ
		else
			break;
	}
	// ��Ŷ �����ϰ� �̸�ŭ ������.
	int left_data = next_recv_ptr - g_clients[id].m_packet_start;
	// ���۸� �� ������ �ʱ�ȭ�ؾ� ��. (������ �о������)
	if ((MAX_BUFFER - (next_recv_ptr - g_clients[id].m_recv_over.iocp_buf) < MIN_BUFFER)) { // ���۰� MIN_BUFFER ũ�⺸�� ������
		memcpy(g_clients[id].m_recv_over.iocp_buf, g_clients[id].m_packet_start, left_data); // ���� �ִ� ����Ʈ��ŭ copy 
		g_clients[id].m_packet_start = g_clients[id].m_recv_over.iocp_buf; 
		next_recv_ptr = g_clients[id].m_recv_start + left_data; // ������ ���� ��ġ ����Ŵ
	}
	DWORD recv_flag = 0;

	// ��𼭺��� �ٽ� ������
	g_clients[id].m_recv_start = next_recv_ptr;
	g_clients[id].m_recv_over.wsa_buf.buf = reinterpret_cast<CHAR*>(next_recv_ptr);
	g_clients[id].m_recv_over.wsa_buf.len = MAX_BUFFER - (next_recv_ptr - g_clients[id].m_recv_over.iocp_buf); // ���� �ִ� ���� �뷮
	g_clients[id].c_lock.lock();
	if (true == g_clients[id].connected)
		WSARecv(g_clients[id].sock, &g_clients[id].m_recv_over.wsa_buf, 1, NULL, &recv_flag, &g_clients[id].m_recv_over.wsa_over, NULL);
	g_clients[id].c_lock.unlock();
}

void ProcessMove(int id, char dir)
{
	short y = g_clients[id].y;
	short x = g_clients[id].x;

	switch (dir) {
	case MV_UP:
		if (y > 0) y--;
		break;
	case MV_DOWN:
		if (y < WORLD_HEIGHT - 1) y++;
		break;
	case MV_LEFT:
		if (x > 0) x--;
		break;
	case MV_RIGHT:
		if (x < WORLD_WIDTH - 1) x++;
		break;
	default:
#ifdef DEBUG
		cout << "Unknown Direction in CS_MOVE packet\n";
#endif
		while (true);
		break;
	}

	// �̵� �� �� ����Ʈ
	unordered_set<int> old_viewlist = g_clients[id].view_list; 

	g_clients[id].x = x;
	g_clients[id].y = y;

	// ������ �˸�
	SendMovePacket(id, id);

	// a
	//for (int i = 0; i < MAX_USER; ++i) {
	//	if (false == g_clients[i].in_use) continue;
	//	if (i == id) continue;
	//	SendMovePacket(i, id);
	//}
	// ----------------------- �þ� ó�� �����ϸ� �ּ� ������ ��.. ���� a �ڵ� ������ ��

	// �� ����Ʈ ó��
	//// �̵� �� ��ü-��ü�� ���� ���̳� �� ���̳� ó��
	unordered_set<int> new_viewlist; 
	for (int i = 0; i < MAX_USER; ++i) {
		if (id == i) continue;
		if (false == g_clients[i].connected) continue;
		if (true == IsNear(id, i)) { // i�� id�� �����ٸ�
			new_viewlist.insert(i); // �� �丮��Ʈ�� i�� �ִ´�
		}
	}
	// �þ� ó�� (3�� �õ�)-------------------------------
	for (int ob : new_viewlist) { // �̵� �� new viewlist (�þ߿� ����) ��ü�� ���� ó�� - enter�� ����, move�� �� ����
		if (0 == old_viewlist.count(ob)) { // �̵� ���� �þ߿� ������ ���
			g_clients[id].view_list.insert(ob); // �̵��� Ŭ���̾�Ʈ(id)�� ���� �丮��Ʈ�� �ִ´�
			SendEnterPacket(id, ob); // �̵��� Ŭ���̾�Ʈ(id)���� �þ߿� ���� ob�� enter�ϰ� �Ѵ�
			
			if (0 == g_clients[ob].view_list.count(id)) { // �þ߿� ���� ob�� �丮��Ʈ�� id�� �����ٸ�
				g_clients[ob].view_list.insert(id); // �þ߿� ���� ob�� �丮��Ʈ�� id�� �־��ְ�
				SendEnterPacket(ob, id); // ob���� id�� enter�ϰ� �Ѵ�
			}
			else {// �þ߿� ���� ob�� �丮��Ʈ�� id�� �־��ٸ� 
				SendMovePacket(ob, id); // ob���� id�� move�ϰ� �Ѵ�
			}
		}
		else {  // �̵� ���� �þ߿� �־��� ���
			if (0 != g_clients[ob].view_list.count(id)) { // �þ߿� ���� ob�� id�丮��Ʈ�� ���� �־�����
				SendMovePacket(ob, id); // ob���� id move ��Ŷ ����
			}
			else // id�� ob�� �丮��Ʈ�� �����ٸ�
			{ 
				g_clients[ob].view_list.insert(id); // ob�� �丮��Ʈ�� id �־��� 
				SendEnterPacket(ob, id); // ob���� id�� enter�ϰ� �Ѵ�
			}
		}
	}
	for (int ob : old_viewlist) { // �̵� �� old viewlist ���� ��ü�� ���� ó�� - leave ó�� �� ����, ������ ����
		if (0 == new_viewlist.count(ob)) { // �̵� �� old viewlist���� �ִµ� new viewlist�� ���� -> �þ߿��� �������
			g_clients[id].view_list.erase(ob); // �̵��� id�� �丮��Ʈ���� ob�� ����
			SendLeavePacket(id, ob); // id�� ob�� leave �ϰԲ�
			if (0 != g_clients[ob].view_list.count(id)) { // ob�� �丮��Ʈ�� id�� �־��ٸ�
				g_clients[ob].view_list.erase(id); // ob�� �丮��Ʈ������ id�� ����
				SendLeavePacket(ob, id); // ob�� id�� leave �ϰԲ�
			}
		}
	}
}
	//// �þ� ó�� (2�� �õ�)-----------------------------------
	//// 1. old vl�� �ְ� new vl���� �ִ� ��ü // ���ΰ� 
	//for (auto pl : old_viewlist) {
	//	if (0 == new_viewlist.count(pl)) continue;
	//	if (0 < g_clients[pl].view_list.count(id)) {
	//		SendMovePacket(pl, id);
	//	}
	//	else {
	//		g_clients[pl].view_list.insert(id);
	//		SendEnterPacket(pl, id);
	//	}
	//}
	//// 2. old vl�� ���� new_vl���� �ִ� ��ü
	//for (auto pl : new_viewlist) {
	//	if (0 < old_viewlist.count(pl)) continue;
	//	g_clients[id].view_list.insert(pl);
	//	SendEnterPacket(id, pl);
	//	if (0 == g_clients[pl].view_list.count(id)) {
	//		g_clients[pl].view_list.insert(id);
	//		SendEnterPacket(pl, id);
	//	}
	//	else {
	//		SendMovePacket(pl, id);
	//	}
	//}
	//// 3. old vl�� �ְ� new vl���� ���� �÷��̾�
	//for (auto pl : old_viewlist) {
	//	if (0 < new_viewlist.count(pl)) continue;
	//	g_clients[id].view_list.erase(pl);
	//	SendLeavePacket(id, pl);
	//	// SendLeavePacket(pl, id);
	//	if (0 < g_clients[pl].view_list.count(id)) {
	//		g_clients[pl].view_list.erase(id);
	//		SendLeavePacket(pl, id);
	//	}
	//}


	
	// �þ� ó�� (1�� �õ�) --------------------------------
	////// �þ߿� ���� ��ü ó��
	//for (int ob : new_viewlist) { // �� �丮��Ʈ�� �ִ� ��ü ob�� ���ٰ�
	//	if (0 == old_viewlist.count(ob)) { // id�� �õ� �丮��Ʈ�� ob�� �־��ٸ�
	//		g_clients[id].view_list.insert(ob);
	//		SendEnterPacket(id, ob);

	//		if (0 == g_clients[ob].view_list.count(id)) { // ���� �þ߿� ���ٸ�
	//			g_clients[ob].view_list.insert(id);
	//			SendEnterPacket(ob, id);
	//		}
	//		else { // �ִٸ�
	//		   // �̹� ���� �� ����Ʈ�� �����ϱ� ���� ��Ŷ�� ������ ��
	//			SendMovePacket(ob, id);
	//		}

	//	}
	//	else { // ������ �þ߿� �־���, �̵� �Ŀ��� �þ߿� �ִ� ��ü
	//	   // ��� �þ߿� �־��� ������ ���� ���� ������ ������ ����
	//	   // ��Ƽ �������
	//		if (0 != g_clients[ob].view_list.count(id)) {
	//			SendMovePacket(ob, id);
	//		}
	//		else {
	//			g_clients[ob].view_list.insert(id);
	//			SendEnterPacket(ob, id);
	//		}
	//	}
	//}

	//for (int ob : old_viewlist) { // �������� �þ߿� �־��µ�, �̵� �Ŀ� ����
	//	if (0 == new_viewlist.count(ob)) {
	//		g_clients[id].view_list.erase(id);
	//		SendLeavePacket(id, ob);
	//		if (0 != g_clients[ob].view_list.count(id)) {
	//			g_clients[ob].view_list.erase(id);
	//			SendLeavePacket(ob, id);
	//		}
	//		else {} //�ٸ� �ְ� �̹� ������ ����!

	//	}
	//}

	// �����׸� �˸��� ���� �ٸ� �÷��̾�鵵 �˰�! �θ��θ�
	//for (int i = 0; i < MAX_USER; ++i) {
	//	if (true == g_clients[i].connected) {
	//		SendMovePacket(i, id);
	//	}
	//}


}
void WorkerThread() {
	// �ݺ�
	// - �����带 iocp thread pool�� ��� (GQCS)
	// - iocp�� ó���� �ñ� i/o �Ϸ�
	// - ������ ó�� ������ q
	// - ���� i/o �Ϸ�, ������ ó��
	while (true) {
		DWORD io_size;
		ULONG_PTR iocp_key;
		int key;
		WSAOVERLAPPED* lpover;
		int ret = GetQueuedCompletionStatus(h_iocp, &io_size, &iocp_key, &lpover, INFINITE);
		key = static_cast<int>(iocp_key);
#ifdef DEBUG
		cout << "Completion Detected\n";
#endif
		if (FALSE == ret) { // max user exceed ���� ����. // 0�� ������ �������� �ʴ´�.
			error_display("GQCS Error : ", WSAGetLastError());
		}
		OVER_EX* over_ex = reinterpret_cast<OVER_EX*>(lpover);
		switch (over_ex->op_mode) {
		case OP_MODE_ACCEPT: 
		{ // �� Ŭ���̾�Ʈ accept
			AddNewClient(static_cast<SOCKET>(over_ex->wsa_buf.len)); 
		}
			break;
		case OP_MODE_RECV:
		{
			if (io_size == 0) { // Ŭ���̾�Ʈ ���� ����
				DisconnectClient(key);
			}
			else { // ��Ŷ ó��
#ifdef DEBUG
				cout << "Packet from Client [" << key << "]\n";
#endif
				ProcessRecv(key, io_size);
			}
		}
			break;
		case OP_MODE_SEND:
			delete over_ex;
			break;
		}
	}
}

void AddNewClient(SOCKET ns)
{
	// �� ���� id ã��
	int i;
	id_lock.lock();
	for (i = 0; i < MAX_USER; ++i) {
		if (false == g_clients[i].connected) break;
	}
	id_lock.unlock();
	// id �� ���� close
	if (MAX_USER == i) {
#ifdef DEBUG
		cout << "Max user limit exceeded\n";
#endif
		closesocket(ns);
	}
	// ���� ���� ������
	else {
		// g_clients �迭�� ���� �ֱ�
		g_clients[i].c_lock.lock();
		g_clients[i].id = i;
		g_clients[i].connected = true;
		g_clients[i].sock = ns;
		g_clients[i].name[0] = 0; // null string���� �ʱ�ȭ �ʼ�
		g_clients[i].c_lock.unlock();
		g_clients[i].m_packet_start = g_clients[i].m_recv_over.iocp_buf;
		g_clients[i].m_recv_over.op_mode = OP_MODE_RECV;
		g_clients[i].m_recv_over.wsa_buf.buf = reinterpret_cast<CHAR*>(g_clients[i].m_recv_over.iocp_buf); // ���� ������ �ּ� ����Ŵ
		g_clients[i].m_recv_over.wsa_buf.len = sizeof(g_clients[i].m_recv_over.iocp_buf);
		ZeroMemory(&g_clients[i].m_recv_over.wsa_over, sizeof(g_clients[i].m_recv_over.wsa_over));
		g_clients[i].m_recv_start = g_clients[i].m_recv_over.iocp_buf; // �� ��ġ���� �ޱ� �����Ѵ�.
		g_clients[i].x = rand() % WORLD_WIDTH;
		g_clients[i].y = rand() % WORLD_HEIGHT;
		// ������ iocp�� ���
		DWORD flags = 0;
		CreateIoCompletionPort(reinterpret_cast<HANDLE>(ns), h_iocp, i, 0); // ������ iocp�� ���
		// Recv
		g_clients[i].c_lock.lock();
		int ret = 0;
		if (true == g_clients[i].connected) {
			ret = WSARecv(g_clients[i].sock, &g_clients[i].m_recv_over.wsa_buf, 1, NULL, &flags, &(g_clients[i].m_recv_over.wsa_over), NULL);
		}
		g_clients[i].c_lock.unlock();
		if (ret == SOCKET_ERROR) {
			int error_no = WSAGetLastError();
			if (error_no != ERROR_IO_PENDING) {
				error_display("WSARecv : ", error_no);
			}
		}
	}
	// �ٽ� accept
	SOCKET cSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	g_accept_over.op_mode = OP_MODE_ACCEPT;
	g_accept_over.wsa_buf.len = static_cast<ULONG>(cSocket); // ���� integer���� �׳�.. �־���.... ??
	ZeroMemory(&g_accept_over.wsa_over, sizeof(g_accept_over.wsa_over));// accept overlapped ����ü ��� �� �ʱ�ȭ
	AcceptEx(g_listenSocket, cSocket, g_accept_over.iocp_buf, 0, 32, 32, NULL, &g_accept_over.wsa_over);
}

void DisconnectClient(int id)
{
	// �ֺ� Ŭ��鿡�� ������ �˸�
	for (int i = 0; i < MAX_USER; ++i) {
		if (true == g_clients[i].connected) {
			if (i != id) {
				if (0 != g_clients[i].view_list.count(id)) {// �丮��Ʈ�� �ִ��� Ȯ���ϰ� ����
					g_clients[i].view_list.erase(id);
					SendLeavePacket(i, id);
				}
//				else { // ������ ���� �ʿ䵵 ���� ��Ŷ ���� �ʿ䵵 ����

//				}
			}
		}
	}

	g_clients[id].c_lock.lock();
	g_clients[id].connected = false;
	g_clients[id].view_list.clear();
	closesocket(g_clients[id].sock);
	g_clients[id].sock = 0;
	g_clients[id].c_lock.unlock();

}

void SendPacket(int id, void* p)
{
	unsigned char* packet = reinterpret_cast<unsigned char*>(p);
	OVER_EX* send_over = new OVER_EX;
	memcpy(&send_over->iocp_buf, packet, packet[0]);
	send_over->op_mode = OP_MODE_SEND;
	send_over->wsa_buf.buf = reinterpret_cast<CHAR*>(send_over->iocp_buf);
	send_over->wsa_buf.len = packet[0];
	ZeroMemory(&send_over->wsa_over, sizeof(send_over->wsa_over));
	//Ŭ���̾�Ʈ�� ���Ͽ� �����ϹǷ� lock
	g_clients[id].c_lock.lock();
	if (true == g_clients[id].connected) // use ���̹Ƿ� sock�� ����ִ�
		WSASend(g_clients[id].sock, &send_over->wsa_buf, 1, NULL, 0, &send_over->wsa_over, NULL);
	g_clients[id].c_lock.unlock();
}

void SendLeavePacket(int to_id, int id)
{
	sc_packet_leave packet;
	packet.id = id;
	packet.size = sizeof(packet);
	packet.type = SC_LEAVE;
	SendPacket(to_id, &packet);
}

void SendLoginOK(int id)
{
	sc_packet_login_ok p;
	p.exp = 0;
	p.hp = 100;
	p.id = id;
	p.level = 1;
	p.size = sizeof(p);
	p.type = SC_LOGIN_OK;
	p.x = g_clients[id].x;
	p.y = g_clients[id].y;
	SendPacket(id, &p); // p�� ������ struct�� ���� ī�ǵż� ������
}

void SendEnterPacket(int to_id, int new_id)
{
	sc_packet_enter p;
	p.id = new_id;
	p.size = sizeof(p);
	p.type = SC_ENTER;
	p.x = g_clients[new_id].x;
	p.y = g_clients[new_id].y;
	g_clients[new_id].c_lock.lock();
	strcpy_s(p.name, g_clients[new_id].name);
	g_clients[new_id].c_lock.unlock();
	p.o_type = 0;
	SendPacket(to_id, &p);
}

void SendMovePacket(int to_id, int id)
{
	sc_packet_move p;
	p.id = id;
	p.size = sizeof(p);
	p.type = SC_MOVEPLAYER;
	p.x = g_clients[id].x;
	p.y = g_clients[id].y;
	p.move_time = g_clients[id].move_time;
	SendPacket(to_id, &p);
}

bool IsNear(int p1, int p2)
{
	int dist = (g_clients[p1].x - g_clients[p2].x) * (g_clients[p1].x - g_clients[p2].x);
	dist += (g_clients[p1].y - g_clients[p2].y) * (g_clients[p1].y - g_clients[p2].y);
	return dist <= VIEW_LIMIT * VIEW_LIMIT;
}

void error_display(const char* msg, int err_no) {
	WCHAR* h_mess;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		err_no,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&h_mess, 0, NULL);
	std::cout << msg;
	std::wcout << L"���� =>" << h_mess << std::endl;
	while (true);
	LocalFree(h_mess);
}

