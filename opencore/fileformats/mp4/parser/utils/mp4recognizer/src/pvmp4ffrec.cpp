/* ------------------------------------------------------------------
 * Copyright (C) 1998-2010 PacketVideo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */
#include "atomutils.h"
#include "atomdefs.h"

#include "pvmp4ffrec.h"

// Use default DLL entry point for Symbian
#include "oscl_dll.h"
OSCL_DLL_ENTRY_POINT_DEFAULT()

OSCL_EXPORT_REF bool MP4FileRecognizer::IsMP4File(OSCL_wString& filename,
        Oscl_FileServer* fileServSession)
{
    bool oReturn = false;
    MP4_FF_FILE fileStruct;
    MP4_FF_FILE *fp = &fileStruct;

    fp->_fileServSession = fileServSession;

    if (AtomUtils::OpenMP4File(filename,
                               Oscl_File::MODE_READ | Oscl_File::MODE_BINARY,
                               fp) != 0)
    {
        return (oReturn);
    }

    TOsclFileOffset fileSize;
    TOsclFileOffset filePointer;
    filePointer = AtomUtils::getCurrentFilePosition(fp);
    AtomUtils::seekToEnd(fp);
    fileSize = AtomUtils::getCurrentFilePosition(fp);
    AtomUtils::seekFromStart(fp, filePointer);
    fp->_fileSize = fileSize;

    TOsclFileOffset fpos = filePointer;

    while (fpos < fileSize)
    {
        uint32 atomType = UNKNOWN_ATOM;
        uint32 atomSize = 0;

        AtomUtils::getNextAtomType(fp, atomSize, atomType);

        if (atomType != UNKNOWN_ATOM)
        {
            if (atomType == FILE_TYPE_ATOM)
            {
                uint32 majorBrand;

                if (atomSize >= MINIMUM_SIZE_REQUIRED_TO_READ_MAJOR_BRAND)
                {
                    AtomUtils::read32(fp, majorBrand);
                }

                if (majorBrand == BRAND_ODCF)
                {
                    //This is an OMA2 DCF File.
                    break;
                }
            }
            oReturn = true;
            break;
        }
        else
        {
            if (atomSize < DEFAULT_ATOM_SIZE)
            {
                break;
            }
            if (fileSize < (int32)atomSize)
            {
                break;
            }
            atomSize -= DEFAULT_ATOM_SIZE;
            AtomUtils::seekFromCurrPos(fp, atomSize);
            fpos = AtomUtils::getCurrentFilePosition(fp);
        }
    }

    AtomUtils::CloseMP4File(fp);
    return (oReturn);
}


OSCL_EXPORT_REF bool MP4FileRecognizer::IsMP4File(MP4_FF_FILE_REFERENCE filePtr)
{
    bool oReturn = false;
    MP4_FF_FILE fileStruct;
    MP4_FF_FILE *fp = &fileStruct;

    fp->_pvfile.SetFilePtr(filePtr);

    TOsclFileOffset fileSize;
    TOsclFileOffset filePointer;
    AtomUtils::seekFromStart(fp, 0);
    filePointer = AtomUtils::getCurrentFilePosition(fp);
    AtomUtils::seekToEnd(fp);
    fileSize = AtomUtils::getCurrentFilePosition(fp);
    AtomUtils::seekFromStart(fp, filePointer);
    fp->_fileSize = fileSize;

    TOsclFileOffset fpos = filePointer;

    while (fpos < fileSize)
    {
        uint32 atomType = UNKNOWN_ATOM;
        uint32 atomSize = 0;

        AtomUtils::getNextAtomType(fp, atomSize, atomType);

        if (atomType != UNKNOWN_ATOM)
        {
            if (atomType == FILE_TYPE_ATOM)
            {
                uint32 majorBrand;

                if (atomSize >= MINIMUM_SIZE_REQUIRED_TO_READ_MAJOR_BRAND)
                {
                    AtomUtils::read32(fp, majorBrand);
                }

                if (majorBrand == BRAND_ODCF)
                {
                    //This is an OMA2 DCF File.
                    break;
                }
            }
            oReturn = true;
            break;
        }
        else
        {
            if (atomSize < DEFAULT_ATOM_SIZE)
            {
                break;
            }
            if (fileSize < (int32)atomSize)
            {
                break;
            }
            atomSize -= DEFAULT_ATOM_SIZE;
            AtomUtils::seekFromCurrPos(fp, atomSize);
            fpos = AtomUtils::getCurrentFilePosition(fp);
        }
    }

    return (oReturn);
}

OSCL_EXPORT_REF bool MP4FileRecognizer::IsMP4File(PVMFCPMPluginAccessInterfaceFactory* aCPMAccessFactory,
        Oscl_FileServer* aFileServSession,
        OsclFileHandle* aHandle)
{
    bool oReturn = false;

    /* use a dummy string for file name */
    OSCL_wHeapString<OsclMemAllocator> filename;

    MP4_FF_FILE fileStruct;
    MP4_FF_FILE *fp = &fileStruct;
    fp->_fileServSession = aFileServSession;
    fp->_pvfile.SetCPM(aCPMAccessFactory);
    fp->_pvfile.SetFileHandle(aHandle);

    if (AtomUtils::OpenMP4File(filename,
                               Oscl_File::MODE_READ | Oscl_File::MODE_BINARY,
                               fp) != 0)
    {
        return oReturn;
    }

    AtomUtils::seekFromStart(fp, 0);
    TOsclFileOffset fileSize = 0;
    AtomUtils::getCurrentFileSize(fp, fileSize);
    fp->_fileSize = fileSize;
    TOsclFileOffset fpos = AtomUtils::getCurrentFilePosition(fp);

    while (fpos < fileSize)
    {
        uint32 atomType = UNKNOWN_ATOM;
        uint32 atomSize = 0;

        AtomUtils::getNextAtomType(fp, atomSize, atomType);

        if (atomType != UNKNOWN_ATOM)
        {
            if (atomType == FILE_TYPE_ATOM)
            {
                uint32 majorBrand;

                if (atomSize >= MINIMUM_SIZE_REQUIRED_TO_READ_MAJOR_BRAND)
                {
                    AtomUtils::read32(fp, majorBrand);
                }

                if (majorBrand == BRAND_ODCF)
                {
                    //This is an OMA2 DCF File.
                    break;
                }
            }
            oReturn = true;
            break;
        }
        else
        {
            if (atomSize < DEFAULT_ATOM_SIZE)
            {
                break;
            }
            if ((uint64)fileSize < atomSize)
            {
                break;
            }
            atomSize -= DEFAULT_ATOM_SIZE;
            AtomUtils::seekFromCurrPos(fp, atomSize);
            fpos = AtomUtils::getCurrentFilePosition(fp);
        }
    }

    AtomUtils::seekFromStart(fp, 0);
    AtomUtils::CloseMP4File(fp);
    return (oReturn);
}