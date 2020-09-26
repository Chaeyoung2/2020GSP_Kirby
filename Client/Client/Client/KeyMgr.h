#pragma once

#include "framework.h"

class KeyMgr
{
//	// �̱���ȭ
//private:
//	static KeyMgr* m_pInstance;
//public:
//	static KeyMgr* GetInstance() {
//		if (nullptr == m_pInstance) {
//			m_pInstance = new KeyMgr;
//			return m_pInstance;
//		}
//	}
//	void DestroyInstance() {
//		if (m_pInstance) {
//			delete m_pInstance;
//			m_pInstance = nullptr;
//		}
//	}

public:
	bool	m_bKeyDown[256];	// Ű�� ���ȴ��� üũ�� �迭
	bool	m_bKeyUp[256];	// Ű�� �������� üũ�� �迭

public:
	KeyMgr(void);
	~KeyMgr(void);

public:
	bool StayKeyDown(INT nKey); // Ű�� ������ �ִ��� üũ
	bool OnceKeyDown(INT nKey); // Ű�� �ѹ� ���ȴ��� üũ
	bool OnceKeyUp(INT nKey);	// Ű�� �ѹ� ���ȴ� �������� üũ
	bool IsToggleKey(INT nKey);	// �ѹ� ���������� on off�� �ٲٵ���
};