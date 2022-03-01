#include <renard.h>

void RenardReceiver::begin(renard_addr_t channels, HardwareSerial *hwSerial, uint32_t baud)
{
    // Allocate buffers
    #ifdef RENARD_DYNAMIC_BUFFERS
    channelData = (uint8_t *)malloc(channels);
    if (channelData == nullptr)
        return;
    #endif

    // Set offset (also clears buffer) as well as channel count
    channelCount = min(channels, RENARD_MAX_CHANNEL_COUNT);
    setChannelOffset(0);

    // Init receiver FSM
    state = INIT;
    specialPending = false;
    idleTimeout = DEFAULT_IDLE_TIMEOUT;
    lastCmdReceived = 0;

    // Initialize serial port
    inputPort = hwSerial;
    inputPort->begin(baud);
}

bool RenardReceiver::process()
{
    byte incomingByte;
    bool dataRead = false, iterate = true;
    if (channelData == nullptr)
        return false;

    while (inputPort->available() && iterate)
    {
        dataRead = true;
        incomingByte = inputPort->read();

        // Discard pad bytes immediately
        if (incomingByte == RENARD_PAD_BYTE)
            continue;

        // Reset on start byte
        if (incomingByte == RENARD_START_BYTE)
            state = INIT;

        switch (state)
        {
        case INIT:
            if (incomingByte == RENARD_START_BYTE)
                state = COMMAND;
            break;

        case COMMAND:
            idle = false;
            lastCmdReceived = millis();
            if (incomingByte >= RENARD_MIN_CHANNEL_COMMAND)
            {
                recvAddress = (incomingByte - RENARD_MIN_CHANNEL_COMMAND) * 8;
                state = DATA;
            }
            else if (incomingByte == RENARD_SPECIAL_COMMAND)
            {
                state = SPECIAL_OPCODE;
            }
            else
            {
                state = COMPLETE;
            }
            break;

        case ESCAPE:
            state = processIncomingByte(incomingByte + 0x4e);
            break;

        case DATA:
            if (incomingByte == RENARD_ESCAPE_BYTE)
                state = ESCAPE;
            else
                state = processIncomingByte(incomingByte);
            break;

        // Special command parsing, this is not standard Renard
        // Break outer loop after each special command to allow caller to interpret
        case SPECIAL_OPCODE:
            specialOpcode = incomingByte;
            state = SPECIAL_DATA;
            break;

        case SPECIAL_ESCAPE:
            specialData = incomingByte + 0x4e;
            specialPending = true;
            state = INIT;
            iterate = false;
            break;

        case SPECIAL_DATA:
            if (incomingByte == RENARD_ESCAPE_BYTE)
            {
                state = SPECIAL_ESCAPE;
            }
            else
            {
                specialData = incomingByte;
                specialPending = true;
                state = INIT;
                iterate = false;
            }
            break;

        case COMPLETE:
            // Do nothing with the current byte
            break;
        }
    }
    return dataRead;
}

void RenardReceiver::setChannelOffset(renard_addr_t off)
{
    // Set the channel offset
    channelOffset = off;
    // Wait for next packet in case we were mid-frame
    if (state == DATA)
        state = INIT;
    // clear the channel data buffer
    memset(channelData, 0, channelCount);
}

bool RenardReceiver::isIdle()
{
    // idle flag is cleared when lastcmdreceived is updated
    // This is needed to handle being idle for > the millis() rollover time
    if (idle)
    {
        return true;
    }

    idle = ((millis() - lastCmdReceived) >= idleTimeout);
    return idle;
}

void RenardReceiver::setIdleTimeout(unsigned long millisec)
{
    idleTimeout = millisec;
}

byte RenardReceiver::read(renard_addr_t address, bool implicit_process)
{
    // Read a channel value, indicies relative to the local channel buffer
    if (implicit_process)
        process();

    if (address < channelCount)
        return channelData[address];
    else
        return 0;
}

bool RenardReceiver::specialAvailable(bool implicit_process)
{
    // Indicates if a special command has been received
    if (implicit_process)
        process();

    return specialPending;
}

byte RenardReceiver::readSpecialOpcode()
{
    return specialOpcode;
}

byte RenardReceiver::readSpecialData()
{
    // Clears the special command pending flag
    specialPending = false;
    return specialData;
}

renard_state_t RenardReceiver::processIncomingByte(byte value)
{
    // Applies offset and writes an incoming value
    // to the channelData buffer if in bounds
    renard_addr_t bufferAddress;
    if (recvAddress >= channelOffset)
    {
        bufferAddress = recvAddress - channelOffset;
        if (bufferAddress < channelCount)
            channelData[bufferAddress] = value;
        else
            return COMPLETE;
    }

    // Cannot track more data therefore stop receiving
    if (recvAddress >= RENARD_MAX_ADDRESS)
        return COMPLETE;

    // Increment receive address and keep reading
    recvAddress++;
    return DATA;
}

renard_addr_t RenardReceiver::getChannelOffset()
{
    return channelOffset;
}

renard_addr_t RenardReceiver::getChannelCount()
{
    return channelCount;
}

void RenardReceiver::end()
{
    // Close port and free buffer if it was allocated
    #ifdef RENARD_DYNAMIC_BUFFERS
    if (channelData != nullptr)
    {
        free(channelData);
        channelData = nullptr;
    }
    #endif
    inputPort->end();
}