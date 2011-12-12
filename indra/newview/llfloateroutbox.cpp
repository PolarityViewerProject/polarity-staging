/** 
 * @file llfloateroutbox.cpp
 * @brief Implementation of the merchant outbox window
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

#include "llfloateroutbox.h"

#include "llfloaterreg.h"
#include "llfolderview.h"
#include "llinventorymodelbackgroundfetch.h"
#include "llinventoryobserver.h"
#include "llinventorypanel.h"
#include "llmarketplacefunctions.h"
#include "llnotificationhandler.h"
#include "llnotificationsutil.h"
#include "lltextbox.h"
#include "lltransientfloatermgr.h"
#include "lltrans.h"
#include "llviewernetwork.h"
#include "llwindowshade.h"


///----------------------------------------------------------------------------
/// LLOutboxNotification class
///----------------------------------------------------------------------------

bool LLNotificationsUI::LLOutboxNotification::processNotification(const LLSD& notify)
{
	LLNotificationPtr notification = LLNotifications::instance().find(notify["id"].asUUID());

	if (notification)
	{
		LLFloaterOutbox* outbox_floater = LLFloaterReg::getTypedInstance<LLFloaterOutbox>("outbox");

		outbox_floater->showNotification(notification);
	}

	return false;
}


///----------------------------------------------------------------------------
/// LLOutboxAddedObserver helper class
///----------------------------------------------------------------------------

class LLOutboxAddedObserver : public LLInventoryCategoryAddedObserver
{
public:
	LLOutboxAddedObserver(LLFloaterOutbox * outboxFloater)
		: LLInventoryCategoryAddedObserver()
		, mOutboxFloater(outboxFloater)
	{
	}
	
	void done()
	{
		for (cat_vec_t::iterator it = mAddedCategories.begin(); it != mAddedCategories.end(); ++it)
		{
			LLViewerInventoryCategory* added_category = *it;
			
			LLFolderType::EType added_category_type = added_category->getPreferredType();
			
			if (added_category_type == LLFolderType::FT_OUTBOX)
			{
				mOutboxFloater->setupOutbox(added_category->getUUID());
			}
		}
	}
	
private:
	LLFloaterOutbox *	mOutboxFloater;
};

///----------------------------------------------------------------------------
/// LLFloaterOutbox
///----------------------------------------------------------------------------

LLFloaterOutbox::LLFloaterOutbox(const LLSD& key)
	: LLFloater(key)
	, mCategoriesObserver(NULL)
	, mCategoryAddedObserver(NULL)
	, mOutboxId(LLUUID::null)
	, mOutboxInventoryPanel(NULL)
	, mOutboxItemCount(0)
	, mInventoryFolderCountText(NULL)
	, mInventoryImportInProgress(NULL)
	, mInventoryPlaceholder(NULL)
	, mInventoryText(NULL)
	, mInventoryTitle(NULL)
	, mImportButton(NULL)
	, mWindowShade(NULL)
{
}

LLFloaterOutbox::~LLFloaterOutbox()
{
	if (mCategoriesObserver && gInventory.containsObserver(mCategoriesObserver))
	{
		gInventory.removeObserver(mCategoriesObserver);
	}
	delete mCategoriesObserver;
	
	if (mCategoryAddedObserver && gInventory.containsObserver(mCategoryAddedObserver))
	{
		gInventory.removeObserver(mCategoryAddedObserver);
	}
	delete mCategoryAddedObserver;
}

BOOL LLFloaterOutbox::postBuild()
{
	mInventoryFolderCountText = getChild<LLTextBox>("outbox_folder_count");
	mInventoryImportInProgress = getChild<LLView>("import_progress_indicator");
	mInventoryPlaceholder = getChild<LLView>("outbox_inventory_placeholder_panel");
	mInventoryText = mInventoryPlaceholder->getChild<LLTextBox>("outbox_inventory_placeholder_text");
	mInventoryTitle = mInventoryPlaceholder->getChild<LLTextBox>("outbox_inventory_placeholder_title");
	
	mImportButton = getChild<LLButton>("outbox_import_btn");
	mImportButton->setCommitCallback(boost::bind(&LLFloaterOutbox::onImportButtonClicked, this));
	
	LLFocusableElement::setFocusReceivedCallback(boost::bind(&LLFloaterOutbox::onFocusReceived, this));

	return TRUE;
}

void LLFloaterOutbox::onClose(bool app_quitting)
{
	if (mWindowShade)
	{
		delete mWindowShade;

		mWindowShade = NULL;
	}
}

void LLFloaterOutbox::onOpen(const LLSD& key)
{
	//
	// Initialize the marketplace import API
	//

	LLMarketplaceInventoryImporter::getInstance()->initialize();
	LLMarketplaceInventoryImporter::getInstance()->setStatusChangedCallback(boost::bind(&LLFloaterOutbox::importStatusChanged, this, _1));
	LLMarketplaceInventoryImporter::getInstance()->setStatusReportCallback(boost::bind(&LLFloaterOutbox::importReportResults, this, _1, _2));
	
	//
	// Look for an outbox and set up the inventory API
	//
	
	if (mOutboxId.isNull())
	{
		const bool do_not_create_folder = false;
		const bool do_not_find_in_library = false;
		
		const LLUUID outbox_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_OUTBOX, do_not_create_folder, do_not_find_in_library);
		
		if (outbox_id.isNull())
		{
			// Observe category creation to catch outbox creation
			mCategoryAddedObserver = new LLOutboxAddedObserver(this);
			gInventory.addObserver(mCategoryAddedObserver);
		}
		else
		{
			setupOutbox(outbox_id);
		}
	}
	
	updateView();
	
	//
	// Trigger fetch of outbox contents
	//
	
	fetchOutboxContents();
}

void LLFloaterOutbox::onFocusReceived()
{
	fetchOutboxContents();
}

void LLFloaterOutbox::fetchOutboxContents()
{
	if (mOutboxId.notNull())
	{
		LLInventoryModelBackgroundFetch::instance().start(mOutboxId);
	}
}

void LLFloaterOutbox::setupOutbox(const LLUUID& outboxId)
{
	llassert(outboxId.notNull());
	llassert(mOutboxId.isNull());
	llassert(mCategoriesObserver == NULL);
	
	mOutboxId = outboxId;
	
	// No longer need to observe new category creation
	if (mCategoryAddedObserver && gInventory.containsObserver(mCategoryAddedObserver))
	{
		gInventory.removeObserver(mCategoryAddedObserver);
		delete mCategoryAddedObserver;
	}
	
	// Create observer for outbox modifications
	mCategoriesObserver = new LLInventoryCategoriesObserver();
	gInventory.addObserver(mCategoriesObserver);
	
	mCategoriesObserver->addCategory(mOutboxId, boost::bind(&LLFloaterOutbox::onOutboxChanged, this));
	
	//
	// Set up the outbox inventory view
	//
	
	mOutboxInventoryPanel = 
		LLUICtrlFactory::createFromFile<LLInventoryPanel>("panel_outbox_inventory.xml",
														  mInventoryPlaceholder->getParent(),
														  LLInventoryPanel::child_registry_t::instance());
	
	llassert(mOutboxInventoryPanel);
	
	// Reshape the inventory to the proper size
	LLRect inventory_placeholder_rect = mInventoryPlaceholder->getRect();
	mOutboxInventoryPanel->setShape(inventory_placeholder_rect);
	
	// Set the sort order newest to oldest
	mOutboxInventoryPanel->setSortOrder(LLInventoryFilter::SO_DATE);	
	mOutboxInventoryPanel->getFilter()->markDefault();
	
	fetchOutboxContents();
}

void LLFloaterOutbox::updateItemCount()
{
	S32 item_count = 0;

	if (mOutboxId.notNull())
	{
		LLInventoryModel::cat_array_t * cats;
		LLInventoryModel::item_array_t * items;
		gInventory.getDirectDescendentsOf(mOutboxId, cats, items);

		item_count = cats->count() + items->count();
	}
	
	mOutboxItemCount = item_count;

	switch (mOutboxItemCount)
	{
		case 0:	mInventoryFolderCountText->setText(getString("OutboxFolderCount0"));	break;
		case 1:	mInventoryFolderCountText->setText(getString("OutboxFolderCount1"));	break;
		default:
		{
			std::string item_count_str = llformat("%d", mOutboxItemCount);
			
			LLStringUtil::format_map_t args;
			args["[NUM]"] = item_count_str;
			
			mInventoryFolderCountText->setText(getString("OutboxFolderCountN", args));
			break;
		}
	}
	
	mImportButton->setEnabled(mOutboxItemCount > 0);
}

void LLFloaterOutbox::updateView()
{
	updateItemCount();

	if (mOutboxItemCount > 0)
	{
		mOutboxInventoryPanel->setVisible(TRUE);
		mInventoryPlaceholder->setVisible(FALSE);
	}
	else
	{
		mOutboxInventoryPanel->setVisible(FALSE);
		mInventoryPlaceholder->setVisible(TRUE);

		std::string outbox_text;
		std::string outbox_title;
		std::string outbox_tooltip;
		
		LLStringUtil::format_map_t subs = getMarketplaceStringSubstitutions();
		
		if (mOutboxId.notNull())
		{
			outbox_text = LLTrans::getString("InventoryOutboxNoItems", subs);
			outbox_title = LLTrans::getString("InventoryOutboxNoItemsTitle");
			outbox_tooltip = LLTrans::getString("InventoryOutboxNoItemsTooltip");
		}
		else
		{
			outbox_text = LLTrans::getString("InventoryOutboxNotMerchant", subs);
			outbox_title = LLTrans::getString("InventoryOutboxNotMerchantTitle");
			outbox_tooltip = LLTrans::getString("InventoryOutboxNotMerchantTooltip");
		}
		
		mInventoryText->setValue(outbox_text);
		mInventoryTitle->setValue(outbox_title);
		mInventoryPlaceholder->getParent()->setToolTip(outbox_tooltip);
	}
}

BOOL LLFloaterOutbox::handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
										EDragAndDropType cargo_type,
										void* cargo_data,
										EAcceptance* accept,
										std::string& tooltip_msg)
{
	// Pass drag and drop to this floater to the outbox inventory control

	if (LLMarketplaceInventoryImporter::getInstance()->isImportInProgress() || 
		(mWindowShade && mWindowShade->isShown()))
	{
		return FALSE;
	}

	S32 local_x = x - mOutboxInventoryPanel->getRect().mLeft;
	S32 local_y = y - mOutboxInventoryPanel->getRect().mBottom;

	return mOutboxInventoryPanel->handleDragAndDrop(local_x, local_y, mask, drop, cargo_type, cargo_data, accept, tooltip_msg);
}

void LLFloaterOutbox::onImportButtonClicked()
{
	mOutboxInventoryPanel->clearSelection();

	LLMarketplaceInventoryImporter::instance().triggerImport();
}

void LLFloaterOutbox::onOutboxChanged()
{
	llassert(!mOutboxId.isNull());

	fetchOutboxContents();
	updateView();
}

void LLFloaterOutbox::importReportResults(U32 status, const LLSD& content)
{
	if (status == MarketplaceErrorCodes::IMPORT_DONE)
	{
		LLNotificationsUtil::add("OutboxImportComplete");
	}
	else if (status == MarketplaceErrorCodes::IMPORT_DONE_WITH_ERRORS)
	{
		LLNotificationsUtil::add("OutboxImportHadErrors");
	}
	else
	{
		char status_string[16];
		sprintf(status_string, "%d", status);
		
		LLSD subs;
		subs["[ERROR_CODE]"] = status_string;
		
		//llassert(status == MarketplaceErrorCodes::IMPORT_JOB_FAILED);
		LLNotificationsUtil::add("OutboxImportFailed", subs);
	}
	
	updateView();
}

void LLFloaterOutbox::importStatusChanged(bool inProgress)
{
	if (inProgress)
	{
		mImportButton->setEnabled(false);
		
		mInventoryImportInProgress->setVisible(true);
	}
	else
	{
		mInventoryImportInProgress->setVisible(false);

		mImportButton->setEnabled(mOutboxItemCount > 0);
	}
}

void LLFloaterOutbox::showNotification(LLNotificationPtr notify)
{
	if (mWindowShade)
	{
		delete mWindowShade;
	}
	
	LLRect floater_rect = getLocalRect();
	floater_rect.mTop -= getHeaderHeight();
	floater_rect.stretch(-5, 0);
	
	LLWindowShade::Params params;
	params.name = "notification_shade";
	params.rect = floater_rect;
	params.follows.flags = FOLLOWS_ALL;
	params.notification = notify;
	params.modal = true;
	params.can_close = false;
	params.text_color = LLColor4::white;
	
	mWindowShade = LLUICtrlFactory::create<LLWindowShade>(params);
	
	addChild(mWindowShade);
	mWindowShade->show();
}

