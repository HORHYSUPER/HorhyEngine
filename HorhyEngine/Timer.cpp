#include "stdafx.h"
#include "Timer.h"

//инициализировать таймер
void Timer::init()
{
	//получить частоту таймера и длину тика
	frequency = getFrequency();
	tickLength = 1.0 / frequency;
}

//вернуть частоту таймера
double Timer::getFrequency()
{
	LARGE_INTEGER freq;

	if (!QueryPerformanceFrequency(&freq))
		return  0;
	return (double) freq.QuadPart;
}

//вернуть время (от начального времени)
double Timer::readTimer()
{
	//использовать только первое ядро процессора
	DWORD_PTR oldMask = SetThreadAffinityMask(GetCurrentThread(), 0);
	//засечь текущее время
	LARGE_INTEGER currentTime;
	QueryPerformanceCounter(&currentTime);
	//восстановить маску использования ядер процессора
	SetThreadAffinityMask(GetCurrentThread(), oldMask);
	//вернуть текущее время
	return (currentTime.QuadPart - start.QuadPart) * tickLength;
}

//запустить таймер
void Timer::startTimer()
{
	//инициализировать таймер
	init();
	//использовать только первое ядро процессора
	DWORD_PTR oldMask = SetThreadAffinityMask(GetCurrentThread(), 0);
	//засечь текущее время
	QueryPerformanceCounter(&start);
	//восстановить маску использования ядер процессора
	SetThreadAffinityMask(GetCurrentThread(), oldMask);
	//инициализировать текущий и прошлый замер времени
	lastCheckTime = currentCheckTime = readTimer();
	//инициализировать рандомайзер (можно убрать из класса в принципе)
	srand((int) start.QuadPart);
}

//обновить таймер
void Timer::updateTimer()
{
	//текущий замер становится прошлым
	lastCheckTime = currentCheckTime;
	//текущий замеряется
	currentCheckTime = readTimer();
}
