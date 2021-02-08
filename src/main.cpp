/* -LICENSE-START-
 ** Copyright (c) 2014 Blackmagic Design
 **
 ** Permission is hereby granted, free of charge, to any person or organization
 ** obtaining a copy of the software and accompanying documentation covered by
 ** this license (the "Software") to use, reproduce, display, distribute,
 ** execute, and transmit the Software, and to prepare derivative works of the
 ** Software, and to permit third-parties to whom the Software is furnished to
 ** do so, all subject to the following:
 **
 ** The copyright notices in the Software and this entire statement, including
 ** the above license grant, this restriction and the following disclaimer,
 ** must be included in all copies of the Software, in whole or in part, and
 ** all derivative works of the Software, unless such copies or derivative
 ** works are solely in the form of machine-executable object code generated by
 ** a source language processor.
 **
 ** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 ** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 ** FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 ** SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 ** FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 ** ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 ** DEALINGS IN THE SOFTWARE.
 ** -LICENSE-END-
 */

#include "platform.h"

// Video mode parameters
const BMDDisplayMode kDisplayMode = bmdModeHD1080i50;
const BMDVideoOutputFlags kOutputFlag = bmdVideoOutputVANC;
const BMDPixelFormat kPixelFormat = bmdFormat10BitYUV;

// Frame parameters
const INT32_UNSIGNED kFrameDuration = 1000;
const INT32_UNSIGNED kTimeScale = 25000;
const INT32_UNSIGNED kFrameWidth = 1920;
const INT32_UNSIGNED kFrameHeight = 1080;
const INT32_UNSIGNED kRowBytes = 5120;

// 10-bit YUV blue pixels
const INT32_UNSIGNED kBlueData[4] = {0x40aa298, 0x2a8a62a8, 0x298aa040, 0x2a8102a8};

// Studio Camera control packet:
// Set dynamic range to film.
// See Studio Camera manual for more information on protocol.
const INT8_UNSIGNED kSDIRemoteControlData[9] = {0x00, 0x07, 0x00, 0x00, 0x01, 0x07, 0x01, 0x00, 0x00};

// Data Identifier
const INT8_UNSIGNED kSDIRemoteControlDID = 0x51;

// Secondary Data Identifier
const INT8_UNSIGNED kSDIRemoteControlSDID = 0x53;

// Define VANC line for camera control
const INT32_UNSIGNED kSDIRemoteControlLine = 16;

// Keep track of the number of scheduled frames
INT32_UNSIGNED gTotalFramesScheduled = 0;

class OutputCallback : public IDeckLinkVideoOutputCallback
{
public:
    OutputCallback(IDeckLinkOutput *deckLinkOutput)
    {
        m_deckLinkOutput = deckLinkOutput;
        m_deckLinkOutput->AddRef();
    }
    virtual ~OutputCallback(void)
    {
        m_deckLinkOutput->Release();
    }
    HRESULT STDMETHODCALLTYPE ScheduledFrameCompleted(IDeckLinkVideoFrame *completedFrame, BMDOutputFrameCompletionResult result)
    {
        // When a video frame completes,reschedule another frame
        m_deckLinkOutput->ScheduleVideoFrame(completedFrame, gTotalFramesScheduled * kFrameDuration, kFrameDuration, kTimeScale);
        gTotalFramesScheduled++;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE ScheduledPlaybackHasStopped(void)
    {
        return S_OK;
    }
    // IUnknown needs only a dummy implementation
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv)
    {
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef()
    {
        return 1;
    }

    ULONG STDMETHODCALLTYPE Release()
    {
        return 1;
    }

private:
    IDeckLinkOutput *m_deckLinkOutput;
};

//  This function translates a byte into a 10-bit sample
//   x x x x x x x x x x x x
//       -------------------
//   | | |  0-7 raw data   |
//   | |
//   | even parity bit
//   inverse of bit 8
static inline INT32_UNSIGNED EncodeByte(INT32_UNSIGNED byte)
{
    INT32_UNSIGNED temp = byte;
    // Calculate the even parity bit of bits 0-7 by XOR every individual bits
    temp ^= temp >> 4;
    temp ^= temp >> 2;
    temp ^= temp >> 1;
    // Use lsb as parity bit
    temp &= 1;
    // Put even parity bit on bit 8
    byte |= temp << 8;
    // Bit 9 is inverse of bit 8
    byte |= ((~temp) & 1) << 9;
    return byte;
}
// This function writes 10bit ancillary data to 10bit luma value in YUV 10bit structure
static void WriteAncDataToLuma(INT32_UNSIGNED *&sdiStreamPosition, INT32_UNSIGNED value, INT32_UNSIGNED dataPosition)
{
    switch (dataPosition % 3)
    {
    case 0:
        *sdiStreamPosition++ = (value) << 10;
        break;
    case 1:
        *sdiStreamPosition = (value);
        break;
    case 2:
        *sdiStreamPosition++ |= (value) << 20;
        break;
    default:
        break;
    }
}

static void WriteAncillaryDataPacket(INT32_UNSIGNED *line, const INT8_UNSIGNED did, const INT8_UNSIGNED sdid, const INT8_UNSIGNED *data, INT32_UNSIGNED length)
{
    // Sanity check
    if (length == 0 || length > 255)
        return;

    const INT32_UNSIGNED encodedDID = EncodeByte(did);
    const INT32_UNSIGNED encodedSDID = EncodeByte(sdid);
    const INT32_UNSIGNED encodedDC = EncodeByte(length);

    // Start sequence
    *line++ = 0;
    *line++ = 0x3ff003ff;

    // DID
    *line++ = encodedDID << 10;

    // SDID and DC
    *line++ = encodedSDID | (encodedDC << 20);

    // Checksum does not include the start sequence
    INT32_UNSIGNED sum = encodedDID + encodedSDID + encodedDC;
    // Write the payload
    for (INT32_UNSIGNED i = 0; i < length; ++i)
    {
        const INT32_UNSIGNED encoded = EncodeByte(data[i]);
        WriteAncDataToLuma(line, encoded, i);
        sum += encoded & 0x1ff;
    }

    // Checksum % 512 then copy inverse of bit 8 to bit 9
    sum &= 0x1ff;
    sum |= ((~(sum << 1)) & 0x200);
    WriteAncDataToLuma(line, sum, length);
}

static void SetVancData(IDeckLinkVideoFrameAncillary *ancillary)
{
    HRESULT result;
    INT32_UNSIGNED *buffer;

    result = ancillary->GetBufferForVerticalBlankingLine(kSDIRemoteControlLine, (void **)&buffer);
    if (result != S_OK)
    {
        fprintf(stderr, "Could not get buffer for Vertical blanking line - result = %08x\n", result);
        return;
    }
    // Write camera control data to buffer
    WriteAncillaryDataPacket(buffer, kSDIRemoteControlDID, kSDIRemoteControlSDID, kSDIRemoteControlData, sizeof(kSDIRemoteControlData) / sizeof(kSDIRemoteControlData[0]));
}

int main(int argc, const char *argv[])
{

    IDeckLinkIterator *deckLinkIterator = NULL;
    IDeckLink *deckLink = NULL;
    IDeckLinkOutput *deckLinkOutput = NULL;
    OutputCallback *outputCallback = NULL;
    IDeckLinkVideoFrame *videoFrameBlue = NULL;
    HRESULT result;

    char *deviceNameString = NULL;
    int64_t value;
    IDeckLinkAttributes *deckLinkAttributes = NULL;

    Initialize();

    // Create an IDeckLinkIterator object to enumerate all DeckLink cards in the system
    result = GetDeckLinkIterator(&deckLinkIterator);
    if (result != S_OK)
    {
        fprintf(stderr, "A DeckLink iterator could not be created.  The DeckLink drivers may not be installed.\n");
        goto bail;
    }

    // Obtain the first DeckLink device
    result = deckLinkIterator->Next(&deckLink);
    if (result != S_OK)
    {
        fprintf(stderr, "Could not find DeckLink device - result = %08x\n", result);
        goto bail;
    }

    // Obtain the output interface for the DeckLink device
    result = deckLink->QueryInterface(IID_IDeckLinkOutput, (void **)&deckLinkOutput);
    if (result != S_OK)
    {
        fprintf(stderr, "Could not obtain the IDeckLinkInput interface - result = %08x\n", result);
        goto bail;
    }

    deckLinkOutput->CreateAncillaryData()

    


    result = deckLink->GetModelName((const char **)&deviceNameString);
    printf("=============== %s ===============\n\n", deviceNameString);
    free(deviceNameString);

    deckLink->QueryInterface(IID_IDeckLinkAttributes, (void **)&deckLinkAttributes);
    deckLinkAttributes->GetInt(BMDDeckLinkSubDeviceIndex, &value);
    printf(" %-40s %ld \n", "Sub-device index:", value);





    // // Create an instance of output callback
    // outputCallback = new OutputCallback(deckLinkOutput);
    // if (outputCallback == NULL)
    // {
    //     fprintf(stderr, "Could not create output callback object\n");
    //     goto bail;
    // }

    // // Set the callback object to the DeckLink device's output interface
    // result = deckLinkOutput->SetScheduledFrameCompletionCallback(outputCallback);
    // if (result != S_OK)
    // {
    //     fprintf(stderr, "Could not set callback - result = %08x\n", result);
    //     goto bail;
    // }

    // // Enable video output
    // result = deckLinkOutput->EnableVideoOutput(kDisplayMode, kOutputFlag);
    // if (result != S_OK)
    // {
    //     fprintf(stderr, "Could not enable video output - result = %08x\n", result);
    //     goto bail;
    // }

    // Wait until user presses Enter
    printf("Monitoring... Press <RETURN> to exit\n");

    getchar();

    printf("Exiting.\n");

    // Disable the video input interface
    result = deckLinkOutput->DisableVideoOutput();

    // Release resources
bail:

    // Release the video input interface
    if (deckLinkOutput != NULL)
        deckLinkOutput->Release();

    // Release the Decklink object
    if (deckLink != NULL)
        deckLink->Release();

    // Release the DeckLink iterator
    if (deckLinkIterator != NULL)
        deckLinkIterator->Release();

    // Release the videoframe object
    if (videoFrameBlue != NULL)
        videoFrameBlue->Release();

    // Release the outputCallback callback object
    if (outputCallback)
        delete outputCallback;

    return (result == S_OK) ? 0 : 1;
}
