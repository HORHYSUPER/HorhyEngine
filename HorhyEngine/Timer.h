#ifndef __TIMER_H
#define __TIMER_H

#pragma once

#include <windows.h>

//����� �������
class Timer
{
public:
	Timer() :
		frequency(0),
		tickLength(0),
		currentCheckTime(0),
		lastCheckTime(0)
	{}
	~Timer();

	//��������� ������
	void startTimer();
	//�������� ������
	void updateTimer();
	//�������� ��������� ������� ����� ������� ������� � ����������
	//�������� ��������: ����� ����� �� ����������,
	// ������ ���� ������������ ��� ���������� ���������
	double getTimeInterval() { return currentCheckTime - lastCheckTime; }

private:
	LARGE_INTEGER start; //��������� �����
	double frequency; //������� �������
	double tickLength; //����� ������ ���� �������
	double currentCheckTime; //������� ����� �������
	double lastCheckTime; //���������� ����� �������
						  //���������������� ������
	void init();
	//������� ������� �������
	double getFrequency();
	//������� ����� (�� ���������� �������)
	double readTimer();
};

#endif