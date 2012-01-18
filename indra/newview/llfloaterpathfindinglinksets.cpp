/** 
 * @file llfloaterpathfindinglinksets.cpp
 * @author William Todd Stinson
 * @brief "Pathfinding linksets" floater, allowing manipulation of the Havok AI pathfinding settings.
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
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
#include "llfloater.h"
#include "llfloaterpathfindinglinksets.h"
#include "llsd.h"
#include "v3math.h"
#include "lltextvalidate.h"
#include "llagent.h"
#include "llfloater.h"
#include "llfloaterreg.h"
#include "lltextbase.h"
#include "lllineeditor.h"
#include "llscrolllistitem.h"
#include "llscrolllistctrl.h"
#include "llcheckboxctrl.h"
#include "llbutton.h"
#include "llresmgr.h"
#include "llviewerregion.h"
#include "llhttpclient.h"
#include "lluuid.h"

//#define XXX_STINSON_USE_FAKE_DATA
#ifdef XXX_STINSON_USE_FAKE_DATA
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#endif // XXX_STINSON_USE_FAKE_DATA

//---------------------------------------------------------------------------
// NavmeshDataGetResponder
//---------------------------------------------------------------------------

class NavmeshDataGetResponder : public LLHTTPClient::Responder
{
public:
	NavmeshDataGetResponder(const std::string& pNavmeshDataGetURL, LLFloaterPathfindingLinksets *pLinksetsFloater);
	virtual ~NavmeshDataGetResponder();

	virtual void result(const LLSD& pContent);
	virtual void error(U32 pStatus, const std::string& pReason);

private:
	NavmeshDataGetResponder(const NavmeshDataGetResponder& pOther);

	std::string                  mNavmeshDataGetURL;
	LLFloaterPathfindingLinksets *mLinksetsFloater;
};

//---------------------------------------------------------------------------
// NavmeshDataPutResponder
//---------------------------------------------------------------------------

class NavmeshDataPutResponder : public LLHTTPClient::Responder
{
public:
	NavmeshDataPutResponder(const std::string& pNavmeshDataPutURL, LLFloaterPathfindingLinksets *pLinksetsFloater);
	virtual ~NavmeshDataPutResponder();

	virtual void result(const LLSD& pContent);
	virtual void error(U32 pStatus, const std::string& pReason);

private:
	NavmeshDataPutResponder(const NavmeshDataPutResponder& pOther);

	std::string                  mNavmeshDataPutURL;
	LLFloaterPathfindingLinksets *mLinksetsFloater;
};

//---------------------------------------------------------------------------
// PathfindingLinkset
//---------------------------------------------------------------------------

PathfindingLinkset::PathfindingLinkset(const std::string &pUUID, const LLSD& pNavmeshItem)
	: mUUID(pUUID),
	mName(),
	mDescription(),
	mLandImpact(0U),
	mLocation(),
	mIsFixed(false),
	mIsWalkable(false),
	mIsPhantom(false),
	mA(0.0f),
	mB(0.0f),
	mC(0.0f),
	mD(0.0f)
{
	llassert(pNavmeshItem.has("name"));
	llassert(pNavmeshItem.get("name").isString());
	mName = pNavmeshItem.get("name").asString();

	llassert(pNavmeshItem.has("description"));
	llassert(pNavmeshItem.get("description").isString());
	mDescription = pNavmeshItem.get("description").asString();

	llassert(pNavmeshItem.has("landimpact"));
	llassert(pNavmeshItem.get("landimpact").isInteger());
	llassert(pNavmeshItem.get("landimpact").asInteger() >= 0);
	mLandImpact = pNavmeshItem.get("landimpact").asInteger();

	llassert(pNavmeshItem.has("permanent"));
	llassert(pNavmeshItem.get("permanent").isBoolean());
	mIsFixed = pNavmeshItem.get("permanent").asBoolean();

	llassert(pNavmeshItem.has("walkable"));
	llassert(pNavmeshItem.get("walkable").isBoolean());
	mIsWalkable = pNavmeshItem.get("walkable").asBoolean();

	llassert(pNavmeshItem.has("phantom"));
	//llassert(pNavmeshItem.get("phantom").isBoolean()); XXX stinson 01/10/2012: this should be a boolean but is not
	mIsPhantom = pNavmeshItem.get("phantom").asBoolean();

	llassert(pNavmeshItem.has("A"));
	llassert(pNavmeshItem.get("A").isReal());
	mA = pNavmeshItem.get("A").asReal() * 100.0f;

	llassert(pNavmeshItem.has("B"));
	llassert(pNavmeshItem.get("B").isReal());
	mB = pNavmeshItem.get("B").asReal() * 100.0f;

	llassert(pNavmeshItem.has("C"));
	llassert(pNavmeshItem.get("C").isReal());
	mC = pNavmeshItem.get("C").asReal() * 100.0f;

	llassert(pNavmeshItem.has("D"));
	llassert(pNavmeshItem.get("D").isReal());
	mD = pNavmeshItem.get("D").asReal() * 100.0f;

	llassert(pNavmeshItem.has("position"));
	llassert(pNavmeshItem.get("position").isArray());
	mLocation.setValue(pNavmeshItem.get("position"));
}

PathfindingLinkset::PathfindingLinkset(const PathfindingLinkset& pOther)
	: mUUID(pOther.mUUID),
	mName(pOther.mName),
	mDescription(pOther.mDescription),
	mLandImpact(pOther.mLandImpact),
	mLocation(pOther.mLocation),
	mIsFixed(pOther.mIsFixed),
	mIsWalkable(pOther.mIsWalkable),
	mIsPhantom(pOther.mIsPhantom),
	mA(pOther.mA),
	mB(pOther.mB),
	mC(pOther.mC),
	mD(pOther.mD)
{
}

PathfindingLinkset::~PathfindingLinkset()
{
}

PathfindingLinkset& PathfindingLinkset::operator =(const PathfindingLinkset& pOther)
{
	mUUID = pOther.mUUID;
	mName = pOther.mName;
	mDescription = pOther.mDescription;
	mLandImpact = pOther.mLandImpact;
	mLocation = pOther.mLocation;
	mIsFixed = pOther.mIsFixed;
	mIsWalkable = pOther.mIsWalkable;
	mIsPhantom = pOther.mIsPhantom;
	mA = pOther.mA;
	mB = pOther.mB;
	mC = pOther.mC;
	mD = pOther.mD;

	return *this;
}

const LLUUID& PathfindingLinkset::getUUID() const
{
	return mUUID;
}

const std::string& PathfindingLinkset::getName() const
{
	return mName;
}

const std::string& PathfindingLinkset::getDescription() const
{
	return mDescription;
}

U32 PathfindingLinkset::getLandImpact() const
{
	return mLandImpact;
}

const LLVector3& PathfindingLinkset::getPositionAgent() const
{
	return mLocation;
}

BOOL PathfindingLinkset::isFixed() const
{
	return mIsFixed;
}

void PathfindingLinkset::setFixed(BOOL pIsFixed)
{
	mIsFixed = pIsFixed;
}

BOOL PathfindingLinkset::isWalkable() const
{
	return mIsWalkable;
}

void PathfindingLinkset::setWalkable(BOOL pIsWalkable)
{
	mIsWalkable = pIsWalkable;
}

BOOL PathfindingLinkset::isPhantom() const
{
	return mIsPhantom;
}

void PathfindingLinkset::setPhantom(BOOL pIsPhantom)
{
	mIsPhantom = pIsPhantom;
}

F32 PathfindingLinkset::getA() const
{
	return mA;
}

void PathfindingLinkset::setA(F32 pA)
{
	mA = pA;
}

F32 PathfindingLinkset::getB() const
{
	return mB;
}

void PathfindingLinkset::setB(F32 pB)
{
	mB = pB;
}

F32 PathfindingLinkset::getC() const
{
	return mC;
}

void PathfindingLinkset::setC(F32 pC)
{
	mC = pC;
}

F32 PathfindingLinkset::getD() const
{
	return mD;
}

void PathfindingLinkset::setD(F32 pD)
{
	mD = pD;
}

//---------------------------------------------------------------------------
// FilterString
//---------------------------------------------------------------------------

FilterString::FilterString()
	: mFilter(),
	mUpperFilter()
{
}

FilterString::FilterString(const std::string& pFilter)
	: mFilter(pFilter),
	mUpperFilter()
{
	LLStringUtil::trim(mFilter);
	mUpperFilter = mFilter;
	if (!mUpperFilter.empty())
	{
		LLStringUtil::toUpper(mUpperFilter);
	}
}

FilterString::FilterString(const FilterString& pOther)
	: mFilter(pOther.mFilter),
	mUpperFilter(pOther.mUpperFilter)
{
}

FilterString::~FilterString()
{
}

const std::string& FilterString::get() const
{
	return mFilter;
}

bool FilterString::set(const std::string& pFilter)
{
	std::string newFilter(pFilter);
	LLStringUtil::trim(newFilter);
	bool didFilterChange = (mFilter.compare(newFilter) != 0);
	if (didFilterChange)
	{
		mFilter = newFilter;
		mUpperFilter = newFilter;
		LLStringUtil::toUpper(mUpperFilter);
	}

	return didFilterChange;
}

void FilterString::clear()
{
	mFilter.clear();
	mUpperFilter.clear();
}

bool FilterString::isActive() const
{
	return !mFilter.empty();
}

bool FilterString::doesMatch(const std::string& pTestString) const
{
	bool doesMatch = true;

	if (isActive())
	{
		std::string upperTestString(pTestString);
		LLStringUtil::toUpper(upperTestString);
		doesMatch = (upperTestString.find(mUpperFilter) != std::string::npos);
	}

	return doesMatch;
}

//---------------------------------------------------------------------------
// PathfindingLinksets
//---------------------------------------------------------------------------

PathfindingLinksets::PathfindingLinksets()
	: mAllLinksets(),
	mFilteredLinksets(),
	mIsFiltersDirty(false),
	mNameFilter(),
	mDescriptionFilter(),
	mIsFixedFilter(false),
	mIsWalkableFilter(false)
{
}

PathfindingLinksets::PathfindingLinksets(const LLSD& pNavmeshData)
	: mAllLinksets(),
	mFilteredLinksets(),
	mIsFiltersDirty(false),
	mNameFilter(),
	mDescriptionFilter(),
	mIsFixedFilter(false),
	mIsWalkableFilter(false)
{
	parseNavmeshData(pNavmeshData);
}

PathfindingLinksets::PathfindingLinksets(const PathfindingLinksets& pOther)
	: mAllLinksets(pOther.mAllLinksets),
	mFilteredLinksets(pOther.mFilteredLinksets),
	mIsFiltersDirty(pOther.mIsFiltersDirty),
	mNameFilter(pOther.mNameFilter),
	mDescriptionFilter(pOther.mDescriptionFilter),
	mIsFixedFilter(pOther.mIsFixedFilter),
	mIsWalkableFilter(pOther.mIsWalkableFilter)
{
}

PathfindingLinksets::~PathfindingLinksets()
{
	clearLinksets();
}

void PathfindingLinksets::parseNavmeshData(const LLSD& pNavmeshData)
{
	clearLinksets();

	for (LLSD::map_const_iterator linksetIter = pNavmeshData.beginMap();
		linksetIter != pNavmeshData.endMap(); ++linksetIter)
	{
		const std::string& uuid(linksetIter->first);
		const LLSD& linksetData = linksetIter->second;
		PathfindingLinkset linkset(uuid, linksetData);

		mAllLinksets.insert(std::pair<std::string, PathfindingLinkset>(uuid, linkset));
	}

	mIsFiltersDirty = true;
}

void PathfindingLinksets::clearLinksets()
{
	mAllLinksets.clear();
	mFilteredLinksets.clear();
	mIsFiltersDirty = false;
}

const PathfindingLinksets::PathfindingLinksetMap& PathfindingLinksets::getAllLinksets() const
{
	return mAllLinksets;
}

const PathfindingLinksets::PathfindingLinksetMap& PathfindingLinksets::getFilteredLinksets()
{
	if (!isFiltersActive())
	{
		return mAllLinksets;
	}
	else
	{
		applyFilters();
		return mFilteredLinksets;
	}
}

BOOL PathfindingLinksets::isFiltersActive() const
{
	return (mNameFilter.isActive() || mDescriptionFilter.isActive() || mIsFixedFilter || mIsWalkableFilter);
}

void PathfindingLinksets::setNameFilter(const std::string& pNameFilter)
{
	mIsFiltersDirty = (mNameFilter.set(pNameFilter) || mIsFiltersDirty);
}

const std::string& PathfindingLinksets::getNameFilter() const
{
	return mNameFilter.get();
}

void PathfindingLinksets::setDescriptionFilter(const std::string& pDescriptionFilter)
{
	mIsFiltersDirty = (mDescriptionFilter.set(pDescriptionFilter) || mIsFiltersDirty);
}

const std::string& PathfindingLinksets::getDescriptionFilter() const
{
	return mDescriptionFilter.get();
}

void PathfindingLinksets::setFixedFilter(BOOL pFixedFilter)
{
	mIsFiltersDirty = (mIsFiltersDirty || (mIsFixedFilter == pFixedFilter));
	mIsFixedFilter = pFixedFilter;
}

BOOL PathfindingLinksets::isFixedFilter() const
{
	return mIsFixedFilter;
}

void PathfindingLinksets::setWalkableFilter(BOOL pWalkableFilter)
{
	mIsFiltersDirty = (mIsFiltersDirty || (mIsWalkableFilter == pWalkableFilter));
	mIsWalkableFilter = pWalkableFilter;
}

BOOL PathfindingLinksets::isWalkableFilter() const
{
	return mIsWalkableFilter;
}

void PathfindingLinksets::clearFilters()
{
	mNameFilter.clear();
	mDescriptionFilter.clear();
	mIsFixedFilter = false;
	mIsWalkableFilter = false;
	mIsFiltersDirty = false;
}

void PathfindingLinksets::applyFilters()
{
	mFilteredLinksets.clear();

	for (PathfindingLinksetMap::const_iterator linksetIter = mAllLinksets.begin();
		linksetIter != mAllLinksets.end(); ++linksetIter)
	{
		const std::string& uuid(linksetIter->first);
		const PathfindingLinkset& linkset(linksetIter->second);
		if (doesMatchFilters(linkset))
		{
			mFilteredLinksets.insert(std::pair<std::string, PathfindingLinkset>(uuid, linkset));
		}
	}

	mIsFiltersDirty = false;
}

BOOL PathfindingLinksets::doesMatchFilters(const PathfindingLinkset& pLinkset) const
{
	return ((!mIsFixedFilter || pLinkset.isFixed()) &&
			(!mIsWalkableFilter || pLinkset.isWalkable()) &&
			(!mNameFilter.isActive() || mNameFilter.doesMatch(pLinkset.getName())) &&
			(!mDescriptionFilter.isActive() || mDescriptionFilter.doesMatch(pLinkset.getDescription())));
}

//---------------------------------------------------------------------------
// LLFloaterPathfindingLinksets
//---------------------------------------------------------------------------

BOOL LLFloaterPathfindingLinksets::postBuild()
{
	childSetAction("apply_filters", boost::bind(&LLFloaterPathfindingLinksets::onApplyFiltersClicked, this));
	childSetAction("clear_filters", boost::bind(&LLFloaterPathfindingLinksets::onClearFiltersClicked, this));
	childSetAction("refresh_linksets_list", boost::bind(&LLFloaterPathfindingLinksets::onRefreshLinksetsClicked, this));
	childSetAction("select_all_linksets", boost::bind(&LLFloaterPathfindingLinksets::onSelectAllLinksetsClicked, this));
	childSetAction("select_none_linksets", boost::bind(&LLFloaterPathfindingLinksets::onSelectNoneLinksetsClicked, this));

	mLinksetsScrollList = findChild<LLScrollListCtrl>("pathfinding_linksets");
	llassert(mLinksetsScrollList != NULL);
	mLinksetsScrollList->setCommitCallback(boost::bind(&LLFloaterPathfindingLinksets::onLinksetsSelectionChange, this));
	mLinksetsScrollList->setCommitOnSelectionChange(true);

	mLinksetsStatus = findChild<LLTextBase>("linksets_status");
	llassert(mLinksetsStatus != NULL);

	mFilterByName = findChild<LLLineEditor>("filter_by_name");
	llassert(mFilterByName != NULL);
	mFilterByName->setCommitCallback(boost::bind(&LLFloaterPathfindingLinksets::onApplyFiltersClicked, this));
	mFilterByName->setSelectAllonFocusReceived(true);
	mFilterByName->setCommitOnFocusLost(true);

	mFilterByDescription = findChild<LLLineEditor>("filter_by_description");
	llassert(mFilterByDescription != NULL);
	mFilterByDescription->setCommitCallback(boost::bind(&LLFloaterPathfindingLinksets::onApplyFiltersClicked, this));
	mFilterByDescription->setSelectAllonFocusReceived(true);
	mFilterByDescription->setCommitOnFocusLost(true);

	mFilterByFixed = findChild<LLCheckBoxCtrl>("filter_by_fixed");
	llassert(mFilterByFixed != NULL);
	mFilterByFixed->setCommitCallback(boost::bind(&LLFloaterPathfindingLinksets::onApplyFiltersClicked, this));

	mFilterByWalkable = findChild<LLCheckBoxCtrl>("filter_by_walkable");
	llassert(mFilterByWalkable != NULL);
	mFilterByWalkable->setCommitCallback(boost::bind(&LLFloaterPathfindingLinksets::onApplyFiltersClicked, this));

	mEditFixed = findChild<LLCheckBoxCtrl>("edit_fixed_value");
	llassert(mEditFixed != NULL);

	mEditWalkable = findChild<LLCheckBoxCtrl>("edit_walkable_value");
	llassert(mEditWalkable != NULL);

	mEditPhantom = findChild<LLCheckBoxCtrl>("edit_phantom_value");
	llassert(mEditPhantom != NULL);

	mLabelWalkabilityCoefficients = findChild<LLTextBase>("walkability_coefficients_label");
	llassert(mLabelWalkabilityCoefficients != NULL);

	mLabelEditA = findChild<LLTextBase>("edit_a_label");
	llassert(mLabelEditA != NULL);

	mLabelEditB = findChild<LLTextBase>("edit_b_label");
	llassert(mLabelEditB != NULL);

	mLabelEditC = findChild<LLTextBase>("edit_c_label");
	llassert(mLabelEditC != NULL);

	mLabelEditD = findChild<LLTextBase>("edit_d_label");
	llassert(mLabelEditD != NULL);

	mEditA = findChild<LLLineEditor>("edit_a_value");
	llassert(mEditA != NULL);
	mEditA->setPrevalidate(LLTextValidate::validateFloat);

	mEditB = findChild<LLLineEditor>("edit_b_value");
	llassert(mEditB != NULL);
	mEditB->setPrevalidate(LLTextValidate::validateFloat);

	mEditC = findChild<LLLineEditor>("edit_c_value");
	llassert(mEditC != NULL);
	mEditC->setPrevalidate(LLTextValidate::validateFloat);

	mEditD = findChild<LLLineEditor>("edit_d_value");
	llassert(mEditD != NULL);
	mEditD->setPrevalidate(LLTextValidate::validateFloat);

	mApplyEdits = findChild<LLButton>("apply_edit_values");
	llassert(mApplyEdits != NULL);
	mApplyEdits->setCommitCallback(boost::bind(&LLFloaterPathfindingLinksets::onApplyChangesClicked, this));

	setEnableEditFields(false);
	setFetchState(kFetchInitial);

	return LLFloater::postBuild();
}

void LLFloaterPathfindingLinksets::onOpen(const LLSD& pKey)
{
	sendNavmeshDataGetRequest();
}

void LLFloaterPathfindingLinksets::openLinksetsEditor()
{
	LLFloaterReg::toggleInstanceOrBringToFront("pathfinding_linksets");
}

LLFloaterPathfindingLinksets::EFetchState LLFloaterPathfindingLinksets::getFetchState() const
{
	return mFetchState;
}

BOOL LLFloaterPathfindingLinksets::isFetchInProgress() const
{
	BOOL retVal;
	switch (getFetchState())
	{
	case kFetchStarting :
	case kFetchRequestSent :
	case kFetchRequestSent_MultiRequested :
	case kFetchReceived :
		retVal = true;
		break;
	default :
		retVal = false;
		break;
	}

	return retVal;
}

LLFloaterPathfindingLinksets::LLFloaterPathfindingLinksets(const LLSD& pSeed)
	: LLFloater(pSeed),
	mPathfindingLinksets(),
	mFetchState(kFetchInitial),
	mLinksetsScrollList(NULL),
	mLinksetsStatus(NULL),
	mFilterByName(NULL),
	mFilterByDescription(NULL),
	mFilterByFixed(NULL),
	mFilterByWalkable(NULL),
	mEditFixed(NULL),
	mEditWalkable(NULL),
	mEditPhantom(NULL),
	mLabelWalkabilityCoefficients(NULL),
	mLabelEditA(NULL),
	mLabelEditB(NULL),
	mLabelEditC(NULL),
	mLabelEditD(NULL),
	mEditA(NULL),
	mEditB(NULL),
	mEditC(NULL),
	mEditD(NULL),
	mApplyEdits(NULL)
{
}

LLFloaterPathfindingLinksets::~LLFloaterPathfindingLinksets()
{
}

void LLFloaterPathfindingLinksets::sendNavmeshDataGetRequest()
{
#ifdef XXX_STINSON_USE_FAKE_DATA
	LLSD allData;

	const std::string firstUUID(gObjectList.getObject(0)->getID().asString());
	LLSD firstData;
	firstData["name"] = "Curabitur malesuada";
	firstData["description"] = "Accusam nominavi contentiones per ad";
	firstData["landimpact"] = S32(57);
	firstData["fixed"] = bool(false);
	firstData["walkable"] = bool(false);
	firstData["phantom"] = bool(false);
	firstData["A"] = F32(0.375f);
	firstData["B"] = F32(0.078f);
	firstData["C"] = F32(0.986f);
	firstData["D"] = F32(0.0f);
	LLVector3 firstLocation(135.0f, 57.0f, 2.0f);
	firstData["position"] = firstLocation.getValue();

	const std::string secondUUID(gObjectList.getObject(1)->getID().asString());
	LLSD secondData;
	secondData["name"] = "At tota";
	secondData["description"] = "His ad placerat tincidun";
	secondData["landimpact"] = S32(2);
	secondData["fixed"] = bool(false);
	secondData["walkable"] = bool(false);
	secondData["phantom"] = bool(true);
	secondData["A"] = F32(0.305f);
	secondData["B"] = F32(0.708f);
	secondData["C"] = F32(1.0f);
	secondData["D"] = F32(0.001f);
	LLVector3 secondLocation(15.0f, 157.0f, 22.0f);
	secondData["position"] = secondLocation.getValue();

	const std::string thirdUUID(gObjectList.getObject(2)->getID().asString());
	LLSD thirdData;
	thirdData["name"] = "No soleat";
	thirdData["description"] = "";
	thirdData["landimpact"] = S32(200);
	thirdData["fixed"] = bool(false);
	thirdData["walkable"] = bool(true);
	thirdData["phantom"] = bool(false);
	thirdData["A"] = F32(0.585f);
	thirdData["B"] = F32(0.08f);
	thirdData["C"] = F32(0.02f);
	thirdData["D"] = F32(0.155f);
	LLVector3 thirdLocation(577.0f, 14.0f, -14.5f);
	thirdData["position"] = thirdLocation.getValue();

	const std::string fourthUUID(gObjectList.getObject(3)->getID().asString());
	LLSD fourthData;
	fourthData["name"] = "Paulo tritani bonorum";
	fourthData["description"] = "Vis verear impetus";
	fourthData["landimpact"] = S32(56);
	fourthData["fixed"] = bool(false);
	fourthData["walkable"] = bool(true);
	fourthData["phantom"] = bool(true);
	fourthData["A"] = F32(1.0f);
	fourthData["B"] = F32(1.0f);
	fourthData["C"] = F32(0.0f);
	fourthData["D"] = F32(0.0f);
	LLVector3 fourthLocation(215.0f, 57.0f, 5.0f);
	fourthData["position"] = fourthLocation.getValue();

	const std::string fifthUUID(gObjectList.getObject(4)->getID().asString());
	LLSD fifthData;
	fifthData["name"] = "Curabitur malesuada";
	fifthData["description"] = "Reque possit philosophia";
	fifthData["landimpact"] = S32(20);
	fifthData["fixed"] = bool(true);
	fifthData["walkable"] = bool(false);
	fifthData["phantom"] = bool(false);
	fifthData["A"] = F32(0.375f);
	fifthData["B"] = F32(0.078f);
	fifthData["C"] = F32(0.986f);
	fifthData["D"] = F32(0.0f);
	LLVector3 fifthLocation(135.0f, 57.0f, 2.0f);
	fifthData["position"] = fifthLocation.getValue();

	const std::string sixthUUID(gObjectList.getObject(5)->getID().asString());
	LLSD sixthData;
	sixthData["name"] = "At tota";
	sixthData["description"] = "Usu no aliquid dignissim";
	sixthData["landimpact"] = S32(257);
	sixthData["fixed"] = bool(true);
	sixthData["walkable"] = bool(false);
	sixthData["phantom"] = bool(true);
	sixthData["A"] = F32(0.0f);
	sixthData["B"] = F32(0.0f);
	sixthData["C"] = F32(1.0f);
	sixthData["D"] = F32(0.001f);
	LLVector3 sixthLocation(315.0f, 57.0f, 12.0f);
	sixthData["position"] = sixthLocation.getValue();

	const std::string seventhUUID(gObjectList.getObject(6)->getID().asString());
	LLSD seventhData;
	seventhData["name"] = "No soleat";
	seventhData["description"] = "honestatis";
	seventhData["landimpact"] = S32(25);
	seventhData["fixed"] = bool(true);
	seventhData["walkable"] = bool(true);
	seventhData["phantom"] = bool(false);
	seventhData["A"] = F32(0.554f);
	seventhData["B"] = F32(0.2712f);
	seventhData["C"] = F32(0.325f);
	seventhData["D"] = F32(0.155f);
	LLVector3 seventhLocation(7.0f, 0.0f, 0.0f);
	seventhData["position"] = seventhLocation.getValue();

	const std::string eigthUUID(gObjectList.getObject(7)->getID().asString());
	LLSD eigthData;
	eigthData["name"] = "Sea te aliquam";
	eigthData["description"] = "";
	eigthData["landimpact"] = S32(17);
	eigthData["fixed"] = bool(true);
	eigthData["walkable"] = bool(true);
	eigthData["phantom"] = bool(true);
	eigthData["A"] = F32(0.1f);
	eigthData["B"] = F32(0.1f);
	eigthData["C"] = F32(0.5f);
	eigthData["D"] = F32(0.6f);
	LLVector3 eigthLocation(25.0f, 7.0f, 5.0f);
	eigthData["position"] = eigthLocation.getValue();

	allData[firstUUID] = firstData;
	allData[secondUUID] = secondData;
	allData[thirdUUID] = thirdData;
	allData[fourthUUID] = fourthData;
	allData[fifthUUID] = fifthData;
	allData[sixthUUID] = sixthData;
	allData[seventhUUID] = seventhData;
	allData[eigthUUID] = eigthData;

	handleNavmeshDataGetReply(allData);

#else // XXX_STINSON_USE_FAKE_DATA
	if (isFetchInProgress())
	{
		if (getFetchState() == kFetchRequestSent)
		{
			setFetchState(kFetchRequestSent_MultiRequested);
		}
	}
	else
	{
		setFetchState(kFetchStarting);
		mPathfindingLinksets.clearLinksets();
		updateLinksetsList();

		std::string navmeshDataURL = this->getCapabilityURL();
		if (navmeshDataURL.empty())
		{
			setFetchState(kFetchComplete);
			llwarns << "cannot query navmesh data from current region '" << getRegionName() << "'" << llendl;
		}
		else
		{
			setFetchState(kFetchRequestSent);
			LLHTTPClient::get(navmeshDataURL, new NavmeshDataGetResponder(navmeshDataURL, this));
		}
	}
#endif
}

void LLFloaterPathfindingLinksets::sendNavmeshDataPutRequest(const LLSD& pPostData)
{
	std::string navmeshDataURL = this->getCapabilityURL();
	if (navmeshDataURL.empty())
	{
		llwarns << "cannot put navmesh data for current region '" << getRegionName() << "'" << llendl;
	}
	else
	{
		LLHTTPClient::put(navmeshDataURL, pPostData, new NavmeshDataPutResponder(navmeshDataURL, this));
	}
}

void LLFloaterPathfindingLinksets::handleNavmeshDataGetReply(const LLSD& pNavmeshData)
{
	setFetchState(kFetchReceived);
	mPathfindingLinksets.parseNavmeshData(pNavmeshData);
	updateLinksetsList();
	setFetchState(kFetchComplete);
}

void LLFloaterPathfindingLinksets::handleNavmeshDataGetError(const std::string& pURL, const std::string& pErrorReason)
{
	setFetchState(kFetchError);
	mPathfindingLinksets.clearLinksets();
	updateLinksetsList();
	llwarns << "Error fetching navmesh data from URL '" << pURL << "' because " << pErrorReason << llendl;
}

void LLFloaterPathfindingLinksets::handleNavmeshDataPutReply(const LLSD& pModifiedData)
{
	setFetchState(kFetchReceived);
	mPathfindingLinksets.parseNavmeshData(pModifiedData);
	updateLinksetsList();
	setFetchState(kFetchComplete);
}

void LLFloaterPathfindingLinksets::handleNavmeshDataPutError(const std::string& pURL, const std::string& pErrorReason)
{
	setFetchState(kFetchError);
	mPathfindingLinksets.clearLinksets();
	updateLinksetsList();
	llwarns << "Error putting navmesh data to URL '" << pURL << "' because " << pErrorReason << llendl;
}

std::string LLFloaterPathfindingLinksets::getRegionName() const
{
	std::string regionName("");

	LLViewerRegion* region = gAgent.getRegion();
	if (region != NULL)
	{
		regionName = region->getName();
	}

	return regionName;
}

std::string LLFloaterPathfindingLinksets::getCapabilityURL() const
{
	std::string navmeshDataURL("");

	LLViewerRegion* region = gAgent.getRegion();
	if (region != NULL)
	{
		navmeshDataURL = region->getCapability("ObjectNavmesh");
	}

	return navmeshDataURL;
}

void LLFloaterPathfindingLinksets::setFetchState(EFetchState pFetchState)
{
	mFetchState = pFetchState;
	updateLinksetsStatusMessage();
}

void LLFloaterPathfindingLinksets::onApplyFiltersClicked()
{
	applyFilters();
}

void LLFloaterPathfindingLinksets::onClearFiltersClicked()
{
	clearFilters();
}

void LLFloaterPathfindingLinksets::onLinksetsSelectionChange()
{
	updateLinksetsStatusMessage();
	updateEditFields();
}

void LLFloaterPathfindingLinksets::onRefreshLinksetsClicked()
{
	sendNavmeshDataGetRequest();
}

void LLFloaterPathfindingLinksets::onSelectAllLinksetsClicked()
{
	selectAllLinksets();
}

void LLFloaterPathfindingLinksets::onSelectNoneLinksetsClicked()
{
	selectNoneLinksets();
}

void LLFloaterPathfindingLinksets::onApplyChangesClicked()
{
	applyEditFields();
}

void LLFloaterPathfindingLinksets::applyFilters()
{
	mPathfindingLinksets.setNameFilter(mFilterByName->getText());
	mPathfindingLinksets.setDescriptionFilter(mFilterByDescription->getText());
	mPathfindingLinksets.setFixedFilter(mFilterByFixed->get());
	mPathfindingLinksets.setWalkableFilter(mFilterByWalkable->get());
	updateLinksetsList();
}

void LLFloaterPathfindingLinksets::clearFilters()
{
	mPathfindingLinksets.clearFilters();
	mFilterByName->setText(LLStringExplicit(mPathfindingLinksets.getNameFilter()));
	mFilterByDescription->setText(LLStringExplicit(mPathfindingLinksets.getDescriptionFilter()));
	mFilterByFixed->set(mPathfindingLinksets.isFixedFilter());
	mFilterByWalkable->set(mPathfindingLinksets.isWalkableFilter());
	updateLinksetsList();
}

void LLFloaterPathfindingLinksets::updateLinksetsList()
{
	std::vector<LLScrollListItem*> selectedItems = mLinksetsScrollList->getAllSelected();
	int numSelectedItems = selectedItems.size();
	uuid_vec_t selectedUUIDs;
	if (numSelectedItems > 0)
	{
		selectedUUIDs.reserve(selectedItems.size());
		for (std::vector<LLScrollListItem*>::const_iterator itemIter = selectedItems.begin();
			itemIter != selectedItems.end(); ++itemIter)
		{
			const LLScrollListItem *listItem = *itemIter;
			selectedUUIDs.push_back(listItem->getUUID());
		}
	}

	mLinksetsScrollList->deleteAllItems();
	updateLinksetsStatusMessage();

	const LLVector3& avatarPosition = gAgent.getPositionAgent();
	const PathfindingLinksets::PathfindingLinksetMap& linksetMap = mPathfindingLinksets.getFilteredLinksets();

	for (PathfindingLinksets::PathfindingLinksetMap::const_iterator linksetIter = linksetMap.begin();
		linksetIter != linksetMap.end(); ++linksetIter)
	{
		const PathfindingLinkset& linkset(linksetIter->second);

		LLSD columns;

		columns[0]["column"] = "name";
		columns[0]["value"] = linkset.getName();
		columns[0]["font"] = "SANSSERIF";

		columns[1]["column"] = "description";
		columns[1]["value"] = linkset.getDescription();
		columns[1]["font"] = "SANSSERIF";

		columns[2]["column"] = "land_impact";
		columns[2]["value"] = llformat("%1d", linkset.getLandImpact());
		columns[2]["font"] = "SANSSERIF";

		columns[3]["column"] = "dist_from_you";
		columns[3]["value"] = llformat("%1.0f m", dist_vec(avatarPosition, linkset.getPositionAgent()));
		columns[3]["font"] = "SANSSERIF";

		columns[4]["column"] = "is_fixed";
		columns[4]["value"] = getString(linkset.isFixed() ? "linkset_is_fixed" : "linkset_is_not_fixed");
		columns[4]["font"] = "SANSSERIF";

		columns[5]["column"] = "is_walkable";
		columns[5]["value"] = getString(linkset.isWalkable() ? "linkset_is_walkable" : "linkset_is_not_walkable");
		columns[5]["font"] = "SANSSERIF";

		columns[6]["column"] = "is_phantom";
		columns[6]["value"] = getString(linkset.isPhantom() ? "linkset_is_phantom" : "linkset_is_not_phantom");
		columns[6]["font"] = "SANSSERIF";

		columns[7]["column"] = "a_percent";
		columns[7]["value"] = llformat("%2.0f", linkset.getA());
		columns[7]["font"] = "SANSSERIF";

		columns[8]["column"] = "b_percent";
		columns[8]["value"] = llformat("%2.0f", linkset.getB());
		columns[8]["font"] = "SANSSERIF";

		columns[9]["column"] = "c_percent";
		columns[9]["value"] = llformat("%2.0f", linkset.getC());
		columns[9]["font"] = "SANSSERIF";

		columns[10]["column"] = "d_percent";
		columns[10]["value"] = llformat("%2.0f", linkset.getD());
		columns[10]["font"] = "SANSSERIF";

		LLSD element;
		element["id"] = linkset.getUUID().asString();
		element["column"] = columns;

		mLinksetsScrollList->addElement(element);
	}

	mLinksetsScrollList->selectMultiple(selectedUUIDs);
	updateLinksetsStatusMessage();
}

void LLFloaterPathfindingLinksets::selectAllLinksets()
{
	mLinksetsScrollList->selectAll();
}

void LLFloaterPathfindingLinksets::selectNoneLinksets()
{
	mLinksetsScrollList->deselectAllItems();
}

void LLFloaterPathfindingLinksets::updateLinksetsStatusMessage()
{
	static const LLColor4 warningColor = LLUIColorTable::instance().getColor("DrYellow");

	std::string statusText("");
	LLStyle::Params styleParams;

	switch (getFetchState())
	{
	case kFetchStarting :
		statusText = getString("linksets_fetching_starting");
		break;
	case kFetchRequestSent :
		statusText = getString("linksets_fetching_inprogress");
		break;
	case kFetchRequestSent_MultiRequested :
		statusText = getString("linksets_fetching_inprogress_multi_request");
		break;
	case kFetchReceived :
		statusText = getString("linksets_fetching_received");
		break;
	case kFetchError :
		statusText = getString("linksets_fetching_error");
		styleParams.color = warningColor;
		break;
	case kFetchComplete :
		if (mLinksetsScrollList->isEmpty())
		{
			statusText = getString("linksets_fetching_done_none_found");
		}
		else
		{
			S32 numItems = mLinksetsScrollList->getItemCount();
			S32 numSelectedItems = mLinksetsScrollList->getNumSelected();

			LLLocale locale(LLStringUtil::getLocale());
			std::string numItemsString;
			LLResMgr::getInstance()->getIntegerString(numItemsString, numItems);

			std::string numSelectedItemsString;
			LLResMgr::getInstance()->getIntegerString(numSelectedItemsString, numSelectedItems);

			LLStringUtil::format_map_t string_args;
			string_args["[NUM_SELECTED]"] = numSelectedItemsString;
			string_args["[NUM_TOTAL]"] = numItemsString;
			statusText = getString("linksets_fetching_done_available", string_args);
		}
		break;
	case kFetchInitial:
	default:
		statusText = getString("linksets_fetching_initial");
		break;
	}

	mLinksetsStatus->setText((LLStringExplicit)statusText, styleParams);
}

void LLFloaterPathfindingLinksets::updateEditFields()
{
	std::vector<LLScrollListItem*> selectedItems = mLinksetsScrollList->getAllSelected();
	if (selectedItems.empty())
	{
		mEditFixed->clear();
		mEditWalkable->clear();
		mEditPhantom->clear();
		mEditA->clear();
		mEditB->clear();
		mEditC->clear();
		mEditD->clear();

		setEnableEditFields(false);
	}
	else
	{
		LLScrollListItem *firstItem = selectedItems.front();

		const PathfindingLinksets::PathfindingLinksetMap &linksetsMap = mPathfindingLinksets.getAllLinksets();
		PathfindingLinksets::PathfindingLinksetMap::const_iterator linksetIter = linksetsMap.find(firstItem->getUUID().asString());
		const PathfindingLinkset &linkset(linksetIter->second);

		mEditFixed->set(linkset.isFixed());
		mEditWalkable->set(linkset.isWalkable());
		mEditPhantom->set(linkset.isPhantom());
		mEditA->setValue(LLSD(linkset.getA()));
		mEditB->setValue(LLSD(linkset.getB()));
		mEditC->setValue(LLSD(linkset.getC()));
		mEditD->setValue(LLSD(linkset.getD()));

		setEnableEditFields(true);
	}
}

void LLFloaterPathfindingLinksets::applyEditFields()
{
	std::vector<LLScrollListItem*> selectedItems = mLinksetsScrollList->getAllSelected();
	if (!selectedItems.empty())
	{
		BOOL isFixedBool = mEditFixed->getValue();
		BOOL isWalkableBool = mEditWalkable->getValue();
		BOOL isPhantomBool = mEditPhantom->getValue();
		const std::string &aString = mEditA->getText();
		const std::string &bString = mEditB->getText();
		const std::string &cString = mEditC->getText();
		const std::string &dString = mEditD->getText();
		F32 aValue = (F32)atof(aString.c_str());
		F32 bValue = (F32)atof(bString.c_str());
		F32 cValue = (F32)atof(cString.c_str());
		F32 dValue = (F32)atof(dString.c_str());

		LLSD isFixed = (bool)isFixedBool;
		LLSD isWalkable = (bool)isWalkableBool;
		LLSD isPhantom = (bool)isPhantomBool;
		LLSD a = aValue / 100.0f;
		LLSD b = bValue / 100.0f;
		LLSD c = cValue / 100.0f;
		LLSD d = dValue / 100.0f;

		const PathfindingLinksets::PathfindingLinksetMap &linksetsMap = mPathfindingLinksets.getAllLinksets();

		LLSD editData;
		for (std::vector<LLScrollListItem*>::const_iterator itemIter = selectedItems.begin();
			itemIter != selectedItems.end(); ++itemIter)
		{
			const LLScrollListItem *listItem = *itemIter;
			LLUUID uuid = listItem->getUUID();

			const PathfindingLinksets::PathfindingLinksetMap::const_iterator linksetIter = linksetsMap.find(uuid.asString());
			const PathfindingLinkset &linkset = linksetIter->second;

			LLSD itemData;
			if (linkset.isFixed() != isFixedBool)
			{
				itemData["permanent"] = isFixed;
			}
			if (linkset.isWalkable() != isWalkableBool)
			{
				itemData["walkable"] = isWalkable;
			}
			if (linkset.isPhantom() != isPhantomBool)
			{
				itemData["phantom"] = isPhantom;
			}
			if (linkset.getA() != aValue)
			{
				itemData["A"] = a;
			}
			if (linkset.getB() != bValue)
			{
				itemData["B"] = b;
			}
			if (linkset.getC() != cValue)
			{
				itemData["C"] = c;
			}
			if (linkset.getD() != dValue)
			{
				itemData["D"] = d;
			}

			editData[uuid.asString()] = itemData;
		}
		if (editData.isUndefined())
		{
			llwarns << "No PUT data specified" << llendl;
		}
		else
		{
			sendNavmeshDataPutRequest(editData);
		}
	}
}

void LLFloaterPathfindingLinksets::setEnableEditFields(BOOL pEnabled)
{
	mEditFixed->setEnabled(pEnabled);
	mEditWalkable->setEnabled(pEnabled);
	mEditPhantom->setEnabled(pEnabled);
	mLabelWalkabilityCoefficients->setEnabled(pEnabled);
	mLabelEditA->setEnabled(pEnabled);
	mLabelEditB->setEnabled(pEnabled);
	mLabelEditC->setEnabled(pEnabled);
	mLabelEditD->setEnabled(pEnabled);
	mEditA->setEnabled(pEnabled);
	mEditB->setEnabled(pEnabled);
	mEditC->setEnabled(pEnabled);
	mEditD->setEnabled(pEnabled);
	mApplyEdits->setEnabled(pEnabled);
}

//---------------------------------------------------------------------------
// NavmeshDataGetResponder
//---------------------------------------------------------------------------

NavmeshDataGetResponder::NavmeshDataGetResponder(const std::string& pNavmeshDataGetURL, LLFloaterPathfindingLinksets *pLinksetsFloater)
	: mNavmeshDataGetURL(pNavmeshDataGetURL),
	mLinksetsFloater(pLinksetsFloater)
{
}

NavmeshDataGetResponder::~NavmeshDataGetResponder()
{
	mLinksetsFloater = NULL;
}

void NavmeshDataGetResponder::result(const LLSD& pContent)
{
	mLinksetsFloater->handleNavmeshDataGetReply(pContent);
}

void NavmeshDataGetResponder::error(U32 status, const std::string& reason)
{
	mLinksetsFloater->handleNavmeshDataGetError(mNavmeshDataGetURL, reason);
}

//---------------------------------------------------------------------------
// NavmeshDataPutResponder
//---------------------------------------------------------------------------

NavmeshDataPutResponder::NavmeshDataPutResponder(const std::string& pNavmeshDataPutURL, LLFloaterPathfindingLinksets *pLinksetsFloater)
	: mNavmeshDataPutURL(pNavmeshDataPutURL),
	mLinksetsFloater(pLinksetsFloater)
{
}

NavmeshDataPutResponder::~NavmeshDataPutResponder()
{
	mLinksetsFloater = NULL;
}

void NavmeshDataPutResponder::result(const LLSD& pContent)
{
	mLinksetsFloater->handleNavmeshDataPutReply(pContent);
}

void NavmeshDataPutResponder::error(U32 status, const std::string& reason)
{
	mLinksetsFloater->handleNavmeshDataPutError(mNavmeshDataPutURL, reason);
}
