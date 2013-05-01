/** 
 * @file llviewerregion.cpp
 * @brief Implementation of the LLViewerRegion class.
 *
 * $LicenseInfo:firstyear=2000&license=viewerlgpl$
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

#include "llviewerprecompiledheaders.h"

#include "llviewerregion.h"

// linden libraries
#include "indra_constants.h"
#include "llavatarnamecache.h"		// name lookup cap url
#include "llfloaterreg.h"
#include "llmath.h"
#include "llhttpclient.h"
#include "llregionflags.h"
#include "llregionhandle.h"
#include "llsurface.h"
#include "message.h"
//#include "vmath.h"
#include "v3math.h"
#include "v4math.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llcallingcard.h"
#include "llcaphttpsender.h"
#include "llcapabilitylistener.h"
#include "llcommandhandler.h"
#include "lldir.h"
#include "lleventpoll.h"
#include "llfloatergodtools.h"
#include "llfloaterreporter.h"
#include "llfloaterregioninfo.h"
#include "llhttpnode.h"
#include "llregioninfomodel.h"
#include "llsdutil.h"
#include "llstartup.h"
#include "lltrans.h"
#include "llurldispatcher.h"
#include "llviewerobjectlist.h"
#include "llviewerparceloverlay.h"
#include "llviewerstatsrecorder.h"
#include "llvlmanager.h"
#include "llvlcomposition.h"
#include "llvocache.h"
#include "llworld.h"
#include "llspatialpartition.h"
#include "stringize.h"
#include "llviewercontrol.h"
#include "llsdserialize.h"
#include "llvieweroctree.h"

#ifdef LL_WINDOWS
	#pragma warning(disable:4355)
#endif

const F32 WATER_TEXTURE_SCALE = 8.f;			//  Number of times to repeat the water texture across a region
const S16 MAX_MAP_DIST = 10;
// The server only keeps our pending agent info for 60 seconds.
// We want to allow for seed cap retry, but its not useful after that 60 seconds.
// Give it 3 chances, each at 18 seconds to give ourselves a few seconds to connect anyways if we give up.
const S32 MAX_SEED_CAP_ATTEMPTS_BEFORE_LOGIN = 3;
const F32 CAP_REQUEST_TIMEOUT = 18;
// Even though we gave up on login, keep trying for caps after we are logged in:
const S32 MAX_CAP_REQUEST_ATTEMPTS = 30;

LLViewerRegion* LLViewerRegion::sCurRegionp = NULL;
BOOL LLViewerRegion::sVOCacheCullingEnabled = FALSE;

typedef std::map<std::string, std::string> CapabilityMap;

class LLViewerRegionImpl {
public:
	LLViewerRegionImpl(LLViewerRegion * region, LLHost const & host)
		:	mHost(host),
			mCompositionp(NULL),
			mEventPoll(NULL),
			mSeedCapMaxAttempts(MAX_CAP_REQUEST_ATTEMPTS),
			mSeedCapMaxAttemptsBeforeLogin(MAX_SEED_CAP_ATTEMPTS_BEFORE_LOGIN),
			mSeedCapAttempts(0),
			mHttpResponderID(0),
			mLastCameraUpdate(0),
			mLastCameraOrigin(),
		    // I'd prefer to set the LLCapabilityListener name to match the region
		    // name -- it's disappointing that's not available at construction time.
		    // We could instead store an LLCapabilityListener*, making
		    // setRegionNameAndZone() replace the instance. Would that pose
		    // consistency problems? Can we even request a capability before calling
		    // setRegionNameAndZone()?
		    // For testability -- the new Michael Feathers paradigm --
		    // LLCapabilityListener binds all the globals it expects to need at
		    // construction time.
		    mCapabilityListener(host.getString(), gMessageSystem, *region,
		                        gAgent.getID(), gAgent.getSessionID())
	{
	}

	void buildCapabilityNames(LLSD& capabilityNames);

	// The surfaces and other layers
	LLSurface*	mLandp;

	// Region geometry data
	LLVector3d	mOriginGlobal;	// Location of southwest corner of region (meters)
	LLVector3d	mCenterGlobal;	// Location of center in world space (meters)
	LLHost		mHost;

	// The unique ID for this region.
	LLUUID mRegionID;

	// region/estate owner - usually null.
	LLUUID mOwnerID;

	// Network statistics for the region's circuit...
	LLTimer mLastNetUpdate;

	// Misc
	LLVLComposition *mCompositionp;		// Composition layer for the surface

	LLVOCacheEntry::vocache_entry_map_t	  mCacheMap; //all cached entries
	LLVOCacheEntry::vocache_entry_set_t   mActiveSet; //all active entries;
	LLVOCacheEntry::vocache_entry_set_t   mWaitingSet; //entries waiting for LLDrawable to be generated.	
	std::set< LLviewerOctreeGroup* >      mVisibleGroups; //visible groupa
	LLVOCachePartition*                   mVOCachePartition;
	LLVOCacheEntry::vocache_entry_set_t   mVisibleEntries; //must-be-created visible entries wait for objects creation.	
	LLVOCacheEntry::vocache_entry_priority_list_t mWaitingList; //transient list storing sorted visible entries waiting for object creation.

	// time?
	// LRU info?

	// Cache ID is unique per-region, across renames, moving locations,
	// etc.
	LLUUID mCacheID;

	CapabilityMap mCapabilities;
	
	LLEventPoll* mEventPoll;

	S32 mSeedCapMaxAttempts;
	S32 mSeedCapMaxAttemptsBeforeLogin;
	S32 mSeedCapAttempts;

	S32 mHttpResponderID;

	/// Post an event to this LLCapabilityListener to invoke a capability message on
	/// this LLViewerRegion's server
	/// (https://wiki.lindenlab.com/wiki/Viewer:Messaging/Messaging_Notes#Capabilities)
	LLCapabilityListener mCapabilityListener;

	//spatial partitions for objects in this region
	std::vector<LLViewerOctreePartition*> mObjectPartition;

	LLVector3 mLastCameraOrigin;
	U32       mLastCameraUpdate;
};

// support for secondlife:///app/region/{REGION} SLapps
// N.B. this is defined to work exactly like the classic secondlife://{REGION}
// However, the later syntax cannot support spaces in the region name because
// spaces (and %20 chars) are illegal in the hostname of an http URL. Some
// browsers let you get away with this, but some do not (such as Qt's Webkit).
// Hence we introduced the newer secondlife:///app/region alternative.
class LLRegionHandler : public LLCommandHandler
{
public:
	// requests will be throttled from a non-trusted browser
	LLRegionHandler() : LLCommandHandler("region", UNTRUSTED_THROTTLE) {}

	bool handle(const LLSD& params, const LLSD& query_map, LLMediaCtrl* web)
	{
		// make sure that we at least have a region name
		int num_params = params.size();
		if (num_params < 1)
		{
			return false;
		}

		// build a secondlife://{PLACE} SLurl from this SLapp
		std::string url = "secondlife://";
		for (int i = 0; i < num_params; i++)
		{
			if (i > 0)
			{
				url += "/";
			}
			url += params[i].asString();
		}

		// Process the SLapp as if it was a secondlife://{PLACE} SLurl
		LLURLDispatcher::dispatch(url, "clicked", web, true);
		return true;
	}
};
LLRegionHandler gRegionHandler;

class BaseCapabilitiesComplete : public LLHTTPClient::Responder
{
	LOG_CLASS(BaseCapabilitiesComplete);
public:
    BaseCapabilitiesComplete(U64 region_handle, S32 id)
		: mRegionHandle(region_handle), mID(id)
    { }
	virtual ~BaseCapabilitiesComplete()
	{ }

    void error(U32 statusNum, const std::string& reason)
    {
		LL_WARNS2("AppInit", "Capabilities") << statusNum << ": " << reason << LL_ENDL;
		LLViewerRegion *regionp = LLWorld::getInstance()->getRegionFromHandle(mRegionHandle);
		if (regionp)
		{
			regionp->failedSeedCapability();
		}
    }

    void result(const LLSD& content)
    {
		LLViewerRegion *regionp = LLWorld::getInstance()->getRegionFromHandle(mRegionHandle);
		if(!regionp) //region was removed
		{
			LL_WARNS2("AppInit", "Capabilities") << "Received results for region that no longer exists!" << LL_ENDL;
			return ;
		}
		if( mID != regionp->getHttpResponderID() ) // region is no longer referring to this responder
		{
			LL_WARNS2("AppInit", "Capabilities") << "Received results for a stale http responder!" << LL_ENDL;
			return ;
		}

		LLSD::map_const_iterator iter;
		for(iter = content.beginMap(); iter != content.endMap(); ++iter)
		{
			regionp->setCapability(iter->first, iter->second);
			LL_DEBUGS2("AppInit", "Capabilities") << "got capability for " 
				<< iter->first << LL_ENDL;

			/* HACK we're waiting for the ServerReleaseNotes */
			if (iter->first == "ServerReleaseNotes" && regionp->getReleaseNotesRequested())
			{
				regionp->showReleaseNotes();
			}
		}

		regionp->setCapabilitiesReceived(true);

		if (STATE_SEED_GRANTED_WAIT == LLStartUp::getStartupState())
		{
			LLStartUp::setStartupState( STATE_SEED_CAP_GRANTED );
		}
	}

    static BaseCapabilitiesComplete* build( U64 region_handle, S32 id )
    {
		return new BaseCapabilitiesComplete(region_handle, id);
    }

private:
	U64 mRegionHandle;
	S32 mID;
};


LLViewerRegion::LLViewerRegion(const U64 &handle,
							   const LLHost &host,
							   const U32 grids_per_region_edge, 
							   const U32 grids_per_patch_edge, 
							   const F32 region_width_meters)
:	mImpl(new LLViewerRegionImpl(this, host)),
	mHandle(handle),
	mTimeDilation(1.0f),
	mName(""),
	mZoning(""),
	mIsEstateManager(FALSE),
	mRegionFlags( REGION_FLAGS_DEFAULT ),
	mSimAccess( SIM_ACCESS_MIN ),
	mBillableFactor(1.0),
	mMaxTasks(DEFAULT_MAX_REGION_WIDE_PRIM_COUNT),
	mClassID(0),
	mCPURatio(0),
	mColoName("unknown"),
	mProductSKU("unknown"),
	mProductName("unknown"),
	mHttpUrl(""),
	mCacheLoaded(FALSE),
	mCacheDirty(FALSE),
	mReleaseNotesRequested(FALSE),
	mCapabilitiesReceived(false),
	mBitsReceived(0.f),
	mPacketsReceived(0.f),
	mDead(FALSE)
{
	mWidth = region_width_meters;
	mImpl->mOriginGlobal = from_region_handle(handle); 
	updateRenderMatrix();

	mImpl->mLandp = new LLSurface('l', NULL);

	// Create the composition layer for the surface
	mImpl->mCompositionp =
		new LLVLComposition(mImpl->mLandp,
							grids_per_region_edge,
							region_width_meters / grids_per_region_edge);
	mImpl->mCompositionp->setSurface(mImpl->mLandp);

	// Create the surfaces
	mImpl->mLandp->setRegion(this);
	mImpl->mLandp->create(grids_per_region_edge,
					grids_per_patch_edge,
					mImpl->mOriginGlobal,
					mWidth);

	mParcelOverlay = new LLViewerParcelOverlay(this, region_width_meters);

	setOriginGlobal(from_region_handle(handle));
	calculateCenterGlobal();

	// Create the object lists
	initStats();

	//create object partitions
	//MUST MATCH declaration of eObjectPartitions
	mImpl->mObjectPartition.push_back(new LLHUDPartition(this));		//PARTITION_HUD
	mImpl->mObjectPartition.push_back(new LLTerrainPartition(this));	//PARTITION_TERRAIN
	mImpl->mObjectPartition.push_back(new LLVoidWaterPartition(this));	//PARTITION_VOIDWATER
	mImpl->mObjectPartition.push_back(new LLWaterPartition(this));		//PARTITION_WATER
	mImpl->mObjectPartition.push_back(new LLTreePartition(this));		//PARTITION_TREE
	mImpl->mObjectPartition.push_back(new LLParticlePartition(this));	//PARTITION_PARTICLE
	mImpl->mObjectPartition.push_back(new LLGrassPartition(this));		//PARTITION_GRASS
	mImpl->mObjectPartition.push_back(new LLVolumePartition(this));	//PARTITION_VOLUME
	mImpl->mObjectPartition.push_back(new LLBridgePartition(this));	//PARTITION_BRIDGE
	mImpl->mObjectPartition.push_back(new LLHUDParticlePartition(this));//PARTITION_HUD_PARTICLE
	mImpl->mObjectPartition.push_back(new LLVOCachePartition(this)); //PARTITION_VO_CACHE
	mImpl->mObjectPartition.push_back(NULL);						//PARTITION_NONE

	mImpl->mVOCachePartition = getVOCachePartition();
}


void LLViewerRegion::initStats()
{
	mImpl->mLastNetUpdate.reset();
	mPacketsIn = 0;
	mBitsIn = 0;
	mLastBitsIn = 0;
	mLastPacketsIn = 0;
	mPacketsOut = 0;
	mLastPacketsOut = 0;
	mPacketsLost = 0;
	mLastPacketsLost = 0;
	mPingDelay = 0;
	mAlive = false;					// can become false if circuit disconnects
}

LLViewerRegion::~LLViewerRegion() 
{
	mDead = TRUE;
	mImpl->mActiveSet.clear();
	mImpl->mVisibleEntries.clear();
	mImpl->mVisibleGroups.clear();
	mImpl->mWaitingSet.clear();

	gVLManager.cleanupData(this);
	// Can't do this on destruction, because the neighbor pointers might be invalid.
	// This should be reference counted...
	disconnectAllNeighbors();
	LLViewerPartSim::getInstance()->cleanupRegion(this);

	gObjectList.killObjects(this);

	delete mImpl->mCompositionp;
	delete mParcelOverlay;
	delete mImpl->mLandp;
	delete mImpl->mEventPoll;
	LLHTTPSender::clearSender(mImpl->mHost);
	
	std::for_each(mImpl->mObjectPartition.begin(), mImpl->mObjectPartition.end(), DeletePointer());

	saveObjectCache();

	delete mImpl;
	mImpl = NULL;
}

LLEventPump& LLViewerRegion::getCapAPI() const
{
	return mImpl->mCapabilityListener.getCapAPI();
}

/*virtual*/ 
const LLHost&	LLViewerRegion::getHost() const				
{ 
	return mImpl->mHost; 
}

LLSurface & LLViewerRegion::getLand() const
{
	return *mImpl->mLandp;
}

const LLUUID& LLViewerRegion::getRegionID() const
{
	return mImpl->mRegionID;
}

void LLViewerRegion::setRegionID(const LLUUID& region_id)
{
	mImpl->mRegionID = region_id;
}

void LLViewerRegion::loadObjectCache()
{
	if (mCacheLoaded)
	{
		return;
	}

	// Presume success.  If it fails, we don't want to try again.
	mCacheLoaded = TRUE;

	if(LLVOCache::hasInstance())
	{
		LLVOCache::getInstance()->readFromCache(mHandle, mImpl->mCacheID, mImpl->mCacheMap) ;
		if (mImpl->mCacheMap.empty())
		{
			mCacheDirty = TRUE;
		}
	}
}


void LLViewerRegion::saveObjectCache()
{
	if (!mCacheLoaded)
	{
		return;
	}

	if (mImpl->mCacheMap.empty())
	{
		return;
	}

	if(LLVOCache::hasInstance())
	{
		const F32 start_time_threshold = 600.0f; //seconds
		bool removal_enabled = sVOCacheCullingEnabled && (mRegionTimer.getElapsedTimeF32() > start_time_threshold); //allow to remove invalid objects from object cache file.
		
		LLVOCache::getInstance()->writeToCache(mHandle, mImpl->mCacheID, mImpl->mCacheMap, mCacheDirty, removal_enabled) ;
		mCacheDirty = FALSE;
	}

	mImpl->mCacheMap.clear();
}

void LLViewerRegion::sendMessage()
{
	gMessageSystem->sendMessage(mImpl->mHost);
}

void LLViewerRegion::sendReliableMessage()
{
	gMessageSystem->sendReliable(mImpl->mHost);
}

void LLViewerRegion::setFlags(BOOL b, U32 flags)
{
	if (b)
	{
		mRegionFlags |=  flags;
	}
	else
	{
		mRegionFlags &= ~flags;
	}
}

void LLViewerRegion::setWaterHeight(F32 water_level)
{
	mImpl->mLandp->setWaterHeight(water_level);
}

F32 LLViewerRegion::getWaterHeight() const
{
	return mImpl->mLandp->getWaterHeight();
}

BOOL LLViewerRegion::isVoiceEnabled() const
{
	return (getRegionFlags() & REGION_FLAGS_ALLOW_VOICE);
}

void LLViewerRegion::setRegionFlags(U32 flags)
{
	mRegionFlags = flags;
}


void LLViewerRegion::setOriginGlobal(const LLVector3d &origin_global) 
{ 
	mImpl->mOriginGlobal = origin_global; 
	updateRenderMatrix();
	mImpl->mLandp->setOriginGlobal(origin_global);
	mWind.setOriginGlobal(origin_global);
	calculateCenterGlobal();
}

void LLViewerRegion::updateRenderMatrix()
{
	mRenderMatrix.setTranslation(getOriginAgent());
}

void LLViewerRegion::setTimeDilation(F32 time_dilation)
{
	mTimeDilation = time_dilation;
}

const LLVector3d & LLViewerRegion::getOriginGlobal() const
{
	return mImpl->mOriginGlobal;
}

LLVector3 LLViewerRegion::getOriginAgent() const
{
	return gAgent.getPosAgentFromGlobal(mImpl->mOriginGlobal);
}

const LLVector3d & LLViewerRegion::getCenterGlobal() const
{
	return mImpl->mCenterGlobal;
}

LLVector3 LLViewerRegion::getCenterAgent() const
{
	return gAgent.getPosAgentFromGlobal(mImpl->mCenterGlobal);
}

void LLViewerRegion::setOwner(const LLUUID& owner_id)
{
	mImpl->mOwnerID = owner_id;
}

const LLUUID& LLViewerRegion::getOwner() const
{
	return mImpl->mOwnerID;
}

void LLViewerRegion::setRegionNameAndZone	(const std::string& name_zone)
{
	std::string::size_type pipe_pos = name_zone.find('|');
	S32 length   = name_zone.size();
	if (pipe_pos != std::string::npos)
	{
		mName   = name_zone.substr(0, pipe_pos);
		mZoning = name_zone.substr(pipe_pos+1, length-(pipe_pos+1));
	}
	else
	{
		mName   = name_zone;
		mZoning = "";
	}

	LLStringUtil::stripNonprintable(mName);
	LLStringUtil::stripNonprintable(mZoning);
}

BOOL LLViewerRegion::canManageEstate() const
{
	return gAgent.isGodlike()
		|| isEstateManager()
		|| gAgent.getID() == getOwner();
}

const std::string LLViewerRegion::getSimAccessString() const
{
	return accessToString(mSimAccess);
}

std::string LLViewerRegion::getLocalizedSimProductName() const
{
	std::string localized_spn;
	return LLTrans::findString(localized_spn, mProductName) ? localized_spn : mProductName;
}

// static
std::string LLViewerRegion::regionFlagsToString(U32 flags)
{
	std::string result;

	if (flags & REGION_FLAGS_SANDBOX)
	{
		result += "Sandbox";
	}

	if (flags & REGION_FLAGS_ALLOW_DAMAGE)
	{
		result += " Not Safe";
	}

	return result;
}

// static
std::string LLViewerRegion::accessToString(U8 sim_access)
{
	switch(sim_access)
	{
	case SIM_ACCESS_PG:
		return LLTrans::getString("SIM_ACCESS_PG");

	case SIM_ACCESS_MATURE:
		return LLTrans::getString("SIM_ACCESS_MATURE");

	case SIM_ACCESS_ADULT:
		return LLTrans::getString("SIM_ACCESS_ADULT");

	case SIM_ACCESS_DOWN:
		return LLTrans::getString("SIM_ACCESS_DOWN");

	case SIM_ACCESS_MIN:
	default:
		return LLTrans::getString("SIM_ACCESS_MIN");
	}
}

// static
std::string LLViewerRegion::getAccessIcon(U8 sim_access)
{
	switch(sim_access)
	{
	case SIM_ACCESS_MATURE:
		return "Parcel_M_Dark";

	case SIM_ACCESS_ADULT:
		return "Parcel_R_Light";

	case SIM_ACCESS_PG:
		return "Parcel_PG_Light";

	case SIM_ACCESS_MIN:
	default:
		return "";
	}
}

// static
std::string LLViewerRegion::accessToShortString(U8 sim_access)
{
	switch(sim_access)		/* Flawfinder: ignore */
	{
	case SIM_ACCESS_PG:
		return "PG";

	case SIM_ACCESS_MATURE:
		return "M";

	case SIM_ACCESS_ADULT:
		return "A";

	case SIM_ACCESS_MIN:
	default:
		return "U";
	}
}

// static
U8 LLViewerRegion::shortStringToAccess(const std::string &sim_access)
{
	U8 accessValue;

	if (LLStringUtil::compareStrings(sim_access, "PG") == 0)
	{
		accessValue = SIM_ACCESS_PG;
	}
	else if (LLStringUtil::compareStrings(sim_access, "M") == 0)
	{
		accessValue = SIM_ACCESS_MATURE;
	}
	else if (LLStringUtil::compareStrings(sim_access, "A") == 0)
	{
		accessValue = SIM_ACCESS_ADULT;
	}
	else
	{
		accessValue = SIM_ACCESS_MIN;
	}

	return accessValue;
}

// static
void LLViewerRegion::processRegionInfo(LLMessageSystem* msg, void**)
{
	// send it to 'observers'
	// *TODO: switch the floaters to using LLRegionInfoModel
	llinfos << "Processing region info" << llendl;
	LLRegionInfoModel::instance().update(msg);
	LLFloaterGodTools::processRegionInfo(msg);
	LLFloaterRegionInfo::processRegionInfo(msg);
	LLFloaterReporter::processRegionInfo(msg);
}

void LLViewerRegion::setCacheID(const LLUUID& id)
{
	mImpl->mCacheID = id;
}

S32 LLViewerRegion::renderPropertyLines()
{
	if (mParcelOverlay)
	{
		return mParcelOverlay->renderPropertyLines();
	}
	else
	{
		return 0;
	}
}

// This gets called when the height field changes.
void LLViewerRegion::dirtyHeights()
{
	// Property lines need to be reconstructed when the land changes.
	if (mParcelOverlay)
	{
		mParcelOverlay->setDirty();
	}
}

void LLViewerRegion::replaceCacheEntry(LLVOCacheEntry* old_entry, LLVOCacheEntry* new_entry)
{
	U32 state = LLVOCacheEntry::INACTIVE;
	bool in_vo_tree = false;

	if(old_entry)
	{
		old_entry->copyTo(new_entry);
		state = old_entry->getState();
		in_vo_tree = (state == LLVOCacheEntry::INACTIVE && old_entry->getGroup() != NULL);
		killCacheEntry(old_entry);
	}

	mImpl->mCacheMap[new_entry->getLocalID()] = new_entry;

	if(state == LLVOCacheEntry::ACTIVE)
	{
		llassert(new_entry->getEntry()->hasDrawable());
		mImpl->mActiveSet.insert(new_entry);
	}
	else if(state == LLVOCacheEntry::WAITING)
	{
		mImpl->mWaitingSet.insert(new_entry);
	}
	else if(!old_entry || in_vo_tree)
	{
		addToVOCacheTree(new_entry);
	}
	new_entry->setState(state);
}

//physically delete the cache entry
void LLViewerRegion::killCacheEntry(LLVOCacheEntry* entry)
{	
	if(!entry)
	{
		return;
	}

	//remove from active list and waiting list
	if(entry->isState(LLVOCacheEntry::ACTIVE))
	{
		mImpl->mActiveSet.erase(entry);
	}
	else
	{
		if(entry->isState(LLVOCacheEntry::WAITING))
		{
			mImpl->mWaitingSet.erase(entry);
		}
		
		//remove from mVOCachePartition
		removeFromVOCacheTree(entry);
	}

	//remove from the forced visible list
	mImpl->mVisibleEntries.erase(entry);

	//kill LLViewerObject if exists
	//this should be done by the rendering pipeline automatically.
	
	entry->setState(LLVOCacheEntry::INACTIVE);
	
	//remove from mCacheMap, real deletion
	mImpl->mCacheMap.erase(entry->getLocalID());
}

//physically delete the cache entry	
void LLViewerRegion::killCacheEntry(U32 local_id) 
{
	killCacheEntry(getCacheEntry(local_id));
}

U32 LLViewerRegion::getNumOfActiveCachedObjects() const
{
	return  mImpl->mActiveSet.size();
}

void LLViewerRegion::addActiveCacheEntry(LLVOCacheEntry* entry)
{
	if(!entry || mDead)
	{
		return;
	}

	if(entry->isState(LLVOCacheEntry::WAITING))
	{
		mImpl->mWaitingSet.erase(entry);
	}

	entry->setState(LLVOCacheEntry::ACTIVE);
	entry->setVisible();

	llassert(entry->getEntry()->hasDrawable());
	mImpl->mActiveSet.insert(entry);
}

void LLViewerRegion::removeActiveCacheEntry(LLVOCacheEntry* entry, LLDrawable* drawablep)
{
	if(mDead)
	{
		return;
	}

	if(drawablep->getParent()) //child object
	{
		LLViewerOctreeEntry* parent_oct_entry = drawablep->getParent()->getEntry();
		if(parent_oct_entry && parent_oct_entry->hasVOCacheEntry())
		{
			LLVOCacheEntry* parent = (LLVOCacheEntry*)parent_oct_entry->getVOCacheEntry();
			parent->addChild(entry);
		}
	}
	else //insert to vo cache tree.
	{
		//shift to the local regional space from agent space
		const LLVector3 pos = drawablep->getVObj()->getPositionRegion();
		LLVector4a vec(pos[0], pos[1], pos[2]);
		LLVector4a shift; 
		shift.setSub(vec, entry->getPositionGroup());
		entry->shift(shift);
		
		addToVOCacheTree(entry);
	}

	mImpl->mVisibleEntries.erase(entry);
	mImpl->mActiveSet.erase(entry);
	mImpl->mWaitingSet.erase(entry);
	entry->setState(LLVOCacheEntry::INACTIVE);
}

void LLViewerRegion::addVisibleGroup(LLviewerOctreeGroup* group)
{
	if(mDead || group->isEmpty())
	{
		return;
	}
	group->setVisible();
	mImpl->mVisibleGroups.insert(group);
}

void LLViewerRegion::addToVOCacheTree(LLVOCacheEntry* entry)
{
	if(!sVOCacheCullingEnabled)
	{
		return;
	}

	if(mDead || !entry || !entry->getEntry())
	{
		return;
	}
	if(entry->getGroup()) //already in octree.
	{
		return;
	}

	mImpl->mVOCachePartition->addEntry(entry->getEntry());
}

void LLViewerRegion::removeFromVOCacheTree(LLVOCacheEntry* entry)
{
	if(mDead || !entry || !entry->getEntry())
	{
		return;
	}
	if(!entry->getGroup())
	{
		return;
	}

	mImpl->mVOCachePartition->removeEntry(entry->getEntry());
}

//add the visible entries
void LLViewerRegion::addVisibleCacheEntry(LLVOCacheEntry* entry)
{
	if(mDead || !entry)
	{
		return; 
	}

	if(entry->isState(LLVOCacheEntry::IN_QUEUE))
	{
		return;
	}

	if(entry->isState(LLVOCacheEntry::INACTIVE))
	{
		entry->setState(LLVOCacheEntry::IN_QUEUE);
	}
	mImpl->mVisibleEntries.insert(entry);
}

void LLViewerRegion::clearVisibleGroup(LLviewerOctreeGroup* group)
{
	if(mDead)
	{
		return;
	}

	llassert(!group->getOctreeNode() || group->isEmpty());

	mImpl->mVisibleGroups.erase(group);
}

F32 LLViewerRegion::updateVisibleEntries(F32 max_time)
{
	if(mImpl->mVisibleGroups.empty() && mImpl->mVisibleEntries.empty())
	{
		return max_time;
	}

	LLTimer update_timer;

	const LLVector3 camera_origin = LLViewerCamera::getInstance()->getOrigin();
	const U32 cur_frame = LLViewerOctreeEntryData::getCurrentFrame();
	bool needs_update = ((cur_frame - mImpl->mLastCameraUpdate) > 5) && ((camera_origin - mImpl->mLastCameraOrigin).lengthSquared() > 10.f);	

	//process visible entries
	max_time *= 0.5f; //only use up to half available time to update entries.

#if 1
	for(LLVOCacheEntry::vocache_entry_set_t::iterator iter = mImpl->mVisibleEntries.begin(); iter != mImpl->mVisibleEntries.end();)
	{
		LLVOCacheEntry* vo_entry = *iter;
		vo_entry->calcSceneContribution(camera_origin, needs_update, mImpl->mLastCameraUpdate);

		if(vo_entry->getState() < LLVOCacheEntry::WAITING)
		{			
			mImpl->mWaitingList.insert(vo_entry);
		}

		LLVOCacheEntry* child;
		S32 num_child = vo_entry->getNumOfChildren();
		S32 num_done = 0;
		for(S32 i = 0; i < num_child; i++)
		{
			child = vo_entry->getChild(i);
			if(child->getState() < LLVOCacheEntry::WAITING)
			{
				child->setSceneContribution(vo_entry->getSceneContribution());
				mImpl->mWaitingList.insert(child);
			}
			else
			{
				num_done++;
			}
		}
		if(num_done == num_child)
		{
			vo_entry->clearChildrenList();
		}

		if(!vo_entry->getNumOfChildren())
		{
			if(vo_entry->getState() >= LLVOCacheEntry::WAITING)
			{
				LLVOCacheEntry::vocache_entry_set_t::iterator next_iter = iter;
				++next_iter;
				mImpl->mVisibleEntries.erase(iter);
				iter = next_iter;
			}
			else
			{
				++iter;
			}
		}
		else
		{
			++iter;
		}
	}
#endif

	//process visible groups
	std::set< LLviewerOctreeGroup* >::iterator group_iter = mImpl->mVisibleGroups.begin();
	for(; group_iter != mImpl->mVisibleGroups.end(); ++group_iter)
	{
		LLviewerOctreeGroup* group = *group_iter;
		if(!group->getOctreeNode() || group->isEmpty())
		{
			continue;
		}

		for (LLviewerOctreeGroup::element_iter i = group->getDataBegin(); i != group->getDataEnd(); ++i)
		{
			if((*i)->hasVOCacheEntry())
			{
				LLVOCacheEntry* vo_entry = (LLVOCacheEntry*)(*i)->getVOCacheEntry();

				if(vo_entry->getParentID() > 0) //is a child
				{
					//child visibility depends on its parent.
					continue;
				}

				vo_entry->calcSceneContribution(camera_origin, needs_update, mImpl->mLastCameraUpdate);				
				mImpl->mWaitingList.insert(vo_entry);
			}
		}
	}
	mImpl->mVisibleGroups.clear();

	if(needs_update)
	{
		mImpl->mLastCameraOrigin = camera_origin;
		mImpl->mLastCameraUpdate = cur_frame;
	}

	return 2.0f * max_time - update_timer.getElapsedTimeF32();
}

F32 LLViewerRegion::createVisibleObjects(F32 max_time)
{
	if(mImpl->mWaitingList.empty())
	{
		return max_time;
	}

	LLTimer update_timer;
	S32 max_num_objects = 64; //minimum number of new objects to be added
	for(LLVOCacheEntry::vocache_entry_priority_list_t::iterator iter = mImpl->mWaitingList.begin();
		iter != mImpl->mWaitingList.end(); ++iter)
	{
		LLVOCacheEntry* vo_entry = *iter;
			
		if(vo_entry->getState() < LLVOCacheEntry::WAITING)
		{
			addNewObject(vo_entry);
			if(max_num_objects-- < 0 && update_timer.getElapsedTimeF32() > max_time)
			{
				break;
			}
		}
	}
	mImpl->mWaitingList.clear();

	return max_time - update_timer.getElapsedTimeF32();
}

BOOL LLViewerRegion::idleUpdate(F32 max_update_time)
{
	LLTimer update_timer;

	// did_update returns TRUE if we did at least one significant update
	BOOL did_update = mImpl->mLandp->idleUpdate(max_update_time);
	
	if (mParcelOverlay)
	{
		// Hopefully not a significant time sink...
		mParcelOverlay->idleUpdate();
	}

	max_update_time -= update_timer.getElapsedTimeF32();
	if(max_update_time < 0.f || mImpl->mCacheMap.empty())
	{
		return did_update;
	}

	if(!sVOCacheCullingEnabled)
	{
		return did_update;
	}

	sCurRegionp = this;

	//kill invisible objects
	max_update_time = killInvisibleObjects(max_update_time);	
	
	max_update_time = updateVisibleEntries(max_update_time);
	createVisibleObjects(max_update_time);

	mImpl->mVisibleGroups.clear();

	sCurRegionp = NULL;
	return did_update;
}

F32 LLViewerRegion::killInvisibleObjects(F32 max_time)
{
#if 1
	if(!sVOCacheCullingEnabled)
	{
		return max_time;
	}
	if(mImpl->mActiveSet.empty())
	{
		return max_time;
	}

	static LLVOCacheEntry* last_visited_entry = NULL;

	const size_t MAX_UPDATE = 32; 
	std::vector<LLDrawable*> delete_list;
	S32 update_counter = llmin(MAX_UPDATE, mImpl->mActiveSet.size());
	LLVOCacheEntry::vocache_entry_set_t::iterator iter = mImpl->mActiveSet.upper_bound(last_visited_entry);	
	
	for(; update_counter > 0; --update_counter, ++iter)
	{	
		if(iter == mImpl->mActiveSet.end())
		{
			iter = mImpl->mActiveSet.begin();
		}

		if(!(*iter)->isRecentlyVisible())
		{
			killObject((*iter), delete_list);
		}
	}

	if(iter == mImpl->mActiveSet.end())
	{
		last_visited_entry = NULL;
	}
	else
	{
		last_visited_entry = *iter;
	}

	for(S32 i = 0; i < delete_list.size(); i++)
	{
		gObjectList.killObject(delete_list[i]->getVObj());
	}
	delete_list.clear();
#endif
	return max_time;
}

void LLViewerRegion::killObject(LLVOCacheEntry* entry, std::vector<LLDrawable*>& delete_list)
{
	//kill the object.
	LLDrawable* drawablep = (LLDrawable*)entry->getEntry()->getDrawable();
	llassert(drawablep);

	if(!drawablep->getParent())
	{
		LLViewerObject::const_child_list_t& child_list = drawablep->getVObj()->getChildren();
		for (LLViewerObject::child_list_t::const_iterator iter = child_list.begin();
			iter != child_list.end(); iter++)
		{
			LLViewerObject* child = *iter;
			if(child->mDrawable->isRecentlyVisible())
			{
				//set the parent group visible if any of its children visible.
				((LLViewerOctreeEntryData*)drawablep)->setVisible();
				return;
			}
		}
		delete_list.push_back(drawablep);				
	}				
}

LLViewerObject* LLViewerRegion::addNewObject(LLVOCacheEntry* entry)
{
	LLViewerObject* obj = NULL;
	if(!entry->getEntry()->hasDrawable()) //not added to the rendering pipeline yet
	{
		//add the object
		obj = gObjectList.processObjectUpdateFromCache(entry, this);
		if(obj)
		{
			if(!entry->isState(LLVOCacheEntry::ACTIVE))
			{
				mImpl->mWaitingSet.insert(entry);
				entry->setState(LLVOCacheEntry::WAITING);
			}
		}
	}
	else
	{
		llerrs << "Object is already created." << llendl;
	}
	return obj;
}

//remove from object cache if the object receives a full-update or terse update
LLViewerObject* LLViewerRegion::forceToRemoveFromCache(U32 local_id, LLViewerObject* objectp)
{
	LLVOCacheEntry* entry = getCacheEntry(local_id);
	if (!entry)
	{
		return objectp; //not in the cache, do nothing.
	}
	if(!objectp) //object not created
	{
		entry->setTouched(FALSE); //mark this entry invalid

		//create a new object before delete it from cache.
		objectp = gObjectList.processObjectUpdateFromCache(entry, this);
	}

	//remove from cache.
	killCacheEntry(entry);

	return objectp;
}

// As above, but forcibly do the update.
void LLViewerRegion::forceUpdate()
{
	mImpl->mLandp->idleUpdate(0.f);

	if (mParcelOverlay)
	{
		mParcelOverlay->idleUpdate(true);
	}
}

void LLViewerRegion::connectNeighbor(LLViewerRegion *neighborp, U32 direction)
{
	mImpl->mLandp->connectNeighbor(neighborp->mImpl->mLandp, direction);
}


void LLViewerRegion::disconnectAllNeighbors()
{
	mImpl->mLandp->disconnectAllNeighbors();
}

LLVLComposition * LLViewerRegion::getComposition() const
{
	return mImpl->mCompositionp;
}

F32 LLViewerRegion::getCompositionXY(const S32 x, const S32 y) const
{
	if (x >= 256)
	{
		if (y >= 256)
		{
			LLVector3d center = getCenterGlobal() + LLVector3d(256.f, 256.f, 0.f);
			LLViewerRegion *regionp = LLWorld::getInstance()->getRegionFromPosGlobal(center);
			if (regionp)
			{
				// OK, we need to do some hackery here - different simulators no longer use
				// the same composition values, necessarily.
				// If we're attempting to blend, then we want to make the fractional part of
				// this region match the fractional of the adjacent.  For now, just minimize
				// the delta.
				F32 our_comp = getComposition()->getValueScaled(255, 255);
				F32 adj_comp = regionp->getComposition()->getValueScaled(x - 256.f, y - 256.f);
				while (llabs(our_comp - adj_comp) >= 1.f)
				{
					if (our_comp > adj_comp)
					{
						adj_comp += 1.f;
					}
					else
					{
						adj_comp -= 1.f;
					}
				}
				return adj_comp;
			}
		}
		else
		{
			LLVector3d center = getCenterGlobal() + LLVector3d(256.f, 0, 0.f);
			LLViewerRegion *regionp = LLWorld::getInstance()->getRegionFromPosGlobal(center);
			if (regionp)
			{
				// OK, we need to do some hackery here - different simulators no longer use
				// the same composition values, necessarily.
				// If we're attempting to blend, then we want to make the fractional part of
				// this region match the fractional of the adjacent.  For now, just minimize
				// the delta.
				F32 our_comp = getComposition()->getValueScaled(255.f, (F32)y);
				F32 adj_comp = regionp->getComposition()->getValueScaled(x - 256.f, (F32)y);
				while (llabs(our_comp - adj_comp) >= 1.f)
				{
					if (our_comp > adj_comp)
					{
						adj_comp += 1.f;
					}
					else
					{
						adj_comp -= 1.f;
					}
				}
				return adj_comp;
			}
		}
	}
	else if (y >= 256)
	{
		LLVector3d center = getCenterGlobal() + LLVector3d(0.f, 256.f, 0.f);
		LLViewerRegion *regionp = LLWorld::getInstance()->getRegionFromPosGlobal(center);
		if (regionp)
		{
			// OK, we need to do some hackery here - different simulators no longer use
			// the same composition values, necessarily.
			// If we're attempting to blend, then we want to make the fractional part of
			// this region match the fractional of the adjacent.  For now, just minimize
			// the delta.
			F32 our_comp = getComposition()->getValueScaled((F32)x, 255.f);
			F32 adj_comp = regionp->getComposition()->getValueScaled((F32)x, y - 256.f);
			while (llabs(our_comp - adj_comp) >= 1.f)
			{
				if (our_comp > adj_comp)
				{
					adj_comp += 1.f;
				}
				else
				{
					adj_comp -= 1.f;
				}
			}
			return adj_comp;
		}
	}

	return getComposition()->getValueScaled((F32)x, (F32)y);
}

void LLViewerRegion::calculateCenterGlobal() 
{
	mImpl->mCenterGlobal = mImpl->mOriginGlobal;
	mImpl->mCenterGlobal.mdV[VX] += 0.5 * mWidth;
	mImpl->mCenterGlobal.mdV[VY] += 0.5 * mWidth;
	mImpl->mCenterGlobal.mdV[VZ] = 0.5 * mImpl->mLandp->getMinZ() + mImpl->mLandp->getMaxZ();
}

void LLViewerRegion::calculateCameraDistance()
{
	mCameraDistanceSquared = (F32)(gAgentCamera.getCameraPositionGlobal() - getCenterGlobal()).magVecSquared();
}

std::ostream& operator<<(std::ostream &s, const LLViewerRegion &region)
{
	s << "{ ";
	s << region.mImpl->mHost;
	s << " mOriginGlobal = " << region.getOriginGlobal()<< "\n";
    std::string name(region.getName()), zone(region.getZoning());
    if (! name.empty())
    {
        s << " mName         = " << name << '\n';
    }
    if (! zone.empty())
    {
        s << " mZoning       = " << zone << '\n';
    }
	s << "}";
	return s;
}


// ---------------- Protected Member Functions ----------------

void LLViewerRegion::updateNetStats()
{
	F32 dt = mImpl->mLastNetUpdate.getElapsedTimeAndResetF32();

	LLCircuitData *cdp = gMessageSystem->mCircuitInfo.findCircuit(mImpl->mHost);
	if (!cdp)
	{
		mAlive = false;
		return;
	}

	mAlive = true;
	mDeltaTime = dt;

	mLastPacketsIn =	mPacketsIn;
	mLastBitsIn =		mBitsIn;
	mLastPacketsOut =	mPacketsOut;
	mLastPacketsLost =	mPacketsLost;

	mPacketsIn =				cdp->getPacketsIn();
	mBitsIn =					8 * cdp->getBytesIn();
	mPacketsOut =				cdp->getPacketsOut();
	mPacketsLost =				cdp->getPacketsLost();
	mPingDelay =				cdp->getPingDelay();

	mBitsReceived += mBitsIn - mLastBitsIn;
	mPacketsReceived += mPacketsIn - mLastPacketsIn;
}


U32 LLViewerRegion::getPacketsLost() const
{
	LLCircuitData *cdp = gMessageSystem->mCircuitInfo.findCircuit(mImpl->mHost);
	if (!cdp)
	{
		llinfos << "LLViewerRegion::getPacketsLost couldn't find circuit for " << mImpl->mHost << llendl;
		return 0;
	}
	else
	{
		return cdp->getPacketsLost();
	}
}

S32 LLViewerRegion::getHttpResponderID() const
{
	return mImpl->mHttpResponderID;
}

BOOL LLViewerRegion::pointInRegionGlobal(const LLVector3d &point_global) const
{
	LLVector3 pos_region = getPosRegionFromGlobal(point_global);

	if (pos_region.mV[VX] < 0)
	{
		return FALSE;
	}
	if (pos_region.mV[VX] >= mWidth)
	{
		return FALSE;
	}
	if (pos_region.mV[VY] < 0)
	{
		return FALSE;
	}
	if (pos_region.mV[VY] >= mWidth)
	{
		return FALSE;
	}
	return TRUE;
}

LLVector3 LLViewerRegion::getPosRegionFromGlobal(const LLVector3d &point_global) const
{
	LLVector3 pos_region;
	pos_region.setVec(point_global - mImpl->mOriginGlobal);
	return pos_region;
}

LLVector3d LLViewerRegion::getPosGlobalFromRegion(const LLVector3 &pos_region) const
{
	LLVector3d pos_region_d;
	pos_region_d.setVec(pos_region);
	return pos_region_d + mImpl->mOriginGlobal;
}

LLVector3 LLViewerRegion::getPosAgentFromRegion(const LLVector3 &pos_region) const
{
	LLVector3d pos_global = getPosGlobalFromRegion(pos_region);

	return gAgent.getPosAgentFromGlobal(pos_global);
}

LLVector3 LLViewerRegion::getPosRegionFromAgent(const LLVector3 &pos_agent) const
{
	return pos_agent - getOriginAgent();
}

F32 LLViewerRegion::getLandHeightRegion(const LLVector3& region_pos)
{
	return mImpl->mLandp->resolveHeightRegion( region_pos );
}

bool LLViewerRegion::isAlive()
{
	return mAlive;
}

BOOL LLViewerRegion::isOwnedSelf(const LLVector3& pos)
{
	if (mParcelOverlay)
	{
		return mParcelOverlay->isOwnedSelf(pos);
	} else {
		return FALSE;
	}
}

// Owned by a group you belong to?  (officer or member)
BOOL LLViewerRegion::isOwnedGroup(const LLVector3& pos)
{
	if (mParcelOverlay)
	{
		return mParcelOverlay->isOwnedGroup(pos);
	} else {
		return FALSE;
	}
}

// the new TCP coarse location handler node
class CoarseLocationUpdate : public LLHTTPNode
{
public:
	virtual void post(
		ResponsePtr responder,
		const LLSD& context,
		const LLSD& input) const
	{
		LLHost host(input["sender"].asString());
		LLViewerRegion* region = LLWorld::getInstance()->getRegion(host);
		if( !region )
		{
			return;
		}

		S32 target_index = input["body"]["Index"][0]["Prey"].asInteger();
		S32 you_index    = input["body"]["Index"][0]["You" ].asInteger();

		LLDynamicArray<U32>* avatar_locs = &region->mMapAvatars;
		LLDynamicArray<LLUUID>* avatar_ids = &region->mMapAvatarIDs;
		avatar_locs->reset();
		avatar_ids->reset();

		//llinfos << "coarse locations agent[0] " << input["body"]["AgentData"][0]["AgentID"].asUUID() << llendl;
		//llinfos << "my agent id = " << gAgent.getID() << llendl;
		//llinfos << ll_pretty_print_sd(input) << llendl;

		LLSD 
			locs   = input["body"]["Location"],
			agents = input["body"]["AgentData"];
		LLSD::array_iterator 
			locs_it = locs.beginArray(), 
			agents_it = agents.beginArray();
		BOOL has_agent_data = input["body"].has("AgentData");

		for(int i=0; 
			locs_it != locs.endArray(); 
			i++, locs_it++)
		{
			U8 
				x = locs_it->get("X").asInteger(),
				y = locs_it->get("Y").asInteger(),
				z = locs_it->get("Z").asInteger();
			// treat the target specially for the map, and don't add you or the target
			if(i == target_index)
			{
				LLVector3d global_pos(region->getOriginGlobal());
				global_pos.mdV[VX] += (F64)x;
				global_pos.mdV[VY] += (F64)y;
				global_pos.mdV[VZ] += (F64)z * 4.0;
				LLAvatarTracker::instance().setTrackedCoarseLocation(global_pos);
			}
			else if( i != you_index)
			{
				U32 loc = x << 16 | y << 8 | z; loc = loc;
				U32 pos = 0x0;
				pos |= x;
				pos <<= 8;
				pos |= y;
				pos <<= 8;
				pos |= z;
				avatar_locs->put(pos);
				//llinfos << "next pos: " << x << "," << y << "," << z << ": " << pos << llendl;
				if(has_agent_data) // for backwards compatibility with old message format
				{
					LLUUID agent_id(agents_it->get("AgentID").asUUID());
					//llinfos << "next agent: " << agent_id.asString() << llendl;
					avatar_ids->put(agent_id);
				}
			}
			if (has_agent_data)
			{
				agents_it++;
			}
		}
	}
};

// build the coarse location HTTP node under the "/message" URL
LLHTTPRegistration<CoarseLocationUpdate>
   gHTTPRegistrationCoarseLocationUpdate(
	   "/message/CoarseLocationUpdate");


// the deprecated coarse location handler
void LLViewerRegion::updateCoarseLocations(LLMessageSystem* msg)
{
	//llinfos << "CoarseLocationUpdate" << llendl;
	mMapAvatars.reset();
	mMapAvatarIDs.reset(); // only matters in a rare case but it's good to be safe.

	U8 x_pos = 0;
	U8 y_pos = 0;
	U8 z_pos = 0;

	U32 pos = 0x0;

	S16 agent_index;
	S16 target_index;
	msg->getS16Fast(_PREHASH_Index, _PREHASH_You, agent_index);
	msg->getS16Fast(_PREHASH_Index, _PREHASH_Prey, target_index);

	BOOL has_agent_data = msg->has(_PREHASH_AgentData);
	S32 count = msg->getNumberOfBlocksFast(_PREHASH_Location);
	for(S32 i = 0; i < count; i++)
	{
		msg->getU8Fast(_PREHASH_Location, _PREHASH_X, x_pos, i);
		msg->getU8Fast(_PREHASH_Location, _PREHASH_Y, y_pos, i);
		msg->getU8Fast(_PREHASH_Location, _PREHASH_Z, z_pos, i);
		LLUUID agent_id = LLUUID::null;
		if(has_agent_data)
		{
			msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id, i);
		}

		//llinfos << "  object X: " << (S32)x_pos << " Y: " << (S32)y_pos
		//		<< " Z: " << (S32)(z_pos * 4)
		//		<< llendl;

		// treat the target specially for the map
		if(i == target_index)
		{
			LLVector3d global_pos(mImpl->mOriginGlobal);
			global_pos.mdV[VX] += (F64)(x_pos);
			global_pos.mdV[VY] += (F64)(y_pos);
			global_pos.mdV[VZ] += (F64)(z_pos) * 4.0;
			LLAvatarTracker::instance().setTrackedCoarseLocation(global_pos);
		}
		
		//don't add you
		if( i != agent_index)
		{
			pos = 0x0;
			pos |= x_pos;
			pos <<= 8;
			pos |= y_pos;
			pos <<= 8;
			pos |= z_pos;
			mMapAvatars.put(pos);
			if(has_agent_data)
			{
				mMapAvatarIDs.put(agent_id);
			}
		}
	}
}

void LLViewerRegion::getInfo(LLSD& info)
{
	info["Region"]["Host"] = getHost().getIPandPort();
	info["Region"]["Name"] = getName();
	U32 x, y;
	from_region_handle(getHandle(), &x, &y);
	info["Region"]["Handle"]["x"] = (LLSD::Integer)x;
	info["Region"]["Handle"]["y"] = (LLSD::Integer)y;
}

void LLViewerRegion::getSimulatorFeatures(LLSD& sim_features)
{
	sim_features = mSimulatorFeatures;

}

void LLViewerRegion::setSimulatorFeatures(const LLSD& sim_features)
{
	std::stringstream str;
	
	LLSDSerialize::toPrettyXML(sim_features, str);
	llinfos << str.str() << llendl;
	mSimulatorFeatures = sim_features;
}

//this is called when the parent is not cacheable.
//move all orphan children out of cache and insert to rendering octree.
void LLViewerRegion::findOrphans(U32 parent_id)
{
	orphan_list_t::iterator iter = mOrphanMap.find(parent_id);
	if(iter != mOrphanMap.end())
	{
		std::vector<U32>* children = &mOrphanMap[parent_id];
		for(S32 i = 0; i < children->size(); i++)
		{
			forceToRemoveFromCache((*children)[i], NULL);
		}
		children->clear();
		mOrphanMap.erase(parent_id);
	}
}

void LLViewerRegion::decodeBoundingInfo(LLVOCacheEntry* entry)
{
	if(!sVOCacheCullingEnabled)
	{
		gObjectList.processObjectUpdateFromCache(entry, this);
		return;
	}

	if(entry != NULL && !entry->getEntry())
	{
		entry->setOctreeEntry(NULL);
	}
	else if(entry->getGroup() != NULL)
	{
		return; //already in octree, no post processing.
	}

	LLVector3 pos;
	LLVector3 scale;
	LLQuaternion rot;

	//decode spatial info and parent info
	U32 parent_id = LLViewerObject::extractSpatialExtents(entry->getDP(), pos, scale, rot);
	
	if(parent_id > 0) //has parent
	{
		entry->setParentID(parent_id);
	
		//1, find the parent in cache
		LLVOCacheEntry* parent = getCacheEntry(parent_id);
		
		//2, parent is not in the cache, put into the orphan list.
		if(!parent)
		{
			mOrphanMap[parent_id].push_back(entry->getLocalID());
		}
		else //parent in cache
		{
			if(!parent->isState(LLVOCacheEntry::INACTIVE)) 
			{
				//parent is visible, so is the child.
				addVisibleCacheEntry(entry);
			}
			else
			{
				parent->addChild(entry);
			}
		}

		return;
	}
	
	//
	//no parent
	//
	entry->setBoundingInfo(pos, scale);
	if(!entry->getGroup() && entry->isState(LLVOCacheEntry::INACTIVE))
	{
		addToVOCacheTree(entry);
	}

	if(!parent_id) //a potential parent
	{
		//find all children and update their bounding info
		orphan_list_t::iterator iter = mOrphanMap.find(entry->getLocalID());
		if(iter != mOrphanMap.end())
		{			
			std::vector<U32>* orphans = &mOrphanMap[entry->getLocalID()];
			S32 size = orphans->size();
			for(S32 i = 0; i < size; i++)
			{
				LLVOCacheEntry* child = getCacheEntry((*orphans)[i]);
				if(child)
				{
					entry->addChild(child);
				}
			}
			orphans->clear();
			mOrphanMap.erase(entry->getLocalID());
		}
	}
	
	return ;
}

LLViewerRegion::eCacheUpdateResult LLViewerRegion::cacheFullUpdate(LLDataPackerBinaryBuffer &dp, U32 flags)
{
	eCacheUpdateResult result;
	U32 crc;
	U32 local_id;

	LLViewerObject::unpackU32(&dp, local_id, "LocalID");
	LLViewerObject::unpackU32(&dp, crc, "CRC");

	LLVOCacheEntry* entry = getCacheEntry(local_id);
	
	if (entry)
	{
		// we've seen this object before
		if (entry->getCRC() == crc)
		{
			// Record a hit
			entry->recordDupe();
			result = CACHE_UPDATE_DUPE;
		}
		else
		{
			// Update the cache entry
			LLPointer<LLVOCacheEntry> new_entry = new LLVOCacheEntry(local_id, crc, dp);
			
			//if visible, update it
			if(!entry->isState(LLVOCacheEntry::INACTIVE))
			{
				replaceCacheEntry(entry, new_entry);
			}
			else //invisible
			{
				//remove old entry
				killCacheEntry(entry);
				entry = new_entry;
				
				mImpl->mCacheMap[local_id] = entry;
				decodeBoundingInfo(entry);
			}
			
			result = CACHE_UPDATE_CHANGED;
		}
	}
	else
	{
		// we haven't seen this object before
		// Create new entry and add to map
		result = CACHE_UPDATE_ADDED;
		entry = new LLVOCacheEntry(local_id, crc, dp);
		
		mImpl->mCacheMap[local_id] = entry;
		
		decodeBoundingInfo(entry);
	}
	entry->setUpdateFlags(flags);

	return result;
}

LLViewerRegion::eCacheUpdateResult LLViewerRegion::cacheFullUpdate(LLViewerObject* objectp, LLDataPackerBinaryBuffer &dp, U32 flags)
{
	eCacheUpdateResult result = cacheFullUpdate(dp, flags);

#if 0
	LLVOCacheEntry* entry = mImpl->mCacheMap[objectp->getLocalID()];
	if(!entry)
	{
		return result;
	}

	if(objectp->mDrawable.notNull() && !entry->getEntry())
	{
		entry->setOctreeEntry(objectp->mDrawable->getEntry());
	}
	if(entry->getEntry() && entry->getEntry()->hasDrawable() && entry->isState(LLVOCacheEntry::INACTIVE))
	{
		addActiveCacheEntry(entry);
	}
#endif

	return result;
}

LLVOCacheEntry* LLViewerRegion::getCacheEntryForOctree(U32 local_id)
{
	if(!sVOCacheCullingEnabled)
	{
		return NULL;
	}

	LLVOCacheEntry* entry = getCacheEntry(local_id);
	removeFromVOCacheTree(entry);
		
	return entry;
}

LLVOCacheEntry* LLViewerRegion::getCacheEntry(U32 local_id)
{
	LLVOCacheEntry::vocache_entry_map_t::iterator iter = mImpl->mCacheMap.find(local_id);
	if(iter != mImpl->mCacheMap.end())
	{
		return iter->second;
	}
	return NULL;
}

void LLViewerRegion::addCacheMiss(U32 id, LLViewerRegion::eCacheMissType miss_type)
{
#if 0
	mCacheMissList.insert(CacheMissItem(id, miss_type));
#else
	mCacheMissList.push_back(CacheMissItem(id, miss_type));
#endif
}

// Get data packer for this object, if we have cached data
// AND the CRC matches. JC
bool LLViewerRegion::probeCache(U32 local_id, U32 crc, U32 flags, U8 &cache_miss_type)
{
	//llassert(mCacheLoaded);  This assert failes often, changing to early-out -- davep, 2010/10/18

	LLVOCacheEntry* entry = getCacheEntry(local_id);

	if (entry)
	{
		// we've seen this object before
		if (entry->getCRC() == crc)
		{
			// Record a hit
			entry->recordHit();
			cache_miss_type = CACHE_MISS_TYPE_NONE;
			entry->setUpdateFlags(flags);
			
			if(entry->isState(LLVOCacheEntry::ACTIVE))
			{
				((LLDrawable*)entry->getEntry()->getDrawable())->getVObj()->loadFlags(flags);
				return true;
			}

			if(entry->getGroup() || !entry->isState(LLVOCacheEntry::INACTIVE))
			{
				return true;
			}

			decodeBoundingInfo(entry);
			return true;
		}
		else
		{
			// llinfos << "CRC miss for " << local_id << llendl;

			addCacheMiss(local_id, CACHE_MISS_TYPE_CRC);
		}
	}
	else
	{
		// llinfos << "Cache miss for " << local_id << llendl;
		addCacheMiss(local_id, CACHE_MISS_TYPE_FULL);
	}

	return false;
}

void LLViewerRegion::addCacheMissFull(const U32 local_id)
{
	addCacheMiss(local_id, CACHE_MISS_TYPE_FULL);
}

void LLViewerRegion::requestCacheMisses()
{
	if (!mCacheMissList.size()) 
	{
		return;
	}

	LLMessageSystem* msg = gMessageSystem;
	BOOL start_new_message = TRUE;
	S32 blocks = 0;
	
	//send requests for all cache-missed objects
	for (CacheMissItem::cache_miss_list_t::iterator iter = mCacheMissList.begin(); iter != mCacheMissList.end(); ++iter)
	{
		if (start_new_message)
		{
			msg->newMessageFast(_PREHASH_RequestMultipleObjects);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
			msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
			start_new_message = FALSE;
		}

		msg->nextBlockFast(_PREHASH_ObjectData);
		msg->addU8Fast(_PREHASH_CacheMissType, (*iter).mType);
		msg->addU32Fast(_PREHASH_ID, (*iter).mID);
		blocks++;

		if (blocks >= 255)
		{
			sendReliableMessage();
			start_new_message = TRUE;
			blocks = 0;
		}
	}

	// finish any pending message
	if (!start_new_message)
	{
		sendReliableMessage();
	}	

	mCacheDirty = TRUE ;
	// llinfos << "KILLDEBUG Sent cache miss full " << full_count << " crc " << crc_count << llendl;
	LLViewerStatsRecorder::instance().requestCacheMissesEvent(mCacheMissList.size());
	LLViewerStatsRecorder::instance().log(0.2f);

	mCacheMissList.clear();
}

void LLViewerRegion::dumpCache()
{
	const S32 BINS = 4;
	S32 hit_bin[BINS];
	S32 change_bin[BINS];

	S32 i;
	for (i = 0; i < BINS; ++i)
	{
		hit_bin[i] = 0;
		change_bin[i] = 0;
	}

	LLVOCacheEntry *entry;
	for(LLVOCacheEntry::vocache_entry_map_t::iterator iter = mImpl->mCacheMap.begin(); iter != mImpl->mCacheMap.end(); ++iter)
	{
		entry = iter->second ;

		S32 hits = entry->getHitCount();
		S32 changes = entry->getCRCChangeCount();

		hits = llclamp(hits, 0, BINS-1);
		changes = llclamp(changes, 0, BINS-1);

		hit_bin[hits]++;
		change_bin[changes]++;
	}

	llinfos << "Count " << mImpl->mCacheMap.size() << llendl;
	for (i = 0; i < BINS; i++)
	{
		llinfos << "Hits " << i << " " << hit_bin[i] << llendl;
	}
	for (i = 0; i < BINS; i++)
	{
		llinfos << "Changes " << i << " " << change_bin[i] << llendl;
	}
}

void LLViewerRegion::unpackRegionHandshake()
{
	LLMessageSystem *msg = gMessageSystem;

	U32 region_flags;
	U8 sim_access;
	std::string sim_name;
	LLUUID sim_owner;
	BOOL is_estate_manager;
	F32 water_height;
	F32 billable_factor;
	LLUUID cache_id;

	msg->getU32		("RegionInfo", "RegionFlags", region_flags);
	msg->getU8		("RegionInfo", "SimAccess", sim_access);
	msg->getString	("RegionInfo", "SimName", sim_name);
	msg->getUUID	("RegionInfo", "SimOwner", sim_owner);
	msg->getBOOL	("RegionInfo", "IsEstateManager", is_estate_manager);
	msg->getF32		("RegionInfo", "WaterHeight", water_height);
	msg->getF32		("RegionInfo", "BillableFactor", billable_factor);
	msg->getUUID	("RegionInfo", "CacheID", cache_id );

	setRegionFlags(region_flags);
	setSimAccess(sim_access);
	setRegionNameAndZone(sim_name);
	setOwner(sim_owner);
	setIsEstateManager(is_estate_manager);
	setWaterHeight(water_height);
	setBillableFactor(billable_factor);
	setCacheID(cache_id);

	LLUUID region_id;
	msg->getUUID("RegionInfo2", "RegionID", region_id);
	setRegionID(region_id);
	
	// Retrieve the CR-53 (Homestead/Land SKU) information
	S32 classID = 0;
	S32 cpuRatio = 0;
	std::string coloName;
	std::string productSKU;
	std::string productName;

	// the only reasonable way to decide if we actually have any data is to
	// check to see if any of these fields have positive sizes
	if (msg->getSize("RegionInfo3", "ColoName") > 0 ||
	    msg->getSize("RegionInfo3", "ProductSKU") > 0 ||
	    msg->getSize("RegionInfo3", "ProductName") > 0)
	{
		msg->getS32     ("RegionInfo3", "CPUClassID",  classID);
		msg->getS32     ("RegionInfo3", "CPURatio",    cpuRatio);
		msg->getString  ("RegionInfo3", "ColoName",    coloName);
		msg->getString  ("RegionInfo3", "ProductSKU",  productSKU);
		msg->getString  ("RegionInfo3", "ProductName", productName);
		
		mClassID = classID;
		mCPURatio = cpuRatio;
		mColoName = coloName;
		mProductSKU = productSKU;
		mProductName = productName;
	}

	LLVLComposition *compp = getComposition();
	if (compp)
	{
		LLUUID tmp_id;

		msg->getUUID("RegionInfo", "TerrainDetail0", tmp_id);
		compp->setDetailTextureID(0, tmp_id);
		msg->getUUID("RegionInfo", "TerrainDetail1", tmp_id);
		compp->setDetailTextureID(1, tmp_id);
		msg->getUUID("RegionInfo", "TerrainDetail2", tmp_id);
		compp->setDetailTextureID(2, tmp_id);
		msg->getUUID("RegionInfo", "TerrainDetail3", tmp_id);
		compp->setDetailTextureID(3, tmp_id);

		F32 tmp_f32;
		msg->getF32("RegionInfo", "TerrainStartHeight00", tmp_f32);
		compp->setStartHeight(0, tmp_f32);
		msg->getF32("RegionInfo", "TerrainStartHeight01", tmp_f32);
		compp->setStartHeight(1, tmp_f32);
		msg->getF32("RegionInfo", "TerrainStartHeight10", tmp_f32);
		compp->setStartHeight(2, tmp_f32);
		msg->getF32("RegionInfo", "TerrainStartHeight11", tmp_f32);
		compp->setStartHeight(3, tmp_f32);

		msg->getF32("RegionInfo", "TerrainHeightRange00", tmp_f32);
		compp->setHeightRange(0, tmp_f32);
		msg->getF32("RegionInfo", "TerrainHeightRange01", tmp_f32);
		compp->setHeightRange(1, tmp_f32);
		msg->getF32("RegionInfo", "TerrainHeightRange10", tmp_f32);
		compp->setHeightRange(2, tmp_f32);
		msg->getF32("RegionInfo", "TerrainHeightRange11", tmp_f32);
		compp->setHeightRange(3, tmp_f32);

		// If this is an UPDATE (params already ready, we need to regenerate
		// all of our terrain stuff, by
		if (compp->getParamsReady())
		{
			//this line creates frame stalls on region crossing and removing it appears to have no effect
			//getLand().dirtyAllPatches();
		}
		else
		{
			compp->setParamsReady();
		}
	}


	// Now that we have the name, we can load the cache file
	// off disk.
	loadObjectCache();

	// After loading cache, signal that simulator can start
	// sending data.
	// TODO: Send all upstream viewer->sim handshake info here.
	LLHost host = msg->getSender();
	msg->newMessage("RegionHandshakeReply");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID", gAgent.getID());
	msg->addUUID("SessionID", gAgent.getSessionID());
	msg->nextBlock("RegionInfo");

	U32 flags = 0;
	if(sVOCacheCullingEnabled)
	{
		flags = 0x00000001; //set the bit 0 to be 1 to ask sim to send all cacheable objects.
		if(mImpl->mCacheMap.empty())
		{
			flags |= 0x00000002; //set the bit 1 to be 1 to tell sim the cache file is empty, no need to send cache probes.
		}
	}
	msg->addU32("Flags", flags );
	msg->sendReliable(host);

	mRegionTimer.reset(); //reset region timer.
}

void LLViewerRegionImpl::buildCapabilityNames(LLSD& capabilityNames)
{
	capabilityNames.append("AgentState");
	capabilityNames.append("AttachmentResources");
	capabilityNames.append("AvatarPickerSearch");
	capabilityNames.append("CharacterProperties");
	capabilityNames.append("ChatSessionRequest");
	capabilityNames.append("CopyInventoryFromNotecard");
	capabilityNames.append("CreateInventoryCategory");
	capabilityNames.append("DispatchRegionInfo");
	capabilityNames.append("EnvironmentSettings");
	capabilityNames.append("EstateChangeInfo");
	capabilityNames.append("EventQueueGet");

	if (gSavedSettings.getBOOL("UseHTTPInventory"))
	{
		capabilityNames.append("FetchLib2");
		capabilityNames.append("FetchLibDescendents2");
		capabilityNames.append("FetchInventory2");
		capabilityNames.append("FetchInventoryDescendents2");
	}

	capabilityNames.append("GetDisplayNames");
	capabilityNames.append("GetMesh");
	capabilityNames.append("GetObjectCost");
	capabilityNames.append("GetObjectPhysicsData");
	capabilityNames.append("GetTexture");
	capabilityNames.append("GroupMemberData");
	capabilityNames.append("GroupProposalBallot");
	capabilityNames.append("HomeLocation");
	capabilityNames.append("LandResources");
	capabilityNames.append("MapLayer");
	capabilityNames.append("MapLayerGod");
	capabilityNames.append("MeshUploadFlag");	
	capabilityNames.append("NavMeshGenerationStatus");
	capabilityNames.append("NewFileAgentInventory");
	capabilityNames.append("ObjectMedia");
	capabilityNames.append("ObjectMediaNavigate");
	capabilityNames.append("ObjectNavMeshProperties");
	capabilityNames.append("ParcelPropertiesUpdate");
	capabilityNames.append("ParcelVoiceInfoRequest");
	capabilityNames.append("ProductInfoRequest");
	capabilityNames.append("ProvisionVoiceAccountRequest");
	capabilityNames.append("RemoteParcelRequest");
	capabilityNames.append("RequestTextureDownload");
	capabilityNames.append("ResourceCostSelected");
	capabilityNames.append("RetrieveNavMeshSrc");
	capabilityNames.append("SearchStatRequest");
	capabilityNames.append("SearchStatTracking");
	capabilityNames.append("SendPostcard");
	capabilityNames.append("SendUserReport");
	capabilityNames.append("SendUserReportWithScreenshot");
	capabilityNames.append("ServerReleaseNotes");
	capabilityNames.append("SetDisplayName");
	capabilityNames.append("SimConsoleAsync");
	capabilityNames.append("SimulatorFeatures");
	capabilityNames.append("StartGroupProposal");
	capabilityNames.append("TerrainNavMeshProperties");
	capabilityNames.append("TextureStats");
	capabilityNames.append("UntrustedSimulatorMessage");
	capabilityNames.append("UpdateAgentInformation");
	capabilityNames.append("UpdateAgentLanguage");
	capabilityNames.append("UpdateGestureAgentInventory");
	capabilityNames.append("UpdateGestureTaskInventory");
	capabilityNames.append("UpdateNotecardAgentInventory");
	capabilityNames.append("UpdateNotecardTaskInventory");
	capabilityNames.append("UpdateScriptAgent");
	capabilityNames.append("UpdateScriptTask");
	capabilityNames.append("UploadBakedTexture");
	capabilityNames.append("ViewerMetrics");
	capabilityNames.append("ViewerStartAuction");
	capabilityNames.append("ViewerStats");

	// Please add new capabilities alphabetically to reduce
	// merge conflicts.
}

void LLViewerRegion::setSeedCapability(const std::string& url)
{
	if (getCapability("Seed") == url)
    {
		// llwarns << "Ignoring duplicate seed capability" << llendl;
		return;
    }
	
	delete mImpl->mEventPoll;
	mImpl->mEventPoll = NULL;
	
	mImpl->mCapabilities.clear();
	setCapability("Seed", url);

	LLSD capabilityNames = LLSD::emptyArray();
	mImpl->buildCapabilityNames(capabilityNames);

	llinfos << "posting to seed " << url << llendl;

	S32 id = ++mImpl->mHttpResponderID;
	LLHTTPClient::post(url, capabilityNames, 
						BaseCapabilitiesComplete::build(getHandle(), id),
						LLSD(), CAP_REQUEST_TIMEOUT);
}

S32 LLViewerRegion::getNumSeedCapRetries()
{
	return mImpl->mSeedCapAttempts;
}

void LLViewerRegion::failedSeedCapability()
{
	// Should we retry asking for caps?
	mImpl->mSeedCapAttempts++;
	std::string url = getCapability("Seed");
	if ( url.empty() )
	{
		LL_WARNS2("AppInit", "Capabilities") << "Failed to get seed capabilities, and can not determine url for retries!" << LL_ENDL;
		return;
	}
	// After a few attempts, continue login.  We will keep trying once in-world:
	if ( mImpl->mSeedCapAttempts >= mImpl->mSeedCapMaxAttemptsBeforeLogin &&
		 STATE_SEED_GRANTED_WAIT == LLStartUp::getStartupState() )
	{
		LLStartUp::setStartupState( STATE_SEED_CAP_GRANTED );
	}

	if ( mImpl->mSeedCapAttempts < mImpl->mSeedCapMaxAttempts)
	{
		LLSD capabilityNames = LLSD::emptyArray();
		mImpl->buildCapabilityNames(capabilityNames);

		llinfos << "posting to seed " << url << " (retry " 
				<< mImpl->mSeedCapAttempts << ")" << llendl;

		S32 id = ++mImpl->mHttpResponderID;
		LLHTTPClient::post(url, capabilityNames, 
						BaseCapabilitiesComplete::build(getHandle(), id),
						LLSD(), CAP_REQUEST_TIMEOUT);
	}
	else
	{
		// *TODO: Give a user pop-up about this error?
		LL_WARNS2("AppInit", "Capabilities") << "Failed to get seed capabilities from '" << url << "' after " << mImpl->mSeedCapAttempts << " attempts.  Giving up!" << LL_ENDL;
	}
}

class SimulatorFeaturesReceived : public LLHTTPClient::Responder
{
	LOG_CLASS(SimulatorFeaturesReceived);
public:
    SimulatorFeaturesReceived(const std::string& retry_url, U64 region_handle, 
			S32 attempt = 0, S32 max_attempts = MAX_CAP_REQUEST_ATTEMPTS)
	: mRetryURL(retry_url), mRegionHandle(region_handle), mAttempt(attempt), mMaxAttempts(max_attempts)
    { }
	
	
    void error(U32 statusNum, const std::string& reason)
    {
		LL_WARNS2("AppInit", "SimulatorFeatures") << statusNum << ": " << reason << LL_ENDL;
		retry();
    }

    void result(const LLSD& content)
    {
		LLViewerRegion *regionp = LLWorld::getInstance()->getRegionFromHandle(mRegionHandle);
		if(!regionp) //region is removed or responder is not created.
		{
			LL_WARNS2("AppInit", "SimulatorFeatures") << "Received results for region that no longer exists!" << LL_ENDL;
			return ;
		}
		
		regionp->setSimulatorFeatures(content);
	}

private:
	void retry()
	{
		if (mAttempt < mMaxAttempts)
		{
			mAttempt++;
			LL_WARNS2("AppInit", "SimulatorFeatures") << "Re-trying '" << mRetryURL << "'.  Retry #" << mAttempt << LL_ENDL;
			LLHTTPClient::get(mRetryURL, new SimulatorFeaturesReceived(*this), LLSD(), CAP_REQUEST_TIMEOUT);
		}
	}
	
	std::string mRetryURL;
	U64 mRegionHandle;
	S32 mAttempt;
	S32 mMaxAttempts;
};


void LLViewerRegion::setCapability(const std::string& name, const std::string& url)
{
	if(name == "EventQueueGet")
	{
		delete mImpl->mEventPoll;
		mImpl->mEventPoll = NULL;
		mImpl->mEventPoll = new LLEventPoll(url, getHost());
	}
	else if(name == "UntrustedSimulatorMessage")
	{
		LLHTTPSender::setSender(mImpl->mHost, new LLCapHTTPSender(url));
	}
	else if (name == "SimulatorFeatures")
	{
		// kick off a request for simulator features
		LLHTTPClient::get(url, new SimulatorFeaturesReceived(url, getHandle()), LLSD(), CAP_REQUEST_TIMEOUT);
	}
	else
	{
		mImpl->mCapabilities[name] = url;
		if(name == "GetTexture")
		{
			mHttpUrl = url ;
		}
	}
}

bool LLViewerRegion::isSpecialCapabilityName(const std::string &name)
{
	return name == "EventQueueGet" || name == "UntrustedSimulatorMessage";
}

std::string LLViewerRegion::getCapability(const std::string& name) const
{
	CapabilityMap::const_iterator iter = mImpl->mCapabilities.find(name);
	if(iter == mImpl->mCapabilities.end())
	{
		return "";
	}

	return iter->second;
}

bool LLViewerRegion::capabilitiesReceived() const
{
	return mCapabilitiesReceived;
}

void LLViewerRegion::setCapabilitiesReceived(bool received)
{
	mCapabilitiesReceived = received;

	// Tell interested parties that we've received capabilities,
	// so that they can safely use getCapability().
	if (received)
	{
		mCapabilitiesReceivedSignal(getRegionID());

		// This is a single-shot signal. Forget callbacks to save resources.
		mCapabilitiesReceivedSignal.disconnect_all_slots();
	}
}

boost::signals2::connection LLViewerRegion::setCapabilitiesReceivedCallback(const caps_received_signal_t::slot_type& cb)
{
	return mCapabilitiesReceivedSignal.connect(cb);
}

void LLViewerRegion::logActiveCapabilities() const
{
	int count = 0;
	CapabilityMap::const_iterator iter;
	for (iter = mImpl->mCapabilities.begin(); iter != mImpl->mCapabilities.end(); ++iter, ++count)
	{
		if (!iter->second.empty())
		{
			llinfos << iter->first << " URL is " << iter->second << llendl;
		}
	}
	llinfos << "Dumped " << count << " entries." << llendl;
}

LLSpatialPartition* LLViewerRegion::getSpatialPartition(U32 type)
{
	if (type < mImpl->mObjectPartition.size() && type < PARTITION_VO_CACHE)
	{
		return (LLSpatialPartition*)mImpl->mObjectPartition[type];
	}
	return NULL;
}

LLVOCachePartition* LLViewerRegion::getVOCachePartition()
{
	if(PARTITION_VO_CACHE < mImpl->mObjectPartition.size())
	{
		return (LLVOCachePartition*)mImpl->mObjectPartition[PARTITION_VO_CACHE];
	}
	return NULL;
}

// the viewer can not yet distinquish between normal- and estate-owned objects
// so we collapse these two bits and enable the UI if either are set
const U32 ALLOW_RETURN_ENCROACHING_OBJECT = REGION_FLAGS_ALLOW_RETURN_ENCROACHING_OBJECT
											| REGION_FLAGS_ALLOW_RETURN_ENCROACHING_ESTATE_OBJECT;

bool LLViewerRegion::objectIsReturnable(const LLVector3& pos, const std::vector<LLBBox>& boxes) const
{
	return (mParcelOverlay != NULL)
		&& (mParcelOverlay->isOwnedSelf(pos)
			|| mParcelOverlay->isOwnedGroup(pos)
			|| ((mRegionFlags & ALLOW_RETURN_ENCROACHING_OBJECT)
				&& mParcelOverlay->encroachesOwned(boxes)) );
}

bool LLViewerRegion::childrenObjectReturnable( const std::vector<LLBBox>& boxes ) const
{
	bool result = false;
	result = ( mParcelOverlay && mParcelOverlay->encroachesOnUnowned( boxes ) ) ? 1 : 0;
	return result;
}

bool LLViewerRegion::objectsCrossParcel(const std::vector<LLBBox>& boxes) const
{
	return mParcelOverlay && mParcelOverlay->encroachesOnNearbyParcel(boxes);
}

void LLViewerRegion::getNeighboringRegions( std::vector<LLViewerRegion*>& uniqueRegions )
{
	mImpl->mLandp->getNeighboringRegions( uniqueRegions );
}
void LLViewerRegion::getNeighboringRegionsStatus( std::vector<S32>& regions )
{
	mImpl->mLandp->getNeighboringRegionsStatus( regions );
}
void LLViewerRegion::showReleaseNotes()
{
	std::string url = this->getCapability("ServerReleaseNotes");

	if (url.empty()) {
		// HACK haven't received the capability yet, we'll wait until
		// it arives.
		mReleaseNotesRequested = TRUE;
		return;
	}

	LLWeb::loadURL(url);
	mReleaseNotesRequested = FALSE;
}

std::string LLViewerRegion::getDescription() const
{
    return stringize(*this);
}

bool LLViewerRegion::meshUploadEnabled() const
{
	return (mSimulatorFeatures.has("MeshUploadEnabled") &&
		mSimulatorFeatures["MeshUploadEnabled"].asBoolean());
}

bool LLViewerRegion::meshRezEnabled() const
{
	return (mSimulatorFeatures.has("MeshRezEnabled") &&
				mSimulatorFeatures["MeshRezEnabled"].asBoolean());
}

bool LLViewerRegion::dynamicPathfindingEnabled() const
{
	return ( mSimulatorFeatures.has("DynamicPathfindingEnabled") &&
			 mSimulatorFeatures["DynamicPathfindingEnabled"].asBoolean());
}

