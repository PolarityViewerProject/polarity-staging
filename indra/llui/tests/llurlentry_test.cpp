/**
 * @file llurlentry_test.cpp
 * @author Martin Reddy
 * @brief Unit tests for LLUrlEntry objects
 *
 * $LicenseInfo:firstyear=2009&license=viewergpl$
 * 
 * Copyright (c) 2009, Linden Research, Inc.
 * 
 * The following source code is PROPRIETARY AND CONFIDENTIAL. Use of
 * this source code is governed by the Linden Lab Source Code Disclosure
 * Agreement ("Agreement") previously entered between you and Linden
 * Lab. By accessing, using, copying, modifying or distributing this
 * software, you acknowledge that you have been informed of your
 * obligations under the Agreement and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "linden_common.h"
#include "../llurlentry.h"
#include "llurlentry_stub.cpp"
#include "lltut.h"
#include "../lluicolortable.h"

#include <boost/regex.hpp>

LLUIColor LLUIColorTable::getColor(const std::string& name, const LLColor4& default_color) const
{
	return LLUIColor();
}

LLUIColor::LLUIColor() : mColorPtr(NULL) {}

namespace tut
{
	struct LLUrlEntryData
	{
	};

	typedef test_group<LLUrlEntryData> factory;
	typedef factory::object object;
}

namespace
{
	tut::factory tf("LLUrlEntry");
}

namespace tut
{
	void testRegex(const std::string &testname, LLUrlEntryBase &entry,
				   const char *text, const std::string &expected)
	{
		boost::regex regex = entry.getPattern();
		std::string url = "";
		boost::cmatch result;
		bool found = boost::regex_search(text, result, regex);
		if (found)
		{
			S32 start = static_cast<U32>(result[0].first - text);
			S32 end = static_cast<U32>(result[0].second - text);
			url = entry.getUrl(std::string(text+start, end-start));
		}
		ensure_equals(testname, url, expected);
	}

	template<> template<>
	void object::test<1>()
	{
		//
		// test LLUrlEntryHTTP - standard http Urls
		//
		LLUrlEntryHTTP url;

		testRegex("no valid url", url,
				  "htp://slurl.com/",
				  "");

		testRegex("simple http (1)", url,
				  "http://slurl.com/",
				  "http://slurl.com/");

		testRegex("simple http (2)", url,
				  "http://slurl.com",
				  "http://slurl.com");

		testRegex("simple http (3)", url,
				  "http://slurl.com/about.php",
				  "http://slurl.com/about.php");

		testRegex("simple https", url,
				  "https://slurl.com/about.php",
				  "https://slurl.com/about.php");

		testRegex("http in text (1)", url,
				  "XX http://slurl.com/ XX",
				  "http://slurl.com/");

		testRegex("http in text (2)", url,
				  "XX http://slurl.com/about.php XX",
				  "http://slurl.com/about.php");

		testRegex("https in text", url,
				  "XX https://slurl.com/about.php XX",
				  "https://slurl.com/about.php");

		testRegex("two http urls", url,
				  "XX http://slurl.com/about.php http://secondlife.com/ XX",
				  "http://slurl.com/about.php");

		testRegex("http url with port and username", url,
				  "XX http://nobody@slurl.com:80/about.php http://secondlife.com/ XX",
				  "http://nobody@slurl.com:80/about.php");

		testRegex("http url with port, username, and query string", url,
				  "XX http://nobody@slurl.com:80/about.php?title=hi%20there http://secondlife.com/ XX",
				  "http://nobody@slurl.com:80/about.php?title=hi%20there");

		// note: terminating commas will be removed by LLUrlRegistry:findUrl()
		testRegex("http url with commas in middle and terminating", url,
				  "XX http://slurl.com/?title=Hi,There, XX",
				  "http://slurl.com/?title=Hi,There,");

		// note: terminating periods will be removed by LLUrlRegistry:findUrl()
		testRegex("http url with periods in middle and terminating", url,
				  "XX http://slurl.com/index.php. XX",
				  "http://slurl.com/index.php.");

		// DEV-19842: Closing parenthesis ")" breaks urls
		testRegex("http url with brackets (1)", url,
				  "XX http://en.wikipedia.org/wiki/JIRA_(software) XX",
				  "http://en.wikipedia.org/wiki/JIRA_(software)");

		// DEV-19842: Closing parenthesis ")" breaks urls
		testRegex("http url with brackets (2)", url, 
				  "XX http://jira.secondlife.com/secure/attachment/17990/eggy+avs+in+1.21.0+(93713)+public+nightly.jpg XX",
				  "http://jira.secondlife.com/secure/attachment/17990/eggy+avs+in+1.21.0+(93713)+public+nightly.jpg");

		// DEV-10353: URLs in chat log terminated incorrectly when newline in chat
		testRegex("http url with newlines", url,
				  "XX\nhttp://www.secondlife.com/\nXX",
				  "http://www.secondlife.com/");
	}

	template<> template<>
	void object::test<2>()
	{
		//
		// test LLUrlEntryHTTPLabel - wiki-style http Urls with labels
		//
		LLUrlEntryHTTPLabel url;

		testRegex("invalid wiki url [1]", url,
				  "[http://www.example.org]",
				  "");

		testRegex("invalid wiki url [2]", url,
				  "[http://www.example.org",
				  "");

		testRegex("invalid wiki url [3]", url,
				  "[http://www.example.org Label",
				  "");

		testRegex("example.org with label (spaces)", url,
				  "[http://www.example.org  Text]",
				  "http://www.example.org");

		testRegex("example.org with label (tabs)", url,
				  "[http://www.example.org\t Text]",
				  "http://www.example.org");

		testRegex("SL http URL with label", url,
				  "[http://www.secondlife.com/ Second Life]",
				  "http://www.secondlife.com/");

		testRegex("SL https URL with label", url,
				  "XXX [https://www.secondlife.com/ Second Life] YYY",
				  "https://www.secondlife.com/");

		testRegex("SL http URL with label", url,
				  "[http://www.secondlife.com/?test=Hi%20There Second Life]",
				  "http://www.secondlife.com/?test=Hi%20There");
	}

	template<> template<>
	void object::test<3>()
	{
		//
		// test LLUrlEntrySLURL - second life URLs
		//
		LLUrlEntrySLURL url;

		testRegex("no valid slurl [1]", url,
				  "htp://slurl.com/secondlife/Ahern/50/50/50/",
				  "");

		testRegex("no valid slurl [2]", url,
				  "http://slurl.com/secondlife/",
				  "");

		testRegex("no valid slurl [3]", url,
				  "hhtp://slurl.com/secondlife/Ahern/50/FOO/50/",
				  "");

		testRegex("Ahern (50,50,50) [1]", url,
				  "http://slurl.com/secondlife/Ahern/50/50/50/",
				  "http://slurl.com/secondlife/Ahern/50/50/50/");

		testRegex("Ahern (50,50,50) [2]", url,
				  "XXX http://slurl.com/secondlife/Ahern/50/50/50/ XXX",
				  "http://slurl.com/secondlife/Ahern/50/50/50/");

		testRegex("Ahern (50,50,50) [3]", url,
				  "XXX http://slurl.com/secondlife/Ahern/50/50/50 XXX",
				  "http://slurl.com/secondlife/Ahern/50/50/50");

		testRegex("Ahern (50,50,50) multicase", url,
				  "XXX http://SLUrl.com/SecondLife/Ahern/50/50/50/ XXX",
				  "http://SLUrl.com/SecondLife/Ahern/50/50/50/");

		testRegex("Ahern (50,50) [1]", url,
				  "XXX http://slurl.com/secondlife/Ahern/50/50/ XXX",
				  "http://slurl.com/secondlife/Ahern/50/50/");

		testRegex("Ahern (50,50) [2]", url,
				  "XXX http://slurl.com/secondlife/Ahern/50/50 XXX",
				  "http://slurl.com/secondlife/Ahern/50/50");

		testRegex("Ahern (50)", url,
				  "XXX http://slurl.com/secondlife/Ahern/50 XXX",
				  "http://slurl.com/secondlife/Ahern/50");

		testRegex("Ahern", url,
				  "XXX http://slurl.com/secondlife/Ahern/ XXX",
				  "http://slurl.com/secondlife/Ahern/");

		testRegex("Ahern SLURL with title", url,
				  "XXX http://slurl.com/secondlife/Ahern/50/50/50/?title=YOUR%20TITLE%20HERE! XXX",
				  "http://slurl.com/secondlife/Ahern/50/50/50/?title=YOUR%20TITLE%20HERE!");

		testRegex("Ahern SLURL with msg", url,
				  "XXX http://slurl.com/secondlife/Ahern/50/50/50/?msg=Your%20text%20here. XXX",
				  "http://slurl.com/secondlife/Ahern/50/50/50/?msg=Your%20text%20here.");

		// DEV-21577: In-world SLURLs containing "(" or ")" are not treated as a hyperlink in chat
		testRegex("SLURL with brackets", url,
				  "XXX http://slurl.com/secondlife/Burning%20Life%20(Hyper)/27/210/30 XXX",
				  "http://slurl.com/secondlife/Burning%20Life%20(Hyper)/27/210/30");

		// DEV-35459: SLURLs and teleport Links not parsed properly
		testRegex("SLURL with quote", url,
				  "XXX http://slurl.com/secondlife/A'ksha%20Oasis/41/166/701 XXX",
				  "http://slurl.com/secondlife/A%27ksha%20Oasis/41/166/701");
	}

	template<> template<>
	void object::test<4>()
	{
		//
		// test LLUrlEntryAgent - secondlife://app/agent Urls
		//
		LLUrlEntryAgent url;

		testRegex("Invalid Agent Url", url,
				  "secondlife:///app/agent/0e346d8b-4433-4d66-XXXX-fd37083abc4c/about",
				  "");

		testRegex("Agent Url ", url,
				  "secondlife:///app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/about",
				  "secondlife:///app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/about");

		testRegex("Agent Url in text", url,
				  "XXX secondlife:///app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/about XXX",
				  "secondlife:///app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/about");

		testRegex("Agent Url multicase", url,
				  "XXX secondlife:///App/AGENT/0E346D8B-4433-4d66-a6b0-fd37083abc4c/About XXX",
				  "secondlife:///App/AGENT/0E346D8B-4433-4d66-a6b0-fd37083abc4c/About");

		testRegex("Agent Url alternate command", url,
				  "XXX secondlife:///App/AGENT/0E346D8B-4433-4d66-a6b0-fd37083abc4c/foobar",
				  "secondlife:///App/AGENT/0E346D8B-4433-4d66-a6b0-fd37083abc4c/foobar");

		testRegex("Nebraska Agent Url ", url,
				  "x-grid-location-info://lincoln.lindenlab.com/app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/about",
				  "x-grid-location-info://lincoln.lindenlab.com/app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/about");		

		testRegex("Nebraska Agent Url Multicase with Text", url,
				  "M x-grid-location-info://lincoln.lindenlab.com/app/AGENT/0e346d8b-4433-4d66-a6b0-fd37083abc4c/about M",
				  "x-grid-location-info://lincoln.lindenlab.com/app/AGENT/0e346d8b-4433-4d66-a6b0-fd37083abc4c/about");		
	}

	template<> template<>
	void object::test<5>()
	{
		//
		// test LLUrlEntryGroup - secondlife://app/group Urls
		//
		LLUrlEntryGroup url;

		testRegex("Invalid Group Url", url,
				  "secondlife:///app/group/00005ff3-4044-c79f-XXXX-fb28ae0df991/about",
				  "");

		testRegex("Group Url ", url,
				  "secondlife:///app/group/00005ff3-4044-c79f-9de8-fb28ae0df991/about",
				  "secondlife:///app/group/00005ff3-4044-c79f-9de8-fb28ae0df991/about");

		testRegex("Group Url ", url,
				  "secondlife:///app/group/00005ff3-4044-c79f-9de8-fb28ae0df991/inspect",
				  "secondlife:///app/group/00005ff3-4044-c79f-9de8-fb28ae0df991/inspect");

		testRegex("Group Url in text", url,
				  "XXX secondlife:///app/group/00005ff3-4044-c79f-9de8-fb28ae0df991/about XXX",
				  "secondlife:///app/group/00005ff3-4044-c79f-9de8-fb28ae0df991/about");

		testRegex("Group Url multicase", url,
				  "XXX secondlife:///APP/Group/00005FF3-4044-c79f-9de8-fb28ae0df991/About XXX",
				  "secondlife:///APP/Group/00005FF3-4044-c79f-9de8-fb28ae0df991/About");
		
		testRegex("Nebraska Group Url ", url,
				  "x-grid-location-info://lincoln.lindenlab.com/app/group/0e346d8b-4433-4d66-a6b0-fd37083abc4c/about",
				  "x-grid-location-info://lincoln.lindenlab.com/app/group/0e346d8b-4433-4d66-a6b0-fd37083abc4c/about");		
		
		testRegex("Nebraska Group Url Multicase ith Text", url,
				  "M x-grid-location-info://lincoln.lindenlab.com/app/GROUP/0e346d8b-4433-4d66-a6b0-fd37083abc4c/about M",
				  "x-grid-location-info://lincoln.lindenlab.com/app/GROUP/0e346d8b-4433-4d66-a6b0-fd37083abc4c/about");		
		
	}

	template<> template<>
	void object::test<6>()
	{
		//
		// test LLUrlEntryPlace - secondlife://<location> URLs
		//
		LLUrlEntryPlace url;

		testRegex("no valid slurl [1]", url,
				  "secondlife://Ahern/FOO/50/",
				  "");

		testRegex("Ahern (50,50,50) [1]", url,
				  "secondlife://Ahern/50/50/50/",
				  "secondlife://Ahern/50/50/50/");

		testRegex("Ahern (50,50,50) [2]", url,
				  "XXX secondlife://Ahern/50/50/50/ XXX",
				  "secondlife://Ahern/50/50/50/");

		testRegex("Ahern (50,50,50) [3]", url,
				  "XXX secondlife://Ahern/50/50/50 XXX",
				  "secondlife://Ahern/50/50/50");

		testRegex("Ahern (50,50,50) multicase", url,
				  "XXX SecondLife://Ahern/50/50/50/ XXX",
				  "SecondLife://Ahern/50/50/50/");

		testRegex("Ahern (50,50) [1]", url,
				  "XXX secondlife://Ahern/50/50/ XXX",
				  "secondlife://Ahern/50/50/");

		testRegex("Ahern (50,50) [2]", url,
				  "XXX secondlife://Ahern/50/50 XXX",
				  "secondlife://Ahern/50/50");

		// DEV-21577: In-world SLURLs containing "(" or ")" are not treated as a hyperlink in chat
		testRegex("SLURL with brackets", url,
				  "XXX secondlife://Burning%20Life%20(Hyper)/27/210/30 XXX",
				  "secondlife://Burning%20Life%20(Hyper)/27/210/30");

		// DEV-35459: SLURLs and teleport Links not parsed properly
		testRegex("SLURL with quote", url,
				  "XXX secondlife://A'ksha%20Oasis/41/166/701 XXX",
			          "secondlife://A%27ksha%20Oasis/41/166/701");
		
		testRegex("Nebraska All Hands (50,50) [2] with text", url,
				  "XXX x-grid-location-info://lincoln.lindenlab.com/region/All%20Hands/50/50/50 XXX",
				  "x-grid-location-info://lincoln.lindenlab.com/region/All%20Hands/50/50/50");		
	}

	template<> template<>
	void object::test<7>()
	{
		//
		// test LLUrlEntryParcel - secondlife://app/parcel Urls
		//
		LLUrlEntryParcel url;

		testRegex("Invalid Classified Url", url,
				  "secondlife:///app/parcel/0000060e-4b39-e00b-XXXX-d98b1934e3a8/about",
				  "");

		testRegex("Classified Url ", url,
				  "secondlife:///app/parcel/0000060e-4b39-e00b-d0c3-d98b1934e3a8/about",
				  "secondlife:///app/parcel/0000060e-4b39-e00b-d0c3-d98b1934e3a8/about");

		testRegex("Classified Url in text", url,
				  "XXX secondlife:///app/parcel/0000060e-4b39-e00b-d0c3-d98b1934e3a8/about XXX",
				  "secondlife:///app/parcel/0000060e-4b39-e00b-d0c3-d98b1934e3a8/about");

		testRegex("Classified Url multicase", url,
				  "XXX secondlife:///APP/Parcel/0000060e-4b39-e00b-d0c3-d98b1934e3a8/About XXX",
				  "secondlife:///APP/Parcel/0000060e-4b39-e00b-d0c3-d98b1934e3a8/About");
	}
	template<> template<>
	void object::test<8>()
	{
		//
		// test LLUrlEntryTeleport - secondlife://app/teleport URLs
		//
		LLUrlEntryTeleport url;

		testRegex("no valid teleport [1]", url,
				  "http://slurl.com/secondlife/Ahern/50/50/50/",
				  "");

		testRegex("no valid teleport [2]", url,
				  "secondlife:///app/teleport/",
				  "");

		testRegex("no valid teleport [3]", url,
				  "second-life:///app/teleport/Ahern/50/50/50/",
				  "");

		testRegex("no valid teleport [3]", url,
				  "hhtp://slurl.com/secondlife/Ahern/50/FOO/50/",
				  "");

		testRegex("Ahern (50,50,50) [1]", url,
				  "secondlife:///app/teleport/Ahern/50/50/50/",
				  "secondlife:///app/teleport/Ahern/50/50/50/");

		testRegex("Ahern (50,50,50) [2]", url,
				  "XXX secondlife:///app/teleport/Ahern/50/50/50/ XXX",
				  "secondlife:///app/teleport/Ahern/50/50/50/");

		testRegex("Ahern (50,50,50) [3]", url,
				  "XXX secondlife:///app/teleport/Ahern/50/50/50 XXX",
				  "secondlife:///app/teleport/Ahern/50/50/50");

		testRegex("Ahern (50,50,50) multicase", url,
				  "XXX secondlife:///app/teleport/Ahern/50/50/50/ XXX",
				  "secondlife:///app/teleport/Ahern/50/50/50/");

		testRegex("Ahern (50,50) [1]", url,
				  "XXX secondlife:///app/teleport/Ahern/50/50/ XXX",
				  "secondlife:///app/teleport/Ahern/50/50/");

		testRegex("Ahern (50,50) [2]", url,
				  "XXX secondlife:///app/teleport/Ahern/50/50 XXX",
				  "secondlife:///app/teleport/Ahern/50/50");

		testRegex("Ahern (50)", url,
				  "XXX secondlife:///app/teleport/Ahern/50 XXX",
				  "secondlife:///app/teleport/Ahern/50");

		testRegex("Ahern", url,
				  "XXX secondlife:///app/teleport/Ahern/ XXX",
				  "secondlife:///app/teleport/Ahern/");

		testRegex("Ahern teleport with title", url,
				  "XXX secondlife:///app/teleport/Ahern/50/50/50/?title=YOUR%20TITLE%20HERE! XXX",
				  "secondlife:///app/teleport/Ahern/50/50/50/?title=YOUR%20TITLE%20HERE!");

		testRegex("Ahern teleport with msg", url,
				  "XXX secondlife:///app/teleport/Ahern/50/50/50/?msg=Your%20text%20here. XXX",
				  "secondlife:///app/teleport/Ahern/50/50/50/?msg=Your%20text%20here.");

		// DEV-21577: In-world SLURLs containing "(" or ")" are not treated as a hyperlink in chat
		testRegex("Teleport with brackets", url,
				  "XXX secondlife:///app/teleport/Burning%20Life%20(Hyper)/27/210/30 XXX",
				  "secondlife:///app/teleport/Burning%20Life%20(Hyper)/27/210/30");

		// DEV-35459: SLURLs and teleport Links not parsed properly
		testRegex("Teleport url with quote", url,
				  "XXX secondlife:///app/teleport/A'ksha%20Oasis/41/166/701 XXX",
				  "secondlife:///app/teleport/A%27ksha%20Oasis/41/166/701");
		
		testRegex("Nebraska All Hands", url,
				  "XXX x-grid-location-info://lincoln.lindenlab.com/app/teleport/All%20Hands/50/50/50 XXX",
				  "x-grid-location-info://lincoln.lindenlab.com/app/teleport/All%20Hands/50/50/50");		
	}

	template<> template<>
	void object::test<9>()
	{
		//
		// test LLUrlEntrySL - general secondlife:// URLs
		//
		LLUrlEntrySL url;

		testRegex("no valid slapp [1]", url,
				  "http:///app/",
				  "");

		testRegex("valid slapp [1]", url,
				  "secondlife:///app/",
				  "secondlife:///app/");

		testRegex("valid slapp [2]", url,
				  "secondlife:///app/teleport/Ahern/50/50/50/",
				  "secondlife:///app/teleport/Ahern/50/50/50/");

		testRegex("valid slapp [3]", url,
				  "secondlife:///app/foo",
				  "secondlife:///app/foo");

		testRegex("valid slapp [4]", url,
				  "secondlife:///APP/foo?title=Hi%20There",
				  "secondlife:///APP/foo?title=Hi%20There");

		testRegex("valid slapp [5]", url,
				  "secondlife://host/app/",
				  "secondlife://host/app/");

		testRegex("valid slapp [6]", url,
				  "secondlife://host:8080/foo/bar",
				  "secondlife://host:8080/foo/bar");
	}

	template<> template<>
	void object::test<10>()
	{
		//
		// test LLUrlEntrySLLabel - general secondlife:// URLs with labels
		//
		LLUrlEntrySLLabel url;

		testRegex("invalid wiki url [1]", url,
				  "[secondlife:///app/]",
				  "");

		testRegex("invalid wiki url [2]", url,
				  "[secondlife:///app/",
				  "");

		testRegex("invalid wiki url [3]", url,
				  "[secondlife:///app/ Label",
				  "");

		testRegex("agent slurl with label (spaces)", url,
				  "[secondlife:///app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/about  Text]",
				  "secondlife:///app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/about");

		testRegex("agent slurl with label (tabs)", url,
				  "[secondlife:///app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/about\t Text]",
				  "secondlife:///app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/about");

		testRegex("agent slurl with label", url,
				  "[secondlife:///app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/about FirstName LastName]",
				  "secondlife:///app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/about");

		testRegex("teleport slurl with label", url,
				  "XXX [secondlife:///app/teleport/Ahern/50/50/50/ Teleport to Ahern] YYY",
				  "secondlife:///app/teleport/Ahern/50/50/50/");
	}

	template<> template<>
	void object::test<11>()
	{
		//
		// test LLUrlEntryHTTPNoProtocol - general URLs without a protocol
		//
		LLUrlEntryHTTPNoProtocol url;

		testRegex("naked .com URL", url,
				  "see google.com",
				  "http://google.com");

		testRegex("naked .org URL", url,
				  "see en.wikipedia.org for details",
				  "http://en.wikipedia.org");

		testRegex("naked .net URL", url,
				  "example.net",
				  "http://example.net");

		testRegex("naked .edu URL (2 instances)", url,
				  "MIT web site is at web.mit.edu and also www.mit.edu",
				  "http://web.mit.edu");

		testRegex("don't match e-mail addresses", url,
				  "test@lindenlab.com",
				  "");

		testRegex(".com URL with path", url,
				  "see secondlife.com/status for grid status",
				  "http://secondlife.com/status");

		testRegex(".com URL with port", url,
				  "secondlife.com:80",
				  "http://secondlife.com:80");

		testRegex(".com URL with port and path", url,
				  "see secondlife.com:80/status",
				  "http://secondlife.com:80/status");

		testRegex("www.*.com URL with port and path", url,
				  "see www.secondlife.com:80/status",
				  "http://www.secondlife.com:80/status");

		testRegex("invalid .com URL [1]", url,
				  "..com",
				  "");

		testRegex("invalid .com URL [2]", url,
				  "you.come",
				  "");

		testRegex("invalid .com URL [3]", url,
				  "recommended",
				  "");

		testRegex("invalid .edu URL", url,
				  "hi there scheduled maitenance has begun",
				  "");

		testRegex("invalid .net URL", url,
				  "foo.netty",
				  "");

		testRegex("XML tags around URL [1]", url,
				  "<foo>secondlife.com</foo>",
				  "http://secondlife.com");

		testRegex("XML tags around URL [2]", url,
				  "<foo>secondlife.com/status?bar=1</foo>",
				  "http://secondlife.com/status?bar=1");
	}

	template<> template<>
	void object::test<12>()
	{
		//
		// test LLUrlEntryNoLink - turn off hyperlinking
		//
		LLUrlEntryNoLink url;

		testRegex("<nolink> [1]", url,
				  "<nolink>google.com</nolink>",
				  "google.com");

		testRegex("<nolink> [2]", url,
				  "<nolink>google.com",
				  "");

		testRegex("<nolink> [3]", url,
				  "google.com</nolink>",
				  "");

		testRegex("<nolink> [4]", url,
				  "<nolink>Hello World</nolink>",
				  "Hello World");

		testRegex("<nolink> [5]", url,
				  "<nolink>My Object</nolink>",
				  "My Object");
	}
}
