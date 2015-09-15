#ifndef __TIMER_H
#define __TIMER_H

#pragma once

#include <windows.h>

//класс таймера
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

	//запустить таймер
	void startTimer();
	//обновить таймер
	void updateTimer();
	//получить временной отрезок между текущим замером и предыдущим
	//обратите внимание: время здесь не замеряется,
	// только лишь возвращается уже замерянный результат
	double getTimeInterval() { return currentCheckTime - lastCheckTime; }

private:
	LARGE_INTEGER start; //начальное время
	double frequency; //частота таймера
	double tickLength; //длина одного тика таймера
	double currentCheckTime; //текущий замер времени
	double lastCheckTime; //предыдущий замер времени
						  //инициализировать таймер
	void init();
	//вернуть частоту таймера
	double getFrequency();
	//вернуть время (от начального времени)
	double readTimer();
};

#endif