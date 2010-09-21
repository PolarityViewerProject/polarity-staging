/**
 * @file llnotificationofferhandler.cpp
 * @brief Provides set of utility methods for notifications processing.
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2000-2009, Linden Research, Inc.
 *
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 *
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 *
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 *
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */


#include "llviewerprecompiledheaders.h" // must be first include

#include "llnotificationhandler.h"
#include "llnotifications.h"
#include "llimview.h"
#include "llagent.h"
#include "llfloaterreg.h"
#include "llnearbychat.h"
#include "llimfloater.h"

using namespace LLNotificationsUI;

// static
std::list< std::set<std::string> > LLSysHandler::sExclusiveNotificationGroups;

// static
void LLSysHandler::init()
{
	std::set<std::string> online_offline_group;
	online_offline_group.insert("FriendOnline");
	online_offline_group.insert("FriendOffline");

	sExclusiveNotificationGroups.push_back(online_offline_group);
}

LLSysHandler::LLSysHandler()
{
	if(sExclusiveNotificationGroups.empty())
	{
		init();
	}
}

void LLSysHandler::removeExclusiveNotifications(const LLNotificationPtr& notif)
{
	LLScreenChannel* channel = dynamic_cast<LLScreenChannel *>(mChannel);
	if (channel == NULL)
	{
		return;
	}

	class ExclusiveMatcher: public LLScreenChannel::Matcher
	{
	public:
		ExclusiveMatcher(const std::set<std::string>& excl_group,
				const std::string& from_name) :
			mExclGroup(excl_group), mFromName(from_name)
		{
		}
		bool matches(const LLNotificationPtr notification) const
		{
			for (std::set<std::string>::const_iterator it = mExclGroup.begin(); it
					!= mExclGroup.end(); it++)
			{
				std::string from_name = LLHandlerUtil::getSubstitutionName(notification);
				if (notification->getName() == *it && from_name == mFromName)
				{
					return true;
				}
			}
			return false;
		}
	private:
		const std::set<std::string>& mExclGroup;
		const std::string& mFromName;
	};


	for (exclusive_notif_sets::iterator it = sExclusiveNotificationGroups.begin(); it
			!= sExclusiveNotificationGroups.end(); it++)
	{
		std::set<std::string> group = *it;
		std::set<std::string>::iterator g_it = group.find(notif->getName());
		if (g_it != group.end())
		{
			channel->killMatchedToasts(ExclusiveMatcher(group,
					LLHandlerUtil::getSubstitutionName(notif)));
		}
	}
}

const static std::string GRANTED_MODIFY_RIGHTS("GrantedModifyRights"),
		REVOKED_MODIFY_RIGHTS("RevokedModifyRights"), OBJECT_GIVE_ITEM(
				"ObjectGiveItem"), PAYMENT_RECIVED("PaymentRecived"),
						ADD_FRIEND_WITH_MESSAGE("AddFriendWithMessage"),
						USER_GIVE_ITEM("UserGiveItem"),
						INVENTORY_ACCEPTED("InventoryAccepted"),
						INVENTORY_DECLINED("InventoryDeclined"),
						OFFER_FRIENDSHIP("OfferFriendship"),
						FRIENDSHIP_ACCEPTED("FriendshipAccepted"),
						FRIENDSHIP_OFFERED("FriendshipOffered"),
						FRIENDSHIP_ACCEPTED_BYME("FriendshipAcceptedByMe"),
						FRIENDSHIP_DECLINED_BYME("FriendshipDeclinedByMe"),
						FRIEND_ONLINE("FriendOnline"), FRIEND_OFFLINE("FriendOffline"),
						SERVER_OBJECT_MESSAGE("ServerObjectMessage"),
						TELEPORT_OFFERED("TeleportOffered"),
						TELEPORT_OFFER_SENT("TeleportOfferSent"),
						IM_SYSTEM_MESSAGE_TIP("IMSystemMessageTip");


// static
bool LLHandlerUtil::canLogToIM(const LLNotificationPtr& notification)
{
	return GRANTED_MODIFY_RIGHTS == notification->getName()
			|| REVOKED_MODIFY_RIGHTS == notification->getName()
			|| PAYMENT_RECIVED == notification->getName()
			|| OFFER_FRIENDSHIP == notification->getName()
			|| FRIENDSHIP_OFFERED == notification->getName()
			|| FRIENDSHIP_ACCEPTED == notification->getName()
			|| FRIENDSHIP_ACCEPTED_BYME == notification->getName()
			|| FRIENDSHIP_DECLINED_BYME == notification->getName()
			|| SERVER_OBJECT_MESSAGE == notification->getName()
			|| INVENTORY_ACCEPTED == notification->getName()
			|| INVENTORY_DECLINED == notification->getName()
			|| USER_GIVE_ITEM == notification->getName()
			|| TELEPORT_OFFERED == notification->getName()
			|| TELEPORT_OFFER_SENT == notification->getName()
			|| IM_SYSTEM_MESSAGE_TIP == notification->getName();
}

// static
bool LLHandlerUtil::canLogToNearbyChat(const LLNotificationPtr& notification)
{
	return notification->getType() == "notifytip"
			&&  FRIEND_ONLINE != notification->getName()
			&& FRIEND_OFFLINE != notification->getName()
			&& INVENTORY_ACCEPTED != notification->getName()
			&& INVENTORY_DECLINED != notification->getName()
			&& IM_SYSTEM_MESSAGE_TIP != notification->getName();
}

// static
bool LLHandlerUtil::canSpawnIMSession(const LLNotificationPtr& notification)
{
	return OFFER_FRIENDSHIP == notification->getName()
			|| USER_GIVE_ITEM == notification->getName()
			|| TELEPORT_OFFERED == notification->getName();
}

// static
bool LLHandlerUtil::canAddNotifPanelToIM(const LLNotificationPtr& notification)
{
	return OFFER_FRIENDSHIP == notification->getName()
					|| USER_GIVE_ITEM == notification->getName()
					|| TELEPORT_OFFERED == notification->getName();
}

// static
bool LLHandlerUtil::isNotificationReusable(const LLNotificationPtr& notification)
{
	return OFFER_FRIENDSHIP == notification->getName()
		|| USER_GIVE_ITEM == notification->getName()
		|| TELEPORT_OFFERED == notification->getName();
}

// static
bool LLHandlerUtil::canSpawnSessionAndLogToIM(const LLNotificationPtr& notification)
{
	return canLogToIM(notification) && canSpawnIMSession(notification);
}

// static
bool LLHandlerUtil::canSpawnToast(const LLNotificationPtr& notification)
{
	if(INVENTORY_DECLINED == notification->getName() 
		|| INVENTORY_ACCEPTED == notification->getName())
	{
		// return false for inventory accepted/declined notifications if respective IM window is open (EXT-5909)
		return ! isIMFloaterOpened(notification);
	}

	if(FRIENDSHIP_ACCEPTED == notification->getName())
	{
		// don't show FRIENDSHIP_ACCEPTED if IM window is opened and focused - EXT-6441
		return ! isIMFloaterFocused(notification);
	}

	if(OFFER_FRIENDSHIP == notification->getName()
		|| USER_GIVE_ITEM == notification->getName()
		|| TELEPORT_OFFERED == notification->getName())
	{
		// When ANY offer arrives, show toast, unless IM window is already open - EXT-5904
		return ! isIMFloaterOpened(notification);
	}

	return true;
}

// static
LLIMFloater* LLHandlerUtil::findIMFloater(const LLNotificationPtr& notification)
{
	LLUUID from_id = notification->getPayload()["from_id"];
	LLUUID session_id = LLIMMgr::computeSessionID(IM_NOTHING_SPECIAL, from_id);
	return LLFloaterReg::findTypedInstance<LLIMFloater>("impanel", session_id);
}

// static
bool LLHandlerUtil::isIMFloaterOpened(const LLNotificationPtr& notification)
{
	bool res = false;

	LLIMFloater* im_floater = findIMFloater(notification);
	if (im_floater != NULL)
	{
		res = im_floater->getVisible() == TRUE;
	}

	return res;
}

bool LLHandlerUtil::isIMFloaterFocused(const LLNotificationPtr& notification)
{
	bool res = false;

	LLIMFloater* im_floater = findIMFloater(notification);
	if (im_floater != NULL)
	{
		res = im_floater->hasFocus() == TRUE;
	}

	return res;
}

// static
void LLHandlerUtil::logToIM(const EInstantMessage& session_type,
		const std::string& session_name, const std::string& from_name,
		const std::string& message, const LLUUID& session_owner_id,
		const LLUUID& from_id)
{
	std::string from = from_name;
	if (from_name.empty())
	{
		from = SYSTEM_FROM;
	}

	LLUUID session_id = LLIMMgr::computeSessionID(session_type,
			session_owner_id);
	LLIMModel::LLIMSession* session = LLIMModel::instance().findIMSession(
			session_id);
	if (session == NULL)
	{
		// replace interactive system message marker with correct from string value
		if (INTERACTIVE_SYSTEM_FROM == from_name)
		{
			from = SYSTEM_FROM;
		}
		LLIMModel::instance().logToFile(session_name, from, from_id, message);
	}
	else
	{
		// store active session id
		const LLUUID & active_session_id =
				LLIMModel::instance().getActiveSessionID();

		// set searched session as active to avoid IM toast popup
		LLIMModel::instance().setActiveSessionID(session_id);

		S32 unread = session->mNumUnread;
		S32 participant_unread = session->mParticipantUnreadMessageCount;
		LLIMModel::instance().addMessageSilently(session_id, from, from_id,
				message);
		// we shouldn't increment counters when logging, so restore them
		session->mNumUnread = unread;
		session->mParticipantUnreadMessageCount = participant_unread;

		// update IM floater messages
		updateIMFLoaterMesages(session_id);

		// restore active session id
		if (active_session_id.isNull())
		{
			LLIMModel::instance().resetActiveSessionID();
		}
		else
		{
			LLIMModel::instance().setActiveSessionID(active_session_id);
		}
	}
}

// static
void LLHandlerUtil::logToIMP2P(const LLNotificationPtr& notification)
{
	logToIMP2P(notification, false);
}

// static
void LLHandlerUtil::logToIMP2P(const LLNotificationPtr& notification, bool to_file_only)
{
	const std::string name = LLHandlerUtil::getSubstitutionName(notification);

	const std::string& session_name = notification->getPayload().has(
			"SESSION_NAME") ? notification->getPayload()["SESSION_NAME"].asString() : name;

	// don't create IM p2p session with objects, it's necessary condition to log
	if (notification->getName() != OBJECT_GIVE_ITEM)
	{
		LLUUID from_id = notification->getPayload()["from_id"];

		//there still appears a log history file with weird name " .txt"
		if (" " == session_name || "{waiting}" == session_name || "{nobody}" == session_name)
		{
			llwarning("Weird session name (" + session_name + ") for notification " + notification->getName(), 666)
		}

		if(to_file_only)
		{
			logToIM(IM_NOTHING_SPECIAL, session_name, "", notification->getMessage(),
					LLUUID(), LLUUID());
		}
		else
		{
			logToIM(IM_NOTHING_SPECIAL, session_name, INTERACTIVE_SYSTEM_FROM, notification->getMessage(),
					from_id, LLUUID());
		}
	}
}

// static
void LLHandlerUtil::logGroupNoticeToIMGroup(
		const LLNotificationPtr& notification)
{

	const LLSD& payload = notification->getPayload();
	LLGroupData groupData;
	if (!gAgent.getGroupData(payload["group_id"].asUUID(), groupData))
	{
		llwarns
						<< "Group notice for unkown group: "
								<< payload["group_id"].asUUID() << llendl;
		return;
	}

	const std::string group_name = groupData.mName;
	const std::string sender_name = payload["sender_name"].asString();

	// we can't retrieve sender id from group notice system message, so try to lookup it from cache
	LLUUID sender_id;
	gCacheName->getUUID(sender_name, sender_id);

	logToIM(IM_SESSION_GROUP_START, group_name, sender_name, payload["message"],
			payload["group_id"], sender_id);
}

// static
void LLHandlerUtil::logToNearbyChat(const LLNotificationPtr& notification, EChatSourceType type)
{
	LLNearbyChat* nearby_chat = LLFloaterReg::getTypedInstance<LLNearbyChat>("nearby_chat", LLSD());
	if(nearby_chat)
	{
		LLChat chat_msg(notification->getMessage());
		chat_msg.mSourceType = type;
		chat_msg.mFromName = SYSTEM_FROM;
		chat_msg.mFromID = LLUUID::null;
		nearby_chat->addMessage(chat_msg);
	}
}

// static
LLUUID LLHandlerUtil::spawnIMSession(const std::string& name, const LLUUID& from_id)
{
	LLUUID session_id = LLIMMgr::computeSessionID(IM_NOTHING_SPECIAL, from_id);

	LLIMModel::LLIMSession* session = LLIMModel::instance().findIMSession(
			session_id);
	if (session == NULL)
	{
		session_id = LLIMMgr::instance().addSession(name, IM_NOTHING_SPECIAL, from_id);
	}

	return session_id;
}

// static
std::string LLHandlerUtil::getSubstitutionName(const LLNotificationPtr& notification)
{
	std::string res = notification->getSubstitutions().has("NAME")
		? notification->getSubstitutions()["NAME"]
		: notification->getSubstitutions()["[NAME]"];
	if (res.empty())
	{
		LLUUID from_id = notification->getPayload()["FROM_ID"];

		//*TODO all keys everywhere should be made of the same case, there is a mix of keys in lower and upper cases
		if (from_id.isNull()) 
		{
			from_id = notification->getPayload()["from_id"];
		}
		if(!gCacheName->getFullName(from_id, res))
		{
			res = "";
		}
	}
	return res;
}

// static
void LLHandlerUtil::addNotifPanelToIM(const LLNotificationPtr& notification)
{
	const std::string name = LLHandlerUtil::getSubstitutionName(notification);
	LLUUID from_id = notification->getPayload()["from_id"];

	LLUUID session_id = spawnIMSession(name, from_id);
	// add offer to session
	LLIMModel::LLIMSession * session = LLIMModel::getInstance()->findIMSession(
			session_id);
	llassert_always(session != NULL);

	LLSD offer;
	offer["notification_id"] = notification->getID();
	offer["from"] = SYSTEM_FROM;
	offer["time"] = LLLogChat::timestamp(false);
	offer["index"] = (LLSD::Integer)session->mMsgs.size();
	session->mMsgs.push_front(offer);


	// update IM floater and counters
	LLSD arg;
	arg["session_id"] = session_id;
	arg["num_unread"] = ++(session->mNumUnread);
	arg["participant_unread"] = ++(session->mParticipantUnreadMessageCount);
	LLIMModel::getInstance()->mNewMsgSignal(arg);
}

// static
void LLHandlerUtil::updateIMFLoaterMesages(const LLUUID& session_id)
{
	LLIMFloater* im_floater = LLIMFloater::findInstance(session_id);
	if (im_floater != NULL && im_floater->getVisible())
	{
		im_floater->updateMessages();
	}
}

// static
void LLHandlerUtil::updateVisibleIMFLoaterMesages(const LLNotificationPtr& notification)
{
	const std::string name = LLHandlerUtil::getSubstitutionName(notification);
	LLUUID from_id = notification->getPayload()["from_id"];
	LLUUID session_id = spawnIMSession(name, from_id);

	updateIMFLoaterMesages(session_id);
}

// static
void LLHandlerUtil::decIMMesageCounter(const LLNotificationPtr& notification)
{
	const std::string name = LLHandlerUtil::getSubstitutionName(notification);
	LLUUID from_id = notification->getPayload()["from_id"];
	LLUUID session_id = LLIMMgr::computeSessionID(IM_NOTHING_SPECIAL, from_id);

	LLIMModel::LLIMSession * session = LLIMModel::getInstance()->findIMSession(
			session_id);

	if (session == NULL)
	{
		return;
	}

	LLSD arg;
	arg["session_id"] = session_id;
	session->mNumUnread--;
	arg["num_unread"] = session->mNumUnread;
	session->mParticipantUnreadMessageCount--;
	arg["participant_unread"] = session->mParticipantUnreadMessageCount;
	LLIMModel::getInstance()->mNewMsgSignal(arg);
}
