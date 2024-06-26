/* Arduino PT2314 Library
 * Copyright (C) 2013 by Andy Karpov <andy.karpov@gmail.com>
 *
 * This file is part of the Arduino PT2314 Library
 *
 * This Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * <http://www.gnu.org/licenses/>.
 */

#include <Arduino.h>
#include <log.h>
#include <Wire.h>
#include <PT2314.h>

unsigned char PT2314::eq_table[] = {
		0,	// -14
		0,	// -13
		1,	// -12
		1,	// -11
		2,	// -10
		2,	// -9
		3,	// -8
		3,	// -7
		4,	// -6
		4,	// -5
		5,	// -4
		5,	// -3
		6,	// -2
		6,	// -1
		7,	// 0
		14, // 1
		14, // 2
		13, // 3
		13, // 4
		12, // 5
		12, // 6
		11, // 7
		11, // 8
		10, // 9
		10, // 10
		9,	// 11
		9,	// 12
		8,	// 13
		8,	// 14
};

int PT2314::volume_to_pt2314(int vol)
{
	// volume is controlled by sending an audio byte with 00 in the 2 most significant bits
	// and the volume level in the 6 least significant bits
	// 0b00111111 = 63 (decimal) is the minimum volume:  -78.75dB
	return 63 - ((vol * 63) / 100);
}

int PT2314::eq_to_pt2314(int val)
{
	int r;

	if (val > 28)
	{
		return 0;
	}

	r = PT2314::eq_table[val];

	return r;
}

// helper method
int PT2314::writeI2CChar(unsigned char c)
{
	Wire.beginTransmission(PT2314_I2C_ADDRESS);
	Wire.write(c);
	int rtnCode = Wire.endTransmission();
	return rtnCode;
}

// initialize PT2314
bool PT2314::init(void)
{
	_volume = 0;
	_attenuationL = 100;
	_attenuationR = 100;
	_mute = false;
	_loudness = false;
	_gain_index = 0;
	_channel = 0;
	_bass = 50;
	_treble = 50;

	return updateAll();
}

bool PT2314::volume(int v)
{
	_volume = constrain(v, 0, 100);
	return updateVolume();
}

bool PT2314::muteOn(void)
{
	_mute = true;
	updateAttenuation();
	return updateVolume();
}

bool PT2314::muteOff(void)
{
	_mute = false;
	updateAttenuation();
	return updateVolume();
}

bool PT2314::channel(int ch, bool one_based)
{
	_channel_is_one_based = one_based;
	if (_channel_is_one_based)
		ch = ch - 1; // internal channel number is zero based
	_channel = constrain(ch, 0, 3);
	return updateAudioSwitch();
}

bool PT2314::loudnessOn(void)
{
	_loudness = true;
	log_d("Loudness on");
	return updateAudioSwitch();
}

bool PT2314::loudnessOff(void)
{
	_loudness = false;
	log_d("Loudness off");
	return updateAudioSwitch();
}

bool PT2314::attenuation(int l, int r)
{
	_attenuationL = constrain(l, 0, 100);
	_attenuationR = constrain(r, 0, 100);
	return updateAttenuation();
}

bool PT2314::bass(int b)
{
	_bass = constrain(b, 0, 100);
	return updateBass();
}

bool PT2314::treble(int t)
{
	_treble = constrain(t, 0, 100);
	return updateTreble();
}

bool PT2314::updateVolume()
{
	_volume_pt2314 = volume_to_pt2314(_volume);
	return (writeI2CChar(_volume_pt2314) == 0) ? true : false;
}

bool PT2314::updateAttenuation()
{
	unsigned int aL = map(_attenuationL, 0, 100, 0b00011111, 0b00000000);
	unsigned int aR = map(_attenuationR, 0, 100, 0b00011111, 0b00000000);
	if (_mute)
	{
		if (writeI2CChar(0b11011111) != 0)
		{
			return false;
		}
		if (writeI2CChar(0b11111111) != 0)
		{
			return false;
		}
	}
	else
	{
		if (writeI2CChar(0b11000000 | aL) != 0)
		{
			return false;
		}
		if (writeI2CChar(0b11100000 | aR) != 0)
		{
			return false;
		}
	}

	return true;
}

bool PT2314::gain(int v)
{
	// v=0 means no gain, 1=+3.75dB, 2=+7.5dB, 3=+11.25dB
	_gain_index = constrain(v, 0, 3);
	log_d("Gain index: %d (%.2fdB)", _gain_index, getGain());
	return updateAudioSwitch();
}

bool PT2314::updateAudioSwitch()
{
	int audioByte = 0b01000000; // audio switch + gain +11.25dB.
	// gain byte, 0b00011000 = no gain, 0b00010000 = +3.75dB, 0b00001000 = +7.5dB, 0b00000000 = +11.25dB
	int8_t gain_array[] = {0b00011000, 0b00010000, 0b00001000, 0b00000000};
	audioByte |= gain_array[_gain_index];

	if (_loudness)
	{
		audioByte |= 0b00000000;
	}
	else
	{
		audioByte |= 0b00000100;
	}
	audioByte |= _channel;
	return (writeI2CChar(audioByte) == 0) ? true : false;
}

bool PT2314::updateBass()
{
	unsigned int val = eq_to_pt2314(map(_bass, 0, 100, 0, 28));
	return (writeI2CChar(0x60 | val) == 0) ? true : false;
}

bool PT2314::updateTreble()
{
	unsigned int val = eq_to_pt2314(map(_treble, 0, 100, 0, 28));
	return (writeI2CChar(0x70 | val) == 0) ? true : false;
}

bool PT2314::updateAll()
{
	if (updateVolume() == false)
	{
		return false;
	}

	if (updateAttenuation() == false)
	{
		return false;
	}

	if (updateAudioSwitch() == false)
	{
		return false;
	}

	if (updateBass() == false)
	{
		return false;
	}

	return updateTreble();
}

int PT2314::getChannel()
{
	if (_channel_is_one_based)
		return _channel + 1; // external channel number is one based
	else
		return _channel; // external channel number is one based
}

int PT2314::getVolume()
{
	return _volume;
}

float PT2314::getVolumedB()
{
	float aSteps[] = {0, -1.25, -2.5, -3.75, -5, -6.25, -7.5, -8.75}; // A values in 1.25 dB steps
	float bSteps[] = {0, -10, -20, -30, -40, -50, -60, -70};					// B values in 10 dB steps
	int A = _volume_pt2314 & 0b00000111;															// mask the 3 lsb of volume_pt2314
	int B = (_volume_pt2314 & 0b00111000) >> 3;												// mask the 3 msb of volume_pt2314 and shift 3 bits to the right
	float vol_dB = aSteps[A] + bSteps[B];
	log_d("Volume: %d, A: %d, B: %d, %.2fdB", _volume_pt2314, A, B, vol_dB);
	return vol_dB;
}

int PT2314::getBass()
{
	return _bass;
}

int PT2314::getTreble()
{
	return _treble;
}

int PT2314::getAttenuationL()
{
	return _attenuationL;
}

int PT2314::getAttenuationR()
{
	return _attenuationR;
}

// return the gain in dB
float PT2314::getGain()
{
	// gain byte, 0b00011000 = no gain, 0b00010000 = +3.75dB, 0b00001000 = +7.5dB, 0b00000000 = +11.25dB
	float gain_array_dB[] = {0, 3.75, 7.5, 11.25};
	return gain_array_dB[_gain_index];
}

bool PT2314::getLoudness()
{
	return _loudness;
}