/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2013 see Authors.txt
 *
 * This file is part of MPC-HC.
 *
 * MPC-HC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-HC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "stdafx.h"
#include "mplayerc.h"
#include "MainFrm.h"

#include <math.h>
#include <algorithm>

#include <afxpriv.h>
#include <atlconv.h>
#include <atlrx.h>
#include <atlsync.h>

#include "SysVersion.h"
#include "WinAPIUtils.h"
#include "OpenFileDlg.h"
#include "OpenDlg.h"
#include "SaveDlg.h"
#include "GoToDlg.h"
#include "PnSPresetsDlg.h"
#include "MediaTypesDlg.h"
#include "SaveTextFileDialog.h"
#include "SaveSubtitlesFileDialog.h"
#include "SaveThumbnailsDialog.h"
#include "FavoriteAddDlg.h"
#include "FavoriteOrganizeDlg.h"
#include "ShaderCombineDlg.h"
#include "FullscreenWnd.h"
#include "TunerScanDlg.h"
#include "OpenDirHelper.h"
#include "SubtitleDlDlg.h"
#include "ISDb.h"
#include "UpdateChecker.h"
#include "UpdateCheckerDlg.h"

#include "../DeCSS/VobFile.h"

#include "BaseClasses/mtype.h"
#include <Mpconfig.h>
#include <ks.h>
#include <ksmedia.h>
#include <dvdevcod.h>
#include <dsound.h>

#include <InitGuid.h>
#include <uuids.h>
#include "moreuuids.h"
#include <qnetwork.h>
#include <psapi.h>

#include "DSUtil.h"
#include "text.h"
#include "FGManager.h"
#include "FGManagerBDA.h"

#include "TextPassThruFilter.h"
#include "../filters/Filters.h"
#include "../filters/PinInfoWnd.h"

#include "AllocatorCommon7.h"
#include "AllocatorCommon.h"
#include "SyncAllocatorPresenter.h"

#include "ComPropertySheet.h"
#include "LcdSupport.h"
#include "SettingsDefines.h"

#include "IPinHook.h"

#include <comdef.h>
#include "MPCPngImage.h"
#include "DSMPropertyBag.h"

#define DEFCLIENTW 292
#define DEFCLIENTH 200

#define MENUBARBREAK 30

static UINT s_uTaskbarRestart = RegisterWindowMessage(_T("TaskbarCreated"));
static UINT WM_NOTIFYICON = RegisterWindowMessage(_T("MYWM_NOTIFYICON"));
static UINT s_uTBBC = RegisterWindowMessage(_T("TaskbarButtonCreated"));

#include "../filters/transform/VSFilter/IDirectVobSub.h"

#include "Monitors.h"
#include "MultiMonitor.h"

#ifdef USE_MEDIAINFO_STATIC
#include "MediaInfo/MediaInfo.h"
using namespace MediaInfoLib;
#else
#include "MediaInfoDLL.h"
using namespace MediaInfoDLL;
#endif

DWORD last_run = 0;
UINT flast_nID = 0;
bool b_firstPlay = false;

class CSubClock : public CUnknown, public ISubClock
{
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv) {
        return
            QI(ISubClock)
            CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }

    REFERENCE_TIME m_rt;

public:
    CSubClock() : CUnknown(NAME("CSubClock"), NULL) {
        m_rt = 0;
    }

    DECLARE_IUNKNOWN;

    // ISubClock
    STDMETHODIMP SetTime(REFERENCE_TIME rt) {
        m_rt = rt;
        return S_OK;
    }
    STDMETHODIMP_(REFERENCE_TIME) GetTime() {
        return m_rt;
    }
};

/////////////////////////////////////////////////////////////////////////////
// CMainFrame

IMPLEMENT_DYNAMIC(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
    ON_WM_CREATE()
    ON_WM_DESTROY()
    ON_WM_CLOSE()

    ON_REGISTERED_MESSAGE(s_uTaskbarRestart, OnTaskBarRestart)
    ON_REGISTERED_MESSAGE(WM_NOTIFYICON, OnNotifyIcon)

    ON_REGISTERED_MESSAGE(s_uTBBC, OnTaskBarThumbnailsCreate)

    ON_WM_SETFOCUS()
    ON_WM_GETMINMAXINFO()
    ON_WM_MOVE()
    ON_WM_MOVING()
    ON_WM_SIZE()
    ON_WM_SIZING()
    ON_MESSAGE_VOID(WM_DISPLAYCHANGE, OnDisplayChange)

    ON_WM_SYSCOMMAND()
    ON_WM_ACTIVATEAPP()
    ON_MESSAGE(WM_APPCOMMAND, OnAppCommand)
    ON_WM_INPUT()
    ON_MESSAGE(WM_HOTKEY, OnHotKey)

    ON_WM_TIMER()

    ON_MESSAGE(WM_GRAPHNOTIFY, OnGraphNotify)
    ON_MESSAGE(WM_RESET_DEVICE, OnResetDevice)
    ON_MESSAGE(WM_REARRANGERENDERLESS, OnRepaintRenderLess)
    ON_MESSAGE(WM_RESUMEFROMSTATE, OnResumeFromState)

    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_MBUTTONDOWN()
    ON_WM_MBUTTONUP()
    ON_WM_MBUTTONDBLCLK()
    ON_WM_RBUTTONDOWN()
    ON_WM_RBUTTONUP()
    ON_WM_RBUTTONDBLCLK()
    ON_MESSAGE(WM_XBUTTONDOWN, OnXButtonDown)
    ON_MESSAGE(WM_XBUTTONUP, OnXButtonUp)
    ON_MESSAGE(WM_XBUTTONDBLCLK, OnXButtonDblClk)
    ON_WM_MOUSEWHEEL()
    ON_WM_MOUSEMOVE()

    ON_WM_NCHITTEST()

    ON_WM_HSCROLL()

    ON_WM_INITMENU()
    ON_WM_INITMENUPOPUP()
    ON_WM_UNINITMENUPOPUP()

    ON_COMMAND(ID_MENU_PLAYER_SHORT, OnMenuPlayerShort)
    ON_COMMAND(ID_MENU_PLAYER_LONG, OnMenuPlayerLong)
    ON_COMMAND(ID_MENU_FILTERS, OnMenuFilters)

    ON_UPDATE_COMMAND_UI(IDC_PLAYERSTATUS, OnUpdatePlayerStatus)

    ON_COMMAND(ID_FILE_POST_OPENMEDIA, OnFilePostOpenmedia)
    ON_UPDATE_COMMAND_UI(ID_FILE_POST_OPENMEDIA, OnUpdateFilePostOpenmedia)
    ON_COMMAND(ID_FILE_POST_CLOSEMEDIA, OnFilePostClosemedia)
    ON_UPDATE_COMMAND_UI(ID_FILE_POST_CLOSEMEDIA, OnUpdateFilePostClosemedia)

    ON_COMMAND(ID_BOSS, OnBossKey)

    ON_COMMAND_RANGE(ID_STREAM_AUDIO_NEXT, ID_STREAM_AUDIO_PREV, OnStreamAudio)
    ON_COMMAND_RANGE(ID_STREAM_SUB_NEXT, ID_STREAM_SUB_PREV, OnStreamSub)
    ON_COMMAND(ID_STREAM_SUB_ONOFF, OnStreamSubOnOff)
    ON_COMMAND_RANGE(ID_OGM_AUDIO_NEXT, ID_OGM_AUDIO_PREV, OnOgmAudio)
    ON_COMMAND_RANGE(ID_OGM_SUB_NEXT, ID_OGM_SUB_PREV, OnOgmSub)
    ON_COMMAND_RANGE(ID_DVD_ANGLE_NEXT, ID_DVD_ANGLE_PREV, OnDvdAngle)
    ON_COMMAND_RANGE(ID_DVD_AUDIO_NEXT, ID_DVD_AUDIO_PREV, OnDvdAudio)
    ON_COMMAND_RANGE(ID_DVD_SUB_NEXT, ID_DVD_SUB_PREV, OnDvdSub)
    ON_COMMAND(ID_DVD_SUB_ONOFF, OnDvdSubOnOff)


    ON_COMMAND(ID_FILE_OPENQUICK, OnFileOpenQuick)
    ON_UPDATE_COMMAND_UI(ID_FILE_OPENMEDIA, OnUpdateFileOpen)
    ON_COMMAND(ID_FILE_OPENMEDIA, OnFileOpenmedia)
    ON_UPDATE_COMMAND_UI(ID_FILE_OPENMEDIA, OnUpdateFileOpen)
    ON_WM_COPYDATA()
    ON_COMMAND(ID_FILE_OPENDVDBD, OnFileOpendvd)
    ON_UPDATE_COMMAND_UI(ID_FILE_OPENDVDBD, OnUpdateFileOpen)
    ON_COMMAND(ID_FILE_OPENDEVICE, OnFileOpendevice)
    ON_UPDATE_COMMAND_UI(ID_FILE_OPENDEVICE, OnUpdateFileOpen)
    ON_COMMAND_RANGE(ID_FILE_OPEN_CD_START, ID_FILE_OPEN_CD_END, OnFileOpenCD)
    ON_UPDATE_COMMAND_UI_RANGE(ID_FILE_OPEN_CD_START, ID_FILE_OPEN_CD_END, OnUpdateFileOpen)
    ON_COMMAND(ID_FILE_REOPEN, OnFileReopen)
    ON_WM_DROPFILES()
    ON_COMMAND(ID_FILE_SAVE_COPY, OnFileSaveAs)
    ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_COPY, OnUpdateFileSaveAs)
    ON_COMMAND(ID_FILE_SAVE_IMAGE, OnFileSaveImage)
    ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_IMAGE, OnUpdateFileSaveImage)
    ON_COMMAND(ID_FILE_SAVE_IMAGE_AUTO, OnFileSaveImageAuto)
    ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_IMAGE_AUTO, OnUpdateFileSaveImage)
    ON_COMMAND(ID_FILE_SAVE_THUMBNAILS, OnFileSaveThumbnails)
    ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_THUMBNAILS, OnUpdateFileSaveThumbnails)
    ON_COMMAND(ID_FILE_LOAD_SUBTITLE, OnFileLoadsubtitle)
    ON_UPDATE_COMMAND_UI(ID_FILE_LOAD_SUBTITLE, OnUpdateFileLoadsubtitle)
    ON_COMMAND(ID_FILE_SAVE_SUBTITLE, OnFileSavesubtitle)
    ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_SUBTITLE, OnUpdateFileSavesubtitle)
    ON_COMMAND(ID_FILE_ISDB_SEARCH, OnFileISDBSearch)
    ON_UPDATE_COMMAND_UI(ID_FILE_ISDB_SEARCH, OnUpdateFileISDBSearch)
    ON_COMMAND(ID_FILE_ISDB_UPLOAD, OnFileISDBUpload)
    ON_UPDATE_COMMAND_UI(ID_FILE_ISDB_UPLOAD, OnUpdateFileISDBUpload)
    ON_COMMAND(ID_FILE_ISDB_DOWNLOAD, OnFileISDBDownload)
    ON_UPDATE_COMMAND_UI(ID_FILE_ISDB_DOWNLOAD, OnUpdateFileISDBDownload)
    ON_COMMAND(ID_FILE_PROPERTIES, OnFileProperties)
    ON_UPDATE_COMMAND_UI(ID_FILE_PROPERTIES, OnUpdateFileProperties)
    ON_COMMAND(ID_FILE_CLOSEPLAYLIST, OnFileClosePlaylist)
    ON_UPDATE_COMMAND_UI(ID_FILE_CLOSEPLAYLIST, OnUpdateFileClose)
    ON_COMMAND(ID_FILE_CLOSEMEDIA, OnFileCloseMedia)
    ON_UPDATE_COMMAND_UI(ID_FILE_CLOSEMEDIA, OnUpdateFileClose)

    ON_COMMAND(ID_VIEW_CAPTIONMENU, OnViewCaptionmenu)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CAPTIONMENU, OnUpdateViewCaptionmenu)
    ON_COMMAND_RANGE(ID_VIEW_SEEKER, ID_VIEW_STATUS, OnViewControlBar)
    ON_UPDATE_COMMAND_UI_RANGE(ID_VIEW_SEEKER, ID_VIEW_STATUS, OnUpdateViewControlBar)
    ON_COMMAND(ID_VIEW_SUBRESYNC, OnViewSubresync)
    ON_UPDATE_COMMAND_UI(ID_VIEW_SUBRESYNC, OnUpdateViewSubresync)
    ON_COMMAND(ID_VIEW_PLAYLIST, OnViewPlaylist)
    ON_UPDATE_COMMAND_UI(ID_VIEW_PLAYLIST, OnUpdateViewPlaylist)
    ON_COMMAND(ID_VIEW_EDITLISTEDITOR, OnViewEditListEditor)
    ON_COMMAND(ID_EDL_IN, OnEDLIn)
    ON_UPDATE_COMMAND_UI(ID_EDL_IN, OnUpdateEDLIn)
    ON_COMMAND(ID_EDL_OUT, OnEDLOut)
    ON_UPDATE_COMMAND_UI(ID_EDL_OUT, OnUpdateEDLOut)
    ON_COMMAND(ID_EDL_NEWCLIP, OnEDLNewClip)
    ON_UPDATE_COMMAND_UI(ID_EDL_NEWCLIP, OnUpdateEDLNewClip)
    ON_COMMAND(ID_EDL_SAVE, OnEDLSave)
    ON_UPDATE_COMMAND_UI(ID_EDL_SAVE, OnUpdateEDLSave)
    ON_COMMAND(ID_VIEW_CAPTURE, OnViewCapture)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CAPTURE, OnUpdateViewCapture)
    ON_COMMAND(ID_VIEW_SHADEREDITOR, OnViewShaderEditor)
    ON_UPDATE_COMMAND_UI(ID_VIEW_SHADEREDITOR, OnUpdateViewShaderEditor)
    ON_COMMAND(ID_VIEW_PRESETS_MINIMAL, OnViewMinimal)
    ON_UPDATE_COMMAND_UI(ID_VIEW_PRESETS_MINIMAL, OnUpdateViewMinimal)
    ON_COMMAND(ID_VIEW_PRESETS_COMPACT, OnViewCompact)
    ON_UPDATE_COMMAND_UI(ID_VIEW_PRESETS_COMPACT, OnUpdateViewCompact)
    ON_COMMAND(ID_VIEW_PRESETS_NORMAL, OnViewNormal)
    ON_UPDATE_COMMAND_UI(ID_VIEW_PRESETS_NORMAL, OnUpdateViewNormal)
    ON_COMMAND(ID_VIEW_FULLSCREEN, OnViewFullscreen)
    ON_COMMAND(ID_VIEW_FULLSCREEN_SECONDARY, OnViewFullscreenSecondary)
    ON_UPDATE_COMMAND_UI(ID_VIEW_FULLSCREEN, OnUpdateViewFullscreen)
    ON_COMMAND_RANGE(ID_VIEW_ZOOM_50, ID_VIEW_ZOOM_200, OnViewZoom)
    ON_UPDATE_COMMAND_UI_RANGE(ID_VIEW_ZOOM_50, ID_VIEW_ZOOM_200, OnUpdateViewZoom)
    ON_COMMAND(ID_VIEW_ZOOM_AUTOFIT, OnViewZoomAutoFit)
    ON_UPDATE_COMMAND_UI(ID_VIEW_ZOOM_AUTOFIT, OnUpdateViewZoom)
    ON_COMMAND(ID_VIEW_ZOOM_AUTOFIT_LARGER, OnViewZoomAutoFitLarger)
    ON_UPDATE_COMMAND_UI(ID_VIEW_ZOOM_AUTOFIT_LARGER, OnUpdateViewZoom)
    ON_COMMAND_RANGE(ID_VIEW_VF_HALF, ID_VIEW_VF_ZOOM2, OnViewDefaultVideoFrame)
    ON_UPDATE_COMMAND_UI_RANGE(ID_VIEW_VF_HALF, ID_VIEW_VF_ZOOM2, OnUpdateViewDefaultVideoFrame)
    ON_COMMAND(ID_VIEW_VF_SWITCHZOOM, OnViewSwitchVideoFrame)
    ON_COMMAND(ID_VIEW_VF_KEEPASPECTRATIO, OnViewKeepaspectratio)
    ON_UPDATE_COMMAND_UI(ID_VIEW_VF_KEEPASPECTRATIO, OnUpdateViewKeepaspectratio)
    ON_COMMAND(ID_VIEW_VF_COMPMONDESKARDIFF, OnViewCompMonDeskARDiff)
    ON_UPDATE_COMMAND_UI(ID_VIEW_VF_COMPMONDESKARDIFF, OnUpdateViewCompMonDeskARDiff)
    ON_COMMAND_RANGE(ID_VIEW_RESET, ID_PANSCAN_CENTER, OnViewPanNScan)
    ON_UPDATE_COMMAND_UI_RANGE(ID_VIEW_RESET, ID_PANSCAN_CENTER, OnUpdateViewPanNScan)
    ON_COMMAND_RANGE(ID_PANNSCAN_PRESETS_START, ID_PANNSCAN_PRESETS_END, OnViewPanNScanPresets)
    ON_UPDATE_COMMAND_UI_RANGE(ID_PANNSCAN_PRESETS_START, ID_PANNSCAN_PRESETS_END, OnUpdateViewPanNScanPresets)
    ON_COMMAND_RANGE(ID_PANSCAN_ROTATEXP, ID_PANSCAN_ROTATEZM, OnViewRotate)
    ON_UPDATE_COMMAND_UI_RANGE(ID_PANSCAN_ROTATEXP, ID_PANSCAN_ROTATEZM, OnUpdateViewRotate)
    ON_COMMAND_RANGE(ID_ASPECTRATIO_START, ID_ASPECTRATIO_END, OnViewAspectRatio)
    ON_UPDATE_COMMAND_UI_RANGE(ID_ASPECTRATIO_START, ID_ASPECTRATIO_END, OnUpdateViewAspectRatio)
    ON_COMMAND(ID_ASPECTRATIO_NEXT, OnViewAspectRatioNext)
    ON_COMMAND_RANGE(ID_ONTOP_NEVER, ID_ONTOP_WHILEPLAYINGVIDEO, OnViewOntop)
    ON_UPDATE_COMMAND_UI_RANGE(ID_ONTOP_NEVER, ID_ONTOP_WHILEPLAYINGVIDEO, OnUpdateViewOntop)
    ON_COMMAND(ID_VIEW_OPTIONS, OnViewOptions)

    // Casimir666
    ON_UPDATE_COMMAND_UI(ID_VIEW_TEARING_TEST, OnUpdateViewTearingTest)
    ON_COMMAND(ID_VIEW_TEARING_TEST, OnViewTearingTest)
    ON_UPDATE_COMMAND_UI(ID_VIEW_DISPLAYSTATS, OnUpdateViewDisplayStats)
    ON_COMMAND(ID_VIEW_RESETSTATS, OnViewResetStats)
    ON_COMMAND(ID_VIEW_DISPLAYSTATS, OnViewDisplayStatsSC)
    ON_UPDATE_COMMAND_UI(ID_VIEW_FULLSCREENGUISUPPORT, OnUpdateViewFullscreenGUISupport)
    ON_UPDATE_COMMAND_UI(ID_VIEW_HIGHCOLORRESOLUTION, OnUpdateViewHighColorResolution)
    ON_UPDATE_COMMAND_UI(ID_VIEW_FORCEINPUTHIGHCOLORRESOLUTION, OnUpdateViewForceInputHighColorResolution)
    ON_UPDATE_COMMAND_UI(ID_VIEW_FULLFLOATINGPOINTPROCESSING, OnUpdateViewFullFloatingPointProcessing)
    ON_UPDATE_COMMAND_UI(ID_VIEW_HALFFLOATINGPOINTPROCESSING, OnUpdateViewHalfFloatingPointProcessing)
    ON_UPDATE_COMMAND_UI(ID_VIEW_ENABLEFRAMETIMECORRECTION, OnUpdateViewEnableFrameTimeCorrection)
    ON_UPDATE_COMMAND_UI(ID_VIEW_VSYNC, OnUpdateViewVSync)
    ON_UPDATE_COMMAND_UI(ID_VIEW_VSYNCOFFSET, OnUpdateViewVSyncOffset)
    ON_UPDATE_COMMAND_UI(ID_VIEW_VSYNCACCURATE, OnUpdateViewVSyncAccurate)

    ON_UPDATE_COMMAND_UI(ID_VIEW_SYNCHRONIZEVIDEO, OnUpdateViewSynchronizeVideo)
    ON_UPDATE_COMMAND_UI(ID_VIEW_SYNCHRONIZEDISPLAY, OnUpdateViewSynchronizeDisplay)
    ON_UPDATE_COMMAND_UI(ID_VIEW_SYNCHRONIZENEAREST, OnUpdateViewSynchronizeNearest)

    ON_UPDATE_COMMAND_UI(ID_VIEW_CM_ENABLE, OnUpdateViewColorManagementEnable)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CM_INPUT_AUTO, OnUpdateViewColorManagementInput)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CM_INPUT_HDTV, OnUpdateViewColorManagementInput)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CM_INPUT_SDTV_NTSC, OnUpdateViewColorManagementInput)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CM_INPUT_SDTV_PAL, OnUpdateViewColorManagementInput)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CM_AMBIENTLIGHT_BRIGHT, OnUpdateViewColorManagementAmbientLight)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CM_AMBIENTLIGHT_DIM, OnUpdateViewColorManagementAmbientLight)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CM_AMBIENTLIGHT_DARK, OnUpdateViewColorManagementAmbientLight)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CM_INTENT_PERCEPTUAL, OnUpdateViewColorManagementIntent)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CM_INTENT_RELATIVECOLORIMETRIC, OnUpdateViewColorManagementIntent)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CM_INTENT_SATURATION, OnUpdateViewColorManagementIntent)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CM_INTENT_ABSOLUTECOLORIMETRIC, OnUpdateViewColorManagementIntent)

    ON_UPDATE_COMMAND_UI(ID_VIEW_EVROUTPUTRANGE_0_255, OnUpdateViewEVROutputRange)
    ON_UPDATE_COMMAND_UI(ID_VIEW_EVROUTPUTRANGE_16_235, OnUpdateViewEVROutputRange)

    ON_UPDATE_COMMAND_UI(ID_VIEW_FLUSHGPU_BEFOREVSYNC, OnUpdateViewFlushGPU)
    ON_UPDATE_COMMAND_UI(ID_VIEW_FLUSHGPU_AFTERPRESENT, OnUpdateViewFlushGPU)
    ON_UPDATE_COMMAND_UI(ID_VIEW_FLUSHGPU_WAIT, OnUpdateViewFlushGPU)

    ON_UPDATE_COMMAND_UI(ID_VIEW_D3DFULLSCREEN, OnUpdateViewD3DFullscreen)
    ON_UPDATE_COMMAND_UI(ID_VIEW_DISABLEDESKTOPCOMPOSITION, OnUpdateViewDisableDesktopComposition)
    ON_UPDATE_COMMAND_UI(ID_VIEW_ALTERNATIVEVSYNC, OnUpdateViewAlternativeVSync)


    ON_UPDATE_COMMAND_UI(ID_VIEW_VSYNCOFFSET_INCREASE, OnUpdateViewVSyncOffsetIncrease)
    ON_UPDATE_COMMAND_UI(ID_VIEW_VSYNCOFFSET_DECREASE, OnUpdateViewVSyncOffsetDecrease)
    ON_COMMAND(ID_VIEW_FULLSCREENGUISUPPORT, OnViewFullscreenGUISupport)
    ON_COMMAND(ID_VIEW_HIGHCOLORRESOLUTION, OnViewHighColorResolution)
    ON_COMMAND(ID_VIEW_FORCEINPUTHIGHCOLORRESOLUTION, OnViewForceInputHighColorResolution)
    ON_COMMAND(ID_VIEW_FULLFLOATINGPOINTPROCESSING, OnViewFullFloatingPointProcessing)
    ON_COMMAND(ID_VIEW_HALFFLOATINGPOINTPROCESSING, OnViewHalfFloatingPointProcessing)
    ON_COMMAND(ID_VIEW_ENABLEFRAMETIMECORRECTION, OnViewEnableFrameTimeCorrection)
    ON_COMMAND(ID_VIEW_VSYNC, OnViewVSync)
    ON_COMMAND(ID_VIEW_VSYNCACCURATE, OnViewVSyncAccurate)

    ON_COMMAND(ID_VIEW_SYNCHRONIZEVIDEO, OnViewSynchronizeVideo)
    ON_COMMAND(ID_VIEW_SYNCHRONIZEDISPLAY, OnViewSynchronizeDisplay)
    ON_COMMAND(ID_VIEW_SYNCHRONIZENEAREST, OnViewSynchronizeNearest)

    ON_COMMAND(ID_VIEW_CM_ENABLE, OnViewColorManagementEnable)
    ON_COMMAND(ID_VIEW_CM_INPUT_AUTO, OnViewColorManagementInputAuto)
    ON_COMMAND(ID_VIEW_CM_INPUT_HDTV, OnViewColorManagementInputHDTV)
    ON_COMMAND(ID_VIEW_CM_INPUT_SDTV_NTSC, OnViewColorManagementInputSDTV_NTSC)
    ON_COMMAND(ID_VIEW_CM_INPUT_SDTV_PAL, OnViewColorManagementInputSDTV_PAL)
    ON_COMMAND(ID_VIEW_CM_AMBIENTLIGHT_BRIGHT, OnViewColorManagementAmbientLightBright)
    ON_COMMAND(ID_VIEW_CM_AMBIENTLIGHT_DIM, OnViewColorManagementAmbientLightDim)
    ON_COMMAND(ID_VIEW_CM_AMBIENTLIGHT_DARK, OnViewColorManagementAmbientLightDark)
    ON_COMMAND(ID_VIEW_CM_INTENT_PERCEPTUAL, OnViewColorManagementIntentPerceptual)
    ON_COMMAND(ID_VIEW_CM_INTENT_RELATIVECOLORIMETRIC, OnViewColorManagementIntentRelativeColorimetric)
    ON_COMMAND(ID_VIEW_CM_INTENT_SATURATION, OnViewColorManagementIntentSaturation)
    ON_COMMAND(ID_VIEW_CM_INTENT_ABSOLUTECOLORIMETRIC, OnViewColorManagementIntentAbsoluteColorimetric)

    ON_COMMAND(ID_VIEW_EVROUTPUTRANGE_0_255, OnViewEVROutputRange_0_255)
    ON_COMMAND(ID_VIEW_EVROUTPUTRANGE_16_235, OnViewEVROutputRange_16_235)

    ON_COMMAND(ID_VIEW_FLUSHGPU_BEFOREVSYNC, OnViewFlushGPUBeforeVSync)
    ON_COMMAND(ID_VIEW_FLUSHGPU_AFTERPRESENT, OnViewFlushGPUAfterVSync)
    ON_COMMAND(ID_VIEW_FLUSHGPU_WAIT, OnViewFlushGPUWait)

    ON_COMMAND(ID_VIEW_D3DFULLSCREEN, OnViewD3DFullScreen)
    ON_COMMAND(ID_VIEW_DISABLEDESKTOPCOMPOSITION, OnViewDisableDesktopComposition)
    ON_COMMAND(ID_VIEW_ALTERNATIVEVSYNC, OnViewAlternativeVSync)
    ON_COMMAND(ID_VIEW_RESET_DEFAULT, OnViewResetDefault)
    ON_COMMAND(ID_VIEW_RESET_OPTIMAL, OnViewResetOptimal)

    ON_COMMAND(ID_VIEW_VSYNCOFFSET_INCREASE, OnViewVSyncOffsetIncrease)
    ON_COMMAND(ID_VIEW_VSYNCOFFSET_DECREASE, OnViewVSyncOffsetDecrease)
    ON_UPDATE_COMMAND_UI(ID_SHADERS_TOGGLE, OnUpdateShaderToggle)
    ON_COMMAND(ID_SHADERS_TOGGLE, OnShaderToggle)
    ON_UPDATE_COMMAND_UI(ID_SHADERS_TOGGLE_SCREENSPACE, OnUpdateShaderToggleScreenSpace)
    ON_COMMAND(ID_SHADERS_TOGGLE_SCREENSPACE, OnShaderToggleScreenSpace)
    ON_UPDATE_COMMAND_UI(ID_VIEW_REMAINING_TIME, OnUpdateViewRemainingTime)
    ON_COMMAND(ID_VIEW_REMAINING_TIME, OnViewRemainingTime)
    ON_COMMAND(ID_D3DFULLSCREEN_TOGGLE, OnD3DFullscreenToggle)
    ON_COMMAND_RANGE(ID_GOTO_PREV_SUB, ID_GOTO_NEXT_SUB, OnGotoSubtitle)
    ON_COMMAND_RANGE(ID_SHIFT_SUB_DOWN, ID_SHIFT_SUB_UP, OnShiftSubtitle)
    ON_COMMAND_RANGE(ID_SUB_DELAY_DOWN, ID_SUB_DELAY_UP, OnSubtitleDelay)
    ON_COMMAND_RANGE(ID_LANGUAGE_ENGLISH, ID_LANGUAGE_LAST, OnLanguage)

    ON_COMMAND(ID_PLAY_PLAY, OnPlayPlay)
    ON_COMMAND(ID_PLAY_PAUSE, OnPlayPause)
    ON_COMMAND(ID_PLAY_PLAYPAUSE, OnPlayPlaypause)
    ON_COMMAND(ID_PLAY_STOP, OnPlayStop)
    ON_UPDATE_COMMAND_UI(ID_PLAY_PLAY, OnUpdatePlayPauseStop)
    ON_UPDATE_COMMAND_UI(ID_PLAY_PAUSE, OnUpdatePlayPauseStop)
    ON_UPDATE_COMMAND_UI(ID_PLAY_PLAYPAUSE, OnUpdatePlayPauseStop)
    ON_UPDATE_COMMAND_UI(ID_PLAY_STOP, OnUpdatePlayPauseStop)
    ON_COMMAND_RANGE(ID_PLAY_FRAMESTEP, ID_PLAY_FRAMESTEPCANCEL, OnPlayFramestep)
    ON_UPDATE_COMMAND_UI_RANGE(ID_PLAY_FRAMESTEP, ID_PLAY_FRAMESTEPCANCEL, OnUpdatePlayFramestep)
    ON_COMMAND_RANGE(ID_PLAY_SEEKBACKWARDSMALL, ID_PLAY_SEEKFORWARDLARGE, OnPlaySeek)
    ON_COMMAND_RANGE(ID_PLAY_SEEKKEYBACKWARD, ID_PLAY_SEEKKEYFORWARD, OnPlaySeekKey)
    ON_UPDATE_COMMAND_UI_RANGE(ID_PLAY_SEEKBACKWARDSMALL, ID_PLAY_SEEKFORWARDLARGE, OnUpdatePlaySeek)
    ON_UPDATE_COMMAND_UI_RANGE(ID_PLAY_SEEKKEYBACKWARD, ID_PLAY_SEEKKEYFORWARD, OnUpdatePlaySeek)
    ON_COMMAND(ID_PLAY_GOTO, OnPlayGoto)
    ON_UPDATE_COMMAND_UI(ID_PLAY_GOTO, OnUpdateGoto)
    ON_COMMAND_RANGE(ID_PLAY_DECRATE, ID_PLAY_INCRATE, OnPlayChangeRate)
    ON_UPDATE_COMMAND_UI_RANGE(ID_PLAY_DECRATE, ID_PLAY_INCRATE, OnUpdatePlayChangeRate)
    ON_COMMAND(ID_PLAY_RESETRATE, OnPlayResetRate)
    ON_UPDATE_COMMAND_UI(ID_PLAY_RESETRATE, OnUpdatePlayResetRate)
    ON_COMMAND_RANGE(ID_PLAY_INCAUDDELAY, ID_PLAY_DECAUDDELAY, OnPlayChangeAudDelay)
    ON_UPDATE_COMMAND_UI_RANGE(ID_PLAY_INCAUDDELAY, ID_PLAY_DECAUDDELAY, OnUpdatePlayChangeAudDelay)
    ON_COMMAND_RANGE(ID_FILTERS_SUBITEM_START, ID_FILTERS_SUBITEM_END, OnPlayFilters)
    ON_UPDATE_COMMAND_UI_RANGE(ID_FILTERS_SUBITEM_START, ID_FILTERS_SUBITEM_END, OnUpdatePlayFilters)
    ON_COMMAND_RANGE(ID_SHADERS_START, ID_SHADERS_END, OnPlayShaders)
    ON_COMMAND_RANGE(ID_AUDIO_SUBITEM_START, ID_AUDIO_SUBITEM_END, OnPlayAudio)
    ON_COMMAND_RANGE(ID_SUBTITLES_SUBITEM_START, ID_SUBTITLES_SUBITEM_END, OnPlaySubtitles)
    ON_COMMAND_RANGE(ID_FILTERSTREAMS_SUBITEM_START, ID_FILTERSTREAMS_SUBITEM_END, OnPlayFiltersStreams)
    ON_UPDATE_COMMAND_UI_RANGE(ID_FILTERSTREAMS_SUBITEM_START, ID_FILTERSTREAMS_SUBITEM_END, OnUpdatePlayFiltersStreams)
    ON_COMMAND_RANGE(ID_VOLUME_UP, ID_VOLUME_MUTE, OnPlayVolume)
    ON_COMMAND_RANGE(ID_VOLUME_BOOST_INC, ID_VOLUME_BOOST_MAX, OnPlayVolumeBoost)
    ON_UPDATE_COMMAND_UI_RANGE(ID_VOLUME_BOOST_INC, ID_VOLUME_BOOST_MAX, OnUpdatePlayVolumeBoost)
    ON_COMMAND(ID_CUSTOM_CHANNEL_MAPPING, OnCustomChannelMapping)
    ON_UPDATE_COMMAND_UI(ID_CUSTOM_CHANNEL_MAPPING, OnUpdateCustomChannelMapping)
    ON_COMMAND_RANGE(ID_NORMALIZE, ID_REGAIN_VOLUME, OnNormalizeRegainVolume)
    ON_UPDATE_COMMAND_UI_RANGE(ID_NORMALIZE, ID_REGAIN_VOLUME, OnUpdateNormalizeRegainVolume)
    ON_COMMAND_RANGE(ID_COLOR_BRIGHTNESS_INC, ID_COLOR_RESET, OnPlayColor)
    ON_COMMAND_RANGE(ID_AFTERPLAYBACK_CLOSE, ID_AFTERPLAYBACK_DONOTHING, OnAfterplayback)
    ON_UPDATE_COMMAND_UI_RANGE(ID_AFTERPLAYBACK_CLOSE, ID_AFTERPLAYBACK_DONOTHING, OnUpdateAfterplayback)
    ON_COMMAND_RANGE(ID_AFTERPLAYBACK_EXIT, ID_AFTERPLAYBACK_NEXT, OnAfterplayback)
    ON_UPDATE_COMMAND_UI_RANGE(ID_AFTERPLAYBACK_EXIT, ID_AFTERPLAYBACK_NEXT, OnUpdateAfterplayback)

    ON_COMMAND_RANGE(ID_NAVIGATE_SKIPBACK, ID_NAVIGATE_SKIPFORWARD, OnNavigateSkip)
    ON_UPDATE_COMMAND_UI_RANGE(ID_NAVIGATE_SKIPBACK, ID_NAVIGATE_SKIPFORWARD, OnUpdateNavigateSkip)
    ON_COMMAND_RANGE(ID_NAVIGATE_SKIPBACKFILE, ID_NAVIGATE_SKIPFORWARDFILE, OnNavigateSkipFile)
    ON_UPDATE_COMMAND_UI_RANGE(ID_NAVIGATE_SKIPBACKFILE, ID_NAVIGATE_SKIPFORWARDFILE, OnUpdateNavigateSkipFile)
    ON_COMMAND_RANGE(ID_NAVIGATE_TITLEMENU, ID_NAVIGATE_CHAPTERMENU, OnNavigateMenu)
    ON_UPDATE_COMMAND_UI_RANGE(ID_NAVIGATE_TITLEMENU, ID_NAVIGATE_CHAPTERMENU, OnUpdateNavigateMenu)
    ON_COMMAND_RANGE(ID_NAVIGATE_AUDIO_SUBITEM_START, ID_NAVIGATE_AUDIO_SUBITEM_END, OnNavigateAudio)
    ON_COMMAND_RANGE(ID_NAVIGATE_SUBP_SUBITEM_START, ID_NAVIGATE_SUBP_SUBITEM_END, OnNavigateSubpic)
    ON_COMMAND_RANGE(ID_NAVIGATE_ANGLE_SUBITEM_START, ID_NAVIGATE_ANGLE_SUBITEM_END, OnNavigateAngle)
    ON_COMMAND_RANGE(ID_NAVIGATE_CHAP_SUBITEM_START, ID_NAVIGATE_CHAP_SUBITEM_END, OnNavigateChapters)
    ON_COMMAND_RANGE(ID_NAVIGATE_MENU_LEFT, ID_NAVIGATE_MENU_LEAVE, OnNavigateMenuItem)
    ON_UPDATE_COMMAND_UI_RANGE(ID_NAVIGATE_MENU_LEFT, ID_NAVIGATE_MENU_LEAVE, OnUpdateNavigateMenuItem)
    ON_COMMAND(ID_NAVIGATE_TUNERSCAN, OnTunerScan)
    ON_UPDATE_COMMAND_UI(ID_NAVIGATE_TUNERSCAN, OnUpdateTunerScan)

    ON_COMMAND(ID_FAVORITES_ADD, OnFavoritesAdd)
    ON_UPDATE_COMMAND_UI(ID_FAVORITES_ADD, OnUpdateFavoritesAdd)
    ON_COMMAND(ID_FAVORITES_QUICKADDFAVORITE, OnFavoritesQuickAddFavorite)
    ON_COMMAND(ID_FAVORITES_ORGANIZE, OnFavoritesOrganize)
    ON_UPDATE_COMMAND_UI(ID_FAVORITES_ORGANIZE, OnUpdateFavoritesOrganize)
    ON_COMMAND_RANGE(ID_FAVORITES_FILE_START, ID_FAVORITES_FILE_END, OnFavoritesFile)
    ON_UPDATE_COMMAND_UI_RANGE(ID_FAVORITES_FILE_START, ID_FAVORITES_FILE_END, OnUpdateFavoritesFile)
    ON_COMMAND_RANGE(ID_FAVORITES_DVD_START, ID_FAVORITES_DVD_END, OnFavoritesDVD)
    ON_UPDATE_COMMAND_UI_RANGE(ID_FAVORITES_DVD_START, ID_FAVORITES_DVD_END, OnUpdateFavoritesDVD)
    ON_COMMAND_RANGE(ID_FAVORITES_DEVICE_START, ID_FAVORITES_DEVICE_END, OnFavoritesDevice)
    ON_UPDATE_COMMAND_UI_RANGE(ID_FAVORITES_DEVICE_START, ID_FAVORITES_DEVICE_END, OnUpdateFavoritesDevice)

    ON_COMMAND(ID_RECENT_FILES_CLEAR, OnRecentFileClear)
    ON_UPDATE_COMMAND_UI(ID_RECENT_FILES_CLEAR, OnUpdateRecentFileClear)
    ON_COMMAND_RANGE(ID_RECENT_FILE_START, ID_RECENT_FILE_END, OnRecentFile)
    ON_UPDATE_COMMAND_UI_RANGE(ID_RECENT_FILE_START, ID_RECENT_FILE_END, OnUpdateRecentFile)

    ON_COMMAND(ID_HELP_HOMEPAGE, OnHelpHomepage)
    ON_COMMAND(ID_HELP_CHECKFORUPDATE, OnHelpCheckForUpdate)
    //ON_COMMAND(ID_HELP_DOCUMENTATION, OnHelpDocumentation)
    ON_COMMAND(ID_HELP_TOOLBARIMAGES, OnHelpToolbarImages)
    ON_COMMAND(ID_HELP_DONATE, OnHelpDonate)

    // Open Dir incl. SubDir
    ON_COMMAND(ID_FILE_OPENDIRECTORY, OnFileOpendirectory)
    ON_UPDATE_COMMAND_UI(ID_FILE_OPENDIRECTORY, OnUpdateFileOpen)
    ON_WM_POWERBROADCAST()

    // Navigation panel
    ON_COMMAND(ID_VIEW_NAVIGATION, OnViewNavigation)
    ON_UPDATE_COMMAND_UI(ID_VIEW_NAVIGATION, OnUpdateViewNavigation)

    ON_WM_WTSSESSION_CHANGE()
END_MESSAGE_MAP()

#ifdef _DEBUG
const TCHAR* GetEventString(LONG evCode)
{
#define UNPACK_VALUE(VALUE) case VALUE: return _T(#VALUE);
    switch (evCode) {
            UNPACK_VALUE(EC_COMPLETE);
            UNPACK_VALUE(EC_USERABORT);
            UNPACK_VALUE(EC_ERRORABORT);
            UNPACK_VALUE(EC_TIME);
            UNPACK_VALUE(EC_REPAINT);
            UNPACK_VALUE(EC_STREAM_ERROR_STOPPED);
            UNPACK_VALUE(EC_STREAM_ERROR_STILLPLAYING);
            UNPACK_VALUE(EC_ERROR_STILLPLAYING);
            UNPACK_VALUE(EC_PALETTE_CHANGED);
            UNPACK_VALUE(EC_VIDEO_SIZE_CHANGED);
            UNPACK_VALUE(EC_QUALITY_CHANGE);
            UNPACK_VALUE(EC_SHUTTING_DOWN);
            UNPACK_VALUE(EC_CLOCK_CHANGED);
            UNPACK_VALUE(EC_PAUSED);
            UNPACK_VALUE(EC_OPENING_FILE);
            UNPACK_VALUE(EC_BUFFERING_DATA);
            UNPACK_VALUE(EC_FULLSCREEN_LOST);
            UNPACK_VALUE(EC_ACTIVATE);
            UNPACK_VALUE(EC_NEED_RESTART);
            UNPACK_VALUE(EC_WINDOW_DESTROYED);
            UNPACK_VALUE(EC_DISPLAY_CHANGED);
            UNPACK_VALUE(EC_STARVATION);
            UNPACK_VALUE(EC_OLE_EVENT);
            UNPACK_VALUE(EC_NOTIFY_WINDOW);
            UNPACK_VALUE(EC_STREAM_CONTROL_STOPPED);
            UNPACK_VALUE(EC_STREAM_CONTROL_STARTED);
            UNPACK_VALUE(EC_END_OF_SEGMENT);
            UNPACK_VALUE(EC_SEGMENT_STARTED);
            UNPACK_VALUE(EC_LENGTH_CHANGED);
            UNPACK_VALUE(EC_DEVICE_LOST);
            UNPACK_VALUE(EC_SAMPLE_NEEDED);
            UNPACK_VALUE(EC_PROCESSING_LATENCY);
            UNPACK_VALUE(EC_SAMPLE_LATENCY);
            UNPACK_VALUE(EC_SCRUB_TIME);
            UNPACK_VALUE(EC_STEP_COMPLETE);
            UNPACK_VALUE(EC_TIMECODE_AVAILABLE);
            UNPACK_VALUE(EC_EXTDEVICE_MODE_CHANGE);
            UNPACK_VALUE(EC_STATE_CHANGE);
            UNPACK_VALUE(EC_GRAPH_CHANGED);
            UNPACK_VALUE(EC_CLOCK_UNSET);
            UNPACK_VALUE(EC_VMR_RENDERDEVICE_SET);
            UNPACK_VALUE(EC_VMR_SURFACE_FLIPPED);
            UNPACK_VALUE(EC_VMR_RECONNECTION_FAILED);
            UNPACK_VALUE(EC_PREPROCESS_COMPLETE);
            UNPACK_VALUE(EC_CODECAPI_EVENT);
            UNPACK_VALUE(EC_WMT_INDEX_EVENT);
            UNPACK_VALUE(EC_WMT_EVENT);
            UNPACK_VALUE(EC_BUILT);
            UNPACK_VALUE(EC_UNBUILT);
            UNPACK_VALUE(EC_SKIP_FRAMES);
            UNPACK_VALUE(EC_PLEASE_REOPEN);
            UNPACK_VALUE(EC_STATUS);
            UNPACK_VALUE(EC_MARKER_HIT);
            UNPACK_VALUE(EC_LOADSTATUS);
            UNPACK_VALUE(EC_FILE_CLOSED);
            UNPACK_VALUE(EC_ERRORABORTEX);
            //UNPACK_VALUE(EC_NEW_PIN);
            //UNPACK_VALUE(EC_RENDER_FINISHED);
            UNPACK_VALUE(EC_EOS_SOON);
            UNPACK_VALUE(EC_CONTENTPROPERTY_CHANGED);
            UNPACK_VALUE(EC_BANDWIDTHCHANGE);
            UNPACK_VALUE(EC_VIDEOFRAMEREADY);
            UNPACK_VALUE(EC_BG_AUDIO_CHANGED);
            UNPACK_VALUE(EC_BG_ERROR);
    };
#undef UNPACK_VALUE
    return _T("UNKNOWN");
}
#endif

/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction

CMainFrame::CMainFrame()
    : m_iMediaLoadState(MLS_CLOSED)
    , m_iPlaybackMode(PM_NONE)
    , m_dSpeedRate(1.0)
    , m_rtDurationOverride(-1)
    , m_fFullScreen(false)
    , m_fFirstFSAfterLaunchOnFS(false)
    , m_fHideCursor(false)
    , m_lastMouseMove(-1, -1)
    , m_pLastBar(NULL)
    , m_nCS(0)
    , m_nLoops(0)
    , m_posFirstExtSub(NULL)
    , m_iSubtitleSel(-1)
    , m_ZoomX(1)
    , m_ZoomY(1)
    , m_PosX(0.5)
    , m_PosY(0.5)
    , m_AngleX(0)
    , m_AngleY(0)
    , m_AngleZ(0)
    , m_fCustomGraph(false)
    , m_fRealMediaGraph(false)
    , m_fShockwaveGraph(false)
    , m_fQuicktimeGraph(false)
    , m_fFrameSteppingActive(false)
    , m_fEndOfStream(false)
    , m_fCapturing(false)
    , m_fLiveWM(false)
    , m_fOpeningAborted(false)
    , m_fBuffering(false)
    , m_fileDropTarget(this)
    , m_fTrayIcon(false)
    , m_pFullscreenWnd(NULL)
    , m_pVideoWnd(NULL)
    , m_bRemainingTime(false)
    , m_nCurSubtitle(-1)
    , m_lSubtitleShift(0)
    , m_bToggleShader(false)
    , m_nStepForwardCount(0)
    , m_rtStepForwardStart(0)
    , m_bToggleShaderScreenSpace(false)
    , m_bInOptions(false)
    , m_lCurrentChapter(0)
    , m_lChapterStartTime(0xFFFFFFFF)
    , m_pTaskbarList(NULL)
    , m_pGraphThread(NULL)
    , m_bOpenedThruThread(false)
    , m_nMenuHideTick(0)
    , m_bWasSnapped(false)
    , m_nSeekDirection(SEEK_DIRECTION_NONE)
    , m_bIsBDPlay(false)
    , m_LastOpenBDPath(_T(""))
{
    m_Lcd.SetVolumeRange(0, 100);
    m_liLastSaveTime.QuadPart = 0;
    // Don't let CFrameWnd handle automatically the state of the menu items.
    // This means that menu items without handlers won't be automatically
    // disabled but it avoids some unwanted cases where programmatically
    // disabled menu items are always re-enabled by CFrameWnd.
    m_bAutoMenuEnable = FALSE;
}

CMainFrame::~CMainFrame()
{
    //m_owner.DestroyWindow();
    //delete m_pFullscreenWnd; // double delete see CMainFrame::OnDestroy
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (__super::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    m_popup.LoadMenu(IDR_POPUP);
    m_popupmain.LoadMenu(IDR_POPUPMAIN);

    // create a view to occupy the client area of the frame
    if (!m_wndView.Create(NULL, NULL, AFX_WS_DEFAULT_VIEW,
                          CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, NULL)) {
        TRACE(_T("Failed to create view window\n"));
        return -1;
    }
    // Should never be RTLed
    m_wndView.ModifyStyleEx(WS_EX_LAYOUTRTL, WS_EX_NOINHERITLAYOUT);

    // static bars

    BOOL bResult = m_wndStatusBar.Create(this);
    if (bResult) {
        bResult = m_wndStatsBar.Create(this);
    }
    if (bResult) {
        bResult = m_wndInfoBar.Create(this);
    }
    if (bResult) {
        bResult = m_wndToolBar.Create(this);
    }
    if (bResult) {
        bResult = m_wndSeekBar.Create(this);
    }
    if (!bResult) {
        TRACE(_T("Failed to create all control bars\n"));
        return -1;      // fail to create
    }

    m_pFullscreenWnd = DEBUG_NEW CFullscreenWnd(this);

    m_bars.AddTail(&m_wndSeekBar);
    m_bars.AddTail(&m_wndToolBar);
    m_bars.AddTail(&m_wndInfoBar);
    m_bars.AddTail(&m_wndStatsBar);
    m_bars.AddTail(&m_wndStatusBar);

    m_wndSeekBar.Enable(false);

    // dockable bars

    EnableDocking(CBRS_ALIGN_ANY);

    m_dockingbars.RemoveAll();

    m_wndSubresyncBar.Create(this, AFX_IDW_DOCKBAR_TOP, &m_csSubLock);
    m_wndSubresyncBar.SetBarStyle(m_wndSubresyncBar.GetBarStyle() | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC);
    m_wndSubresyncBar.EnableDocking(CBRS_ALIGN_ANY);
    m_wndSubresyncBar.SetHeight(200);
    m_dockingbars.AddTail(&m_wndSubresyncBar);

    m_wndPlaylistBar.Create(this, AFX_IDW_DOCKBAR_BOTTOM);
    m_wndPlaylistBar.SetBarStyle(m_wndPlaylistBar.GetBarStyle() | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC);
    m_wndPlaylistBar.EnableDocking(CBRS_ALIGN_ANY);
    m_wndPlaylistBar.SetHeight(100);
    m_dockingbars.AddTail(&m_wndPlaylistBar);
    m_wndPlaylistBar.LoadPlaylist(GetRecentFile());

    m_wndEditListEditor.Create(this, AFX_IDW_DOCKBAR_RIGHT);
    m_wndEditListEditor.SetBarStyle(m_wndEditListEditor.GetBarStyle() | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC);
    m_wndEditListEditor.EnableDocking(CBRS_ALIGN_ANY);
    m_dockingbars.AddTail(&m_wndEditListEditor);
    m_wndEditListEditor.SetHeight(100);

    m_wndCaptureBar.Create(this, AFX_IDW_DOCKBAR_LEFT);
    m_wndCaptureBar.SetBarStyle(m_wndCaptureBar.GetBarStyle() | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC);
    m_wndCaptureBar.EnableDocking(CBRS_ALIGN_LEFT | CBRS_ALIGN_RIGHT);
    m_dockingbars.AddTail(&m_wndCaptureBar);

    m_wndNavigationBar.Create(this, AFX_IDW_DOCKBAR_LEFT);
    m_wndNavigationBar.SetBarStyle(m_wndNavigationBar.GetBarStyle() | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC);
    m_wndNavigationBar.EnableDocking(CBRS_ALIGN_LEFT | CBRS_ALIGN_RIGHT);
    m_dockingbars.AddTail(&m_wndNavigationBar);

    m_wndShaderEditorBar.Create(this, AFX_IDW_DOCKBAR_TOP);
    m_wndShaderEditorBar.SetBarStyle(m_wndShaderEditorBar.GetBarStyle() | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC);
    m_wndShaderEditorBar.EnableDocking(CBRS_ALIGN_ANY);
    m_dockingbars.AddTail(&m_wndShaderEditorBar);

    // Hide all dockable bars by default
    POSITION pos = m_dockingbars.GetHeadPosition();
    while (pos) {
        m_dockingbars.GetNext(pos)->ShowWindow(SW_HIDE);
    }

    m_fileDropTarget.Register(this);

    const CAppSettings& s = AfxGetAppSettings();

    // Load the controls
    m_nCS = s.nCS;
    ShowControls(m_nCS);

    SetAlwaysOnTop(s.iOnTop);

    ShowTrayIcon(s.fTrayIcon);

    SetFocus();

    m_pGraphThread = (CGraphThread*)AfxBeginThread(RUNTIME_CLASS(CGraphThread));

    if (m_pGraphThread) {
        m_pGraphThread->SetMainFrame(this);
    }

    if (s.nCmdlnWebServerPort != 0) {
        if (s.nCmdlnWebServerPort > 0) {
            StartWebServer(s.nCmdlnWebServerPort);
        } else if (s.fEnableWebServer) {
            StartWebServer(s.nWebServerPort);
        }
    }

    // Casimir666 : reload Shaders
    {
        CString strList = s.strShaderList;
        CString strRes;
        int curPos = 0;

        strRes = strList.Tokenize(_T("|"), curPos);
        while (strRes != _T("")) {
            m_shaderlabels.AddTail(strRes);
            strRes = strList.Tokenize(_T("|"), curPos);
        }
    }
    {
        CString strList = s.strShaderListScreenSpace;
        CString strRes;
        int curPos = 0;

        strRes = strList.Tokenize(_T("|"), curPos);
        while (strRes != _T("")) {
            m_shaderlabelsScreenSpace.AddTail(strRes);
            strRes = strList.Tokenize(_T("|"), curPos);
        }
    }

    m_bToggleShader = s.fToggleShader;
    m_bToggleShaderScreenSpace = s.fToggleShaderScreenSpace;

    m_strTitle.LoadString(IDR_MAINFRAME);

#ifdef MPCHC_LITE
    m_strTitle += _T(" Lite");
#endif

    SetWindowText(m_strTitle);
    m_Lcd.SetMediaTitle(LPCTSTR(m_strTitle));

    m_OpenFile = false;

    WTSRegisterSessionNotification();

    return 0;
}

void CMainFrame::OnDestroy()
{
    WTSUnRegisterSessionNotification();
    ShowTrayIcon(false);
    m_fileDropTarget.Revoke();

    if (m_pGraphThread) {
        CAMEvent e;
        m_pGraphThread->PostThreadMessage(CGraphThread::TM_EXIT, 0, (LPARAM)&e);
        if (!e.Wait(5000)) {
            TRACE(_T("ERROR: Must call TerminateThread() on CMainFrame::m_pGraphThread->m_hThread\n"));
            TerminateThread(m_pGraphThread->m_hThread, (DWORD) - 1);
        }
    }

    if (m_pFullscreenWnd) {
        if (m_pFullscreenWnd->IsWindow()) {
            m_pFullscreenWnd->DestroyWindow();
        }
        delete m_pFullscreenWnd;
    }

    __super::OnDestroy();
}

void CMainFrame::OnClose()
{
    CAppSettings& s = AfxGetAppSettings();
    // Casimir666 : save shaders list
    {
        POSITION pos;
        CString strList = "";

        pos = m_shaderlabels.GetHeadPosition();
        while (pos) {
            strList += m_shaderlabels.GetAt(pos) + "|";
            m_dockingbars.GetNext(pos);
        }
        s.strShaderList = strList;
    }
    {
        POSITION pos;
        CString  strList = "";

        pos = m_shaderlabelsScreenSpace.GetHeadPosition();
        while (pos) {
            strList += m_shaderlabelsScreenSpace.GetAt(pos) + "|";
            m_dockingbars.GetNext(pos);
        }
        s.strShaderListScreenSpace = strList;
    }

    s.fToggleShader = m_bToggleShader;
    s.fToggleShaderScreenSpace = m_bToggleShaderScreenSpace;

    s.dZoomX = m_ZoomX;
    s.dZoomY = m_ZoomY;

    m_wndPlaylistBar.SavePlaylist();

    SaveControlBars();

    ShowWindow(SW_HIDE);

    CloseMedia();

    s.WinLircClient.DisConnect();
    s.UIceClient.DisConnect();

    SendAPICommand(CMD_DISCONNECT, L"\0");  // according to CMD_NOTIFYENDOFSTREAM (ctrl+f it here), you're not supposed to send NULL here
    __super::OnClose();
}

DROPEFFECT CMainFrame::OnDragEnter(COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
{
    return DROPEFFECT_NONE;
}

DROPEFFECT CMainFrame::OnDragOver(COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
{
    UINT CF_URL = RegisterClipboardFormat(_T("UniformResourceLocator"));
    return pDataObject->IsDataAvailable(CF_URL) ? DROPEFFECT_COPY : DROPEFFECT_NONE;
}

BOOL CMainFrame::OnDrop(COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point)
{
    UINT CF_URL = RegisterClipboardFormat(_T("UniformResourceLocator"));
    BOOL bResult = FALSE;

    if (pDataObject->IsDataAvailable(CF_URL)) {
        FORMATETC fmt = {CF_URL, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        if (HGLOBAL hGlobal = pDataObject->GetGlobalData(CF_URL, &fmt)) {
            LPCSTR pText = (LPCSTR)GlobalLock(hGlobal);
            if (AfxIsValidString(pText)) {
                CStringA url(pText);

                SetForegroundWindow();

                CAtlList<CString> sl;
                sl.AddTail(CString(url));

                if (m_wndPlaylistBar.IsWindowVisible()) {
                    m_wndPlaylistBar.Append(sl, true);
                } else {
                    m_wndPlaylistBar.Open(sl, true);
                    OpenCurPlaylistItem();
                }

                GlobalUnlock(hGlobal);
                bResult = TRUE;
            }
        }
    }

    return bResult;
}

DROPEFFECT CMainFrame::OnDropEx(COleDataObject* pDataObject, DROPEFFECT dropDefault, DROPEFFECT dropList, CPoint point)
{
    return (DROPEFFECT) - 1;
}

void CMainFrame::OnDragLeave()
{
}

DROPEFFECT CMainFrame::OnDragScroll(DWORD dwKeyState, CPoint point)
{
    return DROPEFFECT_NONE;
}

LPCTSTR CMainFrame::GetRecentFile() const
{
    CRecentFileList& MRU = AfxGetAppSettings().MRU;
    MRU.ReadList();
    for (int i = 0; i < MRU.GetSize(); i++) {
        if (!MRU[i].IsEmpty()) {
            return MRU[i].GetString();
        }
    }
    return NULL;
}

void CMainFrame::RestoreControlBars()
{
    POSITION pos = m_dockingbars.GetHeadPosition();
    while (pos) {
        CPlayerBar* pBar = dynamic_cast<CPlayerBar*>(m_dockingbars.GetNext(pos));

        if (pBar) {
            pBar->LoadState(this);
        }
    }
}

void CMainFrame::SaveControlBars()
{
    POSITION pos = m_dockingbars.GetHeadPosition();
    while (pos) {
        CPlayerBar* pBar = dynamic_cast<CPlayerBar*>(m_dockingbars.GetNext(pos));

        if (pBar) {
            pBar->SaveState();
        }
    }
}

LRESULT CMainFrame::OnTaskBarRestart(WPARAM, LPARAM)
{
    m_fTrayIcon = false;
    ShowTrayIcon(AfxGetAppSettings().fTrayIcon);
    return 0;
}

LRESULT CMainFrame::OnNotifyIcon(WPARAM wParam, LPARAM lParam)
{
    if ((UINT)wParam != IDR_MAINFRAME) {
        return -1;
    }

    switch ((UINT)lParam) {
        case WM_LBUTTONDOWN:
            ShowWindow(SW_SHOW);
            CreateThumbnailToolbar();
            MoveVideoWindow();
            SetForegroundWindow();
            break;
        case WM_LBUTTONDBLCLK:
            PostMessage(WM_COMMAND, ID_FILE_OPENMEDIA);
            break;
        case WM_RBUTTONDOWN: {
            POINT p;
            GetCursorPos(&p);
            SetForegroundWindow();
            m_popupmain.GetSubMenu(0)->TrackPopupMenu(TPM_RIGHTBUTTON | TPM_NOANIMATION, p.x, p.y, GetModalParent());
            PostMessage(WM_NULL);
            break;
        }
        case WM_MOUSEMOVE: {
            CString str;
            GetWindowText(str);
            SetTrayTip(str);
            break;
        }
        default:
            break;
    }

    return 0;
}

LRESULT CMainFrame::OnTaskBarThumbnailsCreate(WPARAM, LPARAM)
{
    return CreateThumbnailToolbar();
}

void CMainFrame::ShowTrayIcon(bool fShow)
{
    BOOL bWasVisible = ShowWindow(SW_HIDE);
    NOTIFYICONDATA tnid;

    ZeroMemory(&tnid, sizeof(NOTIFYICONDATA));
    tnid.cbSize = sizeof(NOTIFYICONDATA);
    tnid.hWnd = m_hWnd;
    tnid.uID = IDR_MAINFRAME;

    if (fShow) {
        if (!m_fTrayIcon) {
            tnid.hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_MAINFRAME), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
            tnid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
            tnid.uCallbackMessage = WM_NOTIFYICON;
            StringCchCopy(tnid.szTip, _countof(tnid.szTip), _T("Media Player Classic"));
            Shell_NotifyIcon(NIM_ADD, &tnid);

            m_fTrayIcon = true;
        }
    } else {
        if (m_fTrayIcon) {
            Shell_NotifyIcon(NIM_DELETE, &tnid);

            m_fTrayIcon = false;
        }
    }

    if (bWasVisible) {
        ShowWindow(SW_SHOW);
    }
}

void CMainFrame::SetTrayTip(CString str)
{
    NOTIFYICONDATA tnid;
    tnid.cbSize = sizeof(NOTIFYICONDATA);
    tnid.hWnd = m_hWnd;
    tnid.uID = IDR_MAINFRAME;
    tnid.uFlags = NIF_TIP;
    StringCchCopy(tnid.szTip, _countof(tnid.szTip), str);
    Shell_NotifyIcon(NIM_MODIFY, &tnid);
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!__super::PreCreateWindow(cs)) {
        return FALSE;
    }

    cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
    cs.lpszClass = MPC_WND_CLASS_NAME; //AfxRegisterWndClass(0);

    return TRUE;
}

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN) {
        /*if (m_fShockwaveGraph
          && (pMsg->wParam == VK_LEFT || pMsg->wParam == VK_RIGHT
              || pMsg->wParam == VK_UP || pMsg->wParam == VK_DOWN))
              return FALSE;
        */
        if (pMsg->wParam == VK_ESCAPE) {
            bool fEscapeNotAssigned = !AssignedToCmd(VK_ESCAPE, m_fFullScreen, false);

            if (fEscapeNotAssigned) {
                if (m_fFullScreen) {
                    OnViewFullscreen();
                    if (m_iMediaLoadState == MLS_LOADED) {
                        PostMessage(WM_COMMAND, ID_PLAY_PAUSE);
                    }
                    return TRUE;
                } else if (IsCaptionHidden()) {
                    PostMessage(WM_COMMAND, ID_VIEW_CAPTIONMENU);
                    return TRUE;
                }
            }
        } else if (pMsg->wParam == VK_LEFT && pAMTuner) {
            PostMessage(WM_COMMAND, ID_NAVIGATE_SKIPBACK);
            return TRUE;
        } else if (pMsg->wParam == VK_RIGHT && pAMTuner) {
            PostMessage(WM_COMMAND, ID_NAVIGATE_SKIPFORWARD);
            return TRUE;
        }
    }

    return __super::PreTranslateMessage(pMsg);
}

void CMainFrame::RecalcLayout(BOOL bNotify)
{
    __super::RecalcLayout(bNotify);

    m_wndSeekBar.HideToolTip();

    CRect r;
    GetWindowRect(&r);
    MINMAXINFO mmi;
    memset(&mmi, 0, sizeof(mmi));
    SendMessage(WM_GETMINMAXINFO, 0, (LPARAM)&mmi);
    r |= CRect(r.TopLeft(), CSize(r.Width(), mmi.ptMinTrackSize.y));
    MoveWindow(&r);
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
    __super::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
    __super::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers
void CMainFrame::OnSetFocus(CWnd* pOldWnd)
{
    SetAlwaysOnTop(AfxGetAppSettings().iOnTop);

    // forward focus to the view window
    if (IsWindow(m_wndView.m_hWnd)) {
        m_wndView.SetFocus();
    }
}

BOOL CMainFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
    // let the view have first crack at the command
    if (m_wndView.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo)) {
        return TRUE;
    }

    POSITION pos = m_bars.GetHeadPosition();
    while (pos) {
        if (m_bars.GetNext(pos)->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo)) {
            return TRUE;
        }
    }

    pos = m_dockingbars.GetHeadPosition();
    while (pos) {
        if (m_dockingbars.GetNext(pos)->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo)) {
            return TRUE;
        }
    }

    // otherwise, do default handling
    return __super::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

void CMainFrame::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    const CAppSettings& s = AfxGetAppSettings();
    DWORD style = GetStyle();
    lpMMI->ptMinTrackSize.x = 16;

    if (!IsMenuHidden()) {
        MENUBARINFO mbi;
        memset(&mbi, 0, sizeof(mbi));
        mbi.cbSize = sizeof(mbi);
        ::GetMenuBarInfo(m_hWnd, OBJID_MENU, 0, &mbi);

        // Calculate menu's horizontal length in pixels
        lpMMI->ptMinTrackSize.x = GetSystemMetrics(SM_CYMENU) / 2; //free space after menu
        CRect r;
        for (int i = 0; ::GetMenuItemRect(m_hWnd, mbi.hMenu, i, &r); i++) {
            lpMMI->ptMinTrackSize.x += r.Width();
        }
    }
    if (IsWindow(m_wndToolBar.m_hWnd) && m_wndToolBar.IsVisible()) {
        lpMMI->ptMinTrackSize.x = max(m_wndToolBar.GetMinWidth(), lpMMI->ptMinTrackSize.x);
    }

    lpMMI->ptMinTrackSize.y = 0;
    if (style & WS_CAPTION) {
        lpMMI->ptMinTrackSize.y += GetSystemMetrics(SM_CYCAPTION);
        if (s.iCaptionMenuMode == MODE_SHOWCAPTIONMENU) {
            lpMMI->ptMinTrackSize.y += GetSystemMetrics(SM_CYMENU);      //(mbi.rcBar.bottom - mbi.rcBar.top);
        }
        //else MODE_HIDEMENU
    }

    if (style & WS_THICKFRAME) {
        lpMMI->ptMinTrackSize.x += GetSystemMetrics(SM_CXSIZEFRAME) * 2;
        lpMMI->ptMinTrackSize.y += GetSystemMetrics(SM_CYSIZEFRAME) * 2;
        if ((style & WS_CAPTION) == 0) {
            lpMMI->ptMinTrackSize.x -= 2;
            lpMMI->ptMinTrackSize.y -= 2;
        }
    }

    POSITION pos = m_bars.GetHeadPosition();
    while (pos) {
        CControlBar* pCB = m_bars.GetNext(pos);
        if (!IsWindow(pCB->m_hWnd) || !pCB->IsVisible()) {
            continue;
        }
        lpMMI->ptMinTrackSize.y += pCB->CalcFixedLayout(TRUE, TRUE).cy;
    }

    pos = m_dockingbars.GetHeadPosition();
    while (pos) {
        CSizingControlBar* pCB = m_dockingbars.GetNext(pos);
        if (IsWindow(pCB->m_hWnd) && pCB->IsWindowVisible() && !pCB->IsFloating()) {
            lpMMI->ptMinTrackSize.y += pCB->CalcFixedLayout(TRUE, TRUE).cy - 2;    // 2 is a magic value from CSizingControlBar class, i guess this should be GetSystemMetrics( SM_CXBORDER ) or similar
        }
    }
    if (lpMMI->ptMinTrackSize.y < 16) {
        lpMMI->ptMinTrackSize.y = 16;
    }

    __super::OnGetMinMaxInfo(lpMMI);
}

void CMainFrame::OnMove(int x, int y)
{
    __super::OnMove(x, y);

    //MoveVideoWindow(); // This isn't needed, based on my limited tests. If it is needed then please add a description the scenario(s) where it is needed.
    m_wndView.Invalidate();

    WINDOWPLACEMENT wp;
    GetWindowPlacement(&wp);
    if (!m_fFirstFSAfterLaunchOnFS && !m_fFullScreen && wp.flags != WPF_RESTORETOMAXIMIZED && wp.showCmd != SW_SHOWMINIMIZED) {
        GetWindowRect(AfxGetAppSettings().rcLastWindowPos);
    }
}

void CMainFrame::OnMoving(UINT fwSide, LPRECT pRect)
{
    __super::OnMoving(fwSide, pRect);
    m_bWasSnapped = false;

    if (AfxGetAppSettings().fSnapToDesktopEdges) {
        const CPoint threshold(5, 5);

        CRect r0;
        CMonitors::GetNearestMonitor(this).GetMonitorRect(r0);
        CRect r1 = r0 + threshold;
        CRect r2 = r0 - threshold;

        RECT& wr = *pRect;
        CSize ws = CRect(wr).Size();

        if (wr.left < r1.left && wr.left > r2.left) {
            wr.right = (wr.left = r0.left) + ws.cx;
            m_bWasSnapped = true;
        }

        if (wr.top < r1.top && wr.top > r2.top) {
            wr.bottom = (wr.top = r0.top) + ws.cy;
            m_bWasSnapped = true;
        }

        if (wr.right < r1.right && wr.right > r2.right) {
            wr.left = (wr.right = r0.right) - ws.cx;
            m_bWasSnapped = true;
        }

        if (wr.bottom < r1.bottom && wr.bottom > r2.bottom) {
            wr.top = (wr.bottom = r0.bottom) - ws.cy;
            m_bWasSnapped = true;
        }
    }
}

void CMainFrame::OnSize(UINT nType, int cx, int cy)
{
    __super::OnSize(nType, cx, cy);

    m_OSD.OnSize(nType, cx, cy);

    if (nType == SIZE_RESTORED && m_fTrayIcon) {
        ShowWindow(SW_SHOW);
    }

    if (!m_fFirstFSAfterLaunchOnFS && IsWindowVisible() && !m_fFullScreen) {
        CAppSettings& s = AfxGetAppSettings();
        if (nType != SIZE_MAXIMIZED && nType != SIZE_MINIMIZED) {
            GetWindowRect(s.rcLastWindowPos);
        }
        s.nLastWindowType = nType;
    }
}

void CMainFrame::OnSizing(UINT fwSide, LPRECT pRect)
{
    __super::OnSizing(fwSide, pRect);

    const CAppSettings& s = AfxGetAppSettings();
    bool fCtrl = !!(GetAsyncKeyState(VK_CONTROL) & 0x80000000);

    if (m_iMediaLoadState != MLS_LOADED || m_fFullScreen
            || s.iDefaultVideoSize == DVS_STRETCH
            || (fCtrl == s.fLimitWindowProportions)) {  // remember that fCtrl is initialized with !!whatever(), same with fLimitWindowProportions
        return;
    }

    CSize wsize(pRect->right - pRect->left, pRect->bottom - pRect->top);
    CSize vsize = GetVideoSize();
    CSize fsize(0, 0);

    if (!vsize.cx || !vsize.cy) {
        return;
    }

    // TODO
    {
        DWORD style = GetStyle();

        // This doesn't give correct menu pixel size
        //MENUBARINFO mbi;
        //memset(&mbi, 0, sizeof(mbi));
        //mbi.cbSize = sizeof(mbi);
        //::GetMenuBarInfo(m_hWnd, OBJID_MENU, 0, &mbi);

        if (style & WS_THICKFRAME) {
            fsize.cx += GetSystemMetrics(SM_CXSIZEFRAME) * 2;
            fsize.cy += GetSystemMetrics(SM_CYSIZEFRAME) * 2;
            if ((style & WS_CAPTION) == 0) {
                fsize.cx -= 2;
                fsize.cy -= 2;
            }
        }

        if (style & WS_CAPTION) {
            fsize.cy += GetSystemMetrics(SM_CYCAPTION);
            if (s.iCaptionMenuMode == MODE_SHOWCAPTIONMENU) {
                fsize.cy += GetSystemMetrics(SM_CYMENU);      //mbi.rcBar.bottom - mbi.rcBar.top;
            }
            //else MODE_HIDEMENU
        }

        POSITION pos = m_bars.GetHeadPosition();
        while (pos) {
            CControlBar* pCB = m_bars.GetNext(pos);
            if (IsWindow(pCB->m_hWnd) && pCB->IsVisible()) {
                fsize.cy += pCB->CalcFixedLayout(TRUE, TRUE).cy;
            }
        }

        pos = m_dockingbars.GetHeadPosition();
        while (pos) {
            CSizingControlBar* pCB = m_dockingbars.GetNext(pos);

            if (IsWindow(pCB->m_hWnd) && pCB->IsWindowVisible() && !pCB->IsFloating()) {
                if (pCB->IsHorzDocked()) {
                    fsize.cy += pCB->CalcFixedLayout(TRUE, TRUE).cy - 2;
                } else if (pCB->IsVertDocked()) {
                    fsize.cx += pCB->CalcFixedLayout(TRUE, FALSE).cx;
                }
            }
        }
    }

    wsize -= fsize;

    bool fWider = vsize.cy < vsize.cx;

    switch (fwSide) {
        case WMSZ_TOP:
        case WMSZ_BOTTOM:
            wsize.cx = long(wsize.cy * vsize.cx / (double)vsize.cy + 0.5);
            wsize.cy = long(wsize.cx * vsize.cy / (double)vsize.cx + 0.5);
            break;
        case WMSZ_TOPLEFT:
        case WMSZ_TOPRIGHT:
        case WMSZ_BOTTOMLEFT:
        case WMSZ_BOTTOMRIGHT:
            if (!fWider) {
                wsize.cx = long(wsize.cy * vsize.cx / (double)vsize.cy + 0.5);
                wsize.cy = long(wsize.cx * vsize.cy / (double)vsize.cx + 0.5);
                break;
            }
        case WMSZ_LEFT:
        case WMSZ_RIGHT:
            wsize.cy = long(wsize.cx * vsize.cy / (double)vsize.cx + 0.5);
            wsize.cx = long(wsize.cy * vsize.cx / (double)vsize.cy + 0.5);
            break;
    }

    wsize += fsize;

    switch (fwSide) {
        case WMSZ_TOPLEFT:
            pRect->left = pRect->right - wsize.cx;
            pRect->top = pRect->bottom - wsize.cy;
            break;
        case WMSZ_TOP:
        case WMSZ_TOPRIGHT:
            pRect->right = pRect->left + wsize.cx;
            pRect->top = pRect->bottom - wsize.cy;
            break;
        case WMSZ_RIGHT:
        case WMSZ_BOTTOM:
        case WMSZ_BOTTOMRIGHT:
            pRect->right = pRect->left + wsize.cx;
            pRect->bottom = pRect->top + wsize.cy;
            break;
        case WMSZ_LEFT:
        case WMSZ_BOTTOMLEFT:
            pRect->left = pRect->right - wsize.cx;
            pRect->bottom = pRect->top + wsize.cy;
            break;
    }
}

void CMainFrame::OnDisplayChange() // untested, not sure if it's working...
{
    TRACE(_T("*** CMainFrame::OnDisplayChange()\n"));

    if (m_pFullscreenWnd && m_pFullscreenWnd->IsWindow()) {
        MONITORINFO MonitorInfo;
        HMONITOR    hMonitor;
        ZeroMemory(&MonitorInfo, sizeof(MonitorInfo));
        MonitorInfo.cbSize = sizeof(MonitorInfo);
        hMonitor = MonitorFromWindow(m_pFullscreenWnd->m_hWnd, 0);
        if (GetMonitorInfo(hMonitor, &MonitorInfo)) {
            CRect MonitorRect = CRect(MonitorInfo.rcMonitor);
            m_fullWndSize.cx  = MonitorRect.Width();
            m_fullWndSize.cy  = MonitorRect.Height();
            m_pFullscreenWnd->SetWindowPos(NULL,
                                           MonitorRect.left,
                                           MonitorRect.top,
                                           MonitorRect.Width(),
                                           MonitorRect.Height(),
                                           SWP_NOZORDER);
            MoveVideoWindow();
        }
    }

    const CAppSettings& s = AfxGetAppSettings();
    if (s.iDSVideoRendererType != VIDRNDT_DS_MADVR && s.iDSVideoRendererType != VIDRNDT_DS_DXR) {
        DWORD m_nPCIVendor = 0;
        IDirect3D9* pD3D9 = Direct3DCreate9(D3D_SDK_VERSION);

        if (pD3D9) {
            D3DADAPTER_IDENTIFIER9 adapterIdentifier;
            if (pD3D9->GetAdapterIdentifier(GetAdapter(pD3D9, m_hWnd), 0, &adapterIdentifier) == S_OK) {
                m_nPCIVendor = adapterIdentifier.VendorId;
            }
            pD3D9->Release();
        }

        if (m_nPCIVendor == 0x8086) { // Disable ResetDevice for Intel, until can fix ...
            return;
        }
    }

    if (m_iMediaLoadState == MLS_LOADED) {
        if (m_pGraphThread) {
            m_pGraphThread->PostThreadMessage(CGraphThread::TM_DISPLAY_CHANGE, 0, 0);
        } else {
            DisplayChange();
        }
    }
}

void CMainFrame::OnSysCommand(UINT nID, LPARAM lParam)
{
    // Only stop screensaver if video playing; allow for audio only
    if ((GetMediaState() == State_Running && !m_fAudioOnly)
            && (((nID & 0xFFF0) == SC_SCREENSAVE) || ((nID & 0xFFF0) == SC_MONITORPOWER))) {
        TRACE(_T("SC_SCREENSAVE, nID = %d, lParam = %d\n"), nID, lParam);
        return;
    } else if ((nID & 0xFFF0) == SC_MINIMIZE && m_fTrayIcon) {
        ShowWindow(SW_HIDE);
        return;
    }

    __super::OnSysCommand(nID, lParam);
}

void CMainFrame::OnActivateApp(BOOL bActive, DWORD dwThreadID)
{
    __super::OnActivateApp(bActive, dwThreadID);

    if (AfxGetAppSettings().iOnTop) {
        return;
    }

    MONITORINFO mi;
    mi.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST), &mi);

    if (!bActive && (mi.dwFlags & MONITORINFOF_PRIMARY) && m_fFullScreen && m_iMediaLoadState == MLS_LOADED) {
        bool fExitFullscreen = true;

        if (CWnd* pWnd = GetForegroundWindow()) {
            HMONITOR hMonitor1 = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
            HMONITOR hMonitor2 = MonitorFromWindow(pWnd->m_hWnd, MONITOR_DEFAULTTONEAREST);
            CMonitors monitors;
            if (hMonitor1 && hMonitor2 && ((hMonitor1 != hMonitor2) || (monitors.GetCount() > 1))) {
                fExitFullscreen = false;
            }

            CString title;
            pWnd->GetWindowText(title);

            CString module;

            DWORD pid;
            GetWindowThreadProcessId(pWnd->m_hWnd, &pid);

            if (HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid)) {
                HMODULE hMod;
                DWORD cbNeeded;

                if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
                    module.ReleaseBufferSetLength(GetModuleFileNameEx(hProcess, hMod, module.GetBuffer(MAX_PATH), MAX_PATH));
                }

                CloseHandle(hProcess);
            }

            CPath p(module);
            p.StripPath();
            module = (LPCTSTR)p;
            module.MakeLower();

            CString str;
            str.Format(IDS_MAINFRM_2, module, title);
            SendStatusMessage(str, 5000);
        }

        if (fExitFullscreen) {
            OnViewFullscreen();
        }
    }
}

LRESULT CMainFrame::OnAppCommand(WPARAM wParam, LPARAM lParam)
{
    UINT cmd  = GET_APPCOMMAND_LPARAM(lParam);
    UINT uDevice = GET_DEVICE_LPARAM(lParam);

    if (uDevice != FAPPCOMMAND_OEM
            || cmd == APPCOMMAND_MEDIA_PLAY
            || cmd == APPCOMMAND_MEDIA_PAUSE
            || cmd == APPCOMMAND_MEDIA_CHANNEL_UP
            || cmd == APPCOMMAND_MEDIA_CHANNEL_DOWN
            || cmd == APPCOMMAND_MEDIA_RECORD
            || cmd == APPCOMMAND_MEDIA_FAST_FORWARD
            || cmd == APPCOMMAND_MEDIA_REWIND) {
        const CAppSettings& s = AfxGetAppSettings();

        BOOL fRet = FALSE;

        POSITION pos = s.wmcmds.GetHeadPosition();
        while (pos) {
            const wmcmd& wc = s.wmcmds.GetNext(pos);
            if (wc.appcmd == cmd && TRUE == SendMessage(WM_COMMAND, wc.cmd)) {
                fRet = TRUE;
            }
        }

        if (fRet) {
            return TRUE;
        }
    }

    return Default();
}

void CMainFrame::OnRawInput(UINT nInputcode, HRAWINPUT hRawInput)
{
    const CAppSettings& s = AfxGetAppSettings();
    UINT nMceCmd = AfxGetMyApp()->GetRemoteControlCode(nInputcode, hRawInput);

    switch (nMceCmd) {
        case MCE_DETAILS:
        case MCE_GUIDE:
        case MCE_TVJUMP:
        case MCE_STANDBY:
        case MCE_OEM1:
        case MCE_OEM2:
        case MCE_MYTV:
        case MCE_MYVIDEOS:
        case MCE_MYPICTURES:
        case MCE_MYMUSIC:
        case MCE_RECORDEDTV:
        case MCE_DVDANGLE:
        case MCE_DVDAUDIO:
        case MCE_DVDMENU:
        case MCE_DVDSUBTITLE:
        case MCE_RED:
        case MCE_GREEN:
        case MCE_YELLOW:
        case MCE_BLUE:
        case MCE_MEDIA_NEXTTRACK:
        case MCE_MEDIA_PREVIOUSTRACK:
            POSITION pos = s.wmcmds.GetHeadPosition();
            while (pos) {
                const wmcmd& wc = s.wmcmds.GetNext(pos);
                if (wc.appcmd == nMceCmd) {
                    SendMessage(WM_COMMAND, wc.cmd);
                    break;
                }
            }
            break;
    }
}

LRESULT CMainFrame::OnHotKey(WPARAM wParam, LPARAM lParam)
{
    const CAppSettings& s = AfxGetAppSettings();
    BOOL fRet = FALSE;

    if (GetActiveWindow() == this || s.fGlobalMedia == TRUE) {
        POSITION pos = s.wmcmds.GetHeadPosition();

        while (pos) {
            const wmcmd& wc = s.wmcmds.GetNext(pos);
            if (wc.appcmd == wParam && TRUE == SendMessage(WM_COMMAND, wc.cmd)) {
                fRet = TRUE;
            }
        }
    }

    return fRet;
}

bool g_bNoDuration = false;
bool g_bExternalSubtitleTime = false;

void CMainFrame::OnTimer(UINT_PTR nIDEvent)
{
    switch (nIDEvent) {
        case TIMER_STREAMPOSPOLLER:
            if (m_iMediaLoadState == MLS_LOADED) {
                REFERENCE_TIME rtNow = 0, rtDur = 0;

                if (GetPlaybackMode() == PM_FILE) {
                    pMS->GetCurrentPosition(&rtNow);
                    pMS->GetDuration(&rtDur);

                    // Casimir666 : autosave subtitle sync after play
                    if ((m_nCurSubtitle >= 0) && (m_rtCurSubPos != rtNow)) {
                        if (m_lSubtitleShift != 0) {
                            if (m_wndSubresyncBar.SaveToDisk()) {
                                m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_AG_SUBTITLES_SAVED), 500);
                            } else {
                                m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_MAINFRM_4));
                            }
                        }
                        m_nCurSubtitle   = -1;
                        m_lSubtitleShift = 0;
                    }

                    if (!m_fEndOfStream) {
                        CAppSettings& s = AfxGetAppSettings();

                        if (s.fKeepHistory && s.fRememberFilePos) {
                            FILE_POSITION* filePosition = s.filePositions.GetLatestEntry();

                            if (filePosition) {
                                filePosition->llPosition = rtNow;

                                LARGE_INTEGER time;
                                QueryPerformanceCounter(&time);
                                LARGE_INTEGER freq;
                                QueryPerformanceFrequency(&freq);
                                if ((time.QuadPart - m_liLastSaveTime.QuadPart) >= 30 * freq.QuadPart) { // save every half of minute
                                    m_liLastSaveTime = time;
                                    s.filePositions.SaveLatestEntry();
                                }
                            }
                        }
                    }

                    if (m_rtDurationOverride >= 0) {
                        rtDur = m_rtDurationOverride;
                    }

                    g_bNoDuration = rtDur <= 0;
                    m_wndSeekBar.Enable(rtDur > 0);
                    m_wndSeekBar.SetRange(0, rtDur);
                    m_wndSeekBar.SetPos(rtNow);
                    m_OSD.SetRange(0, rtDur);
                    m_OSD.SetPos(rtNow);
                    m_Lcd.SetMediaRange(0, rtDur);
                    m_Lcd.SetMediaPos(rtNow);
                } else if (GetPlaybackMode() == PM_CAPTURE) {
                    pMS->GetCurrentPosition(&rtNow);
                    if (m_fCapturing && m_wndCaptureBar.m_capdlg.m_pMux) {
                        CComQIPtr<IMediaSeeking> pMuxMS = m_wndCaptureBar.m_capdlg.m_pMux;
                        if (!pMuxMS || FAILED(pMuxMS->GetCurrentPosition(&rtNow))) {
                            rtNow = 0;
                        }
                    }

                    if (m_rtDurationOverride >= 0) {
                        rtDur = m_rtDurationOverride;
                    }

                    g_bNoDuration = rtDur <= 0;
                    m_wndSeekBar.Enable(false);
                    m_wndSeekBar.SetRange(0, rtDur);
                    m_wndSeekBar.SetPos(rtNow);
                    m_OSD.SetRange(0, rtDur);
                    m_OSD.SetPos(rtNow);
                    m_Lcd.SetMediaRange(0, rtDur);
                    m_Lcd.SetMediaPos(rtNow);
                }

                if (m_pCAP && GetPlaybackMode() != PM_FILE) {
                    g_bExternalSubtitleTime = true;
                    if (pDVDI) {
                        DVD_PLAYBACK_LOCATION2 Location;
                        if (pDVDI->GetCurrentLocation(&Location) == S_OK) {
                            double fps = Location.TimeCodeFlags == DVD_TC_FLAG_25fps ? 25.0
                                         : Location.TimeCodeFlags == DVD_TC_FLAG_30fps ? 30.0
                                         : Location.TimeCodeFlags == DVD_TC_FLAG_DropFrame ? 29.97
                                         : 25.0;

                            REFERENCE_TIME rtTimeCode = HMSF2RT(Location.TimeCode, fps);
                            m_pCAP->SetTime(rtTimeCode);
                        } else {
                            m_pCAP->SetTime(/*rtNow*/m_wndSeekBar.GetPos());
                        }
                    } else {
                        // Set rtNow to support DVB subtitle
                        m_pCAP->SetTime(rtNow);
                    }
                } else {
                    g_bExternalSubtitleTime = false;
                }
            }
            break;
        case TIMER_STREAMPOSPOLLER2:
            if (m_iMediaLoadState == MLS_LOADED) {
                __int64 start, stop, pos;
                m_wndSeekBar.GetRange(start, stop);
                pos = m_wndSeekBar.GetPosReal();

                GUID tf;
                pMS->GetTimeFormat(&tf);

                if (GetPlaybackMode() == PM_CAPTURE && !m_fCapturing) {
                    CString str = ResStr(IDS_CAPTURE_LIVE);

                    long lChannel = 0, lVivSub = 0, lAudSub = 0;
                    if (pAMTuner
                            && m_wndCaptureBar.m_capdlg.IsTunerActive()
                            && SUCCEEDED(pAMTuner->get_Channel(&lChannel, &lVivSub, &lAudSub))) {
                        CString ch;
                        ch.Format(_T(" (ch%d)"), lChannel);
                        str += ch;
                    }

                    m_wndStatusBar.SetStatusTimer(str);
                } else {
                    m_wndStatusBar.SetStatusTimer(pos, stop, !!m_wndSubresyncBar.IsWindowVisible(), &tf);
                    if (m_bRemainingTime) {
                        m_OSD.DisplayMessage(OSD_TOPLEFT, m_wndStatusBar.GetStatusTimer());
                    }
                }

                m_wndSubresyncBar.SetTime(pos);

                if (m_pCAP && GetMediaState() == State_Paused) {
                    m_pCAP->Paint(false);
                }
            }
            break;
        case TIMER_FULLSCREENCONTROLBARHIDER: {
            CPoint p;
            GetCursorPos(&p);

            CRect r;
            GetWindowRect(r);
            bool fCursorOutside = !r.PtInRect(p);

            CWnd* pWnd = WindowFromPoint(p);
            if (pWnd && (m_wndView == *pWnd || m_wndView.IsChild(pWnd) || fCursorOutside)) {
                if (AfxGetAppSettings().nShowBarsWhenFullScreenTimeOut >= 0) {
                    ShowControls(CS_NONE);
                }
            }
        }
        break;
        case TIMER_FULLSCREENMOUSEHIDER: {
            CPoint p;
            GetCursorPos(&p);

            CRect r;
            GetWindowRect(r);
            bool fCursorOutside = !r.PtInRect(p);
            CWnd* pWnd = WindowFromPoint(p);
            if (m_pFullscreenWnd->IsWindow()) {
                TRACE(_T("==> HIDE!\n"));
                if (!m_bInOptions && pWnd == m_pFullscreenWnd) {
                    m_pFullscreenWnd->ShowCursor(false);
                }
                KillTimer(TIMER_FULLSCREENMOUSEHIDER);
            } else {
                CWnd* pWnd = WindowFromPoint(p);
                if (pWnd && !m_bInOptions && (m_wndView == *pWnd || m_wndView.IsChild(pWnd) || fCursorOutside)) {
                    m_fHideCursor = true;
                    SetCursor(NULL);
                }
            }
        }
        break;
        case TIMER_STATS: {
            if (pQP) {
                CString rate;
                rate.Format(_T("%.2f"), m_dSpeedRate);
                rate = _T("(") + rate + _T("x)");
                CString info;
                int val = 0;

                /*
                Reproduce:
                1. Start a video
                2. Pause video
                3. Hibernate computer
                4. Start computer again
                MPC-HC window should now be hung

                Stack dump from a Windows 7 64-bit machine:
                Thread 1:
                ntdll_77d30000!ZwWaitForSingleObject+0x15
                ntdll_77d30000!RtlpWaitOnCriticalSection+0x13e
                ntdll_77d30000!RtlEnterCriticalSection+0x150
                QUARTZ!CBlockLock<CKsOpmLib>::CBlockLock<CKsOpmLib>+0x14 <- Lock
                QUARTZ!CImageSync::get_AvgFrameRate+0x24
                QUARTZ!CVMRFilter::get_AvgFrameRate+0x31
                mpc_hc!CMainFrame::OnTimer+0xb80
                mpc_hc!CWnd::OnWndMsg+0x3e8
                mpc_hc!CWnd::WindowProc+0x24
                mpc_hc!CMainFrame::WindowProc+0x15e
                mpc_hc!AfxCallWndProc+0xac
                mpc_hc!AfxWndProc+0x36
                USER32!InternalCallWinProc+0x23
                USER32!UserCallWinProcCheckWow+0x109
                USER32!DispatchMessageWorker+0x3bc
                USER32!DispatchMessageW+0xf
                mpc_hc!AfxInternalPumpMessage+0x40
                mpc_hc!CWinThread::Run+0x5b
                mpc_hc!AfxWinMain+0x69
                mpc_hc!__tmainCRTStartup+0x11a

                Thread 2:
                ntdll_77d30000!ZwWaitForSingleObject+0x15
                ntdll_77d30000!RtlpWaitOnCriticalSection+0x13e
                ntdll_77d30000!RtlEnterCriticalSection+0x150
                QUARTZ!CBlockLock<CKsOpmLib>::CBlockLock<CKsOpmLib>+0x14 <- Lock
                QUARTZ!VMR9::CVMRFilter::NonDelegatingQueryInterface+0x1b
                mpc_hc!DSObjects::COuterVMR9::NonDelegatingQueryInterface+0x5b
                mpc_hc!CMacrovisionKicker::NonDelegatingQueryInterface+0xdc
                QUARTZ!CImageSync::QueryInterface+0x16
                QUARTZ!VMR9::CVMRFilter::CIImageSyncNotifyEvent::QueryInterface+0x19
                mpc_hc!DSObjects::CVMR9AllocatorPresenter::PresentImage+0xa9
                QUARTZ!VMR9::CVMRFilter::CIVMRImagePresenter::PresentImage+0x2c
                QUARTZ!VMR9::CImageSync::DoRenderSample+0xd5
                QUARTZ!VMR9::CImageSync::ReceiveWorker+0xad
                QUARTZ!VMR9::CImageSync::Receive+0x46
                QUARTZ!VMR9::CVideoMixer::CompositeTheStreamsTogether+0x24f
                QUARTZ!VMR9::CVideoMixer::MixerThread+0x184
                QUARTZ!VMR9::CVideoMixer::MixerThreadProc+0xd
                KERNEL32!BaseThreadInitThunk+0xe
                ntdll_77d30000!__RtlUserThreadStart+0x70
                ntdll_77d30000!_RtlUserThreadStart+0x1b

                There can be a bug in QUARTZ or more likely mpc-hc is doing something wrong
                */
                pQP->get_AvgFrameRate(&val); // We hang here due to a lock that never gets released.
                info.Format(_T("%d.%02d %s"), val / 100, val % 100, rate);
                m_wndStatsBar.SetLine(ResStr(IDS_AG_FRAMERATE), info);

                int avg, dev;
                pQP->get_AvgSyncOffset(&avg);
                pQP->get_DevSyncOffset(&dev);
                info.Format(_T("avg: %d ms, dev: %d ms"), avg, dev);
                m_wndStatsBar.SetLine(_T("Sync Offset"), info);

                int drawn, dropped;
                pQP->get_FramesDrawn(&drawn);
                pQP->get_FramesDroppedInRenderer(&dropped);
                info.Format(IDS_MAINFRM_6, drawn, dropped);
                m_wndStatsBar.SetLine(ResStr(IDS_AG_FRAMES), info);

                pQP->get_Jitter(&val);
                info.Format(_T("%d ms"), val);
                m_wndStatsBar.SetLine(_T("Jitter"), info);
            }

            if (pBI) {
                CAtlList<CString> sl;

                for (int i = 0, j = pBI->GetCount(); i < j; i++) {
                    int samples, size;
                    if (S_OK == pBI->GetStatus(i, samples, size)) {
                        CString str;
                        str.Format(_T("[%d]: %03d/%d KB"), i, samples, size / 1024);
                        sl.AddTail(str);
                    }
                }

                if (!sl.IsEmpty()) {
                    CString str;
                    str.Format(_T("%s (p%d)"), Implode(sl, ' '), pBI->GetPriority());

                    m_wndStatsBar.SetLine(ResStr(IDS_AG_BUFFERS), str);
                }
            }

            CInterfaceList<IBitRateInfo> pBRIs;

            BeginEnumFilters(pGB, pEF, pBF) {
                BeginEnumPins(pBF, pEP, pPin) {
                    if (CComQIPtr<IBitRateInfo> pBRI = pPin) {
                        pBRIs.AddTail(pBRI);
                    }
                }
                EndEnumPins;

                if (!pBRIs.IsEmpty()) {
                    CAtlList<CString> sl;

                    POSITION pos = pBRIs.GetHeadPosition();
                    for (int i = 0; pos; i++) {
                        IBitRateInfo* pBRI = pBRIs.GetNext(pos);

                        DWORD cur = pBRI->GetCurrentBitRate() / 1000;
                        DWORD avg = pBRI->GetAverageBitRate() / 1000;

                        if (avg == 0) {
                            continue;
                        }

                        CString str;
                        if (cur != avg) {
                            str.Format(_T("[%d]: %d/%d Kb/s"), i, avg, cur);
                        } else {
                            str.Format(_T("[%d]: %d Kb/s"), i, avg);
                        }
                        sl.AddTail(str);
                    }

                    if (!sl.IsEmpty()) {
                        m_wndStatsBar.SetLine(_T("Bitrate"), Implode(sl, ' ') + _T(" (avg/cur)"));
                    }

                    break;
                }
            }
            EndEnumFilters;

            if (GetPlaybackMode() == PM_DVD) { // we also use this timer to update the info panel for DVD playback
                ULONG ulAvailable, ulCurrent;

                // Location

                CString Location('-');

                DVD_PLAYBACK_LOCATION2 loc;
                ULONG ulNumOfVolumes, ulVolume;
                DVD_DISC_SIDE Side;
                ULONG ulNumOfTitles;
                ULONG ulNumOfChapters;

                if (SUCCEEDED(pDVDI->GetCurrentLocation(&loc))
                        && SUCCEEDED(pDVDI->GetNumberOfChapters(loc.TitleNum, &ulNumOfChapters))
                        && SUCCEEDED(pDVDI->GetDVDVolumeInfo(&ulNumOfVolumes, &ulVolume, &Side, &ulNumOfTitles))) {
                    Location.Format(IDS_MAINFRM_9,
                                    ulVolume, ulNumOfVolumes,
                                    loc.TitleNum, ulNumOfTitles,
                                    loc.ChapterNum, ulNumOfChapters);
                    ULONG tsec = (loc.TimeCode.bHours * 3600)
                                 + (loc.TimeCode.bMinutes * 60)
                                 + (loc.TimeCode.bSeconds);
                    /* This might not always work, such as on resume */
                    if (loc.ChapterNum != m_lCurrentChapter) {
                        m_lCurrentChapter = loc.ChapterNum;
                        m_lChapterStartTime = tsec;
                    } else {
                        /* If a resume point was used, and the user chapter jumps,
                        then it might do some funky time jumping.  Try to 'fix' the
                        chapter start time if this happens */
                        if (m_lChapterStartTime > tsec) {
                            m_lChapterStartTime = tsec;
                        }
                    }
                }

                m_wndInfoBar.SetLine(ResStr(IDS_INFOBAR_LOCATION), Location);

                // Video

                CString Video('-');

                DVD_VideoAttributes VATR;

                if (SUCCEEDED(pDVDI->GetCurrentAngle(&ulAvailable, &ulCurrent))
                        && SUCCEEDED(pDVDI->GetCurrentVideoAttributes(&VATR))) {
                    Video.Format(IDS_MAINFRM_10,
                                 ulCurrent, ulAvailable,
                                 VATR.ulSourceResolutionX, VATR.ulSourceResolutionY, VATR.ulFrameRate,
                                 VATR.ulAspectX, VATR.ulAspectY);
                }

                m_wndInfoBar.SetLine(ResStr(IDS_INFOBAR_VIDEO), Video);

                // Audio

                CString Audio('-');

                DVD_AudioAttributes AATR;

                if (SUCCEEDED(pDVDI->GetCurrentAudio(&ulAvailable, &ulCurrent))
                        && SUCCEEDED(pDVDI->GetAudioAttributes(ulCurrent, &AATR))) {
                    CString lang;
                    if (AATR.Language) {
                        int len = GetLocaleInfo(AATR.Language, LOCALE_SENGLANGUAGE, lang.GetBuffer(64), 64);
                        lang.ReleaseBufferSetLength(max(len - 1, 0));
                    } else {
                        lang.Format(IDS_AG_UNKNOWN, ulCurrent + 1);
                    }

                    switch (AATR.LanguageExtension) {
                        case DVD_AUD_EXT_NotSpecified:
                        default:
                            break;
                        case DVD_AUD_EXT_Captions:
                            lang += _T(" (Captions)");
                            break;
                        case DVD_AUD_EXT_VisuallyImpaired:
                            lang += _T(" (Visually Impaired)");
                            break;
                        case DVD_AUD_EXT_DirectorComments1:
                            lang += _T(" (Director Comments 1)");
                            break;
                        case DVD_AUD_EXT_DirectorComments2:
                            lang += _T(" (Director Comments 2)");
                            break;
                    }

                    CString format = GetDVDAudioFormatName(AATR);

                    Audio.Format(IDS_MAINFRM_11,
                                 lang,
                                 format,
                                 AATR.dwFrequency,
                                 AATR.bQuantization,
                                 AATR.bNumberOfChannels,
                                 (AATR.bNumberOfChannels > 1 ? ResStr(IDS_MAINFRM_13) : ResStr(IDS_MAINFRM_12)));

                    m_wndStatusBar.SetStatusBitmap(
                        AATR.bNumberOfChannels == 1 ? IDB_AUDIOTYPE_MONO
                        : AATR.bNumberOfChannels >= 2 ? IDB_AUDIOTYPE_STEREO
                        : IDB_AUDIOTYPE_NOAUDIO);
                }

                m_wndInfoBar.SetLine(ResStr(IDS_INFOBAR_AUDIO), Audio);

                // Subtitles

                CString Subtitles('-');

                BOOL bIsDisabled;
                DVD_SubpictureAttributes SATR;

                if (SUCCEEDED(pDVDI->GetCurrentSubpicture(&ulAvailable, &ulCurrent, &bIsDisabled))
                        && SUCCEEDED(pDVDI->GetSubpictureAttributes(ulCurrent, &SATR))) {
                    CString lang;
                    int len = GetLocaleInfo(SATR.Language, LOCALE_SENGLANGUAGE, lang.GetBuffer(64), 64);
                    lang.ReleaseBufferSetLength(max(len - 1, 0));

                    switch (SATR.LanguageExtension) {
                        case DVD_SP_EXT_NotSpecified:
                        default:
                            break;
                        case DVD_SP_EXT_Caption_Normal:
                            lang += _T("");
                            break;
                        case DVD_SP_EXT_Caption_Big:
                            lang += _T(" (Big)");
                            break;
                        case DVD_SP_EXT_Caption_Children:
                            lang += _T(" (Children)");
                            break;
                        case DVD_SP_EXT_CC_Normal:
                            lang += _T(" (CC)");
                            break;
                        case DVD_SP_EXT_CC_Big:
                            lang += _T(" (CC Big)");
                            break;
                        case DVD_SP_EXT_CC_Children:
                            lang += _T(" (CC Children)");
                            break;
                        case DVD_SP_EXT_Forced:
                            lang += _T(" (Forced)");
                            break;
                        case DVD_SP_EXT_DirectorComments_Normal:
                            lang += _T(" (Director Comments)");
                            break;
                        case DVD_SP_EXT_DirectorComments_Big:
                            lang += _T(" (Director Comments, Big)");
                            break;
                        case DVD_SP_EXT_DirectorComments_Children:
                            lang += _T(" (Director Comments, Children)");
                            break;
                    }

                    if (bIsDisabled) {
                        lang = _T("-");
                    }

                    Subtitles.Format(_T("%s"),
                                     lang);
                }

                m_wndInfoBar.SetLine(ResStr(IDS_INFOBAR_SUBTITLES), Subtitles);
            }

            if (GetMediaState() == State_Running && !m_fAudioOnly) {
                UINT fSaverActive = 0;
                if (SystemParametersInfo(SPI_GETSCREENSAVEACTIVE, 0, (PVOID)&fSaverActive, 0)) {
                    SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, 0, 0, SPIF_SENDWININICHANGE); // this might not be needed at all...
                    SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, fSaverActive, 0, SPIF_SENDWININICHANGE);
                }

                fSaverActive = 0;
                if (SystemParametersInfo(SPI_GETPOWEROFFACTIVE, 0, (PVOID)&fSaverActive, 0)) {
                    SystemParametersInfo(SPI_SETPOWEROFFACTIVE, 0, 0, SPIF_SENDWININICHANGE); // this might not be needed at all...
                    SystemParametersInfo(SPI_SETPOWEROFFACTIVE, fSaverActive, 0, SPIF_SENDWININICHANGE);
                }
                // prevent screensaver activate, monitor sleep/turn off after playback
                SetThreadExecutionState(ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
            }
        }
        break;
        case TIMER_STATUSERASER: {
            KillTimer(TIMER_STATUSERASER);
            m_playingmsg.Empty();
        }
        break;
        case TIMER_DVBINFO_UPDATER:
            KillTimer(TIMER_DVBINFO_UPDATER);
            ShowCurrentChannelInfo(false, false);
            break;
    }

    __super::OnTimer(nIDEvent);
}

bool CMainFrame::DoAfterPlaybackEvent()
{
    const CAppSettings& s = AfxGetAppSettings();
    bool fExit = (s.nCLSwitches & CLSW_CLOSE) || s.fExitAfterPlayback;

    if (s.nCLSwitches & CLSW_STANDBY) {
        SetPrivilege(SE_SHUTDOWN_NAME);
        SetSystemPowerState(TRUE, FALSE);
        fExit = true; // TODO: unless the app closes, it will call standby or hibernate once again forever, how to avoid that?
    } else if (s.nCLSwitches & CLSW_HIBERNATE) {
        SetPrivilege(SE_SHUTDOWN_NAME);
        SetSystemPowerState(FALSE, FALSE);
        fExit = true; // TODO: unless the app closes, it will call standby or hibernate once again forever, how to avoid that?
    } else if (s.nCLSwitches & CLSW_SHUTDOWN) {
        SetPrivilege(SE_SHUTDOWN_NAME);
        ExitWindowsEx(EWX_SHUTDOWN | EWX_POWEROFF | EWX_FORCEIFHUNG, 0);
        fExit = true;
    } else if (s.nCLSwitches & CLSW_LOGOFF) {
        SetPrivilege(SE_SHUTDOWN_NAME);
        ExitWindowsEx(EWX_LOGOFF | EWX_FORCEIFHUNG, 0);
        fExit = true;
    } else if (s.nCLSwitches & CLSW_LOCK) {
        LockWorkStation();
    }

    if (fExit) {
        SendMessage(WM_COMMAND, ID_FILE_EXIT);
    }

    return fExit;
}

//
// graph event EC_COMPLETE handler
//
bool CMainFrame::GraphEventComplete()
{
    CAppSettings& s = AfxGetAppSettings();

    if (s.fKeepHistory && s.fRememberFilePos) {
        FILE_POSITION* filePosition = s.filePositions.GetLatestEntry();

        if (filePosition) {
            filePosition->llPosition = 0;

            QueryPerformanceCounter(&m_liLastSaveTime);
            s.filePositions.SaveLatestEntry();
        }
    }

    if (m_wndPlaylistBar.GetCount() <= 1) {
        m_nLoops++;

        if (DoAfterPlaybackEvent()) {
            return false;
        }

        if (s.fLoopForever || m_nLoops < s.nLoops) {
            if (GetMediaState() == State_Stopped) {
                SendMessage(WM_COMMAND, ID_PLAY_PLAY);
            } else {
                LONGLONG pos = 0;
                pMS->SetPositions(&pos, AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);

                if (GetMediaState() == State_Paused) {
                    SendMessage(WM_COMMAND, ID_PLAY_PLAY);
                }
            }
        } else {
            bool bNextMediaExist = false;
            if (s.fNextInDirAfterPlayback) {
                bNextMediaExist = SearchInDir(true);
            }
            if (!bNextMediaExist) {
                if (s.fRewind) {
                    SendMessage(WM_COMMAND, ID_PLAY_STOP);
                } else {
                    m_fEndOfStream = true;
                    SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
                }
                m_OSD.ClearMessage();

                if (m_fFullScreen && s.fExitFullScreenAtTheEnd) {
                    OnViewFullscreen();
                }

                if (s.fNextInDirAfterPlayback) {
                    m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_NO_MORE_MEDIA));
                    // Don't move it. Else OSD message "Pause" will rewrite this message.
                }
            }
        }
    } else if (m_wndPlaylistBar.GetCount() > 1) {
        if (m_wndPlaylistBar.IsAtEnd()) {
            if (DoAfterPlaybackEvent()) {
                return false;
            }

            m_nLoops++;
        }

        if (s.fLoopForever || m_nLoops < s.nLoops) {
            int nLoops = m_nLoops;
            SendMessage(WM_COMMAND, ID_NAVIGATE_SKIPFORWARD);
            m_nLoops = nLoops;
        } else {
            if (m_fFullScreen && s.fExitFullScreenAtTheEnd) {
                OnViewFullscreen();
            }

            if (s.fRewind) {
                s.nCLSwitches |= CLSW_OPEN; // HACK
                PostMessage(WM_COMMAND, ID_NAVIGATE_SKIPFORWARD);
            } else {
                m_fEndOfStream = true;
                PostMessage(WM_COMMAND, ID_PLAY_PAUSE);
            }
        }
    }
    return true;
}

//
// our WM_GRAPHNOTIFY handler
//

LRESULT CMainFrame::OnGraphNotify(WPARAM wParam, LPARAM lParam)
{
    CAppSettings& s = AfxGetAppSettings();
    HRESULT hr = S_OK;

    UpdateThumbarButton();

    LONG evCode = 0;
    LONG_PTR evParam1, evParam2;
    while (pME && SUCCEEDED(pME->GetEvent(&evCode, &evParam1, &evParam2, 0))) {
#ifdef _DEBUG
        TRACE(_T("--> CMainFrame::OnGraphNotify on thread: %d; event: 0x%08x (%ws)\n"), GetCurrentThreadId(), evCode, GetEventString(evCode));
#endif
        CString str;

        if (m_fCustomGraph) {
            if (EC_BG_ERROR == evCode) {
                str = CString((char*)evParam1);
            }
        }

        if (!m_fFrameSteppingActive) {
            m_nStepForwardCount = 0;
        }

        hr = pME->FreeEventParams(evCode, evParam1, evParam2);

        switch (evCode) {
            case EC_COMPLETE:
                if (!GraphEventComplete()) {
                    return hr;
                }
                break;
            case EC_ERRORABORT:
                TRACE(_T("\thr = %08x\n"), (HRESULT)evParam1);
                break;
            case EC_BUFFERING_DATA:
                TRACE(_T("\t%d, %d\n"), (HRESULT)evParam1, evParam2);

                m_fBuffering = ((HRESULT)evParam1 != S_OK);
                break;
            case EC_STEP_COMPLETE:
                if (m_fFrameSteppingActive) {
                    m_nStepForwardCount++;
                    m_fFrameSteppingActive = false;
                    pBA->put_Volume(m_VolumeBeforeFrameStepping);
                }
                break;
            case EC_DEVICE_LOST:
                if (GetPlaybackMode() == PM_CAPTURE && evParam2 == 0) {
                    CComQIPtr<IBaseFilter> pBF = (IUnknown*)evParam1;
                    if (!pVidCap && pVidCap == pBF || !pAudCap && pAudCap == pBF) {
                        SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);
                    }
                }
                break;
            case EC_DVD_TITLE_CHANGE: {
                // Casimir666 : Save current chapter
                DVD_POSITION* dvdPosition = s.dvdPositions.GetLatestEntry();
                if (dvdPosition) {
                    dvdPosition->lTitle = (DWORD)evParam1;
                }

                if (GetPlaybackMode() == PM_FILE) {
                    SetupChapters();
                } else if (GetPlaybackMode() == PM_DVD) {
                    m_iDVDTitle = (DWORD)evParam1;

                    if (m_iDVDDomain == DVD_DOMAIN_Title) {
                        CString Domain;
                        Domain.Format(IDS_AG_TITLE, m_iDVDTitle);
                        m_wndInfoBar.SetLine(ResStr(IDS_INFOBAR_DOMAIN), Domain);
                    }

                    SetupDVDChapters();
                }
            }
            break;
            case EC_DVD_DOMAIN_CHANGE: {
                m_iDVDDomain = (DVD_DOMAIN)evParam1;

                CString Domain('-');

                switch (m_iDVDDomain) {
                    case DVD_DOMAIN_FirstPlay:
                        ULONGLONG llDVDGuid;

                        Domain = _T("First Play");

                        if (s.fShowDebugInfo) {
                            m_OSD.DebugMessage(_T("%s"), Domain);
                        }

                        if (pDVDI && SUCCEEDED(pDVDI->GetDiscID(NULL, &llDVDGuid))) {
                            if (s.fShowDebugInfo) {
                                m_OSD.DebugMessage(_T("DVD Title: %d"), s.lDVDTitle);
                            }

                            if (s.lDVDTitle != 0) {
                                s.dvdPositions.AddEntry(llDVDGuid);
                                // Set command line position
                                hr = pDVDC->PlayTitle(s.lDVDTitle, DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL);
                                if (s.fShowDebugInfo) {
                                    m_OSD.DebugMessage(_T("PlayTitle: 0x%08X"), hr);
                                    m_OSD.DebugMessage(_T("DVD Chapter: %d"), s.lDVDChapter);
                                }

                                if (s.lDVDChapter > 1) {
                                    hr = pDVDC->PlayChapterInTitle(s.lDVDTitle, s.lDVDChapter, DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL);
                                    if (s.fShowDebugInfo) {
                                        m_OSD.DebugMessage(_T("PlayChapterInTitle: 0x%08X"), hr);
                                    }
                                } else {
                                    // Trick: skip trailers with some DVDs
                                    hr = pDVDC->Resume(DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL);
                                    if (s.fShowDebugInfo) {
                                        m_OSD.DebugMessage(_T("Resume: 0x%08X"), hr);
                                    }

                                    // If the resume call succeeded, then we skip PlayChapterInTitle
                                    // and PlayAtTimeInTitle.
                                    if (hr == S_OK) {
                                        // This might fail if the Title is not available yet?
                                        hr = pDVDC->PlayAtTime(&s.DVDPosition,
                                                               DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL);
                                        if (s.fShowDebugInfo) {
                                            m_OSD.DebugMessage(_T("PlayAtTime: 0x%08X"), hr);
                                        }
                                    } else {
                                        if (s.fShowDebugInfo)
                                            m_OSD.DebugMessage(_T("Timecode requested: %02d:%02d:%02d.%03d"),
                                                               s.DVDPosition.bHours, s.DVDPosition.bMinutes,
                                                               s.DVDPosition.bSeconds, s.DVDPosition.bFrames);

                                        // Always play chapter 1 (for now, until something else dumb happens)
                                        hr = pDVDC->PlayChapterInTitle(s.lDVDTitle, 1,
                                                                       DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL);
                                        if (s.fShowDebugInfo) {
                                            m_OSD.DebugMessage(_T("PlayChapterInTitle: 0x%08X"), hr);
                                        }

                                        // This might fail if the Title is not available yet?
                                        hr = pDVDC->PlayAtTime(&s.DVDPosition,
                                                               DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL);
                                        if (s.fShowDebugInfo) {
                                            m_OSD.DebugMessage(_T("PlayAtTime: 0x%08X"), hr);
                                        }

                                        if (hr != S_OK) {
                                            hr = pDVDC->PlayAtTimeInTitle(s.lDVDTitle, &s.DVDPosition,
                                                                          DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL);
                                            if (s.fShowDebugInfo) {
                                                m_OSD.DebugMessage(_T("PlayAtTimeInTitle: 0x%08X"), hr);
                                            }
                                        }
                                    } // Resume

                                    hr = pDVDC->PlayAtTime(&s.DVDPosition,
                                                           DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL);
                                    if (s.fShowDebugInfo) {
                                        m_OSD.DebugMessage(_T("PlayAtTime: %d"), hr);
                                    }
                                }

                                m_iDVDTitle   = s.lDVDTitle;
                                s.lDVDTitle   = 0;
                                s.lDVDChapter = 0;
                            } else if (s.fKeepHistory && s.fRememberDVDPos && !s.dvdPositions.AddEntry(llDVDGuid)) {
                                // Set last remembered position (if founded...)
                                DVD_POSITION* dvdPosition = s.dvdPositions.GetLatestEntry();

                                pDVDC->PlayTitle(dvdPosition->lTitle, DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL);
                                pDVDC->Resume(DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL);
#if 1
                                if (SUCCEEDED(hr = pDVDC->PlayAtTimeInTitle(
                                                       dvdPosition->lTitle, &dvdPosition->timecode,
                                                       DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL)))
#else
                                if (SUCCEEDED(hr = pDVDC->PlayAtTime(&dvdPosition->timecode,
                                                                     DVD_CMD_FLAG_Flush, NULL)))
#endif
                                {
                                    m_iDVDTitle = dvdPosition->lTitle;
                                }
                            }

                            if (s.fRememberZoomLevel && !m_fFullScreen && !s.IsD3DFullscreen()) { // Hack to the normal initial zoom for DVD + DXVA ...
                                ZoomVideoWindow();
                            }
                        }
                        break;
                    case DVD_DOMAIN_VideoManagerMenu:
                        Domain = _T("Video Manager Menu");
                        if (s.fShowDebugInfo) {
                            m_OSD.DebugMessage(_T("%s"), Domain);
                        }
                        break;
                    case DVD_DOMAIN_VideoTitleSetMenu:
                        Domain = _T("Video Title Set Menu");
                        if (s.fShowDebugInfo) {
                            m_OSD.DebugMessage(_T("%s"), Domain);
                        }
                        break;
                    case DVD_DOMAIN_Title:
                        Domain.Format(IDS_AG_TITLE, m_iDVDTitle);
                        if (s.fShowDebugInfo) {
                            m_OSD.DebugMessage(_T("%s"), Domain);
                        }
                        {
                            DVD_POSITION* dvdPosition = s.dvdPositions.GetLatestEntry();
                            if (dvdPosition) {
                                dvdPosition->lTitle = m_iDVDTitle;
                            }
                        }
                        break;
                    case DVD_DOMAIN_Stop:
                        Domain.LoadString(IDS_AG_STOP);
                        if (s.fShowDebugInfo) {
                            m_OSD.DebugMessage(_T("%s"), Domain);
                        }
                        break;
                    default:
                        Domain = _T("-");
                        if (s.fShowDebugInfo) {
                            m_OSD.DebugMessage(_T("%s"), Domain);
                        }
                        break;
                }

                m_wndInfoBar.SetLine(ResStr(IDS_INFOBAR_DOMAIN), Domain);

                if (GetPlaybackMode() == PM_FILE) {
                    SetupChapters();
                } else if (GetPlaybackMode() == PM_DVD) {
                    SetupDVDChapters();
                }

#if 0   // UOPs debug traces
                if (hr == VFW_E_DVD_OPERATION_INHIBITED) {
                    ULONG UOPfields = 0;
                    pDVDI->GetCurrentUOPS(&UOPfields);
                    CString message;
                    message.Format(_T("UOP bitfield: 0x%08X; domain: %s"), UOPfields, Domain);
                    m_OSD.DisplayMessage(OSD_TOPLEFT, message);
                } else {
                    m_OSD.DisplayMessage(OSD_TOPRIGHT, Domain);
                }
#endif

                MoveVideoWindow(); // AR might have changed
            }
            break;
            case EC_DVD_CURRENT_HMSF_TIME: {
                double fps = evParam2 == DVD_TC_FLAG_25fps ? 25.0
                             : evParam2 == DVD_TC_FLAG_30fps ? 30.0
                             : evParam2 == DVD_TC_FLAG_DropFrame ? 29.97
                             : 25.0;

                REFERENCE_TIME rtDur = 0;

                DVD_HMSF_TIMECODE tcDur;
                ULONG ulFlags;
                if (SUCCEEDED(pDVDI->GetTotalTitleTime(&tcDur, &ulFlags))) {
                    rtDur = HMSF2RT(tcDur, fps);
                }

                g_bNoDuration = rtDur <= 0;
                m_wndSeekBar.Enable(rtDur > 0);
                m_wndSeekBar.SetRange(0, rtDur);
                m_OSD.SetRange(0, rtDur);
                m_Lcd.SetMediaRange(0, rtDur);

                REFERENCE_TIME rtNow = HMSF2RT(*((DVD_HMSF_TIMECODE*)&evParam1), fps);

                // Casimir666 : Save current position in the chapter
                DVD_POSITION* dvdPosition = s.dvdPositions.GetLatestEntry();
                if (dvdPosition) {
                    memcpy(&dvdPosition->timecode, (void*)&evParam1, sizeof(DVD_HMSF_TIMECODE));
                }

                m_wndSeekBar.SetPos(rtNow);
                m_OSD.SetPos(rtNow);
                m_Lcd.SetMediaPos(rtNow);

                if (m_pSubClock) {
                    m_pSubClock->SetTime(rtNow);
                }
            }
            break;
            case EC_DVD_ERROR: {
                TRACE(_T("\t%d %d\n"), evParam1, evParam2);

                UINT err;

                switch (evParam1) {
                    case DVD_ERROR_Unexpected:
                    default:
                        err = IDS_MAINFRM_16;
                        break;
                    case DVD_ERROR_CopyProtectFail:
                        err = IDS_MAINFRM_17;
                        break;
                    case DVD_ERROR_InvalidDVD1_0Disc:
                        err = IDS_MAINFRM_18;
                        break;
                    case DVD_ERROR_InvalidDiscRegion:
                        err = IDS_MAINFRM_19;
                        break;
                    case DVD_ERROR_LowParentalLevel:
                        err = IDS_MAINFRM_20;
                        break;
                    case DVD_ERROR_MacrovisionFail:
                        err = IDS_MAINFRM_21;
                        break;
                    case DVD_ERROR_IncompatibleSystemAndDecoderRegions:
                        err = IDS_MAINFRM_22;
                        break;
                    case DVD_ERROR_IncompatibleDiscAndDecoderRegions:
                        err = IDS_MAINFRM_23;
                        break;
                }

                SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);

                m_closingmsg.LoadString(err);
            }
            break;
            case EC_DVD_WARNING:
                TRACE(_T("\t%d %d\n"), evParam1, evParam2);
                break;
            case EC_VIDEO_SIZE_CHANGED: {
                TRACE(_T("\t%dx%d\n"), CSize((DWORD)evParam1));

                WINDOWPLACEMENT wp;
                wp.length = sizeof(wp);
                GetWindowPlacement(&wp);

                CSize size((DWORD)evParam1);
                m_fAudioOnly = (size.cx <= 0 || size.cy <= 0);

                if (s.fRememberZoomLevel
                        && !(m_fFullScreen || wp.showCmd == SW_SHOWMAXIMIZED || wp.showCmd == SW_SHOWMINIMIZED)) {
                    ZoomVideoWindow();
                } else {
                    MoveVideoWindow();
                }
            }
            break;
            case EC_LENGTH_CHANGED: {
                REFERENCE_TIME rtDur = 0;
                pMS->GetDuration(&rtDur);
                m_wndPlaylistBar.SetCurTime(rtDur);
            }
            break;
            case EC_BG_AUDIO_CHANGED:
                if (m_fCustomGraph) {
                    int nAudioChannels = (int)evParam1;

                    m_wndStatusBar.SetStatusBitmap(nAudioChannels == 1 ? IDB_AUDIOTYPE_MONO
                                                   : nAudioChannels >= 2 ? IDB_AUDIOTYPE_STEREO
                                                   : IDB_AUDIOTYPE_NOAUDIO);
                }
                break;
            case EC_BG_ERROR:
                if (m_fCustomGraph) {
                    SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);
                    m_closingmsg = !str.IsEmpty() ? str : _T("Unspecified graph error");
                    m_wndPlaylistBar.SetCurValid(false);
                    return hr;
                }
                break;
            case EC_DVD_PLAYBACK_RATE_CHANGE:
                if (m_fCustomGraph && s.AutoChangeFullscrRes.bEnabled &&
                        m_fFullScreen && m_iDVDDomain == DVD_DOMAIN_Title) {
                    AutoChangeMonitorMode();
                }
                break;
        }
    }

    return hr;
}

LRESULT CMainFrame::OnResetDevice(WPARAM wParam, LPARAM lParam)
{
    OAFilterState fs = State_Stopped;
    pMC->GetState(0, &fs);
    if (fs == State_Running) {
        pMC->Pause();
    }

    m_OSD.HideMessage(true);
    BOOL bResult = false;

    if (m_bOpenedThruThread) {
        CAMEvent e;
        m_pGraphThread->PostThreadMessage(CGraphThread::TM_RESET, (WPARAM)&bResult, (LPARAM)&e);
        e.Wait();
    } else {
        ResetDevice();
    }

    m_OSD.HideMessage(false);

    if (fs == State_Running) {
        pMC->Run();
    }
    return S_OK;
}

LRESULT CMainFrame::OnRepaintRenderLess(WPARAM wParam, LPARAM lParam)
{
    MoveVideoWindow();
    return TRUE;
}

LRESULT CMainFrame::OnResumeFromState(WPARAM wParam, LPARAM lParam)
{
    int iPlaybackMode = (int)wParam;

    if (iPlaybackMode == PM_FILE) {
        SeekTo(10000i64 * int(lParam), false);
    } else if (iPlaybackMode == PM_DVD) {
        CComPtr<IDvdState> pDvdState;
        pDvdState.Attach((IDvdState*)lParam);
        if (pDVDC) {
            pDVDC->SetState(pDvdState, DVD_CMD_FLAG_Block, NULL);
        }
    } else if (iPlaybackMode == PM_CAPTURE) {
        // not implemented
    } else {
        ASSERT(0);
        return FALSE;
    }

    return TRUE;
}

BOOL CMainFrame::OnButton(UINT id, UINT nFlags, CPoint point)
{
    SetFocus();

    CRect r;
    if (m_pFullscreenWnd->IsWindow()) {
        m_pFullscreenWnd->GetClientRect(r);
    } else {
        m_wndView.GetClientRect(r);
        m_wndView.MapWindowPoints(this, &r);
    }

    if (id != wmcmd::WDOWN && id != wmcmd::WUP && !r.PtInRect(point)) {
        return FALSE;
    }

    BOOL ret = FALSE;
    WORD cmd = AssignedToCmd(id, m_fFullScreen);

    if (cmd) {
        SendMessage(WM_COMMAND, cmd);
        ret = TRUE;
    }

    return ret;
}

static bool s_fLDown = false;

void CMainFrame::OnLButtonDown(UINT nFlags, CPoint point)
{
    if (!m_pFullscreenWnd->IsWindow() || !m_OSD.OnLButtonDown(nFlags, point)) {
        SetFocus();

        bool fClicked = false;

        if (GetPlaybackMode() == PM_DVD) {
            CPoint p = point - m_wndView.GetVideoRect().TopLeft();

            if (SUCCEEDED(pDVDC->ActivateAtPosition(p))
                    || m_iDVDDomain == DVD_DOMAIN_VideoManagerMenu
                    || m_iDVDDomain == DVD_DOMAIN_VideoTitleSetMenu) {
                fClicked = true;
            }
        }

        if (!fClicked) {
            bool fLeftMouseBtnUnassigned = !AssignedToCmd(wmcmd::LDOWN, m_fFullScreen);

            if (!m_fFullScreen && ((IsCaptionHidden() && AfxGetAppSettings().nCS <= CS_SEEKBAR) || fLeftMouseBtnUnassigned || ((GetTickCount() - m_nMenuHideTick) < 100))) {
                PostMessage(WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(point.x, point.y));
            } else {
                s_fLDown = true;
                if (OnButton(wmcmd::LDOWN, nFlags, point)) {
                    return;
                }
            }
        }

        __super::OnLButtonDown(nFlags, point);
    }
}

void CMainFrame::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (!m_pFullscreenWnd->IsWindow() || !m_OSD.OnLButtonUp(nFlags, point)) {
        bool fLeftMouseBtnUnassigned = !AssignedToCmd(wmcmd::LDOWN, m_fFullScreen);
        if (fLeftMouseBtnUnassigned || ((GetTickCount() - m_nMenuHideTick) < 100)) {
            PostMessage(WM_NCLBUTTONUP, HTCAPTION, MAKELPARAM(point.x, point.y));
        } else if (OnButton(wmcmd::LUP, nFlags, point)) {
            return;
        }

        __super::OnLButtonUp(nFlags, point);
    }
}

void CMainFrame::OnLButtonDblClk(UINT nFlags, CPoint point)
{
    if (s_fLDown) {
        SendMessage(WM_LBUTTONDOWN, nFlags, MAKELPARAM(point.x, point.y));
        s_fLDown = false;
    }
    if (!OnButton(wmcmd::LDBLCLK, nFlags, point)) {
        __super::OnLButtonDblClk(nFlags, point);
    }
}

void CMainFrame::OnMButtonDown(UINT nFlags, CPoint point)
{
    SendMessage(WM_CANCELMODE);
    if (!OnButton(wmcmd::MDOWN, nFlags, point)) {
        __super::OnMButtonDown(nFlags, point);
    }
}

void CMainFrame::OnMButtonUp(UINT nFlags, CPoint point)
{
    if (!OnButton(wmcmd::MUP, nFlags, point)) {
        __super::OnMButtonUp(nFlags, point);
    }
}

void CMainFrame::OnMButtonDblClk(UINT nFlags, CPoint point)
{
    SendMessage(WM_MBUTTONDOWN, nFlags, MAKELPARAM(point.x, point.y));
    if (!OnButton(wmcmd::MDBLCLK, nFlags, point)) {
        __super::OnMButtonDblClk(nFlags, point);
    }
}

void CMainFrame::OnRButtonDown(UINT nFlags, CPoint point)
{
    if (!OnButton(wmcmd::RDOWN, nFlags, point)) {
        __super::OnRButtonDown(nFlags, point);
    }
}

void CMainFrame::OnRButtonUp(UINT nFlags, CPoint point)
{
    CPoint p;
    GetCursorPos(&p);
    SetFocus();
    if (WindowFromPoint(p) != m_pFullscreenWnd && !OnButton(wmcmd::RUP, nFlags, point)) {
        __super::OnRButtonUp(nFlags, point);
    }
}

void CMainFrame::OnRButtonDblClk(UINT nFlags, CPoint point)
{
    SendMessage(WM_RBUTTONDOWN, nFlags, MAKELPARAM(point.x, point.y));
    if (!OnButton(wmcmd::RDBLCLK, nFlags, point)) {
        __super::OnRButtonDblClk(nFlags, point);
    }
}

LRESULT CMainFrame::OnXButtonDown(WPARAM wParam, LPARAM lParam)
{
    SendMessage(WM_CANCELMODE);
    UINT fwButton = GET_XBUTTON_WPARAM(wParam);
    return OnButton(fwButton == XBUTTON1 ? wmcmd::X1DOWN : fwButton == XBUTTON2 ? wmcmd::X2DOWN : wmcmd::NONE,
                    GET_KEYSTATE_WPARAM(wParam), CPoint(lParam));
}

LRESULT CMainFrame::OnXButtonUp(WPARAM wParam, LPARAM lParam)
{
    UINT fwButton = GET_XBUTTON_WPARAM(wParam);
    return OnButton(fwButton == XBUTTON1 ? wmcmd::X1UP : fwButton == XBUTTON2 ? wmcmd::X2UP : wmcmd::NONE,
                    GET_KEYSTATE_WPARAM(wParam), CPoint(lParam));
}

LRESULT CMainFrame::OnXButtonDblClk(WPARAM wParam, LPARAM lParam)
{
    SendMessage(WM_XBUTTONDOWN, wParam, lParam);
    UINT fwButton = GET_XBUTTON_WPARAM(wParam);
    return OnButton(fwButton == XBUTTON1 ? wmcmd::X1DBLCLK : fwButton == XBUTTON2 ? wmcmd::X2DBLCLK : wmcmd::NONE,
                    GET_KEYSTATE_WPARAM(wParam), CPoint(lParam));
}

BOOL CMainFrame::OnMouseWheel(UINT nFlags, short zDelta, CPoint point)
{
    ScreenToClient(&point);

    BOOL fRet =
        zDelta > 0 ? OnButton(wmcmd::WUP, nFlags, point) :
        zDelta < 0 ? OnButton(wmcmd::WDOWN, nFlags, point) :
        FALSE;

    return fRet;
}

void CMainFrame::OnMouseMove(UINT nFlags, CPoint point)
{
    // Waffs : ignore mousemoves when entering fullscreen
    if (m_lastMouseMove.x == -1 && m_lastMouseMove.y == -1) {
        m_lastMouseMove.x = point.x;
        m_lastMouseMove.y = point.y;
    }

    if (!m_OSD.OnMouseMove(nFlags, point)) {
        if (GetPlaybackMode() == PM_DVD) {
            CPoint vp = point - m_wndView.GetVideoRect().TopLeft();
            ULONG pulButtonIndex;
            if (!m_fHideCursor) {
                SetCursor(LoadCursor(NULL, SUCCEEDED(pDVDI->GetButtonAtPosition(vp, &pulButtonIndex)) ? IDC_HAND : IDC_ARROW));
            }
            pDVDC->SelectAtPosition(vp);
        }

        CSize diff = m_lastMouseMove - point;
        const CAppSettings& s = AfxGetAppSettings();

        if (m_pFullscreenWnd->IsWindow() && (abs(diff.cx) + abs(diff.cy)) >= 1) {
            //TRACE(_T("==> SHOW!\n"));
            m_pFullscreenWnd->ShowCursor(true);

            // Casimir666 : hide the cursor if we are not in the DVD menu
            if ((GetPlaybackMode() == PM_FILE) || (GetPlaybackMode() == PM_DVD)) {
                KillTimer(TIMER_FULLSCREENMOUSEHIDER);
                SetTimer(TIMER_FULLSCREENMOUSEHIDER, 2000, NULL);
            }
        } else if (m_fFullScreen && (abs(diff.cx) + abs(diff.cy)) >= 1) {
            int nTimeOut = s.nShowBarsWhenFullScreenTimeOut;

            if (nTimeOut < 0) {
                m_fHideCursor = false;
                if (s.fShowBarsWhenFullScreen) {
                    ShowControls(m_nCS);
                    if (GetPlaybackMode() == PM_CAPTURE && !s.fHideNavigation && s.iDefaultCaptureDevice == 1) {
                        m_wndNavigationBar.m_navdlg.UpdateElementList();
                        m_wndNavigationBar.ShowControls(this, TRUE);
                    }
                }

                KillTimer(TIMER_FULLSCREENCONTROLBARHIDER);
                SetTimer(TIMER_FULLSCREENMOUSEHIDER, 2000, NULL);
            } else if (nTimeOut == 0) {
                CRect r;
                GetClientRect(r);
                r.top = r.bottom;

                POSITION pos = m_bars.GetHeadPosition();
                for (int i = 1; pos; i <<= 1) {
                    CControlBar* pNext = m_bars.GetNext(pos);
                    CSize size = pNext->CalcFixedLayout(FALSE, TRUE);
                    if (m_nCS & i) {
                        r.top -= size.cy;
                    }
                }


                // HACK: the controls would cover the menu too early hiding some buttons
                if (GetPlaybackMode() == PM_DVD
                        && (m_iDVDDomain == DVD_DOMAIN_VideoManagerMenu
                            || m_iDVDDomain == DVD_DOMAIN_VideoTitleSetMenu)) {
                    r.top = r.bottom - 10;
                }

                m_fHideCursor = false;

                if (r.PtInRect(point)) {
                    if (s.fShowBarsWhenFullScreen) {
                        ShowControls(m_nCS);
                    }
                } else {
                    if (s.fShowBarsWhenFullScreen) {
                        ShowControls(CS_NONE);
                    }
                }

                // PM_CAPTURE: Left Navigation panel for switching channels
                if (GetPlaybackMode() == PM_CAPTURE && !s.fHideNavigation && s.iDefaultCaptureDevice == 1) {
                    CRect rLeft;
                    GetClientRect(rLeft);
                    rLeft.right = rLeft.left;
                    CSize size = m_wndNavigationBar.CalcFixedLayout(FALSE, TRUE);
                    rLeft.right += size.cx;

                    m_fHideCursor = false;

                    if (rLeft.PtInRect(point)) {
                        if (s.fShowBarsWhenFullScreen) {
                            m_wndNavigationBar.m_navdlg.UpdateElementList();
                            m_wndNavigationBar.ShowControls(this, TRUE);
                        }
                    } else {
                        if (s.fShowBarsWhenFullScreen) {
                            m_wndNavigationBar.ShowControls(this, FALSE);
                        }
                    }
                }

                SetTimer(TIMER_FULLSCREENMOUSEHIDER, 2000, NULL);
            } else {
                m_fHideCursor = false;
                if (s.fShowBarsWhenFullScreen) {
                    ShowControls(m_nCS);
                }

                SetTimer(TIMER_FULLSCREENCONTROLBARHIDER, nTimeOut * 1000, NULL);
                SetTimer(TIMER_FULLSCREENMOUSEHIDER, max(nTimeOut * 1000, 2000), NULL);
            }
        }

        m_lastMouseMove = point;

        __super::OnMouseMove(nFlags, point);
    }
}

LRESULT CMainFrame::OnNcHitTest(CPoint point)
{
    LRESULT nHitTest = __super::OnNcHitTest(point);
    return ((IsCaptionHidden()) && nHitTest == HTCLIENT) ? HTCAPTION : nHitTest;
}

void CMainFrame::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    bool Shift_State = !!(::GetKeyState(VK_SHIFT) & 0x8000);
    if (AfxGetAppSettings().fFastSeek) {
        Shift_State = !Shift_State;
    }

    if (pScrollBar->IsKindOf(RUNTIME_CLASS(CVolumeCtrl))) {
        OnPlayVolume(0);
    } else if (pScrollBar->IsKindOf(RUNTIME_CLASS(CPlayerSeekBar)) && m_iMediaLoadState == MLS_LOADED) {
        SeekTo(m_wndSeekBar.GetPos(), Shift_State);
    } else if (pScrollBar == m_pVideoWnd) {
        SeekTo(m_OSD.GetPos(), Shift_State);
    }

    __super::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CMainFrame::OnInitMenu(CMenu* pMenu)
{
    __super::OnInitMenu(pMenu);

    const UINT uiMenuCount = pMenu->GetMenuItemCount();
    if (uiMenuCount == -1) {
        return;
    }

    MENUITEMINFO mii;
    mii.cbSize = sizeof(mii);

    for (UINT i = 0; i < uiMenuCount; ++i) {
#ifdef _DEBUG
        CString str;
        pMenu->GetMenuString(i, str, MF_BYPOSITION);
        str.Remove('&');
#endif
        UINT itemID = pMenu->GetMenuItemID(i);
        if (itemID == 0xFFFFFFFF) {
            mii.fMask = MIIM_ID;
            pMenu->GetMenuItemInfo(i, &mii, TRUE);
            itemID = mii.wID;
        }

        CMenu* pSubMenu = NULL;

        if (itemID == ID_FAVORITES) {
            SetupFavoritesSubMenu();
            pSubMenu = &m_favorites;
        }/*else if (itemID == ID_RECENT_FILES) {
            SetupRecentFilesSubMenu();
            pSubMenu = &m_recentfiles;
        }*/

        if (pSubMenu) {
            mii.fMask = MIIM_STATE | MIIM_SUBMENU | MIIM_ID;
            mii.fType = MF_POPUP;
            mii.wID = itemID; // save ID after set popup type
            mii.hSubMenu = pSubMenu->m_hMenu;
            mii.fState = (pSubMenu->GetMenuItemCount() > 0 ? MF_ENABLED : (MF_DISABLED | MF_GRAYED));
            pMenu->SetMenuItemInfo(i, &mii, TRUE);
        }
    }
}

void CMainFrame::OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu)
{
    __super::OnInitMenuPopup(pPopupMenu, nIndex, bSysMenu);

    UINT uiMenuCount = pPopupMenu->GetMenuItemCount();
    if (uiMenuCount == -1) {
        return;
    }

    MENUITEMINFO mii;
    mii.cbSize = sizeof(mii);

    for (UINT i = 0; i < uiMenuCount; ++i) {
#ifdef _DEBUG
        CString str;
        pPopupMenu->GetMenuString(i, str, MF_BYPOSITION);
        str.Remove('&');
#endif
        UINT firstSubItemID = 0;
        CMenu* sm = pPopupMenu->GetSubMenu(i);
        if (sm) {
            firstSubItemID = sm->GetMenuItemID(0);
        }

        if (firstSubItemID == ID_NAVIGATE_SKIPBACK) { // is "Navigate" submenu {
            UINT fState = (m_iMediaLoadState == MLS_LOADED
                           && (1/*GetPlaybackMode() == PM_DVD *//*|| (GetPlaybackMode() == PM_FILE && m_PlayList.GetCount() > 0)*/))
                          ? MF_ENABLED
                          : (MF_DISABLED | MF_GRAYED);
            pPopupMenu->EnableMenuItem(i, MF_BYPOSITION | fState);
            continue;
        }
        if (firstSubItemID == ID_VIEW_VF_HALF               // is "Video Frame" submenu
                || firstSubItemID == ID_VIEW_INCSIZE        // is "Pan&Scan" submenu
                || firstSubItemID == ID_ASPECTRATIO_SOURCE  // is "Override Aspect Ratio" submenu
                || firstSubItemID == ID_VIEW_ZOOM_50) {     // is "Zoom" submenu
            UINT fState = (m_iMediaLoadState == MLS_LOADED && !m_fAudioOnly)
                          ? MF_ENABLED
                          : (MF_DISABLED | MF_GRAYED);
            pPopupMenu->EnableMenuItem(i, MF_BYPOSITION | fState);
            continue;
        }

        UINT itemID = pPopupMenu->GetMenuItemID(i);
        if (itemID == 0xFFFFFFFF) {
            mii.fMask = MIIM_ID;
            pPopupMenu->GetMenuItemInfo(i, &mii, TRUE);
            itemID = mii.wID;
        }
        CMenu* pSubMenu = NULL;

        if (itemID == ID_FILE_OPENDISC32774) {
            SetupOpenCDSubMenu();
            pSubMenu = &m_opencds;
        } else if (itemID == ID_FILTERS) {
            SetupFiltersSubMenu();
            pSubMenu = &m_filters;
        } else if (itemID == ID_MENU_LANGUAGE) {
            SetupLanguageMenu();
            pSubMenu = &m_language;
        } else if (itemID == ID_AUDIOS) {
            SetupAudioSwitcherSubMenu();
            pSubMenu = &m_audios;
        } else if (itemID == ID_SUBTITLES) {
            SetupSubtitlesSubMenu();
            pSubMenu = &m_subtitles;
        } else if (itemID == ID_AUDIOLANGUAGE) {
            SetupNavAudioSubMenu();
            pSubMenu = &m_navaudio;
        } else if (itemID == ID_SUBTITLELANGUAGE) {
            SetupNavSubtitleSubMenu();
            pSubMenu = &m_navsubtitle;
        } else if (itemID == ID_VIDEOANGLE) {

            CString menu_str;
            if (GetPlaybackMode() == PM_DVD) {
                menu_str.LoadString(IDS_MENU_VIDEO_ANGLE);
            } else {
                menu_str.LoadString(IDS_MENU_VIDEO_STREAM);
            }

            mii.fMask = MIIM_STRING;
            mii.dwTypeData = (LPTSTR)(LPCTSTR)menu_str;
            pPopupMenu->SetMenuItemInfo(i, &mii, TRUE);

            SetupNavAngleSubMenu();
            pSubMenu = &m_navangle;
        } else if (itemID == ID_JUMPTO) {
            SetupNavChaptersSubMenu();
            pSubMenu = &m_navchapters;
        } else if (itemID == ID_FAVORITES) {
            SetupFavoritesSubMenu();
            pSubMenu = &m_favorites;
        } else if (itemID == ID_RECENT_FILES) {
            SetupRecentFilesSubMenu();
            pSubMenu = &m_recentfiles;
        } else if (itemID == ID_SHADERS) {
            SetupShadersSubMenu();
            pSubMenu = &m_shaders;
        }

        if (pSubMenu) {
            mii.fMask = MIIM_STATE | MIIM_SUBMENU | MIIM_ID;
            mii.fType = MF_POPUP;
            mii.wID = itemID; // save ID after set popup type
            mii.hSubMenu = pSubMenu->m_hMenu;
            mii.fState = (pSubMenu->GetMenuItemCount() > 0 ? MF_ENABLED : (MF_DISABLED | MF_GRAYED));
            pPopupMenu->SetMenuItemInfo(i, &mii, TRUE);
            //continue;
        }
    }

    uiMenuCount = pPopupMenu->GetMenuItemCount();
    if (uiMenuCount == -1) {
        return;
    }

    for (UINT i = 0; i < uiMenuCount; ++i) {
        UINT nID = pPopupMenu->GetMenuItemID(i);
        if (nID == ID_SEPARATOR || nID == -1
                || nID >= ID_FAVORITES_FILE_START && nID <= ID_FAVORITES_FILE_END
                || nID >= ID_RECENT_FILE_START && nID <= ID_RECENT_FILE_END
                || nID >= ID_NAVIGATE_CHAP_SUBITEM_START && nID <= ID_NAVIGATE_CHAP_SUBITEM_END) {
            continue;
        }

        CString str;
        pPopupMenu->GetMenuString(i, str, MF_BYPOSITION);
        int k = str.Find('\t');
        if (k > 0) {
            str = str.Left(k);
        }

        CString key = CPPageAccelTbl::MakeAccelShortcutLabel(nID);
        if (key.IsEmpty() && k < 0) {
            continue;
        }
        str += _T("\t") + key;

        // BUG(?): this disables menu item update ui calls for some reason...
        //pPopupMenu->ModifyMenu(i, MF_BYPOSITION|MF_STRING, nID, str);

        // this works fine
        MENUITEMINFO mii;
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_STRING;
        mii.dwTypeData = (LPTSTR)(LPCTSTR)str;
        pPopupMenu->SetMenuItemInfo(i, &mii, TRUE);

    }

    uiMenuCount = pPopupMenu->GetMenuItemCount();
    if (uiMenuCount == -1) {
        return;
    }

    bool fPnSPresets = false;

    for (UINT i = 0; i < uiMenuCount; ++i) {
        UINT nID = pPopupMenu->GetMenuItemID(i);

        if (nID >= ID_PANNSCAN_PRESETS_START && nID < ID_PANNSCAN_PRESETS_END) {
            do {
                nID = pPopupMenu->GetMenuItemID(i);
                pPopupMenu->DeleteMenu(i, MF_BYPOSITION);
                uiMenuCount--;
            } while (i < uiMenuCount && nID >= ID_PANNSCAN_PRESETS_START && nID < ID_PANNSCAN_PRESETS_END);

            nID = pPopupMenu->GetMenuItemID(i);
        }

        if (nID == ID_VIEW_RESET) {
            fPnSPresets = true;
        }
    }

    if (fPnSPresets) {
        const CAppSettings& s = AfxGetAppSettings();
        INT_PTR i = 0, j = s.m_pnspresets.GetCount();
        for (; i < j; i++) {
            int k = 0;
            CString label = s.m_pnspresets[i].Tokenize(_T(","), k);
            pPopupMenu->InsertMenu(ID_VIEW_RESET, MF_BYCOMMAND, ID_PANNSCAN_PRESETS_START + i, label);
        }
        //if (j > 0)
        {
            pPopupMenu->InsertMenu(ID_VIEW_RESET, MF_BYCOMMAND, ID_PANNSCAN_PRESETS_START + i, ResStr(IDS_PANSCAN_EDIT));
            pPopupMenu->InsertMenu(ID_VIEW_RESET, MF_BYCOMMAND | MF_SEPARATOR);
        }
    }
}

void CMainFrame::OnUnInitMenuPopup(CMenu* pPopupMenu, UINT nFlags)
{
    __super::OnUnInitMenuPopup(pPopupMenu, nFlags);
    m_nMenuHideTick = GetTickCount();
}

BOOL CMainFrame::OnMenu(CMenu* pMenu)
{
    if (!pMenu) {
        return FALSE;
    }
    const CAppSettings& s = AfxGetAppSettings();
    // Do not show popup menu in D3D fullscreen for Sync Renderer. It has several adverse effects.
    if (IsD3DFullScreenMode() && s.iDSVideoRendererType == VIDRNDT_DS_SYNC) {
        return FALSE;
    }
    KillTimer(TIMER_FULLSCREENMOUSEHIDER);
    m_fHideCursor = false;

    CPoint point;
    GetCursorPos(&point);

    MSG msg;
    pMenu->TrackPopupMenu(TPM_RIGHTBUTTON | TPM_NOANIMATION, point.x + 1, point.y + 1, this);

    if (AfxGetMyApp()->m_fClosingState) {
        return FALSE; //prevent crash when player closes with context menu open
    }

    PeekMessage(&msg, this->m_hWnd, WM_LBUTTONDOWN, WM_LBUTTONDOWN, PM_REMOVE); //remove the click LMB, which closes the popup menu

    if (m_fFullScreen) {
        SetTimer(TIMER_FULLSCREENMOUSEHIDER, 2000, NULL);    //need when working with menus and use the keyboard only
    }

    return TRUE;
}

void CMainFrame::OnMenuPlayerShort()
{
    if (IsMenuHidden() || m_pFullscreenWnd->IsWindow()) {
        OnMenu(m_popupmain.GetSubMenu(0));
    } else {
        OnMenu(m_popup.GetSubMenu(0));
    }
}

void CMainFrame::OnMenuPlayerLong()
{
    OnMenu(m_popupmain.GetSubMenu(0));
}

void CMainFrame::OnMenuFilters()
{
    SetupFiltersSubMenu();
    OnMenu(&m_filters);
}

void CMainFrame::OnUpdatePlayerStatus(CCmdUI* pCmdUI)
{
    if (m_iMediaLoadState == MLS_LOADING) {
        pCmdUI->SetText(ResStr(IDS_CONTROLS_OPENING));
        if ((AfxGetAppSettings().fUseWin7TaskBar) && (m_pTaskbarList)) {
            m_pTaskbarList->SetProgressState(m_hWnd, TBPF_INDETERMINATE);
        }
    } else if (m_iMediaLoadState == MLS_LOADED) {
        CString msg;

        if (!m_playingmsg.IsEmpty()) {
            msg = m_playingmsg;
        } else if (m_fCapturing) {
            msg.LoadString(IDS_CONTROLS_CAPTURING);

            if (pAMDF) {
                long lDropped = 0;
                pAMDF->GetNumDropped(&lDropped);
                long lNotDropped = 0;
                pAMDF->GetNumNotDropped(&lNotDropped);

                if ((lDropped + lNotDropped) > 0) {
                    msg.AppendFormat(IDS_MAINFRM_37, lDropped + lNotDropped, lDropped);
                }
            }

            CComPtr<IPin> pPin;
            if (SUCCEEDED(pCGB->FindPin(m_wndCaptureBar.m_capdlg.m_pDst, PINDIR_INPUT, NULL, NULL, FALSE, 0, &pPin))) {
                LONGLONG size = 0;
                if (CComQIPtr<IStream> pStream = pPin) {
                    pStream->Commit(STGC_DEFAULT);

                    WIN32_FIND_DATA findFileData;
                    HANDLE h = FindFirstFile(m_wndCaptureBar.m_capdlg.m_file, &findFileData);
                    if (h != INVALID_HANDLE_VALUE) {
                        size = ((LONGLONG)findFileData.nFileSizeHigh << 32) | findFileData.nFileSizeLow;

                        if (size < 1024i64 * 1024) {
                            msg.AppendFormat(IDS_MAINFRM_38, size / 1024);
                        } else { //if (size < 1024i64*1024*1024)
                            msg.AppendFormat(IDS_MAINFRM_39, size / 1024 / 1024);
                        }

                        FindClose(h);
                    }
                }

                ULARGE_INTEGER FreeBytesAvailable, TotalNumberOfBytes, TotalNumberOfFreeBytes;
                if (GetDiskFreeSpaceEx(
                            m_wndCaptureBar.m_capdlg.m_file.Left(m_wndCaptureBar.m_capdlg.m_file.ReverseFind('\\') + 1),
                            &FreeBytesAvailable, &TotalNumberOfBytes, &TotalNumberOfFreeBytes)) {
                    if (FreeBytesAvailable.QuadPart < 1024i64 * 1024) {
                        msg.AppendFormat(IDS_MAINFRM_40, FreeBytesAvailable.QuadPart / 1024);
                    } else { //if (FreeBytesAvailable.QuadPart < 1024i64*1024*1024)
                        msg.AppendFormat(IDS_MAINFRM_41, FreeBytesAvailable.QuadPart / 1024 / 1024);
                    }
                }

                if (m_wndCaptureBar.m_capdlg.m_pMux) {
                    __int64 pos = 0;
                    CComQIPtr<IMediaSeeking> pMuxMS = m_wndCaptureBar.m_capdlg.m_pMux;
                    if (pMuxMS && SUCCEEDED(pMuxMS->GetCurrentPosition(&pos)) && pos > 0) {
                        double bytepersec = 10000000.0 * size / pos;
                        if (bytepersec > 0) {
                            m_rtDurationOverride = __int64(10000000.0 * (FreeBytesAvailable.QuadPart + size) / bytepersec);
                        }
                    }
                }

                if (m_wndCaptureBar.m_capdlg.m_pVidBuffer
                        || m_wndCaptureBar.m_capdlg.m_pAudBuffer) {
                    int nFreeVidBuffers = 0, nFreeAudBuffers = 0;
                    if (CComQIPtr<IBufferFilter> pVB = m_wndCaptureBar.m_capdlg.m_pVidBuffer) {
                        nFreeVidBuffers = pVB->GetFreeBuffers();
                    }
                    if (CComQIPtr<IBufferFilter> pAB = m_wndCaptureBar.m_capdlg.m_pAudBuffer) {
                        nFreeAudBuffers = pAB->GetFreeBuffers();
                    }

                    msg.AppendFormat(IDS_MAINFRM_42, nFreeVidBuffers, nFreeAudBuffers);
                }
            }
        } else if (m_fBuffering) {
            BeginEnumFilters(pGB, pEF, pBF) {
                if (CComQIPtr<IAMNetworkStatus, &IID_IAMNetworkStatus> pAMNS = pBF) {
                    long BufferingProgress = 0;
                    if (SUCCEEDED(pAMNS->get_BufferingProgress(&BufferingProgress)) && BufferingProgress > 0) {
                        msg.Format(IDS_CONTROLS_BUFFERING, BufferingProgress);

                        __int64 start = 0, stop = 0;
                        m_wndSeekBar.GetRange(start, stop);
                        m_fLiveWM = (stop == start);
                    }
                    break;
                }
            }
            EndEnumFilters;
        } else if (pAMOP) {
            __int64 t = 0, c = 0;
            if (SUCCEEDED(pMS->GetDuration(&t)) && t > 0 && SUCCEEDED(pAMOP->QueryProgress(&t, &c)) && t > 0 && c < t) {
                msg.Format(IDS_CONTROLS_BUFFERING, c * 100 / t);
            }

            if (m_fUpdateInfoBar) {
                OpenSetupInfoBar();
            }
        }

        OAFilterState fs = GetMediaState();
        CString UI_Text =
            !msg.IsEmpty() ? msg :
            fs == State_Stopped ? ResStr(IDS_CONTROLS_STOPPED) :
            (fs == State_Paused || m_fFrameSteppingActive) ? ResStr(IDS_CONTROLS_PAUSED) :
            fs == State_Running ? ResStr(IDS_CONTROLS_PLAYING) :
            _T("");
        if ((!m_fAudioOnly) && (UI_Text == ResStr(IDS_CONTROLS_PAUSED) || UI_Text == ResStr(IDS_CONTROLS_PLAYING))) {
            CString DXVA_Text = GetDXVADecoderDescription();
            if (!(_T("Not using DXVA") == DXVA_Text) || (_T("Unknown") == DXVA_Text)) {
                UI_Text += _T(" [DXVA]");
            }
        }
        pCmdUI->SetText(UI_Text);
    } else if (m_iMediaLoadState == MLS_CLOSING) {
        pCmdUI->SetText(ResStr(IDS_CONTROLS_CLOSING));
        if ((AfxGetAppSettings().fUseWin7TaskBar) && (m_pTaskbarList)) {
            m_pTaskbarList->SetProgressState(m_hWnd, TBPF_INDETERMINATE);
        }
    } else {
        pCmdUI->SetText(m_closingmsg);
    }
}

void CMainFrame::OnFilePostOpenmedia()
{
    OpenSetupInfoBar();
    OpenSetupStatsBar();
    OpenSetupStatusBar();
    //OpenSetupToolBar();
    OpenSetupCaptureBar();

    REFERENCE_TIME rtDur = 0;
    pMS->GetDuration(&rtDur);
    m_wndPlaylistBar.SetCurTime(rtDur);

    if (GetPlaybackMode() == PM_CAPTURE) {
        ShowControlBar(&m_wndSubresyncBar, FALSE, TRUE);
        ShowControls(m_nCS & ~CS_SEEKBAR, true);
        //ShowControlBar(&m_wndPlaylistBar, FALSE, TRUE);
        //ShowControlBar(&m_wndCaptureBar, TRUE, TRUE);
    }

    m_nCurSubtitle   = -1;
    m_lSubtitleShift = 0;
    if (m_pCAP) {
        m_pCAP->SetSubtitleDelay(0);
    }

    SetLoadState(MLS_LOADED);
    CAppSettings& s = AfxGetAppSettings();

    // IMPORTANT: must not call any windowing msgs before
    // this point, it will deadlock when OpenMediaPrivate is
    // still running and the renderer window was created on
    // the same worker-thread

    {
        WINDOWPLACEMENT wp;
        wp.length = sizeof(wp);
        GetWindowPlacement(&wp);

        // restore magnification
        if (IsWindowVisible() && s.fRememberZoomLevel
                && !(m_fFullScreen || wp.showCmd == SW_SHOWMAXIMIZED || wp.showCmd == SW_SHOWMINIMIZED)) {
            ZoomVideoWindow(false);
        }
    }

    // Waffs : PnS command line
    if (!s.strPnSPreset.IsEmpty()) {
        for (int i = 0; i < s.m_pnspresets.GetCount(); i++) {
            int j = 0;
            CString str = s.m_pnspresets[i];
            CString label = str.Tokenize(_T(","), j);
            if (s.strPnSPreset == label) {
                OnViewPanNScanPresets(i + ID_PANNSCAN_PRESETS_START);
            }
        }
        s.strPnSPreset.Empty();
    }
    SendNowPlayingToMSN();
    SendNowPlayingToApi();
}

void CMainFrame::OnUpdateFilePostOpenmedia(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_iMediaLoadState == MLS_LOADING);
}

void CMainFrame::OnFilePostClosemedia()
{
    if (IsD3DFullScreenMode()) {
        KillTimer(TIMER_FULLSCREENMOUSEHIDER);
        KillTimer(TIMER_FULLSCREENCONTROLBARHIDER);
        m_fHideCursor = false;
    }
    m_wndView.SetVideoRect();
    m_wndSeekBar.Enable(false);
    m_wndSeekBar.SetPos(0);
    m_wndSeekBar.RemoveChapters();
    m_wndInfoBar.RemoveAllLines();
    m_wndStatsBar.RemoveAllLines();
    m_wndStatusBar.Clear();
    m_wndStatusBar.ShowTimer(false);

    if (AfxGetAppSettings().fEnableEDLEditor) {
        m_wndEditListEditor.CloseFile();
    }

    if (IsWindow(m_wndSubresyncBar.m_hWnd)) {
        ShowControlBar(&m_wndSubresyncBar, FALSE, TRUE);
        SetSubtitle(NULL);
    }

    if (IsWindow(m_wndCaptureBar.m_hWnd)) {
        ShowControlBar(&m_wndCaptureBar, FALSE, TRUE);
        m_wndCaptureBar.m_capdlg.SetupVideoControls(_T(""), NULL, NULL, NULL);
        m_wndCaptureBar.m_capdlg.SetupAudioControls(_T(""), NULL, CInterfaceArray<IAMAudioInputMixer>());
    }

    if (GetPlaybackMode() == PM_CAPTURE) {
        // Restore the controls
        ShowControls(AfxGetAppSettings().nCS, true);
    }

    RecalcLayout();

    SetWindowText(m_strTitle);
    m_Lcd.SetMediaTitle(LPCTSTR(m_strTitle));

    SetAlwaysOnTop(AfxGetAppSettings().iOnTop);

    // this will prevent any further UI updates on the dynamically added menu items
    SetupFiltersSubMenu();
    SetupAudioSwitcherSubMenu();
    SetupSubtitlesSubMenu();
    SetupNavAudioSubMenu();
    SetupNavSubtitleSubMenu();
    SetupNavAngleSubMenu();
    SetupNavChaptersSubMenu();
    SetupFavoritesSubMenu();
    SetupRecentFilesSubMenu();

    SendNowPlayingToMSN();
}

void CMainFrame::OnUpdateFilePostClosemedia(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(!!m_hWnd && m_iMediaLoadState == MLS_CLOSING);
}

void CMainFrame::OnBossKey()
{
    // Disable the boss key when using D3D fullscreen
    if (IsD3DFullScreenMode()) {
        return;
    }

    // Disable animation
    ANIMATIONINFO AnimationInfo;
    AnimationInfo.cbSize = sizeof(ANIMATIONINFO);
    ::SystemParametersInfo(SPI_GETANIMATION, sizeof(ANIMATIONINFO), &AnimationInfo, 0);
    int m_WindowAnimationType = AnimationInfo.iMinAnimate;
    AnimationInfo.iMinAnimate = 0;
    ::SystemParametersInfo(SPI_SETANIMATION, sizeof(ANIMATIONINFO), &AnimationInfo, 0);

    SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
    if (m_fFullScreen) {
        SendMessage(WM_COMMAND, ID_VIEW_FULLSCREEN);
    }
    SendMessage(WM_SYSCOMMAND, SC_MINIMIZE, -1);

    // Enable animation
    AnimationInfo.iMinAnimate = m_WindowAnimationType;
    ::SystemParametersInfo(SPI_SETANIMATION, sizeof(ANIMATIONINFO), &AnimationInfo, 0);
}

void CMainFrame::OnStreamAudio(UINT nID)
{
    nID -= ID_STREAM_AUDIO_NEXT;

    if (m_iMediaLoadState != MLS_LOADED) {
        return;
    }

    CComQIPtr<IAMStreamSelect> pSS = FindFilter(__uuidof(CAudioSwitcherFilter), pGB);
    if (!pSS) {
        pSS = FindFilter(CLSID_MorganStreamSwitcher, pGB);
    }

    DWORD cStreams = 0;
    if (pSS && SUCCEEDED(pSS->Count(&cStreams)) && cStreams > 1) {
        for (DWORD i = 0; i < cStreams; i++) {
            AM_MEDIA_TYPE* pmt = NULL;
            DWORD dwFlags = 0;
            LCID lcid = 0;
            DWORD dwGroup = 0;
            WCHAR* pszName = NULL;
            if (FAILED(pSS->Info(i, &pmt, &dwFlags, &lcid, &dwGroup, &pszName, NULL, NULL))) {
                return;
            }

            if (pmt) {
                DeleteMediaType(pmt);
            }
            if (pszName) {
                CoTaskMemFree(pszName);
            }

            if (dwFlags & (AMSTREAMSELECTINFO_ENABLED | AMSTREAMSELECTINFO_EXCLUSIVE)) {
                long stream_index = (i + (nID == 0 ? 1 : cStreams - 1)) % cStreams;
                pSS->Enable(stream_index, AMSTREAMSELECTENABLE_ENABLE);
                if (SUCCEEDED(pSS->Info(stream_index, &pmt, &dwFlags, &lcid, &dwGroup, &pszName, NULL, NULL))) {
                    CString strMessage;
                    strMessage.Format(IDS_AUDIO_STREAM, pszName);
                    m_OSD.DisplayMessage(OSD_TOPLEFT, strMessage);
                    if (pmt) {
                        DeleteMediaType(pmt);
                    }
                    if (pszName) {
                        CoTaskMemFree(pszName);
                    }
                }
                break;
            }
        }
    } else if (GetPlaybackMode() == PM_FILE) {
        SendMessage(WM_COMMAND, ID_OGM_AUDIO_NEXT + nID);
    } else if (GetPlaybackMode() == PM_DVD) {
        SendMessage(WM_COMMAND, ID_DVD_AUDIO_NEXT + nID);
    }
}

void CMainFrame::OnStreamSub(UINT nID)
{
    nID -= ID_STREAM_SUB_NEXT;
    if (m_iMediaLoadState != MLS_LOADED) {
        return;
    }

    int cnt = 0;
    POSITION pos = m_pSubStreams.GetHeadPosition();
    while (pos) {
        SubtitleInput& subElement = m_pSubStreams.GetNext(pos);

        if (CComQIPtr<IAMStreamSelect> pSSF = subElement.sourceFilter) {
            DWORD cStreams;
            if (FAILED(pSSF->Count(&cStreams))) {
                continue;
            }

            for (int i = 0; i < (int)cStreams; i++) {
                DWORD dwGroup;

                if (SUCCEEDED(pSSF->Info(i, NULL, NULL, NULL, &dwGroup, NULL, NULL, NULL))
                        && dwGroup == 2) {
                    cnt++;
                }
            }
        } else {
            cnt += subElement.subStream->GetStreamCount();
        }
    }

    if (cnt > 0) {
        int i = ((m_iSubtitleSel & 0x7fffffff) + (nID == 0 ? 1 : cnt - 1)) % cnt;
        m_iSubtitleSel = i | (m_iSubtitleSel & 0x80000000);
        UpdateSubtitle(true);
        SetFocus();
    } else if (GetPlaybackMode() == PM_FILE) {
        SendMessage(WM_COMMAND, ID_OGM_SUB_NEXT + nID);
    } else if (GetPlaybackMode() == PM_DVD) {
        SendMessage(WM_COMMAND, ID_DVD_SUB_NEXT + nID);
    }
}

void CMainFrame::OnStreamSubOnOff()
{
    if (m_iMediaLoadState != MLS_LOADED) {
        return;
    }

    int cnt = 0;
    POSITION pos = m_pSubStreams.GetHeadPosition();
    while (pos) {
        cnt += m_pSubStreams.GetNext(pos).subStream->GetStreamCount();
    }

    if (cnt > 0) {
        if (m_iSubtitleSel == -1) {
            m_iSubtitleSel = 0;
        } else {
            m_iSubtitleSel ^= 0x80000000;
        }
        UpdateSubtitle(true);
        SetFocus();
        AfxGetAppSettings().fEnableSubtitles = !(m_iSubtitleSel & 0x80000000);
    } else if (GetPlaybackMode() == PM_DVD) {
        SendMessage(WM_COMMAND, ID_DVD_SUB_ONOFF);
    }
}

void CMainFrame::OnOgmAudio(UINT nID)
{
    nID -= ID_OGM_AUDIO_NEXT;

    if (m_iMediaLoadState != MLS_LOADED) {
        return;
    }

    CComQIPtr<IAMStreamSelect> pSS = FindSourceSelectableFilter();
    if (!pSS) {
        return;
    }

    CAtlArray<int> snds;

    DWORD cStreams = 0;
    if (SUCCEEDED(pSS->Count(&cStreams)) && cStreams > 1) {
        INT_PTR iSel = -1;
        for (int i = 0; i < (int)cStreams; i++) {
            AM_MEDIA_TYPE* pmt = NULL;
            DWORD dwFlags = 0;
            LCID lcid = 0;
            DWORD dwGroup = 0;
            WCHAR* pszName = NULL;
            if (FAILED(pSS->Info(i, &pmt, &dwFlags, &lcid, &dwGroup, &pszName, NULL, NULL))) {
                return;
            }

            if (dwGroup == 1) {
                if (dwFlags & (AMSTREAMSELECTINFO_ENABLED | AMSTREAMSELECTINFO_EXCLUSIVE)) {
                    iSel = snds.GetCount();
                }
                snds.Add(i);
            }

            if (pmt) {
                DeleteMediaType(pmt);
            }
            if (pszName) {
                CoTaskMemFree(pszName);
            }

        }

        size_t cnt = snds.GetCount();
        if (cnt > 1 && iSel >= 0) {
            int nNewStream = snds[(iSel + (nID == 0 ? 1 : cnt - 1)) % cnt];
            pSS->Enable(nNewStream, AMSTREAMSELECTENABLE_ENABLE);

            AM_MEDIA_TYPE* pmt = NULL;
            DWORD dwFlags = 0;
            LCID lcid = 0;
            DWORD dwGroup = 0;
            WCHAR* pszName = NULL;

            if (SUCCEEDED(pSS->Info(nNewStream, &pmt, &dwFlags, &lcid, &dwGroup, &pszName, NULL, NULL))) {
                CString strMessage;
                CString audio_stream = pszName;
                int k = audio_stream.Find(_T("Audio - "));
                if (k >= 0) {
                    audio_stream = audio_stream.Right(audio_stream.GetLength() - k - 8);
                }
                strMessage.Format(IDS_AUDIO_STREAM, audio_stream);
                m_OSD.DisplayMessage(OSD_TOPLEFT, strMessage);

                if (pmt) {
                    DeleteMediaType(pmt);
                }
                if (pszName) {
                    CoTaskMemFree(pszName);
                }
            }
        }
    }
}

void CMainFrame::OnOgmSub(UINT nID)
{
    nID -= ID_OGM_SUB_NEXT;

    if (m_iMediaLoadState != MLS_LOADED) {
        return;
    }

    CComQIPtr<IAMStreamSelect> pSS = FindSourceSelectableFilter();
    if (!pSS) {
        return;
    }

    CArray<int> subs;

    DWORD cStreams = 0;
    if (SUCCEEDED(pSS->Count(&cStreams)) && cStreams > 1) {
        INT_PTR iSel = -1;
        for (int i = 0; i < (int)cStreams; i++) {
            AM_MEDIA_TYPE* pmt = NULL;
            DWORD dwFlags = 0;
            LCID lcid = 0;
            DWORD dwGroup = 0;
            WCHAR* pszName = NULL;
            if (FAILED(pSS->Info(i, &pmt, &dwFlags, &lcid, &dwGroup, &pszName, NULL, NULL))) {
                return;
            }

            if (dwGroup == 2) {
                if (dwFlags & (AMSTREAMSELECTINFO_ENABLED | AMSTREAMSELECTINFO_EXCLUSIVE)) {
                    iSel = subs.GetCount();
                }
                subs.Add(i);
            }

            if (pmt) {
                DeleteMediaType(pmt);
            }
            if (pszName) {
                CoTaskMemFree(pszName);
            }

        }

        INT_PTR cnt = subs.GetCount();
        if (cnt > 1 && iSel >= 0) {
            int nNewStream = subs[(iSel + (nID == 0 ? 1 : cnt - 1)) % cnt];
            pSS->Enable(nNewStream, AMSTREAMSELECTENABLE_ENABLE);

            AM_MEDIA_TYPE* pmt = NULL;
            DWORD dwFlags = 0;
            LCID lcid = 0;
            DWORD dwGroup = 0;
            WCHAR* pszName = NULL;
            if (SUCCEEDED(pSS->Info(nNewStream, &pmt, &dwFlags, &lcid, &dwGroup, &pszName, NULL, NULL))) {
                CString lang;
                CString strMessage;
                if (lcid == 0) {
                    lang = pszName;
                } else {
                    int len = GetLocaleInfo(lcid, LOCALE_SENGLANGUAGE, lang.GetBuffer(64), 64);
                    lang.ReleaseBufferSetLength(max(len - 1, 0));
                }

                strMessage.Format(IDS_SUBTITLE_STREAM, lang);
                m_OSD.DisplayMessage(OSD_TOPLEFT, strMessage);
                if (pmt) {
                    DeleteMediaType(pmt);
                }
                if (pszName) {
                    CoTaskMemFree(pszName);
                }
            }
        }
    }
}

void CMainFrame::OnDvdAngle(UINT nID)
{
    if (m_iMediaLoadState != MLS_LOADED) {
        return;
    }

    if (pDVDI && pDVDC) {
        ULONG ulAnglesAvailable, ulCurrentAngle;
        if (SUCCEEDED(pDVDI->GetCurrentAngle(&ulAnglesAvailable, &ulCurrentAngle)) && ulAnglesAvailable > 1) {
            ulCurrentAngle += (nID == ID_DVD_ANGLE_NEXT) ? 1 : -1;
            if (ulCurrentAngle > ulAnglesAvailable) {
                ulCurrentAngle = 1;
            } else if (ulCurrentAngle < 1) {
                ulCurrentAngle = ulAnglesAvailable;
            }
            pDVDC->SelectAngle(ulCurrentAngle, DVD_CMD_FLAG_Block, NULL);

            CString osdMessage;
            osdMessage.Format(IDS_AG_ANGLE, ulCurrentAngle);
            m_OSD.DisplayMessage(OSD_TOPLEFT, osdMessage);
        }
    }
}

void CMainFrame::OnDvdAudio(UINT nID)
{
    nID -= ID_DVD_AUDIO_NEXT;

    if (m_iMediaLoadState != MLS_LOADED) {
        return;
    }

    if (pDVDI && pDVDC) {
        ULONG nStreamsAvailable, nCurrentStream;
        if (SUCCEEDED(pDVDI->GetCurrentAudio(&nStreamsAvailable, &nCurrentStream)) && nStreamsAvailable > 1) {
            DVD_AudioAttributes AATR;
            UINT nNextStream = (nCurrentStream + (nID == 0 ? 1 : nStreamsAvailable - 1)) % nStreamsAvailable;

            HRESULT hr = pDVDC->SelectAudioStream(nNextStream, DVD_CMD_FLAG_Block, NULL);
            if (SUCCEEDED(pDVDI->GetAudioAttributes(nNextStream, &AATR))) {
                CString lang;
                CString strMessage;
                if (AATR.Language) {
                    int len = GetLocaleInfo(AATR.Language, LOCALE_SENGLANGUAGE, lang.GetBuffer(64), 64);
                    lang.ReleaseBufferSetLength(max(len - 1, 0));
                } else {
                    lang.Format(IDS_AG_UNKNOWN, nNextStream + 1);
                }

                CString format = GetDVDAudioFormatName(AATR);
                CString str("");

                if (!format.IsEmpty()) {
                    str.Format(IDS_MAINFRM_11,
                               lang,
                               format,
                               AATR.dwFrequency,
                               AATR.bQuantization,
                               AATR.bNumberOfChannels,
                               (AATR.bNumberOfChannels > 1 ? ResStr(IDS_MAINFRM_13) : ResStr(IDS_MAINFRM_12)));
                    str += FAILED(hr) ? _T(" [") + ResStr(IDS_AG_ERROR) + _T("] ") : _T("");
                    strMessage.Format(IDS_AUDIO_STREAM, str);
                    m_OSD.DisplayMessage(OSD_TOPLEFT, strMessage);
                }
            }
        }
    }
}

void CMainFrame::OnDvdSub(UINT nID)
{
    nID -= ID_DVD_SUB_NEXT;

    if (m_iMediaLoadState != MLS_LOADED) {
        return;
    }

    if (pDVDI && pDVDC) {
        ULONG ulStreamsAvailable, ulCurrentStream;
        BOOL bIsDisabled;
        if (SUCCEEDED(pDVDI->GetCurrentSubpicture(&ulStreamsAvailable, &ulCurrentStream, &bIsDisabled))
                && ulStreamsAvailable > 1) {
            //UINT nNextStream = (ulCurrentStream+(nID==0?1:ulStreamsAvailable-1))%ulStreamsAvailable;
            int nNextStream;

            if (!bIsDisabled) {
                nNextStream = ulCurrentStream + (nID == 0 ? 1 : -1);
            } else {
                nNextStream = (nID == 0 ? 0 : ulStreamsAvailable - 1);
            }

            if (!bIsDisabled && ((nNextStream < 0) || ((ULONG)nNextStream >= ulStreamsAvailable))) {
                pDVDC->SetSubpictureState(FALSE, DVD_CMD_FLAG_Block, NULL);
                m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_SUBTITLE_STREAM_OFF));
            } else {
                HRESULT hr = pDVDC->SelectSubpictureStream(nNextStream, DVD_CMD_FLAG_Block, NULL);

                DVD_SubpictureAttributes SATR;
                pDVDC->SetSubpictureState(TRUE, DVD_CMD_FLAG_Block, NULL);
                if (SUCCEEDED(pDVDI->GetSubpictureAttributes(nNextStream, &SATR))) {
                    CString lang;
                    CString strMessage;
                    int len = GetLocaleInfo(SATR.Language, LOCALE_SENGLANGUAGE, lang.GetBuffer(64), 64);
                    lang.ReleaseBufferSetLength(max(len - 1, 0));
                    lang += FAILED(hr) ? _T(" [") + ResStr(IDS_AG_ERROR) + _T("] ") : _T("");
                    strMessage.Format(IDS_SUBTITLE_STREAM, lang);
                    m_OSD.DisplayMessage(OSD_TOPLEFT, strMessage);
                }
            }
        }
    }
}

void CMainFrame::OnDvdSubOnOff()
{
    if (m_iMediaLoadState != MLS_LOADED) {
        return;
    }

    if (pDVDI && pDVDC) {
        ULONG ulStreamsAvailable, ulCurrentStream;
        BOOL bIsDisabled;
        if (SUCCEEDED(pDVDI->GetCurrentSubpicture(&ulStreamsAvailable, &ulCurrentStream, &bIsDisabled))) {
            pDVDC->SetSubpictureState(bIsDisabled, DVD_CMD_FLAG_Block, NULL);
        }
    }
}

//
// menu item handlers
//

// file

void CMainFrame::OnFileOpenQuick()
{
    if (m_iMediaLoadState == MLS_LOADING || !IsWindow(m_wndPlaylistBar)) {
        return;
    }

    const CAppSettings& s = AfxGetAppSettings();
    CString filter;
    CAtlArray<CString> mask;
    s.m_Formats.GetFilter(filter, mask);

    DWORD dwFlags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT | OFN_ENABLEINCLUDENOTIFY | OFN_NOCHANGEDIR;
    if (!s.fKeepHistory) {
        dwFlags |= OFN_DONTADDTORECENT;
    }

    COpenFileDlg fd(mask, true, NULL, NULL, dwFlags, filter, GetModalParent());
    if (fd.DoModal() != IDOK) {
        return;
    }

    CAtlList<CString> fns;

    POSITION pos = fd.GetStartPosition();
    while (pos) {
        fns.AddTail(fd.GetNextPathName(pos));
    }

    bool fMultipleFiles = false;

    if (fns.GetCount() > 1
            || fns.GetCount() == 1
            && (fns.GetHead()[fns.GetHead().GetLength() - 1] == '\\'
                || fns.GetHead()[fns.GetHead().GetLength() - 1] == '*')) {
        fMultipleFiles = true;
    }

    SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);

    ShowWindow(SW_SHOW);
    SetForegroundWindow();

    if (fns.GetCount() == 1) {
        if (OpenBD(fns.GetHead())) {
            return;
        }
    }

    m_wndPlaylistBar.Open(fns, fMultipleFiles);

    if (m_wndPlaylistBar.GetCount() == 1 && m_wndPlaylistBar.IsWindowVisible() && !m_wndPlaylistBar.IsFloating()) {
        ShowControlBar(&m_wndPlaylistBar, FALSE, TRUE);
    }

    OpenCurPlaylistItem();
}

void CMainFrame::OnFileOpenmedia()
{
    if (m_iMediaLoadState == MLS_LOADING || !IsWindow(m_wndPlaylistBar) || m_pFullscreenWnd->IsWindow()) {
        return;
    }

    static COpenDlg dlg;
    if (IsWindow(dlg.GetSafeHwnd()) && dlg.IsWindowVisible()) {
        dlg.SetForegroundWindow();
        return;
    }
    if (dlg.DoModal() != IDOK || dlg.m_fns.GetCount() == 0) {
        return;
    }

    if (!dlg.m_fAppendPlaylist) {
        SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);
    }

    ShowWindow(SW_SHOW);
    SetForegroundWindow();

    if (!dlg.m_fMultipleFiles) {
        if (OpenBD(dlg.m_fns.GetHead())) {
            return;
        }
    }

    if (dlg.m_fAppendPlaylist) {
        m_wndPlaylistBar.Append(dlg.m_fns, dlg.m_fMultipleFiles);
        return;
    }

    m_wndPlaylistBar.Open(dlg.m_fns, dlg.m_fMultipleFiles);

    if (m_wndPlaylistBar.GetCount() == 1 && m_wndPlaylistBar.IsWindowVisible() && !m_wndPlaylistBar.IsFloating()) {
        ShowControlBar(&m_wndPlaylistBar, FALSE, TRUE);
    }

    OpenCurPlaylistItem();
}

void CMainFrame::OnUpdateFileOpen(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_iMediaLoadState != MLS_LOADING);
}

BOOL CMainFrame::OnCopyData(CWnd* pWnd, COPYDATASTRUCT* pCDS)
{
    if (AfxGetMyApp()->m_fClosingState) {
        return FALSE;
    }

    CAppSettings& s = AfxGetAppSettings();

    if (pCDS->dwData != 0x6ABE51 || pCDS->cbData < sizeof(DWORD)) {
        if (s.hMasterWnd) {
            ProcessAPICommand(pCDS);
            return TRUE;
        } else {
            return FALSE;
        }
    }

    /*
    if (m_iMediaLoadState == MLS_LOADING || !IsWindow(m_wndPlaylistBar))
        return FALSE;
    */

    DWORD len = *((DWORD*)pCDS->lpData);
    TCHAR* pBuff = (TCHAR*)((DWORD*)pCDS->lpData + 1);
    TCHAR* pBuffEnd = (TCHAR*)((BYTE*)pBuff + pCDS->cbData - sizeof(DWORD));

    CAtlList<CString> cmdln;

    while (len-- > 0 && pBuff < pBuffEnd) {
        CString str(pBuff);
        pBuff += str.GetLength() + 1;

        cmdln.AddTail(str);
    }

    s.ParseCommandLine(cmdln);

    if (s.nCLSwitches & CLSW_SLAVE) {
        SendAPICommand(CMD_CONNECT, L"%d", GetSafeHwnd());
    }

    POSITION pos = s.slFilters.GetHeadPosition();
    while (pos) {
        CString fullpath = MakeFullPath(s.slFilters.GetNext(pos));

        CPath tmp(fullpath);
        tmp.RemoveFileSpec();
        tmp.AddBackslash();
        CString path = tmp;

        WIN32_FIND_DATA fd = {0};
        HANDLE hFind = FindFirstFile(fullpath, &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    continue;
                }

                CFilterMapper2 fm2(false);
                fm2.Register(path + fd.cFileName);
                while (!fm2.m_filters.IsEmpty()) {
                    if (FilterOverride* f = fm2.m_filters.RemoveTail()) {
                        f->fTemporary = true;

                        bool fFound = false;

                        POSITION pos2 = s.m_filters.GetHeadPosition();
                        while (pos2) {
                            FilterOverride* f2 = s.m_filters.GetNext(pos2);
                            if (f2->type == FilterOverride::EXTERNAL && !f2->path.CompareNoCase(f->path)) {
                                fFound = true;
                                break;
                            }
                        }

                        if (!fFound) {
                            CAutoPtr<FilterOverride> p(f);
                            s.m_filters.AddHead(p);
                        }
                    }
                }
            } while (FindNextFile(hFind, &fd));

            FindClose(hFind);
        }
    }

    bool fSetForegroundWindow = false;

    if ((s.nCLSwitches & CLSW_DVD) && !s.slFiles.IsEmpty()) {
        SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);
        fSetForegroundWindow = true;

        CAutoPtr<OpenDVDData> p(DEBUG_NEW OpenDVDData());
        if (p) {
            p->path = s.slFiles.GetHead();
            p->subs.AddTailList(&s.slSubs);
        }
        OpenMedia(p);
    } else if (s.nCLSwitches & CLSW_CD) {
        SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);
        fSetForegroundWindow = true;

        CAtlList<CString> sl;

        if (!s.slFiles.IsEmpty()) {
            GetCDROMType(s.slFiles.GetHead()[0], sl);
        } else {
            CString dir;
            dir.ReleaseBufferSetLength(GetCurrentDirectory(MAX_PATH, dir.GetBuffer(MAX_PATH)));

            GetCDROMType(dir[0], sl);

            for (TCHAR drive = 'C'; sl.IsEmpty() && drive <= 'Z'; drive++) {
                GetCDROMType(drive, sl);
            }
        }

        m_wndPlaylistBar.Open(sl, true);
        OpenCurPlaylistItem();
    } else if (!s.slFiles.IsEmpty()) {
        CAtlList<CString> sl;
        sl.AddTailList(&s.slFiles);

        ParseDirs(sl);

        bool fMulti = sl.GetCount() > 1;

        if (!fMulti) {
            sl.AddTailList(&s.slDubs);
        }

        if (OpenBD(s.slFiles.GetHead())) {
            if (fSetForegroundWindow && !(s.nCLSwitches & CLSW_NOFOCUS)) {
                SetForegroundWindow();
            }
            s.nCLSwitches &= ~CLSW_NOFOCUS;
            return true;
        } else if (!fMulti && CPath(s.slFiles.GetHead() + _T("\\VIDEO_TS")).IsDirectory()) {
            SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);
            fSetForegroundWindow = true;

            CAutoPtr<OpenDVDData> p(DEBUG_NEW OpenDVDData());
            if (p) {
                p->path = s.slFiles.GetHead();
                p->subs.AddTailList(&s.slSubs);
            }
            OpenMedia(p);
        } else {
            if (last_run && ((GetTickCount() - last_run) < 500)) {
                s.nCLSwitches |= CLSW_ADD;
            }
            last_run = GetTickCount();

            if ((s.nCLSwitches & CLSW_ADD) && m_wndPlaylistBar.GetCount() > 0) {
                m_wndPlaylistBar.Append(sl, fMulti, &s.slSubs);

                if (s.nCLSwitches & (CLSW_OPEN | CLSW_PLAY)) {
                    m_wndPlaylistBar.SetLast();
                    OpenCurPlaylistItem();
                }
            } else {
                SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);
                fSetForegroundWindow = true;

                m_wndPlaylistBar.Open(sl, fMulti, &s.slSubs);
                OpenCurPlaylistItem((s.nCLSwitches & CLSW_STARTVALID) ? s.rtStart : 0);

                s.nCLSwitches &= ~CLSW_STARTVALID;
                s.rtStart = 0;
            }
        }
    } else {
        s.nCLSwitches = CLSW_NONE;
    }

    if (fSetForegroundWindow && !(s.nCLSwitches & CLSW_NOFOCUS)) {
        SetForegroundWindow();
    }

    s.nCLSwitches &= ~CLSW_NOFOCUS;

    return TRUE;
}

int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lp, LPARAM pData)
{
    switch (uMsg) {
        case BFFM_INITIALIZED:
            //Initial directory is set here
            SendMessage(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM)(LPCTSTR)AfxGetAppSettings().strDVDPath);
            break;
        default:
            break;
    }
    return 0;
}

void CMainFrame::OnFileOpendvd()
{
    if ((m_iMediaLoadState == MLS_LOADING) || m_pFullscreenWnd->IsWindow()) {
        return;
    }

    /*
    SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);
    SetForegroundWindow();

    ShowWindow(SW_SHOW);

    CAutoPtr<OpenDVDData> p(DEBUG_NEW OpenDVDData());
    if (p)
    {
        const CAppSettings& s = AfxGetAppSettings();
        if (s.fUseDVDPath && !s.strDVDPath.IsEmpty())
        {
            p->path = s.strDVDPath;
            p->path.Replace('/', '\\');
            if (p->path[p->path.GetLength()-1] != '\\') p->path += '\\';
        }
    }
    OpenMedia(p);*/

    CAppSettings& s = AfxGetAppSettings();
    CString strTitle = ResStr(IDS_MAINFRM_46);
    CString path;

    if (SysVersion::IsVistaOrLater()) {
        CFileDialog dlg(TRUE);
        IFileOpenDialog* openDlgPtr = dlg.GetIFileOpenDialog();

        if (openDlgPtr != NULL) {
            openDlgPtr->SetTitle(strTitle);
            openDlgPtr->SetOptions(FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
            if (FAILED(openDlgPtr->Show(m_hWnd))) {
                openDlgPtr->Release();
                return;
            }
            openDlgPtr->Release();

            path = dlg.GetFolderPath();
        }
    } else {
        TCHAR _path[MAX_PATH];

        BROWSEINFO bi;
        bi.hwndOwner = m_hWnd;
        bi.pidlRoot = NULL;
        bi.pszDisplayName = _path;
        bi.lpszTitle = strTitle;
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_VALIDATE | BIF_USENEWUI | BIF_NONEWFOLDERBUTTON;
        bi.lpfn = BrowseCallbackProc;
        bi.lParam = 0;
        bi.iImage = 0;

        static LPITEMIDLIST iil;
        iil = SHBrowseForFolder(&bi);
        if (iil) {
            SHGetPathFromIDList(iil, _path);
            path = _path;
        }
    }

    if (!path.IsEmpty()) {
        s.strDVDPath = path;
        if (!OpenBD(path)) {
            CAutoPtr<OpenDVDData> p(DEBUG_NEW OpenDVDData());
            p->path = path;
            p->path.Replace('/', '\\');
            if (p->path[p->path.GetLength() - 1] != '\\') {
                p->path += '\\';
            }

            OpenMedia(p);
        }
    }
}

void CMainFrame::OnFileOpendevice()
{
    const CAppSettings& s = AfxGetAppSettings();

    if (m_iMediaLoadState == MLS_LOADING) {
        return;
    }

    SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);
    SetForegroundWindow();

    ShowWindow(SW_SHOW);

    m_wndPlaylistBar.Empty();

    CAutoPtr<OpenDeviceData> p(DEBUG_NEW OpenDeviceData());
    if (p) {
        p->DisplayName[0] = s.strAnalogVideo;
        p->DisplayName[1] = s.strAnalogAudio;
    }
    OpenMedia(p);
    if (GetPlaybackMode() == PM_CAPTURE && !s.fHideNavigation && m_iMediaLoadState == MLS_LOADED && s.iDefaultCaptureDevice == 1) {
        m_wndNavigationBar.m_navdlg.UpdateElementList();
        ShowControlBar(&m_wndNavigationBar, !s.fHideNavigation, TRUE);
    }
}

void CMainFrame::OnFileOpenCD(UINT nID)
{
    nID -= ID_FILE_OPEN_CD_START;

    nID++;
    for (TCHAR drive = 'C'; drive <= 'Z'; drive++) {
        CAtlList<CString> sl;

        switch (GetCDROMType(drive, sl)) {
            case CDROM_Audio:
            case CDROM_VideoCD:
            case CDROM_DVDVideo:
                nID--;
                break;
            default:
                break;
        }

        if (nID == 0) {
            SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);
            SetForegroundWindow();

            ShowWindow(SW_SHOW);

            m_wndPlaylistBar.Open(sl, true);
            OpenCurPlaylistItem();

            break;
        }
    }
}

void CMainFrame::OnFileReopen()
{
    if (!m_LastOpenBDPath.IsEmpty() && OpenBD(m_LastOpenBDPath)) {
        return;
    }
    OpenCurPlaylistItem();
}

void CMainFrame::OnDropFiles(HDROP hDropInfo)
{
    SetForegroundWindow();

    if (m_wndPlaylistBar.IsWindowVisible()) {
        m_wndPlaylistBar.OnDropFiles(hDropInfo);
        return;
    }

    CAtlList<CString> sl;

    UINT nFiles = ::DragQueryFile(hDropInfo, (UINT) - 1, NULL, 0);

    if (nFiles == 1) {
        CString path;
        path.ReleaseBuffer(::DragQueryFile(hDropInfo, 0, path.GetBuffer(MAX_PATH), MAX_PATH));
        if (OpenBD(path)) {
            return;
        }
    }

    for (UINT iFile = 0; iFile < nFiles; iFile++) {
        CString fn;
        fn.ReleaseBuffer(::DragQueryFile(hDropInfo, iFile, fn.GetBuffer(MAX_PATH), MAX_PATH));
        sl.AddTail(fn);
    }

    ParseDirs(sl);

    ::DragFinish(hDropInfo);

    if (sl.IsEmpty()) {
        return;
    }

    if (sl.GetCount() == 1 && m_iMediaLoadState == MLS_LOADED && m_pCAP) {
        ISubStream* pSubStream = NULL;
        if (LoadSubtitle(sl.GetHead(), &pSubStream)) {
            SetSubtitle(pSubStream); // the subtitle at the insert position according to LoadSubtitle()
            CPath p(sl.GetHead());
            p.StripPath();
            SendStatusMessage(CString((LPCTSTR)p) + ResStr(IDS_MAINFRM_47), 3000);
            return;
        }
    }

    m_wndPlaylistBar.Open(sl, true);
    OpenCurPlaylistItem();
}

void CMainFrame::OnFileSaveAs()
{
    CString ext, in = m_wndPlaylistBar.GetCurFileName(), out = in;

    if (out.Find(_T("://")) < 0) {
        ext = CPath(out).GetExtension().MakeLower();
        if (ext == _T(".cda")) {
            out = out.Left(out.GetLength() - 4) + _T(".wav");
        } else if (ext == _T(".ifo")) {
            out = out.Left(out.GetLength() - 4) + _T(".vob");
        }
    } else {
        out.Empty();
    }

    CFileDialog fd(FALSE, 0, out,
                   OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR,
                   ResStr(IDS_MAINFRM_48), GetModalParent(), 0);
    if (fd.DoModal() != IDOK || !in.CompareNoCase(fd.GetPathName())) {
        return;
    }

    CPath p(fd.GetPathName());
    if (!ext.IsEmpty()) {
        p.AddExtension(ext);
    }

    OAFilterState fs = State_Stopped;
    pMC->GetState(0, &fs);
    if (fs == State_Running) {
        pMC->Pause();
    }

    CSaveDlg dlg(in, p);
    dlg.DoModal();

    if (fs == State_Running) {
        pMC->Run();
    }
}

void CMainFrame::OnUpdateFileSaveAs(CCmdUI* pCmdUI)
{
    if (m_iMediaLoadState != MLS_LOADED || GetPlaybackMode() != PM_FILE) {
        pCmdUI->Enable(FALSE);
        return;
    }

    CString fn = m_wndPlaylistBar.GetCurFileName();
    CString ext = fn.Mid(fn.ReverseFind('.') + 1).MakeLower();

    if (fn.Find(_T("://")) >= 0) {
        pCmdUI->Enable(FALSE);
        return;
    }

    pCmdUI->Enable(TRUE);
}

bool CMainFrame::GetDIB(BYTE** ppData, long& size, bool fSilent)
{
    if (!ppData) {
        return false;
    }

    *ppData = NULL;
    size = 0;
    OAFilterState fs = GetMediaState();

    if (!(m_iMediaLoadState == MLS_LOADED && !m_fAudioOnly && (fs == State_Paused || fs == State_Running))) {
        return false;
    }

    if (fs == State_Running && !m_pCAP) {
        pMC->Pause();
        GetMediaState(); // wait for completion of the pause command
    }

    HRESULT hr = S_OK;
    CString errmsg;

    do {
        if (m_pCAP) {
            hr = m_pCAP->GetDIB(NULL, (DWORD*)&size);
            if (FAILED(hr)) {
                errmsg.Format(IDS_MAINFRM_49, hr);
                break;
            }

            *ppData = DEBUG_NEW BYTE[size];
            if (!(*ppData)) {
                return false;
            }

            hr = m_pCAP->GetDIB(*ppData, (DWORD*)&size);
            //if (FAILED(hr)) {errmsg.Format(_T("GetDIB failed, hr = %08x"), hr); break;}
            if (FAILED(hr)) {
                OnPlayPause();
                GetMediaState(); // Pause and retry to support ffdshow queuing.
                int retry = 0;
                while (FAILED(hr) && retry < 20) {
                    hr = m_pCAP->GetDIB(*ppData, (DWORD*)&size);
                    if (SUCCEEDED(hr)) {
                        break;
                    }
                    Sleep(1);
                    retry++;
                }
                if (FAILED(hr)) {
                    errmsg.Format(IDS_MAINFRM_49, hr);
                    break;
                }
            }
        } else if (m_pMFVDC) {
            // Capture with EVR
            BITMAPINFOHEADER bih = {sizeof(BITMAPINFOHEADER)};
            BYTE* pDib;
            DWORD dwSize;
            REFERENCE_TIME rtImage = 0;
            hr = m_pMFVDC->GetCurrentImage(&bih, &pDib, &dwSize, &rtImage);
            if (FAILED(hr) || dwSize == 0) {
                errmsg.Format(IDS_MAINFRM_51, hr);
                break;
            }

            size = (long)dwSize + sizeof(BITMAPINFOHEADER);
            *ppData = DEBUG_NEW BYTE[size];
            if (!(*ppData)) {
                return false;
            }
            memcpy_s(*ppData, size, &bih, sizeof(BITMAPINFOHEADER));
            memcpy_s(*ppData + sizeof(BITMAPINFOHEADER), size - sizeof(BITMAPINFOHEADER), pDib, dwSize);
            CoTaskMemFree(pDib);
        } else {
            hr = pBV->GetCurrentImage(&size, NULL);
            if (FAILED(hr) || size == 0) {
                errmsg.Format(IDS_MAINFRM_51, hr);
                break;
            }

            *ppData = DEBUG_NEW BYTE[size];
            if (!(*ppData)) {
                return false;
            }

            hr = pBV->GetCurrentImage(&size, (long*)*ppData);
            if (FAILED(hr)) {
                errmsg.Format(IDS_MAINFRM_51, hr);
                break;
            }
        }
    } while (0);

    if (!fSilent) {
        if (!errmsg.IsEmpty()) {
            AfxMessageBox(errmsg, MB_OK);
        }
    }

    if (fs == State_Running && GetMediaState() != State_Running) {
        pMC->Run();
    }

    if (FAILED(hr)) {
        SAFE_DELETE_ARRAY(*ppData);
        return false;
    }

    return true;
}

void CMainFrame::SaveDIB(LPCTSTR fn, BYTE* pData, long size)
{
    CPath path(fn);

    PBITMAPINFO bi = reinterpret_cast<PBITMAPINFO>(pData);
    PBITMAPINFOHEADER bih = &bi->bmiHeader;
    int bpp = bih->biBitCount;

    if (bpp != 16 && bpp != 24 && bpp != 32) {
        AfxMessageBox(IDS_MAINFRM_53, MB_ICONWARNING | MB_OK, 0);
        return;
    }
    int w = bih->biWidth;
    int h = abs(bih->biHeight);
    int srcpitch = w * (bpp >> 3);
    int dstpitch = (w * 3 + 3) / 4 * 4; // round w * 3 to next multiple of 4

    BYTE* p = DEBUG_NEW BYTE[dstpitch * h];

    const BYTE* src = pData + sizeof(*bih);
    if (bpp <= 8) {
        if (bih->biClrUsed) {
            src += bih->biClrUsed * sizeof(bi->bmiColors[0]);
        } else {
            src += (1 << bpp) * DWORD(sizeof(bi->bmiColors[0]));
        }
    }

    BitBltFromRGBToRGB(w, h, p, dstpitch, 24, (BYTE*)src + srcpitch * (h - 1), -srcpitch, bpp);

    {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        ULONG_PTR gdiplusToken;
        Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

        Gdiplus::Bitmap* bm = new Gdiplus::Bitmap(w, h, dstpitch, PixelFormat24bppRGB, p);

        UINT num;  // number of image decoders
        UINT size; // size, in bytes, of the image decoder array

        // How many decoders are there?
        // How big (in bytes) is the array of all ImageCodecInfo objects?
        Gdiplus::GetImageDecodersSize(&num, &size);

        // Create a buffer large enough to hold the array of ImageCodecInfo
        // objects that will be returned by GetImageDecoders.
        Gdiplus::ImageCodecInfo* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)DEBUG_NEW BYTE[size];

        // GetImageDecoders creates an array of ImageCodecInfo objects
        // and copies that array into a previously allocated buffer.
        // The third argument, imageCodecInfos, is a pointer to that buffer.
        Gdiplus::GetImageDecoders(num, size, pImageCodecInfo);

        // Find the encoder based on the extension
        CStringW ext = _T("*") + path.GetExtension();
        CLSID encoderClsid = CLSID_NULL;
        CAtlList<CStringW> extsList;
        for (UINT i = 0; i < num && encoderClsid == CLSID_NULL; i++) {
            Explode(CStringW(pImageCodecInfo[i].FilenameExtension), extsList, L";");

            POSITION pos = extsList.GetHeadPosition();
            while (pos && encoderClsid == CLSID_NULL) {
                if (extsList.GetNext(pos).CompareNoCase(ext) == 0) {
                    encoderClsid = pImageCodecInfo[i].Clsid;
                }
            }
        }

        Gdiplus::Status s = bm->Save(fn, &encoderClsid, NULL);

        // All GDI+ objects must be destroyed before GdiplusShutdown is called
        delete bm;
        delete [] pImageCodecInfo;
        Gdiplus::GdiplusShutdown(gdiplusToken);
        delete [] p;

        if (s != Gdiplus::Ok) {
            AfxMessageBox(IDS_MAINFRM_53, MB_ICONWARNING | MB_OK, 0);
            return;
        }
    }

    path.m_strPath.Replace(_T("\\\\"), _T("\\"));

    if (CDC* pDC = m_wndStatusBar.m_status.GetDC()) {
        CRect r;
        m_wndStatusBar.m_status.GetClientRect(r);
        path.CompactPath(pDC->m_hDC, r.Width());
        m_wndStatusBar.m_status.ReleaseDC(pDC);
    }

    SendStatusMessage((LPCTSTR)path, 3000);
}

void CMainFrame::SaveImage(LPCTSTR fn)
{
    BYTE* pData = NULL;
    long size = 0;

    if (GetDIB(&pData, size)) {
        SaveDIB(fn, pData, size);
        delete [] pData;

        m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_OSD_IMAGE_SAVED), 3000);
    }
}

void CMainFrame::SaveThumbnails(LPCTSTR fn)
{
    if (!pMC || !pMS || GetPlaybackMode() != PM_FILE /*&& GetPlaybackMode() != PM_DVD*/) {
        return;
    }

    REFERENCE_TIME rtPos = GetPos();
    REFERENCE_TIME rtDur = GetDur();

    if (rtDur <= 0) {
        AfxMessageBox(IDS_MAINFRM_54, MB_ICONWARNING | MB_OK, 0);
        return;
    }

    pMC->Pause();
    GetMediaState(); // wait for completion of the pause command

    CSize video, wh(0, 0), arxy(0, 0);

    if (m_pCAP) {
        wh = m_pCAP->GetVideoSize(false);
        arxy = m_pCAP->GetVideoSize(true);
    } else if (m_pMFVDC) {
        m_pMFVDC->GetNativeVideoSize(&wh, &arxy);
    } else {
        pBV->GetVideoSize(&wh.cx, &wh.cy);

        long arx = 0, ary = 0;
        CComQIPtr<IBasicVideo2> pBV2 = pBV;
        if (pBV2 && SUCCEEDED(pBV2->GetPreferredAspectRatio(&arx, &ary)) && arx > 0 && ary > 0) {
            arxy.SetSize(arx, ary);
        }
    }

    if (wh.cx <= 0 || wh.cy <= 0) {
        AfxMessageBox(IDS_MAINFRM_55, MB_ICONWARNING | MB_OK, 0);
        return;
    }

    // with the overlay mixer IBasicVideo2 won't tell the new AR when changed dynamically
    DVD_VideoAttributes VATR;
    if (GetPlaybackMode() == PM_DVD && SUCCEEDED(pDVDI->GetCurrentVideoAttributes(&VATR))) {
        arxy.SetSize(VATR.ulAspectX, VATR.ulAspectY);
    }

    video = (arxy.cx <= 0 || arxy.cy <= 0) ? wh : CSize(MulDiv(wh.cy, arxy.cx, arxy.cy), wh.cy);

    //

    const CAppSettings& s = AfxGetAppSettings();

    int cols = max(1, min(10, s.iThumbCols)), rows = max(1, min(20, s.iThumbRows));

    int margin = 5;
    int infoheight = 70;
    int width = max(256, min(2560, s.iThumbWidth));
    int height = width * video.cy / video.cx * rows / cols + infoheight;

    int dibsize = sizeof(BITMAPINFOHEADER) + width * height * 4;

    CAutoVectorPtr<BYTE> dib;
    if (!dib.Allocate(dibsize)) {
        AfxMessageBox(IDS_MAINFRM_56, MB_ICONWARNING | MB_OK, 0);
        return;
    }

    BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)(BYTE*)dib;
    memset(bih, 0, sizeof(BITMAPINFOHEADER));
    bih->biSize = sizeof(BITMAPINFOHEADER);
    bih->biWidth = width;
    bih->biHeight = height;
    bih->biPlanes = 1;
    bih->biBitCount = 32;
    bih->biCompression = BI_RGB;
    bih->biSizeImage = width * height * 4;
    memsetd(bih + 1, 0xffffff, bih->biSizeImage);

    SubPicDesc spd;
    spd.w = width;
    spd.h = height;
    spd.bpp = 32;
    spd.pitch = -width * 4;
    spd.vidrect = CRect(0, 0, width, height);
    spd.bits = (BYTE*)(bih + 1) + (width * 4) * (height - 1);

    {
        BYTE* p = (BYTE*)spd.bits;
        for (int y = 0; y < spd.h; y++, p += spd.pitch)
            for (int x = 0; x < spd.w; x++) {
                ((DWORD*)p)[x] = 0x010101 * (0xe0 + 0x08 * y / spd.h + 0x18 * (spd.w - x) / spd.w);
            }
    }

    CCritSec csSubLock;
    RECT bbox;

    for (int i = 1, pics = cols * rows; i <= pics; i++) {
        REFERENCE_TIME rt = rtDur * i / (pics + 1);
        DVD_HMSF_TIMECODE hmsf = RT2HMS_r(rt);

        SeekTo(rt);

        m_VolumeBeforeFrameStepping = m_wndToolBar.Volume;
        pBA->put_Volume(-10000);

        HRESULT hr = pFS ? pFS->Step(1, NULL) : E_FAIL;

        if (FAILED(hr)) {
            pBA->put_Volume(m_VolumeBeforeFrameStepping);
            AfxMessageBox(IDS_FRAME_STEP_ERROR_RENDERER, MB_ICONEXCLAMATION | MB_OK, 0);
            return;
        }

        HANDLE hGraphEvent = NULL;
        pME->GetEventHandle((OAEVENT*)&hGraphEvent);

        while (hGraphEvent && WaitForSingleObject(hGraphEvent, INFINITE) == WAIT_OBJECT_0) {
            LONG evCode = 0;
            LONG_PTR evParam1, evParam2;
            while (pME && SUCCEEDED(pME->GetEvent(&evCode, &evParam1, &evParam2, 0))) {
                pME->FreeEventParams(evCode, evParam1, evParam2);
                if (EC_STEP_COMPLETE == evCode) {
                    hGraphEvent = NULL;
                }
            }
        }

        pBA->put_Volume(m_VolumeBeforeFrameStepping);

        int col = (i - 1) % cols;
        int row = (i - 1) / cols;

        CSize siz((width - margin * 2) / cols, (height - margin * 2 - infoheight) / rows);
        CPoint p(margin + col * siz.cx, margin + row * siz.cy + infoheight);
        CRect r(p, siz);
        r.DeflateRect(margin, margin);

        CRenderedTextSubtitle rts(&csSubLock);
        rts.CreateDefaultStyle(0);
        rts.m_dstScreenSize.SetSize(width, height);
        STSStyle* style = DEBUG_NEW STSStyle();
        style->marginRect.SetRectEmpty();
        rts.AddStyle(_T("thumbs"), style);

        CStringW str;
        str.Format(L"{\\an7\\1c&Hffffff&\\4a&Hb0&\\bord1\\shad4\\be1}{\\p1}m %d %d l %d %d %d %d %d %d{\\p}",
                   r.left, r.top, r.right, r.top, r.right, r.bottom, r.left, r.bottom);
        rts.Add(str, true, 0, 1, _T("thumbs"));
        str.Format(L"{\\an3\\1c&Hffffff&\\3c&H000000&\\alpha&H80&\\fs16\\b1\\bord2\\shad0\\pos(%d,%d)}%02d:%02d:%02d",
                   r.right - 5, r.bottom - 3, hmsf.bHours, hmsf.bMinutes, hmsf.bSeconds);
        rts.Add(str, true, 1, 2, _T("thumbs"));

        rts.Render(spd, 0, 25, bbox);

        BYTE* pData = NULL;
        long size = 0;
        if (!GetDIB(&pData, size)) {
            return;
        }

        BITMAPINFO* bi = (BITMAPINFO*)pData;

        if (bi->bmiHeader.biBitCount != 32) {
            CString strTemp;
            strTemp.Format(IDS_MAINFRM_57, bi->bmiHeader.biBitCount);
            AfxMessageBox(strTemp);
            delete [] pData;
            return;
        }

        int sw = bi->bmiHeader.biWidth;
        int sh = abs(bi->bmiHeader.biHeight);
        int sp = sw * 4;
        const BYTE* src = pData + sizeof(bi->bmiHeader);
        if (bi->bmiHeader.biHeight >= 0) {
            src += sp * (sh - 1);
            sp = -sp;
        }

        int dp = spd.pitch;
        BYTE* dst = (BYTE*)spd.bits + spd.pitch * r.top + r.left * 4;

        for (DWORD h = r.bottom - r.top, y = 0, yd = (sh << 8) / h; h > 0; y += yd, h--) {
            DWORD yf = y & 0xff;
            DWORD yi = y >> 8;

            DWORD* s0 = (DWORD*)(src + (int)yi * sp);
            DWORD* s1 = (DWORD*)(src + (int)yi * sp + sp);
            DWORD* d = (DWORD*)dst;

            for (DWORD w = r.right - r.left, x = 0, xd = (sw << 8) / w; w > 0; x += xd, w--) {
                DWORD xf = x & 0xff;
                DWORD xi = x >> 8;

                DWORD c0 = s0[xi];
                DWORD c1 = s0[xi + 1];
                DWORD c2 = s1[xi];
                DWORD c3 = s1[xi + 1];

                c0 = ((c0 & 0xff00ff) + ((((c1 & 0xff00ff) - (c0 & 0xff00ff)) * xf) >> 8)) & 0xff00ff
                     | ((c0 & 0x00ff00) + ((((c1 & 0x00ff00) - (c0 & 0x00ff00)) * xf) >> 8)) & 0x00ff00;

                c2 = ((c2 & 0xff00ff) + ((((c3 & 0xff00ff) - (c2 & 0xff00ff)) * xf) >> 8)) & 0xff00ff
                     | ((c2 & 0x00ff00) + ((((c3 & 0x00ff00) - (c2 & 0x00ff00)) * xf) >> 8)) & 0x00ff00;

                c0 = ((c0 & 0xff00ff) + ((((c2 & 0xff00ff) - (c0 & 0xff00ff)) * yf) >> 8)) & 0xff00ff
                     | ((c0 & 0x00ff00) + ((((c2 & 0x00ff00) - (c0 & 0x00ff00)) * yf) >> 8)) & 0x00ff00;

                *d++ = c0;
            }

            dst += dp;
        }

        rts.Render(spd, 10000, 25, bbox);

        delete [] pData;
    }

    {
        CRenderedTextSubtitle rts(&csSubLock);
        rts.CreateDefaultStyle(0);
        rts.m_dstScreenSize.SetSize(width, height);
        STSStyle* style = DEBUG_NEW STSStyle();
        style->marginRect.SetRect(margin * 2, margin * 2, margin * 2, height - infoheight - margin);
        rts.AddStyle(_T("thumbs"), style);

        CStringW str;
        str.Format(L"{\\an9\\fs%d\\b1\\bord0\\shad0\\1c&Hffffff&}%s", infoheight - 10, width >= 550 ? L"Media Player Classic" : L"MPC");

        rts.Add(str, true, 0, 1, _T("thumbs"), _T(""), _T(""), CRect(0, 0, 0, 0), -1);

        DVD_HMSF_TIMECODE hmsf = RT2HMS_r(rtDur);

        CPath path(m_wndPlaylistBar.GetCurFileName());
        path.StripPath();
        CStringW fnp = (LPCTSTR)path;

        CStringW fs;
        WIN32_FIND_DATA wfd;
        HANDLE hFind = FindFirstFile(m_wndPlaylistBar.GetCurFileName(), &wfd);
        if (hFind != INVALID_HANDLE_VALUE) {
            FindClose(hFind);

            __int64 size = (__int64(wfd.nFileSizeHigh) << 32) | wfd.nFileSizeLow;
            const int MAX_FILE_SIZE_BUFFER = 65;
            WCHAR szFileSize[MAX_FILE_SIZE_BUFFER];
            StrFormatByteSizeW(size, szFileSize, MAX_FILE_SIZE_BUFFER);
            CString szByteSize;
            szByteSize.Format(_T("%I64d"), size);
            fs.Format(IDS_MAINFRM_58, szFileSize, FormatNumber(szByteSize));
        }

        CStringW ar;
        if (arxy.cx > 0 && arxy.cy > 0 && arxy.cx != wh.cx && arxy.cy != wh.cy) {
            ar.Format(L"(%d:%d)", arxy.cx, arxy.cy);
        }

        str.Format(IDS_MAINFRM_59,
                   fnp, fs, wh.cx, wh.cy, ar, hmsf.bHours, hmsf.bMinutes, hmsf.bSeconds);
        rts.Add(str, true, 0, 1, _T("thumbs"));

        rts.Render(spd, 0, 25, bbox);
    }

    SaveDIB(fn, (BYTE*)dib, dibsize);

    SeekTo(rtPos);

    m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_OSD_THUMBS_SAVED), 3000);
}

static CString MakeSnapshotFileName(LPCTSTR prefix)
{
    CTime t = CTime::GetCurrentTime();
    CString fn;
    fn.Format(_T("%s_[%s]%s"), prefix, t.Format(_T("%Y.%m.%d_%H.%M.%S")), AfxGetAppSettings().strSnapShotExt);
    return fn;
}

BOOL CMainFrame::IsRendererCompatibleWithSaveImage()
{
    BOOL result = TRUE;
    const CAppSettings& s = AfxGetAppSettings();

    if (m_fRealMediaGraph && (s.iRMVideoRendererType == VIDRNDT_RM_DEFAULT)) {
        AfxMessageBox(IDS_SCREENSHOT_ERROR_REAL, MB_ICONEXCLAMATION | MB_OK, 0);
        result = FALSE;
    } else if (m_fQuicktimeGraph && (s.iQTVideoRendererType == VIDRNDT_QT_DEFAULT)) {
        AfxMessageBox(IDS_SCREENSHOT_ERROR_QT, MB_ICONEXCLAMATION | MB_OK, 0);
        result = FALSE;
    } else if (m_fShockwaveGraph) {
        AfxMessageBox(IDS_SCREENSHOT_ERROR_SHOCKWAVE, MB_ICONEXCLAMATION | MB_OK, 0);
        result = FALSE;
    } else if (s.iDSVideoRendererType == VIDRNDT_DS_OVERLAYMIXER) {
        AfxMessageBox(IDS_SCREENSHOT_ERROR_OVERLAY, MB_ICONEXCLAMATION | MB_OK, 0);
        result = FALSE;
    }

    return result;
}

CString CMainFrame::GetVidPos() const
{
    CString posstr = _T("");
    if ((GetPlaybackMode() == PM_FILE) || (GetPlaybackMode() == PM_DVD)) {
        __int64 start, stop, pos;
        m_wndSeekBar.GetRange(start, stop);
        pos = m_wndSeekBar.GetPosReal();

        DVD_HMSF_TIMECODE tcNow = RT2HMSF(pos);
        DVD_HMSF_TIMECODE tcDur = RT2HMSF(stop);

        if (tcDur.bHours > 0 || (pos >= stop && tcNow.bHours > 0)) {
            posstr.Format(_T("%02d.%02d.%02d"), tcNow.bHours, tcNow.bMinutes, tcNow.bSeconds);
        } else {
            posstr.Format(_T("%02d.%02d"), tcNow.bMinutes, tcNow.bSeconds);
        }
    }

    return posstr;
}

void CMainFrame::OnFileSaveImage()
{
    CAppSettings& s = AfxGetAppSettings();

    /* Check if a compatible renderer is being used */
    if (!IsRendererCompatibleWithSaveImage()) {
        return;
    }

    CPath psrc(s.strSnapShotPath);

    CStringW prefix = _T("snapshot");
    if (GetPlaybackMode() == PM_FILE) {
        CPath path(m_wndPlaylistBar.GetCurFileName());
        path.StripPath();
        prefix.Format(_T("%s_snapshot_%s"), path, GetVidPos());
    } else if (GetPlaybackMode() == PM_DVD) {
        prefix.Format(_T("dvd_snapshot_%s"), GetVidPos());
    }
    psrc.Combine(s.strSnapShotPath, MakeSnapshotFileName(prefix));

    CFileDialog fd(FALSE, 0, (LPCTSTR)psrc,
                   OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR,
                   _T("BMP - Windows Bitmap (*.bmp)|*.bmp|JPG - JPEG Image (*.jpg)|*.jpg|PNG - Portable Network Graphics (*.png)|*.png||"), GetModalParent(), 0);

    if (s.strSnapShotExt == _T(".bmp")) {
        fd.m_pOFN->nFilterIndex = 1;
    } else if (s.strSnapShotExt == _T(".jpg")) {
        fd.m_pOFN->nFilterIndex = 2;
    } else if (s.strSnapShotExt == _T(".png")) {
        fd.m_pOFN->nFilterIndex = 3;
    }

    if (fd.DoModal() != IDOK) {
        return;
    }

    if (fd.m_pOFN->nFilterIndex == 1) {
        s.strSnapShotExt = _T(".bmp");
    } else if (fd.m_pOFN->nFilterIndex == 2) {
        s.strSnapShotExt = _T(".jpg");
    } else {
        fd.m_pOFN->nFilterIndex = 3;
        s.strSnapShotExt = _T(".png");
    }

    CPath pdst(fd.GetPathName());
    if (pdst.GetExtension().MakeLower() != s.strSnapShotExt) {
        pdst.RenameExtension(s.strSnapShotExt);
    }
    CString path = (LPCTSTR)pdst;
    pdst.RemoveFileSpec();
    s.strSnapShotPath = (LPCTSTR)pdst;

    SaveImage(path);
}

void CMainFrame::OnFileSaveImageAuto()
{
    const CAppSettings& s = AfxGetAppSettings();

    /* Check if a compatible renderer is being used */
    if (!IsRendererCompatibleWithSaveImage()) {
        return;
    }

    CStringW prefix = _T("snapshot");
    if (GetPlaybackMode() == PM_FILE) {
        CPath path(m_wndPlaylistBar.GetCurFileName());
        path.StripPath();
        prefix.Format(_T("%s_snapshot_%s"), path, GetVidPos());
    } else if (GetPlaybackMode() == PM_DVD) {
        prefix.Format(_T("dvd_snapshot_%s"), GetVidPos());
    }

    CString fn;
    fn.Format(_T("%s\\%s"), s.strSnapShotPath, MakeSnapshotFileName(prefix));
    SaveImage(fn);
}

void CMainFrame::OnUpdateFileSaveImage(CCmdUI* pCmdUI)
{
    OAFilterState fs = GetMediaState();
    pCmdUI->Enable(m_iMediaLoadState == MLS_LOADED && !m_fAudioOnly && (fs == State_Paused || fs == State_Running));
}

void CMainFrame::OnFileSaveThumbnails()
{
    CAppSettings& s = AfxGetAppSettings();

    /* Check if a compatible renderer is being used */
    if (!IsRendererCompatibleWithSaveImage()) {
        return;
    }

    CPath psrc(s.strSnapShotPath);
    CStringW prefix = _T("thumbs");
    if (GetPlaybackMode() == PM_FILE) {
        CPath path(m_wndPlaylistBar.GetCurFileName());
        path.StripPath();
        prefix.Format(_T("%s_thumbs"), path);
    }
    psrc.Combine(s.strSnapShotPath, MakeSnapshotFileName(prefix));

    CSaveThumbnailsDialog fd(
        s.iThumbRows, s.iThumbCols, s.iThumbWidth,
        0, (LPCTSTR)psrc,
        _T("BMP - Windows Bitmap (*.bmp)|*.bmp|JPG - JPEG Image (*.jpg)|*.jpg|PNG - Portable Network Graphics (*.png)|*.png||"), GetModalParent());

    if (s.strSnapShotExt == _T(".bmp")) {
        fd.m_pOFN->nFilterIndex = 1;
    } else if (s.strSnapShotExt == _T(".jpg")) {
        fd.m_pOFN->nFilterIndex = 2;
    } else if (s.strSnapShotExt == _T(".png")) {
        fd.m_pOFN->nFilterIndex = 3;
    }

    if (fd.DoModal() != IDOK) {
        return;
    }

    if (fd.m_pOFN->nFilterIndex == 1) {
        s.strSnapShotExt = _T(".bmp");
    } else if (fd.m_pOFN->nFilterIndex == 2) {
        s.strSnapShotExt = _T(".jpg");
    } else {
        fd.m_pOFN->nFilterIndex = 3;
        s.strSnapShotExt = _T(".png");
    }

    s.iThumbRows = fd.m_rows;
    s.iThumbCols = fd.m_cols;
    s.iThumbWidth = fd.m_width;

    CPath pdst(fd.GetPathName());
    if (pdst.GetExtension().MakeLower() != s.strSnapShotExt) {
        pdst.RenameExtension(s.strSnapShotExt);
    }
    CString path = (LPCTSTR)pdst;
    pdst.RemoveFileSpec();
    s.strSnapShotPath = (LPCTSTR)pdst;

    SaveThumbnails(path);
}

void CMainFrame::OnUpdateFileSaveThumbnails(CCmdUI* pCmdUI)
{
    OAFilterState fs = GetMediaState();
    UNREFERENCED_PARAMETER(fs);
    pCmdUI->Enable(m_iMediaLoadState == MLS_LOADED && !m_fAudioOnly && (GetPlaybackMode() == PM_FILE /*|| GetPlaybackMode() == PM_DVD*/));
}

void CMainFrame::OnFileLoadsubtitle()
{
    if (!m_pCAP) {
        AfxMessageBox(IDS_MAINFRM_60, MB_ICONINFORMATION | MB_OK, 0);
        return;
    }

    static TCHAR szFilter[] =
        _T(".srt .sub .ssa .ass .smi .psb .txt .idx .usf .xss|")
        _T("*.srt;*.sub;*.ssa;*.ass;*smi;*.psb;*.txt;*.idx;*.usf;*.xss||");

    CFileDialog fd(TRUE, NULL, NULL,
                   OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_NOCHANGEDIR,
                   szFilter, GetModalParent(), 0);

    if (fd.DoModal() != IDOK) {
        return;
    }

    ISubStream* pSubStream = NULL;
    if (LoadSubtitle(fd.GetPathName(), &pSubStream)) {
        SetSubtitle(pSubStream);    // the subtitle at the insert position according to LoadSubtitle()
    }
}

void CMainFrame::OnUpdateFileLoadsubtitle(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_iMediaLoadState == MLS_LOADED && /*m_pCAP &&*/ !m_fAudioOnly);
}

void CMainFrame::OnFileSavesubtitle()
{
    SubtitleInput subElement;
    int i = GetSubtitleInput(m_iSubtitleSel, subElement);

    if (i >= 0) {
        CLSID clsid;
        if (FAILED(subElement.subStream->GetClassID(&clsid))) {
            return;
        }

        OpenMediaData* pOMD = m_wndPlaylistBar.GetCurOMD();
        CString suggestedFileName(_T(""));
        if (OpenFileData* p = dynamic_cast<OpenFileData*>(pOMD)) {
            // HACK: get the file name from the current playlist item
            suggestedFileName = m_wndPlaylistBar.GetCurFileName();
            suggestedFileName = suggestedFileName.Left(suggestedFileName.ReverseFind('.')); // exclude the extension, it will be auto completed
        }
        delete pOMD;

        if (clsid == __uuidof(CVobSubFile)) {
            CVobSubFile* pVSF = (CVobSubFile*)(ISubStream*)subElement.subStream;

            // remember to set lpszDefExt to the first extension in the filter so that the save dialog autocompletes the extension
            // and tracks attempts to overwrite in a graceful manner
            CSaveSubtitlesFileDialog fd(m_pCAP->GetSubtitleDelay(), _T("idx"), suggestedFileName,
                                        _T("VobSub (*.idx, *.sub)|*.idx;*.sub||"), GetModalParent());

            if (fd.DoModal() == IDOK) {
                CAutoLock cAutoLock(&m_csSubLock);
                pVSF->Save(fd.GetPathName(), fd.GetDelay());
            }
        } else if (clsid == __uuidof(CRenderedTextSubtitle)) {
            CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)subElement.subStream;

            CString filter;
            // WATCH the order in GFN.h for exttype
            filter += _T("SubRip (*.srt)|*.srt|");
            filter += _T("MicroDVD (*.sub)|*.sub|");
            filter += _T("SAMI (*.smi)|*.smi|");
            filter += _T("PowerDivX (*.psb)|*.psb|");
            filter += _T("SubStation Alpha (*.ssa)|*.ssa|");
            filter += _T("Advanced SubStation Alpha (*.ass)|*.ass|");
            filter += _T("|");

            // same thing as in the case of CVobSubFile above for lpszDefExt
            CSaveSubtitlesFileDialog fd(pRTS->m_encoding, m_pCAP->GetSubtitleDelay(), _T("srt"), suggestedFileName, filter, GetModalParent());

            if (fd.DoModal() == IDOK) {
                CAutoLock cAutoLock(&m_csSubLock);
                pRTS->SaveAs(fd.GetPathName(), (exttype)(fd.m_ofn.nFilterIndex - 1), m_pCAP->GetFPS(), fd.GetDelay(), fd.GetEncoding());
            }
        }
    }
}

void CMainFrame::OnUpdateFileSavesubtitle(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_iSubtitleSel >= 0);
}

void CMainFrame::OnFileISDBSearch()
{
    CStringA url = "http://" + AfxGetAppSettings().strISDb + "/index.php?";
    CStringA args = makeargs(m_wndPlaylistBar.m_pl);
    ShellExecute(m_hWnd, _T("open"), CString(url + args), NULL, NULL, SW_SHOWDEFAULT);
}

void CMainFrame::OnUpdateFileISDBSearch(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(TRUE);
}

void CMainFrame::OnFileISDBUpload()
{
    CStringA url = "http://" + AfxGetAppSettings().strISDb + "/ul.php?";
    CStringA args = makeargs(m_wndPlaylistBar.m_pl);
    ShellExecute(m_hWnd, _T("open"), CString(url + args), NULL, NULL, SW_SHOWDEFAULT);
}

void CMainFrame::OnUpdateFileISDBUpload(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_wndPlaylistBar.GetCount() > 0);
}

void CMainFrame::OnFileISDBDownload()
{
    const CAppSettings& s = AfxGetAppSettings();
    filehash fh;
    if (!::mpc_filehash((CString)m_wndPlaylistBar.GetCurFileName(), fh)) {
        MessageBeep((UINT) - 1);
        return;
    }

    try {
        CStringA url = "http://" + s.strISDb + "/index.php?";
        CStringA args;
        args.Format("player=mpc-hc&name[0]=%s&size[0]=%016I64x&hash[0]=%016I64x",
                    UrlEncode(CStringA(fh.name), true), fh.size, fh.mpc_filehash);
        url.Append(args);

        CSubtitleDlDlg dlg(GetModalParent(), url);
        dlg.DoModal();
    } catch (CInternetException* ie) {
        ie->Delete();
        return;
    }
}

void CMainFrame::OnUpdateFileISDBDownload(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_iMediaLoadState == MLS_LOADED && m_pCAP && !m_fAudioOnly);
}

void CMainFrame::OnFileProperties()
{
    CPPageFileInfoSheet m_fileinfo(m_wndPlaylistBar.GetCurFileName(), this, GetModalParent());
    m_fileinfo.DoModal();
}

void CMainFrame::OnUpdateFileProperties(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_iMediaLoadState == MLS_LOADED && GetPlaybackMode() == PM_FILE);
}

void CMainFrame::OnFileCloseMedia()
{
    CloseMedia();
}

void CMainFrame::OnUpdateViewTearingTest(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_iMediaLoadState == MLS_LOADED && !m_fAudioOnly);
    pCmdUI->SetCheck(AfxGetMyApp()->m_Renderers.m_fTearingTest);
}

void CMainFrame::OnViewTearingTest()
{
    AfxGetMyApp()->m_Renderers.m_fTearingTest = !AfxGetMyApp()->m_Renderers.m_fTearingTest;
}

void CMainFrame::OnUpdateViewDisplayStats(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = (s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM
                      || s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS
                      || s.iDSVideoRendererType == VIDRNDT_DS_SYNC)
                     && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D;

    pCmdUI->Enable(supported && m_iMediaLoadState == MLS_LOADED && !m_fAudioOnly);
    pCmdUI->SetCheck(supported && (AfxGetMyApp()->m_Renderers.m_fDisplayStats));
}

void CMainFrame::OnViewResetStats()
{
    AfxGetMyApp()->m_Renderers.m_bResetStats = true; // Reset by "consumer"
}

void CMainFrame::OnViewDisplayStatsSC()
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = (s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM
                      || s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS
                      || s.iDSVideoRendererType == VIDRNDT_DS_SYNC)
                     && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D;

    if (supported) {
        if (!AfxGetMyApp()->m_Renderers.m_fDisplayStats) {
            AfxGetMyApp()->m_Renderers.m_bResetStats = true; // to Reset statictics on first call ...
        }

        ++AfxGetMyApp()->m_Renderers.m_fDisplayStats;
        if (AfxGetMyApp()->m_Renderers.m_fDisplayStats > 3) {
            AfxGetMyApp()->m_Renderers.m_fDisplayStats = 0;
        }
    }
}

void CMainFrame::OnUpdateViewVSync(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM
                       || s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D);

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(!supported || (r.m_AdvRendSets.iVMR9VSync));
}

void CMainFrame::OnUpdateViewVSyncOffset(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM
                       || s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D)
                     && r.m_AdvRendSets.fVMR9AlterativeVSync;

    pCmdUI->Enable(supported);
    CString Temp;
    Temp.Format(L"%d", r.m_AdvRendSets.iVMR9VSyncOffset);
    pCmdUI->SetText(Temp);
}

void CMainFrame::OnUpdateViewVSyncAccurate(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM
                       || s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D);

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(r.m_AdvRendSets.iVMR9VSyncAccurate);
}

void CMainFrame::OnUpdateViewSynchronizeVideo(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_SYNC) && GetPlaybackMode() == PM_NONE);

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(r.m_AdvRendSets.bSynchronizeVideo);
}

void CMainFrame::OnUpdateViewSynchronizeDisplay(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_SYNC) && GetPlaybackMode() == PM_NONE);

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(r.m_AdvRendSets.bSynchronizeDisplay);
}

void CMainFrame::OnUpdateViewSynchronizeNearest(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = (s.iDSVideoRendererType == VIDRNDT_DS_SYNC);

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(r.m_AdvRendSets.bSynchronizeNearest);
}

void CMainFrame::OnUpdateViewColorManagementEnable(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    const CRenderersData& rd = AfxGetMyApp()->m_Renderers;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM
                       || s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D)
                     && rd.m_bFP16Support;

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(r.m_AdvRendSets.iVMR9ColorManagementEnable);
}

void CMainFrame::OnUpdateViewColorManagementInput(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    const CRenderersData& rd = AfxGetMyApp()->m_Renderers;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM
                       || s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D)
                     && rd.m_bFP16Support
                     && r.m_AdvRendSets.iVMR9ColorManagementEnable;

    pCmdUI->Enable(supported);

    switch (pCmdUI->m_nID) {
        case ID_VIEW_CM_INPUT_AUTO:
            pCmdUI->SetCheck(r.m_AdvRendSets.iVMR9ColorManagementInput == VIDEO_SYSTEM_UNKNOWN);
            break;

        case ID_VIEW_CM_INPUT_HDTV:
            pCmdUI->SetCheck(r.m_AdvRendSets.iVMR9ColorManagementInput == VIDEO_SYSTEM_HDTV);
            break;

        case ID_VIEW_CM_INPUT_SDTV_NTSC:
            pCmdUI->SetCheck(r.m_AdvRendSets.iVMR9ColorManagementInput == VIDEO_SYSTEM_SDTV_NTSC);
            break;

        case ID_VIEW_CM_INPUT_SDTV_PAL:
            pCmdUI->SetCheck(r.m_AdvRendSets.iVMR9ColorManagementInput == VIDEO_SYSTEM_SDTV_PAL);
            break;
    }
}

void CMainFrame::OnUpdateViewColorManagementAmbientLight(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    const CRenderersData& rd = AfxGetMyApp()->m_Renderers;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM
                       || s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D)
                     && rd.m_bFP16Support &&
                     r.m_AdvRendSets.iVMR9ColorManagementEnable;

    pCmdUI->Enable(supported);

    switch (pCmdUI->m_nID) {
        case ID_VIEW_CM_AMBIENTLIGHT_BRIGHT:
            pCmdUI->SetCheck(r.m_AdvRendSets.iVMR9ColorManagementAmbientLight == AMBIENT_LIGHT_BRIGHT);
            break;
        case ID_VIEW_CM_AMBIENTLIGHT_DIM:
            pCmdUI->SetCheck(r.m_AdvRendSets.iVMR9ColorManagementAmbientLight == AMBIENT_LIGHT_DIM);
            break;
        case ID_VIEW_CM_AMBIENTLIGHT_DARK:
            pCmdUI->SetCheck(r.m_AdvRendSets.iVMR9ColorManagementAmbientLight == AMBIENT_LIGHT_DARK);
            break;
    }
}

void CMainFrame::OnUpdateViewColorManagementIntent(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    const CRenderersData& rd = AfxGetMyApp()->m_Renderers;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM
                       || s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D)
                     && rd.m_bFP16Support
                     && r.m_AdvRendSets.iVMR9ColorManagementEnable;

    pCmdUI->Enable(supported);

    switch (pCmdUI->m_nID) {
        case ID_VIEW_CM_INTENT_PERCEPTUAL:
            pCmdUI->SetCheck(r.m_AdvRendSets.iVMR9ColorManagementIntent == COLOR_RENDERING_INTENT_PERCEPTUAL);
            break;
        case ID_VIEW_CM_INTENT_RELATIVECOLORIMETRIC:
            pCmdUI->SetCheck(r.m_AdvRendSets.iVMR9ColorManagementIntent == COLOR_RENDERING_INTENT_RELATIVE_COLORIMETRIC);
            break;
        case ID_VIEW_CM_INTENT_SATURATION:
            pCmdUI->SetCheck(r.m_AdvRendSets.iVMR9ColorManagementIntent == COLOR_RENDERING_INTENT_SATURATION);
            break;
        case ID_VIEW_CM_INTENT_ABSOLUTECOLORIMETRIC:
            pCmdUI->SetCheck(r.m_AdvRendSets.iVMR9ColorManagementIntent == COLOR_RENDERING_INTENT_ABSOLUTE_COLORIMETRIC);
            break;
    }
}

void CMainFrame::OnUpdateViewEVROutputRange(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM
                       || s.iDSVideoRendererType == VIDRNDT_DS_SYNC)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D);

    pCmdUI->Enable(supported);

    if (pCmdUI->m_nID == ID_VIEW_EVROUTPUTRANGE_0_255) {
        pCmdUI->SetCheck(r.m_AdvRendSets.iEVROutputRange == 0);
    } else if (pCmdUI->m_nID == ID_VIEW_EVROUTPUTRANGE_16_235) {
        pCmdUI->SetCheck(r.m_AdvRendSets.iEVROutputRange == 1);
    }
}

void CMainFrame::OnUpdateViewFlushGPU(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM
                       || s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D);

    pCmdUI->Enable(supported);

    if (pCmdUI->m_nID == ID_VIEW_FLUSHGPU_BEFOREVSYNC) {
        pCmdUI->SetCheck(r.m_AdvRendSets.iVMRFlushGPUBeforeVSync != 0);
    } else if (pCmdUI->m_nID == ID_VIEW_FLUSHGPU_AFTERPRESENT) {
        pCmdUI->SetCheck(r.m_AdvRendSets.iVMRFlushGPUAfterPresent != 0);
    } else if (pCmdUI->m_nID == ID_VIEW_FLUSHGPU_WAIT) {
        pCmdUI->SetCheck(r.m_AdvRendSets.iVMRFlushGPUWait != 0);
    }

}

void CMainFrame::OnUpdateViewD3DFullscreen(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM
                       || s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS
                       || s.iDSVideoRendererType == VIDRNDT_DS_SYNC)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D);

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(s.fD3DFullscreen);
}

void CMainFrame::OnUpdateViewDisableDesktopComposition(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM
                       || s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS
                       || s.iDSVideoRendererType == VIDRNDT_DS_SYNC)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D
                      && SysVersion::IsVistaOrLater());

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(r.m_AdvRendSets.iVMRDisableDesktopComposition);
}

void CMainFrame::OnUpdateViewAlternativeVSync(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM
                       || s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D);

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(r.m_AdvRendSets.fVMR9AlterativeVSync);
}

void CMainFrame::OnUpdateViewFullscreenGUISupport(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM
                       || s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D);

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(r.m_AdvRendSets.iVMR9FullscreenGUISupport);
}

void CMainFrame::OnUpdateViewHighColorResolution(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    const CRenderersData& rd = AfxGetMyApp()->m_Renderers;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM
                       || s.iDSVideoRendererType == VIDRNDT_DS_SYNC)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D)
                     && rd.m_b10bitSupport;

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(r.m_AdvRendSets.iEVRHighColorResolution);
}

void CMainFrame::OnUpdateViewForceInputHighColorResolution(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    const CRenderersData& rd = AfxGetMyApp()->m_Renderers;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D)
                     && rd.m_b10bitSupport;

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(r.m_AdvRendSets.iEVRForceInputHighColorResolution);
}

void CMainFrame::OnUpdateViewFullFloatingPointProcessing(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    const CRenderersData& rd = AfxGetMyApp()->m_Renderers;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM
                       || s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D)
                     && rd.m_bFP16Support;

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(r.m_AdvRendSets.iVMR9FullFloatingPointProcessing);
}

void CMainFrame::OnUpdateViewHalfFloatingPointProcessing(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    const CRenderersData& rd = AfxGetMyApp()->m_Renderers;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM
                       || s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D)
                     && rd.m_bFP16Support;

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(r.m_AdvRendSets.iVMR9HalfFloatingPointProcessing);
}

void CMainFrame::OnUpdateViewEnableFrameTimeCorrection(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D);

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(r.m_AdvRendSets.iEVREnableFrameTimeCorrection);
}

void CMainFrame::OnUpdateViewVSyncOffsetIncrease(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = s.iDSVideoRendererType == VIDRNDT_DS_SYNC
                     || (((s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM
                           || s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS)
                          && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D)
                         && r.m_AdvRendSets.fVMR9AlterativeVSync);

    pCmdUI->Enable(supported);
}

void CMainFrame::OnUpdateViewVSyncOffsetDecrease(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = s.iDSVideoRendererType == VIDRNDT_DS_SYNC
                     || (((s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM
                           || s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS)
                          && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D)
                         && r.m_AdvRendSets.fVMR9AlterativeVSync);

    pCmdUI->Enable(supported);
}

void CMainFrame::OnViewVSync()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.iVMR9VSync = !s.m_AdvRendSets.iVMR9VSync;
    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, s.m_AdvRendSets.iVMR9VSync
                         ? ResStr(IDS_OSD_RS_VSYNC_ON)
                         : ResStr(IDS_OSD_RS_VSYNC_OFF));
}

void CMainFrame::OnViewVSyncAccurate()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.iVMR9VSyncAccurate = !s.m_AdvRendSets.iVMR9VSyncAccurate;
    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, s.m_AdvRendSets.iVMR9VSyncAccurate
                         ? ResStr(IDS_OSD_RS_ACCURATE_VSYNC_ON)
                         : ResStr(IDS_OSD_RS_ACCURATE_VSYNC_OFF));
}

void CMainFrame::OnViewSynchronizeVideo()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.bSynchronizeVideo = !s.m_AdvRendSets.bSynchronizeVideo;
    if (s.m_AdvRendSets.bSynchronizeVideo) {
        s.m_AdvRendSets.bSynchronizeDisplay = false;
        s.m_AdvRendSets.bSynchronizeNearest = false;
        s.m_AdvRendSets.iVMR9VSync = false;
        s.m_AdvRendSets.iVMR9VSyncAccurate = false;
        s.m_AdvRendSets.fVMR9AlterativeVSync = false;
    }

    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, s.m_AdvRendSets.bSynchronizeVideo
                         ? ResStr(IDS_OSD_RS_SYNC_TO_DISPLAY_ON)
                         : ResStr(IDS_OSD_RS_SYNC_TO_DISPLAY_ON));
}

void CMainFrame::OnViewSynchronizeDisplay()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.bSynchronizeDisplay = !s.m_AdvRendSets.bSynchronizeDisplay;
    if (s.m_AdvRendSets.bSynchronizeDisplay) {
        s.m_AdvRendSets.bSynchronizeVideo = false;
        s.m_AdvRendSets.bSynchronizeNearest = false;
        s.m_AdvRendSets.iVMR9VSync = false;
        s.m_AdvRendSets.iVMR9VSyncAccurate = false;
        s.m_AdvRendSets.fVMR9AlterativeVSync = false;
    }

    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, s.m_AdvRendSets.bSynchronizeDisplay
                         ? ResStr(IDS_OSD_RS_SYNC_TO_VIDEO_ON)
                         : ResStr(IDS_OSD_RS_SYNC_TO_VIDEO_ON));
}

void CMainFrame::OnViewSynchronizeNearest()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.bSynchronizeNearest = !s.m_AdvRendSets.bSynchronizeNearest;
    if (s.m_AdvRendSets.bSynchronizeNearest) {
        s.m_AdvRendSets.bSynchronizeVideo = false;
        s.m_AdvRendSets.bSynchronizeDisplay = false;
        s.m_AdvRendSets.iVMR9VSync = false;
        s.m_AdvRendSets.iVMR9VSyncAccurate = false;
        s.m_AdvRendSets.fVMR9AlterativeVSync = false;
    }

    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, s.m_AdvRendSets.bSynchronizeNearest
                         ? ResStr(IDS_OSD_RS_PRESENT_NEAREST_ON)
                         : ResStr(IDS_OSD_RS_PRESENT_NEAREST_OFF));
}

void CMainFrame::OnViewColorManagementEnable()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.iVMR9ColorManagementEnable = !s.m_AdvRendSets.iVMR9ColorManagementEnable;
    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, s.m_AdvRendSets.iVMR9ColorManagementEnable
                         ? ResStr(IDS_OSD_RS_COLOR_MANAGEMENT_ON)
                         : ResStr(IDS_OSD_RS_COLOR_MANAGEMENT_OFF));
}

void CMainFrame::OnViewColorManagementInputAuto()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.iVMR9ColorManagementInput = VIDEO_SYSTEM_UNKNOWN;
    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_OSD_RS_INPUT_TYPE_AUTO));
}

void CMainFrame::OnViewColorManagementInputHDTV()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.iVMR9ColorManagementInput = VIDEO_SYSTEM_HDTV;
    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_OSD_RS_INPUT_TYPE_HDTV));
}

void CMainFrame::OnViewColorManagementInputSDTV_NTSC()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.iVMR9ColorManagementInput = VIDEO_SYSTEM_SDTV_NTSC;
    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_OSD_RS_INPUT_TYPE_SD_NTSC));
}

void CMainFrame::OnViewColorManagementInputSDTV_PAL()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.iVMR9ColorManagementInput = VIDEO_SYSTEM_SDTV_PAL;
    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_OSD_RS_INPUT_TYPE_SD_PAL));
}

void CMainFrame::OnViewColorManagementAmbientLightBright()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.iVMR9ColorManagementAmbientLight = AMBIENT_LIGHT_BRIGHT;
    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_OSD_RS_AMBIENT_LIGHT_BRIGHT));
}

void CMainFrame::OnViewColorManagementAmbientLightDim()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.iVMR9ColorManagementAmbientLight = AMBIENT_LIGHT_DIM;
    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_OSD_RS_AMBIENT_LIGHT_DIM));
}

void CMainFrame::OnViewColorManagementAmbientLightDark()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.iVMR9ColorManagementAmbientLight = AMBIENT_LIGHT_DARK;
    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_OSD_RS_AMBIENT_LIGHT_DARK));
}

void CMainFrame::OnViewColorManagementIntentPerceptual()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.iVMR9ColorManagementIntent = COLOR_RENDERING_INTENT_PERCEPTUAL;
    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_OSD_RS_REND_INTENT_PERCEPT));
}

void CMainFrame::OnViewColorManagementIntentRelativeColorimetric()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.iVMR9ColorManagementIntent = COLOR_RENDERING_INTENT_RELATIVE_COLORIMETRIC;
    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_OSD_RS_REND_INTENT_RELATIVE));
}

void CMainFrame::OnViewColorManagementIntentSaturation()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.iVMR9ColorManagementIntent = COLOR_RENDERING_INTENT_SATURATION;
    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_OSD_RS_REND_INTENT_SATUR));
}

void CMainFrame::OnViewColorManagementIntentAbsoluteColorimetric()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.iVMR9ColorManagementIntent = COLOR_RENDERING_INTENT_ABSOLUTE_COLORIMETRIC;
    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_OSD_RS_REND_INTENT_ABSOLUTE));
}

void CMainFrame::OnViewEVROutputRange_0_255()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.iEVROutputRange = 0;
    s.UpdateData(true);
    CString strOSD;
    strOSD.Format(IDS_OSD_RS_OUTPUT_RANGE, _T("0 - 255"));
    m_OSD.DisplayMessage(OSD_TOPRIGHT, strOSD);
}

void CMainFrame::OnViewEVROutputRange_16_235()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.iEVROutputRange = 1;
    s.UpdateData(true);
    CString strOSD;
    strOSD.Format(IDS_OSD_RS_OUTPUT_RANGE, _T("16 - 235"));
    m_OSD.DisplayMessage(OSD_TOPRIGHT, strOSD);
}

void CMainFrame::OnViewFlushGPUBeforeVSync()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.iVMRFlushGPUBeforeVSync = !s.m_AdvRendSets.iVMRFlushGPUBeforeVSync;
    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, s.m_AdvRendSets.iVMRFlushGPUBeforeVSync
                         ? ResStr(IDS_OSD_RS_FLUSH_BEF_VSYNC_ON)
                         : ResStr(IDS_OSD_RS_FLUSH_BEF_VSYNC_OFF));
}

void CMainFrame::OnViewFlushGPUAfterVSync()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.iVMRFlushGPUAfterPresent = !s.m_AdvRendSets.iVMRFlushGPUAfterPresent;
    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, s.m_AdvRendSets.iVMRFlushGPUAfterPresent
                         ? ResStr(IDS_OSD_RS_FLUSH_AFT_PRES_ON)
                         : ResStr(IDS_OSD_RS_FLUSH_AFT_PRES_OFF));
}

void CMainFrame::OnViewFlushGPUWait()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.iVMRFlushGPUWait = !s.m_AdvRendSets.iVMRFlushGPUWait;
    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, s.m_AdvRendSets.iVMRFlushGPUWait
                         ? ResStr(IDS_OSD_RS_WAIT_ON)
                         : ResStr(IDS_OSD_RS_WAIT_OFF));
}

void CMainFrame::OnViewD3DFullScreen()
{
    CAppSettings& s = AfxGetAppSettings();
    s.fD3DFullscreen = !s.fD3DFullscreen;
    s.SaveSettings();
    m_OSD.DisplayMessage(OSD_TOPRIGHT, s.fD3DFullscreen
                         ? ResStr(IDS_OSD_RS_D3D_FULLSCREEN_ON)
                         : ResStr(IDS_OSD_RS_D3D_FULLSCREEN_OFF));
}

void CMainFrame::OnViewDisableDesktopComposition()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.iVMRDisableDesktopComposition = !s.m_AdvRendSets.iVMRDisableDesktopComposition;
    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, s.m_AdvRendSets.iVMRDisableDesktopComposition
                         ? ResStr(IDS_OSD_RS_NO_DESKTOP_COMP_ON)
                         : ResStr(IDS_OSD_RS_NO_DESKTOP_COMP_OFF));
}

void CMainFrame::OnViewAlternativeVSync()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.fVMR9AlterativeVSync = !s.m_AdvRendSets.fVMR9AlterativeVSync;
    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, s.m_AdvRendSets.fVMR9AlterativeVSync
                         ? ResStr(IDS_OSD_RS_ALT_VSYNC_ON)
                         : ResStr(IDS_OSD_RS_ALT_VSYNC_OFF));
}

void CMainFrame::OnViewResetDefault()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.SetDefault();
    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_OSD_RS_RESET_DEFAULT));
}

void CMainFrame::OnViewResetOptimal()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.SetOptimal();
    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_OSD_RS_RESET_OPTIMAL));
}

void CMainFrame::OnViewFullscreenGUISupport()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.iVMR9FullscreenGUISupport = !s.m_AdvRendSets.iVMR9FullscreenGUISupport;
    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, s.m_AdvRendSets.iVMR9FullscreenGUISupport
                         ? ResStr(IDS_OSD_RS_D3D_FS_GUI_SUPP_ON)
                         : ResStr(IDS_OSD_RS_D3D_FS_GUI_SUPP_OFF));
}

void CMainFrame::OnViewHighColorResolution()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.iEVRHighColorResolution = !s.m_AdvRendSets.iEVRHighColorResolution;
    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, s.m_AdvRendSets.iEVRHighColorResolution
                         ? ResStr(IDS_OSD_RS_10BIT_RBG_OUT_ON)
                         : ResStr(IDS_OSD_RS_10BIT_RBG_OUT_OFF));
}

void CMainFrame::OnViewForceInputHighColorResolution()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.iEVRForceInputHighColorResolution = !s.m_AdvRendSets.iEVRForceInputHighColorResolution;
    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, s.m_AdvRendSets.iEVRForceInputHighColorResolution
                         ? ResStr(IDS_OSD_RS_10BIT_RBG_IN_ON)
                         : ResStr(IDS_OSD_RS_10BIT_RBG_IN_OFF));
}

void CMainFrame::OnViewFullFloatingPointProcessing()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.iVMR9FullFloatingPointProcessing = !s.m_AdvRendSets.iVMR9FullFloatingPointProcessing;
    if (s.m_AdvRendSets.iVMR9FullFloatingPointProcessing) {
        s.m_AdvRendSets.iVMR9HalfFloatingPointProcessing = false;
    }
    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, s.m_AdvRendSets.iVMR9FullFloatingPointProcessing
                         ? ResStr(IDS_OSD_RS_FULL_FP_PROCESS_ON)
                         : ResStr(IDS_OSD_RS_FULL_FP_PROCESS_OFF));
}

void CMainFrame::OnViewHalfFloatingPointProcessing()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.iVMR9HalfFloatingPointProcessing = !s.m_AdvRendSets.iVMR9HalfFloatingPointProcessing;
    if (s.m_AdvRendSets.iVMR9HalfFloatingPointProcessing) {
        s.m_AdvRendSets.iVMR9FullFloatingPointProcessing = false;
    }
    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, s.m_AdvRendSets.iVMR9HalfFloatingPointProcessing
                         ? ResStr(IDS_OSD_RS_HALF_FP_PROCESS_ON)
                         : ResStr(IDS_OSD_RS_HALF_FP_PROCESS_OFF));
}

void CMainFrame::OnViewEnableFrameTimeCorrection()
{
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;
    s.m_AdvRendSets.iEVREnableFrameTimeCorrection = !s.m_AdvRendSets.iEVREnableFrameTimeCorrection;
    s.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, s.m_AdvRendSets.iEVREnableFrameTimeCorrection
                         ? ResStr(IDS_OSD_RS_FT_CORRECTION_ON)
                         : ResStr(IDS_OSD_RS_FT_CORRECTION_OFF));
}

void CMainFrame::OnViewVSyncOffsetIncrease()
{
    CAppSettings& s = AfxGetAppSettings();
    CRenderersSettings& r = s.m_RenderersSettings;
    CString strOSD;
    if (s.iDSVideoRendererType == VIDRNDT_DS_SYNC) {
        r.m_AdvRendSets.fTargetSyncOffset = r.m_AdvRendSets.fTargetSyncOffset - 0.5; // Yeah, it should be a "-"
        strOSD.Format(IDS_OSD_RS_TARGET_VSYNC_OFFSET, r.m_AdvRendSets.fTargetSyncOffset);
    } else {
        ++r.m_AdvRendSets.iVMR9VSyncOffset;
        strOSD.Format(IDS_OSD_RS_VSYNC_OFFSET, r.m_AdvRendSets.iVMR9VSyncOffset);
    }
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, strOSD);
}

void CMainFrame::OnViewVSyncOffsetDecrease()
{
    CAppSettings& s = AfxGetAppSettings();
    CRenderersSettings& r = s.m_RenderersSettings;
    CString strOSD;
    if (s.iDSVideoRendererType == VIDRNDT_DS_SYNC) {
        r.m_AdvRendSets.fTargetSyncOffset = r.m_AdvRendSets.fTargetSyncOffset + 0.5;
        strOSD.Format(IDS_OSD_RS_TARGET_VSYNC_OFFSET, r.m_AdvRendSets.fTargetSyncOffset);
    } else {
        --r.m_AdvRendSets.iVMR9VSyncOffset;
        strOSD.Format(IDS_OSD_RS_VSYNC_OFFSET, r.m_AdvRendSets.iVMR9VSyncOffset);
    }
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, strOSD);
}

void CMainFrame::OnUpdateViewRemainingTime(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    pCmdUI->Enable(s.fShowOSD && (m_iMediaLoadState != MLS_CLOSED));
    pCmdUI->SetCheck(m_bRemainingTime);
}

void CMainFrame::OnViewRemainingTime()
{
    m_bRemainingTime = !m_bRemainingTime;

    if (!m_bRemainingTime) {
        m_OSD.ClearMessage();
    }

    OnTimer(TIMER_STREAMPOSPOLLER2);
}

void CMainFrame::OnUpdateShaderToggle(CCmdUI* pCmdUI)
{
    if (m_shaderlabels.IsEmpty()) {
        pCmdUI->Enable(FALSE);
        m_bToggleShader = false;
        pCmdUI->SetCheck(0);
    } else {
        pCmdUI->Enable(TRUE);
        pCmdUI->SetCheck(m_bToggleShader);
    }
}

void CMainFrame::OnUpdateShaderToggleScreenSpace(CCmdUI* pCmdUI)
{
    if (m_shaderlabelsScreenSpace.IsEmpty()) {
        pCmdUI->Enable(FALSE);
        m_bToggleShaderScreenSpace = false;
        pCmdUI->SetCheck(0);
    } else {
        pCmdUI->Enable(TRUE);
        pCmdUI->SetCheck(m_bToggleShaderScreenSpace);
    }
}

void CMainFrame::OnShaderToggle()
{
    m_bToggleShader = !m_bToggleShader;
    if (m_bToggleShader) {
        SetShaders();
        m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_MAINFRM_65));
    } else {
        if (m_pCAP) {
            m_pCAP->SetPixelShader(NULL, NULL);
        }
        m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_MAINFRM_66));
    }
}

void CMainFrame::OnShaderToggleScreenSpace()
{
    m_bToggleShaderScreenSpace = !m_bToggleShaderScreenSpace;
    if (m_bToggleShaderScreenSpace) {
        SetShaders();
        m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_MAINFRM_PPONSCR));
    } else {
        if (m_pCAP2) {
            m_pCAP2->SetPixelShader2(NULL, NULL, true);
        }
        m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_MAINFRM_PPOFFSCR));
    }
}

void CMainFrame::OnD3DFullscreenToggle()
{
    CAppSettings& s = AfxGetAppSettings();
    CString strMsg;

    s.fD3DFullscreen = !s.fD3DFullscreen;
    strMsg = s.fD3DFullscreen ? ResStr(IDS_OSD_RS_D3D_FULLSCREEN_ON) : ResStr(IDS_OSD_RS_D3D_FULLSCREEN_OFF);

    if (m_iMediaLoadState == MLS_CLOSED) {
        m_closingmsg = strMsg;
    } else {
        m_OSD.DisplayMessage(OSD_TOPRIGHT, strMsg);
    }
}

void CMainFrame::OnFileClosePlaylist()
{
    SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);
    RestoreDefaultWindowRect();
}

void CMainFrame::OnUpdateFileClose(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_iMediaLoadState == MLS_LOADED || m_iMediaLoadState == MLS_LOADING);
}

// view

void CMainFrame::OnViewCaptionmenu()
{
    CAppSettings& s = AfxGetAppSettings();
    s.iCaptionMenuMode++;
    s.iCaptionMenuMode %= MODE_COUNT; // three states: normal->borderless->frame only->

    if (m_fFullScreen) {
        return;
    }

    DWORD dwRemove = 0, dwAdd = 0;
    DWORD dwFlags = SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOZORDER;
    HMENU hMenu = NULL;

    CRect wr;
    GetWindowRect(&wr);

    switch (s.iCaptionMenuMode) {
        case MODE_SHOWCAPTIONMENU:  // borderless -> normal
            dwAdd = WS_CAPTION | WS_THICKFRAME;
            hMenu = m_hMenuDefault;
            wr.right  += GetSystemMetrics(SM_CXSIZEFRAME) * 2;
            wr.bottom += GetSystemMetrics(SM_CYSIZEFRAME) * 2;
            wr.bottom += GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYMENU);
            break;
        case MODE_HIDEMENU:         // normal -> hidemenu
            hMenu =  NULL;
            wr.bottom -= GetSystemMetrics(SM_CYMENU);
            break;
        case MODE_FRAMEONLY:        // hidemenu -> frameonly
            dwRemove = WS_CAPTION;
            wr.right  -= 2;
            wr.bottom -= GetSystemMetrics(SM_CYCAPTION) + 2;
            if (IsZoomed()) { // If the window is maximized, we want it to stay centered.
                dwFlags &= ~SWP_NOMOVE;
                wr.left += 1;
            }
            break;
        case MODE_BORDERLESS:       // frameonly -> borderless
            dwRemove = WS_THICKFRAME;
            wr.right  -= GetSystemMetrics(SM_CXSIZEFRAME) * 2 - 2;
            wr.bottom -= GetSystemMetrics(SM_CYSIZEFRAME) * 2 - 2;
            if (IsZoomed()) { // If the window is maximized, we want it to stay centered.
                dwFlags &= ~SWP_NOMOVE;
                wr.top = 0;
                wr.left = 0;
            }
            break;
    }

    ModifyStyle(dwRemove, dwAdd, SWP_NOZORDER);
    ::SetMenu(m_hWnd, hMenu);
    if (IsZoomed()) { // If the window is maximized, we want it to stay maximized.
        dwFlags |= SWP_NOSIZE;
    }
    // NOTE: wr.left and wr.top are ignored due to SWP_NOMOVE flag
    SetWindowPos(NULL, wr.left, wr.top, wr.Width(), wr.Height(), dwFlags);

    MoveVideoWindow();
}

void CMainFrame::OnUpdateViewCaptionmenu(CCmdUI* pCmdUI)
{
    static int NEXT_MODE[] = {IDS_VIEW_HIDEMENU, IDS_VIEW_FRAMEONLY, IDS_VIEW_BORDERLESS, IDS_VIEW_CAPTIONMENU};
    int idx = (AfxGetAppSettings().iCaptionMenuMode %= MODE_COUNT);
    pCmdUI->SetText(ResStr(NEXT_MODE[idx]));
}

void CMainFrame::OnViewControlBar(UINT nID)
{
    nID -= ID_VIEW_SEEKER;
    UINT bitID = (1u << nID);

    // Remember the change
    AfxGetAppSettings().nCS ^= bitID;

    ShowControls(m_nCS ^ bitID, true);
}

void CMainFrame::OnUpdateViewControlBar(CCmdUI* pCmdUI)
{
    UINT nID = pCmdUI->m_nID - ID_VIEW_SEEKER;
    pCmdUI->SetCheck(!!(m_nCS & (1 << nID)));

    if (pCmdUI->m_nID == ID_VIEW_SEEKER) {
        pCmdUI->Enable(GetPlaybackMode() != PM_CAPTURE);
    }
}

void CMainFrame::OnViewSubresync()
{
    ShowControlBar(&m_wndSubresyncBar, !m_wndSubresyncBar.IsWindowVisible(), TRUE);
}

void CMainFrame::OnUpdateViewSubresync(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_wndSubresyncBar.IsWindowVisible());
    pCmdUI->Enable(m_pCAP && m_iSubtitleSel >= 0);
}

void CMainFrame::OnViewPlaylist()
{
    ShowControlBar(&m_wndPlaylistBar, !m_wndPlaylistBar.IsWindowVisible(), TRUE);
}

void CMainFrame::OnUpdateViewPlaylist(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_wndPlaylistBar.IsWindowVisible());
    pCmdUI->Enable(m_iMediaLoadState == MLS_CLOSED && m_iMediaLoadState != MLS_LOADED
                   || m_iMediaLoadState == MLS_LOADED /*&& (GetPlaybackMode() == PM_FILE || GetPlaybackMode() == PM_CAPTURE)*/);
}

void CMainFrame::OnViewEditListEditor()
{
    CAppSettings& s = AfxGetAppSettings();

    if (s.fEnableEDLEditor || (AfxMessageBox(IDS_MB_SHOW_EDL_EDITOR, MB_ICONQUESTION | MB_YESNO, 0) == IDYES)) {
        s.fEnableEDLEditor = true;
        ShowControlBar(&m_wndEditListEditor, !m_wndEditListEditor.IsWindowVisible(), TRUE);
    }
}

void CMainFrame::OnEDLIn()
{
    if (AfxGetAppSettings().fEnableEDLEditor && (m_iMediaLoadState == MLS_LOADED) && (GetPlaybackMode() == PM_FILE)) {
        REFERENCE_TIME rt;

        pMS->GetCurrentPosition(&rt);
        m_wndEditListEditor.SetIn(rt);
    }
}

void CMainFrame::OnUpdateEDLIn(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_wndEditListEditor.IsWindowVisible());
}

void CMainFrame::OnEDLOut()
{
    if (AfxGetAppSettings().fEnableEDLEditor && (m_iMediaLoadState == MLS_LOADED) && (GetPlaybackMode() == PM_FILE)) {
        REFERENCE_TIME rt;

        pMS->GetCurrentPosition(&rt);
        m_wndEditListEditor.SetOut(rt);
    }
}

void CMainFrame::OnUpdateEDLOut(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_wndEditListEditor.IsWindowVisible());
}

void CMainFrame::OnEDLNewClip()
{
    if (AfxGetAppSettings().fEnableEDLEditor && (m_iMediaLoadState == MLS_LOADED) && (GetPlaybackMode() == PM_FILE)) {
        REFERENCE_TIME rt;

        pMS->GetCurrentPosition(&rt);
        m_wndEditListEditor.NewClip(rt);
    }
}

void CMainFrame::OnUpdateEDLNewClip(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_wndEditListEditor.IsWindowVisible());
}

void CMainFrame::OnEDLSave()
{
    if (AfxGetAppSettings().fEnableEDLEditor && (m_iMediaLoadState == MLS_LOADED) && (GetPlaybackMode() == PM_FILE)) {
        m_wndEditListEditor.Save();
    }
}

void CMainFrame::OnUpdateEDLSave(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_wndEditListEditor.IsWindowVisible());
}

// Navigation menu
void CMainFrame::OnViewNavigation()
{
    CAppSettings& s = AfxGetAppSettings();
    s.fHideNavigation = !s.fHideNavigation;
    m_wndNavigationBar.m_navdlg.UpdateElementList();
    if (GetPlaybackMode() == PM_CAPTURE) {
        ShowControlBar(&m_wndNavigationBar, !s.fHideNavigation, TRUE);
    }
}

void CMainFrame::OnUpdateViewNavigation(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(!AfxGetAppSettings().fHideNavigation);
    pCmdUI->Enable(m_iMediaLoadState == MLS_LOADED && GetPlaybackMode() == PM_CAPTURE && AfxGetAppSettings().iDefaultCaptureDevice == 1);
}

void CMainFrame::OnViewCapture()
{
    ShowControlBar(&m_wndCaptureBar, !m_wndCaptureBar.IsWindowVisible(), TRUE);
}

void CMainFrame::OnUpdateViewCapture(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_wndCaptureBar.IsWindowVisible());
    pCmdUI->Enable(m_iMediaLoadState == MLS_LOADED && GetPlaybackMode() == PM_CAPTURE);
}

void CMainFrame::OnViewShaderEditor()
{
    ShowControlBar(&m_wndShaderEditorBar, !m_wndShaderEditorBar.IsWindowVisible(), TRUE);
    AfxGetAppSettings().fShaderEditorWasOpened = true;
}

void CMainFrame::OnUpdateViewShaderEditor(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_wndShaderEditorBar.IsWindowVisible());
    pCmdUI->Enable(TRUE);
}

void CMainFrame::OnViewMinimal()
{
    SetUIPreset(MODE_BORDERLESS, CS_NONE);
}

void CMainFrame::OnUpdateViewMinimal(CCmdUI* pCmdUI)
{
}

void CMainFrame::OnViewCompact()
{
    SetUIPreset(MODE_FRAMEONLY, CS_SEEKBAR);
}

void CMainFrame::OnUpdateViewCompact(CCmdUI* pCmdUI)
{
}

void CMainFrame::OnViewNormal()
{
    SetUIPreset(MODE_SHOWCAPTIONMENU, CS_SEEKBAR | CS_TOOLBAR | CS_STATUSBAR | CS_INFOBAR);
}

void CMainFrame::OnUpdateViewNormal(CCmdUI* pCmdUI)
{
}

void CMainFrame::SetUIPreset(int iCaptionMenuMode, UINT nCS)
{
    while (AfxGetAppSettings().iCaptionMenuMode != iCaptionMenuMode) {
        SendMessage(WM_COMMAND, ID_VIEW_CAPTIONMENU);
    }

    // Remember the change
    AfxGetAppSettings().nCS = nCS;
    // Hide seek bar on capture mode
    if (GetPlaybackMode() == PM_CAPTURE) {
        nCS &= ~CS_SEEKBAR;
    }
    ShowControls(nCS, true);
}

void CMainFrame::OnViewFullscreen()
{
    ToggleFullscreen(true, true);
}

void CMainFrame::OnViewFullscreenSecondary()
{
    ToggleFullscreen(true, false);
}

void CMainFrame::OnUpdateViewFullscreen(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_iMediaLoadState == MLS_LOADED && !m_fAudioOnly || m_fFullScreen);
    pCmdUI->SetCheck(m_fFullScreen);
}

void CMainFrame::OnViewZoom(UINT nID)
{
    double scale = (nID == ID_VIEW_ZOOM_50) ? 0.5 : (nID == ID_VIEW_ZOOM_200) ? 2.0 : 1.0;

    ZoomVideoWindow(true, scale);

    CString strODSMessage;
    strODSMessage.Format(IDS_OSD_ZOOM, scale * 100);
    m_OSD.DisplayMessage(OSD_TOPLEFT, strODSMessage, 3000);
}

void CMainFrame::OnUpdateViewZoom(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_iMediaLoadState == MLS_LOADED && !m_fAudioOnly);
}

void CMainFrame::OnViewZoomAutoFit()
{
    ZoomVideoWindow(true, GetZoomAutoFitScale(false));
    m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_OSD_ZOOM_AUTO), 3000);
}

void CMainFrame::OnViewZoomAutoFitLarger()
{
    ZoomVideoWindow(true, GetZoomAutoFitScale(true));
    m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_OSD_ZOOM_AUTO_LARGER), 3000);
}

void CMainFrame::OnViewDefaultVideoFrame(UINT nID)
{
    AfxGetAppSettings().iDefaultVideoSize = nID - ID_VIEW_VF_HALF;
    m_ZoomX = m_ZoomY = 1;
    m_PosX = m_PosY = 0.5;
    MoveVideoWindow();
}

void CMainFrame::OnUpdateViewDefaultVideoFrame(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_iMediaLoadState == MLS_LOADED && !m_fAudioOnly);

    int dvs = pCmdUI->m_nID - ID_VIEW_VF_HALF;
    if (AfxGetAppSettings().iDefaultVideoSize == dvs && pCmdUI->m_pMenu) {
        pCmdUI->m_pMenu->CheckMenuRadioItem(ID_VIEW_VF_HALF, ID_VIEW_VF_ZOOM2, pCmdUI->m_nID, MF_BYCOMMAND);
    }
}

void CMainFrame::OnViewSwitchVideoFrame()
{
    int vs = AfxGetAppSettings().iDefaultVideoSize;
    if (vs <= DVS_DOUBLE || vs == DVS_FROMOUTSIDE) {
        vs = DVS_STRETCH;
    } else if (vs == DVS_FROMINSIDE) {
        vs = DVS_ZOOM1;
    } else if (vs == DVS_ZOOM2) {
        vs = DVS_FROMOUTSIDE;
    } else {
        vs++;
    }
    switch (vs) { // TODO: Read messages from resource file
        case DVS_STRETCH:
            m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_STRETCH_TO_WINDOW));
            break;
        case DVS_FROMINSIDE:
            m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_TOUCH_WINDOW_FROM_INSIDE));
            break;
        case DVS_ZOOM1:
            m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_ZOOM1));
            break;
        case DVS_ZOOM2:
            m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_ZOOM2));
            break;
        case DVS_FROMOUTSIDE:
            m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_TOUCH_WINDOW_FROM_OUTSIDE));
            break;
    }
    AfxGetAppSettings().iDefaultVideoSize = vs;
    m_ZoomX = m_ZoomY = 1;
    m_PosX = m_PosY = 0.5;
    MoveVideoWindow();
}

void CMainFrame::OnViewKeepaspectratio()
{
    AfxGetAppSettings().fKeepAspectRatio = !AfxGetAppSettings().fKeepAspectRatio;
    MoveVideoWindow();
}

void CMainFrame::OnUpdateViewKeepaspectratio(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_iMediaLoadState == MLS_LOADED && !m_fAudioOnly);
    pCmdUI->SetCheck(AfxGetAppSettings().fKeepAspectRatio);
}

void CMainFrame::OnViewCompMonDeskARDiff()
{
    AfxGetAppSettings().fCompMonDeskARDiff = !AfxGetAppSettings().fCompMonDeskARDiff;
    MoveVideoWindow();
}

void CMainFrame::OnUpdateViewCompMonDeskARDiff(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_iMediaLoadState == MLS_LOADED && !m_fAudioOnly);
    pCmdUI->SetCheck(AfxGetAppSettings().fCompMonDeskARDiff);
}

void CMainFrame::OnViewPanNScan(UINT nID)
{
    if (m_iMediaLoadState != MLS_LOADED) {
        return;
    }

    int x = 0, y = 0;
    int dx = 0, dy = 0;

    switch (nID) {
        case ID_VIEW_RESET:
            m_ZoomX = m_ZoomY = 1.0;
            m_PosX = m_PosY = 0.5;
            m_AngleX = m_AngleY = m_AngleZ = 0;
            break;
        case ID_VIEW_INCSIZE:
            x = y = 1;
            break;
        case ID_VIEW_DECSIZE:
            x = y = -1;
            break;
        case ID_VIEW_INCWIDTH:
            x = 1;
            break;
        case ID_VIEW_DECWIDTH:
            x = -1;
            break;
        case ID_VIEW_INCHEIGHT:
            y = 1;
            break;
        case ID_VIEW_DECHEIGHT:
            y = -1;
            break;
        case ID_PANSCAN_CENTER:
            m_PosX = m_PosY = 0.5;
            break;
        case ID_PANSCAN_MOVELEFT:
            dx = -1;
            break;
        case ID_PANSCAN_MOVERIGHT:
            dx = 1;
            break;
        case ID_PANSCAN_MOVEUP:
            dy = -1;
            break;
        case ID_PANSCAN_MOVEDOWN:
            dy = 1;
            break;
        case ID_PANSCAN_MOVEUPLEFT:
            dx = dy = -1;
            break;
        case ID_PANSCAN_MOVEUPRIGHT:
            dx = 1;
            dy = -1;
            break;
        case ID_PANSCAN_MOVEDOWNLEFT:
            dx = -1;
            dy = 1;
            break;
        case ID_PANSCAN_MOVEDOWNRIGHT:
            dx = dy = 1;
            break;
        default:
            break;
    }

    if (x > 0 && m_ZoomX < 3) {
        m_ZoomX *= 1.02;
    } else if (x < 0 && m_ZoomX > 0.2) {
        m_ZoomX /= 1.02;
    }

    if (y > 0 && m_ZoomY < 3) {
        m_ZoomY *= 1.02;
    } else if (y < 0 && m_ZoomY > 0.2) {
        m_ZoomY /= 1.02;
    }

    if (dx < 0 && m_PosX > 0) {
        m_PosX = max(m_PosX - 0.005 * m_ZoomX, 0);
    } else if (dx > 0 && m_PosX < 1) {
        m_PosX = min(m_PosX + 0.005 * m_ZoomX, 1);
    }

    if (dy < 0 && m_PosY > 0) {
        m_PosY = max(m_PosY - 0.005 * m_ZoomY, 0);
    } else if (dy > 0 && m_PosY < 1) {
        m_PosY = min(m_PosY + 0.005 * m_ZoomY, 1);
    }

    MoveVideoWindow(true);
}

void CMainFrame::OnUpdateViewPanNScan(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_iMediaLoadState == MLS_LOADED && !m_fAudioOnly);
}

void CMainFrame::OnViewPanNScanPresets(UINT nID)
{
    if (m_iMediaLoadState != MLS_LOADED) {
        return;
    }

    CAppSettings& s = AfxGetAppSettings();

    nID -= ID_PANNSCAN_PRESETS_START;

    if ((INT_PTR)nID == s.m_pnspresets.GetCount()) {
        CPnSPresetsDlg dlg;
        dlg.m_pnspresets.Copy(s.m_pnspresets);
        if (dlg.DoModal() == IDOK) {
            s.m_pnspresets.Copy(dlg.m_pnspresets);
            s.SaveSettings();
        }
        return;
    }

    m_PosX = 0.5;
    m_PosY = 0.5;
    m_ZoomX = 1.0;
    m_ZoomY = 1.0;

    CString str = s.m_pnspresets[nID];

    int i = 0, j = 0;
    for (CString token = str.Tokenize(_T(","), i); !token.IsEmpty(); token = str.Tokenize(_T(","), i), j++) {
        float f = 0;
        if (_stscanf_s(token, _T("%f"), &f) != 1) {
            continue;
        }

        switch (j) {
            case 0:
                break;
            case 1:
                m_PosX = f;
                break;
            case 2:
                m_PosY = f;
                break;
            case 3:
                m_ZoomX = f;
                break;
            case 4:
                m_ZoomY = f;
                break;
            default:
                break;
        }
    }

    if (j != 5) {
        return;
    }

    m_PosX = min(max(m_PosX, 0), 1);
    m_PosY = min(max(m_PosY, 0), 1);
    m_ZoomX = min(max(m_ZoomX, 0.2), 3);
    m_ZoomY = min(max(m_ZoomY, 0.2), 3);

    MoveVideoWindow(true);
}

void CMainFrame::OnUpdateViewPanNScanPresets(CCmdUI* pCmdUI)
{
    int nID = pCmdUI->m_nID - ID_PANNSCAN_PRESETS_START;
    const CAppSettings& s = AfxGetAppSettings();
    pCmdUI->Enable(m_iMediaLoadState == MLS_LOADED && !m_fAudioOnly && nID >= 0 && nID <= s.m_pnspresets.GetCount());
}

void CMainFrame::OnViewRotate(UINT nID)
{
    if (!m_pCAP) {
        return;
    }

    switch (nID) {
        case ID_PANSCAN_ROTATEXP:
            m_AngleX += 2;
            break;
        case ID_PANSCAN_ROTATEXM:
            m_AngleX -= 2;
            break;
        case ID_PANSCAN_ROTATEYP:
            m_AngleY += 2;
            break;
        case ID_PANSCAN_ROTATEYM:
            m_AngleY -= 2;
            break;
        case ID_PANSCAN_ROTATEZP:
            m_AngleZ += 2;
            break;
        case ID_PANSCAN_ROTATEZM:
            m_AngleZ -= 2;
            break;
        default:
            return;
    }

    m_pCAP->SetVideoAngle(Vector(Vector::DegToRad(m_AngleX), Vector::DegToRad(m_AngleY), Vector::DegToRad(m_AngleZ)));

    CString info;
    info.Format(_T("x: %d, y: %d, z: %d"), m_AngleX, m_AngleY, m_AngleZ);
    SendStatusMessage(info, 3000);
}

void CMainFrame::OnUpdateViewRotate(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_iMediaLoadState == MLS_LOADED && !m_fAudioOnly && m_pCAP);
}

// FIXME
const static SIZE s_ar[] = {{0, 0}, {4, 3}, {5, 4}, {16, 9}, {235, 100}, {185, 100}};

void CMainFrame::OnViewAspectRatio(UINT nID)
{
    CSize& ar = AfxGetAppSettings().sizeAspectRatio;
    ar = s_ar[nID - ID_ASPECTRATIO_START];
    CString info;

    if (ar.cx && ar.cy) {
        info.Format(IDS_MAINFRM_68, ar.cx, ar.cy);
    } else {
        info.LoadString(IDS_MAINFRM_69);
    }
    SendStatusMessage(info, 3000);

    m_OSD.DisplayMessage(OSD_TOPLEFT, info, 3000);

    MoveVideoWindow();
}

void CMainFrame::OnUpdateViewAspectRatio(CCmdUI* pCmdUI)
{
    if (AfxGetAppSettings().sizeAspectRatio == s_ar[pCmdUI->m_nID - ID_ASPECTRATIO_START] && pCmdUI->m_pMenu) {
        pCmdUI->m_pMenu->CheckMenuRadioItem(ID_ASPECTRATIO_START, ID_ASPECTRATIO_END, pCmdUI->m_nID, MF_BYCOMMAND);
    }

    pCmdUI->Enable(m_iMediaLoadState == MLS_LOADED && !m_fAudioOnly);
}

void CMainFrame::OnViewAspectRatioNext()
{
    CSize& ar = AfxGetAppSettings().sizeAspectRatio;
    UINT nID = ID_ASPECTRATIO_START;

    for (int i = 0; i < _countof(s_ar); i++) {
        if (ar == s_ar[i]) {
            nID += (i + 1) % _countof(s_ar);
            break;
        }
    }

    OnViewAspectRatio(nID);
}

void CMainFrame::OnViewOntop(UINT nID)
{
    nID -= ID_ONTOP_NEVER;
    if (AfxGetAppSettings().iOnTop == (int)nID) {
        nID = !nID;
    }
    SetAlwaysOnTop(nID);
}

void CMainFrame::OnUpdateViewOntop(CCmdUI* pCmdUI)
{
    int onTop = pCmdUI->m_nID - ID_ONTOP_NEVER;
    if (AfxGetAppSettings().iOnTop == onTop && pCmdUI->m_pMenu) {
        pCmdUI->m_pMenu->CheckMenuRadioItem(ID_ONTOP_NEVER, ID_ONTOP_WHILEPLAYINGVIDEO, pCmdUI->m_nID, MF_BYCOMMAND);
    }
}

void CMainFrame::OnViewOptions()
{
    ShowOptions();
}

// play

void CMainFrame::OnPlayPlay()
{
    if (m_iMediaLoadState == MLS_CLOSED) {
        b_firstPlay = false;
        OpenCurPlaylistItem();
        return;
    }

    if (m_iMediaLoadState == MLS_LOADED) {
        if (GetMediaState() == State_Stopped) {
            m_dSpeedRate = 1.0;
        }

        if (GetPlaybackMode() == PM_FILE) {
            if (m_fEndOfStream) {
                SendMessage(WM_COMMAND, ID_PLAY_STOP);
            }
            pMS->SetRate(m_dSpeedRate);
            pMC->Run();
        } else if (GetPlaybackMode() == PM_DVD) {
            double dRate = 1.0;
            m_dSpeedRate = 1.0;

            pDVDC->PlayForwards(dRate, DVD_CMD_FLAG_Block, NULL);
            pDVDC->Pause(FALSE);
            pMC->Run();
        } else if (GetPlaybackMode() == PM_CAPTURE) {
            pMC->Stop(); // audio preview won't be in sync if we run it from paused state
            pMC->Run();
            if (AfxGetAppSettings().iDefaultCaptureDevice == 1) {
                CComQIPtr<IBDATuner> pTun = pGB;
                if (pTun) {
                    if (SUCCEEDED(pTun->SetChannel(AfxGetAppSettings().nDVBLastChannel))) {
                        ShowCurrentChannelInfo();
                    }
                }
            }
        }

        SetTimersPlay();
        if (m_fFrameSteppingActive) { // FIXME
            m_fFrameSteppingActive = false;
            pBA->put_Volume(m_VolumeBeforeFrameStepping);
        } else {
            pBA->put_Volume(m_wndToolBar.Volume);
        }

        SetAlwaysOnTop(AfxGetAppSettings().iOnTop);
    }

    MoveVideoWindow();
    m_Lcd.SetStatusMessage(ResStr(IDS_CONTROLS_PLAYING), 3000);
    SetPlayState(PS_PLAY);

    OnTimer(TIMER_STREAMPOSPOLLER);

    m_OpenFile = false;

    SetupEVRColorControl(); // can be configured when streaming begins

    if (b_firstPlay) {
        b_firstPlay = false;
        CString m_strOSD;

        if (GetPlaybackMode() == PM_FILE) {
            m_strOSD = m_wndPlaylistBar.GetCurFileName();
            if (!m_LastOpenBDPath.IsEmpty()) {
                m_strOSD.LoadString(ID_PLAY_PLAY);
                int i = m_strOSD.Find(_T("\n"));
                if (i > 0) {
                    m_strOSD.Delete(i, m_strOSD.GetLength() - i);
                }
                m_strOSD += _T(" BD");
            } else if (m_strOSD != _T("")) {
                m_strOSD.TrimRight('/');
                m_strOSD.Replace('\\', '/');
                m_strOSD = m_strOSD.Mid(m_strOSD.ReverseFind('/') + 1);
            }
        } else if (GetPlaybackMode() == PM_DVD) {
            m_strOSD.LoadString(ID_PLAY_PLAY);
            int i = m_strOSD.Find(_T("\n"));
            if (i > 0) {
                m_strOSD.Delete(i, m_strOSD.GetLength() - i);
            }
            m_strOSD += _T(" DVD");
        }

        if (!m_strOSD.IsEmpty()) {
            m_OSD.DisplayMessage(OSD_TOPLEFT, m_strOSD, 3000);
        }
    }
}

void CMainFrame::OnPlayPauseI()
{
    if (m_iMediaLoadState == MLS_LOADED) {

        if (GetPlaybackMode() == PM_FILE) {
            pMC->Pause();
        } else if (GetPlaybackMode() == PM_DVD) {
            pMC->Pause();
        } else if (GetPlaybackMode() == PM_CAPTURE) {
            pMC->Pause();
        }

        KillTimer(TIMER_STATS);
        SetAlwaysOnTop(AfxGetAppSettings().iOnTop);
    }

    MoveVideoWindow();
    m_Lcd.SetStatusMessage(ResStr(IDS_CONTROLS_PAUSED), 3000);
    SetPlayState(PS_PAUSE);
}

void CMainFrame::OnPlayPause()
{
    // Support ffdshow queuing.
    // To avoid black out on pause, we have to lock g_ffdshowReceive to synchronize with ReceiveMine.
    if (queue_ffdshow_support) {
        CAutoLock lck(&g_ffdshowReceive);
        return OnPlayPauseI();
    }
    OnPlayPauseI();
}

void CMainFrame::OnPlayPlaypause()
{
    OAFilterState fs = GetMediaState();
    if (fs == State_Running) {
        SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
    } else if (fs == State_Stopped || fs == State_Paused) {
        SendMessage(WM_COMMAND, ID_PLAY_PLAY);
    }
}

void CMainFrame::OnApiPause()
{
    OAFilterState fs = GetMediaState();
    if (fs == State_Running) {
        SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
    }
}
void CMainFrame::OnApiPlay()
{
    OAFilterState fs = GetMediaState();
    if (fs == State_Stopped || fs == State_Paused) {
        SendMessage(WM_COMMAND, ID_PLAY_PLAY);
    }
}

void CMainFrame::OnPlayStop()
{
    if (m_iMediaLoadState == MLS_LOADED) {
        if (GetPlaybackMode() == PM_FILE) {
            LONGLONG pos = 0;
            pMS->SetPositions(&pos, AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);
            pMC->Stop();

            // BUG: after pause or stop the netshow url source filter won't continue
            // on the next play command, unless we cheat it by setting the file name again.
            //
            // Note: WMPx may be using some undocumented interface to restart streaming.

            BeginEnumFilters(pGB, pEF, pBF) {
                CComQIPtr<IAMNetworkStatus, &IID_IAMNetworkStatus> pAMNS = pBF;
                CComQIPtr<IFileSourceFilter> pFSF = pBF;
                if (pAMNS && pFSF) {
                    WCHAR* pFN = NULL;
                    AM_MEDIA_TYPE mt;
                    if (SUCCEEDED(pFSF->GetCurFile(&pFN, &mt)) && pFN && *pFN) {
                        pFSF->Load(pFN, NULL);
                        CoTaskMemFree(pFN);
                    }
                    break;
                }
            }
            EndEnumFilters;
        } else if (GetPlaybackMode() == PM_DVD) {
            pDVDC->SetOption(DVD_ResetOnStop, TRUE);
            pMC->Stop();
            pDVDC->SetOption(DVD_ResetOnStop, FALSE);
        } else if (GetPlaybackMode() == PM_CAPTURE) {
            pMC->Stop();
        }

        m_dSpeedRate = 1.0;

        if (m_fFrameSteppingActive) { // FIXME
            m_fFrameSteppingActive = false;
            pBA->put_Volume(m_VolumeBeforeFrameStepping);
        }

        m_fEndOfStream = false;
    }

    m_nLoops = 0;

    if (m_hWnd) {
        KillTimersStop();
        MoveVideoWindow();

        if (m_iMediaLoadState == MLS_LOADED) {
            __int64 start, stop;
            m_wndSeekBar.GetRange(start, stop);
            GUID tf;
            pMS->GetTimeFormat(&tf);
            if (GetPlaybackMode() != PM_CAPTURE) {
                m_wndStatusBar.SetStatusTimer(m_wndSeekBar.GetPosReal(), stop, !!m_wndSubresyncBar.IsWindowVisible(), &tf);
            }

            SetAlwaysOnTop(AfxGetAppSettings().iOnTop);
        }
    }

    m_Lcd.SetStatusMessage(ResStr(IDS_CONTROLS_STOPPED), 3000);
    SetPlayState(PS_STOP);
}

void CMainFrame::OnUpdatePlayPauseStop(CCmdUI* pCmdUI)
{
    OAFilterState fs = m_fFrameSteppingActive ? State_Paused : GetMediaState();

    pCmdUI->SetCheck(fs == State_Running && pCmdUI->m_nID == ID_PLAY_PLAY
                     || fs == State_Paused && pCmdUI->m_nID == ID_PLAY_PAUSE
                     || fs == State_Stopped && pCmdUI->m_nID == ID_PLAY_STOP
                     || (fs == State_Paused || fs == State_Running) && pCmdUI->m_nID == ID_PLAY_PLAYPAUSE);

    bool fEnable = false;

    if (fs >= 0) {
        if (GetPlaybackMode() == PM_FILE || GetPlaybackMode() == PM_CAPTURE) {
            fEnable = true;

            if (fs == State_Stopped && pCmdUI->m_nID == ID_PLAY_PAUSE && m_fRealMediaGraph) {
                fEnable = false;    // can't go into paused state from stopped with rm
            } else if (m_fCapturing) {
                fEnable = false;
            } else if (m_fLiveWM && pCmdUI->m_nID == ID_PLAY_PAUSE) {
                fEnable = false;
            }
        } else if (GetPlaybackMode() == PM_DVD) {
            fEnable = m_iDVDDomain != DVD_DOMAIN_VideoManagerMenu
                      && m_iDVDDomain != DVD_DOMAIN_VideoTitleSetMenu;

            if (fs == State_Stopped && pCmdUI->m_nID == ID_PLAY_PAUSE) {
                fEnable = false;
            }
        }
    } else if (pCmdUI->m_nID == ID_PLAY_PLAY && m_wndPlaylistBar.GetCount() > 0) {
        fEnable = true;
    }

    pCmdUI->Enable(fEnable);
}

void CMainFrame::OnPlayFramestep(UINT nID)
{
    REFERENCE_TIME rt;

    if (pFS && m_fQuicktimeGraph) {
        if (GetMediaState() != State_Paused) {
            SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
        }

        pFS->Step(nID == ID_PLAY_FRAMESTEP ? 1 : -1, NULL);
    } else if (pFS && nID == ID_PLAY_FRAMESTEP) {
        m_OSD.EnableShowMessage(false);

        if (GetMediaState() != State_Paused && !queue_ffdshow_support) {
            SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
        }

        // To support framestep back, store the initial position when
        // stepping forward
        if (m_nStepForwardCount == 0) {
            pMS->GetCurrentPosition(&m_rtStepForwardStart);
        }

        m_fFrameSteppingActive = true;

        m_VolumeBeforeFrameStepping = m_wndToolBar.Volume;
        pBA->put_Volume(-10000);

        pFS->Step(1, NULL);

        m_OSD.EnableShowMessage();

    } else if (S_OK == pMS->IsFormatSupported(&TIME_FORMAT_FRAME)) {
        if (GetMediaState() != State_Paused) {
            SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
        }

        pMS->SetTimeFormat(&TIME_FORMAT_FRAME);
        pMS->GetCurrentPosition(&rt);
        if (nID == ID_PLAY_FRAMESTEP) {
            rt++;
        } else if (nID == ID_PLAY_FRAMESTEPCANCEL) {
            rt--;
        }
        pMS->SetPositions(&rt, AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);
        pMS->SetTimeFormat(&TIME_FORMAT_MEDIA_TIME);
    } else { //if (s.iDSVideoRendererType != VIDRNDT_DS_VMR9WINDOWED && s.iDSVideoRendererType != VIDRNDT_DS_VMR9RENDERLESS)
        if (GetMediaState() != State_Paused) {
            SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
        }

        REFERENCE_TIME rtAvgTime = 0;
        BeginEnumFilters(pGB, pEF, pBF) {
            BeginEnumPins(pBF, pEP, pPin) {
                AM_MEDIA_TYPE mt;
                pPin->ConnectionMediaType(&mt);

                if (mt.majortype == MEDIATYPE_Video && mt.formattype == FORMAT_VideoInfo) {
                    rtAvgTime = ((VIDEOINFOHEADER*)mt.pbFormat)->AvgTimePerFrame;
                } else if (mt.majortype == MEDIATYPE_Video && mt.formattype == FORMAT_VideoInfo2) {
                    rtAvgTime = ((VIDEOINFOHEADER2*)mt.pbFormat)->AvgTimePerFrame;
                }
            }
            EndEnumPins;
        }
        EndEnumFilters;

        // Exit of framestep forward : calculate the initial position
        if (m_nStepForwardCount != 0) {
            pFS->CancelStep();
            rt = m_rtStepForwardStart + m_nStepForwardCount * rtAvgTime;
            m_nStepForwardCount = 0;
        } else {
            pMS->GetCurrentPosition(&rt);
        }
        if (nID == ID_PLAY_FRAMESTEP) {
            rt += rtAvgTime;
        } else if (nID == ID_PLAY_FRAMESTEPCANCEL) {
            rt -= rtAvgTime;
        }
        pMS->SetPositions(&rt, AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);
    }
}

void CMainFrame::OnUpdatePlayFramestep(CCmdUI* pCmdUI)
{
    bool fEnable = false;

    if (m_iMediaLoadState == MLS_LOADED && !m_fAudioOnly &&
            (GetPlaybackMode() != PM_DVD || m_iDVDDomain == DVD_DOMAIN_Title) &&
            GetPlaybackMode() != PM_CAPTURE &&
            !m_fLiveWM) {
        if (S_OK == pMS->IsFormatSupported(&TIME_FORMAT_FRAME)) {
            fEnable = true;
        } else if (pCmdUI->m_nID == ID_PLAY_FRAMESTEP) {
            fEnable = true;
        } else if (pCmdUI->m_nID == ID_PLAY_FRAMESTEPCANCEL && pFS && pFS->CanStep(0, NULL) == S_OK) {
            fEnable = true;
        } else if (m_fQuicktimeGraph && pFS) {
            fEnable = true;
        }
    }

    pCmdUI->Enable(fEnable);
}

void CMainFrame::OnPlaySeek(UINT nID)
{
    CAppSettings& s = AfxGetAppSettings();

    REFERENCE_TIME dt =
        nID == ID_PLAY_SEEKBACKWARDSMALL ? -10000i64 * s.nJumpDistS :
        nID == ID_PLAY_SEEKFORWARDSMALL ? +10000i64 * s.nJumpDistS :
        nID == ID_PLAY_SEEKBACKWARDMED ? -10000i64 * s.nJumpDistM :
        nID == ID_PLAY_SEEKFORWARDMED ? +10000i64 * s.nJumpDistM :
        nID == ID_PLAY_SEEKBACKWARDLARGE ? -10000i64 * s.nJumpDistL :
        nID == ID_PLAY_SEEKFORWARDLARGE ? +10000i64 * s.nJumpDistL :
        0;

    m_nSeekDirection = (nID == ID_PLAY_SEEKBACKWARDSMALL || nID == ID_PLAY_SEEKBACKWARDMED || nID == ID_PLAY_SEEKBACKWARDLARGE) ? SEEK_DIRECTION_BACKWARD :
                       (nID == ID_PLAY_SEEKFORWARDSMALL || nID == ID_PLAY_SEEKFORWARDMED || nID == ID_PLAY_SEEKFORWARDLARGE) ? SEEK_DIRECTION_FORWARD : SEEK_DIRECTION_NONE;

    if (!dt) {
        return;
    }

    // HACK: the custom graph should support frame based seeking instead
    if (m_fShockwaveGraph) {
        dt /= 10000i64 * 100;
    }

    SeekTo(m_wndSeekBar.GetPos() + dt, s.fFastSeek);
}

void CMainFrame::SetTimersPlay()
{
    SetTimer(TIMER_STREAMPOSPOLLER, 40, NULL);
    SetTimer(TIMER_STREAMPOSPOLLER2, 500, NULL);
    SetTimer(TIMER_STATS, 1000, NULL);
}

void CMainFrame::KillTimersStop()
{
    KillTimer(TIMER_STREAMPOSPOLLER2);
    KillTimer(TIMER_STREAMPOSPOLLER);
    KillTimer(TIMER_STATS);
    KillTimer(TIMER_DVBINFO_UPDATER);
}

static int rangebsearch(REFERENCE_TIME val, CAtlArray<REFERENCE_TIME>& rta)
{
    int i = 0, j = (int)rta.GetCount() - 1, ret = -1;

    if (j >= 0 && val >= rta[j]) {
        return j;
    }

    while (i < j) {
        int mid = (i + j) >> 1;
        REFERENCE_TIME midt = rta[mid];
        if (val == midt) {
            ret = mid;
            break;
        } else if (val < midt) {
            ret = -1;
            if (j == mid) {
                mid--;
            }
            j = mid;
        } else if (val > midt) {
            ret = mid;
            if (i == mid) {
                mid++;
            }
            i = mid;
        }
    }

    return ret;
}

void CMainFrame::OnPlaySeekKey(UINT nID)
{
    if (m_kfs.GetCount() > 0) {

        if (GetMediaState() == State_Stopped) {
            SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
        }

        HRESULT hr;
        REFERENCE_TIME rtCurrent, rtDur;
        hr = pMS->GetCurrentPosition(&rtCurrent);
        hr = pMS->GetDuration(&rtDur);
        int dec = 1;
        int i = rangebsearch(rtCurrent, m_kfs);

        if (i > 0) {
            dec = (UINT)max(min(rtCurrent - m_kfs[i - 1], 10000000), 0);
        }

        rtCurrent =
            nID == ID_PLAY_SEEKKEYBACKWARD ? max(rtCurrent - dec, 0) :
            nID == ID_PLAY_SEEKKEYFORWARD ? rtCurrent : 0;

        i = rangebsearch(rtCurrent, m_kfs);

        if (nID == ID_PLAY_SEEKKEYBACKWARD) {
            rtCurrent = m_kfs[max(i, 0)];
        } else if (nID == ID_PLAY_SEEKKEYFORWARD && i < (int)m_kfs.GetCount() - 1) {
            rtCurrent = m_kfs[i + 1];
        } else {
            return;
        }

        // HACK: if d3d or something changes fpu control word the values of
        // m_kfs may be different now (if it was asked again), adding a little
        // to the seek position eliminates this error usually.

        rtCurrent += 10;

        hr = pMS->SetPositions(
                 &rtCurrent, AM_SEEKING_AbsolutePositioning | AM_SEEKING_SeekToKeyFrame,
                 NULL, AM_SEEKING_NoPositioning);

        m_OSD.DisplayMessage(OSD_TOPLEFT, m_wndStatusBar.GetStatusTimer(), 1500);
    }
}

void CMainFrame::OnUpdatePlaySeek(CCmdUI* pCmdUI)
{
    bool fEnable = false;
    OAFilterState fs = GetMediaState();

    if (m_iMediaLoadState == MLS_LOADED && (fs == State_Paused || fs == State_Running)) {
        fEnable = true;
        if (GetPlaybackMode() == PM_DVD && (m_iDVDDomain != DVD_DOMAIN_Title || fs != State_Running)) {
            fEnable = false;
        } else if (GetPlaybackMode() == PM_CAPTURE) {
            fEnable = false;
        }
    }

    pCmdUI->Enable(fEnable);
}

void CMainFrame::OnPlayGoto()
{
    if ((m_iMediaLoadState != MLS_LOADED) || m_pFullscreenWnd->IsWindow()) {
        return;
    }

    REFTIME atpf = 0;
    if (FAILED(pBV->get_AvgTimePerFrame(&atpf)) || atpf < 0) {
        atpf = 0;

        BeginEnumFilters(pGB, pEF, pBF) {
            if (atpf > 0) {
                break;
            }

            BeginEnumPins(pBF, pEP, pPin) {
                if (atpf > 0) {
                    break;
                }

                AM_MEDIA_TYPE mt;
                pPin->ConnectionMediaType(&mt);

                if (mt.majortype == MEDIATYPE_Video && mt.formattype == FORMAT_VideoInfo) {
                    atpf = (REFTIME)((VIDEOINFOHEADER*)mt.pbFormat)->AvgTimePerFrame / 10000000i64;
                } else if (mt.majortype == MEDIATYPE_Video && mt.formattype == FORMAT_VideoInfo2) {
                    atpf = (REFTIME)((VIDEOINFOHEADER2*)mt.pbFormat)->AvgTimePerFrame / 10000000i64;
                }
            }
            EndEnumPins;
        }
        EndEnumFilters;
    }

    REFERENCE_TIME start, dur = -1;
    m_wndSeekBar.GetRange(start, dur);
    CGoToDlg dlg(m_wndSeekBar.GetPos(), dur, atpf > 0 ? (1.0 / atpf) : 0);
    if (IDOK != dlg.DoModal() || dlg.m_time < 0) {
        return;
    }

    SeekTo(dlg.m_time);
}

void CMainFrame::OnUpdateGoto(CCmdUI* pCmdUI)
{
    bool fEnable = false;

    if (m_iMediaLoadState == MLS_LOADED) {
        fEnable = true;
        if (GetPlaybackMode() == PM_DVD && m_iDVDDomain != DVD_DOMAIN_Title) {
            fEnable = false;
        } else if (GetPlaybackMode() == PM_CAPTURE) {
            fEnable = false;
        }
    }

    pCmdUI->Enable(fEnable);
}

void CMainFrame::SetPlayingRate(double rate)
{
    if (m_iMediaLoadState != MLS_LOADED) {
        return;
    }
    HRESULT hr = E_FAIL;
    if (GetPlaybackMode() == PM_FILE) {
        if (rate < 0.125) {
            if (GetMediaState() != State_Paused) {
                SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
            }
            return;
        } else {
            if (GetMediaState() != State_Running) {
                SendMessage(WM_COMMAND, ID_PLAY_PLAY);
            }
            hr = pMS->SetRate(rate);
        }
    } else if (GetPlaybackMode() == PM_DVD) {
        if (GetMediaState() != State_Running) {
            SendMessage(WM_COMMAND, ID_PLAY_PLAY);
        }
        if (rate > 0) {
            hr = pDVDC->PlayForwards(rate, DVD_CMD_FLAG_Block, NULL);
        } else {
            hr = pDVDC->PlayBackwards(-rate, DVD_CMD_FLAG_Block, NULL);
        }
    }
    if (SUCCEEDED(hr)) {
        m_dSpeedRate = rate;
        CString strODSMessage;
        strODSMessage.Format(IDS_OSD_SPEED, rate);
        m_OSD.DisplayMessage(OSD_TOPRIGHT, strODSMessage);
    }
}

void CMainFrame::OnPlayChangeRate(UINT nID)
{
    if (m_iMediaLoadState != MLS_LOADED) {
        return;
    }

    if (GetPlaybackMode() == PM_CAPTURE) {
        if (GetMediaState() != State_Running) {
            SendMessage(WM_COMMAND, ID_PLAY_PLAY);
        }

        long lChannelMin = 0, lChannelMax = 0;
        pAMTuner->ChannelMinMax(&lChannelMin, &lChannelMax);
        long lChannel = 0, lVivSub = 0, lAudSub = 0;
        pAMTuner->get_Channel(&lChannel, &lVivSub, &lAudSub);

        long lFreqOrg = 0, lFreqNew = -1;
        pAMTuner->get_VideoFrequency(&lFreqOrg);

        //long lSignalStrength;
        do {
            if (nID == ID_PLAY_DECRATE) {
                lChannel--;
            } else if (nID == ID_PLAY_INCRATE) {
                lChannel++;
            }

            //if (lChannel < lChannelMin) lChannel = lChannelMax;
            //if (lChannel > lChannelMax) lChannel = lChannelMin;

            if (lChannel < lChannelMin || lChannel > lChannelMax) {
                break;
            }

            if (FAILED(pAMTuner->put_Channel(lChannel, AMTUNER_SUBCHAN_DEFAULT, AMTUNER_SUBCHAN_DEFAULT))) {
                break;
            }

            long flFoundSignal;
            pAMTuner->AutoTune(lChannel, &flFoundSignal);

            pAMTuner->get_VideoFrequency(&lFreqNew);
        } while (FALSE);
        /*SUCCEEDED(pAMTuner->SignalPresent(&lSignalStrength))
          && (lSignalStrength != AMTUNER_SIGNALPRESENT || lFreqNew == lFreqOrg));*/

    } else {
        if (GetPlaybackMode() == PM_DVD) {
            if (nID == ID_PLAY_INCRATE) {
                if (m_dSpeedRate > 0) {
                    SetPlayingRate(m_dSpeedRate * 2.0);
                } else if (m_dSpeedRate >= -1) {
                    SetPlayingRate(1);
                } else {
                    SetPlayingRate(m_dSpeedRate / 2.0);
                }
            } else if (nID == ID_PLAY_DECRATE) {
                if (m_dSpeedRate < 0) {
                    SetPlayingRate(m_dSpeedRate * 2.0);
                } else if (m_dSpeedRate <= 1) {
                    SetPlayingRate(-1);
                } else {
                    SetPlayingRate(m_dSpeedRate / 2.0);
                }
            }
        } else {
            const CAppSettings& s = AfxGetAppSettings();
            double dSpeedStep = s.nSpeedStep / 100.0;

            if (nID == ID_PLAY_INCRATE) {
                if (s.nSpeedStep > 0) {
                    SetPlayingRate(m_dSpeedRate + dSpeedStep);
                } else {
                    SetPlayingRate(m_dSpeedRate * 2.0);
                }
            } else if (nID == ID_PLAY_DECRATE) {
                if (s.nSpeedStep > 0) {
                    SetPlayingRate(m_dSpeedRate - dSpeedStep);
                } else {
                    SetPlayingRate(m_dSpeedRate / 2.0);
                }
            }
        }

    }
}

void CMainFrame::OnUpdatePlayChangeRate(CCmdUI* pCmdUI)
{
    bool fEnable = false;

    if (m_iMediaLoadState == MLS_LOADED) {
        bool fInc = pCmdUI->m_nID == ID_PLAY_INCRATE;

        fEnable = true;
        if (fInc && m_dSpeedRate >= 128.0) {
            fEnable = false;
        } else if (!fInc && GetPlaybackMode() == PM_FILE &&  m_dSpeedRate < 0.125) {
            fEnable = false;
        } else if (!fInc && GetPlaybackMode() == PM_DVD && m_dSpeedRate <= -128.0) {
            fEnable = false;
        } else if (GetPlaybackMode() == PM_DVD && m_iDVDDomain != DVD_DOMAIN_Title) {
            fEnable = false;
        } else if (m_fRealMediaGraph || m_fShockwaveGraph) {
            fEnable = false;
        } else if (GetPlaybackMode() == PM_CAPTURE && (!m_wndCaptureBar.m_capdlg.IsTunerActive() || m_fCapturing)) {
            fEnable = false;
        } else if (m_fLiveWM) {
            fEnable = false;
        }
    }

    pCmdUI->Enable(fEnable);
}

void CMainFrame::OnPlayResetRate()
{
    if (m_iMediaLoadState != MLS_LOADED) {
        return;
    }

    HRESULT hr = E_FAIL;

    if (GetMediaState() != State_Running) {
        SendMessage(WM_COMMAND, ID_PLAY_PLAY);
    }

    if (GetPlaybackMode() == PM_FILE) {
        hr = pMS->SetRate(1.0);
    } else if (GetPlaybackMode() == PM_DVD) {
        hr = pDVDC->PlayForwards(1.0, DVD_CMD_FLAG_Block, NULL);
    }

    if (SUCCEEDED(hr)) {
        m_dSpeedRate = 1.0;

        CString strODSMessage;
        strODSMessage.Format(IDS_OSD_SPEED, m_dSpeedRate);
        m_OSD.DisplayMessage(OSD_TOPRIGHT, strODSMessage);
    }
}

void CMainFrame::OnUpdatePlayResetRate(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_iMediaLoadState == MLS_LOADED);
}

void CMainFrame::SetAudioDelay(REFERENCE_TIME rtShift)
{
    if (CComQIPtr<IAudioSwitcherFilter> pASF = FindFilter(__uuidof(CAudioSwitcherFilter), pGB)) {
        pASF->SetAudioTimeShift(rtShift);

        CString str;
        str.Format(IDS_MAINFRM_70, rtShift / 10000);
        SendStatusMessage(str, 3000);
        m_OSD.DisplayMessage(OSD_TOPLEFT, str);
    }
}

void CMainFrame::SetSubtitleDelay(int delay_ms)
{
    if (m_pCAP) {
        m_pCAP->SetSubtitleDelay(delay_ms);

        CString strSubDelay;
        strSubDelay.Format(IDS_MAINFRM_139, delay_ms);
        SendStatusMessage(strSubDelay, 3000);
        m_OSD.DisplayMessage(OSD_TOPLEFT, strSubDelay);
    }
}

void CMainFrame::OnPlayChangeAudDelay(UINT nID)
{
    if (CComQIPtr<IAudioSwitcherFilter> pASF = FindFilter(__uuidof(CAudioSwitcherFilter), pGB)) {
        REFERENCE_TIME rtShift = pASF->GetAudioTimeShift();
        rtShift +=
            nID == ID_PLAY_INCAUDDELAY ? 100000 :
            nID == ID_PLAY_DECAUDDELAY ? -100000 :
            0;

        SetAudioDelay(rtShift);
    }
}

void CMainFrame::OnUpdatePlayChangeAudDelay(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(!!pGB /*&& !!FindFilter(__uuidof(CAudioSwitcherFilter), pGB)*/);
}

void CMainFrame::OnPlayFilters(UINT nID)
{
    //ShowPPage(m_spparray[nID - ID_FILTERS_SUBITEM_START], m_hWnd);

    CComPtr<IUnknown> pUnk = m_pparray[nID - ID_FILTERS_SUBITEM_START];

    CComPropertySheet ps(ResStr(IDS_PROPSHEET_PROPERTIES), GetModalParent());

    if (CComQIPtr<ISpecifyPropertyPages> pSPP = pUnk) {
        ps.AddPages(pSPP);
    }

    if (CComQIPtr<IBaseFilter> pBF = pUnk) {
        HRESULT hr;
        CComPtr<IPropertyPage> pPP = DEBUG_NEW CInternalPropertyPageTempl<CPinInfoWnd>(NULL, &hr);
        ps.AddPage(pPP, pBF);
    }

    if (ps.GetPageCount() > 0) {
        ps.DoModal();
        OpenSetupStatusBar();
    }
}

void CMainFrame::OnUpdatePlayFilters(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(!m_fCapturing);
}

enum {
    ID_SHADERS_SELECT = ID_SHADERS_START,
    ID_SHADERS_SELECT_SCREENSPACE
};

void CMainFrame::OnPlayShaders(UINT nID)
{
    if (nID == ID_SHADERS_SELECT) {
        if (IDOK != CShaderCombineDlg(m_shaderlabels, m_shaderlabelsScreenSpace, GetModalParent()).DoModal()) {
            return;
        }
    }

    SetShaders();
}

void CMainFrame::OnPlayAudio(UINT nID)
{
    int i = (int)nID - (1 + ID_AUDIO_SUBITEM_START);

    CComQIPtr<IAMStreamSelect> pSS = FindFilter(__uuidof(CAudioSwitcherFilter), pGB);
    if (!pSS) {
        pSS = FindFilter(CLSID_MorganStreamSwitcher, pGB);
    }

    if (i == -1) {
        ShowOptions(CPPageAudioSwitcher::IDD);
    } else if (i >= 0 && pSS) {
        pSS->Enable(i, AMSTREAMSELECTENABLE_ENABLE);
    }
}

void CMainFrame::OnPlaySubtitles(UINT nID)
{
    // currently the subtitles submenu contains 5 items, apart from the actual subtitles list
    int i = (int)nID - (5 + ID_SUBTITLES_SUBITEM_START);

    if (i == -5) {
        // options
        ShowOptions(CPPageSubtitles::IDD);
    } else if (i == -4) {
        // styles
        SubtitleInput subElement;
        int j = GetSubtitleInput(m_iSubtitleSel, subElement);
        CLSID clsid;

        if (j >= 0 && SUCCEEDED(subElement.subStream->GetClassID(&clsid))) {
            if (clsid == __uuidof(CRenderedTextSubtitle)) {
                CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)subElement.subStream;

                CAutoPtrArray<CPPageSubStyle> pages;
                CAtlArray<STSStyle*> styles;

                POSITION pos = pRTS->m_styles.GetStartPosition();
                for (int k = 0; pos; k++) {
                    CString key;
                    STSStyle* val;
                    pRTS->m_styles.GetNextAssoc(pos, key, val);

                    CAutoPtr<CPPageSubStyle> page(DEBUG_NEW CPPageSubStyle());
                    page->InitStyle(key, *val);
                    pages.Add(page);
                    styles.Add(val);
                }

                CString m_style = ResStr(IDS_SUBTITLES_STYLES);
                int k = m_style.Find(_T("&"));
                if (k != -1) {
                    m_style.Delete(k, 1);
                }
                CPropertySheet dlg(m_style, GetModalParent());
                for (int l = 0; l < (int)pages.GetCount(); l++) {
                    dlg.AddPage(pages[l]);
                }

                if (dlg.DoModal() == IDOK) {
                    for (int l = 0; l < (int)pages.GetCount(); l++) {
                        pages[l]->GetStyle(*styles[l]);
                    }
                    UpdateSubtitle(false, false);
                }
            }
        }
    } else if (i == -3) {
        // reload
        ReloadSubtitle();
    } else if (i == -2) {
        // enable
        if (m_iSubtitleSel == -1) {
            m_iSubtitleSel = 0;
        } else {
            m_iSubtitleSel ^= (1 << 31);
        }
        UpdateSubtitle();
    } else if (i == -1) {
        // override default style
        // TODO: default subtitles style toggle here
        AfxGetAppSettings().fUseDefaultSubtitlesStyle = !AfxGetAppSettings().fUseDefaultSubtitlesStyle;
        UpdateSubtitle();
    } else if (i >= 0) {
        // this is an actual item from the subtitles list
        m_iSubtitleSel = i;
        UpdateSubtitle();
    }

    AfxGetAppSettings().fEnableSubtitles = !(m_iSubtitleSel & 0x80000000);
}

void CMainFrame::OnPlayFiltersStreams(UINT nID)
{
    nID -= ID_FILTERSTREAMS_SUBITEM_START;
    CComPtr<IAMStreamSelect> pAMSS = m_ssarray[nID];
    UINT i = nID;

    while (i > 0 && pAMSS == m_ssarray[i - 1]) {
        i--;
    }

    if (FAILED(pAMSS->Enable(nID - i, AMSTREAMSELECTENABLE_ENABLE))) {
        MessageBeep((UINT) - 1);
    }

    OpenSetupStatusBar();
}

void CMainFrame::OnUpdatePlayFiltersStreams(CCmdUI* pCmdUI)
{
    UINT nID = pCmdUI->m_nID - ID_FILTERSTREAMS_SUBITEM_START;
    CComPtr<IAMStreamSelect> pAMSS = m_ssarray[nID];
    UINT i = nID;

    while (i > 0 && pAMSS == m_ssarray[i - 1]) {
        i--;
    }

    DWORD flags = 0;
    pAMSS->Info(nID - i, NULL, &flags, NULL, NULL, NULL, NULL, NULL);

    if (flags & AMSTREAMSELECTINFO_EXCLUSIVE) {
        pCmdUI->SetRadio(TRUE);
    } else if (flags & AMSTREAMSELECTINFO_ENABLED) {
        pCmdUI->SetCheck(TRUE);
    } else {
        pCmdUI->SetCheck(FALSE);
    }
}

void CMainFrame::OnPlayVolume(UINT nID)
{
    if (m_iMediaLoadState == MLS_LOADED) {
        CString strVolume;
        pBA->put_Volume(m_wndToolBar.Volume);

        //strVolume.Format (L"Vol : %d dB", m_wndToolBar.Volume / 100);
        if (m_wndToolBar.Volume == -10000) {
            strVolume.Format(IDS_VOLUME_OSD, 0);
        } else {
            strVolume.Format(IDS_VOLUME_OSD, m_wndToolBar.m_volctrl.GetPos());
        }
        m_OSD.DisplayMessage(OSD_TOPLEFT, strVolume);
        //SendStatusMessage(strVolume, 3000); // Now the volume is displayed in three places at once.
    }

    m_Lcd.SetVolume((m_wndToolBar.Volume > -10000 ? m_wndToolBar.m_volctrl.GetPos() : 1));
}

void CMainFrame::OnPlayVolumeBoost(UINT nID)
{
    CAppSettings& s = AfxGetAppSettings();
    int i = (int)(s.dAudioBoost_dB * 10 + 0.1);

    switch (nID) {
        case ID_VOLUME_BOOST_INC:
            i = min(i + 10, 100);
            break;
        case ID_VOLUME_BOOST_DEC:
            i = max(i - 10, 0);
            break;
        case ID_VOLUME_BOOST_MIN:
            i = 0;
            break;
        case ID_VOLUME_BOOST_MAX:
            i = 100;
            break;
    }

    s.dAudioBoost_dB = i / 10.f;
    SetVolumeBoost(s.dAudioBoost_dB);
}

void CMainFrame::SetVolumeBoost(float fAudioBoost_dB)
{
    CString strBoost;
    strBoost.Format(IDS_BOOST_OSD, fAudioBoost_dB);

    if (CComQIPtr<IAudioSwitcherFilter> pASF = FindFilter(__uuidof(CAudioSwitcherFilter), pGB)) {
        bool fNormalize, fNormalizeRecover;
        float boost;
        pASF->GetNormalizeBoost(fNormalize, fNormalizeRecover, boost);
        pASF->SetNormalizeBoost(fNormalize, fNormalizeRecover, fAudioBoost_dB);
        m_OSD.DisplayMessage(OSD_TOPLEFT, strBoost);
    }
}

void CMainFrame::OnUpdatePlayVolumeBoost(CCmdUI* pCmdUI)
{
    pCmdUI->Enable();
}

void CMainFrame::OnCustomChannelMapping()
{
    if (CComQIPtr<IAudioSwitcherFilter> pASF = FindFilter(__uuidof(CAudioSwitcherFilter), pGB)) {
        CAppSettings& s = AfxGetAppSettings();
        s.fCustomChannelMapping = !s.fCustomChannelMapping;
        pASF->SetSpeakerConfig(s.fCustomChannelMapping, s.pSpeakerToChannelMap);
        m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(s.fCustomChannelMapping ? IDS_OSD_CUSTOM_CH_MAPPING_ON : IDS_OSD_CUSTOM_CH_MAPPING_OFF));
    }
}

void CMainFrame::OnUpdateCustomChannelMapping(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    pCmdUI->Enable(s.fEnableAudioSwitcher);
}

void CMainFrame::OnNormalizeRegainVolume(UINT nID)
{
    if (CComQIPtr<IAudioSwitcherFilter> pASF = FindFilter(__uuidof(CAudioSwitcherFilter), pGB)) {
        CAppSettings& s = AfxGetAppSettings();
        WORD osdMessage;

        switch (nID) {
            case ID_NORMALIZE:
                s.fAudioNormalize = !s.fAudioNormalize;
                osdMessage = s.fAudioNormalize ? IDS_OSD_NORMALIZE_ON : IDS_OSD_NORMALIZE_OFF;
                break;
            case ID_REGAIN_VOLUME:
                s.fAudioNormalizeRecover = !s.fAudioNormalizeRecover;
                osdMessage = s.fAudioNormalizeRecover ? IDS_OSD_REGAIN_VOLUME_ON : IDS_OSD_REGAIN_VOLUME_OFF;
                break;
        }

        pASF->SetNormalizeBoost(s.fAudioNormalize, s.fAudioNormalizeRecover, s.dAudioBoost_dB);
        m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(osdMessage));
    }
}

void CMainFrame::OnUpdateNormalizeRegainVolume(CCmdUI* pCmdUI)
{
    CAppSettings& s = AfxGetAppSettings();
    pCmdUI->Enable(s.fEnableAudioSwitcher);
}

void CMainFrame::OnPlayColor(UINT nID)
{
    if (m_pMC || m_pMFVP) {
        CAppSettings& s = AfxGetAppSettings();
        //ColorRanges* crs = AfxGetMyApp()->ColorControls;
        int& brightness = s.iBrightness;
        int& contrast   = s.iContrast;
        int& hue        = s.iHue;
        int& saturation = s.iSaturation;
        CString tmp, str;
        switch (nID) {

            case ID_COLOR_BRIGHTNESS_INC:
                brightness += 2;
            case ID_COLOR_BRIGHTNESS_DEC:
                brightness -= 1;
                SetColorControl(ProcAmp_Brightness, brightness, contrast, hue, saturation);
                brightness ? tmp.Format(_T("%+d"), brightness) : tmp = _T("0");
                str.Format(IDS_OSD_BRIGHTNESS, tmp);
                break;

            case ID_COLOR_CONTRAST_INC:
                contrast += 2;
            case ID_COLOR_CONTRAST_DEC:
                contrast -= 1;
                SetColorControl(ProcAmp_Contrast, brightness, contrast, hue, saturation);
                contrast ? tmp.Format(_T("%+d"), contrast) : tmp = _T("0");
                str.Format(IDS_OSD_CONTRAST, tmp);
                break;

            case ID_COLOR_HUE_INC:
                hue += 2;
            case ID_COLOR_HUE_DEC:
                hue -= 1;
                SetColorControl(ProcAmp_Hue, brightness, contrast, hue, saturation);
                hue ? tmp.Format(_T("%+d"), hue) : tmp = _T("0");
                str.Format(IDS_OSD_HUE, tmp);
                break;

            case ID_COLOR_SATURATION_INC:
                saturation += 2;
            case ID_COLOR_SATURATION_DEC:
                saturation -= 1;
                SetColorControl(ProcAmp_Saturation, brightness, contrast, hue, saturation);
                saturation ? tmp.Format(_T("%+d"), saturation) : tmp = _T("0");
                str.Format(IDS_OSD_SATURATION, tmp);
                break;

            case ID_COLOR_RESET:
                brightness = AfxGetMyApp()->GetColorControl(ProcAmp_Brightness)->DefaultValue;
                contrast   = AfxGetMyApp()->GetColorControl(ProcAmp_Contrast)->DefaultValue;
                hue        = AfxGetMyApp()->GetColorControl(ProcAmp_Hue)->DefaultValue;
                saturation = AfxGetMyApp()->GetColorControl(ProcAmp_Saturation)->DefaultValue;
                SetColorControl(ProcAmp_All, brightness, contrast, hue, saturation);
                str.LoadString(IDS_OSD_RESET_COLOR);
                break;
        }
        m_OSD.DisplayMessage(OSD_TOPLEFT, str);
    } else {
        m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_OSD_NO_COLORCONTROL));
    }
}

void CMainFrame::OnAfterplayback(UINT nID)
{
    CAppSettings& s = AfxGetAppSettings();
    s.nCLSwitches &= ~CLSW_AFTERPLAYBACK_MASK;
    WORD osdMsg;

    switch (nID) {
        case ID_AFTERPLAYBACK_NEXT:
            s.fNextInDirAfterPlayback = true;
            s.fExitAfterPlayback = false;
            osdMsg = IDS_AFTERPLAYBACK_NEXT;
            break;
        case ID_AFTERPLAYBACK_EXIT:
            s.fExitAfterPlayback = true;
            s.fNextInDirAfterPlayback = false;
            osdMsg = IDS_AFTERPLAYBACK_EXIT;
            break;
        case ID_AFTERPLAYBACK_DONOTHING:
            s.fExitAfterPlayback = false;
            s.fNextInDirAfterPlayback = false;
            osdMsg = IDS_AFTERPLAYBACK_DONOTHING;
            break;
        case ID_AFTERPLAYBACK_CLOSE:
            s.nCLSwitches |= CLSW_CLOSE;
            osdMsg = IDS_AFTERPLAYBACK_CLOSE;
            break;
        case ID_AFTERPLAYBACK_STANDBY:
            s.nCLSwitches |= CLSW_STANDBY;
            osdMsg = IDS_AFTERPLAYBACK_STANDBY;
            break;
        case ID_AFTERPLAYBACK_HIBERNATE:
            s.nCLSwitches |= CLSW_HIBERNATE;
            osdMsg = IDS_AFTERPLAYBACK_HIBERNATE;
            break;
        case ID_AFTERPLAYBACK_SHUTDOWN:
            s.nCLSwitches |= CLSW_SHUTDOWN;
            osdMsg = IDS_AFTERPLAYBACK_SHUTDOWN;
            break;
        case ID_AFTERPLAYBACK_LOGOFF:
            s.nCLSwitches |= CLSW_LOGOFF;
            osdMsg = IDS_AFTERPLAYBACK_LOGOFF;
            break;
        case ID_AFTERPLAYBACK_LOCK:
            s.nCLSwitches |= CLSW_LOCK;
            osdMsg = IDS_AFTERPLAYBACK_LOCK;
            break;
    }

    m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(osdMsg));
}

void CMainFrame::OnUpdateAfterplayback(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    bool fChecked = false;

    switch (pCmdUI->m_nID) {
        case ID_AFTERPLAYBACK_EXIT:
            fChecked = !!s.fExitAfterPlayback;
            break;
        case ID_AFTERPLAYBACK_NEXT:
            fChecked = !!s.fNextInDirAfterPlayback;
            break;
        case ID_AFTERPLAYBACK_CLOSE:
            fChecked = !!(s.nCLSwitches & CLSW_CLOSE);
            break;
        case ID_AFTERPLAYBACK_STANDBY:
            fChecked = !!(s.nCLSwitches & CLSW_STANDBY);
            break;
        case ID_AFTERPLAYBACK_HIBERNATE:
            fChecked = !!(s.nCLSwitches & CLSW_HIBERNATE);
            break;
        case ID_AFTERPLAYBACK_SHUTDOWN:
            fChecked = !!(s.nCLSwitches & CLSW_SHUTDOWN);
            break;
        case ID_AFTERPLAYBACK_LOGOFF:
            fChecked = !!(s.nCLSwitches & CLSW_LOGOFF);
            break;
        case ID_AFTERPLAYBACK_LOCK:
            fChecked = !!(s.nCLSwitches & CLSW_LOCK);
            break;
        case ID_AFTERPLAYBACK_DONOTHING:
            fChecked = (!s.fExitAfterPlayback) && (!s.fNextInDirAfterPlayback);
            break;
    }

    if (fChecked) {
        if (pCmdUI->m_pMenu) {
            // To make things simpler we (ab)use the CheckMenuRadioItem function to set the radio bullet
            // for the selected item and we use an extra call to SetCheck to ensure the mark is cleared
            // from the other menu's items.
            pCmdUI->m_pMenu->CheckMenuRadioItem(pCmdUI->m_nID, pCmdUI->m_nID, pCmdUI->m_nID, MF_BYCOMMAND);
        }
    } else {
        pCmdUI->SetCheck(FALSE);
    }
}

// navigate
void CMainFrame::OnNavigateSkip(UINT nID)
{
    if (GetPlaybackMode() == PM_FILE) {
        SetupChapters();
        flast_nID = nID;

        if (DWORD nChapters = m_pCB->ChapGetCount()) {
            REFERENCE_TIME rtCur;
            pMS->GetCurrentPosition(&rtCur);

            REFERENCE_TIME rt = rtCur;
            CComBSTR name;
            long i = 0;

            if (nID == ID_NAVIGATE_SKIPBACK) {
                rt -= 30000000;
                i = m_pCB->ChapLookup(&rt, &name);
            } else if (nID == ID_NAVIGATE_SKIPFORWARD) {
                i = m_pCB->ChapLookup(&rt, &name) + 1;
                name.Empty();
                if (i < (int)nChapters) {
                    m_pCB->ChapGet(i, &rt, &name);
                }
            }

            if (i >= 0 && (DWORD)i < nChapters) {
                SeekTo(rt);
                SendStatusMessage(ResStr(IDS_AG_CHAPTER2) + CString(name), 3000);

                REFERENCE_TIME rtDur;
                pMS->GetDuration(&rtDur);
                CString m_strOSD;
                m_strOSD.Format(_T("%s/%s %s%d/%d - \"%s\""), ReftimeToString2(rt), ReftimeToString2(rtDur), ResStr(IDS_AG_CHAPTER2), i + 1, nChapters, name);
                m_OSD.DisplayMessage(OSD_TOPLEFT, m_strOSD, 3000);
                return;
            }
        }

        if (nID == ID_NAVIGATE_SKIPBACK) {
            SendMessage(WM_COMMAND, ID_NAVIGATE_SKIPBACKFILE);
        } else if (nID == ID_NAVIGATE_SKIPFORWARD) {
            SendMessage(WM_COMMAND, ID_NAVIGATE_SKIPFORWARDFILE);
        }
    } else if (GetPlaybackMode() == PM_DVD) {
        m_dSpeedRate = 1.0;

        if (GetMediaState() != State_Running) {
            SendMessage(WM_COMMAND, ID_PLAY_PLAY);
        }

        ULONG ulNumOfVolumes, ulVolume;
        DVD_DISC_SIDE Side;
        ULONG ulNumOfTitles = 0;
        pDVDI->GetDVDVolumeInfo(&ulNumOfVolumes, &ulVolume, &Side, &ulNumOfTitles);

        DVD_PLAYBACK_LOCATION2 Location;
        pDVDI->GetCurrentLocation(&Location);

        ULONG ulNumOfChapters = 0;
        pDVDI->GetNumberOfChapters(Location.TitleNum, &ulNumOfChapters);

        if (nID == ID_NAVIGATE_SKIPBACK) {
            if (Location.ChapterNum == 1 && Location.TitleNum > 1) {
                pDVDI->GetNumberOfChapters(Location.TitleNum - 1, &ulNumOfChapters);
                pDVDC->PlayChapterInTitle(Location.TitleNum - 1, ulNumOfChapters, DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL);
            } else {
                ULONG tsec = (Location.TimeCode.bHours * 3600)
                             + (Location.TimeCode.bMinutes * 60)
                             + (Location.TimeCode.bSeconds);
                ULONG diff = 0;
                if (m_lChapterStartTime != 0xFFFFFFFF && tsec > m_lChapterStartTime) {
                    diff = tsec - m_lChapterStartTime;
                }
                // Restart the chapter if more than 7 seconds have passed
                if (diff <= 7) {
                    pDVDC->PlayPrevChapter(DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL);
                } else {
                    pDVDC->ReplayChapter(DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL);
                }
            }
        } else if (nID == ID_NAVIGATE_SKIPFORWARD) {
            if (Location.ChapterNum == ulNumOfChapters && Location.TitleNum < ulNumOfTitles) {
                pDVDC->PlayChapterInTitle(Location.TitleNum + 1, 1, DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL);
            } else {
                pDVDC->PlayNextChapter(DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL);
            }
        }

        if ((pDVDI->GetCurrentLocation(&Location) == S_OK)) {
            pDVDI->GetNumberOfChapters(Location.TitleNum, &ulNumOfChapters);
            CString m_strTitle;
            m_strTitle.Format(IDS_AG_TITLE2, Location.TitleNum, ulNumOfTitles);
            __int64 start, stop;
            m_wndSeekBar.GetRange(start, stop);

            CString m_strOSD;
            if (stop > 0) {
                DVD_HMSF_TIMECODE stopHMSF = RT2HMS_r(stop);
                m_strOSD.Format(_T("%s/%s %s, %s%02d/%02d"), DVDtimeToString(Location.TimeCode, stopHMSF.bHours > 0), DVDtimeToString(stopHMSF),
                                m_strTitle, ResStr(IDS_AG_CHAPTER2), Location.ChapterNum, ulNumOfChapters);
            } else {
                m_strOSD.Format(_T("%s, %s%02d/%02d"), m_strTitle, ResStr(IDS_AG_CHAPTER2), Location.ChapterNum, ulNumOfChapters);
            }

            m_OSD.DisplayMessage(OSD_TOPLEFT, m_strOSD, 3000);
        }

        /*
        if (nID == ID_NAVIGATE_SKIPBACK)
            pDVDC->PlayPrevChapter(DVD_CMD_FLAG_Block, NULL);
        else if (nID == ID_NAVIGATE_SKIPFORWARD)
            pDVDC->PlayNextChapter(DVD_CMD_FLAG_Block, NULL);
        */
    } else if (GetPlaybackMode() == PM_CAPTURE) {
        if (AfxGetAppSettings().iDefaultCaptureDevice == 1) {
            CComQIPtr<IBDATuner> pTun = pGB;
            if (pTun) {
                int nCurrentChannel;
                const CAppSettings& s = AfxGetAppSettings();
                nCurrentChannel = s.nDVBLastChannel;

                if (nID == ID_NAVIGATE_SKIPBACK) {
                    if (SUCCEEDED(pTun->SetChannel(nCurrentChannel - 1))) {
                        ShowCurrentChannelInfo();
                        if (m_wndNavigationBar.IsVisible()) {
                            m_wndNavigationBar.m_navdlg.UpdatePos(nCurrentChannel - 1);
                        }
                    }
                } else if (nID == ID_NAVIGATE_SKIPFORWARD) {
                    if (SUCCEEDED(pTun->SetChannel(nCurrentChannel + 1))) {
                        ShowCurrentChannelInfo();
                        if (m_wndNavigationBar.IsVisible()) {
                            m_wndNavigationBar.m_navdlg.UpdatePos(nCurrentChannel + 1);
                        }
                    }
                }

            }
        }

    }
}

void CMainFrame::OnUpdateNavigateSkip(CCmdUI* pCmdUI)
{
    // moved to the timer callback function, that runs less frequent
    //if (GetPlaybackMode() == PM_FILE) SetupChapters();

    pCmdUI->Enable(m_iMediaLoadState == MLS_LOADED
                   && ((GetPlaybackMode() == PM_DVD
                        && m_iDVDDomain != DVD_DOMAIN_VideoManagerMenu
                        && m_iDVDDomain != DVD_DOMAIN_VideoTitleSetMenu)
                       || (GetPlaybackMode() == PM_FILE  && AfxGetAppSettings().fUseSearchInFolder)
                       || (GetPlaybackMode() == PM_FILE  && !AfxGetAppSettings().fUseSearchInFolder && (m_wndPlaylistBar.GetCount() > 1 || m_pCB->ChapGetCount() > 1))
                       || (GetPlaybackMode() == PM_CAPTURE && !m_fCapturing)));
}

void CMainFrame::OnNavigateSkipFile(UINT nID)
{
    if (GetPlaybackMode() == PM_FILE || GetPlaybackMode() == PM_CAPTURE) {
        if (m_wndPlaylistBar.GetCount() == 1) {
            if (GetPlaybackMode() == PM_CAPTURE || !AfxGetAppSettings().fUseSearchInFolder) {
                SendMessage(WM_COMMAND, ID_PLAY_STOP); // do not remove this, unless you want a circular call with OnPlayPlay()
                SendMessage(WM_COMMAND, ID_PLAY_PLAY);
            } else {
                if (nID == ID_NAVIGATE_SKIPBACKFILE) {
                    if (!SearchInDir(false)) {
                        m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_FIRST_IN_FOLDER));
                    }
                } else if (nID == ID_NAVIGATE_SKIPFORWARDFILE) {
                    if (!SearchInDir(true)) {
                        m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_LAST_IN_FOLDER));
                    }
                }
            }
        } else {
            if (nID == ID_NAVIGATE_SKIPBACKFILE) {
                m_wndPlaylistBar.SetPrev();
            } else if (nID == ID_NAVIGATE_SKIPFORWARDFILE) {
                m_wndPlaylistBar.SetNext();
            }

            OpenCurPlaylistItem();
        }
    }
}

void CMainFrame::OnUpdateNavigateSkipFile(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_iMediaLoadState == MLS_LOADED
                   && ((GetPlaybackMode() == PM_FILE && (m_wndPlaylistBar.GetCount() > 1 || AfxGetAppSettings().fUseSearchInFolder))
                       || (GetPlaybackMode() == PM_CAPTURE && !m_fCapturing && m_wndPlaylistBar.GetCount() > 1)));
}

void CMainFrame::OnNavigateMenu(UINT nID)
{
    nID -= ID_NAVIGATE_TITLEMENU;

    if (m_iMediaLoadState != MLS_LOADED || GetPlaybackMode() != PM_DVD) {
        return;
    }

    m_dSpeedRate = 1.0;

    if (GetMediaState() != State_Running) {
        SendMessage(WM_COMMAND, ID_PLAY_PLAY);
    }

    pDVDC->ShowMenu((DVD_MENU_ID)(nID + 2), DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL);
}

void CMainFrame::OnUpdateNavigateMenu(CCmdUI* pCmdUI)
{
    UINT nID = pCmdUI->m_nID - ID_NAVIGATE_TITLEMENU;
    ULONG ulUOPs;

    if (m_iMediaLoadState != MLS_LOADED || GetPlaybackMode() != PM_DVD
            || FAILED(pDVDI->GetCurrentUOPS(&ulUOPs))) {
        pCmdUI->Enable(FALSE);
        return;
    }

    pCmdUI->Enable(!(ulUOPs & (UOP_FLAG_ShowMenu_Title << nID)));
}

void CMainFrame::OnNavigateAudio(UINT nID)
{
    nID -= ID_NAVIGATE_AUDIO_SUBITEM_START;

    if (GetPlaybackMode() == PM_FILE || (GetPlaybackMode() == PM_CAPTURE && AfxGetAppSettings().iDefaultCaptureDevice == 1)) {
        OnNavStreamSelectSubMenu(nID, 1);
    } else if (GetPlaybackMode() == PM_DVD) {
        pDVDC->SelectAudioStream(nID, DVD_CMD_FLAG_Block, NULL);
    }
}

void CMainFrame::OnNavigateSubpic(UINT nID)
{
    if (GetPlaybackMode() == PM_FILE || (GetPlaybackMode() == PM_CAPTURE && AfxGetAppSettings().iDefaultCaptureDevice == 1)) {
        OnNavStreamSelectSubMenu(nID - ID_NAVIGATE_SUBP_SUBITEM_START, 2);
    } else if (GetPlaybackMode() == PM_DVD) {
        int i = (int)nID - (1 + ID_NAVIGATE_SUBP_SUBITEM_START);

        if (i == -1) {
            ULONG ulStreamsAvailable, ulCurrentStream;
            BOOL bIsDisabled;
            if (SUCCEEDED(pDVDI->GetCurrentSubpicture(&ulStreamsAvailable, &ulCurrentStream, &bIsDisabled))) {
                pDVDC->SetSubpictureState(bIsDisabled, DVD_CMD_FLAG_Block, NULL);
            }
        } else {
            pDVDC->SelectSubpictureStream(i, DVD_CMD_FLAG_Block, NULL);
            pDVDC->SetSubpictureState(TRUE, DVD_CMD_FLAG_Block, NULL);
        }
    }
}

void CMainFrame::OnNavigateAngle(UINT nID)
{
    nID -= ID_NAVIGATE_ANGLE_SUBITEM_START;

    if (GetPlaybackMode() == PM_FILE) {
        OnNavStreamSelectSubMenu(nID, 0);
    } else if (GetPlaybackMode() == PM_DVD) {
        pDVDC->SelectAngle(nID + 1, DVD_CMD_FLAG_Block, NULL);

        CString osdMessage;
        osdMessage.Format(IDS_AG_ANGLE, nID + 1);
        m_OSD.DisplayMessage(OSD_TOPLEFT, osdMessage);
    }
}

void CMainFrame::OnNavigateChapters(UINT nID)
{
    if (nID < ID_NAVIGATE_CHAP_SUBITEM_START) {
        return;
    }

    if (GetPlaybackMode() == PM_FILE) {
        int id = nID - ID_NAVIGATE_CHAP_SUBITEM_START;

        if (id < (int)m_MPLSPlaylist.GetCount() && m_MPLSPlaylist.GetCount() > 1) {
            POSITION pos = m_MPLSPlaylist.GetHeadPosition();
            int idx = 0;
            while (pos) {
                CHdmvClipInfo::PlaylistItem Item = m_MPLSPlaylist.GetNext(pos);
                if (idx == id) {
                    m_bIsBDPlay = true;
                    m_wndPlaylistBar.Empty();
                    CAtlList<CString> sl;
                    sl.AddTail(CString(Item.m_strFileName));
                    m_wndPlaylistBar.Append(sl, false);
                    OpenCurPlaylistItem();
                    return;
                }
                idx++;
            }
        }

        if (m_MPLSPlaylist.GetCount() > 1) {
            id -= (int)m_MPLSPlaylist.GetCount();
        }

        if (id >= 0 && id < (int)m_pCB->ChapGetCount() && m_pCB->ChapGetCount() > 1) {
            REFERENCE_TIME rt;
            CComBSTR name;
            if (SUCCEEDED(m_pCB->ChapGet(id, &rt, &name))) {
                SeekTo(rt);
                SendStatusMessage(ResStr(IDS_AG_CHAPTER2) + CString(name), 3000);

                REFERENCE_TIME rtDur;
                pMS->GetDuration(&rtDur);
                CString m_strOSD;
                m_strOSD.Format(_T("%s/%s %s%d/%d - \"%s\""), ReftimeToString2(rt), ReftimeToString2(rtDur), ResStr(IDS_AG_CHAPTER2), id + 1, m_pCB->ChapGetCount(), name);
                m_OSD.DisplayMessage(OSD_TOPLEFT, m_strOSD, 3000);
            }
            return;
        }

        if (m_pCB->ChapGetCount() > 1) {
            id -= m_pCB->ChapGetCount();
        }

        if (id >= 0 && id < m_wndPlaylistBar.GetCount() && m_wndPlaylistBar.GetSelIdx() != id) {
            m_wndPlaylistBar.SetSelIdx(id);
            OpenCurPlaylistItem();
        }
    } else if (GetPlaybackMode() == PM_DVD) {
        ULONG ulNumOfVolumes, ulVolume;
        DVD_DISC_SIDE Side;
        ULONG ulNumOfTitles = 0;
        pDVDI->GetDVDVolumeInfo(&ulNumOfVolumes, &ulVolume, &Side, &ulNumOfTitles);

        DVD_PLAYBACK_LOCATION2 Location;
        pDVDI->GetCurrentLocation(&Location);

        ULONG ulNumOfChapters = 0;
        pDVDI->GetNumberOfChapters(Location.TitleNum, &ulNumOfChapters);

        nID -= (ID_NAVIGATE_CHAP_SUBITEM_START - 1);

        if (nID <= ulNumOfTitles) {
            pDVDC->PlayTitle(nID, DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL); // sometimes this does not do anything ...
            pDVDC->PlayChapterInTitle(nID, 1, DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL); // ... but this does!
        } else {
            nID -= ulNumOfTitles;

            if (nID <= ulNumOfChapters) {
                pDVDC->PlayChapter(nID, DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL);
            }
        }

        if ((pDVDI->GetCurrentLocation(&Location) == S_OK)) {
            pDVDI->GetNumberOfChapters(Location.TitleNum, &ulNumOfChapters);
            CString m_strTitle;
            m_strTitle.Format(IDS_AG_TITLE2, Location.TitleNum, ulNumOfTitles);
            __int64 start, stop;
            m_wndSeekBar.GetRange(start, stop);

            CString m_strOSD;
            if (stop > 0) {
                DVD_HMSF_TIMECODE stopHMSF = RT2HMS_r(stop);
                m_strOSD.Format(_T("%s/%s %s, %s%02d/%02d"), DVDtimeToString(Location.TimeCode, stopHMSF.bHours > 0), DVDtimeToString(stopHMSF),
                                m_strTitle, ResStr(IDS_AG_CHAPTER2), Location.ChapterNum, ulNumOfChapters);
            } else {
                m_strOSD.Format(_T("%s, %s%02d/%02d"), m_strTitle, ResStr(IDS_AG_CHAPTER2), Location.ChapterNum, ulNumOfChapters);
            }

            m_OSD.DisplayMessage(OSD_TOPLEFT, m_strOSD, 3000);
        }
    } else if (GetPlaybackMode() == PM_CAPTURE) {
        const CAppSettings& s = AfxGetAppSettings();

        nID -= ID_NAVIGATE_CHAP_SUBITEM_START;

        if (s.iDefaultCaptureDevice == 1) {
            CComQIPtr<IBDATuner>    pTun = pGB;
            if (pTun) {
                if (s.nDVBLastChannel != nID) {
                    if (SUCCEEDED(pTun->SetChannel(nID))) {
                        ShowCurrentChannelInfo();
                        if (m_wndNavigationBar.IsVisible()) {
                            m_wndNavigationBar.m_navdlg.UpdatePos(nID);
                        }
                    }
                }
            }
        }
    }
}

void CMainFrame::OnNavigateMenuItem(UINT nID)
{
    nID -= ID_NAVIGATE_MENU_LEFT;

    if (GetPlaybackMode() == PM_DVD) {
        switch (nID) {
            case 0:
                pDVDC->SelectRelativeButton(DVD_Relative_Left);
                break;
            case 1:
                pDVDC->SelectRelativeButton(DVD_Relative_Right);
                break;
            case 2:
                pDVDC->SelectRelativeButton(DVD_Relative_Upper);
                break;
            case 3:
                pDVDC->SelectRelativeButton(DVD_Relative_Lower);
                break;
            case 4:
                if (m_iDVDDomain != DVD_DOMAIN_Title) { // Casimir666 : for the remote control
                    pDVDC->ActivateButton();
                } else {
                    OnPlayPlay();
                }
                break;
            case 5:
                pDVDC->ReturnFromSubmenu(DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL);
                break;
            case 6:
                pDVDC->Resume(DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL);
                break;
            default:
                break;
        }
    } else if (GetPlaybackMode() == PM_FILE) {
        OnPlayPlay();
    }
}

void CMainFrame::OnUpdateNavigateMenuItem(CCmdUI* pCmdUI)
{
    pCmdUI->Enable((m_iMediaLoadState == MLS_LOADED) && ((GetPlaybackMode() == PM_DVD) || (GetPlaybackMode() == PM_FILE)));
}

void CMainFrame::OnTunerScan()
{
    CTunerScanDlg Dlg;
    Dlg.DoModal();
}

void CMainFrame::OnUpdateTunerScan(CCmdUI* pCmdUI)
{
    pCmdUI->Enable((m_iMediaLoadState == MLS_LOADED) &&
                   (AfxGetAppSettings().iDefaultCaptureDevice == 1) &&
                   ((GetPlaybackMode() == PM_CAPTURE)));
}

// favorites

class CDVDStateStream : public CUnknown, public IStream
{
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv) {
        return
            QI(IStream)
            CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }

    __int64 m_pos;

public:
    CDVDStateStream() : CUnknown(NAME("CDVDStateStream"), NULL) {
        m_pos = 0;
    }

    DECLARE_IUNKNOWN;

    CAtlArray<BYTE> m_data;

    // ISequentialStream
    STDMETHODIMP Read(void* pv, ULONG cb, ULONG* pcbRead) {
        __int64 cbRead = min((__int64)(m_data.GetCount() - m_pos), (__int64)cb);
        cbRead = max(cbRead, 0);
        memcpy(pv, &m_data[(INT_PTR)m_pos], (int)cbRead);
        if (pcbRead) {
            *pcbRead = (ULONG)cbRead;
        }
        m_pos += cbRead;
        return S_OK;
    }
    STDMETHODIMP Write(const void* pv, ULONG cb, ULONG* pcbWritten) {
        BYTE* p = (BYTE*)pv;
        ULONG cbWritten = (ULONG) - 1;
        while (++cbWritten < cb) {
            m_data.Add(*p++);
        }
        if (pcbWritten) {
            *pcbWritten = cbWritten;
        }
        return S_OK;
    }

    // IStream
    STDMETHODIMP Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition) {
        return E_NOTIMPL;
    }

    STDMETHODIMP SetSize(ULARGE_INTEGER libNewSize) {
        return E_NOTIMPL;
    }

    STDMETHODIMP CopyTo(IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten) {
        return E_NOTIMPL;
    }

    STDMETHODIMP Commit(DWORD grfCommitFlags) {
        return E_NOTIMPL;
    }

    STDMETHODIMP Revert() {
        return E_NOTIMPL;
    }

    STDMETHODIMP LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) {
        return E_NOTIMPL;
    }

    STDMETHODIMP UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) {
        return E_NOTIMPL;
    }

    STDMETHODIMP Stat(STATSTG* pstatstg, DWORD grfStatFlag) {
        return E_NOTIMPL;
    }

    STDMETHODIMP Clone(IStream** ppstm) {
        return E_NOTIMPL;
    }
};

void CMainFrame::OnFavoritesAdd()
{
    CAppSettings& s = AfxGetAppSettings();
    bool is_BD = false;

    if (GetPlaybackMode() == PM_FILE) {
        CString fn =  m_wndPlaylistBar.GetCurFileName();
        if (fn.IsEmpty()) {
            BeginEnumFilters(pGB, pEF, pBF) {
                CComQIPtr<IFileSourceFilter> pFSF = pBF;
                if (pFSF) {
                    LPOLESTR pFN = NULL;
                    AM_MEDIA_TYPE mt;
                    if (SUCCEEDED(pFSF->GetCurFile(&pFN, &mt)) && pFN && *pFN) {
                        fn = CStringW(pFN);
                        CoTaskMemFree(pFN);
                    }
                    break;
                }
            }
            EndEnumFilters;
            if (fn.IsEmpty()) {
                return;
            }
            is_BD = true;
        }

        CString desc = fn;
        desc.Replace('\\', '/');
        int i = desc.Find(_T("://")), j = desc.Find(_T("?")), k = desc.ReverseFind('/');
        if (i >= 0) {
            desc = j >= 0 ? desc.Left(j) : desc;
        } else if (k >= 0) {
            desc = desc.Mid(k + 1);
        }

        CFavoriteAddDlg dlg(desc, fn);
        if (dlg.DoModal() != IDOK) {
            return;
        }

        // Name
        CString str = dlg.m_name;
        str.Remove(';');

        // RememberPos
        CString pos(_T("0"));
        if (dlg.m_bRememberPos) {
            pos.Format(_T("%I64d"), GetPos());
        }

        str += ';';
        str += pos;

        // RelativeDrive
        CString relativeDrive;
        relativeDrive.Format(_T("%d"), dlg.m_bRelativeDrive);

        str += ';';
        str += relativeDrive;

        // Paths
        if (is_BD) {
            str += _T(";") + fn;
        } else {
            CPlaylistItem pli;
            if (m_wndPlaylistBar.GetCur(pli)) {
                POSITION pos = pli.m_fns.GetHeadPosition();
                while (pos) {
                    str += _T(";") + pli.m_fns.GetNext(pos);
                }
            }
        }

        s.AddFav(FAV_FILE, str);
    } else if (GetPlaybackMode() == PM_DVD) {
        WCHAR path[MAX_PATH];
        ULONG len = 0;
        pDVDI->GetDVDDirectory(path, MAX_PATH, &len);
        CString fn = path;
        fn.TrimRight(_T("/\\"));

        DVD_PLAYBACK_LOCATION2 Location;
        pDVDI->GetCurrentLocation(&Location);
        CString desc;
        desc.Format(_T("%s - T%02d C%02d - %02d:%02d:%02d"), fn, Location.TitleNum, Location.ChapterNum,
                    Location.TimeCode.bHours, Location.TimeCode.bMinutes, Location.TimeCode.bSeconds);

        CFavoriteAddDlg dlg(fn, desc);
        if (dlg.DoModal() != IDOK) {
            return;
        }

        // Name
        CString str = dlg.m_name;
        str.Remove(';');

        // RememberPos
        CString pos(_T("0"));
        if (dlg.m_bRememberPos) {
            CDVDStateStream stream;
            stream.AddRef();

            CComPtr<IDvdState> pStateData;
            CComQIPtr<IPersistStream> pPersistStream;
            if (SUCCEEDED(pDVDI->GetState(&pStateData))
                    && (pPersistStream = pStateData)
                    && SUCCEEDED(OleSaveToStream(pPersistStream, (IStream*)&stream))) {
                pos = BinToCString(stream.m_data.GetData(), stream.m_data.GetCount());
            }
        }

        str += ';';
        str += pos;

        // Paths
        str += ';';
        str += fn;

        s.AddFav(FAV_DVD, str);
    } else if (GetPlaybackMode() == PM_CAPTURE) {
        // TODO
    }
}

void CMainFrame::OnUpdateFavoritesAdd(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(GetPlaybackMode() == PM_FILE || GetPlaybackMode() == PM_DVD);
}

// TODO: OnFavoritesAdd and OnFavoritesQuickAddFavorite use nearly the same code, do something about it
void CMainFrame::OnFavoritesQuickAddFavorite()
{
    CAppSettings& s = AfxGetAppSettings();
    bool is_BD = false;
    WORD osdMsg = 0;

    if (GetPlaybackMode() == PM_FILE) {
        CString fn =  m_wndPlaylistBar.GetCurFileName();
        if (fn.IsEmpty()) {
            BeginEnumFilters(pGB, pEF, pBF) {
                CComQIPtr<IFileSourceFilter> pFSF = pBF;
                if (pFSF) {
                    LPOLESTR pFN = NULL;
                    AM_MEDIA_TYPE mt;
                    if (SUCCEEDED(pFSF->GetCurFile(&pFN, &mt)) && pFN && *pFN) {
                        fn = CStringW(pFN);
                        CoTaskMemFree(pFN);
                    }
                    break;
                }
            }
            EndEnumFilters;
            if (fn.IsEmpty()) {
                return;
            }
            is_BD = true;
        }

        CString desc = fn;
        desc.Replace('\\', '/');
        int i = desc.Find(_T("://")), j = desc.Find(_T("?")), k = desc.ReverseFind('/');

        if (i >= 0) {
            desc = j >= 0 ? desc.Left(j) : desc;
        } else if (k >= 0) {
            desc = desc.Mid(k + 1);
        }

        CString fn_with_pos(desc);
        if (s.bFavRememberPos) {
            fn_with_pos.Format(_T("%s_%s"), desc, GetVidPos());    // Add file position (time format) so it will be easier to organize later
        }

        // Name
        CString str = fn_with_pos;
        str.Remove(';');

        // RememberPos
        CString pos(_T("0"));
        if (s.bFavRememberPos) {
            pos.Format(_T("%I64d"), GetPos());
        }

        str += ';';
        str += pos;

        // RelativeDrive
        CString relativeDrive;
        relativeDrive.Format(_T("%d"), s.bFavRelativeDrive);

        str += ';';
        str += relativeDrive;

        // Paths
        if (is_BD) {
            str += _T(";") + fn;
        } else {
            CPlaylistItem pli;
            if (m_wndPlaylistBar.GetCur(pli)) {
                POSITION pos = pli.m_fns.GetHeadPosition();
                while (pos) {
                    str += _T(";") + pli.m_fns.GetNext(pos);
                }
            }
        }

        s.AddFav(FAV_FILE, str);
        osdMsg = IDS_FILE_FAV_ADDED;
    } else if (GetPlaybackMode() == PM_DVD) {
        WCHAR path[MAX_PATH];
        ULONG len = 0;
        pDVDI->GetDVDDirectory(path, MAX_PATH, &len);
        CString fn = path;
        fn.TrimRight(_T("/\\"));

        DVD_PLAYBACK_LOCATION2 Location;
        pDVDI->GetCurrentLocation(&Location);
        CString desc;
        desc.Format(_T("%s - T%02d C%02d - %02d:%02d:%02d"), fn, Location.TitleNum, Location.ChapterNum,
                    Location.TimeCode.bHours, Location.TimeCode.bMinutes, Location.TimeCode.bSeconds);

        // Name
        CString str = s.bFavRememberPos ? desc : fn;
        str.Remove(';');

        // RememberPos
        CString pos(_T("0"));
        if (s.bFavRememberPos) {
            CDVDStateStream stream;
            stream.AddRef();

            CComPtr<IDvdState> pStateData;
            CComQIPtr<IPersistStream> pPersistStream;
            if (SUCCEEDED(pDVDI->GetState(&pStateData))
                    && (pPersistStream = pStateData)
                    && SUCCEEDED(OleSaveToStream(pPersistStream, (IStream*)&stream))) {
                pos = BinToCString(stream.m_data.GetData(), stream.m_data.GetCount());
            }
        }

        str += ';';
        str += pos;

        // Paths
        str += ';';
        str += fn;

        s.AddFav(FAV_DVD, str);

        osdMsg = IDS_DVD_FAV_ADDED;
    } else if (GetPlaybackMode() == PM_CAPTURE) {
        // TODO
    }

    if (osdMsg) {
        CString osdMsgStr = ResStr(osdMsg);
        SendStatusMessage(osdMsgStr, 3000);
        m_OSD.DisplayMessage(OSD_TOPLEFT, osdMsgStr, 3000);
    }
}

void CMainFrame::OnFavoritesOrganize()
{
    CFavoriteOrganizeDlg dlg;
    dlg.DoModal();
}

void CMainFrame::OnUpdateFavoritesOrganize(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    CAtlList<CString> sl;
    s.GetFav(FAV_FILE, sl);
    bool enable = !sl.IsEmpty();

    if (!enable) {
        s.GetFav(FAV_DVD, sl);
        enable = !sl.IsEmpty();
    }

    pCmdUI->Enable(enable);
}

void CMainFrame::OnRecentFileClear()
{
    if (IDYES != AfxMessageBox(IDS_RECENT_FILES_QUESTION, MB_ICONQUESTION | MB_YESNO, 0)) {
        return;
    }

    CAppSettings& s = AfxGetAppSettings();

    // Empty MPC-HC's recent menu (iterating reverse because the indexes change)
    for (int i = s.MRU.GetSize() - 1; i >= 0; i--) {
        s.MRU.Remove(i);
    }
    for (int i = s.MRUDub.GetSize() - 1; i >= 0; i--) {
        s.MRUDub.Remove(i);
    }
    s.MRU.WriteList();
    s.MRUDub.WriteList();

    // Empty the "Recent" jump list
    CComPtr<IApplicationDestinations> pDests;
    HRESULT hr = pDests.CoCreateInstance(CLSID_ApplicationDestinations, NULL, CLSCTX_INPROC_SERVER);
    if (SUCCEEDED(hr)) {
        hr = pDests->RemoveAllDestinations();
    }

    // Remove the saved positions in media
    s.filePositions.Empty();
    s.dvdPositions.Empty();
}

void CMainFrame::OnUpdateRecentFileClear(CCmdUI* pCmdUI)
{
    // TODO: Add your command update UI handler code here
}

void CMainFrame::OnFavoritesFile(UINT nID)
{
    nID -= ID_FAVORITES_FILE_START;
    CAtlList<CString> sl;
    AfxGetAppSettings().GetFav(FAV_FILE, sl);

    if (POSITION pos = sl.FindIndex(nID)) {
        PlayFavoriteFile(sl.GetAt(pos));
    }
}

void CMainFrame::PlayFavoriteFile(CString fav)
{
    CAtlList<CString> fns;
    REFERENCE_TIME rtStart = 0;
    BOOL bRelativeDrive = FALSE;
    int i = 0, j = 0;

    for (CString s2 = fav.Tokenize(_T(";"), i);
            !s2.IsEmpty();
            s2 = fav.Tokenize(_T(";"), i), j++) {
        if (j == 0) {
            ;    // desc / name
        } else if (j == 1) {
            _stscanf_s(s2, _T("%I64d"), &rtStart);    // pos
        } else if (j == 2) {
            _stscanf_s(s2, _T("%d"), &bRelativeDrive);    // relative drive
        } else {
            fns.AddTail(s2);    // paths
        }
    }

    // NOTE: This is just for the favorites but we could add a global settings that does this always when on. Could be useful when using removable devices.
    //       All you have to do then is plug in your 500 gb drive, full with movies and/or music, start MPC-HC (from the 500 gb drive) with a preloaded playlist and press play.
    if (bRelativeDrive) {
        // Get the drive MPC-HC is on and apply it to the path list
        CString exePath = GetProgramPath(true);

        CPath exeDrive(exePath);

        if (exeDrive.StripToRoot()) {
            POSITION pos = fns.GetHeadPosition();

            while (pos != NULL) {
                CString& stringPath = fns.GetNext(pos);
                CPath path(stringPath);

                int rootLength = path.SkipRoot();

                if (path.StripToRoot()) {
                    if (_tcsicmp(exeDrive, path) != 0) {   // Do we need to replace the drive letter ?
                        // Replace drive letter
                        CString newPath(exeDrive);

                        newPath += stringPath.Mid(rootLength);  //newPath += stringPath.Mid( 3 );

                        stringPath = newPath;
                    }
                }
            }
        }
    }

    m_wndPlaylistBar.Open(fns, false);
    OpenCurPlaylistItem(max(rtStart, 0));
}

void CMainFrame::OnUpdateFavoritesFile(CCmdUI* pCmdUI)
{
    UINT nID = pCmdUI->m_nID - ID_FAVORITES_FILE_START;
    UNREFERENCED_PARAMETER(nID);
}

void CMainFrame::OnRecentFile(UINT nID)
{
    nID -= ID_RECENT_FILE_START;
    CString str;
    m_recentfiles.GetMenuString(nID + 2, str, MF_BYPOSITION);
    if (!m_wndPlaylistBar.SelectFileInPlaylist(str)) {
        CAtlList<CString> fns;
        fns.AddTail(str);
        m_wndPlaylistBar.Open(fns, false);
    }
    OpenCurPlaylistItem(0);
}

void CMainFrame::OnUpdateRecentFile(CCmdUI* pCmdUI)
{
    UINT nID = pCmdUI->m_nID - ID_RECENT_FILE_START;
    UNREFERENCED_PARAMETER(nID);
}

void CMainFrame::OnFavoritesDVD(UINT nID)
{
    nID -= ID_FAVORITES_DVD_START;

    CAtlList<CString> sl;
    AfxGetAppSettings().GetFav(FAV_DVD, sl);

    if (POSITION pos = sl.FindIndex(nID)) {
        PlayFavoriteDVD(sl.GetAt(pos));
    }
}

void CMainFrame::PlayFavoriteDVD(CString fav)
{
    CString fn;
    CDVDStateStream stream;

    stream.AddRef();

    int i = 0, j = 0;
    for (CString s2 = fav.Tokenize(_T(";"), i);
            !s2.IsEmpty();
            s2 = fav.Tokenize(_T(";"), i), j++) {
        if (j == 0) {
            ;    // desc
        } else if (j == 1 && s2 != _T("0")) { // state
            CStringToBin(s2, stream.m_data);
        } else if (j == 2) {
            fn = s2;    // path
        }
    }

    SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);

    CComPtr<IDvdState> pDvdState;
    HRESULT hr = OleLoadFromStream((IStream*)&stream, IID_IDvdState, (void**)&pDvdState);
    UNREFERENCED_PARAMETER(hr);

    CAutoPtr<OpenDVDData> p(DEBUG_NEW OpenDVDData());
    if (p) {
        p->path = fn;
        p->pDvdState = pDvdState;
    }
    OpenMedia(p);
}

void CMainFrame::OnUpdateFavoritesDVD(CCmdUI* pCmdUI)
{
    UINT nID = pCmdUI->m_nID - ID_FAVORITES_DVD_START;
    UNREFERENCED_PARAMETER(nID);
}

void CMainFrame::OnFavoritesDevice(UINT nID)
{
    nID -= ID_FAVORITES_DEVICE_START;
}

void CMainFrame::OnUpdateFavoritesDevice(CCmdUI* pCmdUI)
{
    UINT nID = pCmdUI->m_nID - ID_FAVORITES_DEVICE_START;
    UNREFERENCED_PARAMETER(nID);
}

// help

void CMainFrame::OnHelpHomepage()
{
    ShellExecute(m_hWnd, _T("open"), _T("http://mpc-hc.sourceforge.net/"), NULL, NULL, SW_SHOWDEFAULT);
}

void CMainFrame::OnHelpCheckForUpdate()
{
    UpdateChecker::CheckForUpdate();
}

/*
void CMainFrame::OnHelpDocumentation()
{
    ShellExecute(m_hWnd, _T("open"), _T("http://sourceforge.net/project/showfiles.php?group_id=82303&package_id=144472"), NULL, NULL, SW_SHOWDEFAULT);
}
*/

void CMainFrame::OnHelpToolbarImages()
{
    ShellExecute(m_hWnd, _T("open"), _T("http://sourceforge.net/apps/trac/mpc-hc/wiki/Toolbar_images"), NULL, NULL, SW_SHOWDEFAULT);
}

void CMainFrame::OnHelpDonate()
{
    ShellExecute(m_hWnd, _T("open"), _T("http://sourceforge.net/donate/index.php?group_id=170561"), NULL, NULL, SW_SHOWDEFAULT);
}

//////////////////////////////////

static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
    CAtlArray<HMONITOR>* ml = (CAtlArray<HMONITOR>*)dwData;
    ml->Add(hMonitor);
    return TRUE;
}

void CMainFrame::SetDefaultWindowRect(int iMonitor)
{
    const CAppSettings& s = AfxGetAppSettings();
    int w, h, x, y;

    if (s.HasFixedWindowSize()) {
        w = s.sizeFixedWindow.cx;
        h = s.sizeFixedWindow.cy;
    } else if (s.fRememberWindowSize) {
        w = s.rcLastWindowPos.Width();
        h = s.rcLastWindowPos.Height();
    } else {
        CRect r1, r2;
        GetClientRect(&r1);
        m_wndView.GetClientRect(&r2);

        CSize logosize = m_wndView.GetLogoSize();
        int _DEFCLIENTW = max(logosize.cx, DEFCLIENTW);
        int _DEFCLIENTH = max(logosize.cy, DEFCLIENTH);

        if (GetSystemMetrics(SM_REMOTESESSION)) {
            _DEFCLIENTH = 0;
        }

        DWORD style = GetStyle();

        w = _DEFCLIENTW + r1.Width() - r2.Width();
        h = _DEFCLIENTH + r1.Height() - r2.Height();

        if (style & WS_THICKFRAME) {
            w += GetSystemMetrics(SM_CXSIZEFRAME) * 2;
            h += GetSystemMetrics(SM_CYSIZEFRAME) * 2;
            if ((style & WS_CAPTION) == 0) {
                w -= 2;
                h -= 2;
            }
        }

        if (style & WS_CAPTION) {
            h += GetSystemMetrics(SM_CYCAPTION);
            if (s.iCaptionMenuMode == MODE_SHOWCAPTIONMENU) {
                h += GetSystemMetrics(SM_CYMENU);
            }
            //else MODE_HIDEMENU
        }
    }

    bool inmonitor = false;
    if (s.fRememberWindowPos) {
        CMonitor monitor;
        CMonitors monitors;
        POINT ptA;
        ptA.x = s.rcLastWindowPos.TopLeft().x;
        ptA.y = s.rcLastWindowPos.TopLeft().y;
        inmonitor = (ptA.x < 0 || ptA.y < 0);
        if (!inmonitor) {
            for (int i = 0; i < monitors.GetCount(); i++) {
                monitor = monitors.GetMonitor(i);
                if (monitor.IsOnMonitor(ptA)) {
                    inmonitor = true;
                    break;
                }
            }
        }
    }

    if (s.fRememberWindowPos && inmonitor) {
        x = s.rcLastWindowPos.TopLeft().x;
        y = s.rcLastWindowPos.TopLeft().y;
    } else {
        HMONITOR hMonitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);

        if (iMonitor > 0) {
            iMonitor--;
            CAtlArray<HMONITOR> ml;
            EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM)&ml);
            if ((size_t)iMonitor < ml.GetCount()) {
                hMonitor = ml[iMonitor];
            }
        }

        MONITORINFO mi;
        mi.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(hMonitor, &mi);

        x = (mi.rcWork.left + mi.rcWork.right - w) / 2; // Center main window
        y = (mi.rcWork.top + mi.rcWork.bottom - h) / 2; // no need to call CenterWindow()
    }

    UINT lastWindowType = s.nLastWindowType;
    MoveWindow(x, y, w, h);

    if (s.iCaptionMenuMode != MODE_SHOWCAPTIONMENU) {
        if (s.iCaptionMenuMode == MODE_FRAMEONLY) {
            ModifyStyle(WS_CAPTION, 0, SWP_NOZORDER);
        } else if (s.iCaptionMenuMode == MODE_BORDERLESS) {
            ModifyStyle(WS_CAPTION | WS_THICKFRAME, 0, SWP_NOZORDER);
        }
        ::SetMenu(m_hWnd, NULL);
        SetWindowPos(NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER);
    }

    if (!s.fRememberWindowPos) {
        CenterWindow();
    }

    if (s.fRememberWindowSize && s.fRememberWindowPos) {
        if (lastWindowType == SIZE_MAXIMIZED) {
            ShowWindow(SW_MAXIMIZE);
        } else if (lastWindowType == SIZE_MINIMIZED) {
            ShowWindow(SW_MINIMIZE);
        }
    }

    if (s.fSavePnSZoom) {
        m_ZoomX = s.dZoomX;
        m_ZoomY = s.dZoomY;
    }
}

void CMainFrame::SetDefaultFullscreenState()
{
    CAppSettings& s = AfxGetAppSettings();

    // Waffs : fullscreen command line
    if (!(s.nCLSwitches & CLSW_ADD) && (s.nCLSwitches & CLSW_FULLSCREEN) && !s.slFiles.IsEmpty()) {
        ToggleFullscreen(true, true);
        SetCursor(NULL);
        s.nCLSwitches &= ~CLSW_FULLSCREEN;
        m_fFirstFSAfterLaunchOnFS = true;
    } else if (s.fRememberWindowSize && s.fRememberWindowPos && !m_fFullScreen && s.fLastFullScreen) {
        // Casimir666 : if fullscreen was on, put it on back
        ToggleFullscreen(true, true);
    }
}

void CMainFrame::RestoreDefaultWindowRect()
{
    const CAppSettings& s = AfxGetAppSettings();

    WINDOWPLACEMENT wp;
    GetWindowPlacement(&wp);

    if (!m_fFullScreen && wp.showCmd != SW_SHOWMAXIMIZED && wp.showCmd != SW_SHOWMINIMIZED
            //&& (GetExStyle()&WS_EX_APPWINDOW)
            && !s.fRememberWindowSize) {
        int x, y, w, h;

        if (s.HasFixedWindowSize()) {
            w = s.sizeFixedWindow.cx;
            h = s.sizeFixedWindow.cy;
        } else {
            CRect r1, r2;
            GetClientRect(&r1);
            m_wndView.GetClientRect(&r2);

            CSize logosize = m_wndView.GetLogoSize();
            int _DEFCLIENTW = max(logosize.cx, DEFCLIENTW);
            int _DEFCLIENTH = max(logosize.cy, DEFCLIENTH);

            DWORD style = GetStyle();
            w = _DEFCLIENTW + r1.Width() - r2.Width();
            h = _DEFCLIENTH + r1.Height() - r2.Height();

            if (style & WS_THICKFRAME) {
                w += GetSystemMetrics(SM_CXSIZEFRAME) * 2;
                h += GetSystemMetrics(SM_CYSIZEFRAME) * 2;
                if ((style & WS_CAPTION) == 0) {
                    w -= 2;
                    h -= 2;
                }
            }

            if (style & WS_CAPTION) {
                h += GetSystemMetrics(SM_CYCAPTION);
                if (s.iCaptionMenuMode == MODE_SHOWCAPTIONMENU) {
                    h += GetSystemMetrics(SM_CYMENU);
                }
                //else MODE_HIDEMENU
            }
        }

        if (s.fRememberWindowPos) {
            x = s.rcLastWindowPos.TopLeft().x;
            y = s.rcLastWindowPos.TopLeft().y;
        } else {
            CRect r;
            GetWindowRect(r);

            x = r.CenterPoint().x - w / 2; // Center window here
            y = r.CenterPoint().y - h / 2; // no need to call CenterWindow()
        }

        MoveWindow(x, y, w, h);

        if (!s.fRememberWindowPos) {
            CenterWindow();
        }
    }
}

OAFilterState CMainFrame::GetMediaState() const
{
    OAFilterState ret = -1;
    if (m_iMediaLoadState == MLS_LOADED) {
        pMC->GetState(0, &ret);
    }
    return ret;
}

void CMainFrame::SetPlaybackMode(int iNewStatus)
{
    m_iPlaybackMode = iNewStatus;
    if (m_wndNavigationBar.IsWindowVisible() && GetPlaybackMode() != PM_CAPTURE) {
        ShowControlBar(&m_wndNavigationBar, !m_wndNavigationBar.IsWindowVisible(), TRUE);
    }
}

CSize CMainFrame::GetVideoSize() const
{
    bool fKeepAspectRatio = AfxGetAppSettings().fKeepAspectRatio;
    bool fCompMonDeskARDiff = AfxGetAppSettings().fCompMonDeskARDiff;

    CSize ret(0, 0);
    if (m_iMediaLoadState != MLS_LOADED || m_fAudioOnly) {
        return ret;
    }

    CSize wh(0, 0), arxy(0, 0);

    if (m_pCAP) {
        wh = m_pCAP->GetVideoSize(false);
        arxy = m_pCAP->GetVideoSize(fKeepAspectRatio);
    } else if (m_pMFVDC) {
        m_pMFVDC->GetNativeVideoSize(&wh, &arxy);   // TODO : check AR !!
    } else {
        pBV->GetVideoSize(&wh.cx, &wh.cy);

        long arx = 0, ary = 0;
        CComQIPtr<IBasicVideo2> pBV2 = pBV;
        // FIXME: It can hang here, for few seconds (CPU goes to 100%), after the window have been moving over to another screen,
        // due to GetPreferredAspectRatio, if it happens before CAudioSwitcherFilter::DeliverEndFlush, it seems.
        if (pBV2 && SUCCEEDED(pBV2->GetPreferredAspectRatio(&arx, &ary)) && arx > 0 && ary > 0) {
            arxy.SetSize(arx, ary);
        }
    }

    if (wh.cx <= 0 || wh.cy <= 0) {
        return ret;
    }

    // with the overlay mixer IBasicVideo2 won't tell the new AR when changed dynamically
    DVD_VideoAttributes VATR;
    if (GetPlaybackMode() == PM_DVD && SUCCEEDED(pDVDI->GetCurrentVideoAttributes(&VATR))) {
        arxy.SetSize(VATR.ulAspectX, VATR.ulAspectY);
    }

    CSize& ar = AfxGetAppSettings().sizeAspectRatio;
    if (ar.cx && ar.cy) {
        arxy = ar;
    }

    ret = (!fKeepAspectRatio || arxy.cx <= 0 || arxy.cy <= 0)
          ? wh
          : CSize(MulDiv(wh.cy, arxy.cx, arxy.cy), wh.cy);

    if (fCompMonDeskARDiff)
        if (HDC hDC = ::GetDC(0)) {
            int _HORZSIZE = GetDeviceCaps(hDC, HORZSIZE);
            int _VERTSIZE = GetDeviceCaps(hDC, VERTSIZE);
            int _HORZRES = GetDeviceCaps(hDC, HORZRES);
            int _VERTRES = GetDeviceCaps(hDC, VERTRES);

            if (_HORZSIZE > 0 && _VERTSIZE > 0 && _HORZRES > 0 && _VERTRES > 0) {
                double a = 1.0 * _HORZSIZE / _VERTSIZE;
                double b = 1.0 * _HORZRES / _VERTRES;

                if (b < a) {
                    ret.cy = (DWORD)(1.0 * ret.cy * a / b);
                } else if (a < b) {
                    ret.cx = (DWORD)(1.0 * ret.cx * b / a);
                }
            }

            ::ReleaseDC(0, hDC);
        }

    return ret;
}

void CMainFrame::ToggleFullscreen(bool fToNearest, bool fSwitchScreenResWhenHasTo)
{
    if (m_pFullscreenWnd->IsWindow()) {
        return;
    }

    CAppSettings& s = AfxGetAppSettings();
    CRect r;
    DWORD dwRemove = 0, dwAdd = 0;
    DWORD dwRemoveEx = 0, dwAddEx = 0;
    HMENU hMenu;
    MONITORINFO mi;
    mi.cbSize = sizeof(MONITORINFO);

    HMONITOR hm     = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
    HMONITOR hm_cur = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);

    CMonitors monitors;

    if (!m_fFullScreen) {
        if (s.bHidePlaylistFullScreen && m_wndPlaylistBar.IsVisible()) {
            m_wndPlaylistBar.SetHiddenDueToFullscreen(true);
            ShowControlBar(&m_wndPlaylistBar, FALSE, TRUE);
        }

        if (!m_fFirstFSAfterLaunchOnFS) {
            GetWindowRect(&m_lastWindowRect);
        }
        if (s.AutoChangeFullscrRes.bEnabled && fSwitchScreenResWhenHasTo && (GetPlaybackMode() != PM_NONE)) {
            AutoChangeMonitorMode();
        }
        m_LastWindow_HM = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);

        CString str;
        CMonitor monitor;
        if (s.strFullScreenMonitor == _T("Current")) {
            hm = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
        } else {
            for (int i = 0; i < monitors.GetCount(); i++) {
                monitor = monitors.GetMonitor(i);
                monitor.GetName(str);
                if ((monitor.IsMonitor()) && (s.strFullScreenMonitor == str)) {
                    hm = monitor;
                    break;
                }
            }
            if (!hm) {
                hm = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
            }
        }

        dwRemove = WS_CAPTION | WS_THICKFRAME;
        GetMonitorInfo(hm, &mi);
        if (fToNearest) {
            r = mi.rcMonitor;
        } else {
            GetDesktopWindow()->GetWindowRect(&r);
        }
        hMenu = NULL;
    } else {
        if (s.AutoChangeFullscrRes.bEnabled && s.AutoChangeFullscrRes.bApplyDefault && s.AutoChangeFullscrRes.dmFullscreenRes[0].fChecked == 1) {
            SetDispMode(s.AutoChangeFullscrRes.dmFullscreenRes[0].dmFSRes, s.strFullScreenMonitor);
        }

        dwAdd = (s.iCaptionMenuMode == MODE_BORDERLESS ? 0 : s.iCaptionMenuMode == MODE_FRAMEONLY ? WS_THICKFRAME : WS_CAPTION | WS_THICKFRAME);
        if (!m_fFirstFSAfterLaunchOnFS) {
            r = m_lastWindowRect;
        }
        hMenu = (s.iCaptionMenuMode == MODE_SHOWCAPTIONMENU) ? m_hMenuDefault : NULL;

        if (s.bHidePlaylistFullScreen && m_wndPlaylistBar.IsHiddenDueToFullscreen()) {
            m_wndPlaylistBar.SetHiddenDueToFullscreen(false);
            ShowControlBar(&m_wndPlaylistBar, TRUE, TRUE);
        }
    }

    m_lastMouseMove.x = m_lastMouseMove.y = -1;

    bool fAudioOnly = m_fAudioOnly;
    m_fAudioOnly = true;

    m_fFullScreen     = !m_fFullScreen;
    s.fLastFullScreen = m_fFullScreen;

    ModifyStyle(dwRemove, dwAdd, SWP_NOZORDER);
    ModifyStyleEx(dwRemoveEx, dwAddEx, SWP_NOZORDER);
    ::SetMenu(m_hWnd, hMenu);

    static bool m_Change_Monitor = false;
    // try disable shader when move from one monitor to other ...
    if (m_fFullScreen) {
        m_Change_Monitor = (hm != hm_cur);
        if ((m_Change_Monitor) && (!m_bToggleShader)) {
            if (m_pCAP) {
                m_pCAP->SetPixelShader(NULL, NULL);
            }
        }
        if (m_Change_Monitor && m_bToggleShaderScreenSpace) {
            if (m_pCAP2) {
                m_pCAP2->SetPixelShader2(NULL, NULL, true);
            }
        }

    } else {
        if (m_Change_Monitor && m_bToggleShader) {
            if (m_pCAP) {
                m_pCAP->SetPixelShader(NULL, NULL);
            }
        }
    }

    if (m_fFullScreen) {
        m_fHideCursor = true;
        if (s.fShowBarsWhenFullScreen) {
            int nTimeOut = s.nShowBarsWhenFullScreenTimeOut;
            if (nTimeOut == 0) {
                ShowControls(CS_NONE);
                ShowControlBar(&m_wndNavigationBar, false, TRUE);
            } else if (nTimeOut > 0) {
                SetTimer(TIMER_FULLSCREENCONTROLBARHIDER, nTimeOut * 1000, NULL);
                SetTimer(TIMER_FULLSCREENMOUSEHIDER, max(nTimeOut * 1000, 2000), NULL);
            }
        } else {
            ShowControls(CS_NONE);
            ShowControlBar(&m_wndNavigationBar, false, TRUE);
        }

        if (s.fPreventMinimize) {
            if (hm != hm_cur) {
                ModifyStyle(WS_MINIMIZEBOX, 0, SWP_NOZORDER);
            }
        }
    } else {
        ModifyStyle(0, WS_MINIMIZEBOX, SWP_NOZORDER);
        KillTimer(TIMER_FULLSCREENCONTROLBARHIDER);
        KillTimer(TIMER_FULLSCREENMOUSEHIDER);
        m_fHideCursor = false;
        ShowControls(m_nCS);
        if (GetPlaybackMode() == PM_CAPTURE && s.iDefaultCaptureDevice == 1) {
            ShowControlBar(&m_wndNavigationBar, !s.fHideNavigation, TRUE);
        }
    }

    m_fAudioOnly = fAudioOnly;

    // Temporarily hide the OSD message if there is one, it will
    // be restored after. This avoid positioning problems.
    m_OSD.HideMessage(true);

    if (m_fFirstFSAfterLaunchOnFS) { //Play started in Fullscreen
        if (s.fRememberWindowSize || s.fRememberWindowPos) {
            r = s.rcLastWindowPos;
            if (!s.fRememberWindowPos) {
                hm = MonitorFromPoint(CPoint(0, 0), MONITOR_DEFAULTTOPRIMARY);
                GetMonitorInfo(hm, &mi);
                CRect m_r = mi.rcMonitor;
                int left = m_r.left + (m_r.Width() - r.Width()) / 2;
                int top = m_r.top + (m_r.Height() - r.Height()) / 2;
                r = CRect(left, top, left + r.Width(), top + r.Height());
            }
            if (!s.fRememberWindowSize) {
                CSize vsize = GetVideoSize();
                r = CRect(r.left, r.top, r.left + vsize.cx, r.top + vsize.cy);
                ShowWindow(SW_HIDE);
            }
            SetWindowPos(NULL, r.left, r.top, r.Width(), r.Height(), SWP_NOZORDER | SWP_NOSENDCHANGING);
            if (!s.fRememberWindowSize) {
                ZoomVideoWindow();
                ShowWindow(SW_SHOW);
            }
        } else {
            if (m_LastWindow_HM != hm_cur) {
                GetMonitorInfo(m_LastWindow_HM, &mi);
                r = mi.rcMonitor;
                ShowWindow(SW_HIDE);
                SetWindowPos(NULL, r.left, r.top, r.Width(), r.Height(), SWP_NOZORDER | SWP_NOSENDCHANGING);
            }
            ZoomVideoWindow();
            if (m_LastWindow_HM != hm_cur) {
                ShowWindow(SW_SHOW);
            }
        }
        m_fFirstFSAfterLaunchOnFS = false;
    } else {
        SetWindowPos(NULL, r.left, r.top, r.Width(), r.Height(), SWP_NOZORDER | SWP_NOSENDCHANGING);
    }

    SetAlwaysOnTop(s.iOnTop);

    MoveVideoWindow();

    m_OSD.HideMessage(false);

    if ((m_Change_Monitor) && (!m_bToggleShader || !m_bToggleShaderScreenSpace)) { // Enabled shader ...
        SetShaders();
    }
}

void CMainFrame::AutoChangeMonitorMode()
{
    const CAppSettings& s = AfxGetAppSettings();
    CStringW mf_hmonitor = s.strFullScreenMonitor;
    double MediaFPS = 0.0;

    if (s.IsD3DFullscreen() && miFPS > 0.9) {
        MediaFPS = miFPS;
    } else if (GetPlaybackMode() == PM_FILE) {
        LONGLONG m_rtTimePerFrame = 1;
        // if ExtractAvgTimePerFrame isn't executed then MediaFPS=10000000.0,
        // (int)(MediaFPS + 0.5)=10000000 and SetDispMode is executed to Default.
        BeginEnumFilters(pGB, pEF, pBF) {
            BeginEnumPins(pBF, pEP, pPin) {
                CMediaTypeEx mt;
                PIN_DIRECTION dir;
                if (FAILED(pPin->QueryDirection(&dir)) || dir != PINDIR_OUTPUT
                        || FAILED(pPin->ConnectionMediaType(&mt))) {
                    continue;
                }
                ExtractAvgTimePerFrame(&mt, m_rtTimePerFrame);
                if (m_rtTimePerFrame == 0) {
                    m_rtTimePerFrame = 1;
                }
            }
            EndEnumPins;
        }
        EndEnumFilters;
        MediaFPS = 10000000.0 / m_rtTimePerFrame;
    } else if (GetPlaybackMode() == PM_DVD) {
        DVD_PLAYBACK_LOCATION2 Location;
        if (pDVDI->GetCurrentLocation(&Location) == S_OK) {
            MediaFPS  = Location.TimeCodeFlags == DVD_TC_FLAG_25fps ? 25.0
                        : Location.TimeCodeFlags == DVD_TC_FLAG_30fps ? 30.0
                        : Location.TimeCodeFlags == DVD_TC_FLAG_DropFrame ? 29.97
                        : 25.0;
        }
    }

    for (size_t rs = 1; rs < MAX_FPS_COUNT ; rs++) {
        if (s.AutoChangeFullscrRes.dmFullscreenRes[rs].fIsData
                && s.AutoChangeFullscrRes.dmFullscreenRes[rs].fChecked
                && MediaFPS >= s.AutoChangeFullscrRes.dmFullscreenRes[rs].vfr_from
                && MediaFPS <= s.AutoChangeFullscrRes.dmFullscreenRes[rs].vfr_to) {

            SetDispMode(s.AutoChangeFullscrRes.dmFullscreenRes[rs].dmFSRes, mf_hmonitor);
            return;
        }
    }
    if (s.AutoChangeFullscrRes.dmFullscreenRes[0].fChecked) {
        SetDispMode(s.AutoChangeFullscrRes.dmFullscreenRes[0].dmFSRes, mf_hmonitor);
    }
}

void CMainFrame::MoveVideoWindow(bool fShowStats)
{
    if ((m_iMediaLoadState == MLS_LOADED) && !m_fAudioOnly && IsWindowVisible()) {
        RECT wr;
        // fullscreen
        if (m_pFullscreenWnd->IsWindow()) {
            m_pFullscreenWnd->GetClientRect(&wr);
        }
        // windowed Mode
        else if (!m_fFullScreen) {
            m_wndView.GetClientRect(&wr);
        }
        // fullscreen on non-primary monitor
        else {
            GetWindowRect(&wr);
            RECT r;
            m_wndView.GetWindowRect(&r);
            wr.left   -= r.left;
            wr.right  -= r.left;
            wr.top    -= r.top;
            wr.bottom -= r.top;
        }

        double dWRWidth  = (double)(wr.right - wr.left);
        double dWRHeight = (double)(wr.bottom - wr.top);

        RECT vr = {0, 0, 0, 0};

        OAFilterState fs = GetMediaState();
        if ((fs == State_Paused) || (fs == State_Running) || (fs == State_Stopped) && (m_fShockwaveGraph || m_fQuicktimeGraph)) {
            SIZE arxy = GetVideoSize();
            double dARx = (double)(arxy.cx);
            double dARy = (double)(arxy.cy);

            dvstype iDefaultVideoSize = static_cast<dvstype>(AfxGetAppSettings().iDefaultVideoSize);
            double dVRWidth, dVRHeight;
            if (iDefaultVideoSize == DVS_HALF) {
                dVRWidth  = dARx * 0.5;
                dVRHeight = dARy * 0.5;
            } else if (iDefaultVideoSize == DVS_NORMAL) {
                dVRWidth  = dARx;
                dVRHeight = dARy;
            } else if (iDefaultVideoSize == DVS_DOUBLE) {
                dVRWidth  = dARx * 2.0;
                dVRHeight = dARy * 2.0;
            } else {
                dVRWidth  = dWRWidth;
                dVRHeight = dWRHeight;
            }

            if (!m_fShockwaveGraph) { // && !m_fQuicktimeGraph)
                double dCRWidth = dVRHeight * dARx / dARy;

                if (iDefaultVideoSize == DVS_FROMINSIDE) {
                    if (dVRWidth < dCRWidth) {
                        dVRHeight = dVRWidth * dARy / dARx;
                    } else {
                        dVRWidth = dCRWidth;
                    }
                } else if (iDefaultVideoSize == DVS_FROMOUTSIDE) {
                    if (dVRWidth > dCRWidth) {
                        dVRHeight = dVRWidth * dARy / dARx;
                    } else {
                        dVRWidth = dCRWidth;
                    }
                } else if ((iDefaultVideoSize == DVS_ZOOM1) || (iDefaultVideoSize == DVS_ZOOM2)) {
                    double minw = dCRWidth;
                    double maxw = dCRWidth;

                    if (dVRWidth < dCRWidth) {
                        minw = dVRWidth;
                    } else {
                        maxw = dVRWidth;
                    }

                    double scale = (iDefaultVideoSize == DVS_ZOOM1) ?
                                   1.0 / 3.0 :
                                   2.0 / 3.0;
                    dVRWidth  = minw + (maxw - minw) * scale;
                    dVRHeight = dVRWidth * dARy / dARx;
                }
            }

            double dScaledVRWidth = m_ZoomX * dVRWidth;
            double dScaledVRHeight = m_ZoomY * dVRHeight;
            // Rounding is required here, else the left-to-right and top-to-bottom sizes will get distorted through rounding twice each
            // Todo: clean this up using decent intrinsic rounding instead of floor(x+.5) and truncation cast to LONG on (y+.5)
            double dPPLeft = floor(m_PosX * (dWRWidth * 3.0 - dScaledVRWidth) - dWRWidth + 0.5);
            double dPPTop  = floor(m_PosY * (dWRHeight * 3.0 - dScaledVRHeight) - dWRHeight + 0.5);
            // left and top parts are allowed to be negative
            vr.left   = (LONG)(dPPLeft);
            vr.top    = (LONG)(dPPTop);
            // right and bottom parts are always at picture center or beyond, so never negative
            vr.right  = (LONG)(dScaledVRWidth + dPPLeft + 0.5);
            vr.bottom = (LONG)(dScaledVRHeight + dPPTop + 0.5);

            if (fShowStats) {
                CString info;
                info.Format(_T("Pos %.3f %.3f, Zoom %.3f %.3f, AR %.3f"), m_PosX, m_PosY, m_ZoomX, m_ZoomY, dScaledVRWidth / dScaledVRHeight);
                SendStatusMessage(info, 3000);
            }
        }

        if (m_pCAP) {
            m_pCAP->SetPosition(wr, vr);
            Vector v(Vector::DegToRad(m_AngleX), Vector::DegToRad(m_AngleY), Vector::DegToRad(m_AngleZ));
            m_pCAP->SetVideoAngle(v);
        } else {
            HRESULT hr;
            hr = pBV->SetDefaultSourcePosition();
            hr = pBV->SetDestinationPosition(vr.left, vr.top, vr.right - vr.left, vr.bottom - vr.top);
            hr = pVW->SetWindowPosition(wr.left, wr.top, wr.right - wr.left, wr.bottom - wr.top);

            if (m_pMFVDC) {
                m_pMFVDC->SetVideoPosition(NULL, &wr);
            }
        }

        m_wndView.SetVideoRect(&wr);
    } else {
        m_wndView.SetVideoRect();
    }

    UpdateThumbarButton();
}

void CMainFrame::HideVideoWindow(bool fHide)
{
    CRect wr;
    if (m_pFullscreenWnd->IsWindow()) {
        m_pFullscreenWnd->GetClientRect(&wr);
    } else if (!m_fFullScreen) {
        m_wndView.GetClientRect(&wr);
    } else {
        GetWindowRect(&wr);

        // this code is needed to work in fullscreen on secondary monitor
        CRect r;
        m_wndView.GetWindowRect(&r);
        wr -= r.TopLeft();
    }

    CRect vr = CRect(0, 0, 0, 0);
    if (m_pCAP) {
        if (fHide) {
            m_pCAP->SetPosition(wr, vr);    // hide
        } else {
            m_pCAP->SetPosition(wr, wr);    // show
        }
    }

}

void CMainFrame::ZoomVideoWindow(bool snap, double scale)
{
    if (m_iMediaLoadState != MLS_LOADED) {
        return;
    }

    const CAppSettings& s = AfxGetAppSettings();

    if (scale <= 0) {
        scale =
            s.iZoomLevel == 0 ? 0.5 :
            s.iZoomLevel == 1 ? 1.0 :
            s.iZoomLevel == 2 ? 2.0 :
            s.iZoomLevel == 3 ? GetZoomAutoFitScale(false) :
            s.iZoomLevel == 4 ? GetZoomAutoFitScale(true) : 1.0;
    }

    if (m_fFullScreen) {
        OnViewFullscreen();
    }

    MINMAXINFO mmi;
    OnGetMinMaxInfo(&mmi);

    CRect r;
    GetWindowRect(r);
    int w = 0, h = 0;

    if (!m_fAudioOnly) {
        CSize arxy = GetVideoSize();

        long lWidth  = long(arxy.cx * scale + 0.5);
        long lHeight = long(arxy.cy * scale + 0.5);

        DWORD style = GetStyle();

        CRect r1, r2;
        GetClientRect(&r1);
        m_wndView.GetClientRect(&r2);

        w = r1.Width() - r2.Width() + lWidth;
        h = r1.Height() - r2.Height() + lHeight;

        if (style & WS_THICKFRAME) {
            w += GetSystemMetrics(SM_CXSIZEFRAME) * 2;
            h += GetSystemMetrics(SM_CYSIZEFRAME) * 2;
            if ((style & WS_CAPTION) == 0) {
                w -= 2;
                h -= 2;
            }
        }

        if (style & WS_CAPTION) {
            h += GetSystemMetrics(SM_CYCAPTION);
            if (s.iCaptionMenuMode == MODE_SHOWCAPTIONMENU) {
                h += GetSystemMetrics(SM_CYMENU);
            }
            //else MODE_HIDEMENU
        }

        if (GetPlaybackMode() == PM_CAPTURE && !s.fHideNavigation && !m_fFullScreen && !m_wndNavigationBar.IsVisible()) {
            CSize r3 = m_wndNavigationBar.CalcFixedLayout(FALSE, TRUE);
            w += r3.cx;
        }

        w = max(w, mmi.ptMinTrackSize.x);
        h = max(h, mmi.ptMinTrackSize.y);
    } else {
        w = r.Width(); // mmi.ptMinTrackSize.x;
        h = mmi.ptMinTrackSize.y;
    }

    // Prevention of going beyond the scopes of screen
    MONITORINFO mi;
    mi.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST), &mi);
    w = min(w, (mi.rcWork.right - mi.rcWork.left));
    h = min(h, (mi.rcWork.bottom - mi.rcWork.top));

    if (!s.fRememberWindowPos) {
        bool isSnapped = false;

        if (snap && s.fSnapToDesktopEdges && m_bWasSnapped) { // check if snapped to edges
            isSnapped = (r.left == mi.rcWork.left) || (r.top == mi.rcWork.top)
                        || (r.right == mi.rcWork.right) || (r.bottom == mi.rcWork.bottom);
        }

        if (isSnapped) { // prefer left, top snap to right, bottom snap
            if (r.left == mi.rcWork.left) {}
            else if (r.right == mi.rcWork.right) {
                r.left = r.right - w;
            }

            if (r.top == mi.rcWork.top) {}
            else if (r.bottom == mi.rcWork.bottom) {
                r.top = r.bottom - h;
            }
        } else {    // center window
            CPoint cp = r.CenterPoint();
            r.left = cp.x - w / 2;
            r.top = cp.y - h / 2;
            m_bWasSnapped = false;
        }
    }

    r.right = r.left + w;
    r.bottom = r.top + h;

    if (r.right > mi.rcWork.right) {
        r.OffsetRect(mi.rcWork.right - r.right, 0);
    }
    if (r.left < mi.rcWork.left) {
        r.OffsetRect(mi.rcWork.left - r.left, 0);
    }
    if (r.bottom > mi.rcWork.bottom) {
        r.OffsetRect(0, mi.rcWork.bottom - r.bottom);
    }
    if (r.top < mi.rcWork.top) {
        r.OffsetRect(0, mi.rcWork.top - r.top);
    }

    if ((m_fFullScreen || !s.HasFixedWindowSize()) && !m_pFullscreenWnd->IsWindow()) {
        MoveWindow(r);
    }

    //ShowWindow(SW_SHOWNORMAL);

    MoveVideoWindow();
}

double CMainFrame::GetZoomAutoFitScale(bool bLargerOnly) const
{
    if (m_iMediaLoadState != MLS_LOADED || m_fAudioOnly) {
        return 1.0;
    }

    const CAppSettings& s = AfxGetAppSettings();

    CSize arxy = GetVideoSize();

    // get the work area
    MONITORINFO mi;
    mi.cbSize = sizeof(MONITORINFO);
    HMONITOR hMonitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
    GetMonitorInfo(hMonitor, &mi);
    RECT& wa = mi.rcWork;

    DWORD style = GetStyle();
    CSize decorationsSize(0, 0);

    if (style & WS_CAPTION) {
        // caption
        decorationsSize.cy += GetSystemMetrics(SM_CYCAPTION);
        // menu
        if (s.iCaptionMenuMode == MODE_SHOWCAPTIONMENU) {
            decorationsSize.cy += GetSystemMetrics(SM_CYMENU);
        }
    }

    if (style & WS_THICKFRAME) {
        // vertical borders
        decorationsSize.cx += 2 * ::GetSystemMetrics(SM_CXSIZEFRAME);
        // horizontal borders
        decorationsSize.cy += 2 * ::GetSystemMetrics(SM_CYSIZEFRAME);
        if (!(style & WS_CAPTION)) {
            decorationsSize.cx -= 2;
            decorationsSize.cy -= 2;
        }
    }

    RECT r;
    if (m_wndSeekBar.IsVisible()) {
        m_wndSeekBar.GetWindowRect(&r);
        decorationsSize.cy += r.bottom - r.top;
    }
    if (m_wndToolBar.IsVisible()) {
        m_wndToolBar.GetWindowRect(&r);
        decorationsSize.cy += r.bottom - r.top;
    }
    if (m_wndInfoBar.IsVisible()) {
        m_wndInfoBar.GetWindowRect(&r);
        decorationsSize.cy += r.bottom - r.top;
    }
    if (m_wndStatsBar.IsVisible()) {
        m_wndStatsBar.GetWindowRect(&r);
        decorationsSize.cy += r.bottom - r.top;
    }
    if (m_wndStatusBar.IsVisible()) {
        m_wndStatusBar.GetWindowRect(&r);
        decorationsSize.cy += r.bottom - r.top;
    }

    LONG width = wa.right - wa.left;
    LONG height = wa.bottom - wa.top;
    if (bLargerOnly && (arxy.cx + decorationsSize.cx < width && arxy.cy + decorationsSize.cy < height)) {
        return 1.0;
    }

    double sx = ((double)width  * s.nAutoFitFactor / 100 - decorationsSize.cx) / arxy.cx;
    double sy = ((double)height * s.nAutoFitFactor / 100 - decorationsSize.cy) / arxy.cy;
    sx = min(sx, sy);
    // Take movie aspect ratio into consideration
    // The scaling is computed so that the height is an integer value
    sy = floor(arxy.cy * floor(arxy.cx * sx + 0.5) / arxy.cx + 0.5) / arxy.cy;

    return sy;
}

void CMainFrame::RepaintVideo()
{
    if (m_pCAP) {
        m_pCAP->Paint(false);
    }
}

void CMainFrame::SetShaders()
{
    if (!m_pCAP) {
        return;
    }

    const CAppSettings& s = AfxGetAppSettings();

    CAtlStringMap<const CAppSettings::Shader*> s2s;

    POSITION pos = s.m_shaders.GetHeadPosition();
    while (pos) {
        const CAppSettings::Shader* pShader = &s.m_shaders.GetNext(pos);
        s2s[pShader->label] = pShader;
    }

    m_pCAP->SetPixelShader(NULL, NULL);
    if (m_pCAP2) {
        m_pCAP2->SetPixelShader2(NULL, NULL, true);
    }

    for (int i = 0; i < 2; ++i) {
        if (i == 0 && !m_bToggleShader) {
            continue;
        }
        if (i == 1 && !m_bToggleShaderScreenSpace) {
            continue;
        }
        CAtlList<CString> labels;

        CAtlList<CString>* pLabels;
        if (i == 0) {
            pLabels = &m_shaderlabels;
        } else {
            if (!m_pCAP2) {
                break;
            }
            pLabels = &m_shaderlabelsScreenSpace;
        }

        pos = pLabels->GetHeadPosition();
        while (pos) {
            const CAppSettings::Shader* pShader = NULL;
            if (s2s.Lookup(pLabels->GetNext(pos), pShader)) {
                CStringA target = pShader->target;
                CStringA srcdata = pShader->srcdata;

                HRESULT hr;
                if (i == 0) {
                    hr = m_pCAP->SetPixelShader(srcdata, target);
                } else {
                    hr = m_pCAP2->SetPixelShader2(srcdata, target, true);
                }

                if (FAILED(hr)) {
                    m_pCAP->SetPixelShader(NULL, NULL);
                    if (m_pCAP2) {
                        m_pCAP2->SetPixelShader2(NULL, NULL, true);
                    }
                    SendStatusMessage(ResStr(IDS_MAINFRM_73) + pShader->label, 3000);
                    return;
                }

                labels.AddTail(pShader->label);
            }
        }

        if (m_iMediaLoadState == MLS_LOADED) {
            CString str = Implode(labels, '|');
            str.Replace(_T("|"), _T(", "));
            SendStatusMessage(ResStr(IDS_AG_SHADER) + str, 3000);
        }
    }
}

void CMainFrame::UpdateShaders(CString label)
{
    if (!m_pCAP) {
        return;
    }

    if (m_shaderlabels.GetCount() <= 1) {
        m_shaderlabels.RemoveAll();
    }

    if (m_shaderlabels.IsEmpty() && !label.IsEmpty()) {
        m_shaderlabels.AddTail(label);
    }

    bool fUpdate = m_shaderlabels.IsEmpty();

    POSITION pos = m_shaderlabels.GetHeadPosition();
    while (pos) {
        if (label == m_shaderlabels.GetNext(pos)) {
            fUpdate = true;
            break;
        }
    }

    if (fUpdate) {
        SetShaders();
    }

}

void CMainFrame::SetBalance(int balance)
{
    int sign = balance > 0 ? -1 : 1; // -1: invert sign for more right channel
    int balance_dB;
    if (balance > -100 && balance < 100) {
        balance_dB = sign * (int)(100 * 20 * log10(1 - abs(balance) / 100.0f));
    } else {
        balance_dB = sign * (-10000);  // -10000: only left, 10000: only right
    }

    if (m_iMediaLoadState == MLS_LOADED) {
        CString strBalance, strBalanceOSD;

        pBA->put_Balance(balance_dB);

        if (balance == 0) {
            strBalance = L"L = R";
        } else if (balance < 0) {
            strBalance.Format(L"L +%d%%", -balance);
        } else { //if (m_nBalance > 0)
            strBalance.Format(L"R +%d%%", balance);
        }

        strBalanceOSD.Format(IDS_BALANCE_OSD, strBalance);
        m_OSD.DisplayMessage(OSD_TOPLEFT, strBalanceOSD);
    }
}

void CMainFrame::SetupIViAudReg()
{
    if (!AfxGetAppSettings().fAutoSpeakerConf) {
        return;
    }

    DWORD spc = 0, defchnum = 0;

    if (AfxGetAppSettings().fAutoSpeakerConf) {
        CComPtr<IDirectSound> pDS;
        if (SUCCEEDED(DirectSoundCreate(NULL, &pDS, NULL))
                && SUCCEEDED(pDS->SetCooperativeLevel(m_hWnd, DSSCL_NORMAL))) {
            if (SUCCEEDED(pDS->GetSpeakerConfig(&spc))) {
                switch (spc) {
                    case DSSPEAKER_DIRECTOUT:
                        defchnum = 6;
                        break;
                    case DSSPEAKER_HEADPHONE:
                        defchnum = 2;
                        break;
                    case DSSPEAKER_MONO:
                        defchnum = 1;
                        break;
                    case DSSPEAKER_QUAD:
                        defchnum = 4;
                        break;
                    default:
                    case DSSPEAKER_STEREO:
                        defchnum = 2;
                        break;
                    case DSSPEAKER_SURROUND:
                        defchnum = 2;
                        break;
                    case DSSPEAKER_5POINT1:
                        defchnum = 5;
                        break;
                    case DSSPEAKER_7POINT1:
                        defchnum = 5;
                        break;
                }
            }
        }
    } else {
        defchnum = 2;
    }

    CRegKey iviaud;
    if (ERROR_SUCCESS == iviaud.Create(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\InterVideo\\Common\\AudioDec"))) {
        DWORD chnum = 0;
        if (FAILED(iviaud.QueryDWORDValue(_T("AUDIO"), chnum))) {
            chnum = 0;
        }
        if (chnum <= defchnum) { // check if the user has already set it..., but we won't skip if it's lower than sensible :P
            iviaud.SetDWORDValue(_T("AUDIO"), defchnum);
        }
    }
}

//
// Open/Close
//

bool CMainFrame::IsRealEngineCompatible(CString strFilename) const
{
    // Real Media engine didn't support Unicode filename (nor filenames with # characters)
    for (int i = 0; i < strFilename.GetLength(); i++) {
        WCHAR Char = strFilename[i];
        if (Char < 32 || Char > 126 || Char == 35) {
            return false;
        }
    }
    return true;
}

void CMainFrame::OpenCreateGraphObject(OpenMediaData* pOMD)
{
    ASSERT(pGB == NULL);

    m_fCustomGraph = false;
    m_fRealMediaGraph = m_fShockwaveGraph = m_fQuicktimeGraph = false;

    const CAppSettings& s = AfxGetAppSettings();

    // CASIMIR666 todo
    if (s.IsD3DFullscreen() &&
            ((s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS)
             || (s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM)
             || (s.iDSVideoRendererType == VIDRNDT_DS_MADVR)
             || (s.iDSVideoRendererType == VIDRNDT_DS_SYNC))) {
        CreateFullScreenWindow();
        m_pVideoWnd = m_pFullscreenWnd;
    } else {
        m_pVideoWnd = &m_wndView;
    }

    if (OpenFileData* p = dynamic_cast<OpenFileData*>(pOMD)) {
        engine_t engine = s.m_Formats.GetEngine(p->fns.GetHead());

        CStringA ct = GetContentType(p->fns.GetHead());

        if (ct == "video/x-ms-asf") {
            // TODO: put something here to make the windows media source filter load later
        } else if (ct == "audio/x-pn-realaudio"
                   || ct == "audio/x-pn-realaudio-plugin"
                   || ct == "audio/x-realaudio-secure"
                   || ct == "video/vnd.rn-realvideo-secure"
                   || ct == "application/vnd.rn-realmedia"
                   || ct.Find("vnd.rn-") >= 0
                   || ct.Find("realaudio") >= 0
                   || ct.Find("realvideo") >= 0) {
            engine = RealMedia;
        } else if (ct == "application/x-shockwave-flash") {
            engine = ShockWave;
        } else if (ct == "video/quicktime"
                   || ct == "application/x-quicktimeplayer") {
            engine = QuickTime;
        }

        HRESULT hr = E_FAIL;
        CComPtr<IUnknown> pUnk;

        if (engine == RealMedia) {
            // TODO : see why Real SDK crash here ...
            //if (!IsRealEngineCompatible(p->fns.GetHead()))
            //  throw ResStr(IDS_REALVIDEO_INCOMPATIBLE);

            pUnk = (IUnknown*)(INonDelegatingUnknown*)DEBUG_NEW DSObjects::CRealMediaGraph(m_pVideoWnd->m_hWnd, hr);
            if (!pUnk) {
                throw(UINT)IDS_AG_OUT_OF_MEMORY;
            }

            if (SUCCEEDED(hr)) {
                pGB = CComQIPtr<IGraphBuilder>(pUnk);
                if (pGB) {
                    m_fRealMediaGraph = true;
                }
            }
        } else if (engine == ShockWave) {
            pUnk = (IUnknown*)(INonDelegatingUnknown*)DEBUG_NEW DSObjects::CShockwaveGraph(m_pVideoWnd->m_hWnd, hr);
            if (!pUnk) {
                throw(UINT)IDS_AG_OUT_OF_MEMORY;
            }

            if (SUCCEEDED(hr)) {
                pGB = CComQIPtr<IGraphBuilder>(pUnk);
            }
            if (FAILED(hr) || !pGB) {
                throw(UINT)IDS_MAINFRM_77;
            }
            m_fShockwaveGraph = true;
        } else if (engine == QuickTime) {
#ifdef _WIN64   // TODOX64
            //MessageBox (ResStr(IDS_MAINFRM_78), _T(""), MB_OK);
#else
            pUnk = (IUnknown*)(INonDelegatingUnknown*)DEBUG_NEW DSObjects::CQuicktimeGraph(m_pVideoWnd->m_hWnd, hr);
            if (!pUnk) {
                throw(UINT)IDS_AG_OUT_OF_MEMORY;
            }

            if (SUCCEEDED(hr)) {
                pGB = CComQIPtr<IGraphBuilder>(pUnk);
                if (pGB) {
                    m_fQuicktimeGraph = true;
                }
            }
#endif
        }

        m_fCustomGraph = m_fRealMediaGraph || m_fShockwaveGraph || m_fQuicktimeGraph;

        if (!m_fCustomGraph) {
            pGB = DEBUG_NEW CFGManagerPlayer(_T("CFGManagerPlayer"), NULL, m_pVideoWnd->m_hWnd);
        }
    } else if (OpenDVDData* p = dynamic_cast<OpenDVDData*>(pOMD)) {
        pGB = DEBUG_NEW CFGManagerDVD(_T("CFGManagerDVD"), NULL, m_pVideoWnd->m_hWnd);
    } else if (OpenDeviceData* p = dynamic_cast<OpenDeviceData*>(pOMD)) {
        if (s.iDefaultCaptureDevice == 1) {
            pGB = DEBUG_NEW CFGManagerBDA(_T("CFGManagerBDA"), NULL, m_pVideoWnd->m_hWnd);
        } else {
            pGB = DEBUG_NEW CFGManagerCapture(_T("CFGManagerCapture"), NULL, m_pVideoWnd->m_hWnd);
        }
    }

    if (!pGB) {
        throw(UINT)IDS_MAINFRM_80;
    }

    pGB->AddToROT();

    pMC = pGB;
    pME = pGB;
    pMS = pGB; // general
    pVW = pGB;
    pBV = pGB; // video
    pBA = pGB; // audio
    pFS = pGB;

    if (!(pMC && pME && pMS)
            || !(pVW && pBV)
            || !(pBA)) {
        throw(UINT)IDS_GRAPH_INTERFACES_ERROR;
    }

    if (FAILED(pME->SetNotifyWindow((OAHWND)m_hWnd, WM_GRAPHNOTIFY, 0))) {
        throw(UINT)IDS_GRAPH_TARGET_WND_ERROR;
    }

    m_pProv = (IUnknown*)DEBUG_NEW CKeyProvider();

    if (CComQIPtr<IObjectWithSite> pObjectWithSite = pGB) {
        pObjectWithSite->SetSite(m_pProv);
    }

    m_pCB = DEBUG_NEW CDSMChapterBag(NULL, NULL);
}

CWnd* CMainFrame::GetModalParent()
{
    const CAppSettings& s = AfxGetAppSettings();
    CWnd* pParentWnd = this;
    if (m_pFullscreenWnd->IsWindow() && s.m_RenderersSettings.m_AdvRendSets.iVMR9FullscreenGUISupport) {
        pParentWnd = m_pFullscreenWnd;
    }
    return pParentWnd;
}

void CMainFrame::OpenFile(OpenFileData* pOFD)
{
    if (pOFD->fns.IsEmpty()) {
        throw(UINT)IDS_MAINFRM_81;
    }

    CAppSettings& s = AfxGetAppSettings();

    bool bMainFile = true;

    POSITION pos = pOFD->fns.GetHeadPosition();
    while (pos) {
        CString fn = pOFD->fns.GetNext(pos);

        fn.Trim();
        if (fn.IsEmpty() && !bMainFile) {
            break;
        }

        HRESULT hr = pGB->RenderFile(CStringW(fn), NULL);

        if (bMainFile && s.fKeepHistory && s.fRememberFilePos && !s.filePositions.AddEntry(fn)) {
            REFERENCE_TIME rtPos = s.filePositions.GetLatestEntry()->llPosition;
            if (pMS) {
                pMS->SetPositions(&rtPos, AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);
            }
        }
        QueryPerformanceCounter(&m_liLastSaveTime);

        if (FAILED(hr)) {
            if (bMainFile) {
                if (s.fReportFailedPins) {
                    CComQIPtr<IGraphBuilderDeadEnd> pGBDE = pGB;
                    if (pGBDE && pGBDE->GetCount()) {
                        CMediaTypesDlg(pGBDE, GetModalParent()).DoModal();
                    }
                }

                UINT err;

                switch (hr) {
                    case E_ABORT:
                        err = IDS_MAINFRM_82;
                        break;
                    case E_FAIL:
                    case E_POINTER:
                    default:
                        err = IDS_MAINFRM_83;
                        break;
                    case E_INVALIDARG:
                        err = IDS_MAINFRM_84;
                        break;
                    case E_OUTOFMEMORY:
                        err = IDS_AG_OUT_OF_MEMORY;
                        break;
                    case VFW_E_CANNOT_CONNECT:
                        err = IDS_MAINFRM_86;
                        break;
                    case VFW_E_CANNOT_LOAD_SOURCE_FILTER:
                        err = IDS_MAINFRM_87;
                        break;
                    case VFW_E_CANNOT_RENDER:
                        err = IDS_MAINFRM_88;
                        break;
                    case VFW_E_INVALID_FILE_FORMAT:
                        err = IDS_MAINFRM_89;
                        break;
                    case VFW_E_NOT_FOUND:
                        err = IDS_MAINFRM_90;
                        break;
                    case VFW_E_UNKNOWN_FILE_TYPE:
                        err = IDS_MAINFRM_91;
                        break;
                    case VFW_E_UNSUPPORTED_STREAM:
                        err = IDS_MAINFRM_92;
                        break;
                }

                throw err;
            }
        }

        if (s.fKeepHistory) {
            CRecentFileList* pMRU = bMainFile ? &s.MRU : &s.MRUDub;
            pMRU->ReadList();
            pMRU->Add(fn);
            pMRU->WriteList();
            SHAddToRecentDocs(SHARD_PATH, fn);
        }

        if (bMainFile) {
            pOFD->title = fn;
        }

        bMainFile = false;

        if (m_fCustomGraph) {
            break;
        }
    }

    if (s.fReportFailedPins) {
        CComQIPtr<IGraphBuilderDeadEnd> pGBDE = pGB;
        if (pGBDE && pGBDE->GetCount()) {
            CMediaTypesDlg(pGBDE, GetModalParent()).DoModal();
        }
    }

    if (!(pAMOP = pGB)) {
        BeginEnumFilters(pGB, pEF, pBF);
        if (pAMOP = pBF) {
            break;
        }
        EndEnumFilters;
    }

    if (FindFilter(__uuidof(CShoutcastSource), pGB)) {
        m_fUpdateInfoBar = true;
    }

    SetupChapters();

    CComQIPtr<IKeyFrameInfo> pKFI;
    BeginEnumFilters(pGB, pEF, pBF);
    if (pKFI = pBF) {
        break;
    }
    EndEnumFilters;
    UINT nKFs = 0;
    if (pKFI && S_OK == pKFI->GetKeyFrameCount(nKFs) && nKFs > 0) {
        UINT k = nKFs;
        if (!m_kfs.SetCount(k) || S_OK != pKFI->GetKeyFrames(&TIME_FORMAT_MEDIA_TIME, m_kfs.GetData(), k) || k != nKFs) {
            m_kfs.RemoveAll();
        }
    }

    SetPlaybackMode(PM_FILE);
}

void CMainFrame::SetupChapters()
{
    // Release the old chapter bag and create a new one.
    // Due to smart pointers the old chapter bag won't
    // be deleted until all classes release it.
    m_pCB.Release();
    m_pCB = DEBUG_NEW CDSMChapterBag(NULL, NULL);

    CInterfaceList<IBaseFilter> pBFs;
    BeginEnumFilters(pGB, pEF, pBF);
    pBFs.AddTail(pBF);
    EndEnumFilters;

    POSITION pos;

    pos = pBFs.GetHeadPosition();
    while (pos && !m_pCB->ChapGetCount()) {
        IBaseFilter* pBF = pBFs.GetNext(pos);

        CComQIPtr<IDSMChapterBag> pCB = pBF;
        if (!pCB) {
            continue;
        }

        for (DWORD i = 0, cnt = pCB->ChapGetCount(); i < cnt; i++) {
            REFERENCE_TIME rt;
            CComBSTR name;
            if (SUCCEEDED(pCB->ChapGet(i, &rt, &name))) {
                m_pCB->ChapAppend(rt, name);
            }
        }
    }

    pos = pBFs.GetHeadPosition();
    while (pos && !m_pCB->ChapGetCount()) {
        IBaseFilter* pBF = pBFs.GetNext(pos);

        CComQIPtr<IChapterInfo> pCI = pBF;
        if (!pCI) {
            continue;
        }

        CHAR iso6391[3];
        ::GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, iso6391, 3);
        CStringA iso6392 = ISO6391To6392(iso6391);
        if (iso6392.GetLength() < 3) {
            iso6392 = "eng";
        }

        UINT cnt = pCI->GetChapterCount(CHAPTER_ROOT_ID);
        for (UINT i = 1; i <= cnt; i++) {
            UINT cid = pCI->GetChapterId(CHAPTER_ROOT_ID, i);

            ChapterElement ce;
            if (pCI->GetChapterInfo(cid, &ce)) {
                char pl[3] = {iso6392[0], iso6392[1], iso6392[2]};
                char cc[] = "  ";
                CComBSTR name;
                name.Attach(pCI->GetChapterStringInfo(cid, pl, cc));
                m_pCB->ChapAppend(ce.rtStart, name);
            }
        }
    }

    pos = pBFs.GetHeadPosition();
    while (pos && !m_pCB->ChapGetCount()) {
        IBaseFilter* pBF = pBFs.GetNext(pos);

        CComQIPtr<IAMExtendedSeeking, &IID_IAMExtendedSeeking> pES = pBF;
        if (!pES) {
            continue;
        }

        long MarkerCount = 0;
        if (SUCCEEDED(pES->get_MarkerCount(&MarkerCount))) {
            for (long i = 1; i <= MarkerCount; i++) {
                double MarkerTime = 0;
                if (SUCCEEDED(pES->GetMarkerTime(i, &MarkerTime))) {
                    CStringW name;
                    name.Format(IDS_AG_CHAPTER, i);

                    CComBSTR bstr;
                    if (S_OK == pES->GetMarkerName(i, &bstr)) {
                        name = bstr;
                    }

                    m_pCB->ChapAppend(REFERENCE_TIME(MarkerTime * 10000000), name);
                }
            }
        }
    }

    pos = pBFs.GetHeadPosition();
    while (pos && !m_pCB->ChapGetCount()) {
        IBaseFilter* pBF = pBFs.GetNext(pos);

        if (GetCLSID(pBF) != CLSID_OggSplitter) {
            continue;
        }

        BeginEnumPins(pBF, pEP, pPin) {
            if (m_pCB->ChapGetCount()) {
                break;
            }

            if (CComQIPtr<IPropertyBag> pPB = pPin) {
                for (int i = 1; ; i++) {
                    CStringW str;
                    CComVariant var;

                    var.Clear();
                    str.Format(L"CHAPTER%02d", i);
                    if (S_OK != pPB->Read(str, &var, NULL)) {
                        break;
                    }

                    int h, m, s, ms;
                    WCHAR wc;
                    if (7 != swscanf_s(CStringW(var), L"%d%c%d%c%d%c%d", &h,
                                       &wc, sizeof(WCHAR), &m,
                                       &wc, sizeof(WCHAR), &s,
                                       &wc, sizeof(WCHAR), &ms)) {
                        break;
                    }

                    CStringW name;
                    name.Format(IDS_AG_CHAPTER, i);
                    var.Clear();
                    str += L"NAME";
                    if (S_OK == pPB->Read(str, &var, NULL)) {
                        name = var;
                    }

                    m_pCB->ChapAppend(10000i64 * (((h * 60 + m) * 60 + s) * 1000 + ms), name);
                }
            }
        }
        EndEnumPins;
    }

    m_pCB->ChapSort();

    if (AfxGetAppSettings().fShowChapters) {
        m_wndSeekBar.SetChapterBag(m_pCB);
    }
}

void CMainFrame::SetupDVDChapters()
{
    // Release the old chapter bag and create a new one.
    // Due to smart pointers the old chapter bag won't
    // be deleted until all classes release it.
    m_pCB.Release();
    m_pCB = DEBUG_NEW CDSMChapterBag(NULL, NULL);

    WCHAR buff[MAX_PATH];
    ULONG len = 0;
    DVD_PLAYBACK_LOCATION2 loc;
    if (pDVDI && SUCCEEDED(pDVDI->GetDVDDirectory(buff, _countof(buff), &len)) &&
            SUCCEEDED(pDVDI->GetCurrentLocation(&loc))) {
        CStringW path;
        path.Format(L"%s\\VTS_%02d_0.IFO", buff, loc.TitleNum);

        CVobFile vob;
        CAtlList<CString> files;
        if (vob.Open(path, files)) {
            for (int i = 0; i < vob.GetChaptersCount(); i++) {
                REFERENCE_TIME rt = vob.GetChapterOffset(i);

                CStringW str;
                str.Format(IDS_AG_CHAPTER, i + 1);

                m_pCB->ChapAppend(rt, str);
            }
            vob.Close();
        }
    }

    m_pCB->ChapSort();

    if (AfxGetAppSettings().fShowChapters) {
        m_wndSeekBar.SetChapterBag(m_pCB);
    }
}

void CMainFrame::OpenDVD(OpenDVDData* pODD)
{
    HRESULT hr = pGB->RenderFile(CStringW(pODD->path), NULL);

    const CAppSettings& s = AfxGetAppSettings();

    if (s.fReportFailedPins) {
        CComQIPtr<IGraphBuilderDeadEnd> pGBDE = pGB;
        if (pGBDE && pGBDE->GetCount()) {
            CMediaTypesDlg(pGBDE, GetModalParent()).DoModal();
        }
    }

    BeginEnumFilters(pGB, pEF, pBF) {
        if ((pDVDC = pBF) && (pDVDI = pBF)) {
            break;
        }
    }
    EndEnumFilters;

    if (hr == E_INVALIDARG) {
        throw(UINT)IDS_MAINFRM_93;
    } else if (hr == VFW_E_CANNOT_RENDER) {
        throw(UINT)IDS_DVD_NAV_ALL_PINS_ERROR;
    } else if (hr == VFW_S_PARTIAL_RENDER) {
        throw(UINT)IDS_DVD_NAV_SOME_PINS_ERROR;
    } else if (hr == E_NOINTERFACE || !pDVDC || !pDVDI) {
        throw(UINT)IDS_DVD_INTERFACES_ERROR;
    } else if (hr == VFW_E_CANNOT_LOAD_SOURCE_FILTER) {
        throw(UINT)IDS_MAINFRM_94;
    } else if (FAILED(hr)) {
        throw(UINT)IDS_AG_FAILED;
    }

    WCHAR buff[MAX_PATH];
    ULONG len = 0;
    if (SUCCEEDED(hr = pDVDI->GetDVDDirectory(buff, _countof(buff), &len))) {
        pODD->title = CString(CStringW(buff));
    }

    // TODO: resetdvd
    pDVDC->SetOption(DVD_ResetOnStop, FALSE);
    pDVDC->SetOption(DVD_HMSF_TimeCodeEvents, TRUE);

    if (s.idMenuLang) {
        pDVDC->SelectDefaultMenuLanguage(s.idMenuLang);
    }
    if (s.idAudioLang) {
        pDVDC->SelectDefaultAudioLanguage(s.idAudioLang, DVD_AUD_EXT_NotSpecified);
    }
    if (s.idSubtitlesLang) {
        pDVDC->SelectDefaultSubpictureLanguage(s.idSubtitlesLang, DVD_SP_EXT_NotSpecified);
    }

    m_iDVDDomain = DVD_DOMAIN_Stop;

    SetPlaybackMode(PM_DVD);
}

HRESULT CMainFrame::OpenBDAGraph()
{
    HRESULT hr = pGB->RenderFile(L"", L"");
    if (SUCCEEDED(hr)) {
        AddTextPassThruFilter();
        SetPlaybackMode(PM_CAPTURE);
    }
    return hr;
}

void CMainFrame::OpenCapture(OpenDeviceData* pODD)
{
    CStringW vidfrname, audfrname;
    CComPtr<IBaseFilter> pVidCapTmp, pAudCapTmp;

    m_VidDispName = pODD->DisplayName[0];

    if (!m_VidDispName.IsEmpty()) {
        if (!CreateFilter(m_VidDispName, &pVidCapTmp, vidfrname)) {
            throw(UINT)IDS_MAINFRM_96;
        }
    }

    m_AudDispName = pODD->DisplayName[1];

    if (!m_AudDispName.IsEmpty()) {
        if (!CreateFilter(m_AudDispName, &pAudCapTmp, audfrname)) {
            throw(UINT)IDS_MAINFRM_96;
        }
    }

    if (!pVidCapTmp && !pAudCapTmp) {
        throw(UINT)IDS_MAINFRM_98;
    }

    pCGB = NULL;
    pVidCap = NULL;
    pAudCap = NULL;

    if (FAILED(pCGB.CoCreateInstance(CLSID_CaptureGraphBuilder2))) {
        throw(UINT)IDS_MAINFRM_99;
    }

    HRESULT hr;

    pCGB->SetFiltergraph(pGB);

    if (pVidCapTmp) {
        if (FAILED(hr = pGB->AddFilter(pVidCapTmp, vidfrname))) {
            throw(UINT)IDS_CAPTURE_ERROR_VID_FILTER;
        }

        pVidCap = pVidCapTmp;

        if (!pAudCapTmp) {
            if (FAILED(pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Interleaved, pVidCap, IID_IAMStreamConfig, (void**)&pAMVSCCap))
                    && FAILED(pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, pVidCap, IID_IAMStreamConfig, (void**)&pAMVSCCap))) {
                TRACE(_T("Warning: No IAMStreamConfig interface for vidcap capture"));
            }

            if (FAILED(pCGB->FindInterface(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Interleaved, pVidCap, IID_IAMStreamConfig, (void**)&pAMVSCPrev))
                    && FAILED(pCGB->FindInterface(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, pVidCap, IID_IAMStreamConfig, (void**)&pAMVSCPrev))) {
                TRACE(_T("Warning: No IAMStreamConfig interface for vidcap capture"));
            }

            if (FAILED(pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Audio, pVidCap, IID_IAMStreamConfig, (void**)&pAMASC))
                    && FAILED(pCGB->FindInterface(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Audio, pVidCap, IID_IAMStreamConfig, (void**)&pAMASC))) {
                TRACE(_T("Warning: No IAMStreamConfig interface for vidcap"));
            } else {
                pAudCap = pVidCap;
            }
        } else {
            if (FAILED(pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, pVidCap, IID_IAMStreamConfig, (void**)&pAMVSCCap))) {
                TRACE(_T("Warning: No IAMStreamConfig interface for vidcap capture"));
            }

            if (FAILED(pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, pVidCap, IID_IAMStreamConfig, (void**)&pAMVSCPrev))) {
                TRACE(_T("Warning: No IAMStreamConfig interface for vidcap capture"));
            }
        }

        if (FAILED(pCGB->FindInterface(&LOOK_UPSTREAM_ONLY, NULL, pVidCap, IID_IAMCrossbar, (void**)&pAMXBar))) {
            TRACE(_T("Warning: No IAMCrossbar interface was found\n"));
        }

        if (FAILED(pCGB->FindInterface(&LOOK_UPSTREAM_ONLY, NULL, pVidCap, IID_IAMTVTuner, (void**)&pAMTuner))) {
            TRACE(_T("Warning: No IAMTVTuner interface was found\n"));
        }
        /*
                if (pAMVSCCap)
                {
                    //DumpStreamConfig(_T("c:\\mpclog.txt"), pAMVSCCap);
                    CComQIPtr<IAMVfwCaptureDialogs> pVfwCD = pVidCap;
                    if (!pAMXBar && pVfwCD)
                    {
                        m_wndCaptureBar.m_capdlg.SetupVideoControls(viddispname, pAMVSCCap, pVfwCD);
                    }
                    else
                    {
                        m_wndCaptureBar.m_capdlg.SetupVideoControls(viddispname, pAMVSCCap, pAMXBar, pAMTuner);
                    }
                }
        */
        // TODO: init pAMXBar

        if (pAMTuner) { // load saved channel
            pAMTuner->put_CountryCode(AfxGetApp()->GetProfileInt(_T("Capture"), _T("Country"), 1));

            int vchannel = pODD->vchannel;
            if (vchannel < 0) {
                vchannel = AfxGetApp()->GetProfileInt(_T("Capture\\") + CString(m_VidDispName), _T("Channel"), -1);
            }
            if (vchannel >= 0) {
                OAFilterState fs = State_Stopped;
                pMC->GetState(0, &fs);
                if (fs == State_Running) {
                    pMC->Pause();
                }
                pAMTuner->put_Channel(vchannel, AMTUNER_SUBCHAN_DEFAULT, AMTUNER_SUBCHAN_DEFAULT);
                if (fs == State_Running) {
                    pMC->Run();
                }
            }
        }
    }

    if (pAudCapTmp) {
        if (FAILED(hr = pGB->AddFilter(pAudCapTmp, CStringW(audfrname)))) {
            throw(UINT)IDS_CAPTURE_ERROR_AUD_FILTER;
        }

        pAudCap = pAudCapTmp;

        if (FAILED(pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Audio, pAudCap, IID_IAMStreamConfig, (void**)&pAMASC))
                && FAILED(pCGB->FindInterface(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Audio, pAudCap, IID_IAMStreamConfig, (void**)&pAMASC))) {
            TRACE(_T("Warning: No IAMStreamConfig interface for vidcap"));
        }
        /*
        CInterfaceArray<IAMAudioInputMixer> pAMAIM;

        BeginEnumPins(pAudCap, pEP, pPin)
        {
            PIN_DIRECTION dir;
            if (FAILED(pPin->QueryDirection(&dir)) || dir != PINDIR_INPUT)
                continue;

            if (CComQIPtr<IAMAudioInputMixer> pAIM = pPin)
                pAMAIM.Add(pAIM);
        }
        EndEnumPins;

        if (pAMASC)
        {
            m_wndCaptureBar.m_capdlg.SetupAudioControls(auddispname, pAMASC, pAMAIM);
        }
        */
    }

    if (!(pVidCap || pAudCap)) {
        throw(UINT)IDS_MAINFRM_108;
    }

    pODD->title.LoadString(IDS_CAPTURE_LIVE);

    SetPlaybackMode(PM_CAPTURE);
}

void CMainFrame::OpenCustomizeGraph()
{
    if (GetPlaybackMode() == PM_CAPTURE) {
        return;
    }

    CleanGraph();

    if (GetPlaybackMode() == PM_FILE) {
        if (m_pCAP && AfxGetAppSettings().fAutoloadSubtitles) {
            AddTextPassThruFilter();
        }
    }

    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    if (r.m_AdvRendSets.bSynchronizeVideo && s.iDSVideoRendererType == VIDRNDT_DS_SYNC) {
        HRESULT hr;
        m_pRefClock = DEBUG_NEW CSyncClockFilter(NULL, &hr);
        CStringW name;
        name = L"SyncClock Filter";
        pGB->AddFilter(m_pRefClock, name);

        CComPtr<IReferenceClock> refClock;
        m_pRefClock->QueryInterface(IID_IReferenceClock, reinterpret_cast<void**>(&refClock));
        CComPtr<IMediaFilter> mediaFilter;
        pGB->QueryInterface(IID_IMediaFilter, reinterpret_cast<void**>(&mediaFilter));
        mediaFilter->SetSyncSource(refClock);
        mediaFilter = NULL;
        refClock = NULL;

        m_pRefClock->QueryInterface(IID_ISyncClock, reinterpret_cast<void**>(&m_pSyncClock));

        CComQIPtr<ISyncClockAdviser> pAdviser = m_pCAP;
        if (pAdviser) {
            pAdviser->AdviseSyncClock(m_pSyncClock);
        }
    }

    if (GetPlaybackMode() == PM_DVD) {
        BeginEnumFilters(pGB, pEF, pBF) {
            if (CComQIPtr<IDirectVobSub2> pDVS2 = pBF) {
                //pDVS2->AdviseSubClock(m_pSubClock = DEBUG_NEW CSubClock);
                //break;

                // TODO: test multiple dvobsub instances with one clock
                if (!m_pSubClock) {
                    m_pSubClock = DEBUG_NEW CSubClock;
                }
                pDVS2->AdviseSubClock(m_pSubClock);
            }
        }
        EndEnumFilters;
    }

    BeginEnumFilters(pGB, pEF, pBF) {
        if (GetCLSID(pBF) == CLSID_OggSplitter) {
            if (CComQIPtr<IAMStreamSelect> pSS = pBF) {
                LCID idAudio = s.idAudioLang;
                if (!idAudio) {
                    idAudio = GetUserDefaultLCID();
                }
                LCID idSub = s.idSubtitlesLang;
                if (!idSub) {
                    idSub = GetUserDefaultLCID();
                }

                DWORD cnt = 0;
                pSS->Count(&cnt);
                for (DWORD i = 0; i < cnt; i++) {
                    AM_MEDIA_TYPE* pmt = NULL;
                    DWORD dwFlags = 0;
                    LCID lcid = 0;
                    DWORD dwGroup = 0;
                    WCHAR* pszName = NULL;
                    if (SUCCEEDED(pSS->Info((long)i, &pmt, &dwFlags, &lcid, &dwGroup, &pszName, NULL, NULL))) {
                        CStringW name(pszName), sound(ResStr(IDS_AG_SOUND)), subtitle(L"Subtitle");

                        if (idAudio != (LCID) - 1 && (idAudio & 0x3ff) == (lcid & 0x3ff) // sublang seems to be zeroed out in ogm...
                                && name.GetLength() > sound.GetLength()
                                && !name.Left(sound.GetLength()).CompareNoCase(sound)) {
                            if (SUCCEEDED(pSS->Enable(i, AMSTREAMSELECTENABLE_ENABLE))) {
                                idAudio = (LCID) - 1;
                            }
                        }

                        if (idSub != (LCID) - 1 && (idSub & 0x3ff) == (lcid & 0x3ff) // sublang seems to be zeroed out in ogm...
                                && name.GetLength() > subtitle.GetLength()
                                && !name.Left(subtitle.GetLength()).CompareNoCase(subtitle)
                                && name.Mid(subtitle.GetLength()).Trim().CompareNoCase(L"off")) {
                            if (SUCCEEDED(pSS->Enable(i, AMSTREAMSELECTENABLE_ENABLE))) {
                                idSub = (LCID) - 1;
                            }
                        }

                        if (pmt) {
                            DeleteMediaType(pmt);
                        }
                        if (pszName) {
                            CoTaskMemFree(pszName);
                        }
                    }
                }
            }
        }
    }
    EndEnumFilters;

    CleanGraph();
}

void CMainFrame::OpenSetupVideo()
{
    m_fAudioOnly = true;

    if (m_pMFVDC) { // EVR
        m_fAudioOnly = false;
    } else if (m_pCAP) {
        CSize vs = m_pCAP->GetVideoSize();
        m_fAudioOnly = (vs.cx <= 0 || vs.cy <= 0);
    } else {
        {
            long w = 0, h = 0;

            if (CComQIPtr<IBasicVideo> pBV = pGB) {
                pBV->GetVideoSize(&w, &h);
            }

            if (w > 0 && h > 0) {
                m_fAudioOnly = false;
            }
        }

        if (m_fAudioOnly) {
            BeginEnumFilters(pGB, pEF, pBF) {
                long w = 0, h = 0;

                if (CComQIPtr<IVideoWindow> pVW = pBF) {
                    long lVisible;
                    if (FAILED(pVW->get_Visible(&lVisible))) {
                        continue;
                    }

                    pVW->get_Width(&w);
                    pVW->get_Height(&h);
                }

                if (w > 0 && h > 0) {
                    m_fAudioOnly = false;
                    break;
                }
            }
            EndEnumFilters;
        }
    }

    if (m_fShockwaveGraph) {
        m_fAudioOnly = false;
    }

    if (m_pCAP) {
        SetShaders();
    }
    // else
    {
        // TESTME

        pVW->put_Owner((OAHWND)m_pVideoWnd->m_hWnd);
        pVW->put_WindowStyle(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
        pVW->put_MessageDrain((OAHWND)m_hWnd);

        for (CWnd* pWnd = m_wndView.GetWindow(GW_CHILD); pWnd; pWnd = pWnd->GetNextWindow()) {
            pWnd->EnableWindow(FALSE);    // little trick to let WM_SETCURSOR thru
        }
    }

    if (m_fAudioOnly && m_pFullscreenWnd->IsWindow()) {
        m_pFullscreenWnd->DestroyWindow();
    }
}

void CMainFrame::OpenSetupAudio()
{
    pBA->put_Volume(m_wndToolBar.Volume);

    // FIXME
    int balance = AfxGetAppSettings().nBalance;

    int sign = balance > 0 ? -1 : 1; // -1: invert sign for more right channel
    if (balance > -100 && balance < 100) {
        balance = sign * (int)(100 * 20 * log10(1 - abs(balance) / 100.0f));
    } else {
        balance = sign * (-10000);  // -10000: only left, 10000: only right
    }

    pBA->put_Balance(balance);
}
/*
void CMainFrame::OpenSetupToolBar()
{
//  m_wndToolBar.Volume = AfxGetAppSettings().nVolume;
//  SetBalance(AfxGetAppSettings().nBalance);
}
*/
void CMainFrame::OpenSetupCaptureBar()
{
    if (GetPlaybackMode() == PM_CAPTURE) {
        if (pVidCap && pAMVSCCap) {
            CComQIPtr<IAMVfwCaptureDialogs> pVfwCD = pVidCap;

            if (!pAMXBar && pVfwCD) {
                m_wndCaptureBar.m_capdlg.SetupVideoControls(m_VidDispName, pAMVSCCap, pVfwCD);
            } else {
                m_wndCaptureBar.m_capdlg.SetupVideoControls(m_VidDispName, pAMVSCCap, pAMXBar, pAMTuner);
            }
        }

        if (pAudCap && pAMASC) {
            CInterfaceArray<IAMAudioInputMixer> pAMAIM;

            BeginEnumPins(pAudCap, pEP, pPin) {
                if (CComQIPtr<IAMAudioInputMixer> pAIM = pPin) {
                    pAMAIM.Add(pAIM);
                }
            }
            EndEnumPins;

            m_wndCaptureBar.m_capdlg.SetupAudioControls(m_AudDispName, pAMASC, pAMAIM);
        }
    }

    BuildGraphVideoAudio(
        m_wndCaptureBar.m_capdlg.m_fVidPreview, false,
        m_wndCaptureBar.m_capdlg.m_fAudPreview, false);
}

void CMainFrame::OpenSetupInfoBar()
{
    if (GetPlaybackMode() == PM_FILE) {
        bool fEmpty = true;
        BeginEnumFilters(pGB, pEF, pBF) {
            if (CComQIPtr<IAMMediaContent, &IID_IAMMediaContent> pAMMC = pBF) {
                CComBSTR bstr;
                if (SUCCEEDED(pAMMC->get_Title(&bstr))) {
                    m_wndInfoBar.SetLine(ResStr(IDS_INFOBAR_TITLE), bstr.m_str);
                    if (bstr.Length()) {
                        fEmpty = false;
                    }
                }
                bstr.Empty();
                if (SUCCEEDED(pAMMC->get_AuthorName(&bstr))) {
                    m_wndInfoBar.SetLine(ResStr(IDS_INFOBAR_AUTHOR), bstr.m_str);
                    if (bstr.Length()) {
                        fEmpty = false;
                    }
                }
                bstr.Empty();
                if (SUCCEEDED(pAMMC->get_Copyright(&bstr))) {
                    m_wndInfoBar.SetLine(ResStr(IDS_INFOBAR_COPYRIGHT), bstr.m_str);
                    if (bstr.Length()) {
                        fEmpty = false;
                    }
                }
                bstr.Empty();
                if (SUCCEEDED(pAMMC->get_Rating(&bstr))) {
                    m_wndInfoBar.SetLine(ResStr(IDS_INFOBAR_RATING), bstr.m_str);
                    if (bstr.Length()) {
                        fEmpty = false;
                    }
                }
                bstr.Empty();
                if (SUCCEEDED(pAMMC->get_Description(&bstr))) {
                    m_wndInfoBar.SetLine(ResStr(IDS_INFOBAR_DESCRIPTION), bstr.m_str);
                    if (bstr.Length()) {
                        fEmpty = false;
                    }
                }
                bstr.Empty();
                if (!fEmpty) {
                    RecalcLayout();
                    break;
                }
            }
        }
        EndEnumFilters;
    } else if (GetPlaybackMode() == PM_DVD) {
        CString info('-');
        m_wndInfoBar.SetLine(ResStr(IDS_INFOBAR_DOMAIN), info);
        m_wndInfoBar.SetLine(ResStr(IDS_INFOBAR_LOCATION), info);
        m_wndInfoBar.SetLine(ResStr(IDS_INFOBAR_VIDEO), info);
        m_wndInfoBar.SetLine(ResStr(IDS_INFOBAR_AUDIO), info);
        m_wndInfoBar.SetLine(ResStr(IDS_INFOBAR_SUBTITLES), info);
        RecalcLayout();
    }
}

void CMainFrame::OpenSetupStatsBar()
{
    CString info('-');

    BeginEnumFilters(pGB, pEF, pBF) {
        if (!pQP && (pQP = pBF)) {
            m_wndStatsBar.SetLine(ResStr(IDS_AG_FRAMERATE), info);
            m_wndStatsBar.SetLine(_T("Sync Offset"), info);
            m_wndStatsBar.SetLine(ResStr(IDS_AG_FRAMES), info);
            m_wndStatsBar.SetLine(_T("Jitter"), info);
            m_wndStatsBar.SetLine(ResStr(IDS_AG_BUFFERS), info);
            m_wndStatsBar.SetLine(_T("Bitrate"), info);
            RecalcLayout();
        }

        if (!pBI && (pBI = pBF)) {
            m_wndStatsBar.SetLine(ResStr(IDS_AG_BUFFERS), info);
            m_wndStatsBar.SetLine(_T("Bitrate"), info); // FIXME: shouldn't be here
            RecalcLayout();
        }
    }
    EndEnumFilters;
}

void CMainFrame::OpenSetupStatusBar()
{
    m_wndStatusBar.ShowTimer(true);

    //

    if (!m_fCustomGraph) {
        UINT id = IDB_AUDIOTYPE_NOAUDIO;

        BeginEnumFilters(pGB, pEF, pBF) {
            CComQIPtr<IBasicAudio> pBA = pBF;
            if (!pBA) {
                continue;
            }

            BeginEnumPins(pBF, pEP, pPin) {
                if (S_OK == pGB->IsPinDirection(pPin, PINDIR_INPUT)
                        && S_OK == pGB->IsPinConnected(pPin)) {
                    AM_MEDIA_TYPE mt;
                    memset(&mt, 0, sizeof(mt));
                    pPin->ConnectionMediaType(&mt);

                    if (mt.majortype == MEDIATYPE_Audio && mt.formattype == FORMAT_WaveFormatEx) {
                        switch (((WAVEFORMATEX*)mt.pbFormat)->nChannels) {
                            case 1:
                                id = IDB_AUDIOTYPE_MONO;
                                break;
                            case 2:
                            default:
                                id = IDB_AUDIOTYPE_STEREO;
                                break;
                        }
                        break;
                    } else if (mt.majortype == MEDIATYPE_Midi) {
                        id = NULL;
                        break;
                    }
                }
            }
            EndEnumPins;

            if (id != IDB_AUDIOTYPE_NOAUDIO) {
                break;
            }
        }
        EndEnumFilters;

        m_wndStatusBar.SetStatusBitmap(id);
    }

    //

    HICON hIcon = NULL;

    if (GetPlaybackMode() == PM_FILE) {
        CString fn = m_wndPlaylistBar.GetCurFileName();
        CString ext = fn.Mid(fn.ReverseFind('.') + 1);
        hIcon = LoadIcon(ext, true);
    } else if (GetPlaybackMode() == PM_DVD) {
        hIcon = LoadIcon(_T(".ifo"), true);
    } else if (GetPlaybackMode() == PM_DVD) {
        //hIcon = ; // TODO
    }

    m_wndStatusBar.SetStatusTypeIcon(hIcon);
}

void CMainFrame::OpenSetupWindowTitle(CString fn)
{
    CString title = ResStr(IDR_MAINFRAME);

    const CAppSettings& s = AfxGetAppSettings();

    int i = s.iTitleBarTextStyle;

    if (!fn.IsEmpty() && (i == 0 || i == 1)) {
        if (i == 1) {
            if (GetPlaybackMode() == PM_FILE) {
                fn.Replace('\\', '/');
                CString fn2 = fn.Mid(fn.ReverseFind('/') + 1);
                if (!fn2.IsEmpty()) {
                    fn = fn2;
                }

                if (s.fTitleBarTextTitle) {
                    BeginEnumFilters(pGB, pEF, pBF) {
                        if (CComQIPtr<IAMMediaContent, &IID_IAMMediaContent> pAMMC = pBF) {
                            CComBSTR bstr;
                            if (SUCCEEDED(pAMMC->get_Title(&bstr)) && bstr.Length()) {
                                fn = CString(bstr.m_str);
                                break;
                            }
                        }
                    }
                    EndEnumFilters;
                }
            } else if (GetPlaybackMode() == PM_DVD) {
                fn = _T("DVD");
            } else if (GetPlaybackMode() == PM_CAPTURE) {
                fn.LoadString(IDS_CAPTURE_LIVE);
            }
        }
        title = fn;
    }

    SetWindowText(title);
    m_Lcd.SetMediaTitle(LPCTSTR(fn));
}

DWORD CMainFrame::SetupAudioStreams()
{
    if (m_iMediaLoadState != MLS_LOADED) {
        return 0;
    }

    CComQIPtr<IAMStreamSelect> pSS = FindFilter(__uuidof(CAudioSwitcherFilter), pGB);
    if (!pSS) {
        pSS = FindFilter(CLSID_MorganStreamSwitcher, pGB);
    }

    DWORD cStreams = 0;
    if (pSS && SUCCEEDED(pSS->Count(&cStreams)) && cStreams > 0) {
        CAtlArray<CString> langs;
        int tPos = 0;
        CString lang = AfxGetAppSettings().strAudiosLanguageOrder.Tokenize(_T(",; "), tPos);
        while (tPos != -1) {
            langs.Add(lang.MakeLower());
            lang = AfxGetAppSettings().strAudiosLanguageOrder.Tokenize(_T(",; "), tPos);
        }

        DWORD selected = 1;
        int  maxrating = 0;
        for (DWORD i = 0; i < cStreams; i++) {
            WCHAR* pName = NULL;
            if (FAILED(pSS->Info(i, NULL, NULL, NULL, NULL, &pName, NULL, NULL))) {
                continue;
            }
            CString name(pName);
            CoTaskMemFree(pName);
            name.Trim();
            name.MakeLower();

            int rating = 0;
            for (size_t j = 0; j < langs.GetCount(); j++) {
                int num = _tstoi(langs[j]) - 1;
                if (num >= 0) { // this is track number
                    if (i != num) {
                        continue;  // not matched
                    }
                } else { // this is lang string
                    int len = langs[j].GetLength();
                    if (name.Left(len) != langs[j] && name.Find(_T("[") + langs[j]) < 0) {
                        continue; // not matched
                    }
                }
                rating = 16 * int(langs.GetCount() - j);
                break;
            }
            if (name.Find(_T("[default,forced]")) != -1) { // for LAV Splitter
                rating += 4 + 2;
            }
            if (name.Find(_T("[forced]")) != -1) {
                rating += 4;
            }
            if (name.Find(_T("[default]")) != -1) {
                rating += 2;
            }
            if (i == 0) {
                rating += 1;
            }

            if (rating > maxrating) {
                maxrating = rating;
                selected = i;
            }
        }
        return selected + 1;
    }

    return 0;
}

DWORD CMainFrame::SetupSubtitleStreams()
{
    size_t cStreams = m_pSubStreams.GetCount();
    if (cStreams > 0) {
        bool externalPriority = false;
        CAtlArray<CString> langs;
        int tPos = 0;
        CString lang = AfxGetAppSettings().strSubtitlesLanguageOrder.Tokenize(_T(",; "), tPos);
        while (tPos != -1) {
            langs.Add(lang.MakeLower());
            lang = AfxGetAppSettings().strSubtitlesLanguageOrder.Tokenize(_T(",; "), tPos);
        }

        DWORD selected = 1;
        DWORD i = 0;
        int  maxrating = 0;
        POSITION pos = m_pSubStreams.GetHeadPosition();
        while (pos) {
            if (m_posFirstExtSub == pos) {
                externalPriority = AfxGetAppSettings().fPrioritizeExternalSubtitles;
            }
            SubtitleInput& subElement = m_pSubStreams.GetNext(pos);
            CComPtr<ISubStream> pSubStream = subElement.subStream;
            CComQIPtr<IAMStreamSelect> pSSF = subElement.sourceFilter;

            int count = 0;
            if (pSSF) {
                DWORD cStreams;
                if (SUCCEEDED(pSSF->Count(&cStreams))) {
                    count = (int)cStreams;
                }
            } else {
                count = pSubStream->GetStreamCount();
            }

            for (int j = 0; j < count; j++) {
                WCHAR* pName;
                HRESULT hr;
                DWORD dwGroup = 2;
                if (pSSF) {
                    hr = pSSF->Info(j, NULL, NULL, NULL, &dwGroup, &pName, NULL, NULL);
                    if (dwGroup != 2) {
                        CoTaskMemFree(pName);
                        continue;
                    }
                } else {
                    hr = pSubStream->GetStreamInfo(j, &pName, NULL);
                }
                CString name(pName);
                CoTaskMemFree(pName);
                name.Trim();
                name.MakeLower();

                int rating = 0;
                for (size_t j = 0; j < langs.GetCount(); j++) {
                    int num = _tstoi(langs[j]) - 1;
                    if (num >= 0) { // this is track number
                        if (i != num) {
                            continue;  // not matched
                        }
                    } else { // this is lang string
                        int len = langs[j].GetLength();
                        if (name.Left(len) != langs[j] && name.Find(_T("[") + langs[j]) < 0) {
                            continue; // not matched
                        }
                    }
                    rating = 16 * int(langs.GetCount() - j);
                    break;
                }
                if (externalPriority) {
                    rating += 8;
                }
                if (name.Find(_T("[default,forced]")) != -1) { // for LAV Splitter
                    rating += 4 + 2;
                }
                if (name.Find(_T("[forced]")) != -1) {
                    rating += 4;
                }
                if (name.Find(_T("[default]")) != -1) {
                    rating += 2;
                }
                if (i == 0) {
                    rating += 1;
                }

                if (rating > maxrating) {
                    maxrating = rating;
                    selected = i;
                }
                i++;
            }
        }
        return selected + 1;
    }

    return 0;
}

bool CMainFrame::OpenMediaPrivate(CAutoPtr<OpenMediaData> pOMD)
{
    CAppSettings& s = AfxGetAppSettings();

    if (m_iMediaLoadState != MLS_CLOSED) {
        ASSERT(0);
        return false;
    }

    OpenFileData* pFileData = dynamic_cast<OpenFileData*>(pOMD.m_p);
    OpenDVDData* pDVDData = dynamic_cast<OpenDVDData*>(pOMD.m_p);
    OpenDeviceData* pDeviceData = dynamic_cast<OpenDeviceData*>(pOMD.m_p);
    if (!pFileData && !pDVDData  && !pDeviceData) {
        ASSERT(0);
        return false;
    }

    // Clear DXVA state ...
    ClearDXVAState();

#ifdef _DEBUG
    // Debug trace code - Begin
    // Check for bad / buggy auto loading file code
    if (pFileData) {
        POSITION pos = pFileData->fns.GetHeadPosition();
        UINT index = 0;
        while (pos != NULL) {
            CString path = pFileData->fns.GetNext(pos);
            TRACE(_T("--> CMainFrame::OpenMediaPrivate - pFileData->fns[%d]:\n"), index);
            TRACE(_T("\t%ws\n"), path.GetString()); // %ws - wide character string always
            index++;
        }
    }
    // Debug trace code - End
#endif

    CString mi_fn = _T("");

    if (pFileData) {
        if (pFileData->fns.IsEmpty()) {
            return false;
        }

        CString fn = pFileData->fns.GetHead();

        int i = fn.Find(_T(":\\"));
        if (i > 0) {
            CString drive = fn.Left(i + 2);
            UINT type = GetDriveType(drive);
            CAtlList<CString> sl;
            if (type == DRIVE_REMOVABLE || type == DRIVE_CDROM && GetCDROMType(drive[0], sl) != CDROM_Audio) {
                int ret = IDRETRY;
                while (ret == IDRETRY) {
                    WIN32_FIND_DATA findFileData;
                    HANDLE h = FindFirstFile(fn, &findFileData);
                    if (h != INVALID_HANDLE_VALUE) {
                        FindClose(h);
                        ret = IDOK;
                    } else {
                        CString msg;
                        msg.Format(IDS_MAINFRM_114, fn);
                        ret = AfxMessageBox(msg, MB_RETRYCANCEL);
                    }
                }

                if (ret != IDOK) {
                    return false;
                }
            }
            mi_fn = fn;
        }
    }

    if (s.AutoChangeFullscrRes.bEnabled && s.IsD3DFullscreen()) {
        // DVD
        if (pDVDData) {
            mi_fn = pDVDData->path;
            CPath path(mi_fn);
            CString ext = path.GetExtension();
            if (ext == _T("")) {
                if (mi_fn.Right(10) == _T("\\VIDEO_TS\\")) {
                    mi_fn = mi_fn + _T("VTS_01_1.VOB");
                } else {
                    mi_fn = mi_fn + _T("\\VIDEO_TS\\VTS_01_1.VOB");
                }
            } else if (ext == _T(".IFO")) {
                path.RemoveFileSpec();
                mi_fn = path + _T("\\VTS_01_1.VOB");
            }
        } else {
            CPath path(mi_fn);
            CString ext = path.GetExtension();
            // BD
            if (ext == _T(".mpls")) {
                CHdmvClipInfo ClipInfo;
                CAtlList<CHdmvClipInfo::PlaylistItem> CurPlaylist;
                CHdmvClipInfo::PlaylistItem Item;
                REFERENCE_TIME rtDuration;
                if (SUCCEEDED(ClipInfo.ReadPlaylist(mi_fn, rtDuration, CurPlaylist))) {
                    Item = CurPlaylist.GetHead();
                    mi_fn = Item.m_strFileName;
                }
            }
        }

        // Get FPS
        miFPS = 0.0;

#ifdef USE_MEDIAINFO_STATIC
        MediaInfoLib::MediaInfo MI;
#else
        MediaInfo MI;
#endif

        if (MI.Open(mi_fn.GetString())) {
            CString strFPS =  MI.Get(Stream_Video, 0, _T("FrameRate"), Info_Text, Info_Name).c_str();

            // 3:2 pulldown ???
            CString strST = MI.Get(Stream_Video, 0, _T("ScanType"), Info_Text, Info_Name).c_str();
            CString strSO = MI.Get(Stream_Video, 0, _T("ScanOrder"), Info_Text, Info_Name).c_str();

            if (strFPS == _T("29.970") && (strSO == _T("2:3 Pulldown")
                                           || strST == _T("Progressive") && (strSO == _T("TFF")
                                                   || strSO  == _T("BFF")
                                                   || strSO  == _T("2:3 Pulldown")))) {

                strFPS = _T("23.976");
            }
            miFPS = wcstod(strFPS, NULL);

            AutoChangeMonitorMode();
        }
    }

    SetLoadState(MLS_LOADING);

    // FIXME: Don't show "Closed" initially
    PostMessage(WM_KICKIDLE);

    CString err;

    m_fUpdateInfoBar = false;
    BeginWaitCursor();

    try {
        CComPtr<IVMRMixerBitmap9>    pVMB;
        CComPtr<IMFVideoMixerBitmap> pMFVMB;
        CComPtr<IMadVRTextOsd>       pMVTO;
        if (m_fOpeningAborted) {
            throw(UINT)IDS_AG_ABORTED;
        }

        OpenCreateGraphObject(pOMD);

        if (m_fOpeningAborted) {
            throw(UINT)IDS_AG_ABORTED;
        }

        SetupIViAudReg();

        if (m_fOpeningAborted) {
            throw(UINT)IDS_AG_ABORTED;
        }

        if (pFileData) {
            OpenFile(pFileData);
        } else if (pDVDData) {
            OpenDVD(pDVDData);
        } else if (pDeviceData) {
            if (s.iDefaultCaptureDevice == 1) {
                HRESULT hr = OpenBDAGraph();
                if (FAILED(hr)) {
                    throw(UINT)IDS_CAPTURE_ERROR_DEVICE;
                }
            } else {
                OpenCapture(pDeviceData);
            }
        } else {
            throw(UINT)IDS_INVALID_PARAMS_ERROR;
        }

        m_pCAP2 = NULL;
        m_pCAP = NULL;

        pGB->FindInterface(__uuidof(ISubPicAllocatorPresenter), (void**)&m_pCAP, TRUE);
        pGB->FindInterface(__uuidof(ISubPicAllocatorPresenter2), (void**)&m_pCAP2, TRUE);
        pGB->FindInterface(__uuidof(IVMRMixerControl9), (void**)&m_pMC, TRUE);
        pGB->FindInterface(__uuidof(IVMRMixerBitmap9), (void**)&pVMB, TRUE);
        pGB->FindInterface(__uuidof(IMFVideoMixerBitmap), (void**)&pMFVMB, TRUE);
        pMVTO = m_pCAP;

        if (s.fShowOSD || s.fShowDebugInfo) { // Force OSD on when the debug switch is used
            if (pVMB) {
                m_OSD.Start(m_pVideoWnd, pVMB, s.IsD3DFullscreen());
            } else if (pMFVMB) {
                m_OSD.Start(m_pVideoWnd, pMFVMB, s.IsD3DFullscreen());
            } else if (pMVTO) {
                m_OSD.Start(m_pVideoWnd, pMVTO);
            }
        }

        SetupVMR9ColorControl();

        // === EVR !
        pGB->FindInterface(__uuidof(IMFVideoDisplayControl), (void**)&m_pMFVDC,  TRUE);
        pGB->FindInterface(__uuidof(IMFVideoProcessor), (void**)&m_pMFVP, TRUE);
        if (m_pMFVDC) {
            m_pMFVDC->SetVideoWindow(m_pVideoWnd->m_hWnd);
        }

        //SetupEVRColorControl();
        //does not work at this location
        //need to choose the correct mode (IMFVideoProcessor::SetVideoProcessorMode)

        BeginEnumFilters(pGB, pEF, pBF) {
            if (m_pLN21 = pBF) {
                m_pLN21->SetServiceState(s.fClosedCaptions ? AM_L21_CCSTATE_On : AM_L21_CCSTATE_Off);
                break;
            }
        }
        EndEnumFilters;

        if (m_fOpeningAborted) {
            throw(UINT)IDS_AG_ABORTED;
        }

        OpenCustomizeGraph();

        if (m_fOpeningAborted) {
            throw(UINT)IDS_AG_ABORTED;
        }

        OpenSetupVideo();

        if (m_fOpeningAborted) {
            throw(UINT)IDS_AG_ABORTED;
        }

        OpenSetupAudio();

        if (m_fOpeningAborted) {
            throw(UINT)IDS_AG_ABORTED;
        }

        if (m_pCAP && (!m_fAudioOnly || m_fRealMediaGraph)) {

            if (s.fDisableInternalSubtitles) {
                m_pSubStreams.RemoveAll(); // Needs to be replaced with code that checks for forced subtitles.
            }

            m_posFirstExtSub = NULL;
            POSITION pos = pOMD->subs.GetHeadPosition();
            while (pos) {
                LoadSubtitle(pOMD->subs.GetNext(pos));
            }
        }

        if (m_fOpeningAborted) {
            throw(UINT)IDS_AG_ABORTED;
        }

        OpenSetupWindowTitle(pOMD->title);

        if (s.fEnableEDLEditor) {
            m_wndEditListEditor.OpenFile(pOMD->title);
        }

        if (pFileData) {
            m_OpenFile = true;
        }

        if (::GetCurrentThreadId() == AfxGetApp()->m_nThreadID) {
            OnFilePostOpenmedia();
        } else {
            PostMessage(WM_COMMAND, ID_FILE_POST_OPENMEDIA);
        }

        while (m_iMediaLoadState != MLS_LOADED
                && m_iMediaLoadState != MLS_CLOSING // FIXME
              ) {
            Sleep(50);
        }

        DWORD audstm = SetupAudioStreams();
        DWORD substm = SetupSubtitleStreams();

        // PostMessage instead of SendMessage because the user might call CloseMedia and then we would deadlock

        PostMessage(WM_COMMAND, ID_PLAY_PAUSE);

        b_firstPlay = true;

        if (!(AfxGetAppSettings().nCLSwitches & CLSW_OPEN) && (AfxGetAppSettings().nLoops > 0)) {
            PostMessage(WM_COMMAND, ID_PLAY_PLAY);
        } else {
            // If we don't start playing immediately, we need to initialize
            // the seekbar and the time counter.
            OnTimer(TIMER_STREAMPOSPOLLER);
            OnTimer(TIMER_STREAMPOSPOLLER2);
        }

        // Casimir666 : audio selection should be done before running the graph to prevent an
        // unnecessary seek when a file is opened (PostMessage ID_AUDIO_SUBITEM_START removed)

        if (audstm) {
            OnPlayAudio(ID_AUDIO_SUBITEM_START + audstm);
        }
        if (substm) {
            m_iSubtitleSel = substm - 1; // only select, turn on/off in another place

            if (!AfxGetAppSettings().fEnableSubtitles) {
                m_iSubtitleSel |= 0x80000000;
            } else {
                UpdateSubtitle();
            }
        }

        AfxGetAppSettings().nCLSwitches &= ~CLSW_OPEN;

        if (pFileData) {
            if (pFileData->rtStart > 0) {
                PostMessage(WM_RESUMEFROMSTATE, (WPARAM)PM_FILE, (LPARAM)(pFileData->rtStart / 10000));  // REFERENCE_TIME doesn't fit in LPARAM under a 32bit env.
            }
        } else if (pDVDData) {
            if (pDVDData->pDvdState) {
                PostMessage(WM_RESUMEFROMSTATE, (WPARAM)PM_DVD, (LPARAM)(CComPtr<IDvdState>(pDVDData->pDvdState).Detach()));    // must be released by the called message handler
            }
        } else if (pDeviceData) {
            m_wndCaptureBar.m_capdlg.SetVideoInput(pDeviceData->vinput);
            m_wndCaptureBar.m_capdlg.SetVideoChannel(pDeviceData->vchannel);
            m_wndCaptureBar.m_capdlg.SetAudioInput(pDeviceData->ainput);
        }
    } catch (LPCTSTR msg) {
        err = msg;
    } catch (CString& msg) {
        err = msg;
    } catch (UINT msg) {
        err.LoadString(msg);
    }

    EndWaitCursor();

    if (!err.IsEmpty()) {
        CloseMediaPrivate();
        m_closingmsg = err;

        if (err != ResStr(IDS_AG_ABORTED)) {
            if (pFileData) {
                m_wndPlaylistBar.SetCurValid(false);

                if (m_wndPlaylistBar.IsAtEnd()) {
                    m_nLoops++;
                }

                if (s.fLoopForever || m_nLoops < s.nLoops) {
                    bool hasValidFile = false;

                    if (flast_nID == ID_NAVIGATE_SKIPBACK) {
                        hasValidFile = m_wndPlaylistBar.SetPrev();
                    } else {
                        hasValidFile = m_wndPlaylistBar.SetNext();
                    }

                    if (hasValidFile) {
                        OpenCurPlaylistItem();
                    }
                } else if (m_wndPlaylistBar.GetCount() > 1) {
                    DoAfterPlaybackEvent();
                }
            } else {
                OnNavigateSkip(ID_NAVIGATE_SKIPFORWARD);
            }
        }
    } else {
        m_wndPlaylistBar.SetCurValid(true);

        // Apply command line audio shift
        if (s.rtShift != 0) {
            SetAudioDelay(s.rtShift);
            s.rtShift = 0;
        }
    }

    flast_nID = 0;

    if (AfxGetAppSettings().AutoChangeFullscrRes.bEnabled && m_fFullScreen) {
        AutoChangeMonitorMode();
    }
    if (m_fFullScreen && AfxGetAppSettings().fRememberZoomLevel) {
        m_fFirstFSAfterLaunchOnFS = true;
    }

    m_LastOpenFile = pOMD->title;

    PostMessage(WM_KICKIDLE); // calls main thread to update things

    if (!m_bIsBDPlay) {
        m_MPLSPlaylist.RemoveAll();
        m_LastOpenBDPath = _T("");
    }
    m_bIsBDPlay = false;

    return err.IsEmpty();
}

void CMainFrame::CloseMediaPrivate()
{
    SetLoadState(MLS_CLOSING); // why it before OnPlayStop()? // TODO: remake or add detailed comments
    OnPlayStop(); // SendMessage(WM_COMMAND, ID_PLAY_STOP);
    if (pMC) {
        pMC->Stop(); // needed for StreamBufferSource, because m_iMediaLoadState is always MLS_CLOSED // TODO: fix the opening for such media
    }
    SetPlaybackMode(PM_NONE);
    m_fLiveWM = false;
    m_fEndOfStream = false;
    m_rtDurationOverride = -1;
    m_kfs.RemoveAll();
    m_pCB = NULL;

    //if (pVW) pVW->put_Visible(OAFALSE);
    //if (pVW) pVW->put_MessageDrain((OAHWND)NULL), pVW->put_Owner((OAHWND)NULL);

    m_pCAP = NULL; // IMPORTANT: IVMRSurfaceAllocatorNotify/IVMRSurfaceAllocatorNotify9 has to be released before the VMR/VMR9, otherwise it will crash in Release()
    m_pCAP2 = NULL;
    m_pMC = NULL;
    m_pMFVP = NULL;
    m_pMFVDC = NULL;
    m_pLN21 = NULL;
    m_pSyncClock = NULL;
    m_OSD.Stop();

    pAMXBar.Release();
    pAMTuner.Release();
    pAMDF.Release();
    pAMVCCap.Release();
    pAMVCPrev.Release();
    pAMVSCCap.Release();
    pAMVSCPrev.Release();
    pAMASC.Release();
    pVidCap.Release();
    pAudCap.Release();
    pCGB.Release();
    pDVDC.Release();
    pDVDI.Release();
    pQP.Release();
    pBI.Release();
    pAMOP.Release();
    pFS.Release();
    pMC.Release();
    pME.Release();
    pMS.Release();
    pVW.Release();
    pBV.Release();
    pBA.Release();

    if (pGB) {
        pGB->RemoveFromROT();
        pGB.Release();
    }

    if (m_pFullscreenWnd->IsWindow()) {
        m_pFullscreenWnd->DestroyWindow();    // TODO : still freezing sometimes...
    }

    m_fRealMediaGraph = m_fShockwaveGraph = m_fQuicktimeGraph = false;

    m_pSubClock = NULL;

    m_pProv.Release();

    {
        CAutoLock cAutoLock(&m_csSubLock);
        m_pSubStreams.RemoveAll();
    }

    m_VidDispName.Empty();
    m_AudDispName.Empty();

    m_closingmsg.LoadString(IDS_CONTROLS_CLOSED);

    AfxGetAppSettings().nCLSwitches &= CLSW_OPEN | CLSW_PLAY | CLSW_AFTERPLAYBACK_MASK | CLSW_NOFOCUS;

    SetLoadState(MLS_CLOSED);
}

void CMainFrame::ParseDirs(CAtlList<CString>& sl)
{
    POSITION pos = sl.GetHeadPosition();

    while (pos) {
        CString fn = sl.GetNext(pos);
        WIN32_FIND_DATA fd = {0};
        HANDLE hFind = FindFirstFile(fn, &fd);

        if (hFind != INVALID_HANDLE_VALUE) {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (fn[fn.GetLength() - 1] != '\\') {
                    fn += '\\';
                }

                COpenDirHelper::RecurseAddDir(fn, &sl);
            }

            FindClose(hFind);
        }
    }
}

static bool SearchInDirCompare(const CString& str1, const CString& str2)
{
    return (StrCmpLogicalW(str1, str2) < 0);
}

bool CMainFrame::SearchInDir(bool bDirForward)
{
    CStringArray files;
    CMediaFormats& mf = AfxGetAppSettings().m_Formats;
    CString mask = m_LastOpenFile.Left(m_LastOpenFile.ReverseFind(_T('\\')) + 1) + _T("*.*");
    CFileFind finder;
    BOOL bHasNext = finder.FindFile(mask);

    while (bHasNext) {
        bHasNext = finder.FindNextFile();

        if (!finder.IsDirectory()) {
            CString path = finder.GetFilePath();
            CString ext = path.Mid(path.ReverseFind('.'));
            if (mf.FindExt(ext)) {
                files.Add(path);
            }
        }
    }

    finder.Close();

    if (files.GetCount() <= 1) {
        return false;
    }

    std::sort(files.GetData(), files.GetData() + files.GetCount(), SearchInDirCompare);

    INT_PTR current = -1;
    for (INT_PTR i = 0, l = files.GetCount(); i < l && current < 0; i++) {
        if (m_LastOpenFile.CompareNoCase(files[i]) == 0) {
            current = i;
        }
    }

    if (current < 0) {
        return false;
    }

    CAtlList<CString> sl;

    if (bDirForward) {
        current++;
        if (current >= files.GetCount()) {
            return false;
        }
    } else {
        current--;
        if (current < 0) {
            return false;
        }
    }

    sl.AddHead(files[current]);
    m_wndPlaylistBar.Open(sl, false);
    OpenCurPlaylistItem();

    return true;
}

void CMainFrame::DoTunerScan(TunerScanData* pTSD)
{
    if (GetPlaybackMode() == PM_CAPTURE) {
        CComQIPtr<IBDATuner> pTun = pGB;
        if (pTun) {
            BOOLEAN bPresent;
            BOOLEAN bLocked;
            LONG lStrength;
            LONG lQuality;
            int nProgress;
            int nOffset = pTSD->Offset ? 3 : 1;
            LONG lOffsets[3] = {0, pTSD->Offset, -pTSD->Offset};
            bool bSucceeded;
            m_bStopTunerScan = false;

            for (ULONG ulFrequency = pTSD->FrequencyStart; ulFrequency <= pTSD->FrequencyStop; ulFrequency += pTSD->Bandwidth) {
                bSucceeded = false;
                for (int nOffsetPos = 0; nOffsetPos < nOffset && !bSucceeded; nOffsetPos++) {
                    pTun->SetFrequency(ulFrequency + lOffsets[nOffsetPos]);
                    Sleep(200);
                    if (SUCCEEDED(pTun->GetStats(bPresent, bLocked, lStrength, lQuality)) && bPresent) {
                        ::SendMessage(pTSD->Hwnd, WM_TUNER_STATS, lStrength, lQuality);
                        pTun->Scan(ulFrequency + lOffsets[nOffsetPos], pTSD->Hwnd);
                        bSucceeded = true;
                    }
                }

                nProgress = MulDiv(ulFrequency - pTSD->FrequencyStart, 100, pTSD->FrequencyStop - pTSD->FrequencyStart);
                ::SendMessage(pTSD->Hwnd, WM_TUNER_SCAN_PROGRESS, nProgress, 0);
                ::SendMessage(pTSD->Hwnd, WM_TUNER_STATS, lStrength, lQuality);

                if (m_bStopTunerScan) {
                    break;
                }
            }

            ::SendMessage(pTSD->Hwnd, WM_TUNER_SCAN_END, 0, 0);
        }
    }
}

// msn

void CMainFrame::SendNowPlayingToMSN()
{
    if (!AfxGetAppSettings().fNotifyMSN) {
        return;
    }

    CString title, author;

    if (m_iMediaLoadState == MLS_LOADED) {
        m_wndInfoBar.GetLine(ResStr(IDS_INFOBAR_TITLE), title);
        m_wndInfoBar.GetLine(ResStr(IDS_INFOBAR_AUTHOR), author);

        if (title.IsEmpty()) {
            CPlaylistItem pli;
            m_wndPlaylistBar.GetCur(pli);

            if (!pli.m_fns.IsEmpty()) {
                CString label = !pli.m_label.IsEmpty() ? pli.m_label : pli.m_fns.GetHead();

                if (GetPlaybackMode() == PM_FILE) {
                    CString fn = label;
                    if (fn.Find(_T("://")) >= 0) {
                        int i = fn.Find('?');
                        if (i >= 0) {
                            fn = fn.Left(i);
                        }
                    }
                    CPath path(fn);
                    path.StripPath();
                    path.MakePretty();
                    path.RemoveExtension();
                    title = (LPCTSTR)path;
                    author.Empty();
                } else if (GetPlaybackMode() == PM_CAPTURE) {
                    title = label != pli.m_fns.GetHead() ? label : ResStr(IDS_CAPTURE_LIVE);
                    author.Empty();
                } else if (GetPlaybackMode() == PM_DVD) {
                    title = _T("DVD");
                    author.Empty();
                }
            }
        }
    }

    CStringW buff;
    buff += L"\\0Music\\0";
    buff += title.IsEmpty() ? L"0" : L"1";
    buff += L"\\0";
    buff += author.IsEmpty() ? L"{0}" : L"{0} - {1}";
    buff += L"\\0";
    if (!author.IsEmpty()) {
        buff += CStringW(author) + L"\\0";
    }
    buff += CStringW(title) + L"\\0";
    buff += L"\\0\\0";

    COPYDATASTRUCT data;
    data.dwData = 0x0547;
    data.lpData = (PVOID)(LPCWSTR)buff;
    data.cbData = buff.GetLength() * 2 + 2;

    HWND hWnd = ::FindWindowEx(NULL, NULL, _T("MsnMsgrUIManager"), NULL);
    while (hWnd) {
        ::SendMessage(hWnd, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&data);
        hWnd = ::FindWindowEx(NULL, hWnd, _T("MsnMsgrUIManager"), NULL);
    }
}


// dynamic menus

void CMainFrame::SetupOpenCDSubMenu()
{
    CMenu* pSub = &m_opencds;

    if (!IsMenu(pSub->m_hMenu)) {
        pSub->CreatePopupMenu();
    } else while (pSub->RemoveMenu(0, MF_BYPOSITION)) {
            ;
        }

    if (m_iMediaLoadState == MLS_LOADING) {
        return;
    }

    if (AfxGetAppSettings().fHideCDROMsSubMenu) {
        return;
    }

    UINT id = ID_FILE_OPEN_CD_START;

    for (TCHAR drive = 'C'; drive <= 'Z'; drive++) {
        CString label = GetDriveLabel(drive), str;

        CAtlList<CString> files;
        switch (GetCDROMType(drive, files)) {
            case CDROM_Audio:
                if (label.IsEmpty()) {
                    label = _T("Audio CD");
                }
                str.Format(_T("%s (%c:)"), label, drive);
                break;
            case CDROM_VideoCD:
                if (label.IsEmpty()) {
                    label = _T("(S)VCD");
                }
                str.Format(_T("%s (%c:)"), label, drive);
                break;
            case CDROM_DVDVideo:
                if (label.IsEmpty()) {
                    label = _T("DVD Video");
                }
                str.Format(_T("%s (%c:)"), label, drive);
                break;
            default:
                break;
        }

        if (!str.IsEmpty()) {
            pSub->AppendMenu(MF_STRING | MF_ENABLED, id++, str);
        }
    }
}

void CMainFrame::SetupFiltersSubMenu()
{
    CMenu* pSub = &m_filters;

    if (!IsMenu(pSub->m_hMenu)) {
        pSub->CreatePopupMenu();
    } else while (pSub->RemoveMenu(0, MF_BYPOSITION)) {
            ;
        }

    m_filterpopups.RemoveAll();
    m_pparray.RemoveAll();
    m_ssarray.RemoveAll();

    if (m_iMediaLoadState == MLS_LOADED) {
        UINT idf = 0;
        UINT ids = ID_FILTERS_SUBITEM_START;
        UINT idl = ID_FILTERSTREAMS_SUBITEM_START;

        BeginEnumFilters(pGB, pEF, pBF) {
            CString name(GetFilterName(pBF));
            if (name.GetLength() >= 43) {
                name = name.Left(40) + _T("...");
            }

            CLSID clsid = GetCLSID(pBF);
            if (clsid == CLSID_AVIDec) {
                CComPtr<IPin> pPin = GetFirstPin(pBF);
                AM_MEDIA_TYPE mt;
                if (pPin && SUCCEEDED(pPin->ConnectionMediaType(&mt))) {
                    DWORD c = ((VIDEOINFOHEADER*)mt.pbFormat)->bmiHeader.biCompression;
                    switch (c) {
                        case BI_RGB:
                            name += _T(" (RGB)");
                            break;
                        case BI_RLE4:
                            name += _T(" (RLE4)");
                            break;
                        case BI_RLE8:
                            name += _T(" (RLE8)");
                            break;
                        case BI_BITFIELDS:
                            name += _T(" (BITF)");
                            break;
                        default:
                            name.Format(_T("%s (%c%c%c%c)"),
                                        CString(name),
                                        (TCHAR)((c >> 0) & 0xff),
                                        (TCHAR)((c >> 8) & 0xff),
                                        (TCHAR)((c >> 16) & 0xff),
                                        (TCHAR)((c >> 24) & 0xff));
                            break;
                    }
                }
            } else if (clsid == CLSID_ACMWrapper) {
                CComPtr<IPin> pPin = GetFirstPin(pBF);
                AM_MEDIA_TYPE mt;
                if (pPin && SUCCEEDED(pPin->ConnectionMediaType(&mt))) {
                    WORD c = ((WAVEFORMATEX*)mt.pbFormat)->wFormatTag;
                    name.Format(_T("%s (0x%04x)"), CString(name), (int)c);
                }
            } else if (clsid == __uuidof(CTextPassThruFilter)
                       || clsid == __uuidof(CNullTextRenderer)
                       || clsid == GUIDFromCString(_T("{48025243-2D39-11CE-875D-00608CB78066}"))) { // ISCR
                // hide these
                continue;
            }

            CAutoPtr<CMenu> pSubSub(DEBUG_NEW CMenu);
            pSubSub->CreatePopupMenu();

            int nPPages = 0;

            CComQIPtr<ISpecifyPropertyPages> pSPP = pBF;

            /*if (pSPP)
              {
                  CAUUID caGUID;
                  caGUID.pElems = NULL;
                  if (SUCCEEDED(pSPP->GetPages(&caGUID)) && caGUID.cElems > 0)
              {
            */
            m_pparray.Add(pBF);
            pSubSub->AppendMenu(MF_STRING | MF_ENABLED, ids, ResStr(IDS_MAINFRM_116));
            /*
            if (caGUID.pElems) CoTaskMemFree(caGUID.pElems);
            */
            nPPages++;
            /*    }
              }
            */
            BeginEnumPins(pBF, pEP, pPin) {
                CString name = GetPinName(pPin);
                name.Replace(_T("&"), _T("&&"));

                if (pSPP = pPin) {
                    CAUUID caGUID;
                    caGUID.pElems = NULL;
                    if (SUCCEEDED(pSPP->GetPages(&caGUID)) && caGUID.cElems > 0) {
                        m_pparray.Add(pPin);
                        pSubSub->AppendMenu(MF_STRING | MF_ENABLED, ids + nPPages, name + ResStr(IDS_MAINFRM_117));

                        if (caGUID.pElems) {
                            CoTaskMemFree(caGUID.pElems);
                        }

                        nPPages++;
                    }
                }
            }
            EndEnumPins;

            CComQIPtr<IAMStreamSelect> pSS = pBF;
            if (pSS) {
                DWORD nStreams = 0;
                DWORD flags = (DWORD) - 1;
                DWORD group = (DWORD) - 1;
                DWORD prevgroup = (DWORD) - 1;
                LCID lcid;
                WCHAR* wname = NULL;
                CComPtr<IUnknown> pObj, pUnk;

                pSS->Count(&nStreams);

                if (nStreams > 0 && nPPages > 0) {
                    pSubSub->AppendMenu(MF_SEPARATOR | MF_ENABLED);
                }

                UINT idlstart = idl;

                for (DWORD i = 0; i < nStreams; i++, pObj = NULL, pUnk = NULL) {
                    m_ssarray.Add(pSS);

                    flags = group = 0;
                    wname = NULL;
                    pSS->Info(i, NULL, &flags, &lcid, &group, &wname, &pObj, &pUnk);

                    if (group != prevgroup && idl > idlstart) {
                        pSubSub->AppendMenu(MF_SEPARATOR | MF_ENABLED);
                    }
                    prevgroup = group;

                    if (flags & AMSTREAMSELECTINFO_EXCLUSIVE) {
                    } else if (flags & AMSTREAMSELECTINFO_ENABLED) {
                    }

                    if (!wname) {
                        CStringW stream(ResStr(IDS_AG_UNKNOWN_STREAM));
                        size_t count = stream.GetLength() + 3 + 1;
                        wname = (WCHAR*)CoTaskMemAlloc(count * sizeof(WCHAR));
                        swprintf_s(wname, count, L"%s %d", stream, min(i + 1, 999));
                    }

                    CString name(wname);
                    name.Replace(_T("&"), _T("&&"));

                    pSubSub->AppendMenu(MF_STRING | MF_ENABLED, idl++, name);

                    CoTaskMemFree(wname);
                }

                if (nStreams == 0) {
                    pSS.Release();
                }
            }

            if (nPPages == 1 && !pSS) {
                pSub->AppendMenu(MF_STRING | MF_ENABLED, ids, name);
            } else {
                pSub->AppendMenu(MF_STRING | MF_DISABLED | MF_GRAYED, idf, name);

                if (nPPages > 0 || pSS) {
                    MENUITEMINFO mii;
                    mii.cbSize = sizeof(mii);
                    mii.fMask = MIIM_STATE | MIIM_SUBMENU;
                    mii.fType = MF_POPUP;
                    mii.hSubMenu = pSubSub->m_hMenu;
                    mii.fState = (pSPP || pSS) ? MF_ENABLED : (MF_DISABLED | MF_GRAYED);
                    pSub->SetMenuItemInfo(idf, &mii, TRUE);

                    m_filterpopups.Add(pSubSub);
                }
            }

            ids += nPPages;
            idf++;
        }
        EndEnumFilters;
    }
}

void CMainFrame::SetupLanguageMenu()
{
    const CAppSettings& s = AfxGetAppSettings();

    if (!IsMenu(m_language.m_hMenu)) {
        m_language.CreatePopupMenu();
    } else {
        // Empty the menu
        while (m_language.RemoveMenu(0, MF_BYPOSITION));
    }

    UINT uiCount = 0;
    CString appPath = GetProgramPath();

    for (size_t i = 0; i < CMPlayerCApp::languageResourcesCount; i++) {
        const LanguageResource& lr = CMPlayerCApp::languageResources[i];

        if (lr.dllPath == NULL || FileExists(appPath + lr.dllPath)) {
            m_language.AppendMenu(MF_STRING | MF_ENABLED, lr.resourceID, lr.name);

            if (lr.localeID == s.language) {
                m_language.CheckMenuItem(uiCount, MF_BYPOSITION | MF_CHECKED);
            }

            uiCount++;
        }
    }

    if (uiCount <= 1) {
        m_language.RemoveMenu(0, MF_BYPOSITION);
    }
}

void CMainFrame::SetupAudioSwitcherSubMenu()
{
    CMenu* pSub = &m_audios;

    if (!IsMenu(pSub->m_hMenu)) {
        pSub->CreatePopupMenu();
    } else while (pSub->RemoveMenu(0, MF_BYPOSITION)) {
            ;
        }

    if (m_iMediaLoadState == MLS_LOADED) {
        UINT id = ID_AUDIO_SUBITEM_START;

        CComQIPtr<IAMStreamSelect> pSS = FindFilter(__uuidof(CAudioSwitcherFilter), pGB);
        if (!pSS) {
            pSS = FindFilter(CLSID_MorganStreamSwitcher, pGB);
        }

        if (pSS) {
            DWORD cStreams = 0;
            if (SUCCEEDED(pSS->Count(&cStreams)) && cStreams > 0) {
                pSub->AppendMenu(MF_STRING | MF_ENABLED, id++, ResStr(IDS_SUBTITLES_OPTIONS));
                pSub->AppendMenu(MF_SEPARATOR | MF_ENABLED);

                long iSel = 0;

                for (long i = 0; i < (long)cStreams; i++) {
                    DWORD dwFlags;
                    WCHAR* pName = NULL;
                    if (FAILED(pSS->Info(i, NULL, &dwFlags, NULL, NULL, &pName, NULL, NULL))) {
                        break;
                    }
                    if (dwFlags) {
                        iSel = i;
                    }

                    CString name(pName);
                    name.Replace(_T("&"), _T("&&"));

                    pSub->AppendMenu(MF_STRING | MF_ENABLED, id++, name);

                    CoTaskMemFree(pName);
                }

                pSub->CheckMenuRadioItem(2, 2 + cStreams - 1, 2 + iSel, MF_BYPOSITION);
            }
        }
    }
}

void CMainFrame::SetupSubtitlesSubMenu()
{
    CMenu* pSub = &m_subtitles;

    if (!IsMenu(pSub->m_hMenu)) {
        pSub->CreatePopupMenu();
    } else while (pSub->RemoveMenu(0, MF_BYPOSITION)) {
            ;
        }

    if (m_iMediaLoadState != MLS_LOADED || m_fAudioOnly || !m_pCAP) {
        return;
    }

    UINT id = ID_SUBTITLES_SUBITEM_START;

    POSITION pos = m_pSubStreams.GetHeadPosition();

    // Build the static menu's items
    bool bStyleEnabled = false;
    if (pos) {
        pSub->AppendMenu(MF_STRING | MF_ENABLED, id++, ResStr(IDS_SUBTITLES_OPTIONS));
        pSub->AppendMenu(MF_STRING | MF_ENABLED, id++, ResStr(IDS_SUBTITLES_STYLES));
        pSub->AppendMenu(MF_STRING | MF_ENABLED, id++, ResStr(IDS_SUBTITLES_RELOAD));
        pSub->AppendMenu(MF_SEPARATOR);

        pSub->AppendMenu(MF_STRING | MF_ENABLED, id++, ResStr(IDS_SUBTITLES_ENABLE));
        pSub->AppendMenu(MF_STRING | MF_ENABLED, id++, ResStr(IDS_SUBTITLES_DEFAULT_STYLE));
        pSub->AppendMenu(MF_SEPARATOR);
    }

    // Build the dynamic menu's items
    int i = 0;
    while (pos) {
        SubtitleInput& subElement = m_pSubStreams.GetNext(pos);

        CLSID clsid;
        if (i == m_iSubtitleSel && SUCCEEDED(subElement.subStream->GetClassID(&clsid))
                && clsid == __uuidof(CRenderedTextSubtitle)) {
            bStyleEnabled = true;
        }

        if (CComQIPtr<IAMStreamSelect> pSSF = subElement.sourceFilter) {
            DWORD cStreams;
            if (FAILED(pSSF->Count(&cStreams))) {
                continue;
            }

            for (int j = 0, cnt = (int)cStreams; j < cnt; j++) {
                DWORD dwGroup;
                LCID lcid;
                WCHAR* pszName = NULL;

                if (FAILED(pSSF->Info(j, NULL, NULL, &lcid, &dwGroup, &pszName, NULL, NULL))
                        || !pszName) {
                    continue;
                }

                CString name(pszName);
                CString lcname = CString(name).MakeLower();
                CoTaskMemFree(pszName);

                if (dwGroup != 2) {
                    continue;
                }

                CString str;

                if (lcname.Find(_T(" off")) >= 0) {
                    str.LoadString(IDS_AG_DISABLED);
                } else {
                    if (lcid != 0) {
                        int len = GetLocaleInfo(lcid, LOCALE_SENGLANGUAGE, str.GetBuffer(64), 64);
                        str.ReleaseBufferSetLength(max(len - 1, 0));
                    }

                    CString lcstr = CString(str).MakeLower();

                    if (str.IsEmpty() || lcname.Find(lcstr) >= 0) {
                        str = name;
                    } else if (!name.IsEmpty()) {
                        str = name + _T(" (") + str + _T(")");
                    }
                }

                str.Replace(_T("&"), _T("&&"));
                pSub->AppendMenu(MF_STRING | MF_ENABLED, id++, str);
                i++;
            }
        } else {
            CComPtr<ISubStream> pSubStream = subElement.subStream;
            if (!pSubStream) {
                continue;
            }

            for (int j = 0, cnt = pSubStream->GetStreamCount(); j < cnt; j++) {
                WCHAR* pName = NULL;
                if (SUCCEEDED(pSubStream->GetStreamInfo(j, &pName, NULL))) {
                    CString name(pName);
                    name.Replace(_T("&"), _T("&&"));

                    pSub->AppendMenu(MF_STRING | MF_ENABLED, id++, name);
                    CoTaskMemFree(pName);
                } else {
                    pSub->AppendMenu(MF_STRING | MF_ENABLED, id++, ResStr(IDS_AG_UNKNOWN));
                }
                i++;
            }
        }
        // TODO: find a better way to group these entries
        /*if (pos && m_pSubStreams.GetAt(pos).subStream) {
            CLSID cur, next;
            pSubStream->GetClassID(&cur);
            m_pSubStreams.GetAt(pos).subStream->GetClassID(&next);

            if (cur != next) {
                pSub->AppendMenu(MF_SEPARATOR);
            }
        }*/
    }

    // Set the menu's items' state
    const CAppSettings& s = AfxGetAppSettings();
    // Style
    if (!bStyleEnabled) {
        pSub->EnableMenuItem(1, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);
    }
    // Enabled
    if (s.fEnableSubtitles) {
        pSub->CheckMenuItem(4, MF_BYPOSITION | MF_CHECKED);
    }
    // Default style
    // TODO: foxX - default subtitles style toggle here; still wip
    if (!s.fEnableSubtitles) {
        pSub->EnableMenuItem(5, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);
    }
    if (s.fUseDefaultSubtitlesStyle) {
        pSub->CheckMenuItem(5, MF_BYPOSITION | MF_CHECKED);
    }
    // Selected subtitles track
    pSub->CheckMenuRadioItem(7, 7 + i - 1, 7 + abs(m_iSubtitleSel), MF_BYPOSITION);
}

void CMainFrame::SetupNavAudioSubMenu()
{
    CMenu* pSub = &m_navaudio;

    if (!IsMenu(pSub->m_hMenu)) {
        pSub->CreatePopupMenu();
    } else while (pSub->RemoveMenu(0, MF_BYPOSITION)) {
            ;
        }

    if (m_iMediaLoadState != MLS_LOADED) {
        return;
    }

    UINT id = ID_NAVIGATE_AUDIO_SUBITEM_START;

    if (GetPlaybackMode() == PM_FILE || (GetPlaybackMode() == PM_CAPTURE && AfxGetAppSettings().iDefaultCaptureDevice == 1)) {
        SetupNavStreamSelectSubMenu(pSub, id, 1);
    } else if (GetPlaybackMode() == PM_DVD) {
        ULONG ulStreamsAvailable, ulCurrentStream;
        if (FAILED(pDVDI->GetCurrentAudio(&ulStreamsAvailable, &ulCurrentStream))) {
            return;
        }

        LCID DefLanguage;
        DVD_AUDIO_LANG_EXT ext;
        if (FAILED(pDVDI->GetDefaultAudioLanguage(&DefLanguage, &ext))) {
            return;
        }

        for (ULONG i = 0; i < ulStreamsAvailable; i++) {
            LCID Language;
            if (FAILED(pDVDI->GetAudioLanguage(i, &Language))) {
                continue;
            }

            UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;
            if (Language == DefLanguage) {
                flags |= MF_DEFAULT;
            }
            if (i == ulCurrentStream) {
                flags |= MF_CHECKED;
            }

            CString str;
            if (Language) {
                int len = GetLocaleInfo(Language, LOCALE_SENGLANGUAGE, str.GetBuffer(256), 256);
                str.ReleaseBufferSetLength(max(len - 1, 0));
            } else {
                str.Format(IDS_AG_UNKNOWN, i + 1);
            }

            DVD_AudioAttributes ATR;
            if (SUCCEEDED(pDVDI->GetAudioAttributes(i, &ATR))) {
                switch (ATR.LanguageExtension) {
                    case DVD_AUD_EXT_NotSpecified:
                    default:
                        break;
                    case DVD_AUD_EXT_Captions:
                        str += _T(" (Captions)");
                        break;
                    case DVD_AUD_EXT_VisuallyImpaired:
                        str += _T(" (Visually Impaired)");
                        break;
                    case DVD_AUD_EXT_DirectorComments1:
                        str += ResStr(IDS_MAINFRM_121);
                        break;
                    case DVD_AUD_EXT_DirectorComments2:
                        str += ResStr(IDS_MAINFRM_122);
                        break;
                }

                CString format = GetDVDAudioFormatName(ATR);

                if (!format.IsEmpty()) {
                    str.Format(IDS_MAINFRM_11,
                               CString(str),
                               format,
                               ATR.dwFrequency,
                               ATR.bQuantization,
                               ATR.bNumberOfChannels,
                               (ATR.bNumberOfChannels > 1 ? ResStr(IDS_MAINFRM_13) : ResStr(IDS_MAINFRM_12)));
                }
            }

            str.Replace(_T("&"), _T("&&"));

            pSub->AppendMenu(flags, id++, str);
        }
    }
}

void CMainFrame::SetupNavSubtitleSubMenu()
{
    CMenu* pSub = &m_navsubtitle;

    if (!IsMenu(pSub->m_hMenu)) {
        pSub->CreatePopupMenu();
    } else while (pSub->RemoveMenu(0, MF_BYPOSITION)) {
            ;
        }

    if (m_iMediaLoadState != MLS_LOADED) {
        return;
    }

    UINT id = ID_NAVIGATE_SUBP_SUBITEM_START;

    if (GetPlaybackMode() == PM_FILE || (GetPlaybackMode() == PM_CAPTURE && AfxGetAppSettings().iDefaultCaptureDevice == 1)) {
        SetupNavStreamSelectSubMenu(pSub, id, 2);
    } else if (GetPlaybackMode() == PM_DVD) {
        ULONG ulStreamsAvailable, ulCurrentStream;
        BOOL bIsDisabled;
        if (FAILED(pDVDI->GetCurrentSubpicture(&ulStreamsAvailable, &ulCurrentStream, &bIsDisabled))
                || ulStreamsAvailable == 0) {
            return;
        }

        LCID DefLanguage;
        DVD_SUBPICTURE_LANG_EXT ext;
        if (FAILED(pDVDI->GetDefaultSubpictureLanguage(&DefLanguage, &ext))) {
            return;
        }

        pSub->AppendMenu(MF_STRING | (bIsDisabled ? 0 : MF_CHECKED), id++, ResStr(IDS_AG_ENABLED));
        pSub->AppendMenu(MF_SEPARATOR | MF_ENABLED);

        for (ULONG i = 0; i < ulStreamsAvailable; i++) {
            LCID Language;
            if (FAILED(pDVDI->GetSubpictureLanguage(i, &Language))) {
                continue;
            }

            UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;
            if (Language == DefLanguage) {
                flags |= MF_DEFAULT;
            }
            if (i == ulCurrentStream) {
                flags |= MF_CHECKED;
            }

            CString str;
            if (Language) {
                int len = GetLocaleInfo(Language, LOCALE_SENGLANGUAGE, str.GetBuffer(256), 256);
                str.ReleaseBufferSetLength(max(len - 1, 0));
            } else {
                str.Format(IDS_AG_UNKNOWN, i + 1);
            }

            DVD_SubpictureAttributes ATR;
            if (SUCCEEDED(pDVDI->GetSubpictureAttributes(i, &ATR))) {
                switch (ATR.LanguageExtension) {
                    case DVD_SP_EXT_NotSpecified:
                    default:
                        break;
                    case DVD_SP_EXT_Caption_Normal:
                        str += _T("");
                        break;
                    case DVD_SP_EXT_Caption_Big:
                        str += _T(" (Big)");
                        break;
                    case DVD_SP_EXT_Caption_Children:
                        str += _T(" (Children)");
                        break;
                    case DVD_SP_EXT_CC_Normal:
                        str += _T(" (CC)");
                        break;
                    case DVD_SP_EXT_CC_Big:
                        str += _T(" (CC Big)");
                        break;
                    case DVD_SP_EXT_CC_Children:
                        str += _T(" (CC Children)");
                        break;
                    case DVD_SP_EXT_Forced:
                        str += _T(" (Forced)");
                        break;
                    case DVD_SP_EXT_DirectorComments_Normal:
                        str += _T(" (Director Comments)");
                        break;
                    case DVD_SP_EXT_DirectorComments_Big:
                        str += _T(" (Director Comments, Big)");
                        break;
                    case DVD_SP_EXT_DirectorComments_Children:
                        str += _T(" (Director Comments, Children)");
                        break;
                }
            }

            str.Replace(_T("&"), _T("&&"));

            pSub->AppendMenu(flags, id++, str);
        }
    }
}

void CMainFrame::SetupNavAngleSubMenu()
{
    CMenu* pSub = &m_navangle;

    if (!IsMenu(pSub->m_hMenu)) {
        pSub->CreatePopupMenu();
    } else while (pSub->RemoveMenu(0, MF_BYPOSITION)) {
            ;
        }

    if (m_iMediaLoadState != MLS_LOADED) {
        return;
    }

    UINT id = ID_NAVIGATE_ANGLE_SUBITEM_START;

    if (GetPlaybackMode() == PM_FILE) {
        SetupNavStreamSelectSubMenu(pSub, id, 0);
    } else if (GetPlaybackMode() == PM_DVD) {
        ULONG ulStreamsAvailable, ulCurrentStream;
        if (FAILED(pDVDI->GetCurrentAngle(&ulStreamsAvailable, &ulCurrentStream))) {
            return;
        }

        if (ulStreamsAvailable < 2) {
            return;    // one choice is not a choice...
        }

        for (ULONG i = 1; i <= ulStreamsAvailable; i++) {
            UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;
            if (i == ulCurrentStream) {
                flags |= MF_CHECKED;
            }

            CString str;
            str.Format(IDS_AG_ANGLE, i);

            pSub->AppendMenu(flags, id++, str);
        }
    }
}

static CString StripPath(CString path)
{
    CString p = path;
    p.Replace('\\', '/');
    p = p.Mid(p.ReverseFind('/') + 1);
    return (p.IsEmpty() ? path : p);
}

void CMainFrame::SetupNavChaptersSubMenu()
{
    CMenu* pSub = &m_navchapters;

    if (!IsMenu(pSub->m_hMenu)) {
        pSub->CreatePopupMenu();
    } else while (pSub->RemoveMenu(0, MF_BYPOSITION)) {
            ;
        }

    if (m_iMediaLoadState != MLS_LOADED) {
        return;
    }

    UINT id = ID_NAVIGATE_CHAP_SUBITEM_START;

    if (GetPlaybackMode() == PM_FILE) {
        if (m_MPLSPlaylist.GetCount() > 1) {
            DWORD idx = 1;
            POSITION pos = m_MPLSPlaylist.GetHeadPosition();
            while (pos) {
                UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;
                if (id != ID_NAVIGATE_CHAP_SUBITEM_START && pos == m_MPLSPlaylist.GetHeadPosition()) {
                    //pSub->AppendMenu(MF_SEPARATOR);
                    flags |= MF_MENUBARBREAK;
                }
                if (idx == MENUBARBREAK) {
                    flags |= MF_MENUBARBREAK;
                    idx = 0;
                }
                idx++;

                CHdmvClipInfo::PlaylistItem Item = m_MPLSPlaylist.GetNext(pos);
                CString time = _T("[") + ReftimeToString2(Item.Duration()) + _T("]");
                CString name = StripPath(Item.m_strFileName);

                if (name == m_wndPlaylistBar.m_pl.GetHead().GetLabel()) {
                    flags |= MF_CHECKED;
                }

                name.Replace(_T("&"), _T("&&"));
                pSub->AppendMenu(flags, id++, name + '\t' + time);
            }
        }

        SetupChapters();
        REFERENCE_TIME rt = GetPos();
        DWORD j = m_pCB->ChapLookup(&rt, NULL);

        if (m_pCB->ChapGetCount() > 1) {
            for (DWORD i = 0, idx = 0; i < m_pCB->ChapGetCount(); i++, id++, idx++) {
                rt = 0;
                CComBSTR bstr;
                if (FAILED(m_pCB->ChapGet(i, &rt, &bstr))) {
                    continue;
                }

                CString time = _T("[") + ReftimeToString2(rt) + _T("]");

                CString name = CString(bstr);
                name.Replace(_T("&"), _T("&&"));
                name.Replace(_T("\t"), _T(" "));

                UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;
                if (i == j) {
                    flags |= MF_CHECKED;
                }

                if (idx == MENUBARBREAK) {
                    flags |= MF_MENUBARBREAK;
                    idx = 0;
                }

                if (id != ID_NAVIGATE_CHAP_SUBITEM_START && i == 0) {
                    //pSub->AppendMenu(MF_SEPARATOR);
                    if (m_MPLSPlaylist.GetCount() > 1) {
                        flags |= MF_MENUBARBREAK;
                    }
                }
                pSub->AppendMenu(flags, id, name + '\t' + time);
            }
        }

        if (m_wndPlaylistBar.GetCount() > 1) {
            POSITION pos = m_wndPlaylistBar.m_pl.GetHeadPosition();
            while (pos) {
                UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;
                if (pos == m_wndPlaylistBar.m_pl.GetPos()) {
                    flags |= MF_CHECKED;
                }
                if (id != ID_NAVIGATE_CHAP_SUBITEM_START && pos == m_wndPlaylistBar.m_pl.GetHeadPosition()) {
                    pSub->AppendMenu(MF_SEPARATOR);
                }
                CPlaylistItem& pli = m_wndPlaylistBar.m_pl.GetNext(pos);
                CString name = pli.GetLabel();
                name.Replace(_T("&"), _T("&&"));
                pSub->AppendMenu(flags, id++, name);
            }
        }

    } else if (GetPlaybackMode() == PM_DVD) {
        ULONG ulNumOfVolumes, ulVolume;
        DVD_DISC_SIDE Side;
        ULONG ulNumOfTitles = 0;
        pDVDI->GetDVDVolumeInfo(&ulNumOfVolumes, &ulVolume, &Side, &ulNumOfTitles);

        DVD_PLAYBACK_LOCATION2 Location;
        pDVDI->GetCurrentLocation(&Location);

        ULONG ulNumOfChapters = 0;
        pDVDI->GetNumberOfChapters(Location.TitleNum, &ulNumOfChapters);

        ULONG ulUOPs = 0;
        pDVDI->GetCurrentUOPS(&ulUOPs);

        for (ULONG i = 1; i <= ulNumOfTitles; i++) {
            UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;
            if (i == Location.TitleNum) {
                flags |= MF_CHECKED;
            }
            if (ulUOPs & UOP_FLAG_Play_Title) {
                flags |= MF_DISABLED | MF_GRAYED;
            }

            CString str;
            str.Format(IDS_AG_TITLE, i);

            pSub->AppendMenu(flags, id++, str);
        }

        for (ULONG i = 1; i <= ulNumOfChapters; i++) {
            UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;
            if (i == Location.ChapterNum) {
                flags |= MF_CHECKED;
            }
            if (ulUOPs & UOP_FLAG_Play_Chapter) {
                flags |= MF_DISABLED | MF_GRAYED;
            }
            if (i == 1) {
                flags |= MF_MENUBARBREAK;
            }

            CString str;
            str.Format(IDS_AG_CHAPTER, i);

            pSub->AppendMenu(flags, id++, str);
        }
    } else if (GetPlaybackMode() == PM_CAPTURE && AfxGetAppSettings().iDefaultCaptureDevice == 1) {
        const CAppSettings& s = AfxGetAppSettings();

        POSITION pos = s.m_DVBChannels.GetHeadPosition();
        while (pos) {
            const CDVBChannel& channel = s.m_DVBChannels.GetNext(pos);
            UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;

            if ((UINT)channel.GetPrefNumber() == s.nDVBLastChannel) {
                flags |= MF_CHECKED;
            }
            pSub->AppendMenu(flags, ID_NAVIGATE_CHAP_SUBITEM_START + channel.GetPrefNumber(), channel.GetName());
        }
    }
}

IBaseFilter* CMainFrame::FindSourceSelectableFilter()
{
    // splitters for video files (mpeg files with only audio track is very rare)
    IBaseFilter* pSF = FindFilter(__uuidof(CMpegSplitterFilter), pGB);
    if (!pSF) {
        pSF = FindFilter(__uuidof(CMpegSourceFilter), pGB);
    }
    // universal splitters
    if (!pSF) {
        pSF = FindFilter(CLSID_OggSplitter, pGB);
    }
    if (!pSF) {
        pSF = FindFilter(L"{171252A0-8820-4AFE-9DF8-5C92B2D66B04}", pGB); // LAV Splitter
    }
    if (!pSF) {
        pSF = FindFilter(L"{B98D13E7-55DB-4385-A33D-09FD1BA26338}", pGB); // LAV Splitter Source
    }
    if (!pSF) {
        pSF = FindFilter(L"{55DA30FC-F16B-49fc-BAA5-AE59FC65F82D}", pGB); // Haali Media Source
    }
    if (!pSF) {
        pSF = FindFilter(L"{564FD788-86C9-4444-971E-CC4A243DA150}", pGB); // Haali Media Splitter with previous file source (like rarfilesource)
    }
    if (!pSF) {
        pSF = FindFilter(L"{529A00DB-0C43-4f5b-8EF2-05004CBE0C6F}", pGB); // AV Splitter
    }
    if (!pSF) {
        pSF = FindFilter(L"{D8980E15-E1F6-4916-A10F-D7EB4E9E10B8}", pGB); // AV Source
    }

    return pSF;
}

void CMainFrame::SetupNavStreamSelectSubMenu(CMenu* pSub, UINT id, DWORD dwSelGroup)
{
    UINT baseid = id;

    CComQIPtr<IAMStreamSelect> pSS = FindSourceSelectableFilter();
    if (!pSS) {
        pSS = pGB;
    }
    if (!pSS) {
        return;
    }

    DWORD cStreams;
    if (FAILED(pSS->Count(&cStreams))) {
        return;
    }

    DWORD dwPrevGroup = (DWORD) - 1;

    for (int i = 0, j = cStreams; i < j; i++) {
        DWORD dwFlags, dwGroup;
        LCID lcid;
        WCHAR* pszName = NULL;

        if (FAILED(pSS->Info(i, NULL, &dwFlags, &lcid, &dwGroup, &pszName, NULL, NULL))
                || !pszName) {
            continue;
        }

        CString name(pszName);
        CString lcname = CString(name).MakeLower();

        if (pszName) {
            CoTaskMemFree(pszName);
        }

        if (dwGroup != dwSelGroup) {
            continue;
        }

        if (dwPrevGroup != -1 && dwPrevGroup != dwGroup) {
            pSub->AppendMenu(MF_SEPARATOR);
        }
        dwPrevGroup = dwGroup;

        CString str;

        if (lcname.Find(_T(" off")) >= 0) {
            str.LoadString(IDS_AG_DISABLED);
        } else {
            if (lcid != 0) {
                int len = GetLocaleInfo(lcid, LOCALE_SENGLANGUAGE, str.GetBuffer(64), 64);
                str.ReleaseBufferSetLength(max(len - 1, 0));
            }

            CString lcstr = CString(str).MakeLower();

            if (str.IsEmpty() || lcname.Find(lcstr) >= 0) {
                str = name;
            } else if (!name.IsEmpty()) {
                str = CString(name) + _T(" (") + str + _T(")");
            }
        }

        UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;
        if (dwFlags) {
            flags |= MF_CHECKED;
        }

        str.Replace(_T("&"), _T("&&"));
        pSub->AppendMenu(flags, id++, str);
    }
}

void CMainFrame::OnNavStreamSelectSubMenu(UINT id, DWORD dwSelGroup)
{
    CComQIPtr<IAMStreamSelect> pSS = FindSourceSelectableFilter();
    if (!pSS) {
        pSS = pGB;
    }
    if (!pSS) {
        return;
    }

    DWORD cStreams;
    if (FAILED(pSS->Count(&cStreams))) {
        return;
    }

    for (int i = 0, j = cStreams; i < j; i++) {
        DWORD dwFlags, dwGroup;
        LCID lcid;
        WCHAR* pszName = NULL;

        if (FAILED(pSS->Info(i, NULL, &dwFlags, &lcid, &dwGroup, &pszName, NULL, NULL))
                || !pszName) {
            continue;
        }

        if (pszName) {
            CoTaskMemFree(pszName);
        }

        if (dwGroup != dwSelGroup) {
            continue;
        }

        if (id == 0) {
            pSS->Enable(i, AMSTREAMSELECTENABLE_ENABLE);
            break;
        }

        id--;
    }
}

void CMainFrame::SetupRecentFilesSubMenu()
{
    CMenu* pSub = &m_recentfiles;

    if (!IsMenu(pSub->m_hMenu)) {
        pSub->CreatePopupMenu();
    } else while (pSub->RemoveMenu(0, MF_BYPOSITION)) {
            ;
        }

    UINT id = ID_RECENT_FILE_START;
    CRecentFileList& MRU = AfxGetAppSettings().MRU;
    MRU.ReadList();

    int mru_count = 0;
    for (int i = 0; i < MRU.GetSize(); i++) {
        if (!MRU[i].IsEmpty()) {
            mru_count++;
            break;
        }
    }
    if (mru_count) {
        pSub->AppendMenu(MF_STRING | MF_ENABLED, ID_RECENT_FILES_CLEAR, ResStr(IDS_RECENT_FILES_CLEAR));
        pSub->AppendMenu(MF_SEPARATOR | MF_ENABLED);
    }

    for (int i = 0; i < MRU.GetSize(); i++) {
        UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;
        if (!MRU[i].IsEmpty()) {
            pSub->AppendMenu(flags, id, MRU[i]);
        }
        id++;
    }
}

void CMainFrame::SetupFavoritesSubMenu()
{
    CMenu* pSub = &m_favorites;

    if (!IsMenu(pSub->m_hMenu)) {
        pSub->CreatePopupMenu();
    } else while (pSub->RemoveMenu(0, MF_BYPOSITION)) {
            ;
        }

    const CAppSettings& s = AfxGetAppSettings();

    pSub->AppendMenu(MF_STRING | MF_ENABLED, ID_FAVORITES_ADD, ResStr(IDS_FAVORITES_ADD));
    pSub->AppendMenu(MF_STRING | MF_ENABLED, ID_FAVORITES_ORGANIZE, ResStr(IDS_FAVORITES_ORGANIZE));

    UINT nLastGroupStart = pSub->GetMenuItemCount();
    UINT id = ID_FAVORITES_FILE_START;
    CAtlList<CString> sl;
    AfxGetAppSettings().GetFav(FAV_FILE, sl);
    POSITION pos = sl.GetHeadPosition();

    while (pos) {
        UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;

        CString f_str = sl.GetNext(pos);
        f_str.Replace(_T("&"), _T("&&"));
        f_str.Replace(_T("\t"), _T(" "));

        CAtlList<CString> sl;
        Explode(f_str, sl, ';', 3);

        f_str = sl.RemoveHead();

        CString str;

        if (!sl.IsEmpty()) {
            bool bPositionDataPresent = false;

            // pos
            REFERENCE_TIME rt = 0;
            if (1 == _stscanf_s(sl.GetHead(), _T("%I64d"), &rt) && rt > 0) {
                DVD_HMSF_TIMECODE hmsf = RT2HMSF(rt);
                str.Format(_T("[%02d:%02d:%02d]"), hmsf.bHours, hmsf.bMinutes, hmsf.bSeconds);
                bPositionDataPresent = true;
            }

            // relative drive
            if (sl.GetCount() > 1) {   // Here to prevent crash if old favorites settings are present
                sl.RemoveHead();

                BOOL bRelativeDrive = FALSE;
                if (_stscanf_s(sl.GetHead(), _T("%d"), &bRelativeDrive) == 1) {
                    if (bRelativeDrive) {
                        str.Format(_T("[RD]%s"), CString(str));
                    }
                }
            }
            if (!str.IsEmpty()) {
                f_str.Format(_T("%s\t%.14s"), CString(f_str), CString(str));
            }
        }

        if (!f_str.IsEmpty()) {
            pSub->AppendMenu(flags, id, f_str);
        }

        id++;
    }

    if (id > ID_FAVORITES_FILE_START) {
        pSub->InsertMenu(nLastGroupStart, MF_SEPARATOR | MF_ENABLED | MF_BYPOSITION);
    }

    nLastGroupStart = pSub->GetMenuItemCount();

    id = ID_FAVORITES_DVD_START;
    s.GetFav(FAV_DVD, sl);
    pos = sl.GetHeadPosition();

    while (pos) {
        UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;

        CString str = sl.GetNext(pos);
        str.Replace(_T("&"), _T("&&"));

        CAtlList<CString> sl;
        Explode(str, sl, ';', 2);

        str = sl.RemoveHead();

        if (!sl.IsEmpty()) {
            // TODO
        }

        if (!str.IsEmpty()) {
            pSub->AppendMenu(flags, id, str);
        }

        id++;
    }

    if (id > ID_FAVORITES_DVD_START) {
        pSub->InsertMenu(nLastGroupStart, MF_SEPARATOR | MF_ENABLED | MF_BYPOSITION);
    }

    nLastGroupStart = pSub->GetMenuItemCount();

    id = ID_FAVORITES_DEVICE_START;

    s.GetFav(FAV_DEVICE, sl);

    pos = sl.GetHeadPosition();
    while (pos) {
        UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;

        CString str = sl.GetNext(pos);
        str.Replace(_T("&"), _T("&&"));

        CAtlList<CString> sl;
        Explode(str, sl, ';', 2);

        str = sl.RemoveHead();

        if (!str.IsEmpty()) {
            pSub->AppendMenu(flags, id, str);
        }

        id++;
    }
}

void CMainFrame::SetupShadersSubMenu()
{
    CMenu* pSub = &m_shaders;

    if (!IsMenu(pSub->m_hMenu)) {
        pSub->CreatePopupMenu();
    } else while (pSub->RemoveMenu(0, MF_BYPOSITION)) {
            ;
        }

    pSub->AppendMenu(MF_STRING | MF_ENABLED, ID_SHADERS_TOGGLE, ResStr(IDS_SHADERS_TOGGLE));
    pSub->AppendMenu(MF_STRING | MF_ENABLED, ID_SHADERS_TOGGLE_SCREENSPACE, ResStr(IDS_SHADERS_TOGGLE_SCREENSPACE));
    pSub->AppendMenu(MF_STRING | MF_ENABLED, ID_SHADERS_SELECT, ResStr(IDS_SHADERS_SELECT));
    pSub->AppendMenu(MF_SEPARATOR);
    pSub->AppendMenu(MF_STRING | MF_ENABLED, ID_VIEW_SHADEREDITOR, ResStr(IDS_SHADERS_EDIT));
}

/////////////

void CMainFrame::ShowControls(int nCS, bool fSave /*= false*/)
{
    int nCSprev = m_nCS;
    int hbefore = 0, hafter = 0;
    m_pLastBar = NULL;
    POSITION pos = m_bars.GetHeadPosition();

    for (int i = 1; pos; i <<= 1) {
        CControlBar* pNext = m_bars.GetNext(pos);
        ShowControlBar(pNext, !!(nCS & i), TRUE);
        if (nCS & i) {
            m_pLastBar = pNext;
        }

        CSize s = pNext->CalcFixedLayout(FALSE, TRUE);
        if (nCSprev & i) {
            hbefore += s.cy;
        }
        if (nCS & i) {
            hafter += s.cy;
        }
    }

    WINDOWPLACEMENT wp;
    wp.length = sizeof(wp);
    GetWindowPlacement(&wp);

    if (wp.showCmd != SW_SHOWMAXIMIZED && !m_fFullScreen) {
        CRect r;
        GetWindowRect(r);
        MoveWindow(r.left, r.top, r.Width(), r.Height() + hafter - hbefore);
    }

    if (fSave) {
        m_nCS = nCS;
    }

    RecalcLayout();
}

void CMainFrame::SetAlwaysOnTop(int i)
{
    AfxGetAppSettings().iOnTop = i;

    if (!m_fFullScreen) {
        const CWnd* pInsertAfter = NULL;

        if (i == 0) {
            pInsertAfter = &wndNoTopMost;
        } else if (i == 1) {
            pInsertAfter = &wndTopMost;
        } else if (i == 2) {
            pInsertAfter = GetMediaState() == State_Running ? &wndTopMost : &wndNoTopMost;
        } else { // if (i == 3)
            pInsertAfter = (GetMediaState() == State_Running && !m_fAudioOnly) ? &wndTopMost : &wndNoTopMost;
        }

        SetWindowPos(pInsertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    } else if (!(GetWindowLongPtr(m_hWnd, GWL_EXSTYLE)&WS_EX_TOPMOST)) {
        if (!AfxGetAppSettings().IsD3DFullscreen()) {
            SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }
}

void CMainFrame::AddTextPassThruFilter()
{
    BeginEnumFilters(pGB, pEF, pBF) {
        if (!IsSplitter(pBF)) {
            continue;
        }

        BeginEnumPins(pBF, pEP, pPin) {
            CComPtr<IPin> pPinTo;
            AM_MEDIA_TYPE mt;
            if (FAILED(pPin->ConnectedTo(&pPinTo)) || !pPinTo
                    || FAILED(pPin->ConnectionMediaType(&mt))
                    || mt.majortype != MEDIATYPE_Text && mt.majortype != MEDIATYPE_Subtitle) {
                continue;
            }

            CComQIPtr<IBaseFilter> pTPTF = DEBUG_NEW CTextPassThruFilter(this);
            CStringW name;
            name.Format(L"TextPassThru%08x", pTPTF);
            if (FAILED(pGB->AddFilter(pTPTF, name))) {
                continue;
            }

            HRESULT hr;

            hr = pPinTo->Disconnect();
            hr = pPin->Disconnect();

            if (FAILED(hr = pGB->ConnectDirect(pPin, GetFirstPin(pTPTF, PINDIR_INPUT), NULL))
                    || FAILED(hr = pGB->ConnectDirect(GetFirstPin(pTPTF, PINDIR_OUTPUT), pPinTo, NULL))) {
                hr = pGB->ConnectDirect(pPin, pPinTo, NULL);
            } else {
                SubtitleInput subElement(CComQIPtr<ISubStream>(pTPTF), pBF);
                m_pSubStreams.AddTail(subElement);
            }
        }
        EndEnumPins;
    }
    EndEnumFilters;
}

bool CMainFrame::LoadSubtitle(CString fn, ISubStream** actualStream)
{
    CComQIPtr<ISubStream> pSubStream;

    // TMP: maybe this will catch something for those who get a runtime error dialog when opening subtitles from cds
    try {
        if (!pSubStream) {
            CAutoPtr<CVobSubFile> pVSF(DEBUG_NEW CVobSubFile(&m_csSubLock));
            CString ext = CPath(fn).GetExtension().MakeLower();
            if ((ext == _T(".idx") || ext == _T(".sub")) && pVSF && pVSF->Open(fn) && pVSF->GetStreamCount() > 0) {
                pSubStream = pVSF.Detach();
            }
        }

        if (!pSubStream) {
            CAutoPtr<CRenderedTextSubtitle> pRTS(DEBUG_NEW CRenderedTextSubtitle(&m_csSubLock, &AfxGetAppSettings().subdefstyle, AfxGetAppSettings().fUseDefaultSubtitlesStyle));

            // The filename of the video file
            CString videoName = m_wndPlaylistBar.GetCurFileName();
            videoName = videoName.Left(videoName.ReverseFind('.')).Mid(videoName.ReverseFind('\\') + 1);

            // The filename of the subtitle file
            CString subName = fn.Left(fn.ReverseFind('.')).Mid(fn.ReverseFind('\\') + 1);

            CString name;
            if (subName.Find(videoName) != -1 && videoName.CompareNoCase(subName) != 0 && subName.Replace(videoName, _T("")) == 1) {
                name = subName.TrimLeft('.');
            } else {
                name.LoadString(IDS_UNDETERMINED);
            }

            if (pRTS && pRTS->Open(fn, DEFAULT_CHARSET, name) && pRTS->GetStreamCount() > 0) {
                pSubStream = pRTS.Detach();
            }
        }
    } catch (CException* e) {
        e->Delete();
    }

    if (pSubStream) {
        SubtitleInput subElement(pSubStream);
        m_pSubStreams.AddTail(subElement);

        if (!m_posFirstExtSub) {
            m_posFirstExtSub = m_pSubStreams.GetTailPosition();
        }

        if (actualStream != NULL) {
            *actualStream = m_pSubStreams.GetTail().subStream;
        }
    }

    return !!pSubStream;
}

void CMainFrame::UpdateSubtitle(bool fDisplayMessage, bool fApplyDefStyle)
{
    if (!m_pCAP) {
        return;
    }

    SubtitleInput subElement;
    int i = GetSubtitleInput(m_iSubtitleSel, subElement);

    if (i >= 0) {
        WCHAR* pName = NULL;
        if (CComQIPtr<IAMStreamSelect> pSSF = subElement.sourceFilter) {
            DWORD dwFlags;
            if (FAILED(pSSF->Info(i, NULL, &dwFlags, NULL, NULL, &pName, NULL, NULL))) {
                dwFlags = 0;
                pName = NULL;
            }
            // Enable the track only if it isn't already the only selected track in the group
            if (!(dwFlags & AMSTREAMSELECTINFO_EXCLUSIVE)) {
                pSSF->Enable(i, AMSTREAMSELECTENABLE_ENABLE);
            }
            i = 0;
        }
        {
            // m_csSubLock shouldn't be locked when using IAMStreamSelect::Enable
            CAutoLock cAutoLock(&m_csSubLock);
            subElement.subStream->SetStream(i);
            SetSubtitle(subElement.subStream, fApplyDefStyle);
        }

        if (fDisplayMessage) {
            if (pName || SUCCEEDED(subElement.subStream->GetStreamInfo(0, &pName, NULL))) {
                CString strMessage;
                strMessage.Format(IDS_SUBTITLE_STREAM, pName);
                m_OSD.DisplayMessage(OSD_TOPLEFT, strMessage);
            }
        }
        if (pName) {
            CoTaskMemFree(pName);
        }
    } else {
        if (fDisplayMessage && m_iSubtitleSel < 0) {
            m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_SUBTITLE_STREAM_OFF));
        }

        m_pCAP->SetSubPicProvider(NULL);
    }
}

void CMainFrame::SetSubtitle(ISubStream* pSubStream, bool fApplyDefStyle)
{
    CAppSettings& s = AfxGetAppSettings();

    if (pSubStream) {
        CLSID clsid;
        pSubStream->GetClassID(&clsid);

        if (clsid == __uuidof(CVobSubFile)) {
            CVobSubFile* pVSF = (CVobSubFile*)(ISubStream*)pSubStream;

            if (fApplyDefStyle) {
                pVSF->SetAlignment(s.fOverridePlacement, s.nHorPos, s.nVerPos, 1, 1);
            }
        } else if (clsid == __uuidof(CVobSubStream)) {
            CVobSubStream* pVSS = (CVobSubStream*)(ISubStream*)pSubStream;

            if (fApplyDefStyle) {
                pVSS->SetAlignment(s.fOverridePlacement, s.nHorPos, s.nVerPos, 1, 1);
            }
        } else if (clsid == __uuidof(CRenderedTextSubtitle)) {
            CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)pSubStream;

            STSStyle style;

            if (fApplyDefStyle || pRTS->m_fUsingAutoGeneratedDefaultStyle) {
                style = s.subdefstyle;

                if (s.fOverridePlacement) {
                    style.scrAlignment = 2;
                    int w = pRTS->m_dstScreenSize.cx;
                    int h = pRTS->m_dstScreenSize.cy;
                    int mw = w - style.marginRect.left - style.marginRect.right;
                    style.marginRect.bottom = h - MulDiv(h, s.nVerPos, 100);
                    style.marginRect.left = MulDiv(w, s.nHorPos, 100) - mw / 2;
                    style.marginRect.right = w - (style.marginRect.left + mw);
                }

                bool res = pRTS->SetDefaultStyle(style);
                UNREFERENCED_PARAMETER(res);
            }

            if (pRTS->GetDefaultStyle(style) && style.relativeTo == 2) {
                style.relativeTo = s.subdefstyle.relativeTo;
                pRTS->SetDefaultStyle(style);
            }

            pRTS->SetOverride(s.fUseDefaultSubtitlesStyle, &s.subdefstyle);

            pRTS->Deinit();
        }

        int i = 0;
        bool found = false;
        POSITION pos = m_pSubStreams.GetHeadPosition();
        while (pos) {
            SubtitleInput& subElement = m_pSubStreams.GetNext(pos);
            CComQIPtr<IAMStreamSelect> pSSF = subElement.sourceFilter;

            if (pSSF) {
                DWORD cStreams;
                if (FAILED(pSSF->Count(&cStreams))) {
                    continue;
                }

                for (int j = 0, cnt = (int)cStreams; j < cnt; j++) {
                    DWORD dwFlags, dwGroup;

                    if (FAILED(pSSF->Info(j, NULL, &dwFlags, NULL, &dwGroup, NULL, NULL, NULL))) {
                        continue;
                    }

                    if (dwGroup != 2) {
                        continue;
                    }

                    if (pSubStream == subElement.subStream
                            && (dwFlags & (AMSTREAMSELECTINFO_ENABLED | AMSTREAMSELECTINFO_EXCLUSIVE))) {
                        break;
                    }

                    i++;
                }
            }

            if (pSubStream == subElement.subStream) {
                m_iSubtitleSel = i + subElement.subStream->GetStream();
                found = true;
                break;
            } else if (!pSSF) {
                i += subElement.subStream->GetStreamCount();
            }
        }
        // We are trying to set a subtitles stream that isn't in the list so we abort here.
        if (!found) {
            return;
        }
    }

    m_nSubtitleId = (DWORD_PTR)pSubStream;

    if (m_pCAP) {
        m_pCAP->SetSubPicProvider(CComQIPtr<ISubPicProvider>(pSubStream));
        m_wndSubresyncBar.SetSubtitle(pSubStream, m_pCAP->GetFPS());
    }
}

void CMainFrame::ReplaceSubtitle(ISubStream* pSubStreamOld, ISubStream* pSubStreamNew)
{
    POSITION pos = m_pSubStreams.GetHeadPosition();
    while (pos) {
        POSITION cur = pos;
        if (pSubStreamOld == m_pSubStreams.GetNext(pos).subStream) {
            m_pSubStreams.GetAt(cur).subStream = pSubStreamNew;
            UpdateSubtitle();
            break;
        }
    }
}

void CMainFrame::InvalidateSubtitle(DWORD_PTR nSubtitleId, REFERENCE_TIME rtInvalidate)
{
    if (m_pCAP) {
        if (nSubtitleId == -1 || nSubtitleId == m_nSubtitleId) {
            m_pCAP->Invalidate(rtInvalidate);
        }
    }
}

void CMainFrame::ReloadSubtitle()
{
    POSITION pos = m_pSubStreams.GetHeadPosition();
    while (pos) {
        m_pSubStreams.GetNext(pos).subStream->Reload();
    }
    UpdateSubtitle();
}

void CMainFrame::SetSubtitleTrackIdx(int index)
{
    if (m_iMediaLoadState == MLS_LOADED) {
        if (index < 0) {
            m_iSubtitleSel ^= 0x80000000;
        } else {
            SubtitleInput subElement;
            int i = GetSubtitleInput(index, subElement);
            if (i >= 0) {
                m_iSubtitleSel = index;
            }
        }
        UpdateSubtitle();
        AfxGetAppSettings().fEnableSubtitles = !(m_iSubtitleSel & 0x80000000);
    }
}

void CMainFrame::SetAudioTrackIdx(int index)
{
    if (m_iMediaLoadState == MLS_LOADED) {
        CComQIPtr<IAMStreamSelect> pSS = FindFilter(__uuidof(CAudioSwitcherFilter), pGB);
        if (!pSS) {
            pSS = FindFilter(CLSID_MorganStreamSwitcher, pGB);
        }

        DWORD cStreams = 0;
        DWORD dwFlags = AMSTREAMSELECTENABLE_ENABLE;
        if (pSS && SUCCEEDED(pSS->Count(&cStreams)))
            if ((index >= 0) && (index < ((int)cStreams))) {
                pSS->Enable(index, dwFlags);
            }
    }
}

REFERENCE_TIME CMainFrame::GetPos() const
{
    return (m_iMediaLoadState == MLS_LOADED ? m_wndSeekBar.GetPos() : 0);
}

REFERENCE_TIME CMainFrame::GetDur() const
{
    __int64 start, stop;
    m_wndSeekBar.GetRange(start, stop);
    return (m_iMediaLoadState == MLS_LOADED ? stop : 0);
}

void CMainFrame::SeekTo(REFERENCE_TIME rtPos, bool fSeekToKeyFrame)
{
    ASSERT(pMS != NULL);
    if (pMS == NULL) {
        return;
    }
    OAFilterState fs = GetMediaState();

    if (rtPos < 0) {
        rtPos = 0;
    }

    m_nStepForwardCount = 0;
    if (GetPlaybackMode() != PM_CAPTURE) {
        __int64 start, stop;
        m_wndSeekBar.GetRange(start, stop);
        GUID tf;
        pMS->GetTimeFormat(&tf);
        if (rtPos > stop) {
            rtPos = stop;
        }
        m_wndStatusBar.SetStatusTimer(rtPos, stop, !!m_wndSubresyncBar.IsWindowVisible(), &tf);
        m_OSD.DisplayMessage(OSD_TOPLEFT, m_wndStatusBar.GetStatusTimer(), 1500);
    }

    if (GetPlaybackMode() == PM_FILE) {
        if (fs == State_Stopped) {
            SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
        }

        if (fSeekToKeyFrame) {
            if (!m_kfs.IsEmpty()) {
                int i = rangebsearch(rtPos, m_kfs);
                if (i >= 1 && i < (int)m_kfs.GetCount() - 1) {
                    rtPos = m_kfs[i + ((m_nSeekDirection == SEEK_DIRECTION_FORWARD) ? 1 : (m_nSeekDirection == SEEK_DIRECTION_BACKWARD) ? (-1) : SEEK_DIRECTION_NONE)];
                }
            }
        }
        m_nSeekDirection = SEEK_DIRECTION_NONE;

        pMS->SetPositions(&rtPos, AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);
    } else if (GetPlaybackMode() == PM_DVD && m_iDVDDomain == DVD_DOMAIN_Title) {
        if (fs != State_Running) {
            SendMessage(WM_COMMAND, ID_PLAY_PLAY);
        }

        DVD_HMSF_TIMECODE tc = RT2HMSF(rtPos);
        pDVDC->PlayAtTime(&tc, DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL);
    } else if (GetPlaybackMode() == PM_CAPTURE) {
        TRACE(_T("Warning (CMainFrame::SeekTo): Trying to seek in capture mode"));
    }
    m_fEndOfStream = false;

    OnTimer(TIMER_STREAMPOSPOLLER);
    OnTimer(TIMER_STREAMPOSPOLLER2);

    SendCurrentPositionToApi(true);
}

void CMainFrame::CleanGraph()
{
    if (!pGB) {
        return;
    }

    BeginEnumFilters(pGB, pEF, pBF) {
        CComQIPtr<IAMFilterMiscFlags> pAMMF(pBF);
        if (pAMMF && (pAMMF->GetMiscFlags()&AM_FILTER_MISC_FLAGS_IS_SOURCE)) {
            continue;
        }

        // some capture filters forget to set AM_FILTER_MISC_FLAGS_IS_SOURCE
        // or to implement the IAMFilterMiscFlags interface
        if (pBF == pVidCap || pBF == pAudCap) {
            continue;
        }

        if (CComQIPtr<IFileSourceFilter>(pBF)) {
            continue;
        }

        int nIn, nOut, nInC, nOutC;
        if (CountPins(pBF, nIn, nOut, nInC, nOutC) > 0 && (nInC + nOutC) == 0) {
            TRACE(CStringW(L"Removing: ") + GetFilterName(pBF) + '\n');

            pGB->RemoveFilter(pBF);
            pEF->Reset();
        }
    }
    EndEnumFilters;
}

#define AUDIOBUFFERLEN 500

static void SetLatency(IBaseFilter* pBF, int cbBuffer)
{
    BeginEnumPins(pBF, pEP, pPin) {
        if (CComQIPtr<IAMBufferNegotiation> pAMBN = pPin) {
            ALLOCATOR_PROPERTIES ap;
            ap.cbAlign = -1;  // -1 means no preference.
            ap.cbBuffer = cbBuffer;
            ap.cbPrefix = -1;
            ap.cBuffers = -1;
            pAMBN->SuggestAllocatorProperties(&ap);
        }
    }
    EndEnumPins;
}

HRESULT CMainFrame::BuildCapture(IPin* pPin, IBaseFilter* pBF[3], const GUID& majortype, AM_MEDIA_TYPE* pmt)
{
    IBaseFilter* pBuff = pBF[0];
    IBaseFilter* pEnc = pBF[1];
    IBaseFilter* pMux = pBF[2];

    if (!pPin || !pMux) {
        return E_FAIL;
    }

    CString err;
    HRESULT hr = S_OK;
    CFilterInfo fi;

    if (FAILED(pMux->QueryFilterInfo(&fi)) || !fi.pGraph) {
        pGB->AddFilter(pMux, L"Multiplexer");
    }

    CStringW prefix;
    CString type;
    if (majortype == MEDIATYPE_Video) {
        prefix = L"Video ";
        type.LoadString(IDS_CAPTURE_ERROR_VIDEO);
    } else if (majortype == MEDIATYPE_Audio) {
        prefix = L"Audio ";
        type.LoadString(IDS_CAPTURE_ERROR_AUDIO);
    }

    if (pBuff) {
        hr = pGB->AddFilter(pBuff, prefix + L"Buffer");
        if (FAILED(hr)) {
            err.Format(IDS_CAPTURE_ERROR_ADD_BUFFER, type);
            MessageBox(err, ResStr(IDS_CAPTURE_ERROR), MB_ICONERROR | MB_OK);
            return hr;
        }

        hr = pGB->ConnectFilter(pPin, pBuff);
        if (FAILED(hr)) {
            err.Format(IDS_CAPTURE_ERROR_CONNECT_BUFF, type);
            MessageBox(err, ResStr(IDS_CAPTURE_ERROR), MB_ICONERROR | MB_OK);
            return hr;
        }

        pPin = GetFirstPin(pBuff, PINDIR_OUTPUT);
    }

    if (pEnc) {
        hr = pGB->AddFilter(pEnc, prefix + L"Encoder");
        if (FAILED(hr)) {
            err.Format(IDS_CAPTURE_ERROR_ADD_ENCODER, type);
            MessageBox(err, ResStr(IDS_CAPTURE_ERROR), MB_ICONERROR | MB_OK);
            return hr;
        }

        hr = pGB->ConnectFilter(pPin, pEnc);
        if (FAILED(hr)) {
            err.Format(IDS_CAPTURE_ERROR_CONNECT_ENC, type);
            MessageBox(err, ResStr(IDS_CAPTURE_ERROR), MB_ICONERROR | MB_OK);
            return hr;
        }

        pPin = GetFirstPin(pEnc, PINDIR_OUTPUT);

        if (CComQIPtr<IAMStreamConfig> pAMSC = pPin) {
            if (pmt->majortype == majortype) {
                hr = pAMSC->SetFormat(pmt);
                if (FAILED(hr)) {
                    err.Format(IDS_CAPTURE_ERROR_COMPRESSION, type);
                    MessageBox(err, ResStr(IDS_CAPTURE_ERROR), MB_ICONERROR | MB_OK);
                    return hr;
                }
            }
        }

    }

    //if (pMux)
    {
        hr = pGB->ConnectFilter(pPin, pMux);
        if (FAILED(hr)) {
            err.Format(IDS_CAPTURE_ERROR_MULTIPLEXER, type);
            MessageBox(err, ResStr(IDS_CAPTURE_ERROR), MB_ICONERROR | MB_OK);
            return hr;
        }
    }

    CleanGraph();

    return S_OK;
}

bool CMainFrame::BuildToCapturePreviewPin(
    IBaseFilter* pVidCap, IPin** ppVidCapPin, IPin** ppVidPrevPin,
    IBaseFilter* pAudCap, IPin** ppAudCapPin, IPin** ppAudPrevPin)
{
    HRESULT hr;
    *ppVidCapPin = *ppVidPrevPin = NULL;
    *ppAudCapPin = *ppAudPrevPin = NULL;
    CComPtr<IPin> pDVAudPin;

    if (pVidCap) {
        CComPtr<IPin> pPin;
        if (!pAudCap // only look for interleaved stream when we don't use any other audio capture source
                && SUCCEEDED(pCGB->FindPin(pVidCap, PINDIR_OUTPUT, &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Interleaved, TRUE, 0, &pPin))) {
            CComPtr<IBaseFilter> pDVSplitter;
            hr = pDVSplitter.CoCreateInstance(CLSID_DVSplitter);
            hr = pGB->AddFilter(pDVSplitter, L"DV Splitter");

            hr = pCGB->RenderStream(NULL, &MEDIATYPE_Interleaved, pPin, NULL, pDVSplitter);

            pPin = NULL;
            hr = pCGB->FindPin(pDVSplitter, PINDIR_OUTPUT, NULL, &MEDIATYPE_Video, TRUE, 0, &pPin);
            hr = pCGB->FindPin(pDVSplitter, PINDIR_OUTPUT, NULL, &MEDIATYPE_Audio, TRUE, 0, &pDVAudPin);

            CComPtr<IBaseFilter> pDVDec;
            hr = pDVDec.CoCreateInstance(CLSID_DVVideoCodec);
            hr = pGB->AddFilter(pDVDec, L"DV Video Decoder");

            hr = pGB->ConnectFilter(pPin, pDVDec);

            pPin = NULL;
            hr = pCGB->FindPin(pDVDec, PINDIR_OUTPUT, NULL, &MEDIATYPE_Video, TRUE, 0, &pPin);
        } else if (FAILED(pCGB->FindPin(pVidCap, PINDIR_OUTPUT, &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, TRUE, 0, &pPin))) {
            MessageBox(ResStr(IDS_CAPTURE_ERROR_VID_CAPT_PIN), ResStr(IDS_CAPTURE_ERROR), MB_ICONERROR | MB_OK);
            return false;
        }

        CComPtr<IBaseFilter> pSmartTee;
        hr = pSmartTee.CoCreateInstance(CLSID_SmartTee);
        hr = pGB->AddFilter(pSmartTee, L"Smart Tee (video)");

        hr = pGB->ConnectFilter(pPin, pSmartTee);

        hr = pSmartTee->FindPin(L"Preview", ppVidPrevPin);
        hr = pSmartTee->FindPin(L"Capture", ppVidCapPin);
    }

    if (pAudCap || pDVAudPin) {
        CComPtr<IPin> pPin;
        if (pDVAudPin) {
            pPin = pDVAudPin;
        } else if (FAILED(pCGB->FindPin(pAudCap, PINDIR_OUTPUT, &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Audio, TRUE, 0, &pPin))) {
            MessageBox(ResStr(IDS_CAPTURE_ERROR_AUD_CAPT_PIN), ResStr(IDS_CAPTURE_ERROR), MB_ICONERROR | MB_OK);
            return false;
        }

        CComPtr<IBaseFilter> pSmartTee;
        hr = pSmartTee.CoCreateInstance(CLSID_SmartTee);
        hr = pGB->AddFilter(pSmartTee, L"Smart Tee (audio)");

        hr = pGB->ConnectFilter(pPin, pSmartTee);

        hr = pSmartTee->FindPin(L"Preview", ppAudPrevPin);
        hr = pSmartTee->FindPin(L"Capture", ppAudCapPin);
    }

    return true;
}

bool CMainFrame::BuildGraphVideoAudio(int fVPreview, bool fVCapture, int fAPreview, bool fACapture)
{
    if (!pCGB) {
        return false;
    }

    OAFilterState __fs = GetMediaState();
    REFERENCE_TIME __rt = 0;

    if (m_iMediaLoadState == MLS_LOADED) {
        __rt = GetPos();
    }

    if (__fs != State_Stopped) {
        SendMessage(WM_COMMAND, ID_PLAY_STOP);
    }

    HRESULT hr;

    pGB->NukeDownstream(pVidCap);
    pGB->NukeDownstream(pAudCap);

    CleanGraph();

    if (pAMVSCCap) {
        hr = pAMVSCCap->SetFormat(&m_wndCaptureBar.m_capdlg.m_mtv);
    }
    if (pAMVSCPrev) {
        hr = pAMVSCPrev->SetFormat(&m_wndCaptureBar.m_capdlg.m_mtv);
    }
    if (pAMASC) {
        hr = pAMASC->SetFormat(&m_wndCaptureBar.m_capdlg.m_mta);
    }

    CComPtr<IBaseFilter> pVidBuffer = m_wndCaptureBar.m_capdlg.m_pVidBuffer;
    CComPtr<IBaseFilter> pAudBuffer = m_wndCaptureBar.m_capdlg.m_pAudBuffer;
    CComPtr<IBaseFilter> pVidEnc = m_wndCaptureBar.m_capdlg.m_pVidEnc;
    CComPtr<IBaseFilter> pAudEnc = m_wndCaptureBar.m_capdlg.m_pAudEnc;
    CComPtr<IBaseFilter> pMux = m_wndCaptureBar.m_capdlg.m_pMux;
    CComPtr<IBaseFilter> pDst = m_wndCaptureBar.m_capdlg.m_pDst;
    CComPtr<IBaseFilter> pAudMux = m_wndCaptureBar.m_capdlg.m_pAudMux;
    CComPtr<IBaseFilter> pAudDst = m_wndCaptureBar.m_capdlg.m_pAudDst;

    bool fFileOutput = (pMux && pDst) || (pAudMux && pAudDst);
    bool fCapture = (fVCapture || fACapture);

    if (pAudCap) {
        AM_MEDIA_TYPE* pmt = &m_wndCaptureBar.m_capdlg.m_mta;
        int ms = (fACapture && fFileOutput && m_wndCaptureBar.m_capdlg.m_fAudOutput) ? AUDIOBUFFERLEN : 60;
        if (pMux != pAudMux && fACapture) {
            SetLatency(pAudCap, -1);
        } else if (pmt->pbFormat) {
            SetLatency(pAudCap, ((WAVEFORMATEX*)pmt->pbFormat)->nAvgBytesPerSec * ms / 1000);
        }
    }

    CComPtr<IPin> pVidCapPin, pVidPrevPin, pAudCapPin, pAudPrevPin;
    BuildToCapturePreviewPin(pVidCap, &pVidCapPin, &pVidPrevPin, pAudCap, &pAudCapPin, &pAudPrevPin);

    //if (pVidCap)
    {
        bool fVidPrev = pVidPrevPin && fVPreview;
        bool fVidCap = pVidCapPin && fVCapture && fFileOutput && m_wndCaptureBar.m_capdlg.m_fVidOutput;

        if (fVPreview == 2 && !fVidCap && pVidCapPin) {
            pVidPrevPin = pVidCapPin;
            pVidCapPin = NULL;
        }

        if (fVidPrev) {
            m_pCAP = NULL;
            m_pCAP2 = NULL;
            pGB->Render(pVidPrevPin);
            pGB->FindInterface(__uuidof(ISubPicAllocatorPresenter), (void**)&m_pCAP, TRUE);
            pGB->FindInterface(__uuidof(ISubPicAllocatorPresenter2), (void**)&m_pCAP2, TRUE);
        }

        if (fVidCap) {
            IBaseFilter* pBF[3] = {pVidBuffer, pVidEnc, pMux};
            HRESULT hr2 = BuildCapture(pVidCapPin, pBF, MEDIATYPE_Video, &m_wndCaptureBar.m_capdlg.m_mtcv);
            UNREFERENCED_PARAMETER(hr2);
        }

        pAMDF = NULL;
        pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, pVidCap, IID_IAMDroppedFrames, (void**)&pAMDF);
    }

    //if (pAudCap)
    {
        bool fAudPrev = pAudPrevPin && fAPreview;
        bool fAudCap = pAudCapPin && fACapture && fFileOutput && m_wndCaptureBar.m_capdlg.m_fAudOutput;

        if (fAPreview == 2 && !fAudCap && pAudCapPin) {
            pAudPrevPin = pAudCapPin;
            pAudCapPin = NULL;
        }

        if (fAudPrev) {
            pGB->Render(pAudPrevPin);
        }

        if (fAudCap) {
            IBaseFilter* pBF[3] = {pAudBuffer, pAudEnc, pAudMux ? pAudMux : pMux};
            HRESULT hr2 = BuildCapture(pAudCapPin, pBF, MEDIATYPE_Audio, &m_wndCaptureBar.m_capdlg.m_mtca);
            UNREFERENCED_PARAMETER(hr2);
        }
    }

    if ((pVidCap || pAudCap) && fCapture && fFileOutput) {
        if (pMux != pDst) {
            hr = pGB->AddFilter(pDst, L"File Writer V/A");
            hr = pGB->ConnectFilter(GetFirstPin(pMux, PINDIR_OUTPUT), pDst);
        }

        if (CComQIPtr<IConfigAviMux> pCAM = pMux) {
            int nIn, nOut, nInC, nOutC;
            CountPins(pMux, nIn, nOut, nInC, nOutC);
            pCAM->SetMasterStream(nInC - 1);
            //pCAM->SetMasterStream(-1);
            pCAM->SetOutputCompatibilityIndex(FALSE);
        }

        if (CComQIPtr<IConfigInterleaving> pCI = pMux) {
            //if (FAILED(pCI->put_Mode(INTERLEAVE_CAPTURE)))
            if (FAILED(pCI->put_Mode(INTERLEAVE_NONE_BUFFERED))) {
                pCI->put_Mode(INTERLEAVE_NONE);
            }

            REFERENCE_TIME rtInterleave = 10000i64 * AUDIOBUFFERLEN, rtPreroll = 0; //10000i64*500
            pCI->put_Interleaving(&rtInterleave, &rtPreroll);
        }

        if (pMux != pAudMux && pAudMux != pAudDst) {
            hr = pGB->AddFilter(pAudDst, L"File Writer A");
            hr = pGB->ConnectFilter(GetFirstPin(pAudMux, PINDIR_OUTPUT), pAudDst);
        }
    }

    REFERENCE_TIME stop = MAX_TIME;
    hr = pCGB->ControlStream(&PIN_CATEGORY_CAPTURE, NULL, NULL, NULL, &stop, 0, 0); // stop in the infinite

    CleanGraph();

    OpenSetupVideo();
    OpenSetupAudio();
    OpenSetupStatsBar();
    OpenSetupStatusBar();

    if (m_iMediaLoadState == MLS_LOADED) {
        SeekTo(__rt);

        if (__fs == State_Stopped) {
            SendMessage(WM_COMMAND, ID_PLAY_STOP);
        } else if (__fs == State_Paused) {
            SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
        } else if (__fs == State_Running) {
            SendMessage(WM_COMMAND, ID_PLAY_PLAY);
        }
    }

    return true;
}

bool CMainFrame::StartCapture()
{
    if (!pCGB || m_fCapturing) {
        return false;
    }

    if (!m_wndCaptureBar.m_capdlg.m_pMux && !m_wndCaptureBar.m_capdlg.m_pDst) {
        return false;
    }

    HRESULT hr;

    ::SetPriorityClass(::GetCurrentProcess(), HIGH_PRIORITY_CLASS);

    // rare to see two capture filters to support IAMPushSource at the same time...
    //hr = CComQIPtr<IAMGraphStreams>(pGB)->SyncUsingStreamOffset(TRUE); // TODO:

    BuildGraphVideoAudio(
        m_wndCaptureBar.m_capdlg.m_fVidPreview, true,
        m_wndCaptureBar.m_capdlg.m_fAudPreview, true);

    hr = pME->CancelDefaultHandling(EC_REPAINT);
    SendMessage(WM_COMMAND, ID_PLAY_PLAY);
    m_fCapturing = true;

    return true;
}

bool CMainFrame::StopCapture()
{
    if (!pCGB || !m_fCapturing) {
        return false;
    }

    if (!m_wndCaptureBar.m_capdlg.m_pMux && !m_wndCaptureBar.m_capdlg.m_pDst) {
        return false;
    }

    m_wndStatusBar.SetStatusMessage(ResStr(IDS_CONTROLS_COMPLETING));
    m_fCapturing = false;

    BuildGraphVideoAudio(
        m_wndCaptureBar.m_capdlg.m_fVidPreview, false,
        m_wndCaptureBar.m_capdlg.m_fAudPreview, false);

    pME->RestoreDefaultHandling(EC_REPAINT);

    ::SetPriorityClass(::GetCurrentProcess(), AfxGetAppSettings().dwPriority);

    m_rtDurationOverride = -1;

    return true;
}

//

void CMainFrame::ShowOptions(int idPage)
{
    // Disable the options dialog when using D3D fullscreen
    if (IsD3DFullScreenMode()) {
        return;
    }

    CAppSettings& s = AfxGetAppSettings();

    CPPageSheet options(ResStr(IDS_OPTIONS_CAPTION), pGB, GetModalParent(), idPage);
    m_bInOptions = true;

    if (options.DoModal() == IDOK) {
        m_bInOptions = false;
        if (!m_fFullScreen) {
            SetAlwaysOnTop(s.iOnTop);
        }

        m_wndToolBar.m_volctrl.SetPageSize(s.nVolumeStep);
        m_wndView.LoadLogo();
        s.SaveSettings();

    }
    Invalidate();
    m_bInOptions = false;
}

void CMainFrame::StartWebServer(int nPort)
{
    if (!m_pWebServer) {
        m_pWebServer.Attach(DEBUG_NEW CWebServer(this, nPort));
    }
}

void CMainFrame::StopWebServer()
{
    if (m_pWebServer) {
        m_pWebServer.Free();
    }
}

CString CMainFrame::GetStatusMessage() const
{
    CString str;
    m_wndStatusBar.m_status.GetWindowText(str);
    return str;
}

void CMainFrame::SendStatusMessage(CString msg, int nTimeOut)
{
    KillTimer(TIMER_STATUSERASER);

    m_playingmsg.Empty();
    if (nTimeOut <= 0) {
        return;
    }

    m_playingmsg = msg;
    SetTimer(TIMER_STATUSERASER, nTimeOut, NULL);
    m_Lcd.SetStatusMessage(msg, nTimeOut);
}

void CMainFrame::OpenCurPlaylistItem(REFERENCE_TIME rtStart)
{
    if (m_wndPlaylistBar.GetCount() == 0) {
        return;
    }

    CPlaylistItem pli;
    if (!m_wndPlaylistBar.GetCur(pli)) {
        m_wndPlaylistBar.SetFirstSelected();
    }
    if (!m_wndPlaylistBar.GetCur(pli)) {
        return;
    }

    CAutoPtr<OpenMediaData> p(m_wndPlaylistBar.GetCurOMD(rtStart));
    if (p) {
        OpenMedia(p);
    }
}

void CMainFrame::AddCurDevToPlaylist()
{
    if (GetPlaybackMode() == PM_CAPTURE) {
        m_wndPlaylistBar.Append(
            m_VidDispName,
            m_AudDispName,
            m_wndCaptureBar.m_capdlg.GetVideoInput(),
            m_wndCaptureBar.m_capdlg.GetVideoChannel(),
            m_wndCaptureBar.m_capdlg.GetAudioInput()
        );
    }
}

void CMainFrame::OpenMedia(CAutoPtr<OpenMediaData> pOMD)
{
    // shortcut
    if (OpenDeviceData* p = dynamic_cast<OpenDeviceData*>(pOMD.m_p)) {
        if (m_iMediaLoadState == MLS_LOADED && pAMTuner
                && m_VidDispName == p->DisplayName[0] && m_AudDispName == p->DisplayName[1]) {
            m_wndCaptureBar.m_capdlg.SetVideoInput(p->vinput);
            m_wndCaptureBar.m_capdlg.SetVideoChannel(p->vchannel);
            m_wndCaptureBar.m_capdlg.SetAudioInput(p->ainput);
            SendNowPlayingToMSN();
            return;
        }
    }

    if (m_iMediaLoadState != MLS_CLOSED) {
        CloseMedia();
    }

    //m_iMediaLoadState = MLS_LOADING; // HACK: hides the logo

    const CAppSettings& s = AfxGetAppSettings();

    bool fUseThread = m_pGraphThread && AfxGetAppSettings().fEnableWorkerThreadForOpening
                      // don't use a worker thread in D3DFullscreen mode except madVR
                      && (!AfxGetAppSettings().IsD3DFullscreen() || AfxGetAppSettings().iDSVideoRendererType == VIDRNDT_DS_MADVR);

    if (OpenFileData* p = dynamic_cast<OpenFileData*>(pOMD.m_p)) {
        if (p->fns.GetCount() > 0) {
            engine_t e = s.m_Formats.GetEngine(p->fns.GetHead());
            if (e != DirectShow /*&& e != RealMedia && e != QuickTime*/) {
                fUseThread = false;
            }
        }
    } else if (OpenDeviceData* p = dynamic_cast<OpenDeviceData*>(pOMD.m_p)) {
        fUseThread = false;
    }

    if (fUseThread) {
        m_pGraphThread->PostThreadMessage(CGraphThread::TM_OPEN, 0, (LPARAM)pOMD.Detach());
        m_bOpenedThruThread = true;
    } else {
        OpenMediaPrivate(pOMD);
        m_bOpenedThruThread = false;
    }
}

bool CMainFrame::ResetDevice()
{
    if (m_pCAP) {
        return m_pCAP->ResetDevice();
    }
    return true;
}

bool CMainFrame::DisplayChange()
{
    if (m_pCAP) {
        return m_pCAP->DisplayChange();
    }
    return true;
}

void CMainFrame::CloseMedia()
{
    if (m_iMediaLoadState == MLS_CLOSING) {
        TRACE(_T("WARNING: CMainFrame::CloseMedia() called twice or more\n"));
        return;
    }

    int nTimeWaited = 0;

    while (m_iMediaLoadState == MLS_LOADING) {
        m_fOpeningAborted = true;

        if (pGB) {
            pGB->Abort();    // TODO: lock on graph objects somehow, this is not thread safe
        }

        if (nTimeWaited > 5 * 1000 && m_pGraphThread) {
            MessageBeep(MB_ICONEXCLAMATION);
            TRACE(_T("CRITICAL ERROR: !!! Must kill opener thread !!!"));
            TerminateThread(m_pGraphThread->m_hThread, (DWORD) - 1);
            m_pGraphThread = (CGraphThread*)AfxBeginThread(RUNTIME_CLASS(CGraphThread));
            m_bOpenedThruThread = false;
            break;
        }

        Sleep(50);

        nTimeWaited += 50;
    }

    m_fOpeningAborted = false;

    m_closingmsg.Empty();

    SetLoadState(MLS_CLOSING);

    OnFilePostClosemedia();

    if (m_pGraphThread && m_bOpenedThruThread) {
        CAMEvent e;
        m_pGraphThread->PostThreadMessage(CGraphThread::TM_CLOSE, 0, (LPARAM)&e);
        e.Wait(); // either opening or closing has to be blocked to prevent reentering them, closing is the better choice
    } else {
        CloseMediaPrivate();
    }

    UnloadExternalObjects();

    if (m_pFullscreenWnd->IsWindow()) {
        m_pFullscreenWnd->ShowWindow(SW_HIDE);
    }
}

void CMainFrame::StartTunerScan(CAutoPtr<TunerScanData> pTSD)
{
    if (m_pGraphThread) {
        m_pGraphThread->PostThreadMessage(CGraphThread::TM_TUNER_SCAN, 0, (LPARAM)pTSD.Detach());
    } else {
        DoTunerScan(pTSD);
    }
}

void CMainFrame::StopTunerScan()
{
    m_bStopTunerScan = true;
}

void CMainFrame::ShowCurrentChannelInfo(bool fShowOSD /*= true*/, bool fShowInfoBar /*= false*/)
{
    CAppSettings& s = AfxGetAppSettings();
    CDVBChannel* pChannel = s.FindChannelByPref(s.nDVBLastChannel);
    CString description;
    CString osd;
    EventDescriptor NowNext;

    if (pChannel != NULL) {
        // Get EIT information:
        CComQIPtr<IBDATuner> pTun = pGB;
        if (pTun && pChannel->GetNowNextFlag()) {
            pTun->UpdatePSI(NowNext);
            // Set a timer to update the infos
            time_t tNow;
            time(&tNow);
            time_t tElapse = NowNext.duration - (tNow - NowNext.startTime);
            if (tElapse < 0) {
                tElapse = 0;
            }
            // We set a 15s delay to let some room for the program infos to change
            tElapse += 15;
            SetTimer(TIMER_DVBINFO_UPDATER, 1000 * (UINT)tElapse, NULL);
        } else {
            m_wndInfoBar.RemoveAllLines();
            if (fShowOSD) {
                m_OSD.DisplayMessage(OSD_TOPLEFT, pChannel->GetName(), 3500);
            }
            return;
        }

        if (fShowOSD) {
            osd = pChannel->GetName();
            osd += _T(" | ") + NowNext.eventName + _T(" (") + NowNext.strStartTime + _T(" - ") + NowNext.strEndTime + _T(")");
            m_OSD.DisplayMessage(OSD_TOPLEFT, osd , 3500);
        }

        // Update Info Bar
        m_wndInfoBar.RemoveAllLines();
        m_wndInfoBar.SetLine(ResStr(IDS_INFOBAR_CHANNEL), pChannel->GetName());
        m_wndInfoBar.SetLine(ResStr(IDS_INFOBAR_TITLE), NowNext.eventName);
        m_wndInfoBar.SetLine(ResStr(IDS_INFOBAR_TIME), NowNext.strStartTime + _T(" - ") + NowNext.strEndTime);

        for (int i = 0; i < NowNext.extendedDescriptorsItemsDesc.GetCount(); i++) {
            m_wndInfoBar.SetLine(NowNext.extendedDescriptorsItemsDesc.ElementAt(i), NowNext.extendedDescriptorsItemsContent.ElementAt(i));
        }

        description = NowNext.eventDesc;
        if (!NowNext.extendedDescriptorsTexts.IsEmpty()) {
            if (!description.IsEmpty()) {
                description += _T("; ");
            }
            description += NowNext.extendedDescriptorsTexts.GetTail();
        }
        m_wndInfoBar.SetLine(ResStr(IDS_INFOBAR_DESCRIPTION), description);

        RecalcLayout();

        if (fShowInfoBar) {
            ShowControls(m_nCS | CS_INFOBAR, true);
        }
    }
}

// ==== Added by CASIMIR666
void CMainFrame::SetLoadState(MPC_LOADSTATE iState)
{
    m_iMediaLoadState = iState;
    SendAPICommand(CMD_STATE, L"%d", iState);
}

void CMainFrame::SetPlayState(MPC_PLAYSTATE iState)
{
    m_Lcd.SetPlayState((CMPC_Lcd::PlayState)iState);
    SendAPICommand(CMD_PLAYMODE, L"%d", iState);

    if (m_fEndOfStream) {
        SendAPICommand(CMD_NOTIFYENDOFSTREAM, L"\0");     // do not pass NULL here!
    }

    // Prevent sleep when playing audio and/or video, but allow screensaver when only audio
    if (!m_fAudioOnly) {
        SetThreadExecutionState(iState == PS_PLAY ? ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED : ES_CONTINUOUS);
    } else {
        SetThreadExecutionState(iState == PS_PLAY ? ES_CONTINUOUS | ES_SYSTEM_REQUIRED : ES_CONTINUOUS);
    }
}

bool CMainFrame::CreateFullScreenWindow()
{
    HMONITOR      hMonitor;
    MONITORINFOEX MonitorInfo;
    CRect         MonitorRect;

    if (m_pFullscreenWnd->IsWindow()) {
        m_pFullscreenWnd->DestroyWindow();
    }

    ZeroMemory(&MonitorInfo, sizeof(MonitorInfo));
    MonitorInfo.cbSize  = sizeof(MonitorInfo);

    CMonitors monitors;
    CString str;
    CMonitor monitor;
    const CAppSettings& s = AfxGetAppSettings();
    hMonitor = NULL;

    if (!s.iMonitor) {
        if (s.strFullScreenMonitor == _T("Current")) {
            hMonitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
        } else {
            for (int i = 0; i < monitors.GetCount(); i++) {
                monitor = monitors.GetMonitor(i);
                monitor.GetName(str);

                if ((monitor.IsMonitor()) && (s.strFullScreenMonitor == str)) {
                    hMonitor = monitor;
                    break;
                }
            }
            if (!hMonitor) {
                hMonitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
            }
        }
    } else {
        hMonitor = MonitorFromWindow(m_hWnd, 0);
    }
    if (GetMonitorInfo(hMonitor, &MonitorInfo)) {
        MonitorRect = CRect(MonitorInfo.rcMonitor);
        // Window creation
        DWORD dwStyle    = WS_POPUP | WS_VISIBLE;
        m_fullWndSize.cx = MonitorRect.Width();
        m_fullWndSize.cy = MonitorRect.Height();

        m_pFullscreenWnd->CreateEx(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, _T(""), ResStr(IDS_MAINFRM_136), dwStyle, MonitorRect.left, MonitorRect.top, MonitorRect.Width(), MonitorRect.Height(), NULL, NULL, NULL);
        //SetWindowLongPtr(m_pFullscreenWnd->m_hWnd, GWL_EXSTYLE, WS_EX_TOPMOST); // TODO : still freezing sometimes...
        /*
        CRect r;
        GetWindowRect(r);

        int x = MonitorRect.left + (MonitorRect.Width()/2)-(r.Width()/2);
        int y = MonitorRect.top + (MonitorRect.Height()/2)-(r.Height()/2);
        int w = r.Width();
        int h = r.Height();
        MoveWindow(x, y, w, h);
        */
    }

    return m_pFullscreenWnd->IsWindow();
}

bool CMainFrame::IsD3DFullScreenMode() const
{
    return m_pFullscreenWnd->IsWindow();
};

void CMainFrame::SetupEVRColorControl()
{
    if (m_pMFVP) {
        if (FAILED(m_pMFVP->GetProcAmpRange(DXVA2_ProcAmp_Brightness, AfxGetMyApp()->GetEVRColorControl(ProcAmp_Brightness)))) {
            return;
        }
        if (FAILED(m_pMFVP->GetProcAmpRange(DXVA2_ProcAmp_Contrast,   AfxGetMyApp()->GetEVRColorControl(ProcAmp_Contrast)))) {
            return;
        }
        if (FAILED(m_pMFVP->GetProcAmpRange(DXVA2_ProcAmp_Hue,        AfxGetMyApp()->GetEVRColorControl(ProcAmp_Hue)))) {
            return;
        }
        if (FAILED(m_pMFVP->GetProcAmpRange(DXVA2_ProcAmp_Saturation, AfxGetMyApp()->GetEVRColorControl(ProcAmp_Saturation)))) {
            return;
        }

        AfxGetMyApp()->UpdateColorControlRange(true);
        SetColorControl(ProcAmp_All, AfxGetAppSettings().iBrightness, AfxGetAppSettings().iContrast, AfxGetAppSettings().iHue, AfxGetAppSettings().iSaturation);
    }
}

void CMainFrame::SetupVMR9ColorControl()
{
    if (m_pMC) {
        if (FAILED(m_pMC->GetProcAmpControlRange(0, AfxGetMyApp()->GetVMR9ColorControl(ProcAmp_Brightness)))) {
            return;
        }
        if (FAILED(m_pMC->GetProcAmpControlRange(0, AfxGetMyApp()->GetVMR9ColorControl(ProcAmp_Contrast)))) {
            return;
        }
        if (FAILED(m_pMC->GetProcAmpControlRange(0, AfxGetMyApp()->GetVMR9ColorControl(ProcAmp_Hue)))) {
            return;
        }
        if (FAILED(m_pMC->GetProcAmpControlRange(0, AfxGetMyApp()->GetVMR9ColorControl(ProcAmp_Saturation)))) {
            return;
        }

        AfxGetMyApp()->UpdateColorControlRange(false);
        SetColorControl(ProcAmp_All, AfxGetAppSettings().iBrightness, AfxGetAppSettings().iContrast, AfxGetAppSettings().iHue, AfxGetAppSettings().iSaturation);
    }
}

void CMainFrame::SetColorControl(DWORD flags, int& brightness, int& contrast, int& hue, int& saturation)
{
    static VMR9ProcAmpControl  ClrControl;
    static DXVA2_ProcAmpValues ClrValues;

    COLORPROPERTY_RANGE* cr;
    if (flags & ProcAmp_Brightness) {
        cr = AfxGetMyApp()->GetColorControl(ProcAmp_Brightness);
        brightness = min(max(brightness, cr->MinValue), cr->MaxValue);
    }
    if (flags & ProcAmp_Contrast) {
        cr = AfxGetMyApp()->GetColorControl(ProcAmp_Contrast);
        contrast = min(max(contrast, cr->MinValue), cr->MaxValue);
    }
    if (flags & ProcAmp_Hue) {
        cr = AfxGetMyApp()->GetColorControl(ProcAmp_Hue);
        hue = min(max(hue, cr->MinValue), cr->MaxValue);
    }
    if (flags & ProcAmp_Saturation) {
        cr = AfxGetMyApp()->GetColorControl(ProcAmp_Saturation);
        saturation = min(max(saturation, cr->MinValue), cr->MaxValue);
    }


    if (m_pMC) {
        ClrControl.dwSize     = sizeof(ClrControl);
        ClrControl.dwFlags    = flags;
        ClrControl.Brightness = (float)brightness;
        ClrControl.Contrast   = (float)(contrast + 100) / 100;
        ClrControl.Hue        = (float)hue;
        ClrControl.Saturation = (float)(saturation + 100) / 100;

        m_pMC->SetProcAmpControl(0, &ClrControl);
    } else if (m_pMFVP) {
        ClrValues.Brightness = IntToFixed(brightness);
        ClrValues.Contrast   = IntToFixed(contrast + 100, 100);
        ClrValues.Hue        = IntToFixed(hue);
        ClrValues.Saturation = IntToFixed(saturation + 100, 100);

        m_pMFVP->SetProcAmpValues(flags, &ClrValues);
    }
}

void CMainFrame::SetClosedCaptions(bool enable)
{
    if (m_pLN21) {
        m_pLN21->SetServiceState(enable ? AM_L21_CCSTATE_On : AM_L21_CCSTATE_Off);
    }
}


LPCTSTR CMainFrame::GetDVDAudioFormatName(DVD_AudioAttributes& ATR) const
{
    switch (ATR.AudioFormat) {
        case DVD_AudioFormat_AC3:
            return _T("AC3");
        case DVD_AudioFormat_MPEG1:
        case DVD_AudioFormat_MPEG1_DRC:
            return _T("MPEG1");
        case DVD_AudioFormat_MPEG2:
        case DVD_AudioFormat_MPEG2_DRC:
            return _T("MPEG2");
        case DVD_AudioFormat_LPCM:
            return _T("LPCM");
        case DVD_AudioFormat_DTS:
            return _T("DTS");
        case DVD_AudioFormat_SDDS:
            return _T("SDDS");
        case DVD_AudioFormat_Other:
        default:
            return MAKEINTRESOURCE(IDS_MAINFRM_137);
    }
}

afx_msg void CMainFrame::OnGotoSubtitle(UINT nID)
{
    OnPlayPause();
    m_rtCurSubPos = m_wndSeekBar.GetPosReal();
    m_lSubtitleShift = 0;
    m_nCurSubtitle = m_wndSubresyncBar.FindNearestSub(m_rtCurSubPos, (nID == ID_GOTO_NEXT_SUB));
    if ((m_nCurSubtitle != -1) && pMS) {
        pMS->SetPositions(&m_rtCurSubPos, AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);
    }
}

afx_msg void CMainFrame::OnShiftSubtitle(UINT nID)
{
    if (m_nCurSubtitle >= 0) {
        long lShift = (nID == ID_SHIFT_SUB_DOWN) ? -100 : 100;
        CString strSubShift;

        if (m_wndSubresyncBar.ShiftSubtitle(m_nCurSubtitle, lShift, m_rtCurSubPos)) {
            m_lSubtitleShift += lShift;
            if (pMS) {
                pMS->SetPositions(&m_rtCurSubPos, AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);
            }
        }

        strSubShift.Format(IDS_MAINFRM_138, m_lSubtitleShift);
        m_OSD.DisplayMessage(OSD_TOPLEFT, strSubShift);
    }
}

afx_msg void CMainFrame::OnSubtitleDelay(UINT nID)
{
    if (m_pCAP) {
        if (m_pSubStreams.IsEmpty()) {
            SendStatusMessage(ResStr(IDS_SUBTITLES_ERROR), 3000);
            return;
        }

        int newDelay;
        int oldDelay = m_pCAP->GetSubtitleDelay();

        if (nID == ID_SUB_DELAY_DOWN) {
            newDelay = oldDelay - AfxGetAppSettings().nSubDelayInterval;
        } else {
            newDelay = oldDelay + AfxGetAppSettings().nSubDelayInterval;
        }

        SetSubtitleDelay(newDelay);
    }
}

afx_msg void CMainFrame::OnLanguage(UINT nID)
{
    CMenu  DefaultMenu;
    CMenu* OldMenu;

    if (nID == ID_LANGUAGE_HEBREW) { // Show a warning when switching to Hebrew (must not be translated)
        MessageBox(_T("The Hebrew translation will be correctly displayed (with a right-to-left layout) after restarting the application.\n"),
                   _T("Media Player Classic - Home Cinema"), MB_ICONINFORMATION | MB_OK);
    }

    CMPlayerCApp::SetLanguage(CMPlayerCApp::GetLanguageResourceByResourceID(nID));

    m_opencds.DestroyMenu();
    m_filters.DestroyMenu();
    m_subtitles.DestroyMenu();
    m_audios.DestroyMenu();
    m_navaudio.DestroyMenu();
    m_navsubtitle.DestroyMenu();
    m_navangle.DestroyMenu();
    m_navchapters.DestroyMenu();
    m_favorites.DestroyMenu();
    m_shaders.DestroyMenu();
    m_recentfiles.DestroyMenu();

    m_popup.DestroyMenu();
    m_popup.LoadMenu(IDR_POPUP);
    m_popupmain.DestroyMenu();
    m_popupmain.LoadMenu(IDR_POPUPMAIN);

    OldMenu = GetMenu();
    DefaultMenu.LoadMenu(IDR_MAINFRAME);

    SetMenu(&DefaultMenu);
    m_hMenuDefault = DefaultMenu;
    DefaultMenu.Detach();
    // TODO : destroy old menu ???
    //OldMenu->DestroyMenu();
}

void CMainFrame::ProcessAPICommand(COPYDATASTRUCT* pCDS)
{
    CAtlList<CString> fns;
    REFERENCE_TIME rtPos = 0;

    switch (pCDS->dwData) {
        case CMD_OPENFILE:
            fns.AddHead((LPCWSTR)pCDS->lpData);
            m_wndPlaylistBar.Open(fns, false);
            OpenCurPlaylistItem();
            break;
        case CMD_STOP:
            OnPlayStop();
            break;
        case CMD_CLOSEFILE:
            CloseMedia();
            break;
        case CMD_PLAYPAUSE:
            OnPlayPlaypause();
            break;
        case CMD_PLAY:
            OnApiPlay();
            break;
        case CMD_PAUSE:
            OnApiPause();
            break;
        case CMD_ADDTOPLAYLIST:
            fns.AddHead((LPCWSTR)pCDS->lpData);
            m_wndPlaylistBar.Append(fns, true);
            break;
        case CMD_STARTPLAYLIST:
            OpenCurPlaylistItem();
            break;
        case CMD_CLEARPLAYLIST:
            m_wndPlaylistBar.Empty();
            break;
        case CMD_SETPOSITION:
            rtPos = 10000 * REFERENCE_TIME(_wtof((LPCWSTR)pCDS->lpData) * 1000); //with accuracy of 1 ms
            // imianz: quick and dirty trick
            // Pause->SeekTo->Play (in place of SeekTo only) seems to prevents in most cases
            // some strange video effects on avi files (ex. locks a while and than running fast).
            if (!m_fAudioOnly && GetMediaState() == State_Running) {
                SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
                SeekTo(rtPos);
                SendMessage(WM_COMMAND, ID_PLAY_PLAY);
            } else {
                SeekTo(rtPos);
            }
            // show current position overridden by play command
            m_OSD.DisplayMessage(OSD_TOPLEFT, m_wndStatusBar.GetStatusTimer(), 2000);
            break;
        case CMD_SETAUDIODELAY:
            rtPos = _wtol((LPCWSTR)pCDS->lpData) * 10000;
            SetAudioDelay(rtPos);
            break;
        case CMD_SETSUBTITLEDELAY:
            SetSubtitleDelay(_wtoi((LPCWSTR)pCDS->lpData));
            break;
        case CMD_SETINDEXPLAYLIST:
            //m_wndPlaylistBar.SetSelIdx(_wtoi((LPCWSTR)pCDS->lpData));
            break;
        case CMD_SETAUDIOTRACK:
            SetAudioTrackIdx(_wtoi((LPCWSTR)pCDS->lpData));
            break;
        case CMD_SETSUBTITLETRACK:
            SetSubtitleTrackIdx(_wtoi((LPCWSTR)pCDS->lpData));
            break;
        case CMD_GETVERSION: {
            CStringW buff = AfxGetMyApp()->m_strVersion;
            SendAPICommand(CMD_VERSION, buff);
            break;
        }
        case CMD_GETSUBTITLETRACKS:
            SendSubtitleTracksToApi();
            break;
        case CMD_GETAUDIOTRACKS:
            SendAudioTracksToApi();
            break;
        case CMD_GETCURRENTPOSITION:
            SendCurrentPositionToApi();
            break;
        case CMD_GETNOWPLAYING:
            SendNowPlayingToApi();
            break;
        case CMD_JUMPOFNSECONDS:
            JumpOfNSeconds(_wtoi((LPCWSTR)pCDS->lpData));
            break;
        case CMD_GETPLAYLIST:
            SendPlaylistToApi();
            break;
        case CMD_JUMPFORWARDMED:
            OnPlaySeek(ID_PLAY_SEEKFORWARDMED);
            break;
        case CMD_JUMPBACKWARDMED:
            OnPlaySeek(ID_PLAY_SEEKBACKWARDMED);
            break;
        case CMD_TOGGLEFULLSCREEN:
            OnViewFullscreen();
            break;
        case CMD_INCREASEVOLUME:
            m_wndToolBar.m_volctrl.IncreaseVolume();
            break;
        case CMD_DECREASEVOLUME:
            m_wndToolBar.m_volctrl.DecreaseVolume();
            break;
        case CMD_SHADER_TOGGLE:
            OnShaderToggle();
            break;
        case CMD_CLOSEAPP:
            PostMessage(WM_CLOSE);
            break;
        case CMD_SETSPEED:
            SetPlayingRate(_wtof((LPCWSTR)pCDS->lpData));
            break;
        case CMD_OSDSHOWMESSAGE:
            ShowOSDCustomMessageApi((MPC_OSDDATA*)pCDS->lpData);
            break;
    }
}

void CMainFrame::SendAPICommand(MPCAPI_COMMAND nCommand, LPCWSTR fmt, ...)
{
    const CAppSettings& s = AfxGetAppSettings();

    if (s.hMasterWnd) {
        COPYDATASTRUCT CDS;

        va_list args;
        va_start(args, fmt);

        int nBufferLen = _vsctprintf(fmt, args) + 1; // _vsctprintf doesn't count the null terminator
        TCHAR* pBuff = DEBUG_NEW TCHAR[nBufferLen];
        _vstprintf_s(pBuff, nBufferLen, fmt, args);

        CDS.cbData = (DWORD)nBufferLen * sizeof(TCHAR);
        CDS.dwData = nCommand;
        CDS.lpData = (LPVOID)pBuff;

        ::SendMessage(s.hMasterWnd, WM_COPYDATA, (WPARAM)GetSafeHwnd(), (LPARAM)&CDS);

        va_end(args);
        delete [] pBuff;
    }
}

void CMainFrame::SendNowPlayingToApi()
{
    if (!AfxGetAppSettings().hMasterWnd) {
        return;
    }


    if (m_iMediaLoadState == MLS_LOADED) {
        CPlaylistItem pli;
        CString title, author, description;
        CString label;
        CString strDur;

        if (GetPlaybackMode() == PM_FILE) {
            m_wndInfoBar.GetLine(ResStr(IDS_INFOBAR_TITLE), title);
            m_wndInfoBar.GetLine(ResStr(IDS_INFOBAR_AUTHOR), author);
            m_wndInfoBar.GetLine(ResStr(IDS_INFOBAR_DESCRIPTION), description);

            m_wndPlaylistBar.GetCur(pli);
            if (!pli.m_fns.IsEmpty()) {
                label = !pli.m_label.IsEmpty() ? pli.m_label : pli.m_fns.GetHead();
                REFERENCE_TIME rtDur;
                pMS->GetDuration(&rtDur);
                strDur.Format(L"%.3f", rtDur / 10000000.0);
            }
        } else if (GetPlaybackMode() == PM_DVD) {
            DVD_DOMAIN DVDDomain;
            ULONG ulNumOfChapters = 0;
            DVD_PLAYBACK_LOCATION2 Location;

            // Get current DVD Domain
            if (SUCCEEDED(pDVDI->GetCurrentDomain(&DVDDomain))) {
                switch (DVDDomain) {
                    case DVD_DOMAIN_Stop:
                        title = _T("DVD - Stopped");
                        break;
                    case DVD_DOMAIN_FirstPlay:
                        title = _T("DVD - FirstPlay");
                        break;
                    case DVD_DOMAIN_VideoManagerMenu:
                        title = _T("DVD - RootMenu");
                        break;
                    case DVD_DOMAIN_VideoTitleSetMenu:
                        title = _T("DVD - TitleMenu");
                        break;
                    case DVD_DOMAIN_Title:
                        title = _T("DVD - Title");
                        break;
                }

                // get title information
                if (DVDDomain == DVD_DOMAIN_Title) {
                    // get current location (title number & chapter)
                    if (SUCCEEDED(pDVDI->GetCurrentLocation(&Location))) {
                        // get number of chapters in current title
                        pDVDI->GetNumberOfChapters(Location.TitleNum, &ulNumOfChapters);
                    }

                    // get total time of title
                    DVD_HMSF_TIMECODE tcDur;
                    ULONG ulFlags;
                    if (SUCCEEDED(pDVDI->GetTotalTitleTime(&tcDur, &ulFlags))) {
                        // calculate duration in seconds
                        strDur.Format(L"%u", tcDur.bHours * 60 * 60 + tcDur.bMinutes * 60 + tcDur.bSeconds);
                    }

                    // build string
                    // DVD - xxxxx|currenttitle|numberofchapters|currentchapter|titleduration
                    author.Format(L"%d", Location.TitleNum);
                    description.Format(L"%d", ulNumOfChapters);
                    label.Format(L"%d", Location.ChapterNum);
                }
            }
        }

        title.Replace(L"|", L"\\|");
        author.Replace(L"|", L"\\|");
        description.Replace(L"|", L"\\|");
        label.Replace(L"|", L"\\|");

        CStringW buff;
        buff.Format(L"%s|%s|%s|%s|%s", title, author, description, label, strDur);

        SendAPICommand(CMD_NOWPLAYING, buff);
        SendSubtitleTracksToApi();
        SendAudioTracksToApi();
    }
}

void CMainFrame::SendSubtitleTracksToApi()
{
    CStringW strSubs;
    POSITION pos = m_pSubStreams.GetHeadPosition();

    if (m_iMediaLoadState == MLS_LOADED) {
        if (pos) {
            while (pos) {
                CComPtr<ISubStream> pSubStream = m_pSubStreams.GetNext(pos).subStream;

                for (int i = 0, j = pSubStream->GetStreamCount(); i < j; i++) {
                    WCHAR* pName = NULL;
                    if (SUCCEEDED(pSubStream->GetStreamInfo(i, &pName, NULL))) {
                        CString name(pName);
                        if (!strSubs.IsEmpty()) {
                            strSubs.Append(L"|");
                        }
                        name.Replace(L"|", L"\\|");
                        strSubs.AppendFormat(L"%s", name);
                        CoTaskMemFree(pName);
                    }
                }
            }
            if (AfxGetAppSettings().fEnableSubtitles) {
                if (m_iSubtitleSel >= 0) {
                    strSubs.AppendFormat(L"|%i", m_iSubtitleSel);
                } else {
                    strSubs.Append(L"|-1");
                }
            } else {
                strSubs.Append(L"|-1");
            }
        } else {
            strSubs.Append(L"-1");
        }
    } else {
        strSubs.Append(L"-2");
    }
    SendAPICommand(CMD_LISTSUBTITLETRACKS, strSubs);
}

void CMainFrame::SendAudioTracksToApi()
{
    CStringW strAudios;

    if (m_iMediaLoadState == MLS_LOADED) {
        CComQIPtr<IAMStreamSelect> pSS = FindFilter(__uuidof(CAudioSwitcherFilter), pGB);
        if (!pSS) {
            pSS = FindFilter(CLSID_MorganStreamSwitcher, pGB);
        }

        DWORD cStreams = 0;
        if (pSS && SUCCEEDED(pSS->Count(&cStreams))) {
            int currentStream = -1;
            for (int i = 0; i < (int)cStreams; i++) {
                AM_MEDIA_TYPE* pmt = NULL;
                DWORD dwFlags = 0;
                LCID lcid = 0;
                DWORD dwGroup = 0;
                WCHAR* pszName = NULL;
                if (FAILED(pSS->Info(i, &pmt, &dwFlags, &lcid, &dwGroup, &pszName, NULL, NULL))) {
                    return;
                }
                if (dwFlags == AMSTREAMSELECTINFO_EXCLUSIVE) {
                    currentStream = i;
                }
                CString name(pszName);
                if (!strAudios.IsEmpty()) {
                    strAudios.Append(L"|");
                }
                name.Replace(L"|", L"\\|");
                strAudios.AppendFormat(L"%s", name);
                if (pmt) {
                    DeleteMediaType(pmt);
                }
                if (pszName) {
                    CoTaskMemFree(pszName);
                }
            }
            strAudios.AppendFormat(L"|%i", currentStream);

        } else {
            strAudios.Append(L"-1");
        }
    } else {
        strAudios.Append(L"-2");
    }
    SendAPICommand(CMD_LISTAUDIOTRACKS, strAudios);

}

void CMainFrame::SendPlaylistToApi()
{
    CStringW strPlaylist;
    int index;
    POSITION pos = m_wndPlaylistBar.m_pl.GetHeadPosition(), pos2;

    while (pos) {
        CPlaylistItem& pli = m_wndPlaylistBar.m_pl.GetNext(pos);

        if (pli.m_type == CPlaylistItem::file) {
            pos2 = pli.m_fns.GetHeadPosition();
            while (pos2) {
                CString fn = pli.m_fns.GetNext(pos2);
                if (!strPlaylist.IsEmpty()) {
                    strPlaylist.Append(L"|");
                }
                fn.Replace(L"|", L"\\|");
                strPlaylist.AppendFormat(L"%s", fn);
            }
        }
    }
    index = m_wndPlaylistBar.GetSelIdx();
    if (strPlaylist.IsEmpty()) {
        strPlaylist.Append(L"-1");
    } else {
        strPlaylist.AppendFormat(L"|%i", index);
    }
    SendAPICommand(CMD_PLAYLIST, strPlaylist);
}

void CMainFrame::SendCurrentPositionToApi(bool fNotifySeek)
{
    if (!AfxGetAppSettings().hMasterWnd) {
        return;
    }

    if (m_iMediaLoadState == MLS_LOADED) {
        CStringW strPos;

        if (GetPlaybackMode() == PM_FILE) {
            REFERENCE_TIME rtCur;
            pMS->GetCurrentPosition(&rtCur);
            strPos.Format(L"%.3f", rtCur / 10000000.0);
        } else if (GetPlaybackMode() == PM_DVD) {
            DVD_PLAYBACK_LOCATION2 Location;
            // get current location while playing disc, will return 0, if at a menu
            if (pDVDI->GetCurrentLocation(&Location) == S_OK) {
                strPos.Format(L"%u", Location.TimeCode.bHours * 60 * 60 + Location.TimeCode.bMinutes * 60 + Location.TimeCode.bSeconds);
            }
        }

        SendAPICommand(fNotifySeek ? CMD_NOTIFYSEEK : CMD_CURRENTPOSITION, strPos);
    }
}

void CMainFrame::ShowOSDCustomMessageApi(MPC_OSDDATA* osdData)
{
    m_OSD.DisplayMessage((OSD_MESSAGEPOS)osdData->nMsgPos, osdData->strMsg, osdData->nDurationMS);
}

void CMainFrame::JumpOfNSeconds(int nSeconds)
{
    if (m_iMediaLoadState == MLS_LOADED) {
        REFERENCE_TIME rtCur;

        if (GetPlaybackMode() == PM_FILE) {
            pMS->GetCurrentPosition(&rtCur);
            DVD_HMSF_TIMECODE tcCur = RT2HMSF(rtCur);
            long lPosition = tcCur.bHours * 60 * 60 + tcCur.bMinutes * 60 + tcCur.bSeconds + nSeconds;

            // revert the update position to REFERENCE_TIME format
            tcCur.bHours   = (BYTE)(lPosition / 3600);
            tcCur.bMinutes = (lPosition / 60) % 60;
            tcCur.bSeconds = lPosition % 60;
            rtCur = HMSF2RT(tcCur);

            // quick and dirty trick:
            // pause->seekto->play seems to prevents some strange
            // video effect (ex. locks for a while and than running fast)
            if (!m_fAudioOnly) {
                SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
            }
            SeekTo(rtCur);
            if (!m_fAudioOnly) {
                SendMessage(WM_COMMAND, ID_PLAY_PLAY);
                // show current position overridden by play command
                m_OSD.DisplayMessage(OSD_TOPLEFT, m_wndStatusBar.GetStatusTimer(), 2000);
            }
        }
    }
}


// TODO : to be finished !
//void CMainFrame::AutoSelectTracks()
//{
//  LCID DefAudioLanguageLcid    [2] = {MAKELCID( MAKELANGID(LANG_FRENCH, SUBLANG_DEFAULT), SORT_DEFAULT), MAKELCID( MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), SORT_DEFAULT)};
//  int  DefAudioLanguageIndex   [2] = {-1, -1};
//  LCID DefSubtitleLanguageLcid [2] = {0, MAKELCID( MAKELANGID(LANG_FRENCH, SUBLANG_DEFAULT), SORT_DEFAULT)};
//  int  DefSubtitleLanguageIndex[2] = {-1, -1};
//  LCID Language = MAKELCID(MAKELANGID(LANG_FRENCH, SUBLANG_DEFAULT), SORT_DEFAULT);
//
//  if ((m_iMediaLoadState == MLS_LOADING) || (m_iMediaLoadState == MLS_LOADED))
//  {
//      if (GetPlaybackMode() == PM_FILE)
//      {
//          CComQIPtr<IAMStreamSelect> pSS = FindFilter(__uuidof(CAudioSwitcherFilter), pGB);
//          if (!pSS) pSS = FindFilter(CLSID_MorganStreamSwitcher, pGB);
//
//          DWORD cStreams = 0;
//          if (pSS && SUCCEEDED(pSS->Count(&cStreams)))
//          {
//              for (int i = 0; i < (int)cStreams; i++)
//              {
//                  AM_MEDIA_TYPE* pmt = NULL;
//                  DWORD dwFlags = 0;
//                  LCID lcid = 0;
//                  DWORD dwGroup = 0;
//                  WCHAR* pszName = NULL;
//                  if (FAILED(pSS->Info(i, &pmt, &dwFlags, &lcid, &dwGroup, &pszName, NULL, NULL)))
//                      return;
//              }
//          }
//
//          POSITION pos = m_pSubStreams.GetHeadPosition();
//          while (pos)
//          {
//              CComPtr<ISubStream> pSubStream = m_pSubStreams.GetNext(pos).subStream;
//              if (!pSubStream) continue;
//
//              for (int i = 0, j = pSubStream->GetStreamCount(); i < j; i++)
//              {
//                  WCHAR* pName = NULL;
//                  if (SUCCEEDED(pSubStream->GetStreamInfo(i, &pName, &Language)))
//                  {
//                      if (DefAudioLanguageLcid[0] == Language)    DefSubtitleLanguageIndex[0] = i;
//                      if (DefSubtitleLanguageLcid[1] == Language) DefSubtitleLanguageIndex[1] = i;
//                      CoTaskMemFree(pName);
//                  }
//              }
//          }
//      }
//      else if (GetPlaybackMode() == PM_DVD)
//      {
//          ULONG ulStreamsAvailable, ulCurrentStream;
//          BOOL  bIsDisabled;
//
//          if (SUCCEEDED(pDVDI->GetCurrentSubpicture(&ulStreamsAvailable, &ulCurrentStream, &bIsDisabled)))
//          {
//              for (ULONG i = 0; i < ulStreamsAvailable; i++)
//              {
//                  DVD_SubpictureAttributes    ATR;
//                  if (SUCCEEDED(pDVDI->GetSubpictureLanguage(i, &Language)))
//                  {
//                      // Auto select forced subtitle
//                      if ((DefAudioLanguageLcid[0] == Language) && (ATR.LanguageExtension == DVD_SP_EXT_Forced))
//                          DefSubtitleLanguageIndex[0] = i;
//
//                      if (DefSubtitleLanguageLcid[1] == Language) DefSubtitleLanguageIndex[1] = i;
//                  }
//              }
//          }
//
//          if (SUCCEEDED(pDVDI->GetCurrentAudio(&ulStreamsAvailable, &ulCurrentStream)))
//          {
//              for (ULONG i = 0; i < ulStreamsAvailable; i++)
//              {
//                  if (SUCCEEDED(pDVDI->GetAudioLanguage(i, &Language)))
//                  {
//                      if (DefAudioLanguageLcid[0] == Language)    DefAudioLanguageIndex[0] = i;
//                      if (DefAudioLanguageLcid[1] == Language)    DefAudioLanguageIndex[1] = i;
//                  }
//              }
//          }
//
//          // Select best audio/subtitles tracks
//          if (DefAudioLanguageLcid[0] != -1)
//          {
//              pDVDC->SelectAudioStream(DefAudioLanguageIndex[0], DVD_CMD_FLAG_Block, NULL);
//              if (DefSubtitleLanguageIndex[0] != -1)
//                  pDVDC->SelectSubpictureStream(DefSubtitleLanguageIndex[0], DVD_CMD_FLAG_Block, NULL);
//          }
//          else if ((DefAudioLanguageLcid[1] != -1) && (DefSubtitleLanguageLcid[1] != -1))
//          {
//              pDVDC->SelectAudioStream      (DefAudioLanguageIndex[1],    DVD_CMD_FLAG_Block, NULL);
//              pDVDC->SelectSubpictureStream (DefSubtitleLanguageIndex[1], DVD_CMD_FLAG_Block, NULL);
//          }
//      }
//
//
//  }
//}

void CMainFrame::OnFileOpendirectory()
{
    if (m_iMediaLoadState == MLS_LOADING || !IsWindow(m_wndPlaylistBar)) {
        return;
    }

    const CAppSettings& s = AfxGetAppSettings();
    CString strTitle = ResStr(IDS_MAINFRM_DIR_TITLE);
    CString path;

    if (SysVersion::IsVistaOrLater()) {
        CFileDialog dlg(TRUE);
        dlg.AddCheckButton(IDS_MAINFRM_DIR_CHECK, ResStr(IDS_MAINFRM_DIR_CHECK), TRUE);
        IFileOpenDialog* openDlgPtr = dlg.GetIFileOpenDialog();

        if (openDlgPtr != NULL) {
            openDlgPtr->SetTitle(strTitle);
            openDlgPtr->SetOptions(FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
            if (FAILED(openDlgPtr->Show(m_hWnd))) {
                openDlgPtr->Release();
                return;
            }
            openDlgPtr->Release();

            path = dlg.GetFolderPath();

            BOOL recur = TRUE;
            dlg.GetCheckButtonState(IDS_MAINFRM_DIR_CHECK, recur);
            COpenDirHelper::m_incl_subdir = !!recur;
        } else {
            return;
        }
    } else {
        CString filter;
        CAtlArray<CString> mask;
        s.m_Formats.GetFilter(filter, mask);

        COpenDirHelper::strLastOpenDir = s.strLastOpenDir;

        TCHAR _path[MAX_PATH];
        COpenDirHelper::m_incl_subdir = TRUE;

        BROWSEINFO bi;
        bi.hwndOwner      = m_hWnd;
        bi.pidlRoot       = NULL;
        bi.pszDisplayName = _path;
        bi.lpszTitle      = strTitle;
        bi.ulFlags        = BIF_RETURNONLYFSDIRS | BIF_VALIDATE | BIF_STATUSTEXT;
        bi.lpfn           = COpenDirHelper::BrowseCallbackProcDIR;
        bi.lParam         = 0;
        bi.iImage         = 0;

        static LPITEMIDLIST iil;
        iil = SHBrowseForFolder(&bi);
        if (iil) {
            SHGetPathFromIDList(iil, _path);
        } else {
            return;
        }
        path = _path;
    }

    if (path[path.GetLength() - 1] != '\\') {
        path += '\\';
    }

    CAtlList<CString> sl;
    sl.AddTail(path);
    if (COpenDirHelper::m_incl_subdir) {
        COpenDirHelper::RecurseAddDir(path, &sl);
    }

    if (m_wndPlaylistBar.IsWindowVisible()) {
        m_wndPlaylistBar.Append(sl, true);
    } else {
        m_wndPlaylistBar.Open(sl, true);
        OpenCurPlaylistItem();
    }
}

HRESULT CMainFrame::CreateThumbnailToolbar()
{
    if (!AfxGetAppSettings().fUseWin7TaskBar || !SysVersion::Is7OrLater()) {
        return E_FAIL;
    }

    if (m_pTaskbarList) {
        m_pTaskbarList->Release();
    }
    HRESULT hr = CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_pTaskbarList));
    if (SUCCEEDED(hr)) {
        CMPCPngImage image;
        if (!image.Load(MAKEINTRESOURCE(IDF_WIN7_TOOLBAR))) {
            m_pTaskbarList->Release();
            image.CleanUp();
            return E_FAIL;
        }

        BITMAP bi;
        image.GetBitmap(&bi);
        int nI = bi.bmWidth / bi.bmHeight;
        HIMAGELIST hImageList = ImageList_Create(bi.bmHeight, bi.bmHeight, ILC_COLOR32, nI, 0);

        ImageList_Add(hImageList, (HBITMAP)image, 0);
        hr = m_pTaskbarList->ThumbBarSetImageList(m_hWnd, hImageList);

        if (SUCCEEDED(hr)) {
            THUMBBUTTON buttons[5] = {};

            // PREVIOUS
            buttons[0].dwMask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;
            buttons[0].dwFlags = THBF_DISABLED;
            buttons[0].iId = IDTB_BUTTON3;
            buttons[0].iBitmap = 0;
            StringCchCopy(buttons[0].szTip, _countof(buttons[0].szTip), ResStr(IDS_AG_PREVIOUS));

            // STOP
            buttons[1].dwMask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;
            buttons[1].dwFlags = THBF_DISABLED;
            buttons[1].iId = IDTB_BUTTON1;
            buttons[1].iBitmap = 1;
            StringCchCopy(buttons[1].szTip, _countof(buttons[1].szTip), ResStr(IDS_AG_STOP));

            // PLAY/PAUSE
            buttons[2].dwMask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;
            buttons[2].dwFlags = THBF_DISABLED;
            buttons[2].iId = IDTB_BUTTON2;
            buttons[2].iBitmap = 3;
            StringCchCopy(buttons[2].szTip, _countof(buttons[2].szTip), ResStr(IDS_AG_PLAYPAUSE));

            // NEXT
            buttons[3].dwMask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;
            buttons[3].dwFlags = THBF_DISABLED;
            buttons[3].iId = IDTB_BUTTON4;
            buttons[3].iBitmap = 4;
            StringCchCopy(buttons[3].szTip, _countof(buttons[3].szTip), ResStr(IDS_AG_NEXT));

            // FULLSCREEN
            buttons[4].dwMask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;
            buttons[4].dwFlags = THBF_DISABLED;
            buttons[4].iId = IDTB_BUTTON5;
            buttons[4].iBitmap = 5;
            StringCchCopy(buttons[4].szTip, _countof(buttons[4].szTip), ResStr(IDS_AG_FULLSCREEN));

            hr = m_pTaskbarList->ThumbBarAddButtons(m_hWnd, ARRAYSIZE(buttons), buttons);
        }
        ImageList_Destroy(hImageList);
        image.CleanUp();
    }

    return hr;
}

HRESULT CMainFrame::UpdateThumbarButton()
{
    if (!m_pTaskbarList) {
        return E_FAIL;
    }

    if (!AfxGetAppSettings().fUseWin7TaskBar) {
        m_pTaskbarList->SetOverlayIcon(m_hWnd, NULL, L"");
        m_pTaskbarList->SetProgressState(m_hWnd, TBPF_NOPROGRESS);

        THUMBBUTTON buttons[5] = {};

        buttons[0].dwMask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;
        buttons[0].dwFlags = THBF_HIDDEN;
        buttons[0].iId = IDTB_BUTTON3;

        buttons[1].dwMask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;
        buttons[1].dwFlags = THBF_HIDDEN;
        buttons[1].iId = IDTB_BUTTON1;

        buttons[2].dwMask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;
        buttons[2].dwFlags = THBF_HIDDEN;
        buttons[2].iId = IDTB_BUTTON2;

        buttons[3].dwMask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;
        buttons[3].dwFlags = THBF_HIDDEN;
        buttons[3].iId = IDTB_BUTTON4;

        buttons[4].dwMask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;
        buttons[4].dwFlags = THBF_HIDDEN;
        buttons[4].iId = IDTB_BUTTON5;

        HRESULT hr = m_pTaskbarList->ThumbBarUpdateButtons(m_hWnd, ARRAYSIZE(buttons), buttons);
        return hr;
    }

    THUMBBUTTON buttons[5] = {};

    buttons[0].dwMask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;
    buttons[0].dwFlags = (!AfxGetAppSettings().fUseSearchInFolder && m_wndPlaylistBar.GetCount() <= 1 && (m_pCB && m_pCB->ChapGetCount() <= 1)) ? THBF_DISABLED : THBF_ENABLED;
    buttons[0].iId = IDTB_BUTTON3;
    buttons[0].iBitmap = 0;
    StringCchCopy(buttons[0].szTip, _countof(buttons[0].szTip), ResStr(IDS_AG_PREVIOUS));

    buttons[1].dwMask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;
    buttons[1].iId = IDTB_BUTTON1;
    buttons[1].iBitmap = 1;
    StringCchCopy(buttons[1].szTip, _countof(buttons[1].szTip), ResStr(IDS_AG_STOP));

    buttons[2].dwMask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;
    buttons[2].iId = IDTB_BUTTON2;
    buttons[2].iBitmap = 3;
    StringCchCopy(buttons[2].szTip, _countof(buttons[2].szTip), ResStr(IDS_AG_PLAYPAUSE));

    buttons[3].dwMask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;
    buttons[3].dwFlags = (!AfxGetAppSettings().fUseSearchInFolder && m_wndPlaylistBar.GetCount() <= 1 && (m_pCB && m_pCB->ChapGetCount() <= 1)) ? THBF_DISABLED : THBF_ENABLED;
    buttons[3].iId = IDTB_BUTTON4;
    buttons[3].iBitmap = 4;
    StringCchCopy(buttons[3].szTip, _countof(buttons[3].szTip), ResStr(IDS_AG_NEXT));

    buttons[4].dwMask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;
    buttons[4].dwFlags = THBF_ENABLED;
    buttons[4].iId = IDTB_BUTTON5;
    buttons[4].iBitmap = 5;
    StringCchCopy(buttons[4].szTip, _countof(buttons[4].szTip), ResStr(IDS_AG_FULLSCREEN));

    if (m_iMediaLoadState == MLS_LOADED) {
        HICON hIcon = NULL;
        OAFilterState fs = GetMediaState();
        if (fs == State_Running) {
            buttons[1].dwFlags = THBF_ENABLED;
            buttons[2].dwFlags = THBF_ENABLED;
            buttons[2].iBitmap = 2;

            hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_TB_PLAY), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
            m_pTaskbarList->SetProgressState(m_hWnd, TBPF_NORMAL);
        } else if (fs == State_Stopped) {
            buttons[1].dwFlags = THBF_DISABLED;
            buttons[2].dwFlags = THBF_ENABLED;
            buttons[2].iBitmap = 3;

            hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_TB_STOP), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
            m_pTaskbarList->SetProgressState(m_hWnd, TBPF_NOPROGRESS);
        } else if (fs == State_Paused) {
            buttons[1].dwFlags = THBF_ENABLED;
            buttons[2].dwFlags = THBF_ENABLED;
            buttons[2].iBitmap = 3;

            hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_TB_PAUSE), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
            m_pTaskbarList->SetProgressState(m_hWnd, TBPF_PAUSED);
        }

        if (m_fAudioOnly) {
            buttons[4].dwFlags = THBF_DISABLED;
        }

        if (GetPlaybackMode() == PM_DVD && m_iDVDDomain != DVD_DOMAIN_Title) {
            buttons[0].dwFlags = THBF_DISABLED;
            buttons[1].dwFlags = THBF_DISABLED;
            buttons[2].dwFlags = THBF_DISABLED;
            buttons[3].dwFlags = THBF_DISABLED;
        }

        m_pTaskbarList->SetOverlayIcon(m_hWnd, hIcon, L"");

        if (hIcon != NULL) {
            DestroyIcon(hIcon);
        }
    } else {
        buttons[0].dwFlags = THBF_DISABLED;
        buttons[1].dwFlags = THBF_DISABLED;
        buttons[2].dwFlags = THBF_DISABLED;
        buttons[3].dwFlags = THBF_DISABLED;
        buttons[4].dwFlags = THBF_DISABLED;

        m_pTaskbarList->SetOverlayIcon(m_hWnd, NULL, L"");
        m_pTaskbarList->SetProgressState(m_hWnd, TBPF_NOPROGRESS);
    }

    HRESULT hr = m_pTaskbarList->ThumbBarUpdateButtons(m_hWnd, ARRAYSIZE(buttons), buttons);

    UpdateThumbnailClip();

    return hr;
}

HRESULT CMainFrame::UpdateThumbnailClip()
{
    if (!m_pTaskbarList) {
        return E_FAIL;
    }

    if ((!AfxGetAppSettings().fUseWin7TaskBar) || (m_iMediaLoadState != MLS_LOADED) || (m_fAudioOnly) || m_fFullScreen) {
        return m_pTaskbarList->SetThumbnailClip(m_hWnd, NULL);
    }

    RECT vid_rect, result_rect;
    m_wndView.GetClientRect(&vid_rect);

    // Remove the menu from thumbnail clip preview if it displayed
    result_rect.left = 2;
    result_rect.right = result_rect.left + (vid_rect.right - vid_rect.left) - 4;
    result_rect.top = (AfxGetAppSettings().iCaptionMenuMode == MODE_SHOWCAPTIONMENU) ? 22 : 2;
    result_rect.bottom = result_rect.top + (vid_rect.bottom - vid_rect.top) - 4;

    return m_pTaskbarList->SetThumbnailClip(m_hWnd, &result_rect);
}

LRESULT CMainFrame::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    if ((message == WM_COMMAND) && (THBN_CLICKED == HIWORD(wParam))) {
        int const wmId = LOWORD(wParam);
        switch (wmId) {
            case IDTB_BUTTON1:
                SendMessage(WM_COMMAND, ID_PLAY_STOP);
                break;
            case IDTB_BUTTON2:
                SendMessage(WM_COMMAND, ID_PLAY_PLAYPAUSE);
                break;
            case IDTB_BUTTON3:
                SendMessage(WM_COMMAND, ID_NAVIGATE_SKIPBACK);
                break;
            case IDTB_BUTTON4:
                SendMessage(WM_COMMAND, ID_NAVIGATE_SKIPFORWARD);
                break;
            case IDTB_BUTTON5:
                WINDOWPLACEMENT wp;
                GetWindowPlacement(&wp);
                if (wp.showCmd == SW_SHOWMINIMIZED) {
                    SendMessage(WM_SYSCOMMAND, SC_RESTORE, -1);
                }
                SetForegroundWindow();
                SendMessage(WM_COMMAND, ID_VIEW_FULLSCREEN);
                break;
            default:
                break;
        }
        return 0;
    }

    return __super::WindowProc(message, wParam, lParam);
}

UINT CMainFrame::OnPowerBroadcast(UINT nPowerEvent, UINT nEventData)
{
    static BOOL bWasPausedBeforeSuspention;
    OAFilterState mediaState;

    switch (nPowerEvent) {
        case PBT_APMSUSPEND:            // System is suspending operation.
            TRACE(_T("OnPowerBroadcast - suspending\n"));   // For user tracking
            bWasPausedBeforeSuspention = FALSE;             // Reset value
            mediaState = GetMediaState();

            if (mediaState == State_Running) {
                bWasPausedBeforeSuspention = TRUE;
                SendMessage(WM_COMMAND, ID_PLAY_PAUSE);     // Pause
            }
            break;
        case PBT_APMRESUMEAUTOMATIC:    // Operation is resuming automatically from a low-power state. This message is sent every time the system resumes.
            TRACE(_T("OnPowerBroadcast - resuming\n"));     // For user tracking

            // Resume if we paused before suspension.
            if (bWasPausedBeforeSuspention) {
                SendMessage(WM_COMMAND, ID_PLAY_PLAY);      // Resume
            }
            break;
    }

    return __super::OnPowerBroadcast(nPowerEvent, nEventData);
}

#define NOTIFY_FOR_THIS_SESSION 0

void CMainFrame::OnSessionChange(UINT nSessionState, UINT nId)
{
    static BOOL bWasPausedBeforeSessionChange;

    switch (nSessionState) {
        case WTS_SESSION_LOCK:
            TRACE(_T("OnSessionChange - Lock session\n"));
            bWasPausedBeforeSessionChange = FALSE;

            if (GetMediaState() == State_Running && !m_fAudioOnly) {
                bWasPausedBeforeSessionChange = TRUE;
                SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
            }
            break;
        case WTS_SESSION_UNLOCK:
            TRACE(_T("OnSessionChange - UnLock session\n"));

            if (bWasPausedBeforeSessionChange) {
                SendMessage(WM_COMMAND, ID_PLAY_PLAY);
            }
            break;
    }
}

void CMainFrame::WTSRegisterSessionNotification()
{
    typedef BOOL (WINAPI * WTSREGISTERSESSIONNOTIFICATION)(HWND, DWORD);
    HINSTANCE hWtsLib = LoadLibrary(_T("wtsapi32.dll"));

    if (hWtsLib) {
        WTSREGISTERSESSIONNOTIFICATION fnWtsRegisterSessionNotification;

        fnWtsRegisterSessionNotification = (WTSREGISTERSESSIONNOTIFICATION)GetProcAddress(hWtsLib, "WTSRegisterSessionNotification");

        if (fnWtsRegisterSessionNotification) {
            fnWtsRegisterSessionNotification(m_hWnd, NOTIFY_FOR_THIS_SESSION);
        }

        FreeLibrary(hWtsLib);
        hWtsLib = NULL;
    }
}

void CMainFrame::WTSUnRegisterSessionNotification()
{
    typedef BOOL (WINAPI * WTSUNREGISTERSESSIONNOTIFICATION)(HWND);
    HINSTANCE hWtsLib = LoadLibrary(_T("wtsapi32.dll"));

    if (hWtsLib) {
        WTSUNREGISTERSESSIONNOTIFICATION fnWtsUnRegisterSessionNotification;

        fnWtsUnRegisterSessionNotification = (WTSUNREGISTERSESSIONNOTIFICATION)GetProcAddress(hWtsLib, "WTSUnRegisterSessionNotification");

        if (fnWtsUnRegisterSessionNotification) {
            fnWtsUnRegisterSessionNotification(m_hWnd);
        }

        FreeLibrary(hWtsLib);
        hWtsLib = NULL;
    }
}

void CMainFrame::EnableShaders1(bool enable)
{
    if (enable && !m_shaderlabels.IsEmpty()) {
        m_bToggleShader = true;
        SetShaders();
    } else {
        m_bToggleShader = false;
        if (m_pCAP) {
            m_pCAP->SetPixelShader(NULL, NULL);
        }
    }
}

void CMainFrame::EnableShaders2(bool enable)
{
    if (enable && !m_shaderlabelsScreenSpace.IsEmpty()) {
        m_bToggleShaderScreenSpace = true;
        SetShaders();
    } else {
        m_bToggleShaderScreenSpace = false;
        if (m_pCAP2) {
            m_pCAP2->SetPixelShader2(NULL, NULL, true);
        }
    }
}

bool CMainFrame::OpenBD(CString Path)
{
    CHdmvClipInfo ClipInfo;
    CString strPlaylistFile;
    CAtlList<CHdmvClipInfo::PlaylistItem> MainPlaylist;

#if INTERNAL_SOURCEFILTER_MPEG
    const CAppSettings& s = AfxGetAppSettings();
    bool InternalMpegSplitter = s.SrcFilters[SRC_MPEG];
#else
    bool InternalMpegSplitter = false;
#endif

    m_LastOpenBDPath = Path;

    CString ext = CPath(Path).GetExtension();
    ext.MakeLower();

    if ((CPath(Path).IsDirectory() && Path.Find(_T("\\BDMV")))
            || CPath(Path + _T("\\BDMV")).IsDirectory()
            || (!ext.IsEmpty() && ext == _T(".bdmv"))) {
        if (!ext.IsEmpty() && ext == _T(".bdmv")) {
            Path.Replace(_T("\\BDMV\\"), _T("\\"));
            CPath _Path(Path);
            _Path.RemoveFileSpec();
            Path = CString(_Path);
        } else if (Path.Find(_T("\\BDMV"))) {
            Path.Replace(_T("\\BDMV"), _T("\\"));
        }
        if (SUCCEEDED(ClipInfo.FindMainMovie(Path, strPlaylistFile, MainPlaylist, m_MPLSPlaylist))) {
            m_bIsBDPlay = true;

            if (!InternalMpegSplitter && !ext.IsEmpty() && ext == _T(".bdmv")) {
                return false;
            } else {
                m_wndPlaylistBar.Empty();
                CAtlList<CString> sl;

                if (InternalMpegSplitter) {
                    sl.AddTail(CString(strPlaylistFile));
                } else {
                    sl.AddTail(CString(Path + _T("BDMV\\index.bdmv")));
                }

                m_wndPlaylistBar.Append(sl, false);
                OpenCurPlaylistItem();
                return true;
            }
        }
    }

    m_LastOpenBDPath = _T("");
    return false;
}

// Returns the local id of the stream and set subElement the corresponding SubElement
// Returns -1 in case of error.
int CMainFrame::GetSubtitleInput(int i, SubtitleInput& subElement)
{
    POSITION pos = m_pSubStreams.GetHeadPosition();
    bool found = false;

    while (pos && i >= 0) {
        subElement = m_pSubStreams.GetNext(pos);

        if (CComQIPtr<IAMStreamSelect> pSSF = subElement.sourceFilter) {
            DWORD cStreams;
            if (FAILED(pSSF->Count(&cStreams))) {
                continue;
            }

            for (int j = 0, cnt = (int)cStreams; j < cnt; j++) {
                DWORD dwGroup;

                if (FAILED(pSSF->Info(j, NULL, NULL, NULL, &dwGroup, NULL, NULL, NULL))) {
                    continue;
                }

                if (dwGroup != 2) {
                    continue;
                }

                if (i == 0) {
                    return j;
                }

                i--;
            }
        } else {
            if (i < subElement.subStream->GetStreamCount()) {
                return i;
            } else {
                i -= subElement.subStream->GetStreamCount();
            }
        }
    }

    return -1;
}
