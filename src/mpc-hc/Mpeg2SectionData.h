/*
 * (C) 2009-2012 see Authors.txt
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

#pragma once

#include "DVBChannel.h"

#pragma pack(1)
struct SI_HEADER {
    UINT8       TableID;
    WORD        SectionSyntaxIndicator  : 1;
    WORD        Reserved1               : 3;
    WORD        SectionLength           : 12;
    WORD        BouquetID;
    UINT8       Reserved2               : 1;
    UINT8       VersionNumber           : 5;
    UINT8       CurrentNextIndicator    : 1;
    UINT8       SectionNumber;
    UINT8       LastSectionNumber;
};

struct EventInformationSection {
    UINT8       TableID;
    WORD        SectionSyntaxIndicator  : 1;
    WORD        Reserved1               : 3;
    WORD        SectionLength           : 12;
    ULONG       ServiceId;
    UINT8       Reserved2               : 2;
    UINT8       VersionNumber           : 5;
    UINT8       CurrentNextIndicator    : 1;
    UINT8       SectionNumber;
    UINT8       LastSectionNumber;
    WORD        TransportStreamID;
    WORD        OriginalNetworkID;
    UINT8       SegmentLastSectionNumber;
    UINT8       LastTableID;
    WORD        EventID;
    WORD        StartDate;
    UINT8       StartTime[6];
    UINT8       Duration[6];
    WORD        RunninStatus            : 3;
    WORD        FreeCAMode              : 1;
    WORD        DescriptorsLoopLenght   : 12;

};

class CMpeg2DataParser
{
public:
    CMpeg2DataParser(IBaseFilter* pFilter);

    HRESULT     ParseSDT(ULONG ulFreq);
    HRESULT     ParsePAT();
    HRESULT     ParseNIT();
    HRESULT     ParseEIT(ULONG ulSID, EventDescriptor& NowNext);
    HRESULT     ParsePMT(CDVBChannel& Channel);

    static CStringW ConvertString(BYTE* pBuffer, size_t uLenght);

    CAtlMap<int, CDVBChannel>   Channels;

private:
    CComQIPtr<IMpeg2Data>       m_pData;
    MPEG2_FILTER                m_Filter;


    DVB_STREAM_TYPE ConvertToDVBType(PES_STREAM_TYPE nType);
    HRESULT         ParseSIHeader(CGolombBuffer& gb, DVB_SI SIType, WORD& wSectionLength, WORD& wTSID);
    HRESULT         SetTime(CGolombBuffer& gb, EventDescriptor& NowNext);
};
