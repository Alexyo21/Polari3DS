#include "menus/streaming.h"
#include "memory.h"
#include "draw.h"
#include "minisoc.h"
#include "fmt.h"
#include <arpa/inet.h>
#include "sock_util.h"
#include <3ds/gpu/gx.h>
#include <3ds/gfx.h>
#include <3ds/services/apt.h>
#include "menus/sysconfig.h"
#include "luma_config.h"
#include "luma_shared_config.h"
#include "menus/miscellaneous.h"
#include "menus/config_extra.h"
#include "ifile.h"
#include "menu.h"
#include "menus.h"
#include "volume.h"
#include "luminance.h"
#include "menus/screen_filters.h"
#include "menus/n3ds.h"

#define HEIGHT 240
#define TOPW 400
#define BOTW 320
#define BOTSIZE BOTW*HEIGHT
#define TOPSIZE TOPW*HEIGHT

void toggleGrayscale();
void increaseBlockSize();
void decreaseBlockSize();
void toggleCompression();
void closeRPHandle();
void toggleCPUCopy();

Menu streamingMenu = {
    "Streaming",
    {
        { "Start Stream!", METHOD, .method = &startMainThread },
        { "End Stream!", METHOD, .method = &endThread},
        { "Toggle Grayscale", METHOD, .method = &toggleGrayscale},
        { "Toggle Compression", METHOD, .method = &toggleCompression},
        { "Increase Blocksize", METHOD, .method = &increaseBlockSize},
        { "Decrease Blocksize", METHOD, .method = &decreaseBlockSize},
        { "Toggle CPU copy", METHOD, .method = &toggleCPUCopy},
        {},
    }
};

sock_server serv;
sock_server stream;
struct sock_ctx *cli;
u16 PORT = 4957;
static MyThread testThread;
static MyThread testSendThread;
static u8 ALIGN(8) testThreadStack[0x4000];
static u8 ALIGN(8) testSendThreadStack[0x4000];

Handle sendUDPStarted;
bool isStarted = false;
bool run = false;
bool shouldStop = true;
bool isTop = true;
bool isGraySacle = false;
bool isCPUCopy = true;
bool isSecondFrameBuffer = false;
bool doCompress = false;
bool userNoCompress = true;
bool isNew3DS = false;
u32 currentBlockSize = 8;

void toggleCompression(){doCompress = !doCompress;}
void toggleGrayscale() {isGraySacle = !isGraySacle;}
void increaseBlockSize() {if(currentBlockSize < 32) currentBlockSize = currentBlockSize+4;}
void decreaseBlockSize() {if(currentBlockSize > 4)currentBlockSize = currentBlockSize-4;}
void toggleCPUCopy() {isCPUCopy = !isCPUCopy;}


//Thread functions
void commandThreadMain(void);
MyThread *commandCreateThread(void){
    if(R_FAILED(MyThread_Create(&testThread, commandThreadMain, testThreadStack, 0x4000, 0x10, CORE_SYSTEM)))
        svcBreak(USERBREAK_PANIC);
    return &testThread;
}

void testSendThreadTCPMain(void);
MyThread *testSendCreateTCPThread(void){
    if(R_FAILED(MyThread_Create(&testSendThread, testSendThreadTCPMain, testSendThreadStack, 0x2000, 0x20, CORE_SYSTEM)))
        svcBreak(USERBREAK_PANIC);
    return &testSendThread;
}

void showNotN3DSMessage() {
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();
    do {
        char buf[50];
        sprintf(buf, "Old 3DS is not supported!");
        Draw_Lock();
        Draw_DrawString(10,10, COLOR_RED, buf);
        Draw_FlushFramebuffer();
        Draw_Unlock();
    } while(!(waitInput() & KEY_B) && !menuShouldExit);
}

void startMainThread() {
    APT_CheckNew3DS(&isNew3DS);
    if(!isNew3DS) {
        showNotN3DSMessage();
        return;
    }

    if(!isStarted) {
        isStarted = true;
        commandCreateThread();

        Draw_Lock();
        Draw_ClearFramebuffer();
        Draw_FlushFramebuffer();
        Draw_Unlock();
        do {
            char buf[50];
            sprintf(buf, "Waiting for Wifi... (Press B)");
            Draw_Lock();
            Draw_DrawString(10,10, COLOR_RED, buf);
            Draw_FlushFramebuffer();
            Draw_Unlock();
        } while(!(waitInput() & KEY_B) && !menuShouldExit);
    } else {
        Draw_Lock();
        Draw_ClearFramebuffer();
        Draw_FlushFramebuffer();
        Draw_Unlock();
        do {
            Draw_Lock();
            Draw_DrawString(10,10, COLOR_RED, "Thread already running!");
            Draw_FlushFramebuffer();
            Draw_Unlock();
        } while(!(waitInput() & KEY_B) && !menuShouldExit);
    }
}

void endThread() {
    Result res = 1337;
    char buf[65];
    run = false;
    if(isStarted) {
        shouldStop = true;
        serv.running = false;
        server_finalize(&serv);
    }
    res = MyThread_Join(&testThread, 5 * 1000 * 1000 * 1000LL);
    sprintf(buf, "Thread1 (%li) couldn't be stopped.", (u32)res);

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    if(res == 0 || res == -656406537) {
        closeRPHandle();
        isStarted = false;
    } do {
        Draw_Lock();
        Draw_DrawString(10,10, COLOR_RED, res == 0 ? "Thread successfully ended." : buf);
        Draw_FlushFramebuffer();
        Draw_Unlock();
    } while(!(waitInput() & KEY_B) && !menuShouldExit);
}

//TODO: Try to double buffer top and bot
u8 frameBufferCacheFirst[TOPSIZE*4];
u8 frameBufferCacheSecond[TOPSIZE*4];
u8 frameBufferCacheSend[TOPSIZE*4];

///Pixel processing functions
u32 makeGrayScale(bool isTop, u8 *fb, u32 bytePerPixel, u32 format) {
    (void)format;
    u32 size = 0;
    if(isTop) size = TOPSIZE*bytePerPixel;
    else size = BOTSIZE*bytePerPixel;
    u32 iterator = 0;

    if(bytePerPixel == 2){
        for(u32 i = 0; i < size; i += 2){
            u16 pixeldata = (fb[i] << 8) | (fb[i+1]);
            u8 blue =  pixeldata & 0x1F;
            u8 green = (pixeldata >> 5) & 0x3F;
            u8 red = (pixeldata >> 11) & 0x1F;

            u8 b = (blue  << 3) | (blue  >> 2);
            u8 g = (green << 2) | (green >> 4);
            u8 r = (red   << 3) | (red   >> 2);

            u8 pixel = ((0.11 * b) + (0.59 * g) + (0.3 * r));
            fb[iterator] = pixel;
            iterator++;
        }

    } else {
        for(u32 i = 0; i < size; i += 3) {
            u8 pixel = ((0.11 * fb[i]) + (0.59 * fb[i+1]) + (0.3 * fb[i+2]) );
            fb[iterator] = pixel;
            iterator++;
        }
    }
    return 1;
}

//Copies the Frame Buffer via the CPU
void copyFrameBufCPU(u8 * dest, bool isTop, u32 size) {
    u32 pa = Draw_GetCurrentFramebufferAddress(isTop, true);
    u32 *addr = (u32 *)PA_PTR(pa);
    memcpy(dest, addr, size);
}

void sendFrameBufferTCP(struct sock_ctx *ctx, bool isTop, u8 *framebufferCache,
                        u32 bytePerPixel, GSPGPU_FramebufferFormat format, u32 fbsize) {
    (void)bytePerPixel;
    char buf[9] = { 0x13, 0x37, isTop, (u8)currentBlockSize, (char)format,
                    (char)(fbsize >> 24), (char)(fbsize >> 16),
                    (char)(fbsize >> 8), (char)fbsize };
    socSend(ctx->sockfd, buf, sizeof(buf), 0);
    if(fbsize > 0) socSend(ctx->sockfd, framebufferCache, fbsize, 0);
}

//Just playing around with something
int compressFrame( u8 *framebufferCache, u8 *lastFramebufferCache,
                    u8 *sendBuffer, u32 blockSize, u32 bytePerPixel) {

    u32 size = isTop ? TOPSIZE*bytePerPixel : BOTSIZE*bytePerPixel;
    u32 dataSize = 0;

    for(u32 i = 0; i < size ; i += blockSize){
        if(memcmp(framebufferCache, lastFramebufferCache, blockSize) != 0) {
            *sendBuffer = (char)i;                  //Prepend index into array
            sendBuffer = sendBuffer+1;
            *sendBuffer = (char)(i >> 8);
            sendBuffer = sendBuffer+1;
            *sendBuffer = (char)(i >> 16);
            sendBuffer = sendBuffer+1;
            *sendBuffer = (char)(i >> 24);
            sendBuffer = sendBuffer+1;              //u32 pointer inc
            memcpy(sendBuffer, framebufferCache, blockSize);//memcpy difference
            sendBuffer = sendBuffer+blockSize;      //increment pointer by copied bytes
            dataSize += blockSize+4;                //how much space left?
            if(dataSize >= size/2) {return -1;}     // half frame different, send whole
        }

        framebufferCache = framebufferCache + blockSize;
        lastFramebufferCache = lastFramebufferCache + blockSize;
    }

    return dataSize;
};

Handle hProcess;
Handle dmaHandle;
Handle rpHandleGame;
u32 rpGameFCRAMBase = 0;
#define CURRENT_PROCESS_HANDLE	0xffff8001


//Handle functions
void closeRPHandle() {
    svcCloseHandle(rpHandleGame);
    rpHandleGame = 0;
    rpGameFCRAMBase = 0;
}

//Prevent system from locking up when switching processes (NTR hanging at the 3DS logo)
void closeHandleIfProcessHangs() {
    if(svcWaitSynchronization(rpHandleGame, 0) == 0) {
        closeRPHandle();
    }
}

Handle rpGetGameHandle() {
	Handle hProcess;
    s32 processCount;
    u32 processIDs[50];
    s32 processIdMaxCount = 50;

	if (rpHandleGame == 0) {
        svcGetProcessList(&processCount, processIDs, processIdMaxCount);
        int ret = svcOpenProcess(&hProcess, processIDs[39]);
        if (ret == 0) {
            rpHandleGame = hProcess;
        }
    }

    if (rpHandleGame == 0) {
		return 0;
	}

	if (rpGameFCRAMBase == 0) {
		if (svcFlushProcessDataCache(hProcess, (u32)0x14000000, 0x1000) == 0) {
			rpGameFCRAMBase = 0x14000000;
		}
		else if (svcFlushProcessDataCache(hProcess, (u32)0x30000000, 0x1000) == 0) {
			rpGameFCRAMBase = 0x30000000;
		}
		else {
			return 0;
		}
	}
	return rpHandleGame;
}

//Find buffer
int isInVRAM(u32 phys) {
	if (phys >= 0x18000000) {
		if (phys < 0x18000000 + 0x00600000) {
			return 1;
		}
	}
	return 0;
}

int isInFCRAM(u32 phys) {
	if (phys >= 0x20000000) {
		if (phys < 0x20000000 + 0x10000000) {
			return 1;
		}
	}
	return 0;
}

void sendDebug(bool isTop, struct sock_ctx *ctx) {
    GSPGPU_CaptureInfo displays;
    GSPGPU_ImportDisplayCaptureInfo(&displays);
    
    u32 format = isTop ? GPU_FB_TOP_FMT & 7 : GPU_FB_BOTTOM_FMT & 7;
    u32 bpp = gspGetBytesPerPixel(format);
    //u32 stride = isTop ? GPU_FB_TOP_STRIDE : GPU_FB_BOTTOM_STRIDE;
    u32 stride = displays.screencapture[isTop].framebuf_widthbytesize;
    u32 pa = Draw_GetCurrentFramebufferAddress(isTop, true);
    char buf[4];
    strncpy(buf, isTop ? "Top" : "Bot", 4);
    
    socSend(ctx->sockfd, buf, 3, 0);
    socSend(ctx->sockfd, &bpp, 4, 0);
    socSend(ctx->sockfd, &pa, 4, 0);
    socSend(ctx->sockfd, &stride, 4, 0);
}

//Copies the Frame Buffer via DMA
void copyFrameBufDMA(u8 * dest, bool isTop, u32 size) { 
    DmaDeviceConfig subcfg;
    subcfg.deviceId = 0xFF;                // Don't care copy from ram
    subcfg.allowedAlignments = 1 | 2 | 4 | 8; // allow all
    subcfg.burstSize = 0x0;
    subcfg.burstStride = 0;
    subcfg.transferSize = 0x80;
    subcfg.transferStride = 0;

    DmaConfig config;
    config.channelId = -1;                    //don't care
    config.endianSwapSize = 0;                //don't swap
    config.flags = 1 << 2 | 1 << 6 | 1 << 7;    //Block, src_ram, dst_ram
    config.srcCfg = subcfg;
    config.dstCfg = subcfg;

    Handle processHandle = hProcess;
    u32 pa = Draw_GetCurrentFramebufferAddress(isTop, true);
    uintptr_t addr = 0x0;

    //svcFlushProcessDataCache(CURRENT_PROCESS_HANDLE, (u32)dest, size);
    svcInvalidateProcessDataCache(CURRENT_PROCESS_HANDLE, (u32)dest, size);
    svcCloseHandle(dmaHandle);
	dmaHandle = 0;

    if (isInVRAM(pa)) {
        addr = 0x1F000000 + (pa - 0x18000000);
        svcStartInterProcessDma(&dmaHandle, CURRENT_PROCESS_HANDLE, (u32)dest,
                                processHandle, (u32)addr, size, &config);
    }	else if (isInFCRAM(pa)) {
		processHandle = rpGetGameHandle();
		if (processHandle) {
			addr = rpGameFCRAMBase + (pa - 0x20000000);
            svcStartInterProcessDma(&dmaHandle, CURRENT_PROCESS_HANDLE, (u32)dest,
                                    processHandle, (u32)addr, size, &config);
		}
    }
}

//To skip blank pixels in the Framebuffer (e.g. Mario Kart)
void skipBytes(u32 screenWidth, u8 *src, u8 *dest, u32 bytesInColumn, u32 blankInColumn) {
    for(u32 i = 0; i < screenWidth; i++) {
        for(u32 j = 0; j < bytesInColumn/4; j++) {
            ((u32*)dest)[j] = ((u32*)src)[j];
        }
        src += bytesInColumn+blankInColumn;
        dest += bytesInColumn;
    }
}

//Main Remoteplay Function
void remotePlay(struct sock_ctx *ctx, bool isTop) {
    u8 *fbref = isSecondFrameBuffer ? frameBufferCacheSecond : frameBufferCacheFirst;
    u8 *lastFrame = isSecondFrameBuffer ? frameBufferCacheFirst : frameBufferCacheSecond;

    u32 format = isTop ? GPU_FB_TOP_FMT & 7 : GPU_FB_BOTTOM_FMT & 7;
    u32 bpp = gspGetBytesPerPixel(format);

    u32 stride = isTop ? GPU_FB_TOP_STRIDE : GPU_FB_BOTTOM_STRIDE;

    u32 screenWidth = isTop ? TOPW : BOTW;
    //u32 size = displays.screencapture[isTop].framebuf_widthbytesize;
    u32 size = stride * screenWidth;
    u32 bytesInColumn = bpp*HEIGHT;
    u32 blankInColumn = stride - bytesInColumn;
    u32 realSize = bytesInColumn*screenWidth;

    
    if(isCPUCopy) {copyFrameBufCPU(fbref, isTop, size);}
    else {
        copyFrameBufDMA(fbref, isTop, size);
    	DmaState stat = 0;
	    while(stat != DMASTATE_DONE)
	    {
	    	svcGetDmaState(&stat, dmaHandle);
	    	svcSleepThread(1000);
	    }
    }
    
    if(stride != bytesInColumn) {
        skipBytes(screenWidth, fbref, lastFrame, bytesInColumn, blankInColumn);
        fbref = lastFrame;
    }

    if(isGraySacle) {bpp = makeGrayScale(isTop, fbref, bpp, format); format = 5;}

    int frameDataSize = -1;
    if(doCompress && !userNoCompress){
        frameDataSize = compressFrame(fbref, lastFrame, frameBufferCacheSend, currentBlockSize, bpp);
    }

    if(frameDataSize == -1) {frameDataSize = realSize;} 
    else {
        format = 6;
        fbref = frameBufferCacheSend;
    }

    sendFrameBufferTCP(ctx, isTop, fbref, bpp, format, frameDataSize);

    isSecondFrameBuffer = !isSecondFrameBuffer;

    userNoCompress = false;
    closeHandleIfProcessHangs();
    svcSleepThread(200000);
}

//Networking functions
int acceptClient(struct sock_ctx *ctx) {
    char welcome[] = "Connection was accepted!";
    socSend(ctx->sockfd, welcome, sizeof(welcome), 0);
    return 0;
}

//Get the current client connection
struct sock_ctx *allocClient(struct sock_server *server, u16 port) {
    if(port < PORT || port >= PORT + sizeof(server->serv_ctxs) / sizeof(sock_ctx)){
        return NULL;
    }
    sock_ctx *ctx = &server->serv_ctxs[port - PORT];
    return ctx;
}

int closeConnection(struct sock_ctx *ctx) {
    (void)ctx;
    return 0;
}

void releaseClient(struct sock_server *server, struct sock_ctx *ctx) {
    server->running = false;
    ctx->should_close = true;
    shouldStop = true;
}

//Process incoming data
int handleData(struct sock_ctx *ctx) {
    char buf[4] = {0, 0, 0, 0};

    int r = socRecv(ctx->sockfd, buf, sizeof(buf), 0);
    if(r == -1)
        return -1;

    if(buf[0] == 5) sendDebug(isTop, ctx);
    if(buf[0] == 4) remotePlay(ctx, (bool)buf[1]);
    if(buf[0] == 6) shouldStop = true;
    if(buf[0] == 7) isTop = (bool)buf[1];
    if(buf[0] == 8) isGraySacle = !isGraySacle;
    if(buf[0] == 9) doCompress = !doCompress;
    if(buf[0] == 10) userNoCompress = true;
    return r;
}

//Main Entrypoint
void commandThreadMain(void) {
    run = true;
        Result ret = 0;
        ret = server_init(&serv);
        if(ret != 0) {
            isStarted = false;
            return;
        }

        while(serv.host == 0) {
            serv.host = socGethostid();
            svcSleepThread(1 * 1000 * 1000 * 1000LL);
        }

        serv.clients_per_server = 1;
        serv.accept_cb = (sock_accept_cb)acceptClient;
        serv.data_cb = (sock_data_cb)handleData;
        serv.close_cb = (sock_close_cb)closeConnection;
        serv.alloc = (sock_alloc_func)allocClient;
        serv.free = (sock_free_func)releaseClient;
        svcOpenProcess(&hProcess, 0xf);
        ret = server_bind(&serv, PORT);
        server_run(&serv);
}


/*
//Currently unused
void startSendFrameBufferTCP(struct sock_ctx *ctx) {
        shouldStop = false;
        cli = ctx;
        testSendCreateTCPThread();
}


void testSendThreadMain(void);
MyThread *testSendCreateThread(void){
    if(R_FAILED(MyThread_Create(&testSendThread, testSendThreadMain, testSendThreadStack, 0x2000, 0x1, CORE_SYSTEM)))
        svcBreak(USERBREAK_PANIC);
    return &testSendThread;
}

//Remains of trying to implement UDP
void testSendThreadMain(void) {
    Result res = 0;
    res = miniSocInit();
    if(R_FAILED(res))
        return;

    int sock = socSocket(AF_INET, SOCK_DGRAM, 0);
    while(sock == -1)
    {
        svcSleepThread(1000 * 0000 * 0000LL);
        sock = socSocket(AF_INET, SOCK_DGRAM, 0);
    }

    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(4955);
    saddr.sin_addr.s_addr = gethostid();


    struct sockaddr_in caddr;
    caddr.sin_family = AF_INET;
    caddr.sin_port = htons(4955);
    caddr.sin_addr.s_addr = inet_addr(inet_ntoa(cli->addr_in.sin_addr));

    res = socBind(sock, (struct sockaddr*)&saddr, sizeof(struct sockaddr_in));

    if(res != 0)
    {
        socClose(sock);
        miniSocExit();
        return;
    }

    char buf[50];
    sprintf(buf, "Sending to %s at %d", inet_ntoa(caddr.sin_addr), ntohs(saddr.sin_port));
    Draw_Lock();
    do
    {
        Draw_DrawString(10,10, COLOR_RED, buf);
        Draw_FlushFramebuffer();
    } while(!(waitInput() & KEY_B) && !menuShouldExit);
    Draw_Unlock();

    svcSignalEvent(sendUDPStarted);

    while(!shouldStop && !menuShouldExit) {
        getFrameBuffer(isTop, caddr, sock);
    }

    struct linger linger;
    linger.l_onoff = 1;
    linger.l_linger = 0;

    socSetsockopt(sock, SOL_SOCKET, SO_LINGER, &linger, sizeof(struct linger));

    socClose(sock);
    miniSocExit();

    sprintf(buf, "UDP Thread ended.");
    Draw_Lock();
    do
    {
        Draw_DrawString(10,10, COLOR_RED, buf);
        Draw_FlushFramebuffer();
    } while(!(waitInput() & KEY_B) && !menuShouldExit);
    Draw_Unlock();

    MyThread_Join(&testSendThread, 5 * 1000 * 1000 * 1000LL);
    return;
}

int startSend(struct sock_ctx *ctx) {
    while(!shouldStop) {
        sendFrameBufferTCP(ctx, isTop, isGraySacle);
    }
    return 0;
}

void testSendThreadTCPMain(void) {
    while(!shouldStop && !menuShouldExit) {
        Result ret = 0;
        ret = server_init(&stream);
        if(ret != 0) {
            isStarted = false;
            return;
        }

        stream.host = gethostid();
        stream.clients_per_server = 1;
        stream.accept_cb = (sock_accept_cb)startSend;
        stream.data_cb = (sock_data_cb)closeConnection;
        stream.close_cb = (sock_close_cb)closeConnection;
        stream.alloc = (sock_alloc_func)allocClient;
        stream.free = (sock_free_func)releaseClient;
        server_bind(&stream, PORT+1);
        server_run(&stream);
        server_finalize(&stream);
    }
    MyThread_Join(&testSendThread, 5 * 1000 * 1000 * 1000LL);
    return;
}

void startSendFrameBufferUDP(struct sock_ctx *ctx) {
    shouldStop = false;
    cli = ctx;
    testSendCreateThread();
}*/
