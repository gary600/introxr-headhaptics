import sys
from serial import Serial
from pythonosc.dispatcher import Dispatcher
from pythonosc.osc_server import BlockingOSCUDPServer

WRAP = 1024
RAMP = 10

PREFIX = "/avatar/parameters/skyhap_"

POINTMAP = {
    "R1": 1,
    "R2": 2,
    "R3": 3,
    "R4": 4,
    "R5": 5,
    "R6": 6,
    "R7": 7,
    "L1": 9,
    "L2": 10,
    "L3": 11,
    "L4": 12,
    "L5": 13,
    "L6": 14,
    "L7": 15,
}

# EARMAP = {
#     "REar_angle": (3, 4, 5),
#     "LEar_angle": (3+8, 4+8, 5+8),
# }

ser = Serial(sys.argv[1], 115200)

def handle_haptic(addr: str, arg: float):
    addr = addr.removeprefix(PREFIX)
    if type(arg) is not float:
        return
    if addr in POINTMAP:
        point = POINTMAP[addr]
        target = int(WRAP * arg**3)
        # if (arg < 0.01):
            # target = 0
        # else:
            # target = int(WRAP/2 + WRAP/2 * arg)
        command = f"s{point},{target},{RAMP}\n"
        ser.write(command.encode())
        print(f"{addr}: {arg}")
    # elif addr in EARMAP:
    #     for point in EARMAP[addr]:

# def handle_default(address, *args):
#     print(f"other: {address}: {args}")

disp = Dispatcher()
disp.map(PREFIX + "*", handle_haptic)
# disp.set_default_handler(handle_default)
srv = BlockingOSCUDPServer(("0.0.0.0", 9001), disp)
srv.serve_forever()
