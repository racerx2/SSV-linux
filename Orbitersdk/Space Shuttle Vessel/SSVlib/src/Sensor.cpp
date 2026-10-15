/******* SSV File Modification Notice *******
Date         Developer
2020/04/01   GLS
2020/04/15   GLS
2020/08/24   GLS
2021/01/20   GLS
2021/08/24   GLS
2022/08/05   GLS
2022/09/30   GLS
2022/11/17   GLS
2026/10/15   racerx2
********************************************/
#include <Sensor.h>
#include <MathSSV.h>
#include <stdlib.h>
#include <cassert>


Sensor::Sensor( double MinValue, double MaxValue, double FSerror, double MinVoltage, double MaxVoltage ):draw( (double)rand() / (RAND_MAX + 1.0) )// not upstream: the error's draw, kept; RAND_MAX + 1 overflows int on Linux (design ssv-flightrec rev 4)
{
	assert( (MinVoltage < MaxVoltage) && "Sensor::Sensor.(MinVoltage < MaxVoltage)" );

	this->MaxValue = MaxValue;
	this->MinValue = MinValue;
	this->MaxVoltage = MaxVoltage;
	this->MinVoltage = MinVoltage;

	conversor = (MaxVoltage - MinVoltage) / (MaxValue - MinValue);

	error = (MaxValue - MinValue) * FSerror;// not upstream: the full-scale error, its draw is applied in SetValue (design ssv-flightrec)
	return;
}

Sensor::~Sensor( void )
{
	return;
}

void Sensor::Connect( DiscreteBundle* pBundle, unsigned short iLine )
{
	dipOutput.Connect( pBundle, iLine );
	return;
}

void Sensor::Disconnect( void )
{
	dipOutput.ResetLine();// set to 0 before disconnecting
	dipOutput.Disconnect();
	return;
}

void Sensor::SetValue( double value )
{
	value += error * (draw.Get() - 0.5);// not upstream: the recording's error in a playback (design ssv-flightrec rev 4)

	double v = (value - MinValue) * conversor;
	v = range( MinVoltage, v, MaxVoltage );

	dipOutput.SetLine( static_cast<float>(v) );
	return;
}
