#ifndef renard_h
#define renard_h

#include <Arduino.h>
#include <HardwareSerial.h>

#ifdef RENARD_LARGE_ADDRESS
    #define RENARD_MAX_ADDRESS UINT16_MAX
    typedef uint16_t renard_addr_t;
#else
    #define RENARD_MAX_ADDRESS UINT8_MAX
    typedef uint8_t renard_addr_t;
#endif

#define RENARD_START_BYTE 0x7e
#define RENARD_PAD_BYTE 0x7d
#define RENARD_ESCAPE_BYTE 0x7f
#define RENARD_MIN_CHANNEL_COMMAND 0x80
#define RENARD_SPECIAL_COMMAND 0x7a
#define RENARD_MAX_CHANNEL_COUNT 32

#define DEFAULT_IDLE_TIMEOUT 5000

typedef enum { INIT, COMMAND, DATA, SPECIAL_OPCODE, SPECIAL_DATA, SPECIAL_ESCAPE, ESCAPE, COMPLETE} renard_state_t;

class RenardReceiver {
    public:
        void begin(renard_addr_t channels, HardwareSerial *hwSerial, uint32_t baud);
        renard_addr_t getChannelCount();
        void setChannelOffset(renard_addr_t count);
        void setIdleTimeout(unsigned long timeout);
        bool isIdle();
        renard_addr_t getChannelOffset();
        byte read(renard_addr_t address, bool implicit_process=true);
        bool process();
        bool specialAvailable(bool implicit_process=true);
        byte readSpecialOpcode();
        byte readSpecialData();
        void end();

    private:
        #ifdef RENARD_DYNAMIC_BUFFERS
        byte* channelData=nullptr;
        #else
        byte channelData[RENARD_MAX_CHANNEL_COUNT];
        #endif
        renard_addr_t channelCount, channelOffset;
        HardwareSerial* inputPort;
        renard_state_t state;
        // 2 vars to handle being idle for > millis() wraparound interval
        bool idle;
        unsigned long idleTimeout, lastCmdReceived;
        renard_addr_t recvAddress;
        renard_state_t processIncomingByte(byte value);
        bool specialPending;
        byte specialOpcode, specialData;
};
#endif