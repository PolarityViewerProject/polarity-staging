/** 
 * @file llvieweraudio.cpp
 * @brief Audio functions that used to be in viewer.cpp
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

#include "llaudioengine.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llappviewer.h"
#include "lldeferredsounds.h"
#include "llvieweraudio.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llvoiceclient.h"
#include "llviewermedia.h"
#include "llviewerregion.h"
#include "llprogressview.h"
#include "llcallbacklist.h"
#include "llstartup.h"
#include "llviewerparcelmgr.h"
#include "llparcel.h"
#include "llviewermessage.h"

#include "llstreamingaudio.h"
#include "llvoavatarself.h"

/////////////////////////////////////////////////////////

LLViewerAudio::LLViewerAudio() :
	mDone(true),
	mFadeState(FADE_IDLE),
	mFadeTime(),
    mIdleListnerActive(false),
	mForcedTeleportFade(false),
	mWasPlaying(false)
{
	mTeleportFailedConnection = LLViewerParcelMgr::getInstance()->
		setTeleportFailedCallback(boost::bind(&LLViewerAudio::onTeleportFailed, this));
	mTeleportFinishedConnection = LLViewerParcelMgr::getInstance()->
		setTeleportFinishedCallback(boost::bind(&LLViewerAudio::onTeleportFinished, this, _1, _2));
	mTeleportStartedConnection = LLViewerMessage::getInstance()->
		setTeleportStartedCallback(boost::bind(&LLViewerAudio::onTeleportStarted, this));
}

LLViewerAudio::~LLViewerAudio()
{
	mTeleportFailedConnection.disconnect();
	mTeleportFinishedConnection.disconnect();
	mTeleportStartedConnection.disconnect();
}

void LLViewerAudio::registerIdleListener()
{
	if(mIdleListnerActive==false)
	{
		mIdleListnerActive = true;
		doOnIdleRepeating(boost::bind(boost::bind(&LLViewerAudio::onIdleUpdate, this)));
	}
}

void LLViewerAudio::startInternetStreamWithAutoFade(std::string streamURI)
{
	// Old and new stream are identical
	if (mNextStreamURI == streamURI)
	{
		return;
	}

	// Record the URI we are going to be switching to	
	mNextStreamURI = streamURI;

	switch (mFadeState)
	{
	case FADE_IDLE:
		if (LLAudioEngine::isInstanceMissing())
		{
			break;
		}
		// If a stream is playing fade it out first
		if (!gAudiop->getInternetStreamURL().empty())
		{
			// The order of these tests is important, state FADE_OUT will be processed below
			mFadeState = FADE_OUT;
		}
		// Otherwise the new stream can be faded in
		else
		{
			mFadeState = FADE_IN;

			LLStreamingAudioInterface *stream = gAudiop->getStreamingAudioImpl();
			if(stream && stream->supportsAdjustableBufferSizes())
				stream->setBufferSizes(gSavedSettings.getU32("FMODStreamBufferSize"),gSavedSettings.getU32("FMODDecodeBufferSize"));

			gAudiop->startInternetStream(mNextStreamURI);
			startFading();
			registerIdleListener();
			break;
		}

	case FADE_OUT:
		startFading();
		registerIdleListener();
		break;

	case FADE_IN:
		registerIdleListener();
		break;

	default:
		LL_WARNS() << "Unknown fading state: " << mFadeState << LL_ENDL;
		break;
	}
}

// A return of false from onIdleUpdate means it will be called again next idle update.
// A return of true means we have finished with it and the callback will be deleted.
bool LLViewerAudio::onIdleUpdate()
{
	bool fadeIsFinished = false;

	// There is a delay in the login sequence between when the parcel information has
	// arrived and the music stream is started and when the audio system is called to set
	// initial volume levels.  This code extends the fade time so you hear a full fade in.
	if ((LLStartUp::getStartupState() < STATE_STARTED))
	{
		stream_fade_timer.reset();
		stream_fade_timer.setTimerExpirySec(mFadeTime);
	}

	if (mDone)
	{
		//  This should be a rare or never occurring state.
		if (mFadeState == FADE_IDLE)
		{
			deregisterIdleListener();
			fadeIsFinished = true; // Stop calling onIdleUpdate
		}

		// we have finished the current fade operation
		if (mFadeState == FADE_OUT)
		{
			if (gAudiop)
			{
				// Clear URI
				gAudiop->startInternetStream(LLStringUtil::null);
				gAudiop->stopInternetStream();
			}
				
			if (!mNextStreamURI.empty())
			{
				mFadeState = FADE_IN;

				if (gAudiop)
				{
					LLStreamingAudioInterface *stream = gAudiop->getStreamingAudioImpl();
					if(stream && stream->supportsAdjustableBufferSizes())
						stream->setBufferSizes(gSavedSettings.getU32("FMODStreamBufferSize"),gSavedSettings.getU32("FMODDecodeBufferSize"));

					gAudiop->startInternetStream(mNextStreamURI);
				}

				startFading();
			}
			else
			{
				mFadeState = FADE_IDLE;
				deregisterIdleListener();
				fadeIsFinished = true; // Stop calling onIdleUpdate
			}
		}
		else if (mFadeState == FADE_IN)
		{
			if (gAudiop && mNextStreamURI != gAudiop->getInternetStreamURL())
			{
				mFadeState = FADE_OUT;
				startFading();
			}
			else
			{
				mFadeState = FADE_IDLE;
				deregisterIdleListener();
				fadeIsFinished = true; // Stop calling onIdleUpdate
			}
		}
	}

	return fadeIsFinished;
}

void LLViewerAudio::stopInternetStreamWithAutoFade()
{
	mFadeState = FADE_IDLE;
	mNextStreamURI = LLStringUtil::null;
	mDone = true;
	
	if (gAudiop)
	{
		gAudiop->startInternetStream(LLStringUtil::null);
		gAudiop->stopInternetStream();
	}
}

void LLViewerAudio::startFading()
{
	const F32 AUDIO_MUSIC_FADE_IN_TIME = 3.0f;
	const F32 AUDIO_MUSIC_FADE_OUT_TIME = 2.0f;
	// This minimum fade time prevents divide by zero and negative times
	const F32 AUDIO_MUSIC_MINIMUM_FADE_TIME = 0.01f;

	if(mDone)
	{
		// The fade state here should only be one of FADE_IN or FADE_OUT, but, in case it is not,
		// rather than check for both states assume a fade in and check for the fade out case.
		mFadeTime = AUDIO_MUSIC_FADE_IN_TIME;
		if (LLViewerAudio::getInstance()->getFadeState() == LLViewerAudio::FADE_OUT)
		{
			mFadeTime = AUDIO_MUSIC_FADE_OUT_TIME;
		}

		// Prevent invalid fade time
		mFadeTime = llmax(mFadeTime, AUDIO_MUSIC_MINIMUM_FADE_TIME);

		stream_fade_timer.reset();
		stream_fade_timer.setTimerExpirySec(mFadeTime);
		mDone = false;
	}
}

F32 LLViewerAudio::getFadeVolume()
{
	F32 fade_volume = 1.0f;

	if (stream_fade_timer.hasExpired())
	{
		mDone = true;
		// If we have been fading out set volume to 0 until the next fade state occurs to prevent
		// an audio transient.
		if (LLViewerAudio::getInstance()->getFadeState() == LLViewerAudio::FADE_OUT)
		{
			fade_volume = 0.0f;
		}
	}

	if (!mDone)
	{
		// Calculate how far we are into the fade time
		fade_volume = stream_fade_timer.getElapsedTimeF32() / mFadeTime;
		
		if (LLViewerAudio::getInstance()->getFadeState() == LLViewerAudio::FADE_OUT)
		{
			// If we are not fading in then we are fading out, so invert the fade
			// direction; start loud and move towards zero volume.
			fade_volume = 1.0f - fade_volume;
		}
	}

	return fade_volume;
}

void LLViewerAudio::onTeleportStarted()
{
	if (gAudiop && !LLViewerAudio::getInstance()->getForcedTeleportFade())
	{
		// Even though the music was turned off it was starting up (with autoplay disabled) occasionally
		// after a failed teleport or after an intra-parcel teleport.  Also, the music sometimes was not
		// restarting after a successful intra-parcel teleport. Setting mWasPlaying fixes these issues.
		LLViewerAudio::getInstance()->setWasPlaying(!gAudiop->getInternetStreamURL().empty());
		LLViewerAudio::getInstance()->setForcedTeleportFade(true);
		LLViewerAudio::getInstance()->startInternetStreamWithAutoFade(LLStringUtil::null);
		LLViewerAudio::getInstance()->setNextStreamURI(LLStringUtil::null);
	}
}

void LLViewerAudio::onTeleportFailed()
{
	// Calling audio_update_volume makes sure that the music stream is properly set to be restored to
	// its previous value
	audio_update_volume(false);

	if (gAudiop && mWasPlaying)
	{
		LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
		if (parcel)
		{
			mNextStreamURI = parcel->getMusicURL();
			LL_INFOS() << "Teleport failed -- setting music stream to " << mNextStreamURI << LL_ENDL;
		}
	}
	mWasPlaying = false;
}

void LLViewerAudio::onTeleportFinished(const LLVector3d& pos, const bool& local)
{
	// Calling audio_update_volume makes sure that the music stream is properly set to be restored to
	// its previous value
	audio_update_volume(false);

	if (gAudiop && local && mWasPlaying)
	{
		LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
		if (parcel)
		{
			mNextStreamURI = parcel->getMusicURL();
			LL_INFOS() << "Intraparcel teleport -- setting music stream to " << mNextStreamURI << LL_ENDL;
		}
	}
	mWasPlaying = false;
}

void init_audio() 
{
	if (!gAudiop) 
	{
		LL_WARNS() << "Failed to create an appropriate Audio Engine" << LL_ENDL;
		return;
	}
	LLVector3d lpos_global = gAgentCamera.getCameraPositionGlobal();
	LLVector3 lpos_global_f;

	lpos_global_f.setVec(lpos_global);
					
	gAudiop->setListener(lpos_global_f,
						  LLVector3::zero,	// LLViewerCamera::getInstance()->getVelocity(),    // !!! BUG need to replace this with smoothed velocity!
						  LLViewerCamera::getInstance()->getUpAxis(),
						  LLViewerCamera::getInstance()->getAtAxis());

// load up our initial set of sounds we'll want so they're in memory and ready to be played

	BOOL mute_audio = gSavedSettings.getBOOL("MuteAudio");

	if (!mute_audio && FALSE == gSavedSettings.getBOOL("NoPreload"))
	{
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndAlert")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndBadKeystroke")));
		//gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndChatFromObject")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndClick")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndClickRelease")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndHealthReductionF")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndHealthReductionM")));
		//gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndIncomingChat")));
		//gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndIncomingIM")));
		//gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndInvApplyToObject")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndInvalidOp")));
		//gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndInventoryCopyToInv")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndMoneyChangeDown")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndMoneyChangeUp")));
		//gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndObjectCopyToInv")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndObjectCreate")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndObjectDelete")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndObjectRezIn")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndObjectRezOut")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndSnapshot")));
		//gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndStartAutopilot")));
		//gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndStartFollowpilot")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndStartIM")));
		//gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndStopAutopilot")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndTeleportOut")));
		//gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndTextureApplyToObject")));
		//gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndTextureCopyToInv")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndTyping")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndWindowClose")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndWindowOpen")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndRestart")));
	}

	audio_update_volume(true);
}

void audio_update_volume(bool force_update)
{
	static LLCachedControl<F32> sAudioLevelMaster(gSavedSettings, "AudioLevelMaster");
	static LLCachedControl<bool> sMuteAudio(gSavedSettings, "MuteAudio");
	F32 master_volume = sAudioLevelMaster();
	BOOL mute_audio = (BOOL)sMuteAudio;


	LLProgressView* progress = gViewerWindow->getProgressView();
	BOOL progress_view_visible = FALSE;

	if (progress)
	{
		progress_view_visible = progress->getVisible();
	}

	if (!gViewerWindow->getActive() && gSavedSettings.getBOOL("MuteWhenMinimized"))
	{
		mute_audio = TRUE;
	}
	F32 mute_volume = mute_audio ? 0.0f : 1.0f;

	if (gAudiop) 
	{
		// Sound Effects

		gAudiop->setMasterGain ( master_volume );

		static LLCachedControl<F32> sAudioLevelDoppler(gSavedSettings, "AudioLevelDoppler");
		static LLCachedControl<F32> sAudioLevelRolloff(gSavedSettings, "AudioLevelRolloff");
		static LLCachedControl<F32> sAudioLevelUnderwaterRolloff(gSavedSettings, "AudioLevelUnderwaterRolloff");
		gAudiop->setDopplerFactor(sAudioLevelDoppler);

		if(!LLViewerCamera::getInstance()->cameraUnderWater())
		gAudiop->setRolloffFactor(sAudioLevelRolloff);
		else
			gAudiop->setRolloffFactor(sAudioLevelUnderwaterRolloff);

		gAudiop->setMuted(mute_audio || progress_view_visible);
		
		//Play any deferred sounds when unmuted
		if(!gAudiop->getMuted())
		{
			LLDeferredSounds::instance().playdeferredSounds();
		}

		if (force_update)
		{
			audio_update_wind(true);
		}

		// handle secondary gains
		static LLCachedControl<bool> muteSounds(gSavedSettings, "MuteSounds");
		static LLCachedControl<bool> muteUI(gSavedSettings, "MuteUI");
		static LLCachedControl<bool> muteAmbient(gSavedSettings, "MuteAmbient");
		static LLCachedControl<F32> audioLevelSFX(gSavedSettings, "AudioLevelSFX");
		static LLCachedControl<F32> audioLevelUI(gSavedSettings, "AudioLevelUI");
		static LLCachedControl<F32> audioLevelAmbient(gSavedSettings, "AudioLevelAmbient");

		gAudiop->setSecondaryGain(LLAudioEngine::AUDIO_TYPE_SFX,
								muteSounds ? 0.f : (F32)audioLevelSFX);
		gAudiop->setSecondaryGain(LLAudioEngine::AUDIO_TYPE_UI,
								muteUI ? 0.f : (F32)audioLevelUI);
		gAudiop->setSecondaryGain(LLAudioEngine::AUDIO_TYPE_AMBIENT,
								muteAmbient ? 0.f : (F32)audioLevelAmbient);

		// Streaming Music

		if (!progress_view_visible && LLViewerAudio::getInstance()->getForcedTeleportFade())
		{
			LLViewerAudio::getInstance()->setWasPlaying(!gAudiop->getInternetStreamURL().empty());
			LLViewerAudio::getInstance()->setForcedTeleportFade(false);
		}

		static LLCachedControl<F32> audioLevelMusic(gSavedSettings, "AudioLevelMusic");
		static LLCachedControl<bool> muteMusic(gSavedSettings, "MuteMusic");
		F32 music_volume = (F32)audioLevelMusic;
		BOOL music_muted = (BOOL)muteMusic;

		F32 fade_volume = LLViewerAudio::getInstance()->getFadeVolume();

		music_volume = mute_volume * master_volume * music_volume * fade_volume;
		gAudiop->setInternetStreamGain (music_muted ? 0.f : music_volume);
	}

	// Streaming Media
	static LLCachedControl<F32> audioLevelMedia(gSavedSettings, "AudioLevelMedia");
	static LLCachedControl<bool> muteMedia(gSavedSettings, "MuteMedia");
	F32 media_volume = (F32)audioLevelMedia;
	BOOL media_muted = (BOOL)muteMedia;

	media_volume = mute_volume * master_volume * media_volume;
	LLViewerMedia::setVolume( media_muted ? 0.0f : media_volume );

	// Voice
	if (LLVoiceClient::getInstance())
	{
		static LLCachedControl<F32> audioLevelVoice(gSavedSettings, "AudioLevelVoice");
		F32 voice_volume = (F32)audioLevelVoice;
		voice_volume = mute_volume * master_volume * voice_volume;
		static LLCachedControl<bool> muteVoice(gSavedSettings, "MuteVoice");
		BOOL voice_mute = (BOOL)muteVoice;
		LLVoiceClient::getInstance()->setVoiceVolume(voice_mute ? 0.f : voice_volume);
		static LLCachedControl<F32> audioLevelMic(gSavedSettings, "AudioLevelMic");
		LLVoiceClient::getInstance()->setMicGain(voice_mute ? 0.f : (F32)audioLevelMic);

		static LLCachedControl<bool> muteWhenMinimized(gSavedSettings, "MuteWhenMinimized");
		if (!gViewerWindow->getActive() && muteWhenMinimized)
		{
			LLVoiceClient::getInstance()->setMuteMic(true);
		}
		else
		{
			LLVoiceClient::getInstance()->setMuteMic(false);
		}
	}
}

void audio_update_listener()
{
	if (gAudiop)
	{
		// update listener position because agent has moved	
		LLVector3d lpos_global = gAgentCamera.getCameraPositionGlobal();		
		LLVector3 lpos_global_f;
		lpos_global_f.setVec(lpos_global);
	
		gAudiop->setListener(lpos_global_f,
							 // LLViewerCamera::getInstance()VelocitySmoothed, 
							 // LLVector3::zero,	
							 gAgent.getVelocity(),    // !!! *TODO: need to replace this with smoothed velocity!
							 LLViewerCamera::getInstance()->getUpAxis(),
							 LLViewerCamera::getInstance()->getAtAxis());
	}
}

void audio_update_wind(bool force_update)
{
#ifdef kAUDIO_ENABLE_WIND

	LLViewerRegion* region = gAgent.getRegion();
	if (region)
	{
        // Scale down the contribution of weather-simulation wind to the
        // ambient wind noise.  Wind velocity averages 3.5 m/s, with gusts to 7 m/s
        // whereas steady-state avatar walk velocity is only 3.2 m/s.
        // Without this the world feels desolate on first login when you are
        // standing still.
        static LLUICachedControl<F32> wind_level("AudioLevelWind", 0.5f);
        LLVector3 scaled_wind_vec = gWindVec * wind_level;
        
        // Mix in the avatar's motion, subtract because when you walk north,
        // the apparent wind moves south.
        LLVector3 final_wind_vec = scaled_wind_vec - gAgent.getVelocity();
        
		// rotate the wind vector to be listener (agent) relative
		gRelativeWindVec = gAgent.getFrameAgent().rotateToLocal( final_wind_vec );

		// don't use the setter setMaxWindGain() because we don't
		// want to screw up the fade-in on startup by setting actual source gain
		// outside the fade-in.
		static LLCachedControl<bool> muteAudio(gSavedSettings, "MuteAudio");
		static LLCachedControl<bool> muteAmbient(gSavedSettings, "MuteAmbient");
		static LLCachedControl<F32> audioLevelMaster(gSavedSettings, "AudioLevelMaster");
		static LLCachedControl<F32> audioLevelAmbient(gSavedSettings, "AudioLevelAmbient");
		F32 master_volume = muteAudio ? 0.f : (F32)audioLevelMaster;
		F32 ambient_volume = muteAmbient ? 0.f : (F32)audioLevelAmbient;
		F32 max_wind_volume = master_volume * ambient_volume;

		const F32 WIND_SOUND_TRANSITION_TIME = 2.f;
		// amount to change volume this frame
		F32 volume_delta = (LLFrameTimer::getFrameDeltaTimeF32() / WIND_SOUND_TRANSITION_TIME) * max_wind_volume;
		if (force_update) 
		{
			// initialize wind volume (force_update) by using large volume_delta
			// which is sufficient to completely turn off or turn on wind noise
			volume_delta = 1.f;
		}

		if (LLAudioEngine::isInstanceMissing())
		{
			return;
		}

		// mute wind when not flying
		// <FS:Ansarriel> FIRE-12819: Disable wind sounds while under water
		//if (gAgent.getFlying())
		if (gAgent.getFlying() && isAgentAvatarValid() && !gAgentAvatarp->mBelowWater)
		// </FS:>
		{
			// volume increases by volume_delta, up to no more than max_wind_volume
			gAudiop->mMaxWindGain = llmin(gAudiop->mMaxWindGain + volume_delta, max_wind_volume);
		}
		else
		{
			// volume decreases by volume_delta, down to no less than 0
			gAudiop->mMaxWindGain = llmax(gAudiop->mMaxWindGain - volume_delta, 0.f);
		}
		
		gAudiop->updateWind(gRelativeWindVec, gAgentCamera.getCameraPositionAgent()[VZ] - gAgent.getRegion()->getWaterHeight());
	}
#endif
}
