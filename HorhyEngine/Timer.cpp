#include "stdafx.h"
#include "Timer.h"

//���������������� ������
void Timer::init()
{
	//�������� ������� ������� � ����� ����
	frequency = getFrequency();
	tickLength = 1.0 / frequency;
}

//������� ������� �������
double Timer::getFrequency()
{
	LARGE_INTEGER freq;

	if (!QueryPerformanceFrequency(&freq))
		return  0;
	return (double) freq.QuadPart;
}

//������� ����� (�� ���������� �������)
double Timer::readTimer()
{
	//������������ ������ ������ ���� ����������
	DWORD_PTR oldMask = SetThreadAffinityMask(GetCurrentThread(), 0);
	//������ ������� �����
	LARGE_INTEGER currentTime;
	QueryPerformanceCounter(&currentTime);
	//������������ ����� ������������� ���� ����������
	SetThreadAffinityMask(GetCurrentThread(), oldMask);
	//������� ������� �����
	return (currentTime.QuadPart - start.QuadPart) * tickLength;
}

//��������� ������
void Timer::startTimer()
{
	//���������������� ������
	init();
	//������������ ������ ������ ���� ����������
	DWORD_PTR oldMask = SetThreadAffinityMask(GetCurrentThread(), 0);
	//������ ������� �����
	QueryPerformanceCounter(&start);
	//������������ ����� ������������� ���� ����������
	SetThreadAffinityMask(GetCurrentThread(), oldMask);
	//���������������� ������� � ������� ����� �������
	lastCheckTime = currentCheckTime = readTimer();
	//���������������� ����������� (����� ������ �� ������ � ��������)
	srand((int) start.QuadPart);
}

//�������� ������
void Timer::updateTimer()
{
	//������� ����� ���������� �������
	lastCheckTime = currentCheckTime;
	//������� ����������
	currentCheckTime = readTimer();
}
