#!/usr/bin/env python3
import socket, struct, sys, pygame, os, numpy
from PIL import Image
from enum import Enum
import binascii
import time

WELCOME = "Connection was accepted!\x00"
DSSCREENHEIGHT = 240
TOPWIDTH = 400
BOTTOMWIDTH = 320
TOPSIZE = TOPWIDTH*DSSCREENHEIGHT
BOTSIZE = BOTTOMWIDTH*DSSCREENHEIGHT
IMGTOP = (DSSCREENHEIGHT,TOPWIDTH)
IMGBOT = (DSSCREENHEIGHT,BOTTOMWIDTH)
IP = "192.168.3.72"
PORT = 4957
HEADERSIZE = 9
MAGIC = b'\x13\x37'


class BrokenPacket(Exception):
    pass

class GSPGPU_FramebufferFormats(Enum):
    GSP_RGBA8_OES=0,    #///< RGBA8     (4 bytes)
    GSP_BGR8_OES=1,     #///< BGR8      (3 bytes)
    GSP_RGB565_OES=2,   #///< RGB565    (2 bytes)
    GSP_RGB5_A1_OES=3,  #///< RGB5A1    (2 bytes)
    GSP_RGBA4_OES=4     #///< RGBA4     (2 bytes)

def getBytesPerPixel(pixelFormat):
    if pixelFormat == GSPGPU_FramebufferFormats.GSP_RGBA8_OES:
        return 4
    if pixelFormat == GSPGPU_FramebufferFormats.GSP_BGR8_OES:
        return 3
    if pixelFormat == 5:
        return 1
    else:
        return 2

def getPixelFormat(pixelFormat):
    if pixelFormat == 0:
        return GSPGPU_FramebufferFormats.GSP_RGBA8_OES
    if pixelFormat == 1:
        return GSPGPU_FramebufferFormats.GSP_BGR8_OES
    if pixelFormat == 2:
        return GSPGPU_FramebufferFormats.GSP_RGB565_OES
    if pixelFormat == 3:
        return GSPGPU_FramebufferFormats.GSP_RGB5_A1_OES
    if pixelFormat == 4:
        return GSPGPU_FramebufferFormats.GSP_RGBA4_OES
    else:
        return pixelFormat

def getScreen(sock, oldFrame, lastbpp):
    headerSize = HEADERSIZE
    header = bytearray(headerSize)
    magic = b''
    isTop = b''
    unused = b''
    pixelFormat = b''
    size = 0
    fullsize = 0
    
    while headerSize:
        try:
            nbytes = sock.recv_into(header, headerSize)
            headerSize -= nbytes
            time.sleep(5/1000000.0)
        except socket.timeout:
            print("Timeout!!! Try again...")
            break

    magic = bytes([header[0],header[1]])
    isTop = header[2]
    currentBlocksize = header[3]
    pixelFormat = getPixelFormat(header[4])
    bytesPerPixel = getBytesPerPixel(pixelFormat)
    fbsize = struct.unpack(">I", bytes([header[5], header[6], header[7],header[8]]))[0]


    if magic == MAGIC:
        size = fbsize
        fullsize = size
    else:
        print("Broken or invalid Packet!")
        print("Magic: ", header[:2], "Expected: ", MAGIC)
        raise BrokenPacket
    
    data = bytearray(size)
    view = memoryview(data)
    while size:
        try:
            nbytes = sock.recv_into(view, size)
            view = view[nbytes:]
            size -= nbytes
        except socket.timeout:
            print("Timeout!!! Try again...")
            break
    
    #print("Top: ", isTop, "Format: ", pixelFormat)
    #print("Fbsize: ", fbsize)
    #print(header)
    #print("BPP: ", bytesPerPixel)
    #print("First 10 bytes: ", data[:4*4+4])
    #print("BlockSize: ", currentBlocksize)
    #print("Data length: ", len(data))
    
    #compressed
    if pixelFormat == 6:
        oldFrame = bytearray(oldFrame)
        view = memoryview(oldFrame)
        #if data != b'':
        #    print(data[:20])
        for i in range(0, fbsize, currentBlocksize+4):
            offset = struct.unpack("<I", bytes([data[i], data[i+1], data[i+2],data[i+3]]))[0]
            for j in range(currentBlocksize):
                oldFrame[offset+j] = int(data[i+4+j])
        return bytes(oldFrame),lastbpp

    if len(data) != fullsize:
        print("Package not complete...")
        data += bytearray(fullsize-len(data))
    
    
    #uncompressed
    #file = open("frame.bin", "wb")
    #file.write(data)
    #sys.exit(1)
    
    return bytes(data),bytesPerPixel

def sendCapturePacket(sock, isTop):
    MSG = b'\x04' + bytes([isTop])
    sock.send(MSG)

def prepareImage(imgData, captureTop, bpp):
    if captureTop:
        imgSize = IMGTOP
    else:
        imgSize = IMGBOT

    if bpp == 1:
        img = Image.frombuffer('P', imgSize, imgData, 'raw', 'P', 0, 1)
    elif bpp == 2:
        img = Image.frombuffer('RGB', imgSize, imgData, 'raw', 'BGR;16', 0, 1)
    else:
        img = Image.frombuffer('RGB', imgSize, imgData, 'raw', pixelformat, 0, 1)   
    
    img = img.rotate(90, 0, 1)                        
    if WIDTH == TOPWIDTH*2 or WIDTH == (BOTTOMWIDTH+TOPWIDTH)*2:
        img = img.resize((imgSize[1]*2, imgSize[0]*2))
    
    bufdat = img.tobytes()
    imgBlit = ''

    if bpp == 1:
        imgBlit = pygame.image.fromstring(bufdat, img.size, 'P')
        imgBlit.set_palette(palette)
    else:
        imgBlit = pygame.image.fromstring(bufdat, img.size, 'RGB')
    
    return imgBlit

if len(sys.argv) >= 1:
    os.environ['SDL_VIDEO_CENTERED'] = '1'
    pygame.init()
    FPS = 60
    DISPLAY = pygame.display.Info()
    WIDTH = TOPWIDTH
    HEIGHT = DSSCREENHEIGHT*2
    #For Debug, start in large screen
    WIDTH = WIDTH*2
    HEIGHT = HEIGHT*2
    NAME = "3DS View"
    SCALED = True
    ROTATED = False

    screen = pygame.display.set_mode((WIDTH, HEIGHT))
    pygame.display.set_caption(NAME)
    clock = pygame.time.Clock()
    palette = []
    for i in range(255):
        palette.append((i,i,i))
    
    font = pygame.font.Font('freesansbold.ttf', 32)

    try:
        RED = 255
        GREEN = 0
        BLUE = 255
        ENABLE = 1
        COLOR = (ENABLE << 23) + (BLUE << 15) + (GREEN << 7) + RED
        BINCOL = struct.pack("<I", COLOR)
        MESSAGE = struct.pack("<B", int(sys.argv[1]))
        #MESSAGE = b'\x04'

        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((IP, PORT))
        sock.settimeout(1)
        data = sock.recv(1024)
        string = data.decode("ascii")
        print(string)
        if string == WELCOME:
            print("Sending...")
            
            if MESSAGE == b'\x04':
                pixelformat = 'BGR'
                index = 0
                running = True
                captureTop = False
                toggle = False
                imgData = b''
                imgSize = (0,0)
                stop = False
                screen.fill([0, 0, 0])
                printTimer = pygame.time.get_ticks()
                oldBotFrame = bytearray()
                oldTopFrame = bytearray()
                oldFrame = bytearray()
                bpp = 3
                text = font.render("", True, (255,255,255))
                doShow = False
                
                while running:
                    clock.tick(FPS) 

                    for event in pygame.event.get():
                        if event.type == pygame.QUIT:
                            running = False
                        if event.type == pygame.KEYDOWN:
                            if event.key == pygame.K_ESCAPE:
                                running = False
                            if event.key == pygame.K_LEFT:
                                toggle = not toggle
                                print("Alternate polling: ", toggle)
                            if event.key == pygame.K_PLUS:
                                if SCALED == False:
                                    WIDTH = int(WIDTH*2)
                                    HEIGHT = int(HEIGHT*2)
                                else:
                                    WIDTH = int(WIDTH/2)
                                    HEIGHT = int(HEIGHT/2)
                                SCALED = not SCALED
                                print("Setting to: ", WIDTH, HEIGHT)
                                screen = pygame.display.set_mode((WIDTH, HEIGHT))
                            if event.key == pygame.K_UP:
                                toggle = False
                                captureTop = True
                                print("Setting captureTop: ", captureTop)
                            if event.key == pygame.K_DOWN:
                                toggle = False
                                captureTop = False
                                print("Setting captureTop: ", captureTop)
                            if event.key == pygame.K_s:
                                stop = not stop
                                print("Stop: ", stop)
                            if event.key == pygame.K_a:
                                if pixelformat == 'BGR':
                                    pixelformat = 'RGB'
                                else:
                                    pixelformat = 'BGR'
                            if event.key == pygame.K_b:
                                sock.send(b'\x08')
                            if event.key == pygame.K_c:
                                print("Compress!")
                                sock.send(b'\x09')
                            if event.key == pygame.K_f:
                                doShow = not doShow
                                text = font.render("", True, (255,255,255))
                            if event.key == pygame.K_n:
                                sock.send(b'\x0A')
                            if event.key == pygame.K_r:
                                if ROTATED == True:
                                    WIDTH = TOPWIDTH
                                    HEIGHT = DSSCREENHEIGHT*2
                                else:
                                    WIDTH = TOPWIDTH+BOTTOMWIDTH
                                    HEIGHT = DSSCREENHEIGHT
                                if SCALED == True:
                                    WIDTH = WIDTH*2
                                    HEIGHT = HEIGHT*2
                                ROTATED = not ROTATED
                                screen = pygame.display.set_mode((WIDTH, HEIGHT))
                            
                                
                    if not stop:
                        if captureTop:
                            oldTopFrame = data
                        else:
                            oldBotFrame = data
                        
                        if len(oldTopFrame) == 0 or len(oldBotFrame) == 0:
                            sock.send(b'\x0A')
                        
                        sendCapturePacket(sock, captureTop)
                        try:
                            if captureTop:
                                data,bpp = getScreen(sock, oldTopFrame, bpp)
                            else:
                                data,bpp = getScreen(sock, oldBotFrame, bpp)
                        except BrokenPacket:
                            print("Caught Broken packet!")
                            sock.send(b'\x0A')
                        except IndexError:
                            print("Corrupted Array Index out of bounds!")
                            sock.send(b'\x0A')

                        imgBlit = prepareImage(data, captureTop, bpp)
                        
                        if captureTop:
                            screen.blit(imgBlit, [0,0])
                        elif ROTATED == True:
                            screen.blit(imgBlit, [WIDTH-imgBlit.get_width(),0])
                        else:
                            screen.blit(imgBlit, [(WIDTH-imgBlit.get_width())/2,HEIGHT/2])
                        
                        if toggle:
                            captureTop = not captureTop
                        
                        if pygame.time.get_ticks() - printTimer > 500 and doShow:
                            printTimer = pygame.time.get_ticks()
                            fps = int(clock.get_fps())
                            txt = ''
                            #txt += "Time: {} ".format(str(printTimer))
                            txt += "FPS: {}".format(str(fps))
                            text = font.render(txt, True, (255,255,255), (128,128,128))
                            #print( )
                            #pygame.display.set_caption(NAME + " FPS: " + str(fps))
                        
                        screen.blit(text, (0,0))
                        pygame.display.update()

            elif MESSAGE == b'\x03':
                sock.send(MESSAGE+BINCOL)
            elif MESSAGE == b'\x05':
                sock.send(MESSAGE)
                data = b''
                while True:
                    data += sock.recv(1024)
                    print(binascii.hexlify(data))            
            else:
                sock.send(MESSAGE)
                sock.recv(1024)
            if MESSAGE == b'\x02':
                data = sock.recv(1024)
                print(data.decode())

        sock.close()
    
    except KeyboardInterrupt:
        print("Closing socket...")
        sock.close()

    #except ValueError:
    #    print("Not enough data!")
    #    print("Closing socket...")
    #    sock.close()

else:
    print(len(sys.argv))
    print("No MESSAGE was given!")
