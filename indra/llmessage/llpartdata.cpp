/** 
 * @file llpartdata.cpp
 * @brief Particle system data packing
 *
 * $LicenseInfo:firstyear=2003&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "llpartdata.h"
#include "message.h"

#include "lldatapacker.h"
#include "v4coloru.h"

#include "llsdutil.h"
#include "llsdutil_math.h"



const S32 PS_PART_DATA_GLOW_BLEND_SIZE = 4;
const S32 PS_PART_DATA_BLOCK_SIZE = 4 + 2 + 4 + 4 + 2 + 2; // 18
const S32 PS_DATA_BLOCK_SIZE = 68 + PS_PART_DATA_BLOCK_SIZE + PS_PART_DATA_GLOW_BLEND_SIZE; // 68 + 18 + 4 = 90


const F32 MAX_PART_SCALE = 4.f;

bool LLPartData::hasGlow() const
{
	return mStartGlow > 0.f || mEndGlow > 0.f;
}

BOOL LLPartData::pack(LLDataPacker &dp)
{
	LLColor4U coloru;
	dp.packU32(mFlags, "pdflags");
	dp.packFixed(mMaxAge, "pdmaxage", FALSE, 8, 8);
	coloru.setVec(mStartColor);
	dp.packColor4U(coloru, "pdstartcolor");
	coloru.setVec(mEndColor);
	dp.packColor4U(coloru, "pdendcolor");
	dp.packFixed(mStartScale.mV[0], "pdstartscalex", FALSE, 3, 5);
	dp.packFixed(mStartScale.mV[1], "pdstartscaley", FALSE, 3, 5);
	dp.packFixed(mEndScale.mV[0], "pdendscalex", FALSE, 3, 5);
	dp.packFixed(mEndScale.mV[1], "pdendscaley", FALSE, 3, 5);

	if (hasGlow() || hasBlendFunc())
	{
		dp.packU8(mStartGlow * 255,"pdstartglow");
		dp.packU8(mEndGlow * 255,"pdendglow");
		dp.packU8(mBlendFuncSource,"pdblendsource");
		dp.packU8(mBlendFuncDest,"pdblenddest");
	}
	return TRUE;
}

LLSD LLPartData::asLLSD() const
{
	LLSD sd = LLSD();
	sd["pdflags"] = ll_sd_from_U32(mFlags);
	sd["pdmaxage"] = mMaxAge;
	sd["pdstartcolor"] = ll_sd_from_color4(mStartColor);
	sd["pdendcolor"] = ll_sd_from_color4(mEndColor);
	sd["pdstartscale"] = ll_sd_from_vector2(mStartScale);
	sd["pdendscale"] = ll_sd_from_vector2(mEndScale);
	sd["pdstartglow"] =	mStartGlow;
	sd["pdendglow"] = mEndGlow;
	sd["pdblendsource"] = (S32)mBlendFuncSource;
	sd["pdblenddest"] = (S32)mBlendFuncDest;

	return sd;
}

bool LLPartData::fromLLSD(LLSD& sd)
{
	mFlags = ll_U32_from_sd(sd["pdflags"]);
	mMaxAge = (F32)sd["pdmaxage"].asReal();
	mStartColor = ll_color4_from_sd(sd["pdstartcolor"]);
	mEndColor = ll_color4_from_sd(sd["pdendcolor"]);
	mStartScale = ll_vector2_from_sd(sd["pdstartscale"]);
	mEndScale = ll_vector2_from_sd(sd["pdendscale"]);

	mStartGlow = sd.has("pdstartglow") ? sd["pdstartglow"].asReal() : 0.f;
	mEndGlow = sd.has("pdendglow") ? sd["pdendglow"].asReal() : 0.f;
	mBlendFuncSource = sd.has("pdblendsource") ? (U8)sd["pdblendsource"].asInteger() : LL_PART_BF_SOURCE_ALPHA;
	mBlendFuncDest = sd.has("pdblenddest") ? (U8)sd["pdblenddest"].asInteger() : LL_PART_BF_ONE_MINUS_SOURCE_ALPHA;

	return true;
}


BOOL LLPartData::unpack(LLDataPacker &dp)
{
	LLColor4U coloru;

	dp.unpackU32(mFlags, "pdflags");
	dp.unpackFixed(mMaxAge, "pdmaxage", FALSE, 8, 8);

	dp.unpackColor4U(coloru, "pdstartcolor");
	mStartColor.setVec(coloru);
	dp.unpackColor4U(coloru, "pdendcolor");
	mEndColor.setVec(coloru);
	dp.unpackFixed(mStartScale.mV[0], "pdstartscalex", FALSE, 3, 5);
	dp.unpackFixed(mStartScale.mV[1], "pdstartscaley", FALSE, 3, 5);
	dp.unpackFixed(mEndScale.mV[0], "pdendscalex", FALSE, 3, 5);
	dp.unpackFixed(mEndScale.mV[1], "pdendscaley", FALSE, 3, 5);

	if (dp.hasNext())
	{
		U8 tmp_glow = 0;
		dp.unpackU8(tmp_glow,"pdstartglow");
		mStartGlow = tmp_glow / 255.f;
		dp.unpackU8(tmp_glow,"pdendglow");
		mEndGlow = tmp_glow / 255.f;
		dp.unpackU8(mBlendFuncSource,"pdblendsource");
		dp.unpackU8(mBlendFuncDest,"pdblenddest");
	}

	return TRUE;
}


void LLPartData::setFlags(const U32 flags)
{
	mFlags = flags;
}


void LLPartData::setMaxAge(const F32 max_age)
{
	mMaxAge = llclamp(max_age, 0.f, 30.f);
}


void LLPartData::setStartScale(const F32 xs, const F32 ys)
{
	mStartScale.mV[VX] = llmin(xs, MAX_PART_SCALE);
	mStartScale.mV[VY] = llmin(ys, MAX_PART_SCALE);
}


void LLPartData::setEndScale(const F32 xs, const F32 ys)
{
	mEndScale.mV[VX] = llmin(xs, MAX_PART_SCALE);
	mEndScale.mV[VY] = llmin(ys, MAX_PART_SCALE);
}


void LLPartData::setStartColor(const LLVector3 &rgb)
{
	mStartColor.setVec(rgb.mV[0], rgb.mV[1], rgb.mV[2]);
}


void LLPartData::setEndColor(const LLVector3 &rgb)
{
	mEndColor.setVec(rgb.mV[0], rgb.mV[1], rgb.mV[2]);
}

void LLPartData::setStartAlpha(const F32 alpha)
{
	mStartColor.mV[3] = alpha;
}
void LLPartData::setEndAlpha(const F32 alpha)
{
	mEndColor.mV[3] = alpha;
}

// static
bool LLPartData::validBlendFunc(S32 func)
{
	if (func >= 0
		&& func < LL_PART_BF_COUNT
		&& func != UNSUPPORTED_DEST_ALPHA
		&& func != UNSUPPORTED_ONE_MINUS_DEST_ALPHA)
	{
		return true;
	}
	return false;
}

LLPartSysData::LLPartSysData()
{
	mCRC = 0;
	mFlags = 0;

	mPartData.mFlags = 0;
	mPartData.mStartColor = LLColor4(1.f, 1.f, 1.f, 1.f);
	mPartData.mEndColor = LLColor4(1.f, 1.f, 1.f, 1.f);
	mPartData.mStartScale = LLVector2(1.f, 1.f);
	mPartData.mEndScale = LLVector2(1.f, 1.f);
	mPartData.mMaxAge = 10.0;
	mPartData.mBlendFuncSource = LLPartData::LL_PART_BF_SOURCE_ALPHA;
	mPartData.mBlendFuncDest = LLPartData::LL_PART_BF_ONE_MINUS_SOURCE_ALPHA;
	mPartData.mStartGlow = 0.f;
	mPartData.mEndGlow = 0.f;

	mMaxAge = 0.0;
	mStartAge = 0.0;
	mPattern = LL_PART_SRC_PATTERN_DROP;			// Pattern for particle velocity
	mInnerAngle = 0.0;								// Inner angle of PATTERN_ANGLE_*
	mOuterAngle = 0.0;								// Outer angle of PATTERN_ANGLE_*
	mBurstRate = 0.1f;								// How often to do a burst of particles
	mBurstPartCount = 1;							// How many particles in a burst
	mBurstSpeedMin = 1.f;						// Minimum particle velocity
	mBurstSpeedMax = 1.f;						// Maximum particle velocity
	mBurstRadius = 0.f;

	mNumParticles = 0;
}


BOOL LLPartSysData::pack(LLDataPacker &dp)
{
	dp.packU32(mCRC, "pscrc");
	dp.packU32(mFlags, "psflags");
	dp.packU8(mPattern, "pspattern");
	dp.packFixed(mMaxAge, "psmaxage", FALSE, 8, 8);
	dp.packFixed(mStartAge, "psstartage", FALSE, 8, 8);
	dp.packFixed(mInnerAngle, "psinnerangle", FALSE, 3, 5);
	dp.packFixed(mOuterAngle, "psouterangle", FALSE, 3, 5);
	dp.packFixed(mBurstRate, "psburstrate", FALSE, 8, 8);
	dp.packFixed(mBurstRadius, "psburstradius", FALSE, 8, 8);
	dp.packFixed(mBurstSpeedMin, "psburstspeedmin", FALSE, 8, 8);
	dp.packFixed(mBurstSpeedMax, "psburstspeedmax", FALSE, 8, 8);
	dp.packU8(mBurstPartCount, "psburstpartcount");

	dp.packFixed(mAngularVelocity.mV[0], "psangvelx", TRUE, 8, 7);
	dp.packFixed(mAngularVelocity.mV[1], "psangvely", TRUE, 8, 7);
	dp.packFixed(mAngularVelocity.mV[2], "psangvelz", TRUE, 8, 7);

	dp.packFixed(mPartAccel.mV[0], "psaccelx", TRUE, 8, 7);
	dp.packFixed(mPartAccel.mV[1], "psaccely", TRUE, 8, 7);
	dp.packFixed(mPartAccel.mV[2], "psaccelz", TRUE, 8, 7);

	dp.packUUID(mPartImageID, "psuuid");
	dp.packUUID(mTargetUUID, "pstargetuuid");
	mPartData.pack(dp);
	return TRUE;
}


BOOL LLPartSysData::unpack(LLDataPacker &dp)
{
	dp.unpackU32(mCRC, "pscrc");
	dp.unpackU32(mFlags, "psflags");
	dp.unpackU8(mPattern, "pspattern");
	dp.unpackFixed(mMaxAge, "psmaxage", FALSE, 8, 8);
	dp.unpackFixed(mStartAge, "psstartage", FALSE, 8, 8);
	dp.unpackFixed(mInnerAngle, "psinnerangle", FALSE, 3, 5);
	dp.unpackFixed(mOuterAngle, "psouterangle", FALSE, 3, 5);
	dp.unpackFixed(mBurstRate, "psburstrate", FALSE, 8, 8);
	mBurstRate = llmax(0.01f, mBurstRate);
	dp.unpackFixed(mBurstRadius, "psburstradius", FALSE, 8, 8);
	dp.unpackFixed(mBurstSpeedMin, "psburstspeedmin", FALSE, 8, 8);
	dp.unpackFixed(mBurstSpeedMax, "psburstspeedmax", FALSE, 8, 8);
	dp.unpackU8(mBurstPartCount, "psburstpartcount");

	dp.unpackFixed(mAngularVelocity.mV[0], "psangvelx", TRUE, 8, 7);
	dp.unpackFixed(mAngularVelocity.mV[1], "psangvely", TRUE, 8, 7);
	dp.unpackFixed(mAngularVelocity.mV[2], "psangvelz", TRUE, 8, 7);

	dp.unpackFixed(mPartAccel.mV[0], "psaccelx", TRUE, 8, 7);
	dp.unpackFixed(mPartAccel.mV[1], "psaccely", TRUE, 8, 7);
	dp.unpackFixed(mPartAccel.mV[2], "psaccelz", TRUE, 8, 7);

	dp.unpackUUID(mPartImageID, "psuuid");
	dp.unpackUUID(mTargetUUID, "pstargetuuid");
	mPartData.unpack(dp);
	return TRUE;
}

std::ostream& operator<<(std::ostream& s, const LLPartSysData &data)
{
	s << "Flags: " << std::hex << data.mFlags;
	s << " Pattern: " << std::hex << (U32) data.mPattern << "\n";
	s << "Age: [" << data.mStartAge << ", " << data.mMaxAge << "]\n";
	s << "Angle: [" << data.mInnerAngle << ", " << data.mOuterAngle << "]\n";
	s << "Burst Rate: " << data.mBurstRate << "\n";
	s << "Burst Radius: " << data.mBurstRadius << "\n";
	s << "Burst Speed: [" << data.mBurstSpeedMin << ", " << data.mBurstSpeedMax << "]\n";
	s << "Burst Part Count: " << std::hex << (U32) data.mBurstPartCount << "\n";
	s << "Angular Velocity: " << data.mAngularVelocity << "\n";
	s << "Accel: " << data.mPartAccel;
	return s;
}

BOOL LLPartSysData::isNullPS(const S32 block_num)
{
	U8 ps_data_block[PS_DATA_BLOCK_SIZE];
	U32 crc;

	S32 size;
	// Check size of block
	size = gMessageSystem->getSize("ObjectData", block_num, "PSBlock");
	
	if (!size)
	{
		return TRUE;
	}
	
	gMessageSystem->getBinaryData("ObjectData", "PSBlock", ps_data_block, PS_DATA_BLOCK_SIZE, block_num, PS_DATA_BLOCK_SIZE);

	if (size > PS_DATA_BLOCK_SIZE)
	{
		//size is too big, newer particle version unsupported
		return TRUE;
	}

	LLDataPackerBinaryBuffer dp(ps_data_block, size);
	dp.unpackU32(crc, "crc");

	if (crc == 0)
	{
		return TRUE;
	}
	return FALSE;
}


//static
BOOL LLPartSysData::packNull()
{
	U8 ps_data_block[PS_DATA_BLOCK_SIZE];
	gMessageSystem->addBinaryData("PSBlock", ps_data_block, 0);
	return TRUE;
}


BOOL LLPartSysData::packBlock()
{
	U8 ps_data_block[PS_DATA_BLOCK_SIZE];

	LLDataPackerBinaryBuffer dp(ps_data_block, PS_DATA_BLOCK_SIZE);

	pack(dp);

	S32 size = dp.getCurrentSize();

	// Add to message
	gMessageSystem->addBinaryData("PSBlock", ps_data_block, size);

	return TRUE;
}                                         


BOOL LLPartSysData::unpackBlock(const S32 block_num)
{
	U8 ps_data_block[PS_DATA_BLOCK_SIZE];

	// Check size of block
	S32 size = gMessageSystem->getSize("ObjectData", block_num, "PSBlock");

	if (size > PS_DATA_BLOCK_SIZE)
	{
		// Larger packets are newer and unsupported
		return FALSE;
	}

	// Get from message
	gMessageSystem->getBinaryData("ObjectData", "PSBlock", ps_data_block, size, block_num, PS_DATA_BLOCK_SIZE);

	LLDataPackerBinaryBuffer dp(ps_data_block, size);
	unpack(dp);

	return TRUE;
}

void LLPartSysData::clampSourceParticleRate()
{
	F32 particle_rate = 0;
	particle_rate = mBurstPartCount/mBurstRate;
	if (particle_rate > 256.f)
	{
		mBurstPartCount = llfloor(((F32)mBurstPartCount)*(256.f/particle_rate));
	}
}

void LLPartSysData::setPartAccel(const LLVector3 &accel)
{
	mPartAccel.mV[VX] = llclamp(accel.mV[VX], -100.f, 100.f);
	mPartAccel.mV[VY] = llclamp(accel.mV[VY], -100.f, 100.f);
	mPartAccel.mV[VZ] = llclamp(accel.mV[VZ], -100.f, 100.f);
}
